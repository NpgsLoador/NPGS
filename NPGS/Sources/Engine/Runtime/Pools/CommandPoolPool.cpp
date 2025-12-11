#include "stdafx.h"
#include "CommandPoolPool.hpp"

#include <format>
#include <string>

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

    std::unique_ptr<FCommandPoolInfo> FCommandPoolPool::CreateResource(const FCommandPoolCreateInfo& CreateInfo)
    {
        std::uint64_t ResourceId = NextResourceId_.fetch_add(1, std::memory_order::relaxed);
        std::string Name = std::format("CommandPool_PoolInst_{:#x}_ID_{}", reinterpret_cast<std::uintptr_t>(this), ResourceId);
        FVulkanCommandPool CommandPool(Device_, Name, QueueFamilyIndex_, CreateInfo.Flags);

        auto ResourceInfoPtr               = std::make_unique<FCommandPoolInfo>();
        ResourceInfoPtr->Resource          = std::make_unique<FVulkanCommandPool>(std::move(CommandPool));
        ResourceInfoPtr->LastUsedTimestamp = GetCurrentTimeMs();
        ResourceInfoPtr->UsageCount        = 1;

        return ResourceInfoPtr;
    }
} // namespace Npgs
