// This manager is a "pool" instead of a "manager"
// Named manager because the "vk::CommandPool" has "pool" item.

#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>

#include "Engine/Core/Runtime/Graphics/Vulkan/Core.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

class FCommandPoolManager
{
public:
    class FPoolGuard
    {
    public:
        FPoolGuard(FCommandPoolManager* Manager, FVulkanCommandPool* Pool, std::size_t UsageCount);
        FPoolGuard(const FPoolGuard&) = delete;
        FPoolGuard(FPoolGuard&& Other) noexcept;
        ~FPoolGuard();

        FPoolGuard& operator=(const FPoolGuard&) = delete;
        FPoolGuard& operator=(FPoolGuard&& Other) noexcept;

        FVulkanCommandPool* operator*();
        const FVulkanCommandPool* operator*() const;

    private:
        FCommandPoolManager* _Manager;
        FVulkanCommandPool*  _Pool;
        std::size_t          _UsageCount;
    };

private:
    struct FPoolInfo
    {
        std::unique_ptr<FVulkanCommandPool> Pool;
        std::size_t LastUsedTimestamp;
        std::size_t UsageCount;
    };

public:
    FCommandPoolManager(vk::Device Device, std::uint32_t QueueFamilyIndex, std::uint32_t MinPoolLimit,
                        std::uint32_t MaxPoolLimit, std::uint32_t PoolReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs);

    FCommandPoolManager(const FCommandPoolManager&) = delete;
    FCommandPoolManager(FCommandPoolManager&&)      = delete;
    ~FCommandPoolManager();

    FCommandPoolManager& operator=(const FCommandPoolManager&) = delete;
    FCommandPoolManager& operator=(FCommandPoolManager&&)      = delete;

    FPoolGuard AcquirePool();
    void OptimizePoolCount();

    void SetMinPoolLimit(std::uint32_t MinPoolLimit);
    void SetMaxPoolLimit(std::uint32_t MaxPoolLimit);
    void SetPoolReclaimThreshold(std::uint32_t PoolReclaimThresholdMs);
    void SetMaintenanceInterval(std::uint32_t MaintenanceIntervalMs);

    std::uint32_t GetMinPoolLimit() const;
    std::uint32_t GetMaxPoolLimit() const;
    std::uint32_t GetPoolReclaimThreshold() const;
    std::uint32_t GetMaintenanceInterval() const;

private:
    void CreatePool();
    void ReleasePool(FVulkanCommandPool* Pool, std::size_t UsageCount);
    void Maintenance();
    std::size_t GetCurrentTimeMs() const;

private:
    std::mutex                 _Mutex;
    std::condition_variable    _Condition;
    std::deque<FPoolInfo>      _AvailablePools;
    std::atomic<std::uint32_t> _BusyPoolCount{};
    std::atomic<std::uint32_t> _PeakPoolDemand{};

    vk::Device                 _Device{ FVulkanCore::GetClassInstance()->GetDevice() };
    std::uint32_t              _QueueFamilyIndex;
    std::uint32_t              _MinPoolLimit;
    std::uint32_t              _MaxPoolLimit;
    std::uint32_t              _PoolReclaimThresholdMs;
    std::uint32_t              _MaintenanceIntervalMs;

    std::atomic<bool>          _bStopMaintenance{ false };
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "CommandPoolManager.inl"
