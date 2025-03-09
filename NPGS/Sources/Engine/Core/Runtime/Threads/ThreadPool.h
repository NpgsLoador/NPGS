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

#include "Engine/Core/Base/Base.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_THREAD_BEGIN

class FThreadPool
{
public:
    template <typename Func, typename... Args>
    auto Submit(Func&& Pred, Args&&... Params);

    void Terminate();
    void ChangeHyperThread();
    int GetMaxThreadCount() const;

    static FThreadPool* GetInstance();

private:
    FThreadPool();
    FThreadPool(const FThreadPool&) = delete;
    FThreadPool(FThreadPool&&)      = delete;
    ~FThreadPool();

    FThreadPool& operator=(const FThreadPool&) = delete;
    FThreadPool& operator=(FThreadPool&&)      = delete;

    void SetThreadAffinity(std::thread& Thread, std::size_t CoreId) const;

private:
    std::vector<std::thread>          _Threads;
    std::queue<std::function<void()>> _Tasks;
    std::mutex                        _Mutex;
    std::condition_variable           _Condition;
    int                               _kMaxThreadCount;
    int                               _kPhysicalCoreCount;
    int                               _kHyperThreadIndex;
    bool                              _Terminate{ false };
};

template <typename DataType, typename ResultType>
void MakeChunks(int MaxThread, std::vector<DataType>& Data, std::vector<std::vector<DataType>>& DataLists,
                std::vector<std::promise<std::vector<ResultType>>>& Promises,
                std::vector<std::future<std::vector<ResultType>>>& ChunkFutures);

_THREAD_END
_RUNTIME_END
_NPGS_END

#include "ThreadPool.inl"
