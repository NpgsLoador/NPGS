#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Runtime/Graphics/Resources/StagingBuffer.hpp"
#include "Engine/Runtime/Pools/ResourcePool.hpp"

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
                           std::uint32_t BufferReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs, EPoolUsage PoolUsage);

        FBufferGuard AcquireBuffer(vk::DeviceSize RequestedSize);

    private:
        std::unique_ptr<FStagingBufferInfo> CreateResource(const FStagingBufferCreateInfo& CreateInfo) override;
        bool HandleResourceEmergency(const FStagingBufferCreateInfo& CreateInfo, FStagingBufferInfo& LowUsageResource) override;
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
            64uz   * 1024,
            256uz  * 1024,
            1uz    * 1024 * 1024,
            4uz    * 1024 * 1024,
            16uz   * 1024 * 1024,
            64uz   * 1024 * 1024,
            256uz  * 1024 * 1024,
            1024uz * 1024 * 1024,
            4096uz * 1024 * 1024
        };
    };
} // namespace Npgs
