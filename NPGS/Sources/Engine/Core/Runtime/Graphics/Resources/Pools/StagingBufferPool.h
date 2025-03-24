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
    using BufferGuard = Base::FResourceGuard;

    FStagingBufferPool(std::uint32_t MinBufferLimit, std::uint32_t MaxBufferLimit,
                       std::uint32_t BufferReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs);

private:
    void CreateResource(const FStagingBufferCreateInfo& CreateInfo) override;
    vk::DeviceSize AlignSize(vk::DeviceSize RequestedSize);

private:
    VmaAllocationCreateInfo _AllocationCreateInfo;

    static constexpr std::array kSizeTiers = {
        64ull  * 1024,
        256ull * 1024,
        1ull   * 1024 * 1024,
        4ull   * 1024 * 1024,
        16ull  * 1024 * 1024,
        64ull  * 1024 * 1024,
        256ull * 1024 * 1024
    };
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "StagingBufferPool.inl"
