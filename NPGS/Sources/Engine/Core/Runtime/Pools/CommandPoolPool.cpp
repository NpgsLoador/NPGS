#include "stdafx.h"
#include "CommandPoolPool.h"

namespace Npgs
{
    FCommandPoolPool::FCommandPoolPool(std::uint32_t MinAvailableBufferLimit, std::uint32_t MaxAllocatedBufferLimit,
                                       std::uint32_t PoolReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs,
                                       vk::Device Device, std::uint32_t QueueFamilyIndex)
        : Base(MinAvailableBufferLimit, MaxAllocatedBufferLimit, PoolReclaimThresholdMs, MaintenanceIntervalMs)
        , Device_(Device)
        , QueueFamilyIndex_(QueueFamilyIndex)
    {
    }

    void FCommandPoolPool::CreateResource(const FCommandPoolCreateInfo& CreateInfo)
    {
        FVulkanCommandPool CommandPool(Device_, QueueFamilyIndex_, CreateInfo.Flags);
        auto ResourceInfoPtr               = std::make_unique<FCommandPoolInfo>();
        ResourceInfoPtr->Resource          = std::make_unique<FVulkanCommandPool>(std::move(CommandPool));
        ResourceInfoPtr->LastUsedTimestamp = GetCurrentTimeMs();
        ResourceInfoPtr->UsageCount        = 1;

        AvailableResources_.push_back(std::move(ResourceInfoPtr));
    }
} // namespace Npgs
