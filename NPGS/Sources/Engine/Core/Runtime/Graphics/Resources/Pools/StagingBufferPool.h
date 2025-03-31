#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Resources/Resources.h"
#include "Engine/Core/Runtime/Graphics/Resources/Pools/ResourcePool.hpp"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

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
        vk::DeviceSize Size;
        bool bAllocatedByVma;
    };

public:
    using Base = TResourcePool<FStagingBuffer, FStagingBufferCreateInfo>;
    using FBufferGuard = Base::FResourceGuard;

    FStagingBufferPool(std::uint32_t MinBufferLimit, std::uint32_t MaxBufferLimit,
                       std::uint32_t BufferReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs);

    FBufferGuard AcquireBuffer(vk::DeviceSize RequestedSize, const VmaAllocationCreateInfo* AllocationCreateInfo = nullptr);

private:
    void CreateResource(const FStagingBufferCreateInfo& CreateInfo) override;
    bool HandleResourceEmergency(FResourceInfo& LowUsageResource, const FStagingBufferCreateInfo& CreateInfo) override;
    vk::DeviceSize AlignSize(vk::DeviceSize RequestedSize);
    void TryPreallocateBuffers(vk::DeviceSize RequestedSize, bool bAllocatedByVma);

private:
    VmaAllocationCreateInfo _AllocationCreateInfo;

    static constexpr std::array kSizeTiers = {
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

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "StagingBufferPool.inl"
