#include "StagingBufferPool.h"

#include <algorithm>
#include <utility>

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

FStagingBufferPool::FStagingBufferPool(std::uint32_t MinBufferLimit, std::uint32_t MaxBufferLimit,
                                       std::uint32_t BufferReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs)
    : Base(MinBufferLimit, MaxBufferLimit, BufferReclaimThresholdMs, MaintenanceIntervalMs)
{
    _AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    std::array<vk::DeviceSize, 3> InitialSizes{ kSizeTiers[0], kSizeTiers[1], kSizeTiers[2] };
    std::size_t InitialCount = std::min(static_cast<std::size_t>(MinBufferLimit), 2 * InitialSizes.size());
    for (std::size_t i = 0; i != InitialCount; ++i)
    {
        FStagingBufferCreateInfo CreateInfo
        {
            .Size = InitialSizes[i % InitialSizes.size()],
            .AllocationCreateInfo = i >= InitialSizes.size() ? &_AllocationCreateInfo : nullptr
        };

        CreateResource(CreateInfo);
    }
}

FStagingBufferPool::FBufferGuard
FStagingBufferPool::AcquireBuffer(vk::DeviceSize RequestedSize, const VmaAllocationCreateInfo* AllocationCreateInfo)
{
    bool bAllocatedByVma = (AllocationCreateInfo != nullptr);
    vk::DeviceSize AlignedSize = AlignSize(RequestedSize);

    FStagingBufferCreateInfo CreateInfo{ AlignedSize, AllocationCreateInfo };
    return AcquireResource(CreateInfo, [=](const FResourceInfo& BaseInfo) -> bool
    {
        auto& Info = static_cast<const FStagingBufferInfo&>(BaseInfo);
        return Info.Size >= RequestedSize &&
              (Info.Size <= AlignedSize * 2 || Info.Size <= RequestedSize + 1ull * 1024 * 1024) &&
               Info.bAllocatedByVma == bAllocatedByVma;
    });
}

void FStagingBufferPool::CreateResource(const FStagingBufferCreateInfo& CreateInfo)
{
    FStagingBufferInfo BufferInfo;

    vk::DeviceSize AlignedSize = AlignSize(CreateInfo.Size);
    if (CreateInfo.AllocationCreateInfo != nullptr)
    {
        vk::BufferCreateInfo BufferCreateInfo({}, AlignedSize, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);
        BufferInfo.Resource = std::make_unique<FStagingBuffer>(_AllocationCreateInfo, BufferCreateInfo);
        BufferInfo.bAllocatedByVma = true;
    }
    else
    {
        BufferInfo.Resource = std::make_unique<FStagingBuffer>(AlignedSize);
        BufferInfo.bAllocatedByVma = false;
    }

    BufferInfo.Resource->GetMemory().SetPersistentMapping(true);
    BufferInfo.Size = AlignedSize;
    BufferInfo.LastUsedTimestamp = GetCurrentTimeMs();
    BufferInfo.UsageCount = 1;

    _AvailableResources.push_back(std::move(BufferInfo));
    TryPreallocateBuffers(CreateInfo.Size, CreateInfo.AllocationCreateInfo != nullptr);
}

bool FStagingBufferPool::HandleResourceEmergency(FResourceInfo& LowUsageResource, const FStagingBufferCreateInfo& CreateInfo)
{
    auto& BufferInfo = static_cast<FStagingBufferInfo&>(LowUsageResource);
    bool bRequestVma = (CreateInfo.AllocationCreateInfo != nullptr);
    if (BufferInfo.bAllocatedByVma == bRequestVma && BufferInfo.Size < CreateInfo.Size)
    {
        vk::DeviceSize NewSize = std::max(AlignSize(CreateInfo.Size), BufferInfo.Size * 2);
        if (NewSize > kSizeTiers.back())
        {
            NewSize = BufferInfo.Size * 1.5;
        }

        FStagingBufferCreateInfo NewCreateInfo{ NewSize, CreateInfo.AllocationCreateInfo };
        CreateResource(NewCreateInfo);
        return true;
    }

    return false;
}

vk::DeviceSize FStagingBufferPool::AlignSize(vk::DeviceSize RequestedSize)
{
    auto it = std::lower_bound(kSizeTiers.begin(), kSizeTiers.end(), RequestedSize);
    if (it != kSizeTiers.end())
    {
        return *it;
    }

    constexpr vk::DeviceSize kAlighment = 2ull * 1024 * 1024;
    return (RequestedSize + kAlighment - 1) & ~(kAlighment - 1);
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
