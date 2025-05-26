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

    void FCommandBufferPool::CreateResource(const FCommandBufferCreateInfo& CreateInfo)
    {
        FVulkanCommandBuffer CommandBuffer;
        CommandPool_.AllocateBuffer(CreateInfo.CommandBufferLevel, CommandBuffer);
        auto ResourceInfoPtr               = std::make_unique<FCommandBufferInfo>();
        ResourceInfoPtr->Resource          = std::make_unique<FVulkanCommandBuffer>(std::move(CommandBuffer));
        ResourceInfoPtr->LastUsedTimestamp = GetCurrentTimeMs();
        ResourceInfoPtr->UsageCount        = 1;

        AvailableResources_.push_back(std::move(ResourceInfoPtr));
    }
} // namespace Npgs/
