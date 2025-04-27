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

namespace Npgs
{
    template <typename ResourceType>
    struct TResourceInfo
    {
        std::unique_ptr<ResourceType> Resource;
        std::size_t LastUsedTimestamp{};
        std::size_t UsageCount{};

        virtual ~TResourceInfo() = default;
    };

    template <typename ResourceType, typename ResourceCreateInfoType, typename ResourceInfoType = TResourceInfo<ResourceType>>
    class TResourcePool
    {
    public:
        class FResourceGuard
        {
        public:
            FResourceGuard(TResourcePool* Pool, ResourceType* Resource, std::size_t UsageCount)
                : Pool_(Pool), Resource_(Resource), UsageCount_(UsageCount)
            {
            }

            FResourceGuard(const FResourceGuard&) = delete;
            FResourceGuard(FResourceGuard&& Other) noexcept
                : Pool_(std::exchange(Other.Pool_, nullptr))
                , Resource_(std::exchange(Other.Resource_, nullptr))
                , UsageCount_(std::exchange(Other.UsageCount_, 0))
            {
            }

            ~FResourceGuard()
            {
                if (Pool_ != nullptr && Resource_ != nullptr)
                {
                    Pool_->ReleaseResource(Resource_, UsageCount_);
                    --Pool_->BusyResourceCount_;

                    std::uint32_t BusyResourceCount  = Pool_->BusyResourceCount_.load();
                    std::uint32_t PeakResourceDemand = Pool_->PeakResourceDemand_.load();
                    while (BusyResourceCount > PeakResourceDemand &&
                           !Pool_->PeakResourceDemand_.compare_exchange_weak(PeakResourceDemand, BusyResourceCount));
                }
            }

            FResourceGuard& operator=(const FResourceGuard&) = delete;
            FResourceGuard& operator=(FResourceGuard&& Other) noexcept
            {
                if (this != &Other)
                {
                    Pool_       = std::exchange(Other.Pool_, nullptr);
                    Resource_   = std::exchange(Other.Resource_, nullptr);
                    UsageCount_ = std::exchange(Other.UsageCount_, 0);
                }

                return *this;
            }

            ResourceType* operator->()
            {
                return Resource_;
            }

            const ResourceType* operator->() const
            {
                return Resource_;
            }

            ResourceType& operator*()
            {
                return *Resource_;
            }

            const ResourceType& operator*() const
            {
                return *Resource_;
            }

        private:
            TResourcePool* Pool_;
            ResourceType*  Resource_;
            std::size_t    UsageCount_;
        };

        struct FStatisticsInfo
        {
            std::size_t   AvailableResourceCount;
            std::uint32_t BusyResourceCount;
            std::uint32_t PeakResourceDemand;
        };

    public:
        TResourcePool(std::uint32_t MinAvailablePoolLimit, std::uint32_t MaxAllocatedPoolLimit,
                      std::uint32_t PoolReclaimThresholdMs, std::uint32_t MaintenanceIntervalMs)
            : MinAvailableResourceLimit_(MinAvailablePoolLimit)
            , MaxAllocatedResourceLimit_(MaxAllocatedPoolLimit)
            , ResourceReclaimThresholdMs_(PoolReclaimThresholdMs)
            , MaintenanceIntervalMs_(MaintenanceIntervalMs)
        {
            std::thread([this]() -> void { Maintenance(); }).detach();
        }

        TResourcePool(const TResourcePool&) = delete;
        TResourcePool(TResourcePool&&)      = delete;

        virtual ~TResourcePool()
        {
            MaintenanceIntervalMs_ = std::clamp(MaintenanceIntervalMs_, 0u, 500u);
            bStopMaintenance_.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        TResourcePool& operator=(const TResourcePool&) = delete;
        TResourcePool& operator=(TResourcePool&&)      = delete;

        template <typename Func>
        requires std::predicate<Func, const std::unique_ptr<ResourceInfoType>&>
        FResourceGuard AcquireResource(const ResourceCreateInfoType& CreateInfo, Func&& Pred)
        {
            std::unique_lock Lock(Mutex_);

            std::vector<typename std::deque<std::unique_ptr<ResourceInfoType>>::iterator> MatchedResources;
            for (auto it = AvailableResources_.begin(); it != AvailableResources_.end(); ++it)
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
                        return (*Lhs)->UsageCount       != (*Rhs)->UsageCount ?
                               (*Lhs)->UsageCount        > (*Rhs)->UsageCount :
                               (*Lhs)->LastUsedTimestamp > (*Rhs)->LastUsedTimestamp;
                    });
                }

                auto BestIt = MatchedResources.front();
                ResourceType* Resource = (*BestIt)->Resource.release();
                std::size_t UsageCount = (*BestIt)->UsageCount + 1;
                AvailableResources_.erase(BestIt);
                ++BusyResourceCount_;

                return FResourceGuard(this, Resource, UsageCount);
            }

            if (BusyResourceCount_.load() + AvailableResources_.size() < MaxAllocatedResourceLimit_)
            {
                CreateResource(CreateInfo);

                ResourceType* Resource = AvailableResources_.back()->Resource.release();
                AvailableResources_.pop_back();
                ++BusyResourceCount_;
                return FResourceGuard(this, Resource, 1);
            }

            constexpr std::uint32_t kMaxWaitTimeMs = 2000;
            if (Condition_.wait_for(Lock, std::chrono::milliseconds(kMaxWaitTimeMs),
                [this, &Pred]() -> bool { return std::any_of(AvailableResources_.begin(), AvailableResources_.end(), Pred); }))
            {
                Lock.unlock();
                return AcquireResource(CreateInfo, Pred);
            }

            if (!AvailableResources_.empty())
            {
                for (auto it = AvailableResources_.begin(); it != AvailableResources_.end(); ++it)
                {
                    if (HandleResourceEmergency(**it, CreateInfo))
                    {
                        ResourceType* Resource = (*it)->Resource.release();
                        std::size_t UsageCount = (*it)->UsageCount + 1;
                        AvailableResources_.erase(it);
                        ++BusyResourceCount_;
                        return FResourceGuard(this, Resource, UsageCount);
                    }
                }
            }

            throw std::runtime_error("Failed to acquire resource. Reset the max resource limit or reduce resource requirements.");
        }

        void Reset()
        {
            std::lock_guard Lock(Mutex_);
            AvailableResources_.clear();
        }

        void SetMinAvailableResourceLimit(std::uint32_t MinAvailableResourceLimit)
        {
            MinAvailableResourceLimit_ = std::min(MinAvailableResourceLimit, MaxAllocatedResourceLimit_);
        }

        void SetMaxAllocatedResourceLimit(std::uint32_t MaxAllocatedResourceLimit)
        {
            MaxAllocatedResourceLimit_ = std::max(MaxAllocatedResourceLimit, MinAvailableResourceLimit_);
        }

        void SetResourceReclaimThreshold(std::uint32_t ResourceReclaimThresholdMs)
        {
            ResourceReclaimThresholdMs_ = ResourceReclaimThresholdMs;
        }

        void SetMaintenanceInterval(std::uint32_t MaintenanceIntervalMs)
        {
            NpgsAssert(MaintenanceIntervalMs > 0 && MaintenanceIntervalMs < std::numeric_limits<std::uint32_t>::max() / 2,
                       "Maintenance interval must be greater than 0 and less than UINT32_MAX / 2.");

            MaintenanceIntervalMs_ = MaintenanceIntervalMs;
        }

        std::uint32_t GetMinAvailableResourceLimit() const
        {
            return MinAvailableResourceLimit_;
        }

        std::uint32_t GetMaxAllocatedResourceLimit() const
        {
            return MaxAllocatedResourceLimit_;
        }

        std::uint32_t GetResourceReclaimThreshold() const
        {
            return ResourceReclaimThresholdMs_;
        }

        std::uint32_t GetMaintenanceInterval() const
        {
            return MaintenanceIntervalMs_;
        }

        FStatisticsInfo GetStatisticsInfo() const
        {
            return FStatisticsInfo
            {
                .AvailableResourceCount = AvailableResources_.size(),
                .BusyResourceCount      = BusyResourceCount_.load(),
                .PeakResourceDemand     = PeakResourceDemand_.load()
            };
        }

    protected:
        virtual void CreateResource(const ResourceCreateInfoType& CreateInfo) = 0;
        virtual bool HandleResourceEmergency(ResourceInfoType& LowUsageResource, const ResourceCreateInfoType& CreateInfo) = 0;

        virtual void OnReleaseResource(ResourceInfoType& ResourceInfo)
        {
            // Default implementation does nothing.
        }

        virtual void ReleaseResource(ResourceType* Resource, std::size_t UsageCount)
        {
            std::lock_guard Lock(Mutex_);

            auto ResourceInfoPtr = std::make_unique<ResourceInfoType>();
            ResourceInfoPtr->Resource          = std::make_unique<ResourceType>(std::move(*Resource));
            ResourceInfoPtr->LastUsedTimestamp = GetCurrentTimeMs();
            ResourceInfoPtr->UsageCount        = UsageCount;

            OnReleaseResource(*ResourceInfoPtr);

            AvailableResources_.push_back(std::move(ResourceInfoPtr));
            Condition_.notify_one();
        }

        virtual void OptimizeResourceCount()
        {
            std::size_t CurrentTimeMs = GetCurrentTimeMs();
            std::lock_guard Lock(Mutex_);

            std::uint32_t TargetCount = std::max(MinAvailableResourceLimit_, PeakResourceDemand_.load());
            if (AvailableResources_.size() > TargetCount)
            {
                std::erase_if(AvailableResources_, [&](const auto& ResourceInfo) -> bool
                {
                    return CurrentTimeMs - ResourceInfo->LastUsedTimestamp > ResourceReclaimThresholdMs_;
                });
            }

            if (AvailableResources_.size() > TargetCount)
            {
                std::sort(AvailableResources_.begin(), AvailableResources_.end(),
                [](const auto& Lhs, const auto& Rhs) -> bool
                {
                    return Lhs->UsageCount > Rhs->UsageCount;
                });

                AvailableResources_.resize(TargetCount);
            }
        }

        std::size_t GetCurrentTimeMs() const
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }

    private:
        void Maintenance()
        {
            while (!bStopMaintenance_.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(MaintenanceIntervalMs_));
                if (!bStopMaintenance_.load())
                {
                    OptimizeResourceCount();
                }
            }
        }

    protected:
        std::mutex                                    Mutex_;
        std::condition_variable                       Condition_;
        std::deque<std::unique_ptr<ResourceInfoType>> AvailableResources_;
        std::atomic<std::uint32_t>                    BusyResourceCount_{};
        std::atomic<std::uint32_t>                    PeakResourceDemand_{};

        std::uint32_t                                 MinAvailableResourceLimit_;
        std::uint32_t                                 MaxAllocatedResourceLimit_;
        std::uint32_t                                 ResourceReclaimThresholdMs_;
        std::uint32_t                                 MaintenanceIntervalMs_;

        std::atomic<bool>                             bStopMaintenance_{ false };
    };
} // namespace Npgs
