// This manager is a "pool" instead of a "manager"
// Named manager because the "vk::CommandPool" has "pool" item.

#pragma once

#include <cstddef>
#include <cstdint>

#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Resources/Pools/ResourcePool.hpp"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

namespace Npgs
{
    struct FCommandPoolCreateInfo
    {
        vk::CommandPoolCreateFlags Flags;
        std::uint32_t QueueFamilyIndex;
    };

    class FCommandPoolManager : public TResourcePool<FVulkanCommandPool, FCommandPoolCreateInfo>
    {
    public:
        using Base          = TResourcePool<FVulkanCommandPool, FCommandPoolCreateInfo>;
        using FPoolGuard    = Base::FResourceGuard;
        using FResourceInfo = TResourceInfo<FVulkanCommandPool>;

    public:
        FCommandPoolManager(std::uint32_t MinPoolLimit, std::uint32_t MaxPoolLimit, std::uint32_t PoolReclaimThresholdMs,
                            std::uint32_t MaintenanceIntervalMs, vk::Device Device, std::uint32_t QueueFamilyIndex);

        FPoolGuard AcquirePool();

    private:
        void CreateResource(const FCommandPoolCreateInfo& CreateInfo) override;
        bool HandleResourceEmergency(FResourceInfo& LowUsageResource, const FCommandPoolCreateInfo& CreateInfo) override;

    private:
        vk::Device    _Device;
        std::uint32_t _QueueFamilyIndex;
    };
} // namespace Npgs

#include "CommandPoolManager.inl"
