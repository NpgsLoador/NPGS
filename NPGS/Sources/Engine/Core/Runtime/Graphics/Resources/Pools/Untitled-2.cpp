#pragma once

#include "ResourcePoolBase.h"
#include "Resources.h"
#include <array>
#include <map>
#include <vector>
#include <deque>
#include <algorithm>

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
    // 预定义的缓冲区大小分级（桶）
    static constexpr std::array<vk::DeviceSize, 7> kSizeTiers = {
        64 * 1024,          // 64KB
        256 * 1024,         // 256KB
        1 * 1024 * 1024,    // 1MB
        4 * 1024 * 1024,    // 4MB
        16 * 1024 * 1024,   // 16MB
        64 * 1024 * 1024,   // 64MB
        256 * 1024 * 1024   // 256MB
    };

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
                      std::uint32_t MaxBufferLimit = 128,
                      std::uint32_t BufferReclaimThresholdMs = 30000,
                      std::uint32_t MaintenanceIntervalMs = 60000)
        : Base(MinBufferLimit, MaxBufferLimit, BufferReclaimThresholdMs, MaintenanceIntervalMs),
          _Device(Device)
    {
        // 初始化VMA默认参数
        _VmaAllocationDefaults.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        _VmaAllocationDefaults.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        
        // 预创建不同大小级别的基本缓冲区
        std::array<vk::DeviceSize, 3> initialSizes = {
            64 * 1024,        // 小型 64KB
            1 * 1024 * 1024,  // 中型 1MB
            4 * 1024 * 1024   // 大型 4MB
        };
        
        std::uint32_t initialCount = std::min(MinBufferLimit, static_cast<std::uint32_t>(2 * initialSizes.size()));
        for (std::uint32_t i = 0; i < initialCount; ++i)
        {
            vk::DeviceSize size = initialSizes[i % initialSizes.size()];
            bool useVma = (i >= initialSizes.size()); // 创建一半VMA, 一半非VMA
            
            FStagingBufferCreateInfo CreateInfo{
                _Device,
                size,
                useVma ? &_VmaAllocationDefaults : nullptr
            };
            CreateResource(CreateInfo);
        }
        
        // 初始化大小使用统计
        _LastSizeStatsCleanupTime = GetCurrentTimeMs();
    }
    
    // 分桶策略获取缓冲区
    FBufferGuard AcquireBuffer(vk::DeviceSize Size, const VmaAllocationCreateInfo* AllocationCreateInfo = nullptr)
    {
        // 记录大小请求以进行统计
        RecordSizeRequest(Size, AllocationCreateInfo != nullptr);
        
        bool UseVma = (AllocationCreateInfo != nullptr);
        
        // 将请求大小对齐到下一个分级大小
        vk::DeviceSize AlignedSize = AlignSizeToNextTier(Size);
        
        const VmaAllocationCreateInfo* allocInfo = UseVma ? 
            AllocationCreateInfo : nullptr;
            
        // 使用分桶策略匹配缓冲区
        return AcquireResourceWithPredicate(
            [Size, AlignedSize, UseVma](const FResourceInfo& BaseInfo) -> bool {
                auto& Info = static_cast<const FStagingBufferInfo&>(BaseInfo);
                // 匹配条件:
                // 1. 缓冲区大小足够
                // 2. 缓冲区不会过大(不超过对齐后大小的2倍或最多多1MB)
                // 3. VMA类型匹配
                return Info.Size >= Size && 
                       (Info.Size <= AlignedSize * 2 || Info.Size <= Size + 1 * 1024 * 1024) &&
                       Info.UseVma == UseVma;
            },
            FStagingBufferCreateInfo{_Device, AlignedSize, allocInfo}
        );
    }
    
    // 获取按桶分类的缓冲区统计
    struct FSizeStats {
        std::size_t Count;          // 缓冲区数量
        std::size_t TotalUsages;    // 总使用次数
        std::size_t RequestCount;   // 请求次数
    };
    
    std::map<std::pair<bool, vk::DeviceSize>, FSizeStats> GetBufferSizeStats() const
    {
        std::lock_guard<std::mutex> Lock(_Mutex);
        std::lock_guard<std::mutex> StatsLock(_SizeUsageStatsMutex);
        
        std::map<std::pair<bool, vk::DeviceSize>, FSizeStats> stats;
        
        // 当前可用缓冲区统计
        for (const auto& item : _AvailableResources)
        {
            auto& info = static_cast<const FStagingBufferInfo&>(item);
            auto key = std::make_pair(info.UseVma, info.Size);
            stats[key].Count++;
            stats[key].TotalUsages += info.UsageCount;
        }
        
        // 添加请求统计
        for (auto& pair : _SizeRequestFrequency)
        {
            auto key = std::make_pair(pair.first.second, pair.first.first);
            stats[key].RequestCount = pair.second;
        }
        
        return stats;
    }
    
    // 重置池并释放所有缓冲区
    void Reset()
    {
        std::lock_guard<std::mutex> Lock(_Mutex);
        std::lock_guard<std::mutex> StatsLock(_SizeUsageStatsMutex);
        
        _AvailableResources.clear();
        _SizeRequestHistory.clear();
        _SizeRequestFrequency.clear();
        
        // 重新创建基本缓冲区
        std::array<vk::DeviceSize, 2> initialSizes = {
            64 * 1024,       // 小型
            1 * 1024 * 1024  // 中型
        };
        
        for (std::uint32_t i = 0; i < _MinResourceLimit; ++i)
        {
            vk::DeviceSize size = initialSizes[i % initialSizes.size()];
            FStagingBufferCreateInfo CreateInfo{
                _Device,
                size,
                nullptr // 默认不使用VMA
            };
            CreateResource(CreateInfo);
        }
    }

protected:
    // 实现分桶创建资源
    void CreateResource(const FStagingBufferCreateInfo& CreateInfo) override
    {
        auto BufferInfo = std::make_unique<FStagingBufferInfo>();
        
        // 对齐请求大小到分级大小
        vk::DeviceSize AlignedSize = AlignSizeToNextTier(CreateInfo.Size);
        
        if (CreateInfo.AllocationCreateInfo)
        {
            vk::BufferCreateInfo BufferCreateInfo({}, AlignedSize, 
                vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);
            BufferInfo->Resource = std::make_unique<FStagingBuffer>(*CreateInfo.AllocationCreateInfo, BufferCreateInfo);
            BufferInfo->UseVma = true;
        }
        else
        {
            BufferInfo->Resource = std::make_unique<FStagingBuffer>(AlignedSize);
            BufferInfo->UseVma = false;
        }
        
        // 设置为持久映射，提高效率
        BufferInfo->Resource->GetMemory().SetPersistentMapping(true);
        
        BufferInfo->LastUsedTimestamp = GetCurrentTimeMs();
        BufferInfo->UsageCount = 0;
        BufferInfo->Size = AlignedSize;
        
        _AvailableResources.push_back(std::move(*BufferInfo));
        
        // 检查是否应该创建额外的备用缓冲区
        TryPreallocateBuffers(CreateInfo.Size, CreateInfo.AllocationCreateInfo != nullptr);
    }
    
    // 紧急情况处理 - 当找不到合适大小的缓冲区时
    bool HandleResourceEmergency(FResourceInfo& BaseResourceInfo, const FStagingBufferCreateInfo& RequestedInfo) override
    {
        auto& BufferInfo = static_cast<FStagingBufferInfo&>(BaseResourceInfo);
        
        // 匹配VMA类型，但大小不足，创建新的更大缓冲区
        bool RequestedVma = (RequestedInfo.AllocationCreateInfo != nullptr);
        if (BufferInfo.UseVma == RequestedVma && BufferInfo.Size < RequestedInfo.Size)
        {
            // 创建一个比请求更大的缓冲区，策略是取大一级或两倍大小
            vk::DeviceSize NewSize = std::max(AlignSizeToNextTier(RequestedInfo.Size), BufferInfo.Size * 2);
            
            // 限制最大扩展大小
            if (NewSize > kSizeTiers.back())
            {
                NewSize = BufferInfo.Size * 1.5;
            }
            
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
    
    // 重写资源释放，处理特定的缓冲区属性
    void ReleaseResource(FStagingBuffer* Buffer, std::size_t UsageCount) override
    {
        std::lock_guard<std::mutex> Lock(_Mutex);
        
        auto BufferInfo = std::make_unique<FStagingBufferInfo>();
        BufferInfo->Resource = std::unique_ptr<FStagingBuffer>(Buffer);
        BufferInfo->LastUsedTimestamp = GetCurrentTimeMs();
        BufferInfo->UsageCount = UsageCount;
        BufferInfo->Size = Buffer->GetMemory().GetAllocationSize();
        BufferInfo->UseVma = Buffer->IsUsingVma();
        
        // 确保缓冲区仍然持久映射
        Buffer->GetMemory().SetPersistentMapping(true);
        
        _AvailableResources.push_back(std::move(*BufferInfo));
        _Condition.notify_one();
    }
    
    // 重写优化资源数量，实现分桶优化
    void OptimizeResourceCount() override
    {
        std::size_t CurrentTime = GetCurrentTimeMs();
        std::lock_guard<std::mutex> Lock(_Mutex);
        
        // 收集各种大小级别的缓冲区使用情况统计
        std::map<vk::DeviceSize, std::size_t> sizeUsageStats;  // 大小 -> 使用次数
        std::map<vk::DeviceSize, std::size_t> sizeCountStats;  // 大小 -> 缓冲区数量
        std::map<bool, std::size_t> vmaUsageStats;            // VMA状态 -> 使用次数
        
        for (const auto& Info : _AvailableResources)
        {
            auto& BufferInfo = static_cast<const FStagingBufferInfo&>(Info);
            sizeUsageStats[BufferInfo.Size] += BufferInfo.UsageCount;
            sizeCountStats[BufferInfo.Size]++;
            vmaUsageStats[BufferInfo.UseVma] += BufferInfo.UsageCount;
        }
        
        std::uint32_t TargetCount = std::max(_MinResourceLimit, _PeakResourceDemand.load());
        
        // 第一阶段：移除长时间未使用的缓冲区，但保留每个大小级别至少一个缓冲区
        std::map<std::pair<bool, vk::DeviceSize>, bool> removedSizes;  // (VMA状态, 大小) -> 是否已移除

        std::erase_if(_AvailableResources, [&](const FResourceInfo& Info) -> bool {
            if (_AvailableResources.size() <= _MinResourceLimit) 
                return false;
            
            auto& BufferInfo = static_cast<const FStagingBufferInfo&>(Info);
            auto key = std::make_pair(BufferInfo.UseVma, BufferInfo.Size);
            
            // 如果这个大小级别已经有缓冲区被删除，我们不再保护它
            bool alreadyRemovedThisSize = removedSizes[key];
            
            // 是否应该删除: 过期且(已有相同大小被保留 或 使用率低)
            bool shouldRemove = CurrentTime - Info.LastUsedTimestamp > _ResourceReclaimThresholdMs && 
                               (alreadyRemovedThisSize || Info.UsageCount < 5);
            
            if (shouldRemove) {
                removedSizes[key] = true;
            }
            
            return shouldRemove;
        });
        
        // 第二阶段：如果缓冲区仍然太多，按多级排序策略优化
        if (_AvailableResources.size() > TargetCount)
        {
            // 计算每种类型的总使用率
            std::map<std::pair<bool, vk::DeviceSize>, std::size_t> categoryUsage;  
            for (const auto& Info : _AvailableResources)
            {
                auto& BufferInfo = static_cast<const FStagingBufferInfo&>(Info);
                categoryUsage[{BufferInfo.UseVma, BufferInfo.Size}] += BufferInfo.UsageCount;
            }
            
            // 按复合优先级排序
            std::sort(_AvailableResources.begin(), _AvailableResources.end(),
                [&sizeCountStats, &categoryUsage](const FResourceInfo& a, const FResourceInfo& b) -> bool {
                    auto& aInfo = static_cast<const FStagingBufferInfo&>(a);
                    auto& bInfo = static_cast<const FStagingBufferInfo&>(b);
                    
                    // 优先级1: 每种类型至少保留一个（防止某种大小完全消失）
                    bool aIsLastOfType = (sizeCountStats[aInfo.Size] <= 1);
                    bool bIsLastOfType = (sizeCountStats[bInfo.Size] <= 1);
                    if (aIsLastOfType != bIsLastOfType)
                        return aIsLastOfType;
                    
                    // 优先级2: 分类使用频率（保留高频使用的大小级别）
                    std::size_t aTypeUsage = categoryUsage[{aInfo.UseVma, aInfo.Size}];
                    std::size_t bTypeUsage = categoryUsage[{bInfo.UseVma, bInfo.Size}];
                    if (aTypeUsage != bTypeUsage)
                        return aTypeUsage > bTypeUsage;
                        
                    // 优先级3: 个体使用频率
                    if (a.UsageCount != b.UsageCount)
                        return a.UsageCount > b.UsageCount;
                    
                    // 优先级4: 最近使用时间
                    return a.LastUsedTimestamp > b.LastUsedTimestamp;
                });
            
            // 保留目标数量的缓冲区
            _AvailableResources.resize(TargetCount);
        }
        
        // 第三阶段：检查是否需要收缩大型缓冲区
        const std::size_t kCompactionIntervalMs = 10 * 60 * 1000; // 每10分钟检查
        if (CurrentTime - _LastCompactionTime > kCompactionIntervalMs)
        {
            _LastCompactionTime = CurrentTime;
            CompactOversizedBuffers();
        }
        
        // 清理过期的大小请求统计
        CleanupSizeRequestStats(CurrentTime);
    }

private:
    // 将请求大小对齐到预定义的分级大小
    vk::DeviceSize AlignSizeToNextTier(vk::DeviceSize RequestedSize) const
    {
        // 找到大于等于请求大小的最小级别
        auto it = std::lower_bound(kSizeTiers.begin(), kSizeTiers.end(), RequestedSize);
        
        // 如果找到合适的级别，返回它
        if (it != kSizeTiers.end()) {
            return *it;
        }
        
        // 如果超过最大预定级别，则按2MB对齐
        const vk::DeviceSize kLargeBufferAlignment = 2 * 1024 * 1024; // 2MB
        return ((RequestedSize + kLargeBufferAlignment - 1) / kLargeBufferAlignment) * kLargeBufferAlignment;
    }
    
    // 记录大小请求统计
    void RecordSizeRequest(vk::DeviceSize Size, bool UseVma)
    {
        std::lock_guard<std::mutex> Lock(_SizeUsageStatsMutex);
        
        std::size_t currentTime = GetCurrentTimeMs();
        // 记录为对齐后的大小，便于统计
        vk::DeviceSize alignedSize = AlignSizeToNextTier(Size);
        
        _SizeRequestHistory.push_back({currentTime, alignedSize, UseVma});
        _SizeRequestFrequency[{alignedSize, UseVma}]++;
    }
    
    // 清理过期的大小请求统计
    void CleanupSizeRequestStats(std::size_t CurrentTime)
    {
        std::lock_guard<std::mutex> Lock(_SizeUsageStatsMutex);
        
        if (CurrentTime - _LastSizeStatsCleanupTime < 60 * 1000) // 每分钟最多清理一次
            return;
            
        _LastSizeStatsCleanupTime = CurrentTime;
        
        const std::size_t kMaxHistoryCount = 1000;
        const std::size_t kMaxHistoryTimeMs = 24 * 60 * 60 * 1000; // 24小时
        
        while (!_SizeRequestHistory.empty())
        {
            if (_SizeRequestHistory.size() > kMaxHistoryCount ||
                CurrentTime - _SizeRequestHistory.front().Timestamp > kMaxHistoryTimeMs)
            {
                auto& oldRecord = _SizeRequestHistory.front();
                _SizeRequestFrequency[{oldRecord.Size, oldRecord.UseVma}]--;
                
                if (_SizeRequestFrequency[{oldRecord.Size, oldRecord.UseVma}] == 0) {
                    _SizeRequestFrequency.erase({oldRecord.Size, oldRecord.UseVma});
                }
                
                _SizeRequestHistory.pop_front();
            }
            else {
                break;
            }
        }
    }
    
    // 预分配额外缓冲区
    void TryPreallocateBuffers(vk::DeviceSize RequestedSize, bool UseVma)
    {
        if (_BusyResourceCount.load() + _AvailableResources.size() >= _MaxResourceLimit * 0.8)
            return; // 接近上限，不预分配
            
        std::lock_guard<std::mutex> StatsLock(_SizeUsageStatsMutex);
        
        // 分析请求模式
        std::map<vk::DeviceSize, int> sizeFrequency;
        
        for (const auto& record : _SizeRequestHistory) {
            if (record.UseVma == UseVma) {
                sizeFrequency[record.Size]++;
            }
        }
        
        // 如果是高频大小（至少占同类型请求的15%），预分配
        if (_SizeRequestHistory.size() >= 20 && 
            sizeFrequency[RequestedSize] >= _SizeRequestHistory.size() * 0.15)
        {
            // 检查现有此大小的缓冲区数量
            int existingCount = 0;
            for (const auto& Info : _AvailableResources)
            {
                auto& BufferInfo = static_cast<const FStagingBufferInfo&>(Info);
                if (BufferInfo.Size == RequestedSize && BufferInfo.UseVma == UseVma) {
                    existingCount++;
                }
            }
            
            // 如果现有数量少于预期需求，创建额外缓冲区
            int expectedDemand = sizeFrequency[RequestedSize] / 10 + 1;
            if (existingCount < expectedDemand && existingCount < 5) // 最多预分配5个
            {
                FStagingBufferCreateInfo ExtraInfo{
                    _Device,
                    RequestedSize,
                    UseVma ? &_VmaAllocationDefaults : nullptr
                };
                
                // 创建1-2个额外缓冲区
                for (int i = 0; i < std::min(2, expectedDemand - existingCount); i++) {
                    if (_BusyResourceCount.load() + _AvailableResources.size() < _MaxResourceLimit) {
                        CreateExtraBuffer(ExtraInfo);
                    }
                }
            }
        }
    }
    
    // 创建额外缓冲区（不触发连锁预分配）
    void CreateExtraBuffer(const FStagingBufferCreateInfo& CreateInfo)
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
        
        BufferInfo->Resource->GetMemory().SetPersistentMapping(true);
        BufferInfo->LastUsedTimestamp = GetCurrentTimeMs();
        BufferInfo->UsageCount = 0;
        BufferInfo->Size = CreateInfo.Size;
        
        _AvailableResources.push_back(std::move(*BufferInfo));
    }
    
    // 压缩过大的缓冲区
    void CompactOversizedBuffers()
    {
        // 统计每种大小的使用情况
        std::map<vk::DeviceSize, std::pair<std::size_t, std::size_t>> sizeStats; // 大小 -> (数量, 总使用率)
        
        for (const auto& Info : _AvailableResources)
        {
            auto& BufferInfo = static_cast<const FStagingBufferInfo&>(Info);
            sizeStats[BufferInfo.Size].first++;
            sizeStats[BufferInfo.Size].second += BufferInfo.UsageCount;
        }
        
        // 检查是否有超大但使用率低的缓冲区
        std::vector<vk::DeviceSize> sizesToCompact;
        
        for (const auto& [size, stats] : sizeStats)
        {
            // 超过32MB的大型缓冲区
            if (size >= 32 * 1024 * 1024)
            {
                auto avgUsage = stats.second / stats.first;
                if (avgUsage < 10) // 使用率较低
                {
                    // 检查较小的桶是否足够
                    for (auto it = kSizeTiers.rbegin(); it != kSizeTiers.rend(); ++it)
                    {
                        if (*it < size && *it >= 4 * 1024 * 1024) // 至少保留4MB
                        {
                            // 查看该大小桶的使用情况
                            if (sizeStats.find(*it) == sizeStats.end() || 
                                sizeStats[*it].first < 2) // 该大小缓冲区较少
                            {
                                sizesToCompact.push_back(size);
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        // 替换过大的缓冲区
        for (vk::DeviceSize sizeToCompact : sizesToCompact)
        {
            // 只替换一个，避免过多变动
            for (auto it = _AvailableResources.begin(); it != _AvailableResources.end(); ++it)
            {
                auto& BufferInfo = static_cast<FStagingBufferInfo&>(*it);
                if (BufferInfo.Size == sizeToCompact && BufferInfo.UsageCount < 5)
                {
                    // 找到下一级合适的大小
                    vk::DeviceSize newSize = 0;
                    for (auto sizeIt = kSizeTiers.rbegin(); sizeIt != kSizeTiers.rend(); ++sizeIt)
                    {
                        if (*sizeIt < sizeToCompact && *sizeIt >= 4 * 1024 * 1024)
                        {
                            newSize = *sizeIt;
                            break;
                        }
                    }
                    
                    if (newSize > 0)
                    {
                        // 创建新的更小的缓冲区
                        FStagingBufferCreateInfo CreateInfo{
                            _Device,
                            newSize,
                            BufferInfo.UseVma ? &_VmaAllocationDefaults : nullptr
                        };
                        CreateExtraBuffer(CreateInfo);
                        
                        // 删除旧的大缓冲区
                        _AvailableResources.erase(it);
                        break;
                    }
                }
            }
        }
    }

private:
    vk::Device _Device;
    VmaAllocationCreateInfo _VmaAllocationDefaults{};
    
    // 大小使用统计
    struct FSizeRequest {
        std::size_t Timestamp;
        vk::DeviceSize Size;
        bool UseVma;
    };
    
    mutable std::mutex _SizeUsageStatsMutex;
    std::deque<FSizeRequest> _SizeRequestHistory;
    std::map<std::pair<vk::DeviceSize, bool>, std::size_t> _SizeRequestFrequency;
    
    std::size_t _LastSizeStatsCleanupTime = 0;
    std::size_t _LastCompactionTime = 0;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END