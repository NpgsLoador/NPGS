#include "CommandPoolManager.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

FCommandPoolManager::FPoolGuard::FPoolGuard(FCommandPoolManager* Manager, FVulkanCommandPool* Pool, std::size_t UsageCount)
    : _Manager(Manager), _Pool(Pool), _UsageCount(UsageCount)
{
}

FCommandPoolManager::FPoolGuard::FPoolGuard(FPoolGuard&& Other) noexcept
    :
    _Manager(std::exchange(Other._Manager, nullptr)),
    _Pool(std::exchange(Other._Pool, nullptr)),
    _UsageCount(std::exchange(Other._UsageCount, 0))
{
}

FCommandPoolManager::FPoolGuard::~FPoolGuard()
{
    if (_Manager != nullptr && _Pool != nullptr)
    {
        _Manager->ReleasePool(_Pool, _UsageCount);
        --_Manager->_BusyPoolCount;

        std::uint32_t BusyPoolCount  = _Manager->_BusyPoolCount.load();
        std::uint32_t PeakPoolDemand = _Manager->_PeakPoolDemand.load();
        while (BusyPoolCount > PeakPoolDemand && !_Manager->_PeakPoolDemand.compare_exchange_weak(PeakPoolDemand, BusyPoolCount));
    }
}

FCommandPoolManager::FPoolGuard& FCommandPoolManager::FPoolGuard::operator=(FPoolGuard&& Other) noexcept
{
    if (this != &Other)
    {
        _Manager    = std::exchange(Other._Manager, nullptr);
        _Pool       = std::exchange(Other._Pool, nullptr);
        _UsageCount = std::exchange(Other._UsageCount, 0);
    }

    return *this;
}

FCommandPoolManager::FCommandPoolManager(vk::Device Device, std::uint32_t QueueFamilyIndex, std::uint32_t MinPoolLimit,
                                         std::uint32_t MaxPoolLimit, std::uint32_t PoolReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs)
    :
    _Device(Device),
    _QueueFamilyIndex(QueueFamilyIndex),
    _MinPoolLimit(std::min(MinPoolLimit, MaxPoolLimit)),
    _MaxPoolLimit(std::max(MinPoolLimit, MaxPoolLimit)),
    _PoolReclaimThresholdMs(PoolReclaimThresholdMs),
    _MaintenanceIntervalMs(MaintenanceIntervalMs)
{
    while (_AvailablePools.size() < _MinPoolLimit)
    {
        CreatePool();
    }

    std::thread([this]() -> void { Maintenance(); }).detach();
}

FCommandPoolManager::~FCommandPoolManager()
{
    _bStopMaintenance.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(_MaintenanceIntervalMs * 2));
}

FCommandPoolManager::FPoolGuard FCommandPoolManager::AcquirePool()
{
    std::unique_lock Lock(_Mutex);
    if (_AvailablePools.empty())
    {
        if (_BusyPoolCount.load() + _AvailablePools.size() < _MaxPoolLimit)
        {
            CreatePool();
        }
        else
        {
            constexpr std::uint32_t kMaxWaitTimeMs = 200;
            const std::size_t StartWaitTime = GetCurrentTimeMs();

            if (_Condition.wait_for(Lock, std::chrono::milliseconds(kMaxWaitTimeMs), [this]() -> bool { return !_AvailablePools.empty(); }))
            {
                std::sort(_AvailablePools.begin(), _AvailablePools.end(),
                [](const FPoolInfo& Lhs, const FPoolInfo& Rhs) -> bool
                {
                    return Lhs.UsageCount > Rhs.UsageCount;
                });
            }
            else
            {
                throw std::runtime_error("Failed to acquire command pool. Reset the max pool limit or reduce pool requirements.");
            }
        }
    }

    if (_AvailablePools.size() > 4)
    {
        std::sort(_AvailablePools.begin(), _AvailablePools.end(), [](const FPoolInfo& Lhs, const FPoolInfo& Rhs) -> bool
        {
            return Lhs.UsageCount > Rhs.UsageCount;
        });
    }

    FVulkanCommandPool* Pool = _AvailablePools.front().Pool.release();
    std::size_t   UsageCount = _AvailablePools.front().UsageCount;
    _AvailablePools.pop_front();

    return FPoolGuard(this, Pool, UsageCount);
}

void FCommandPoolManager::OptimizePoolCount()
{
    std::size_t CurrentTimeMs = GetCurrentTimeMs();
    std::lock_guard Lock(_Mutex);

    std::uint32_t TargetCount = std::max(_MinPoolLimit, _PeakPoolDemand.load());
    if (_AvailablePools.size() > TargetCount)
    {
        std::erase_if(_AvailablePools, [&](const FPoolInfo& PoolInfo) -> bool
        {
            return CurrentTimeMs - PoolInfo.LastUsedTimestamp > _PoolReclaimThresholdMs;
        });
    }

    if (_AvailablePools.size() > TargetCount)
    {
        std::sort(_AvailablePools.begin(), _AvailablePools.end(), [](const FPoolInfo& Lhs, const FPoolInfo& Rhs) -> bool
        {
            return Lhs.UsageCount > Rhs.UsageCount;
        });

        _AvailablePools.resize(TargetCount);
    }
}

void FCommandPoolManager::CreatePool()
{
    FPoolInfo PoolInfo
    {
        .Pool = std::make_unique<FVulkanCommandPool>(
            _Device, _QueueFamilyIndex, vk::CommandPoolCreateFlagBits::eResetCommandBuffer),
        .LastUsedTimestamp = GetCurrentTimeMs(),
        .UsageCount        = 0
    };

    _AvailablePools.push_back(std::move(PoolInfo));
}

void FCommandPoolManager::ReleasePool(FVulkanCommandPool* Pool, std::size_t UsageCount)
{
    std::lock_guard Lock(_Mutex);
    FPoolInfo PoolInfo
    {
        .Pool              = std::make_unique<FVulkanCommandPool>(std::move(*Pool)),
        .LastUsedTimestamp = GetCurrentTimeMs(),
        .UsageCount        = UsageCount + 1
    };

    _Condition.notify_one();
    _AvailablePools.push_back(std::move(PoolInfo));
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
