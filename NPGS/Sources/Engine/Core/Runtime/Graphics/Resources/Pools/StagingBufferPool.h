#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Resources/Pools/ResourcePool.hpp"
#include "Engine/Core/Runtime/Graphics/Resources/StagingBuffer.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"

namespace Npgs
{
    struct FStagingBufferCreateInfo
    {
        vk::DeviceSize Size;
        const VmaAllocationCreateInfo* AllocationCreateInfo;
    };

    struct FStagingBufferInfo : public TResourceInfo<FStagingBuffer>
    {
        vk::DeviceSize Size{};
        bool bAllocatedByVma{ false };
    };

    class FStagingBufferPool : public TResourcePool<FStagingBuffer, FStagingBufferCreateInfo, FStagingBufferInfo>
    {
    public:
        enum class EPoolUsage : std::uint8_t
        {
            kSubmit,
            kFetch
        };

    public:
        using Base = TResourcePool<FStagingBuffer, FStagingBufferCreateInfo, FStagingBufferInfo>;
        using FBufferGuard = Base::FResourceGuard;

        FStagingBufferPool(FVulkanContext* VulkanContext, VmaAllocator Allocator,
                           std::uint32_t MinAvailableBufferLimit, std::uint32_t MaxAllocatedBufferLimit,
                           std::uint32_t BufferReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs,
                           EPoolUsage PoolUsage, bool bUsingVma = true);

        FBufferGuard AcquireBuffer(vk::DeviceSize RequestedSize);

    private:
        void CreateResource(const FStagingBufferCreateInfo& CreateInfo) override;
        bool HandleResourceEmergency(FStagingBufferInfo& LowUsageResource, const FStagingBufferCreateInfo& CreateInfo) override;
        void ReleaseResource(FStagingBuffer* Buffer, std::size_t UsageCount) override;
        void OptimizeResourceCount() override;
        void RemoveOversizedBuffers(vk::DeviceSize Threshold);
        vk::DeviceSize AlignSize(vk::DeviceSize RequestedSize);

    private:
        FVulkanContext*         _VulkanContext;
        VmaAllocator            _Allocator;
        VmaAllocationCreateInfo _AllocationCreateInfo;
        bool                    _bUsingVma{ true };

        static constexpr std::array kSizeTiers
        {
            64ull   * 1024,
            256ull  * 1024,
            1ull    * 1024 * 1024,
            4ull    * 1024 * 1024,
            16ull   * 1024 * 1024,
            64ull   * 1024 * 1024,
            256ull  * 1024 * 1024,
            1024ull * 1024 * 1024,
            4096ull * 1024 * 1024
        };
    };
} // namespace Npgs
