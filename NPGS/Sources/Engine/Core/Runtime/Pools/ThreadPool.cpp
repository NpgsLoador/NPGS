#include "stdafx.h"
#include "ThreadPool.hpp"

#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <mutex>

#include <Windows.h>
#include "Engine/Core/Base/Base.hpp"

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

                Length   -= BufferPtr->Size;
                BufferPtr = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(reinterpret_cast<std::uint8_t*>(BufferPtr) + BufferPtr->Size);
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
        Threads_.reserve(MaxThreadCount_);
        Workers_.reserve(MaxThreadCount_);

        for (std::size_t i = 0; i != MaxThreadCount_; ++i)
        {
            Workers_.emplace_back(std::make_unique<FWorker>());
        }

        for (std::size_t i = 0; i != MaxThreadCount_; ++i)
        {
            Threads_.emplace_back([this, i]() -> void
            {
                FWorker& Worker = *Workers_[i];
                while (true)
                {
                    std::function<void()> Task;
                    {
                        std::unique_lock Lock(Worker.Mutex);
                        Worker.Condition.wait(Lock, [this, &Worker]() -> bool
                        {
                            return !Worker.Tasks.empty() || bTerminate_.load();
                        });

                        if (bTerminate_.load() && Worker.Tasks.empty())
                        {
                            return;
                        }

                        Task = std::move(Worker.Tasks.front());
                        Worker.Tasks.pop();
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
        bTerminate_.store(true);
        for (auto& Worker : Workers_)
        {
            Worker->Condition.notify_one();
        }
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
