#include "StagingBufferPool.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>

namespace Npgs
{
    FStagingBufferPool::FStagingBufferPool(FVulkanContext* VulkanContext, VmaAllocator Allocator,
                                           std::uint32_t MinAvailableBufferLimit, std::uint32_t MaxAllocatedBufferLimit,
                                           std::uint32_t BufferReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs,
                                           EPoolUsage PoolUsage, bool bUsingVma)
        : Base(MinAvailableBufferLimit, MaxAllocatedBufferLimit, BufferReclaimThresholdMs, MaintenanceIntervalMs)
        , _VulkanContext(VulkanContext)
        , _Allocator(Allocator)
        , _bUsingVma(bUsingVma)
    {
        _AllocationCreateInfo.usage = PoolUsage == EPoolUsage::kSubmit
                                    ? VMA_MEMORY_USAGE_CPU_TO_GPU
                                    : VMA_MEMORY_USAGE_GPU_TO_CPU;

        std::array<vk::DeviceSize, 3> InitialSizes{ kSizeTiers[0], kSizeTiers[1], kSizeTiers[2] };
        std::size_t InitialCount = std::min(static_cast<std::size_t>(MinAvailableBufferLimit), InitialSizes.size());
        for (std::size_t i = 0; i != InitialCount; ++i)
        {
            FStagingBufferCreateInfo CreateInfo
            {
                .Size = InitialSizes[i % InitialSizes.size()],
                .AllocationCreateInfo = _bUsingVma ? &_AllocationCreateInfo : nullptr
            };

            CreateResource(CreateInfo);
        }
    }

    FStagingBufferPool::FBufferGuard
    FStagingBufferPool::AcquireBuffer(vk::DeviceSize RequestedSize)
    {
        vk::DeviceSize AlignedSize = AlignSize(RequestedSize);

        FStagingBufferCreateInfo CreateInfo{ AlignedSize, &_AllocationCreateInfo };
        return AcquireResource(CreateInfo, [RequestedSize, AlignedSize](const std::unique_ptr<FStagingBufferInfo>& Info) -> bool
        {
            return Info->Size >= RequestedSize && (Info->Size <= AlignedSize * 2 || Info->Size <= RequestedSize + 1ull * 1024 * 1024);
        });
    }

    void FStagingBufferPool::CreateResource(const FStagingBufferCreateInfo& CreateInfo)
    {
        auto BufferInfoPtr = std::make_unique<FStagingBufferInfo>();

        vk::DeviceSize AlignedSize = AlignSize(CreateInfo.Size);
        if (CreateInfo.AllocationCreateInfo != nullptr)
        {
            vk::BufferCreateInfo BufferCreateInfo({}, AlignedSize, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);
            BufferInfoPtr->Resource = std::make_unique<FStagingBuffer>(_VulkanContext, _Allocator, &_AllocationCreateInfo, BufferCreateInfo);
            BufferInfoPtr->bAllocatedByVma = true;
        }
        else
        {
            BufferInfoPtr->Resource = std::make_unique<FStagingBuffer>(_VulkanContext, AlignedSize);
            BufferInfoPtr->bAllocatedByVma = false;
        }

        BufferInfoPtr->Resource->GetMemory().SetPersistentMapping(true);
        BufferInfoPtr->Size              = AlignedSize;
        BufferInfoPtr->LastUsedTimestamp = GetCurrentTimeMs();
        BufferInfoPtr->UsageCount        = 1;

        _AvailableResources.push_back(std::move(BufferInfoPtr));
    }

    bool FStagingBufferPool::HandleResourceEmergency(FStagingBufferInfo& LowUsageResource, const FStagingBufferCreateInfo& CreateInfo)
    {
        auto& BufferInfo = static_cast<FStagingBufferInfo&>(LowUsageResource);
        bool bRequestVma = (CreateInfo.AllocationCreateInfo != nullptr);
        if (BufferInfo.bAllocatedByVma == bRequestVma && BufferInfo.Size < CreateInfo.Size)
        {
            vk::DeviceSize NewSize = std::max(AlignSize(CreateInfo.Size), BufferInfo.Size * 2);
            if (NewSize > kSizeTiers.back())
            {
                NewSize = static_cast<vk::DeviceSize>(BufferInfo.Size * 1.5);
            }

            FStagingBufferCreateInfo NewCreateInfo{ NewSize, CreateInfo.AllocationCreateInfo };
            CreateResource(NewCreateInfo);
            return true;
        }

        return false;
    }

    void FStagingBufferPool::ReleaseResource(FStagingBuffer* Buffer, std::size_t UsageCount)
    {
        std::lock_guard Lock(_Mutex);

        Buffer->GetMemory().SetPersistentMapping(true);
        auto BufferInfoPtr = std::make_unique<FStagingBufferInfo>();
        BufferInfoPtr->LastUsedTimestamp = GetCurrentTimeMs();
        BufferInfoPtr->UsageCount        = UsageCount;
        BufferInfoPtr->Size              = Buffer->GetMemory().GetAllocationSize();
        BufferInfoPtr->bAllocatedByVma   = Buffer->AllocatedByVma();
        BufferInfoPtr->Resource          = std::make_unique<FStagingBuffer>(std::move(*Buffer));

        _AvailableResources.push_back(std::move(BufferInfoPtr));
        _Condition.notify_one();
    }

    void FStagingBufferPool::OptimizeResourceCount()
    {
        std::size_t CurrentTimeMs = GetCurrentTimeMs();
        std::uint32_t TargetCount = std::max(_MinAvailableResourceLimit, _PeakResourceDemand.load());
        std::lock_guard Lock(_Mutex);

        constexpr vk::DeviceSize kCompactSizeThreshold = 32ull * 1024 * 1024;
        RemoveOversizedBuffers(kCompactSizeThreshold);

        if (_AvailableResources.size() < _MinAvailableResourceLimit &&
            _AvailableResources.size() + _BusyResourceCount.load() < _MaxAllocatedResourceLimit)
        {
            std::size_t MinExtraCount = _MinAvailableResourceLimit - _AvailableResources.size();
            std::size_t MaxExtraCount = _MaxAllocatedResourceLimit - _AvailableResources.size() - _BusyResourceCount.load();
            std::size_t ExtraCount    = std::min(MinExtraCount, MaxExtraCount);
            for (std::size_t i = 0; i != ExtraCount; ++i)
            {
                FStagingBufferCreateInfo CreateInfo
                {
                    .Size = kSizeTiers[3],
                    .AllocationCreateInfo = _bUsingVma ? &_AllocationCreateInfo : nullptr
                };

                CreateResource(CreateInfo);
            }

            return;
        }
        else if (_AvailableResources.size() == _MinAvailableResourceLimit)
        {
            return;
        }

        std::unordered_map<vk::DeviceSize, std::vector<std::size_t>> CategoryBufferIndices;
        std::unordered_map<vk::DeviceSize, std::size_t> CategoryUsages;

        for (std::size_t i = 0; i != _AvailableResources.size(); ++i)
        {
            const auto& BufferInfo = *_AvailableResources[i];
            CategoryBufferIndices[BufferInfo.Size].push_back(i);
            CategoryUsages[BufferInfo.Size] += BufferInfo.UsageCount;
        }

        std::vector<std::size_t> BufferNeedRemove;
        for (auto& [Size, Indices] : CategoryBufferIndices)
        {
            if (Indices.size() <= 1)
            {
                continue;
            }

            std::sort(Indices.begin(), Indices.end(), [&](std::size_t Lhs, std::size_t Rhs) -> bool
            {
                const auto& BufferA = *_AvailableResources[Lhs];
                const auto& BufferB = *_AvailableResources[Rhs];

                bool bExpiredA = (CurrentTimeMs - BufferA.LastUsedTimestamp > _ResourceReclaimThresholdMs);
                bool bExpiredB = (CurrentTimeMs - BufferB.LastUsedTimestamp > _ResourceReclaimThresholdMs);
                if (bExpiredA != bExpiredB)
                {
                    return bExpiredA;
                }

                return BufferA.UsageCount < BufferB.UsageCount;
            });

            for (std::size_t i = 0; i != Indices.size(); ++i)
            {
                const auto& BufferInfo = *_AvailableResources[Indices[i]];
                if (CurrentTimeMs - BufferInfo.LastUsedTimestamp > _ResourceReclaimThresholdMs && BufferInfo.UsageCount < 5)
                {
                    BufferNeedRemove.push_back(Indices[i]);
                }
            }
        }

        std::sort(BufferNeedRemove.begin(), BufferNeedRemove.end(), std::greater<std::size_t>{});
        for (std::size_t Index : BufferNeedRemove)
        {
            if (_AvailableResources.size() < TargetCount)
            {
                break;
            }

            _AvailableResources.erase(_AvailableResources.begin() + Index);
        }

        if (_AvailableResources.size() > TargetCount)
        {
            std::unordered_map<vk::DeviceSize, std::size_t> RemainingCounts;
            for (const auto& Resource : _AvailableResources)
            {
                const auto& BufferInfo = *Resource;
                ++RemainingCounts[BufferInfo.Size];
            }

            std::size_t TotalRemoveCount = _AvailableResources.size() - TargetCount;
            std::unordered_map<vk::DeviceSize, std::size_t> NeedRemoveCounts;
            for (auto [Size, Count] : RemainingCounts)
            {
                std::size_t MinKeep   = std::min<std::size_t>(1, Count);
                std::size_t MaxRemove = std::max<std::size_t>(0, Count - MinKeep);

                float       RemoveRatio = static_cast<float>(TotalRemoveCount) / _AvailableResources.size();
                std::size_t RemoveCount = std::min(MaxRemove, static_cast<std::size_t>(Count * RemoveRatio + 0.5));

                NeedRemoveCounts[Size] = RemoveCount;
            }

            std::vector<std::pair<vk::DeviceSize, std::size_t>> IndexedBuffers;
            for (std::size_t i = 0; i != _AvailableResources.size(); ++i)
            {
                const auto& BufferInfo = *_AvailableResources[i];
                IndexedBuffers.emplace_back(BufferInfo.Size, i);
            }

            std::sort(IndexedBuffers.begin(), IndexedBuffers.end(), [&](const auto& Lhs, const auto& Rhs) -> bool
            {
                if (Lhs.first != Rhs.first)
                {
                    return false;
                }

                const auto& BufferA = *_AvailableResources[Lhs.second];
                const auto& BufferB = *_AvailableResources[Rhs.second];

                if (BufferA.LastUsedTimestamp != BufferB.LastUsedTimestamp)
                {
                    return BufferA.LastUsedTimestamp < BufferB.LastUsedTimestamp;
                }

                return BufferA.UsageCount < BufferB.UsageCount;
            });

            std::vector<std::size_t> NeedRemoveIndices;
            std::unordered_map<vk::DeviceSize, std::size_t> RemovedCounts;
            for (auto [Size, Index] : IndexedBuffers)
            {
                if (RemovedCounts[Size] < NeedRemoveCounts[Size])
                {
                    NeedRemoveIndices.push_back(Index);
                    ++RemovedCounts[Size];

                    if (NeedRemoveIndices.size() >= TotalRemoveCount)
                    {
                        break;
                    }
                }
            }

            std::sort(NeedRemoveIndices.begin(), NeedRemoveIndices.end(), std::greater<std::size_t>{});
            for (std::size_t Index : NeedRemoveIndices)
            {
                if (_AvailableResources.size() < TargetCount)
                {
                    break;
                }

                _AvailableResources.erase(_AvailableResources.begin() + Index);
            }
        }
    }

    void FStagingBufferPool::RemoveOversizedBuffers(vk::DeviceSize Threshold)
    {
        std::unordered_map<vk::DeviceSize, std::vector<std::size_t>> CategoryBufferIndices;
        std::unordered_map<vk::DeviceSize, std::size_t> CategoryUsages;

        for (std::size_t i = 0; i != _AvailableResources.size(); ++i)
        {
            const auto& BufferInfo = *_AvailableResources[i];
            CategoryBufferIndices[BufferInfo.Size].push_back(i);
            CategoryUsages[BufferInfo.Size] += BufferInfo.UsageCount;
        }

        std::vector<std::size_t> NeedRemoveIndices;
        constexpr vk::DeviceSize kLargeBufferThreshold = 256ull * 1024 * 1024;
        for (auto [Size, Usage] : CategoryUsages)
        {
            if (Size > Threshold || Usage < 5 || Size > kLargeBufferThreshold)
            {
                NeedRemoveIndices.append_range(CategoryBufferIndices[Size]);
            }
        }

        std::sort(NeedRemoveIndices.begin(), NeedRemoveIndices.end(), std::greater<std::size_t>{});
        for (std::size_t Index : NeedRemoveIndices)
        {
            _AvailableResources.erase(_AvailableResources.begin() + Index);
        }
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
} // namespace Npgs
