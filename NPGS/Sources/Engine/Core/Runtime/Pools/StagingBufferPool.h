#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Resources/StagingBuffer.h"
#include "Engine/Core/Runtime/Pools/ResourcePool.hpp"

namespace Npgs
{
    struct FStagingBufferCreateInfo
    {
        vk::DeviceSize Size;
    };

    struct FStagingBufferInfo : public TResourceInfo<FStagingBuffer>
    {
        vk::DeviceSize Size{};
    };

    class FStagingBufferPool : public TResourcePool<FStagingBuffer, FStagingBufferCreateInfo, FStagingBufferInfo>
    {
    public:
        enum class EPoolUsage : std::uint8_t
        {
            kSubmit = 0,
            kFetch  = 1
        };

    public:
        using Base = TResourcePool<FStagingBuffer, FStagingBufferCreateInfo, FStagingBufferInfo>;
        using FBufferGuard = Base::FResourceGuard;

        FStagingBufferPool(vk::PhysicalDevice PhysicalDevice, vk::Device Device, VmaAllocator Allocator,
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
        vk::PhysicalDevice      PhysicalDevice_;
        vk::Device              Device_;
        VmaAllocator            Allocator_;
        VmaAllocationCreateInfo AllocationCreateInfo_;

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
