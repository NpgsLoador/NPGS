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
        : _MaxThreadCount(std::clamp(MaxThreadCount, 0, static_cast<int>(std::thread::hardware_concurrency())))
        , _PhysicalCoreCount(GetPhysicalCoreCount())
        , _bEnableHyperThread(bEnableHyperThread)
    {
        for (std::size_t i = 0; i != _MaxThreadCount; ++i)
        {
            _Threads.emplace_back([this]() -> void
            {
                while (true)
                {
                    std::function<void()> Task;
                    {
                        std::unique_lock<std::mutex> Lock(_Mutex);
                        _Condition.wait(Lock, [this]() -> bool { return !_Tasks.empty() || _bTerminate; });
                        if (_bTerminate && _Tasks.empty())
                        {
                            return;
                        }

                        Task = std::move(_Tasks.front());
                        _Tasks.pop();
                    }

                    Task();
                }
            });

            if (!_bEnableHyperThread)
            {
                SetThreadAffinity(_Threads.back(), i);
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
            std::unique_lock<std::mutex> Mutex(_Mutex);
            _bTerminate = true;
        }
        _Condition.notify_all();
        for (auto& Thread : _Threads)
        {
            if (Thread.joinable())
            {
                Thread.join();
            }
        }
    }

    void FThreadPool::SetThreadAffinity(std::thread& Thread, std::size_t CoreId) const
    {
        CoreId = CoreId % _PhysicalCoreCount;
        HANDLE Handle = Thread.native_handle();
        DWORD_PTR Mask = 0;
        Mask = static_cast<DWORD_PTR>(Bit(CoreId * 2) + _HyperThreadIndex);
        SetThreadAffinityMask(Handle, Mask);
    }
} // namespace Npgs
