#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "Engine/Core/Base/Assert.h"
#include "Engine/Core/Base/Base.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

template <typename ResourceType, typename ResourceCreateInfoType>
class TResourcePool
{
public:
    class FResourceGuard
    {
    public:
        FResourceGuard(TResourcePool* Pool, ResourceType* Resource, std::size_t UsageCount)
            : _Pool(Pool), _Resource(Resource), _UsageCount(UsageCount)
        {
        }

        FResourceGuard(const FResourceGuard&) = delete;
        FResourceGuard(FResourceGuard&& Other) noexcept
            :
            _Pool(std::exchange(Other._Pool, nullptr)),
            _Resource(std::exchange(Other._Resource, nullptr)),
            _UsageCount(std::exchange(Other._UsageCount, 0))
        {
        }

        ~FResourceGuard()
        {
            if (_Pool != nullptr && _Resource != nullptr)
            {
                _Pool->ReleaseResource(_Resource, _UsageCount);
                --_Pool->_BusyResourceCount;

                std::uint32_t BusyResourceCount  = _Pool->_BusyResourceCount.load();
                std::uint32_t PeakResourceDemand = _Pool->_PeakResourceDemand.load();
                while (BusyResourceCount > PeakResourceDemand &&
                       !_Pool->_PeakResourceDemand.compare_exchange_weak(PeakResourceDemand, BusyResourceCount));
            }
        }

        FResourceGuard& operator=(const FResourceGuard&) = delete;
        FResourceGuard& operator=(FResourceGuard&& Other) noexcept
        {
            if (this != &Other)
            {
                _Pool       = std::exchange(Other._Pool, nullptr);
                _Resource   = std::exchange(Other._Resource, nullptr);
                _UsageCount = std::exchange(Other._UsageCount, 0);
            }

            return *this;
        }

        ResourceType* operator*()
        {
            return _Resource;
        }

        const ResourceType* operator*() const
        {
            return _Resource;
        }

    private:
        TResourcePool* _Pool;
        ResourceType*  _Resource;
        std::size_t    _UsageCount;
    };

    struct FStatisticsInfo
    {
        std::size_t   AvailableResourceCount;
        std::uint32_t BusyResourceCount;
        std::uint32_t PeakResourceDemand;
    };

protected:
    struct FResourceInfo
    {
        std::unique_ptr<ResourceType> Resource;
        std::size_t LastUsedTimestamp;
        std::size_t UsageCount;
    };

public:
    TResourcePool(std::uint32_t MinPoolLimit, std::uint32_t MaxPoolLimit,
                  std::uint32_t PoolReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs)
        :
        _MinResourceLimit(MinPoolLimit),
        _MaxResourceLimit(MaxPoolLimit),
        _ResourceReclaimThresholdMs(PoolReclaimThresholdMs),
        _MaintenanceIntervalMs(MaintenanceIntervalMs)
    {
        std::thread([this]() -> void { Maintenance(); }).detach();
    }

    TResourcePool(const TResourcePool&) = delete;
    TResourcePool(TResourcePool&&)      = delete;

    virtual ~TResourcePool()
    {
        _MaintenanceIntervalMs = std::clamp(_MaintenanceIntervalMs, 0u, 500u);
        _bStopMaintenance.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(_MaintenanceIntervalMs * 2));
    }

    TResourcePool& operator=(const TResourcePool&) = delete;
    TResourcePool& operator=(TResourcePool&&)      = delete;

    template <typename Func>
    requires std::predicate<Func, const FResourceInfo&>
    FResourceGuard AcquireResource(const ResourceCreateInfoType& CreateInfo, Func&& Pred)
    {
        std::unique_lock Lock(_Mutex);

        std::vector<typename std::deque<FResourceInfo>::iterator> MatchedResources;
        for (auto it = _AvailableResources.begin(); it != _AvailableResources.end(); ++it)
        {
            if (Pred(*it))
            {
                MatchedResources.push_back(it);
            }
        }

        if (!MatchedResources.empty())
        {
            if (MatchedResources.size() > 1)
            {
                std::sort(MatchedResources.begin(), MatchedResources.end(),
                [](const auto& Lhs, const auto& Rhs) -> bool
                {
                    return Lhs->UsageCount       != Rhs->UsageCount ?
                           Lhs->UsageCount        > Rhs->UsageCount :
                           Lhs->LastUsedTimestamp > Rhs->LastUsedTimestamp;
                });
            }

            auto BestIt = MatchedResources.front();
            ResourceType* Resource = BestIt->Resource.release();
            std::size_t UsageCount = BestIt->UsageCount + 1;
            _AvailableResources.pop_front();
            ++_BusyResourceCount;

            return FResourceGuard(this, Resource, UsageCount);
        }

        if (_BusyResourceCount.load() + _AvailableResources.size() < _MaxResourceLimit)
        {
            CreateResource(CreateInfo);

            ResourceType* Resource = _AvailableResources.back().Resource.release();
            _AvailableResources.pop_back();
            ++_BusyResourceCount;
            return FResourceGuard(this, Resource, 1);
        }

        constexpr std::uint32_t kMaxWaitTimeMs = 2000;
        if (_Condition.wait_for(Lock, std::chrono::milliseconds(kMaxWaitTimeMs),
        [this, &Pred]() -> bool { return std::any_of(_AvailableResources.begin(), _AvailableResources.end(), Pred); }))
        {
            Lock.unlock();
            return AcquireResource(CreateInfo, Pred);
        }

        if (!_AvailableResources.empty())
        {
            for (auto it = _AvailableResources.begin(); it != _AvailableResources.end(); ++it)
            {
                if (HandleResourceEmergency(*it, CreateInfo))
                {
                    ResourceType* Resource = it->Resource.release();
                    std::size_t UsageCount = it->UsageCount + 1;
                    _AvailableResources.erase(it);
                    ++_BusyResourceCount;
                    return FResourceGuard(this, Resource, UsageCount);
                }
            }
        }

        throw std::runtime_error("Failed to acquire resource. Reset the max resource limit or reduce resource requirements.");
    }

    void OptimizeResourceCount()
    {
        std::size_t CurrentTimeMs = GetCurrentTimeMs();
        std::lock_guard Lock(_Mutex);

        std::uint32_t TargetCount = std::max(_MinResourceLimit, _PeakResourceDemand.load());
        if (_AvailableResources.size() > TargetCount)
        {
            std::erase_if(_AvailableResources, [&](const FResourceInfo& ResourceInfo) -> bool
            {
                return CurrentTimeMs - ResourceInfo.LastUsedTimestamp > _ResourceReclaimThresholdMs;
            });
        }

        if (_AvailableResources.size() > TargetCount)
        {
            std::sort(_AvailableResources.begin(), _AvailableResources.end(),
            [](const FResourceInfo& Lhs, const FResourceInfo& Rhs) -> bool
            {
                return Lhs.UsageCount > Rhs.UsageCount;
            });

            _AvailableResources.resize(TargetCount);
        }
    }

    void FreeSpace()
    {
        std::lock_guard Lock(_Mutex);
        _AvailableResources.clear();
    }

    void SetMinResourceLimit(std::uint32_t MinResourceLimit)
    {
        _MinResourceLimit = std::min(MinResourceLimit, _MaxResourceLimit);
    }

    void SetMaxResourceLimit(std::uint32_t MaxResourceLimit)
    {
        _MaxResourceLimit = std::max(MaxResourceLimit, _MinResourceLimit);
    }

    void SetResourceReclaimThreshold(std::uint32_t ResourceReclaimThresholdMs)
    {
        _ResourceReclaimThresholdMs = ResourceReclaimThresholdMs;
    }

    void SetMaintenanceInterval(std::uint32_t MaintenanceIntervalMs)
    {
        NpgsAssert(MaintenanceIntervalMs > 0 && MaintenanceIntervalMs < std::numeric_limits<std::uint32_t>::max() / 2,
                   "Maintenance interval must be greater than 0 and less than UINT32_MAX / 2.");

        _MaintenanceIntervalMs = MaintenanceIntervalMs;
    }

    std::uint32_t GetMinResourceLimit() const
    {
        return _MinResourceLimit;
    }

    std::uint32_t GetMaxResourceLimit() const
    {
        return _MaxResourceLimit;
    }

    std::uint32_t GetResourceReclaimThreshold() const
    {
        return _ResourceReclaimThresholdMs;
    }

    std::uint32_t GetMaintenanceInterval() const
    {
        return _MaintenanceIntervalMs;
    }

    FStatisticsInfo GetStatisticsInfo() const
    {
        return FStatisticsInfo
        {
            .AvailableResourceCount = _AvailableResources.size(),
            .BusyResourceCount      = _BusyResourceCount.load(),
            .PeakResourceDemand     = _PeakResourceDemand.load()
        };
    }

protected:
    virtual void CreateResource(const ResourceCreateInfoType& CreateInfo) = 0;
    virtual bool HandleResourceEmergency(FResourceInfo& LowUsageResource, const ResourceCreateInfoType& CreateInfo) = 0;

    virtual void OnReleaseResource(FResourceInfo& ResourceInfo)
    {
    }

    virtual void ReleaseResource(ResourceType* Resource, std::size_t UsageCount)
    {
        std::lock_guard Lock(_Mutex);

        FResourceInfo ResourceInfo
        {
            .Resource          = std::make_unique<ResourceType>(std::move(*Resource)),
            .LastUsedTimestamp = GetCurrentTimeMs(),
            .UsageCount        = UsageCount
        };

        OnReleaseResource(ResourceInfo);

        _AvailableResources.push_back(std::move(ResourceInfo));
        _Condition.notify_one();
    }

    std::size_t GetCurrentTimeMs() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

private:
    void Maintenance()
    {
        while (!_bStopMaintenance.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(_MaintenanceIntervalMs));
            if (!_bStopMaintenance.load())
            {
                OptimizeResourceCount();
            }
        }
    }

protected:
    std::mutex                 _Mutex;
    std::condition_variable    _Condition;
    std::deque<FResourceInfo>  _AvailableResources;
    std::atomic<std::uint32_t> _BusyResourceCount{};
    std::atomic<std::uint32_t> _PeakResourceDemand{};

    std::uint32_t              _MinResourceLimit;
    std::uint32_t              _MaxResourceLimit;
    std::uint32_t              _ResourceReclaimThresholdMs;
    std::uint32_t              _MaintenanceIntervalMs;

    std::atomic<bool>          _bStopMaintenance{ false };
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
