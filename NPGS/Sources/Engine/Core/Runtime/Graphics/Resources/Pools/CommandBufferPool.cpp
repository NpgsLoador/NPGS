#include "CommandBufferPool.h"

namespace Npgs
{
    FCommandBufferPool::FCommandBufferPool(std::uint32_t MinAvailableBufferLimit, std::uint32_t MaxAllocatedBufferLimit,
                                           std::uint32_t PoolReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs,
                                           vk::Device Device, std::uint32_t QueueFamilyIndex)
        : Base(MinAvailableBufferLimit, MaxAllocatedBufferLimit, PoolReclaimThresholdMs, MaintenanceIntervalMs)
        , QueueFamilyIndex_(QueueFamilyIndex)
        , CommandPool_(Device, QueueFamilyIndex_, vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
    {
    }
} // namespace Npgs/
