#include "DeviceLocalBuffer.h"

#include <cstddef>
#include <algorithm>
#include <utility>
#include <vector>

#include <vulkan/vulkan_to_string.hpp>

#include "Engine/Core/Runtime/Graphics/Resources/Pools/StagingBufferPool.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Core.h"

namespace Npgs
{
    FDeviceLocalBuffer::FDeviceLocalBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage)
        : _Allocator(nullptr)
    {
        CreateBuffer(Size, Usage);
    }

    FDeviceLocalBuffer::FDeviceLocalBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& BufferCreateInfo)
        : FDeviceLocalBuffer(FVulkanCore::GetClassInstance()->GetVmaAllocator(), AllocationCreateInfo, BufferCreateInfo)
    {
    }

    FDeviceLocalBuffer::FDeviceLocalBuffer(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& BufferCreateInfo)
        : _Allocator(Allocator)
    {
        CreateBuffer(AllocationCreateInfo, BufferCreateInfo);
    }

    FDeviceLocalBuffer::FDeviceLocalBuffer(FDeviceLocalBuffer&& Other) noexcept
        : _BufferMemory(std::move(Other._BufferMemory))
        , _Allocator(std::exchange(Other._Allocator, nullptr))
    {
    }

    FDeviceLocalBuffer& FDeviceLocalBuffer::operator=(FDeviceLocalBuffer&& Other) noexcept
    {
        if (this != &Other)
        {
            _BufferMemory = std::move(Other._BufferMemory);
            _Allocator    = std::exchange(Other._Allocator, nullptr);
        }

        return *this;
    }

    void FDeviceLocalBuffer::CopyData(vk::DeviceSize MapOffset, vk::DeviceSize TargetOffset, vk::DeviceSize Size, const void* Data) const
    {
        if (_BufferMemory->GetMemory().GetMemoryPropertyFlags() & vk::MemoryPropertyFlagBits::eHostVisible)
        {
            _BufferMemory->SubmitBufferData(MapOffset, TargetOffset, Size, Data);
            return;
        }

        auto* VulkanContext = FVulkanContext::GetClassInstance();

        VmaAllocationCreateInfo StagingCreateInfo{ .usage = VMA_MEMORY_USAGE_CPU_TO_GPU };
        VmaAllocationCreateInfo* AllocationCreateInfo = _Allocator ? &StagingCreateInfo : nullptr;

        auto* StagingBufferPool = FStagingBufferPool::GetInstance();
        auto* StagingBuffer     = StagingBufferPool->AcquireBuffer(Size, AllocationCreateInfo);
        StagingBuffer->SubmitBufferData(MapOffset, TargetOffset, Size, Data);

        auto& TransferCommandBuffer = VulkanContext->GetTransferCommandBuffer();
        TransferCommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        vk::BufferCopy Region(0, TargetOffset, Size);
        TransferCommandBuffer->copyBuffer(*StagingBuffer->GetBuffer(), *_BufferMemory->GetResource(), Region);
        TransferCommandBuffer.End();
        StagingBufferPool->ReleaseBuffer(StagingBuffer);

        VulkanContext->ExecuteGraphicsCommands(TransferCommandBuffer);
    }

    void FDeviceLocalBuffer::CopyData(vk::DeviceSize ElementIndex, vk::DeviceSize ElementCount, vk::DeviceSize ElementSize,
                                      vk::DeviceSize SrcStride, vk::DeviceSize DstStride, vk::DeviceSize MapOffset, const void* Data) const
    {
        if (_BufferMemory->GetMemory().GetMemoryPropertyFlags() & vk::MemoryPropertyFlagBits::eHostVisible)
        {
            void* Target = _BufferMemory->GetMemory().GetMappedTargetMemory();
            if (Target == nullptr || !_BufferMemory->GetMemory().IsPereistentlyMapped())
            {
                _BufferMemory->MapMemoryForSubmit(MapOffset, ElementCount * DstStride, Target);
            }

            for (std::size_t i = 0; i != ElementCount; ++i)
            {
                std::copy(static_cast<const std::byte*>(Data) + SrcStride * (i + ElementIndex),
                          static_cast<const std::byte*>(Data) + SrcStride * (i + ElementIndex) + ElementSize,
                          static_cast<std::byte*>(Target)     + DstStride * (i + ElementIndex));
            }

            if (!_BufferMemory->GetMemory().IsPereistentlyMapped())
            {
                _BufferMemory->UnmapMemory(MapOffset, ElementCount * DstStride);
            }

            return;
        }

        auto* VulkanContext = FVulkanContext::GetClassInstance();

        VmaAllocationCreateInfo StagingCreateInfo{ .usage = VMA_MEMORY_USAGE_CPU_TO_GPU };
        VmaAllocationCreateInfo* AllocationCreateInfo = _Allocator ? &StagingCreateInfo : nullptr;

        auto* StagingBufferPool = FStagingBufferPool::GetInstance();
        auto* StagingBuffer     = StagingBufferPool->AcquireBuffer(DstStride * ElementSize, AllocationCreateInfo);
        StagingBuffer->SubmitBufferData(MapOffset, SrcStride * ElementIndex, SrcStride * ElementSize, Data);

        auto& TransferCommandBuffer = VulkanContext->GetTransferCommandBuffer();
        TransferCommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        std::vector<vk::BufferCopy> Regions(ElementCount);
        for (std::size_t i = 0; i < ElementCount; ++i)
        {
            Regions[i] = vk::BufferCopy(SrcStride * (i + ElementIndex), DstStride * (i + ElementIndex), ElementSize);
        }

        TransferCommandBuffer->copyBuffer(*StagingBuffer->GetBuffer(), *_BufferMemory->GetResource(), Regions);
        TransferCommandBuffer.End();
        StagingBufferPool->ReleaseBuffer(StagingBuffer);

        VulkanContext->ExecuteGraphicsCommands(TransferCommandBuffer);
    }

    vk::Result FDeviceLocalBuffer::CreateBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage)
    {
        vk::BufferCreateInfo BufferCreateInfo({}, Size, Usage | vk::BufferUsageFlagBits::eTransferDst);

        vk::MemoryPropertyFlags PreferredMemoryFlags =
            vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible;
        vk::MemoryPropertyFlags FallbackMemoryFlags  = vk::MemoryPropertyFlagBits::eDeviceLocal;

        _BufferMemory = std::make_unique<FVulkanBufferMemory>(BufferCreateInfo, PreferredMemoryFlags);
        if (!_BufferMemory->IsValid())
        {
            _BufferMemory = std::make_unique<FVulkanBufferMemory>(BufferCreateInfo, FallbackMemoryFlags);
            if (!_BufferMemory->IsValid())
            {
                return vk::Result::eErrorInitializationFailed;
            }
        }

        return vk::Result::eSuccess;
    }

    vk::Result FDeviceLocalBuffer::CreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo,
                                                const vk::BufferCreateInfo& BufferCreateInfo)
    {
        _BufferMemory = std::make_unique<FVulkanBufferMemory>(_Allocator, AllocationCreateInfo, BufferCreateInfo);
        if (!_BufferMemory->IsValid())
        {
            return vk::Result::eErrorInitializationFailed;
        }

        return vk::Result::eSuccess;
    }

    vk::Result FDeviceLocalBuffer::RecreateBuffer(vk::DeviceSize Size, vk::BufferUsageFlags Usage)
    {
        FVulkanCore::GetClassInstance()->WaitIdle();
        _BufferMemory.reset();
        return CreateBuffer(Size, Usage);
    }

    vk::Result FDeviceLocalBuffer::RecreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo,
                                                  const vk::BufferCreateInfo& BufferCreateInfo)
    {
        FVulkanCore::GetClassInstance()->WaitIdle();
        _BufferMemory.reset();
        return CreateBuffer(AllocationCreateInfo, BufferCreateInfo);
    }
} // namespace Npgs
