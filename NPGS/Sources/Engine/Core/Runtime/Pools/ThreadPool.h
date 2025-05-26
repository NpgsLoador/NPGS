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

        template <typename Func, typename... Types>
        auto Submit(Func&& Pred, Types&&... Args);

        void Terminate();
        void SwitchHyperThread();
        int GetMaxThreadCount() const;

    private:
        void SetThreadAffinity(std::thread& Thread, std::size_t CoreId) const;

    private:
        std::vector<std::thread>          Threads_;
        std::queue<std::function<void()>> Tasks_;
        std::mutex                        Mutex_;
        std::condition_variable           Condition_;
        int                               MaxThreadCount_;
        int                               PhysicalCoreCount_;
        int                               HyperThreadIndex_{};
        bool                              bEnableHyperThread_;
        bool                              bTerminate_{ false };
    };

    template <typename DataType, typename ResultType>
    void MakeChunks(int MaxThread, std::vector<DataType>& Data, std::vector<std::vector<DataType>>& DataLists,
                    std::vector<std::promise<std::vector<ResultType>>>& Promises,
                    std::vector<std::future<std::vector<ResultType>>>& ChunkFutures);
} // namespace Npgs

#include "ThreadPool.inl"
