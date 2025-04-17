#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Resources/Pools/ResourcePool.hpp"
#include "Engine/Core/Runtime/Graphics/Resources/StagingBuffer.h"

namespace Npgs
{
    struct FStagingBufferCreateInfo
    {
        vk::DeviceSize Size;
        const VmaAllocationCreateInfo* AllocationCreateInfo;
    };

    class FStagingBufferPool : public TResourcePool<FStagingBuffer, FStagingBufferCreateInfo>
    {
    private:
        struct FStagingBufferInfo : public FResourceInfo
        {
            vk::DeviceSize Size{};
            bool bAllocatedByVma{ false };
        };

    public:
        using Base = TResourcePool<FStagingBuffer, FStagingBufferCreateInfo>;
        using FBufferGuard = Base::FResourceGuard;

        FStagingBufferPool(std::uint32_t MinBufferLimit, std::uint32_t MaxBufferLimit, std::uint32_t BufferReclaimThresholdMs,
                           std::uint32_t MaintenanceIntervalMs, bool bUsingVma = true);

        FBufferGuard AcquireBuffer(vk::DeviceSize RequestedSize);

    private:
        void CreateResource(const FStagingBufferCreateInfo& CreateInfo) override;
        bool HandleResourceEmergency(FResourceInfo& LowUsageResource, const FStagingBufferCreateInfo& CreateInfo) override;
        void ReleaseResource(FStagingBuffer* Buffer, std::size_t UsageCount) override;
        void OptimizeResourceCount() override;
        void RemoveOversizedBuffers(vk::DeviceSize Threshold);
        vk::DeviceSize AlignSize(vk::DeviceSize RequestedSize);

    private:
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

#include "StagingBufferPool.inl"
