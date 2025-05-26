#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    template <typename Func, typename... Types>
    auto FThreadPool::Submit(Func&& Pred, Types&&... Args)
    {
        using FReturnType = std::invoke_result_t<Func, Types...>;
        auto Task = std::make_shared<std::packaged_task<FReturnType()>>(
            std::bind(std::forward<Func>(Pred), std::forward<Types>(Args)...));
        std::future<FReturnType> Future = Task->get_future();
        {
            std::unique_lock<std::mutex> Lock(Mutex_);
            Tasks_.emplace([Task]() -> void { (*Task)(); });
        }
        Condition_.notify_one();
        return Future;
    }

    NPGS_INLINE void FThreadPool::SwitchHyperThread()
    {
        HyperThreadIndex_ = 1 - HyperThreadIndex_;
    }

    NPGS_INLINE int FThreadPool::GetMaxThreadCount() const
    {
        return MaxThreadCount_;
    }

    template <typename DataType, typename ResultType>
    void MakeChunks(int MaxThread, std::vector<DataType>& Data, std::vector<std::vector<DataType>>& DataLists,
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
