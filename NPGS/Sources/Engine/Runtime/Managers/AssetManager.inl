#include "AssetManager.hpp"
#include <utility>

#include "Engine/Core/Base/Base.hpp"
#include "Engine/Core/Utils/Utils.hpp"

namespace Npgs
{
    template<typename OriginalType>
    requires std::is_class_v<OriginalType>
    FTypeErasedDeleter::FTypeErasedDeleter(OriginalType*)
        : Deleter_([](void* Ptr) -> void { delete static_cast<OriginalType*>(Ptr); })
    {
    }

    NPGS_INLINE void FTypeErasedDeleter::operator()(void* Ptr) const
    {
        Deleter_(Ptr);
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE TAssetHandle<AssetType>::TAssetHandle(FAssetManager* Manager, FAssetEntry* Asset)
        : Manager_(Manager)
        , Asset_(Asset)
        , ManagerLiveness_(Manager->LivenessToken_)
    {
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE TAssetHandle<AssetType>::TAssetHandle(const TAssetHandle& Other)
        : Manager_(Other.Manager_)
        , Asset_(Other.Asset_)
        , ManagerLiveness_(Other.ManagerLiveness_)
    {
        Asset_->RefCount->fetch_add(1, std::memory_order::relaxed);
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE TAssetHandle<AssetType>::TAssetHandle(TAssetHandle&& Other) noexcept
        : Manager_(std::exchange(Other.Manager_, nullptr))
        , Asset_(std::exchange(Other.Asset_, nullptr))
        , ManagerLiveness_(std::move(Other.ManagerLiveness_))
    {
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE TAssetHandle<AssetType>::~TAssetHandle()
    {
        if (auto IsAlive = ManagerLiveness_.lock())
        {
            if (Manager_ != nullptr && Asset_ != nullptr)
            {
                Manager_->ReleaseAsset(Asset_);
            }
        }
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE TAssetHandle<AssetType>& TAssetHandle<AssetType>::operator=(const TAssetHandle& Other)
    {
        if (this != &Other)
        {
            Manager_         = Other.Manager_;
            Asset_           = Other.Asset_;
            ManagerLiveness_ = Other.ManagerLiveness_;

            Asset_->RefCount->fetch_add(1, std::memory_order::relaxed);
        }

        return *this;
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE TAssetHandle<AssetType>& TAssetHandle<AssetType>::operator=(TAssetHandle&& Other) noexcept
    {
        if (this != &Other)
        {
            Manager_         = std::exchange(Other.Manager_, nullptr);
            Asset_           = std::exchange(Other.Asset_, nullptr);
            ManagerLiveness_ = std::move(Other.ManagerLiveness_);
        }

        return *this;
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE AssetType* TAssetHandle<AssetType>::operator->()
    {
        return static_cast<AssetType*>(Asset_->Payload.get());
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE const AssetType* TAssetHandle<AssetType>::operator->() const
    {
        return static_cast<AssetType*>(Asset_->Payload.get());
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE AssetType& TAssetHandle<AssetType>::operator*()
    {
        return *static_cast<AssetType*>(Asset_->Payload.get());
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE const AssetType& TAssetHandle<AssetType>::operator*() const
    {
        return *static_cast<AssetType*>(Asset_->Payload.get());
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE bool TAssetHandle<AssetType>::operator==(const TAssetHandle& Other) const
    {
        return Manager_ == Other.Manager_ && Asset_ == Other.Asset_;
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE TAssetHandle<AssetType>::operator bool() const
    {
        return Manager_ != nullptr && Asset_ != nullptr;
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE void TAssetHandle<AssetType>::Pin()
    {
        Asset_->RefCount->fetch_add(1, std::memory_order::relaxed);
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE void TAssetHandle<AssetType>::Unpin()
    {
        Asset_->RefCount->fetch_sub(1, std::memory_order::relaxed);
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE AssetType* TAssetHandle<AssetType>::Get()
    {
        return static_cast<AssetType*>(Asset_->Payload.get());
    }

    template <CAssetCompatible AssetType>
    NPGS_INLINE const AssetType* TAssetHandle<AssetType>::Get() const
    {
        return static_cast<AssetType*>(Asset_->Payload.get());
    }

    template <CAssetCompatible AssetType>
    inline std::size_t FAssetHandleHash::operator()(const TAssetHandle<AssetType>& Handle) const noexcept
    {
        std::size_t Hash = 0;
        Utils::HashCombine(Handle.Manager_, Hash);
        Utils::HashCombine(Handle.Asset_,   Hash);
        return Hash;
    }

    template <CAssetCompatible AssetType>
    void FAssetManager::AddAsset(std::string_view Name, AssetType&& Asset)
    {
        if (Assets_.contains(Name))
        {
            return;
        }

        auto Entry = std::make_unique<FAssetEntry>();

        Entry->Payload = std::unique_ptr<void, FTypeErasedDeleter>(
            static_cast<void*>(new AssetType(std::move(Asset))),
            FTypeErasedDeleter(static_cast<AssetType*>(nullptr))
        );

        Entry->RefCount = std::make_unique<std::atomic<std::size_t>>();
        Entry->RefCount->store(0, std::memory_order::relaxed);

        Assets_.emplace(Name, std::move(Entry));
    }

    template <CAssetCompatible AssetType, typename... Types>
    void FAssetManager::AddAsset(std::string_view Name, Types&&... Args)
    {
        if (Assets_.contains(Name))
        {
            return;
        }

        auto Entry = std::make_unique<FAssetEntry>();

        Entry->Payload = std::unique_ptr<void, FTypeErasedDeleter>(
            static_cast<void*>(new AssetType(VulkanContext_, std::forward<Types>(Args)...)),
            FTypeErasedDeleter(static_cast<AssetType*>(nullptr))
        );

        Entry->RefCount = std::make_unique<std::atomic<std::size_t>>();
        Entry->RefCount->store(0, std::memory_order::relaxed);

        Assets_.emplace(Name, std::move(Entry));
    }

    template <CAssetCompatible AssetType>
    TAssetHandle<AssetType> FAssetManager::AcquireAsset(std::string_view Name)
    {
        auto it = Assets_.find(Name);
        if (it == Assets_.end())
        {
            return TAssetHandle<AssetType>();
        }

        auto* AssetEntry = it->second.get();
        AssetEntry->RefCount->fetch_add(1, std::memory_order::relaxed);

        return TAssetHandle<AssetType>(this, AssetEntry);
    }

    NPGS_INLINE void FAssetManager::RemoveAssetImmediately(std::string_view Name)
    {
        Assets_.erase(Name);
    }

    NPGS_INLINE void Npgs::FAssetManager::ReleaseAsset(FAssetEntry* Asset)
    {
        Asset->RefCount->fetch_sub(1, std::memory_order::relaxed);
    }
} // namespace Npgs
