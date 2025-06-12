#include "QueuePool.h"

#include <stdexcept>
#include <utility>

#include <vulkan/vulkan_to_string.hpp>
#include "Engine/Utils/Logger.h"

namespace Npgs
{
    FQueuePool::FQueueGuard::FQueueGuard(FQueuePool* Pool, FQueueInfo&& QueueInfo)
        : Pool_(Pool)
        , QueueInfo_(std::exchange(QueueInfo, {}))
    {
    }

    FQueuePool::FQueueGuard::FQueueGuard(FQueueGuard&& Other) noexcept
        : Pool_(std::exchange(Other.Pool_, nullptr))
        , QueueInfo_(std::exchange(Other.QueueInfo_, {}))
    {
    }

    FQueuePool::FQueueGuard::~FQueueGuard()
    {
        if (Pool_ != nullptr && QueueInfo_.Queue)
        {
            Pool_->ReleaseQueue(std::move(QueueInfo_));
        }
    }

    FQueuePool::FQueueGuard& FQueuePool::FQueueGuard::operator=(FQueueGuard&& Other) noexcept
    {
        if (this != &Other)
        {
            Pool_      = std::exchange(Other.Pool_, nullptr);
            QueueInfo_ = std::exchange(Other.QueueInfo_, {});
        }

        return *this;
    }

    vk::Queue FQueuePool::FQueueGuard::Release()
    {
        Pool_ = nullptr;
        return QueueInfo_.Queue;
    }

    FQueuePool::FQueuePool(vk::Device Device)
        : Device_(Device)
    {
    }

    FQueuePool::FQueueGuard FQueuePool::AcquireQueue(vk::QueueFlags QueueFlags)
    {
        std::uint32_t Index = QueueFamilyIndices_.at(QueueFlags);
        auto& Pool = QueueFamilyPools_.at(Index);

        vk::Queue Queue;
        if (Pool.Queues.try_dequeue(Queue))
        {
            ++Pool.BusyQueueCount;
            return FQueueGuard(this, { Queue, QueueFlags });
        }

        auto& Mutex     = Pool.Mutex;
        auto& WaitQueue = Pool.WaitQueue;
        std::unique_lock Lock(Mutex);
        if (Pool.BusyQueueCount.load() >= Pool.TotalQueueCount)
        {
            auto CurrentThread = std::make_shared<std::condition_variable>();
            WaitQueue.push(CurrentThread);

            CurrentThread->wait(Lock, [this, &Pool, &Queue]() -> bool { return Pool.Queues.try_dequeue(Queue); });

            ++Pool.BusyQueueCount;
            return FQueueGuard(this, { Queue, QueueFlags });
        }

        throw std::runtime_error("Failed to acquire queue: free queue existing but dequeue failed.");
    }

    void FQueuePool::Register(vk::QueueFlags QueueFlags, std::uint32_t QueueFamilyIndex, std::uint32_t QueueCount)
    {
        if (QueueFamilyIndices_.find(QueueFlags) != QueueFamilyIndices_.end())
        {
            NpgsCoreWarn("Queue family {} already registered: {}", QueueFamilyIndex, vk::to_string(QueueFlags));
            return;
        }

        QueueFamilyIndices_[QueueFlags] = QueueFamilyIndex;
        auto& Pool = QueueFamilyPools_[QueueFamilyIndex];
        Pool.TotalQueueCount = QueueCount;
        for (std::uint32_t i = 0; i != QueueCount; ++i)
        {
            vk::Queue Queue = Device_.getQueue(QueueFamilyIndex, i);
            Pool.Queues.enqueue(std::move(Queue));
        }
    }

    void FQueuePool::ReleaseQueue(FQueueInfo&& QueueInfo)
    {
        std::uint32_t Index = QueueFamilyIndices_.at(QueueInfo.QueueFlags);
        auto& Pool          = QueueFamilyPools_.at(Index);
        auto& Mutex         = Pool.Mutex;
        auto& WaitQueue     = Pool.WaitQueue;

        if (Pool.Queues.try_enqueue(QueueInfo.Queue))
        {
            --Pool.BusyQueueCount;
            std::shared_ptr<std::condition_variable> TopThread = nullptr;
            {
                std::lock_guard Lock(Mutex);
                if (!WaitQueue.empty())
                {
                    TopThread = WaitQueue.front();
                    WaitQueue.pop();
                }
            }

            if (TopThread != nullptr)
            {
                TopThread->notify_one();
            }

            return;
        }

        throw std::runtime_error("Failed to release queue: queue enqueue failed.");
    }
} // namespace Npgs
