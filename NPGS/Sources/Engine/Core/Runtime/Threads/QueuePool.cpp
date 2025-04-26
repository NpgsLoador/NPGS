#include "QueuePool.h"

#include <stdexcept>
#include <utility>

#include <vulkan/vulkan_to_string.hpp>

#include "Engine/Utils/Logger.h"

namespace Npgs
{
    FQueuePool::FQueueGuard::FQueueGuard(FQueuePool* Pool, FQueueInfo&& QueueInfo)
        : _Pool(Pool)
        , _QueueInfo(std::exchange(QueueInfo, {}))
    {
    }

    FQueuePool::FQueueGuard::FQueueGuard(FQueueGuard&& Other) noexcept
        : _Pool(std::exchange(Other._Pool, nullptr))
        , _QueueInfo(std::exchange(Other._QueueInfo, {}))
    {
    }

    FQueuePool::FQueueGuard::~FQueueGuard()
    {
        if (_Pool != nullptr && _QueueInfo.Queue)
        {
            _Pool->ReleaseQueue(std::move(_QueueInfo));
        }
    }

    FQueuePool::FQueueGuard& FQueuePool::FQueueGuard::operator=(FQueueGuard&& Other) noexcept
    {
        if (this != &Other)
        {
            _Pool      = std::exchange(Other._Pool, nullptr);
            _QueueInfo = std::exchange(Other._QueueInfo, {});
        }

        return *this;
    }

    vk::Queue FQueuePool::FQueueGuard::Release()
    {
        _Pool = nullptr;
        return _QueueInfo.Queue;
    }

    FQueuePool::FQueuePool(vk::Device Device)
        : _Device(Device)
    {
    }

    FQueuePool::FQueueGuard FQueuePool::AcquireQueue(vk::QueueFlags QueueFlags)
    {
        std::uint32_t Index = _QueueFamilyIndices.at(QueueFlags);
        auto& Pool = _QueueFamilyPools.at(Index);

        vk::Queue Queue;
        if (Pool.Queues.try_dequeue(Queue))
        {
            ++Pool.BusyQueueCount;
            return FQueueGuard(this, { Queue, QueueFlags });
        }

        std::unique_lock Lock(_Mutex);
        if (Pool.BusyQueueCount.load() >= Pool.TotalQueueCount)
        {
            auto CurrentThread = std::make_shared<std::condition_variable>();
            _WaitQueue.push(CurrentThread);

            CurrentThread->wait(Lock, [this, &Pool, &Queue]() -> bool { return Pool.Queues.try_dequeue(Queue); });

            ++Pool.BusyQueueCount;
            return FQueueGuard(this, { Queue, QueueFlags });
        }

        throw std::runtime_error("Failed to acquire queue: free queue existing but dequeue failed.");
    }

    void FQueuePool::Register(vk::QueueFlags QueueFlags, std::uint32_t QueueFamilyIndex, std::uint32_t QueueCount)
    {
        if (_QueueFamilyIndices.find(QueueFlags) != _QueueFamilyIndices.end())
        {
            NpgsCoreWarn("Queue family {} already registered: {}", QueueFamilyIndex, vk::to_string(QueueFlags));
            return;
        }

        _QueueFamilyIndices[QueueFlags] = QueueFamilyIndex;
        auto& Pool = _QueueFamilyPools[QueueFamilyIndex];
        Pool.TotalQueueCount = QueueCount;
        for (std::uint32_t i = 0; i != QueueCount; ++i)
        {
            vk::Queue Queue = _Device.getQueue(QueueFamilyIndex, i);
            Pool.Queues.enqueue(std::move(Queue));
        }
    }

    void FQueuePool::ReleaseQueue(FQueueInfo&& QueueInfo)
    {
        std::uint32_t Index = _QueueFamilyIndices.at(QueueInfo.QueueFlags);
        auto& Pool = _QueueFamilyPools.at(Index);

        if (Pool.Queues.try_enqueue(QueueInfo.Queue))
        {
            if (!_WaitQueue.empty())
            {
                auto TopThread = _WaitQueue.front();
                _WaitQueue.pop();
                TopThread->notify_one();
            }
            --Pool.BusyQueueCount;

            return;
        }

        throw std::runtime_error("Failed to release queue: queue enqueue failed.");
    }
} // namespace Npgs
