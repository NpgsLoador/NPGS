#include "stdafx.h"
#include "ThreadPool.h"

#include <cstdint>
#include <cstdlib>
#include <algorithm>

#include <Windows.h>
#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    namespace
    {
        int GetPhysicalCoreCount()
        {
            DWORD Length = 0;
            GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &Length);
            std::vector<std::uint8_t> Buffer(Length);
            auto* BufferPtr = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(Buffer.data());
            GetLogicalProcessorInformationEx(RelationProcessorCore, BufferPtr, &Length);

            int CoreCount = 0;
            while (Length > 0)
            {
                if (BufferPtr->Relationship == RelationProcessorCore)
                {
                    ++CoreCount;
                }

                Length -= BufferPtr->Size;
                BufferPtr = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
                    reinterpret_cast<std::uint8_t*>(BufferPtr) + BufferPtr->Size);
            }

            return CoreCount;
        }
    }

    // ThreadPool implementations
    // --------------------------
    FThreadPool::FThreadPool(int MaxThreadCount, bool bEnableHyperThread)
        : MaxThreadCount_(std::clamp(MaxThreadCount, 0, static_cast<int>(std::thread::hardware_concurrency())))
        , PhysicalCoreCount_(GetPhysicalCoreCount())
        , bEnableHyperThread_(bEnableHyperThread)
    {
        for (std::size_t i = 0; i != MaxThreadCount_; ++i)
        {
            Threads_.emplace_back([this]() -> void
            {
                while (true)
                {
                    std::function<void()> Task;
                    {
                        std::unique_lock<std::mutex> Lock(Mutex_);
                        Condition_.wait(Lock, [this]() -> bool { return !Tasks_.empty() || bTerminate_; });
                        if (bTerminate_ && Tasks_.empty())
                        {
                            return;
                        }

                        Task = std::move(Tasks_.front());
                        Tasks_.pop();
                    }

                    Task();
                }
            });

            if (!bEnableHyperThread_)
            {
                SetThreadAffinity(Threads_.back(), i);
            }
        }
    }

    FThreadPool::~FThreadPool()
    {
        Terminate();
    }

    void FThreadPool::Terminate()
    {
        {
            std::unique_lock<std::mutex> Lock(Mutex_);
            bTerminate_ = true;
        }
        Condition_.notify_all();
        // for (auto& Thread : Threads_)
        // {
        //     if (Thread.joinable())
        //     {
        //         Thread.join();
        //     }
        // }
    }

    void FThreadPool::SetThreadAffinity(std::jthread& Thread, std::size_t CoreId) const
    {
        CoreId = CoreId % PhysicalCoreCount_;
        HANDLE Handle = Thread.native_handle();
        DWORD_PTR Mask = 0;
        Mask = static_cast<DWORD_PTR>(Bit(CoreId * 2) + HyperThreadIndex_);
        SetThreadAffinityMask(Handle, Mask);
    }
} // namespace Npgs
