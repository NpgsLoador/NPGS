#pragma once

#include <cstddef>
#include <atomic>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace Npgs
{
    class FThreadPool
    {
    private:
        struct FWorker
        {
            std::mutex                        Mutex;
            std::condition_variable           Condition;
            std::queue<std::function<void()>> Tasks;
        };

    public:
        FThreadPool(int MaxThreadCount = 0, bool bEnableHyperThread = false);
        FThreadPool(const FThreadPool&) = delete;
        FThreadPool(FThreadPool&&)      = delete;
        ~FThreadPool();

        FThreadPool& operator=(const FThreadPool&) = delete;
        FThreadPool& operator=(FThreadPool&&)      = delete;

        template <typename Func, typename... Types>
        auto Submit(Func&& Pred, Types&&... Args);

        void SwitchHyperThread();
        int  GetMaxThreadCount() const;

    private:
        void SetThreadAffinity(std::jthread& Thread, std::size_t CoreId) const;

    private:
        std::vector<std::unique_ptr<FWorker>> Workers_;
        std::vector<std::jthread>             Threads_;
        std::atomic<std::size_t>              NextThreadIndex_{};
        int                                   MaxThreadCount_;
        int                                   PhysicalCoreCount_;
        std::atomic<int>                      HyperThreadIndex_{};
        std::atomic<bool>                     bTerminate_{ false };
        bool                                  bEnableHyperThread_;
    };

    template <typename DataType, typename ResultType>
    void MakeChunks(int MaxThread, std::vector<DataType>& Data,
                    std::vector<std::vector<DataType>>& DataLists,
                    std::vector<std::promise<std::vector<ResultType>>>& Promises,
                    std::vector<std::future<std::vector<ResultType>>>& ChunkFutures);
} // namespace Npgs

#include "ThreadPool.inl"
