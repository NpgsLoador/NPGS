#pragma once

#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace Npgs
{
    class FQueuePool
    {
    public:
        FQueuePool()                  = default;
        FQueuePool(const FQueuePool&) = delete;
        FQueuePool(FQueuePool&&)      = delete;
        ~FQueuePool()                 = default;

        FQueuePool& operator=(const FQueuePool&) = delete;
        FQueuePool& operator=(FQueuePool&&)      = delete;

    private:
        std::unordered_map<vk::QueueFlagBits, std::uint32_t>      _QueueFamilyIndices;
        std::unordered_map<std::uint32_t, std::vector<vk::Queue>> _Queues;
    };
} // namespace Npgs

#include "QueuePool.inl"
