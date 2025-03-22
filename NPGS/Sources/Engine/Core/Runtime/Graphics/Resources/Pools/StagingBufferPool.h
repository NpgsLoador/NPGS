#pragma once
//
//#include "Engine/Core/Base/Base.h"
//
//#include <cstddef>
//#include <cstdint>
//#include <atomic>
//#include <condition_variable>
//#include <deque>
//#include <memory>
//#include <mutex>
//
//#include "Engine/Core/Runtime/Graphics/Resources/Resources.h"
//#include "Engine/Core/Runtime/Graphics/Vulkan/Core.h"
//
//_NPGS_BEGIN
//_RUNTIME_BEGIN
//_GRAPHICS_BEGIN
//
//class FStagingBufferPool
//{
//public:
//    class FBufferGuard
//    {
//    public:
//        FBufferGuard(FStagingBufferPool* Pool, FStagingBuffer* Buffer, std::size_t UsageCount);
//        FBufferGuard(const FBufferGuard&) = delete;
//        FBufferGuard(FBufferGuard&& Other) noexcept;
//        ~FBufferGuard();
//
//        FBufferGuard& operator=(const FBufferGuard&) = delete;
//        FBufferGuard& operator=(FBufferGuard&& Other) noexcept;
//
//        FStagingBuffer* operator*();
//        const FStagingBuffer* operator*() const;
//
//    private:
//        FStagingBufferPool* _Pool;
//        FStagingBuffer*     _Buffer;
//        std::size_t         _UsageCount;
//    };
//
//private:
//    struct FBufferInfo
//    {
//        std::unique_ptr<FStagingBuffer> Buffer;
//        std::size_t    LastUsedTimestamp;
//        std::size_t    UsageCount;
//        vk::DeviceSize Size;
//        bool           bUseVma;
//    };
//
//public:
//    FStagingBufferPool();
//    FStagingBufferPool(const FStagingBufferPool&) = delete;
//    FStagingBufferPool(FStagingBufferPool&&)      = delete;
//    ~FStagingBufferPool();
//
//    FStagingBufferPool& operator=(const FStagingBufferPool&) = delete;
//    FStagingBufferPool& operator=(FStagingBufferPool&&)      = delete;
//
//    FBufferGuard AcquireBuffer(vk::DeviceSize Size, const VmaAllocationCreateInfo* AllocationCreateInfo = nullptr);
//    void OptimizeBufferCount();
//    void FreeAllBuffers();
//
//    void SetMinBufferLimit(std::uint32_t MinBufferLimit);
//    void SetMaxBufferLimit(std::uint32_t MaxBufferLimit);
//    void SetBufferReclaimThreshold(std::uint32_t BufferReclaimThresholdMs);
//    void SetMaintenanceInterval(std::uint32_t MaintenanceIntervalMs);
//
//    std::uint32_t GetMinBufferLimit() const;
//    std::uint32_t GetMaxBufferLimit() const;
//    std::uint32_t GetBufferReclaimThreshold() const;
//    std::uint32_t GetMaintenanceInterval() const;
//
//private:
//    void CreateBuffer(vk::DeviceSize Size, const VmaAllocationCreateInfo* AllocationCreateInfo);
//    void ReleaseBuffer(FStagingBuffer* Buffer, std::size_t UsageCount);
//    void Maintenance();
//    std::size_t GetCurrentTimeMs() const;
//
//private:
//    std::mutex                 _Mutex;
//    std::condition_variable    _Condition;
//    std::deque<FBufferInfo>    _AvailableBuffers;
//    std::atomic<std::uint32_t> _BusyBufferCount{};
//    std::atomic<std::uint32_t> _PeakBufferDemand{};
//
//    std::uint32_t              _MinBufferLimit{};
//    std::uint32_t              _MaxBufferLimit{};
//    std::uint32_t              _BufferReclaimThresholdMs{};
//    std::uint32_t              _MaintenanceIntervalMs{};
//
//    std::atomic<bool>          _bStopMaintenance{ false };
//};
//
//_GRAPHICS_END
//_RUNTIME_END
//_NPGS_END
//
#include "StagingBufferPool.inl"
