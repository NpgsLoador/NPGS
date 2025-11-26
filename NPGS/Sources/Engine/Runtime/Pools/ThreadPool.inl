#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <type_traits>
#include <utility>

#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    struct FThreadPool::FWorker
    {
        std::mutex                        Mutex;
        std::condition_variable           Condition;
        std::queue<std::function<void()>> Tasks;
    };

    template <typename Func, typename... Types>
    auto FThreadPool::Submit(Func&& Pred, Types&&... Args)
    {
        using FReturnType = std::invoke_result_t<Func, Types...>;

        if (Threads_.empty())
        {
            return std::future<FReturnType>();
        }

        auto Task = std::make_shared<std::packaged_task<FReturnType()>>(
        [Pred = std::forward<Func>(Pred), ...Args = std::forward<Types>(Args)]() mutable -> FReturnType
        {
            return std::invoke(std::move(Pred), std::move(Args)...);
        });
        std::future<FReturnType> Future = Task->get_future();

        std::size_t ThreadIndex = NextThreadIndex_.fetch_add(1) % MaxThreadCount_;
        auto& TargetWorker = Workers_[ThreadIndex];
        {
            std::unique_lock<std::mutex> Lock(TargetWorker->Mutex);
            TargetWorker->Tasks.emplace([Task]() -> void { (*Task)(); });
        }
        TargetWorker->Condition.notify_one();

        return Future;
    }

    NPGS_INLINE void FThreadPool::SwitchHyperThread()
    {
        HyperThreadIndex_.store(1 - HyperThreadIndex_.load());
    }

    NPGS_INLINE int FThreadPool::GetMaxThreadCount() const
    {
        return MaxThreadCount_;
    }

    template <typename DataType, typename ResultType>
    void MakeChunks(int MaxThread, std::vector<DataType>& Data,
                    std::vector<std::vector<DataType>>& DataLists,
                    std::vector<std::promise<std::vector<ResultType>>>& Promises,
                    std::vector<std::future<std::vector<ResultType>>>& ChunkFutures)
    {
        for (std::size_t i = 0; i != Data.size(); ++i)
        {
            std::size_t ThreadId = i % MaxThread;
            DataLists[ThreadId].push_back(std::move(Data[i]));
        }

        Promises.resize(MaxThread);

        for (int i = 0; i != MaxThread; ++i)
        {
            ChunkFutures.push_back(Promises[i].get_future());
        }
    }
} // namespace Npgs
