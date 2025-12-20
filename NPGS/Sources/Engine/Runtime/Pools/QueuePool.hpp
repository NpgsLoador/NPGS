#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

#include <ankerl/unordered_dense.h>
#include <concurrentqueue/moodycamel/concurrentqueue.h>
#include <vulkan/vulkan.hpp>

namespace Npgs
{
    class FQueuePool
    {
    private:
        struct FQueueInfo
        {
            vk::Queue      Queue;
            vk::QueueFlags QueueFlags;
        };

    public:
        class FQueueGuard
        {
        public:
            FQueueGuard(FQueuePool* Pool, FQueueInfo QueueInfo);
            FQueueGuard(const FQueueGuard&) = delete;
            FQueueGuard(FQueueGuard&& Other) noexcept;
            ~FQueueGuard();

            FQueueGuard& operator=(const FQueueGuard&) = delete;
            FQueueGuard& operator=(FQueueGuard&& Other) noexcept;

            vk::Queue Release();

            vk::Queue* operator->();
            const vk::Queue* operator->() const;
            vk::Queue& operator*();
            const vk::Queue& operator*() const;

        private:
            FQueuePool* Pool_;
            FQueueInfo  QueueInfo_;
        };

    public:
        explicit FQueuePool(vk::Device Device);
        FQueuePool(const FQueuePool&) = delete;
        FQueuePool(FQueuePool&&)      = delete;
        ~FQueuePool()                 = default;

        FQueuePool& operator=(const FQueuePool&) = delete;
        FQueuePool& operator=(FQueuePool&&)      = delete;

        FQueueGuard AcquireQueue(vk::QueueFlags QueueFlags);
        void Register(vk::QueueFlags QueueFlags, std::uint32_t QueueFamilyIndex, std::uint32_t QueueCount);

    private:
        struct FQueueFamilyPool
        {
            moodycamel::ConcurrentQueue<vk::Queue>               Queues;
            std::mutex                                           Mutex;
            std::queue<std::shared_ptr<std::condition_variable>> WaitQueue;
            std::atomic<std::size_t>                             BusyQueueCount{};
            std::size_t                                          TotalQueueCount{};
        };

        struct FQueueHash
        {
            std::size_t operator()(vk::QueueFlags QueueFlags) const;
        };

    private:
        void ReleaseQueue(FQueueInfo&& QueueInfo);

    private:
        ankerl::unordered_dense::map<vk::QueueFlags, std::uint32_t, FQueueHash>        QueueFamilyIndices_;
        ankerl::unordered_dense::map<std::uint32_t, std::unique_ptr<FQueueFamilyPool>> QueueFamilyPools_;
        vk::Device                                                                     Device_;
    };
} // namespace Npgs

#include "QueuePool.inl"
