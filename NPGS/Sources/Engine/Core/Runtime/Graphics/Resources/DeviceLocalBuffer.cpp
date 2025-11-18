#include "stdafx.h"
#include "DeviceLocalBuffer.hpp"

#include <cstddef>
#include <algorithm>
#include <utility>
#include <vector>

#include "Engine/Core/Runtime/Pools/StagingBufferPool.hpp"

namespace Npgs
{
    FDeviceLocalBuffer::FDeviceLocalBuffer(FVulkanContext* VulkanContext, std::string_view Name, VmaAllocator Allocator,
                                           const VmaAllocationCreateInfo& AllocationCreateInfo,
                                           const vk::BufferCreateInfo& BufferCreateInfo)
        : VulkanContext_(VulkanContext)
        , Allocator_(Allocator)
        , Name_(Name)
    {
        CreateBuffer(AllocationCreateInfo, BufferCreateInfo);
    }

    FDeviceLocalBuffer::FDeviceLocalBuffer(FDeviceLocalBuffer&& Other) noexcept
        : VulkanContext_(std::exchange(Other.VulkanContext_, nullptr))
        , Allocator_(std::exchange(Other.Allocator_, nullptr))
        , BufferMemory_(std::move(Other.BufferMemory_))
        , Name_(std::move(Other.Name_))
    {
    }

    FDeviceLocalBuffer& FDeviceLocalBuffer::operator=(FDeviceLocalBuffer&& Other) noexcept
    {
        if (this != &Other)
        {
            VulkanContext_ = std::exchange(Other.VulkanContext_, nullptr);
            Allocator_     = std::exchange(Other.Allocator_, nullptr);
            BufferMemory_  = std::move(Other.BufferMemory_);
            Name_          = std::move(Other.Name_);
        }

        return *this;
    }

    void FDeviceLocalBuffer::CopyData(vk::DeviceSize MapOffset, vk::DeviceSize TargetOffset, vk::DeviceSize Size, const void* Data) const
    {
        if (BufferMemory_->GetMemory().GetMemoryPropertyFlags() & vk::MemoryPropertyFlagBits::eHostVisible)
        {
            BufferMemory_->SubmitBufferData(MapOffset, TargetOffset, Size, Data);
            return;
        }

        auto StagingBuffer = VulkanContext_->AcquireStagingBuffer(Size);
        StagingBuffer->SubmitBufferData(MapOffset, TargetOffset, Size, Data);

        auto CommandPool = VulkanContext_->AcquireCommandPool(FVulkanContext::EQueueType::kTransfer);

        FVulkanCommandBuffer TransferCommandBuffer;
        CommandPool->AllocateBuffer(vk::CommandBufferLevel::ePrimary, "TransferCommandBuffer", TransferCommandBuffer);

        TransferCommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        vk::BufferCopy Region(0, TargetOffset, Size);
        TransferCommandBuffer->copyBuffer(*StagingBuffer->GetBuffer(), *BufferMemory_->GetResource(), Region);
        TransferCommandBuffer.End();

        VulkanContext_->ExecuteCommands(FVulkanContext::EQueueType::kTransfer, TransferCommandBuffer);
        CommandPool->FreeBuffer(TransferCommandBuffer);
    }

    void FDeviceLocalBuffer::CopyData(vk::DeviceSize ElementIndex, vk::DeviceSize ElementCount, vk::DeviceSize ElementSize,
                                      vk::DeviceSize SrcStride, vk::DeviceSize DstStride, vk::DeviceSize MapOffset, const void* Data) const
    {
        if (BufferMemory_->GetMemory().GetMemoryPropertyFlags() & vk::MemoryPropertyFlagBits::eHostVisible)
        {
            void* Target = BufferMemory_->GetMemory().GetMappedTargetMemory();
            if (Target == nullptr || !BufferMemory_->GetMemory().IsPereistentlyMapped())
            {
                BufferMemory_->MapMemoryForSubmit(MapOffset, ElementCount * DstStride, Target);
            }

            for (std::size_t i = 0; i != ElementCount; ++i)
            {
                std::ranges::copy_n(static_cast<const std::byte*>(Data) + SrcStride * (i + ElementIndex), ElementSize,
                                    static_cast<std::byte*>(Target)     + DstStride * (i + ElementIndex));
            }

            if (!BufferMemory_->GetMemory().IsPereistentlyMapped())
            {
                BufferMemory_->UnmapMemory(MapOffset, ElementCount * DstStride);
            }

            return;
        }

        auto StagingBuffer = VulkanContext_->AcquireStagingBuffer(DstStride * ElementSize);
        StagingBuffer->SubmitBufferData(MapOffset, SrcStride * ElementIndex, SrcStride * ElementSize, Data);

        auto CommandPool = VulkanContext_->AcquireCommandPool(FVulkanContext::EQueueType::kTransfer);

        FVulkanCommandBuffer TransferCommandBuffer;
        CommandPool->AllocateBuffer(vk::CommandBufferLevel::ePrimary, "TransferCommandBuffer", TransferCommandBuffer);

        TransferCommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        std::vector<vk::BufferCopy> Regions(ElementCount);
        for (std::size_t i = 0; i < ElementCount; ++i)
        {
            Regions[i] = vk::BufferCopy(SrcStride * (i + ElementIndex), DstStride * (i + ElementIndex), ElementSize);
        }

        TransferCommandBuffer->copyBuffer(*StagingBuffer->GetBuffer(), *BufferMemory_->GetResource(), Regions);
        TransferCommandBuffer.End();
        
        VulkanContext_->ExecuteCommands(FVulkanContext::EQueueType::kTransfer, TransferCommandBuffer);
        CommandPool->FreeBuffer(TransferCommandBuffer);
    }

    vk::Result FDeviceLocalBuffer::CreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo,
                                                const vk::BufferCreateInfo& BufferCreateInfo)
    {
        std::string BufferName = Name_ + "_Buffer";
        std::string MemoryName = Name_ + "_Memory";

        BufferMemory_ = std::make_unique<FVulkanBufferMemory>(
            VulkanContext_->GetDevice(), BufferName, MemoryName, Allocator_, AllocationCreateInfo, BufferCreateInfo);

        if (!BufferMemory_->IsValid())
        {
            return vk::Result::eErrorInitializationFailed;
        }

        return vk::Result::eSuccess;
    }

    vk::Result FDeviceLocalBuffer::RecreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo,
                                                  const vk::BufferCreateInfo& BufferCreateInfo)
    {
        VulkanContext_->WaitIdle();
        BufferMemory_.reset();
        return CreateBuffer(AllocationCreateInfo, BufferCreateInfo);
    }
} // namespace Npgs
