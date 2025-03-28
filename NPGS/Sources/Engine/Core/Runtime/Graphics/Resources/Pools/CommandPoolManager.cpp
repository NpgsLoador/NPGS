#include "CommandPoolManager.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

FCommandPoolManager::FCommandPoolManager(std::uint32_t MinPoolLimit, std::uint32_t MaxPoolLimit, std::uint32_t PoolReclaimThresholdMs,
                                         std::uint32_t MaintenanceIntervalMs, vk::Device Device, std::uint32_t QueueFamilyIndex)
    :
    Base(MinPoolLimit, MaxPoolLimit, PoolReclaimThresholdMs, MaintenanceIntervalMs),
    _Device(Device),
    _QueueFamilyIndex(QueueFamilyIndex)
{
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
