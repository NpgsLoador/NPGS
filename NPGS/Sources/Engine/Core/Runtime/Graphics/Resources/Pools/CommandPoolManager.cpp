#include "CommandPoolManager.h"

namespace Npgs
{
    FCommandPoolManager::FCommandPoolManager(std::uint32_t MinPoolLimit, std::uint32_t MaxPoolLimit, std::uint32_t PoolReclaimThresholdMs,
                                             std::uint32_t MaintenanceIntervalMs, vk::Device Device, std::uint32_t QueueFamilyIndex)
        : Base(MinPoolLimit, MaxPoolLimit, PoolReclaimThresholdMs, MaintenanceIntervalMs)
        , _Device(Device)
        , _QueueFamilyIndex(QueueFamilyIndex)
    {
    }
} // namespace Npgs/
