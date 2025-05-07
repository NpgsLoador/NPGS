#pragma once

#include <cstddef>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <concurrentqueue/concurrentqueue.h>

namespace Npgs
{
    template <typename Ty1>
    struct THasPublicDestructor
    {
        template <typename Ty2>
        static auto Func(int) -> decltype(std::declval<Ty2>().~Ty2(), std::true_type{});

        template <typename>
        static std::false_type Func(...);

        static constexpr bool kbValue = decltype(Func<Ty1>(0))::value;
    };

    template <typename MemoryType>
    class TMemoryPool
    {
    public:
        using FMemoryHandle = std::size_t;

        class FMemoryGuard
        {
        public:
            FMemoryGuard(TMemoryPool* Pool, FMemoryHandle Handle)
                : Pool_(Pool), Handle_(Handle)
            {
            }

            FMemoryGuard(const FMemoryGuard&) = delete;
            FMemoryGuard(FMemoryGuard&& Other) noexcept
                : Pool_(std::exchange(Other.Pool_, nullptr))
                , Handle_(std::exchange(Other.Handle_, nullptr))
            {
            }

            ~FMemoryGuard()
            {
                Pool_->Deallocate(Handle_);
            }

            FMemoryGuard& operator=(const FMemoryGuard&) = delete;
            FMemoryGuard& operator=(FMemoryGuard&& Other) noexcept
            {
                if (this != &Other)
                {
                    Pool_   = std::exchange(Other.Pool_, nullptr);
                    Handle_ = std::exchange(Other.Handle_, 0);
                }

                return *this;
            }

            MemoryType* operator->()
            {
                return Pool_->GetMemory(Handle_);
            }

            const MemoryType* operator->() const
            {
                return Pool_->GetMemory(Handle_);
            }

            MemoryType& operator*()
            {
                return *Pool_->GetMemory(Handle_);
            }

            const MemoryType& operator*() const
            {
                return *Pool_->GetMemory(Handle_);
            }

        private:
            TMemoryPool*  Pool_;
            FMemoryHandle Handle_;
        };

        struct alignas(MemoryType) FMemoryBlock
        {
            std::array<std::byte, sizeof(MemoryType)> Memory;
        };

    public:
        explicit TMemoryPool(std::size_t InitialCapacity, bool bDynamicExpand = false)
            : MemoryBlocks_(InitialCapacity, {})
            , bDynamicExpand_(bDynamicExpand)
        {
            static_assert(THasPublicDestructor<MemoryType>::kbValue, "MemoryType destructor must be public");
            for (std::size_t i = 0; i != MemoryBlocks_.size(); ++i)
            {
                FreeList_.enqueue(i);
            }
        }

        TMemoryPool(const TMemoryPool&) = delete;
        TMemoryPool(TMemoryPool&&)      = delete;
        ~TMemoryPool()                  = default;

        TMemoryPool& operator=(const TMemoryPool&) = delete;
        TMemoryPool& operator=(TMemoryPool&&)      = delete;

        template <typename... Args>
        FMemoryGuard Allocate(Args... ConstructArgs)
        {
            FMemoryHandle MemoryHandle = 0;
            if (FreeList_.try_dequeue(MemoryHandle))
            {
                FMemoryBlock* MemoryBlock = &MemoryBlocks_[MemoryHandle];
                new (MemoryBlock->Memory.data()) MemoryType(std::forward<Args>(ConstructArgs)...);
                return FMemoryGuard(this, MemoryHandle);
            }

            if (!bDynamicExpand_)
            {
                throw std::runtime_error("Failed to allocate memory: no available memory");
            }

            std::size_t NewCapacity = Capacity() + Capacity() / 2;
            MemoryBlocks_.resize(NewCapacity);
            return std::move(Allocate(std::forward<Args>(ConstructArgs)...));
        }

        void Reserve(std::size_t NewCapacity)
        {
            if (NewCapacity < Capacity())
            {
                return;
            }

            std::size_t OldSize = MemoryBlocks_.size();
            MemoryBlocks_.resize(NewCapacity);

            for (std::size_t i = OldSize; i != NewCapacity; ++i)
            {
                FreeList_.enqueue(i);
            }
        }

        void ShrinkToFit()
        {
            std::size_t TargetCapacity = std::max(FreeList_.size_approx(), SizeApprox());
            if (MemoryBlocks_.size() <= TargetCapacity)
            {
                return;
            }

            std::vector<FMemoryBlock> NewMemoryBlocks(TargetCapacity);
            moodycamel::ConcurrentQueue<FMemoryHandle> NewFreeList;
            std::size_t CurrentIndex = 0;

            std::vector<FMemoryHandle> FreeHandles;
            FMemoryHandle Handle = 0;
            while (FreeList_.try_dequeue(Handle))
            {
                FreeHandles.push_back(Handle);
            }

            std::vector<bool> InUsed(MemoryBlocks_.size(), true);
            for (FMemoryHandle FreeHandle : FreeHandles)
            {
                InUsed[FreeHandle] = false;
            }
        }

        std::size_t AvailableApprox() const
        {
            return FreeList_.size_approx();
        }

        std::size_t SizeApprox() const
        {
            return MemoryBlocks_.size() - FreeList_.size_approx();
        }

        std::size_t Capacity() const
        {
            return MemoryBlocks_.capacity();
        }

    private:
        void Deallocate(FMemoryHandle Handle)
        {
            GetMemory(Handle)->~MemoryType();
            FreeList_.enqueue(Handle);
        }

        MemoryType* GetMemory(FMemoryHandle Handle)
        {
            auto* Block = &MemoryBlocks_[Handle];
            return reinterpret_cast<MemoryType*>(Block->Memory.data());
        }
        
    private:
        moodycamel::ConcurrentQueue<FMemoryHandle> FreeList_;
        std::vector<FMemoryBlock>                  MemoryBlocks_;
        bool                                       bDynamicExpand_;
    };
} // namespace Npgs
