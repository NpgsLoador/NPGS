#include "CommandPoolManager.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <thread>

#include "Engine/Core/Base/Assert.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

NPGS_INLINE FVulkanCommandPool* FCommandPoolManager::FPoolGuard::operator*()
{
    return _Pool;
}

NPGS_INLINE const FVulkanCommandPool* FCommandPoolManager::FPoolGuard::operator*() const
{
    return _Pool;
}

NPGS_INLINE void FCommandPoolManager::SetMinPoolLimit(std::uint32_t MinPoolLimit)
{
    _MinPoolLimit = std::min(MinPoolLimit, _MaxPoolLimit);
}

NPGS_INLINE void FCommandPoolManager::SetMaxPoolLimit(std::uint32_t MaxPoolLimit)
{
    _MaxPoolLimit = std::max(MaxPoolLimit, _MinPoolLimit);
}

NPGS_INLINE void FCommandPoolManager::SetPoolReclaimThreshold(std::uint32_t PoolReclaimThresholdMs)
{
    _PoolReclaimThresholdMs = PoolReclaimThresholdMs;
}

NPGS_INLINE void FCommandPoolManager::SetMaintenanceInterval(std::uint32_t MaintenanceIntervalMs)
{
    NpgsAssert(MaintenanceIntervalMs < std::numeric_limits<std::uint32_t>::max() / 2,
               "Maintenance interval cannot be set to infinity.");
    _MaintenanceIntervalMs = MaintenanceIntervalMs;
}

NPGS_INLINE std::uint32_t FCommandPoolManager::GetMinPoolLimit() const
{
    return _MinPoolLimit;
}

NPGS_INLINE std::uint32_t FCommandPoolManager::GetMaxPoolLimit() const
{
    return _MaxPoolLimit;
}

NPGS_INLINE std::uint32_t FCommandPoolManager::GetPoolReclaimThreshold() const
{
    return _PoolReclaimThresholdMs;
}

NPGS_INLINE std::uint32_t FCommandPoolManager::GetMaintenanceInterval() const
{
    return _MaintenanceIntervalMs;
}

NPGS_INLINE void FCommandPoolManager::Maintenance()
{
    while (!_bStopMaintenance.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(_MaintenanceIntervalMs));
        OptimizePoolCount();
    }
}

NPGS_INLINE std::size_t FCommandPoolManager::GetCurrentTimeMs() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
