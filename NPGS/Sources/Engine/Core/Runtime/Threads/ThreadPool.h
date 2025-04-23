#pragma once

#include <cstddef>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace Npgs
{
    class FThreadPool
    {
    public:
        FThreadPool(int MaxThreadCount = 0, bool bEnableHyperThread = false);
        FThreadPool(const FThreadPool&) = default;
        FThreadPool(FThreadPool&&)      = default;
        ~FThreadPool();

        FThreadPool& operator=(const FThreadPool&) = default;
        FThreadPool& operator=(FThreadPool&&)      = default;

        template <typename Func, typename... Args>
        auto Submit(Func&& Pred, Args&&... TaskArgs);

        void Terminate();
        void SwitchHyperThread();
        int GetMaxThreadCount() const;

    private:
        void SetThreadAffinity(std::thread& Thread, std::size_t CoreId) const;

    private:
        std::vector<std::thread>          _Threads;
        std::queue<std::function<void()>> _Tasks;
        std::mutex                        _Mutex;
        std::condition_variable           _Condition;
        int                               _MaxThreadCount;
        int                               _PhysicalCoreCount;
        int                               _HyperThreadIndex{ 0 };
        bool                              _bEnableHyperThread;
        bool                              _bTerminate{ false };
    };

    template <typename DataType, typename ResultType>
    void MakeChunks(int MaxThread, std::vector<DataType>& Data, std::vector<std::vector<DataType>>& DataLists,
                    std::vector<std::promise<std::vector<ResultType>>>& Promises,
                    std::vector<std::future<std::vector<ResultType>>>& ChunkFutures);
} // namespace Npgs

#include "ThreadPool.inl"
