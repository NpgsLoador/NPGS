#pragma once

#include <cstddef>
#include <cstdint>

#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"
#include "Engine/Core/Runtime/Pools/ResourcePool.hpp"

namespace Npgs
{
    struct FCommandPoolCreateInfo
    {
        vk::CommandPoolCreateFlags Flags{ vk::CommandPoolCreateFlagBits::eResetCommandBuffer };
    };

    struct FCommandPoolInfo : public TResourceInfo<FVulkanCommandPool>
    {
        vk::CommandBufferLevel Level{ vk::CommandBufferLevel::ePrimary };
    };

    class FCommandPoolPool : public TResourcePool<FVulkanCommandPool, FCommandPoolCreateInfo, FCommandPoolInfo>
    {
    public:
        using Base = TResourcePool<FVulkanCommandPool, FCommandPoolCreateInfo, FCommandPoolInfo>;
        using FPoolGuard = Base::FResourceGuard;

    public:
        FCommandPoolPool(std::uint32_t MinAvailableBufferLimit, std::uint32_t MaxAllocatedBufferLimit,
                         std::uint32_t PoolReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs,
                         vk::Device Device, std::uint32_t QueueFamilyIndex);

        FPoolGuard AcquirePool(vk::CommandPoolCreateFlags Flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

    private:
        void CreateResource(const FCommandPoolCreateInfo& CreateInfo) override;
        bool HandleResourceEmergency(FCommandPoolInfo& LowUsageResource, const FCommandPoolCreateInfo& CreateInfo) override;
        void OnReleaseResource(FCommandPoolInfo& ResourceInfo) override;

    private:
        vk::Device    Device_;
        std::uint32_t QueueFamilyIndex_;
    };
} // namespace Npgs

#include "CommandPoolPool.inl"
