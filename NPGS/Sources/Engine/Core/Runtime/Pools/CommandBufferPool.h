#pragma once

#include <cstddef>
#include <cstdint>

#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"
#include "Engine/Core/Runtime/Pools/ResourcePool.hpp"

namespace Npgs
{
    struct FCommandBufferCreateInfo
    {
        vk::CommandBufferLevel CommandBufferLevel{ vk::CommandBufferLevel::ePrimary };
        std::uint32_t QueueFamilyIndex;
    };

    struct FCommandBufferInfo : public TResourceInfo<FVulkanCommandBuffer>
    {
        vk::CommandBufferLevel Level{ vk::CommandBufferLevel::ePrimary };
    };

    class FCommandBufferPool : public TResourcePool<FVulkanCommandBuffer, FCommandBufferCreateInfo, FCommandBufferInfo>
    {
    public:
        using Base = TResourcePool<FVulkanCommandBuffer, FCommandBufferCreateInfo, FCommandBufferInfo>;
        using FBufferGuard = Base::FResourceGuard;

    public:
        FCommandBufferPool(std::uint32_t MinAvailableBufferLimit, std::uint32_t MaxAllocatedBufferLimit,
                           std::uint32_t PoolReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs,
                           vk::Device Device, std::uint32_t QueueFamilyIndex);

        FBufferGuard AcquireBuffer(vk::CommandBufferLevel CommandBufferLevel);

    private:
        void CreateResource(const FCommandBufferCreateInfo& CreateInfo) override;
        bool HandleResourceEmergency(FCommandBufferInfo& LowUsageResource, const FCommandBufferCreateInfo& CreateInfo) override;

    private:
        std::uint32_t QueueFamilyIndex_;
        FVulkanCommandPool CommandPool_;
    };
} // namespace Npgs

#include "CommandBufferPool.inl"
