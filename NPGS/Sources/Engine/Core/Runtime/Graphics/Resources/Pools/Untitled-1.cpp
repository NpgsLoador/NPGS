// #pragma once

// #include <cstddef>
// #include <cstdint>
// #include <atomic>
// #include <condition_variable>
// #include <deque>
// #include <memory>
// #include <mutex>
// #include <thread>
// #include <functional>
// #include <algorithm>

// _NPGS_BEGIN
// _RUNTIME_BEGIN
// _GRAPHICS_BEGIN

// /**
//  * 通用资源池基类模板
//  * @tparam TResource 资源类型
//  * @tparam TInfo 创建资源所需信息类型
//  */
// template<typename TResource, typename TInfo>
// class TResourcePoolBase
// {
// public:
//     // 资源守卫类 - RAII模式
//     class FResourceGuard
//     {
//     public:
//         FResourceGuard(TResourcePoolBase* Manager, TResource* Resource, std::size_t UsageCount)
//             : _Manager(Manager), _Resource(Resource), _UsageCount(UsageCount)
//         {
//         }

//         FResourceGuard(const FResourceGuard&) = delete;
        
//         FResourceGuard(FResourceGuard&& Other) noexcept
//             : _Manager(std::exchange(Other._Manager, nullptr)),
//               _Resource(std::exchange(Other._Resource, nullptr)),
//               _UsageCount(std::exchange(Other._UsageCount, 0))
//         {
//         }

//         ~FResourceGuard()
//         {
//             if (_Manager != nullptr && _Resource != nullptr)
//             {
//                 _Manager->ReleaseResource(_Resource, _UsageCount);
//                 --_Manager->_BusyResourceCount;

//                 std::uint32_t BusyResourceCount = _Manager->_BusyResourceCount.load();
//                 std::uint32_t PeakResourceDemand = _Manager->_PeakResourceDemand.load();
//                 while (BusyResourceCount > PeakResourceDemand && 
//                        !_Manager->_PeakResourceDemand.compare_exchange_weak(PeakResourceDemand, BusyResourceCount));
//             }
//         }

//         FResourceGuard& operator=(const FResourceGuard&) = delete;
        
//         FResourceGuard& operator=(FResourceGuard&& Other) noexcept
//         {
//             if (this != &Other)
//             {
//                 _Manager = std::exchange(Other._Manager, nullptr);
//                 _Resource = std::exchange(Other._Resource, nullptr);
//                 _UsageCount = std::exchange(Other._UsageCount, 0);
//             }
//             return *this;
//         }

//         TResource* operator*() { return _Resource; }
//         const TResource* operator*() const { return _Resource; }

//     private:
//         TResourcePoolBase* _Manager;
//         TResource* _Resource;
//         std::size_t _UsageCount;
//     };

// protected:
//     // 资源信息结构
//     struct FResourceInfo
//     {
//         std::unique_ptr<TResource> Resource;
//         std::size_t LastUsedTimestamp;
//         std::size_t UsageCount;
//         // 派生类可以添加更多特定信息
//     };

//     // 用于资源匹配的谓词类型
//     using FMatchPredicate = std::function<bool(const FResourceInfo&)>;

// public:
//     TResourcePoolBase(std::uint32_t MinResourceLimit = 4,
//                      std::uint32_t MaxResourceLimit = 128,
//                      std::uint32_t ResourceReclaimThresholdMs = 10000,
//                      std::uint32_t MaintenanceIntervalMs = 30000)
//         : _MinResourceLimit(MinResourceLimit),
//           _MaxResourceLimit(MaxResourceLimit),
//           _ResourceReclaimThresholdMs(ResourceReclaimThresholdMs),
//           _MaintenanceIntervalMs(MaintenanceIntervalMs)
//     {
//         // 启动维护线程
//         std::thread([this]() -> void { Maintenance(); }).detach();
//     }

//     virtual ~TResourcePoolBase()
//     {
//         _StopMaintenance.store(true);
//         std::this_thread::sleep_for(std::chrono::milliseconds(_MaintenanceIntervalMs * 2));
//     }

//     // 禁止复制
//     TResourcePoolBase(const TResourcePoolBase&) = delete;
//     TResourcePoolBase& operator=(const TResourcePoolBase&) = delete;

//     // 改进后的AcquireResourceWithPredicate方法
//     FResourceGuard AcquireResourceWithPredicate(const FMatchPredicate& MatchPredicate, const TInfo& CreateInfo)
//     {
//         std::unique_lock<std::mutex> Lock(_Mutex);
        
//         // 找到所有匹配的资源
//         std::vector<typename std::deque<FResourceInfo>::iterator> matchingResources;
//         for (auto it = _AvailableResources.begin(); it != _AvailableResources.end(); ++it)
//         {
//             if (MatchPredicate(*it))
//             {
//                 matchingResources.push_back(it);
//             }
//         }
        
//         if (!matchingResources.empty())
//         {
//             // 如果有多个匹配项且数量超过阈值，按使用频率排序（优先选择高频使用资源）
//             if (matchingResources.size() > 1)
//             {
//                 std::sort(matchingResources.begin(), matchingResources.end(),
//                     [](auto& a, auto& b) -> bool {
//                         // 先按使用频率（降序）
//                         if (a->UsageCount != b->UsageCount)
//                             return a->UsageCount > b->UsageCount;
                        
//                         // 次之按最近使用时间（降序）
//                         return a->LastUsedTimestamp > b->LastUsedTimestamp;
//                     });
//             }
            
//             // 使用排序后的第一个（最热）资源
//             auto bestIt = matchingResources.front();
//             TResource* Resource = bestIt->Resource.release();
//             std::size_t UsageCount = bestIt->UsageCount + 1;
//             _AvailableResources.erase(bestIt);
//             ++_BusyResourceCount;
//             return FResourceGuard(this, Resource, UsageCount);
//         }
        
//         // 没找到匹配的，检查是否可以创建新的
//         if (_BusyResourceCount.load() + _AvailableResources.size() < _MaxResourceLimit)
//         {
//             CreateResource(CreateInfo);
            
//             TResource* Resource = _AvailableResources.back().Resource.release();
//             _AvailableResources.pop_back();
//             ++_BusyResourceCount;
//             return FResourceGuard(this, Resource, 1);
//         }
//         else
//         {
//             // 达到限制，需要等待
//             constexpr std::uint32_t kMaxWaitTimeMs = 200;
            
//             if (_Condition.wait_for(Lock, std::chrono::milliseconds(kMaxWaitTimeMs),
//                 [this, &MatchPredicate]() -> bool {
//                     return std::any_of(_AvailableResources.begin(), _AvailableResources.end(), MatchPredicate);
//                 }))
//             {
//                 // 重新调用自身以应用相同的排序策略
//                 Lock.unlock(); // 必须解锁，因为即将递归调用
//                 return AcquireResourceWithPredicate(MatchPredicate, CreateInfo);
//             }
//             else
//             {
//                 // 等待超时，尝试紧急策略
//                 if (_AvailableResources.size() > 4)
//                 {
//                     std::sort(_AvailableResources.begin(), _AvailableResources.end(),
//                         [](const FResourceInfo& Lhs, const FResourceInfo& Rhs) -> bool {
//                             return Lhs.UsageCount < Rhs.UsageCount;
//                         });
                    
//                     // 第一个策略：找一个可替代的低使用率资源
//                     for (auto it = _AvailableResources.begin(); it != _AvailableResources.end(); ++it)
//                     {
//                         if (HandleResourceEmergency(*it, CreateInfo))
//                         {
//                             TResource* Resource = _AvailableResources.back().Resource.release();
//                             _AvailableResources.pop_back();
//                             ++_BusyResourceCount;
//                             return FResourceGuard(this, Resource, 1);
//                         }
//                     }
//                 }
                
//                 // 仍然无法满足，抛出异常
//                 throw std::runtime_error("Failed to acquire resource. Reset the max resource limit or reduce resource requirements.");
//             }
//         }
//     }

//     // 优化资源数量
//     void OptimizeResourceCount()
//     {
//         std::size_t CurrentTime = GetCurrentTimeMs();
//         std::lock_guard<std::mutex> Lock(_Mutex);
        
//         std::uint32_t TargetCount = std::max(_MinResourceLimit, _PeakResourceDemand.load());
        
//         // 阶段1：移除长时间未使用的资源
//         if (_AvailableResources.size() > TargetCount)
//         {
//             std::erase_if(_AvailableResources, [&](const FResourceInfo& Info) -> bool {
//                 return CurrentTime - Info.LastUsedTimestamp > _ResourceReclaimThresholdMs;
//             });
//         }
        
//         // 阶段2：如果仍然超过目标数量，保留使用频率最高的资源
//         if (_AvailableResources.size() > TargetCount)
//         {
//             std::sort(_AvailableResources.begin(), _AvailableResources.end(),
//                 [](const FResourceInfo& Lhs, const FResourceInfo& Rhs) -> bool {
//                     return Lhs.UsageCount > Rhs.UsageCount;
//                 });
            
//             _AvailableResources.resize(TargetCount);
//         }
//     }

//     // 释放所有资源
//     void FreeAllResources()
//     {
//         std::lock_guard<std::mutex> Lock(_Mutex);
//         _AvailableResources.clear();
//     }

//     // 配置方法
//     void SetMinResourceLimit(std::uint32_t MinResourceLimit)
//     {
//         _MinResourceLimit = std::min(MinResourceLimit, _MaxResourceLimit);
//     }

//     void SetMaxResourceLimit(std::uint32_t MaxResourceLimit)
//     {
//         _MaxResourceLimit = std::max(MaxResourceLimit, _MinResourceLimit);
//     }

//     void SetResourceReclaimThreshold(std::uint32_t ResourceReclaimThresholdMs)
//     {
//         _ResourceReclaimThresholdMs = ResourceReclaimThresholdMs;
//     }

//     void SetMaintenanceInterval(std::uint32_t MaintenanceIntervalMs)
//     {
//         _MaintenanceIntervalMs = MaintenanceIntervalMs;
//     }

//     std::uint32_t GetMinResourceLimit() const { return _MinResourceLimit; }
//     std::uint32_t GetMaxResourceLimit() const { return _MaxResourceLimit; }
//     std::uint32_t GetResourceReclaimThreshold() const { return _ResourceReclaimThresholdMs; }
//     std::uint32_t GetMaintenanceInterval() const { return _MaintenanceIntervalMs; }
    
//     // 获取当前统计信息
//     struct FPoolStats
//     {
//         std::size_t AvailableCount;
//         std::size_t BusyCount;
//         std::size_t PeakDemand;
//     };
    
//     FPoolStats GetStats() const
//     {
//         return { 
//             _AvailableResources.size(),
//             _BusyResourceCount.load(),
//             _PeakResourceDemand.load()
//         };
//     }

// protected:
//     // 派生类必须实现的方法
//     virtual void CreateResource(const TInfo& CreateInfo) = 0;
//     virtual bool HandleResourceEmergency(FResourceInfo& LowUsageResource, const TInfo& RequestedInfo) = 0;
    
//     // 释放资源（可被覆盖，但必须调用基类实现）
//     virtual void ReleaseResource(TResource* Resource, std::size_t UsageCount)
//     {
//         std::lock_guard<std::mutex> Lock(_Mutex);
        
//         FResourceInfo ResourceInfo;
//         ResourceInfo.Resource = std::unique_ptr<TResource>(Resource);
//         ResourceInfo.LastUsedTimestamp = GetCurrentTimeMs();
//         ResourceInfo.UsageCount = UsageCount;
        
//         OnResourceReleased(ResourceInfo);
        
//         _AvailableResources.push_back(std::move(ResourceInfo));
//         _Condition.notify_one();
//     }
    
//     // 资源释放时的钩子（派生类可以重写以添加额外处理）
//     virtual void OnResourceReleased(FResourceInfo& ResourceInfo) {}

// private:
//     void Maintenance()
//     {
//         while (!_StopMaintenance.load())
//         {
//             std::this_thread::sleep_for(std::chrono::milliseconds(_MaintenanceIntervalMs));
//             if (!_StopMaintenance.load())
//             {
//                 OptimizeResourceCount();
//             }
//         }
//     }

//     std::size_t GetCurrentTimeMs() const
//     {
//         return std::chrono::duration_cast<std::chrono::milliseconds>(
//             std::chrono::system_clock::now().time_since_epoch()).count();
//     }

// protected:
//     std::mutex _Mutex;
//     std::condition_variable _Condition;
//     std::deque<FResourceInfo> _AvailableResources;
//     std::atomic<std::uint32_t> _BusyResourceCount{0};
//     std::atomic<std::uint32_t> _PeakResourceDemand{0};

//     std::uint32_t _MinResourceLimit;
//     std::uint32_t _MaxResourceLimit;
//     std::uint32_t _ResourceReclaimThresholdMs;
//     std::uint32_t _MaintenanceIntervalMs;

//     std::atomic<bool> _StopMaintenance{false};
// };

// _GRAPHICS_END
// _RUNTIME_END
// _NPGS_END

// #pragma once

// #include "ResourcePoolBase.h"
// #include "Wrappers.h"

// _NPGS_BEGIN
// _RUNTIME_BEGIN
// _GRAPHICS_BEGIN

// // 命令池创建信息
// struct FCommandPoolCreateInfo
// {
//     vk::Device Device;
//     std::uint32_t QueueFamilyIndex;
//     vk::CommandPoolCreateFlags Flags{};
// };

// class FCommandPoolManager : public TResourcePoolBase<FVulkanCommandPool, FCommandPoolCreateInfo>
// {
// public:
//     using Base = TResourcePoolBase<FVulkanCommandPool, FCommandPoolCreateInfo>;
//     using FPoolGuard = typename Base::FResourceGuard;
    
//     FCommandPoolManager(vk::Device Device, std::uint32_t QueueFamilyIndex,
//                        std::uint32_t MinPoolLimit = 4,
//                        std::uint32_t MaxPoolLimit = 128)
//         : Base(MinPoolLimit, MaxPoolLimit),
//           _Device(Device),
//           _QueueFamilyIndex(QueueFamilyIndex)
//     {
//         // 预创建池
//         for (std::uint32_t i = 0; i < MinPoolLimit; ++i)
//         {
//             FCommandPoolCreateInfo CreateInfo{
//                 _Device,
//                 _QueueFamilyIndex,
//                 vk::CommandPoolCreateFlagBits::eResetCommandBuffer
//             };
//             CreateResource(CreateInfo);
//         }
//     }
    
//     // 简化的获取池接口
//     FPoolGuard AcquirePool()
//     {
//         return AcquireResourceWithPredicate(
//             [](const FResourceInfo&) { return true; },  // 匹配任何可用池
//             FCommandPoolCreateInfo{_Device, _QueueFamilyIndex, vk::CommandPoolCreateFlagBits::eResetCommandBuffer}
//         );
//     }

// protected:
//     // 实现基类要求的方法
//     void CreateResource(const FCommandPoolCreateInfo& CreateInfo) override
//     {
//         FResourceInfo PoolInfo;
//         PoolInfo.Resource = std::make_unique<FVulkanCommandPool>(
//             CreateInfo.Device,
//             CreateInfo.QueueFamilyIndex,
//             CreateInfo.Flags
//         );
//         PoolInfo.LastUsedTimestamp = GetCurrentTimeMs();
//         PoolInfo.UsageCount = 0;
        
//         _AvailableResources.push_back(std::move(PoolInfo));
//     }
    
//     bool HandleResourceEmergency(FResourceInfo& LowUsageResource, const FCommandPoolCreateInfo& RequestedInfo) override
//     {
//         // CommandPool不需要特殊处理，因为所有池都是相同类型
//         return true;
//     }
    
//     void OnResourceReleased(FResourceInfo& ResourceInfo) override
//     {
//         // 可以在这里添加池重置逻辑
//         // ResourceInfo.Resource->reset(...);
//     }

// private:
//     vk::Device _Device;
//     std::uint32_t _QueueFamilyIndex;
// };

// _GRAPHICS_END
// _RUNTIME_END
// _NPGS_END

#pragma once

#include "ResourcePoolBase.h"
#include "Resources.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

// 暂存缓冲区创建信息
struct FStagingBufferCreateInfo
{
    vk::Device Device;
    vk::DeviceSize Size;
    const VmaAllocationCreateInfo* AllocationCreateInfo;
};

class FStagingBufferPool : public TResourcePoolBase<FStagingBuffer, FStagingBufferCreateInfo>
{
private:
    // 扩展的资源信息，包含缓冲区大小和VMA状态
    struct FStagingBufferInfo : public FResourceInfo
    {
        vk::DeviceSize Size;
        bool UseVma;
    };

public:
    using Base = TResourcePoolBase<FStagingBuffer, FStagingBufferCreateInfo>;
    using FBufferGuard = typename Base::FResourceGuard;
    
    FStagingBufferPool(vk::Device Device,
                      std::uint32_t MinBufferLimit = 4,
                      std::uint32_t MaxBufferLimit = 128)
        : Base(MinBufferLimit, MaxBufferLimit),
          _Device(Device)
    {
        // 预创建一些基本缓冲区
        for (std::uint32_t i = 0; i < MinBufferLimit; ++i)
        {
            FStagingBufferCreateInfo CreateInfo{
                _Device,
                64 * 1024, // 64KB默认大小
                nullptr    // 默认不使用VMA
            };
            CreateResource(CreateInfo);
        }
    }
    
    // 简化的获取缓冲区接口
    FBufferGuard AcquireBuffer(vk::DeviceSize Size, const VmaAllocationCreateInfo* AllocationCreateInfo = nullptr)
    {
        bool UseVma = (AllocationCreateInfo != nullptr);
        
        return AcquireResourceWithPredicate(
            [Size, UseVma](const FResourceInfo& BaseInfo) -> bool {
                auto& Info = static_cast<const FStagingBufferInfo&>(BaseInfo);
                return Info.Size >= Size && Info.UseVma == UseVma;
            },
            FStagingBufferCreateInfo{_Device, Size, AllocationCreateInfo}
        );
    }

protected:
    // 实现基类要求的方法
    void CreateResource(const FStagingBufferCreateInfo& CreateInfo) override
    {
        auto BufferInfo = std::make_unique<FStagingBufferInfo>();
        
        if (CreateInfo.AllocationCreateInfo)
        {
            vk::BufferCreateInfo BufferCreateInfo({}, CreateInfo.Size, 
                vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);
            BufferInfo->Resource = std::make_unique<FStagingBuffer>(*CreateInfo.AllocationCreateInfo, BufferCreateInfo);
            BufferInfo->UseVma = true;
        }
        else
        {
            BufferInfo->Resource = std::make_unique<FStagingBuffer>(CreateInfo.Size);
            BufferInfo->UseVma = false;
        }
        
        // 设置为持久映射，提高效率
        BufferInfo->Resource->GetMemory().SetPersistentMapping(true);
        
        BufferInfo->LastUsedTimestamp = GetCurrentTimeMs();
        BufferInfo->UsageCount = 0;
        BufferInfo->Size = CreateInfo.Size;
        
        _AvailableResources.push_back(std::move(*BufferInfo));
    }
    
    bool HandleResourceEmergency(FResourceInfo& BaseResourceInfo, const FStagingBufferCreateInfo& RequestedInfo) override
    {
        auto& BufferInfo = static_cast<FStagingBufferInfo&>(BaseResourceInfo);
        
        // 匹配VMA类型，但大小不足，创建新的更大缓冲区
        bool RequestedVma = (RequestedInfo.AllocationCreateInfo != nullptr);
        if (BufferInfo.UseVma == RequestedVma && BufferInfo.Size < RequestedInfo.Size)
        {
            // 创建一个比请求更大的缓冲区
            vk::DeviceSize NewSize = std::max(RequestedInfo.Size, BufferInfo.Size * 2);
            FStagingBufferCreateInfo NewInfo{
                _Device,
                NewSize,
                RequestedInfo.AllocationCreateInfo
            };
            CreateResource(NewInfo);
            return true;
        }
        
        return false;
    }
    
    void ReleaseResource(FStagingBuffer* Buffer, std::size_t UsageCount) override
    {
        std::lock_guard<std::mutex> Lock(_Mutex);
        
        auto BufferInfo = std::make_unique<FStagingBufferInfo>();
        BufferInfo->Resource = std::unique_ptr<FStagingBuffer>(Buffer);
        BufferInfo->LastUsedTimestamp = GetCurrentTimeMs();
        BufferInfo->UsageCount = UsageCount;
        BufferInfo->Size = Buffer->GetMemory().GetAllocationSize();
        BufferInfo->UseVma = Buffer->IsUsingVma();
        
        _AvailableResources.push_back(std::move(*BufferInfo));
        _Condition.notify_one();
    }

private:
    vk::Device _Device;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END