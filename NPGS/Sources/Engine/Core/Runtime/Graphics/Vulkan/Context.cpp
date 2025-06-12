#include "Context.h"

// #include <cstddef>
#include <vulkan/vulkan_to_string.hpp>
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

namespace Npgs
{
    namespace
    {
        vk::SubmitInfo CreateSubmitInfo(const vk::CommandBuffer& CommandBuffer, const vk::Semaphore& WaitSemaphore,
                                        const vk::Semaphore& SignalSemaphore, vk::PipelineStageFlags Flags)
        {
            vk::SubmitInfo SubmitInfo;
            SubmitInfo.setCommandBuffers(CommandBuffer);

            if (WaitSemaphore)
            {
                SubmitInfo.setWaitSemaphores(WaitSemaphore);
                SubmitInfo.setWaitDstStageMask(Flags);
            }

            if (SignalSemaphore)
            {
                SubmitInfo.setSignalSemaphores(SignalSemaphore);
            }

            return SubmitInfo;
        }
    }

    FVulkanContext::FVulkanContext()
        : VulkanCore_(std::make_unique<FVulkanCore>())
    {
        auto InitializeResourcePool = [this]() -> void
        {
            if (VulkanCore_->GetQueueFamilyIndex(EQueueType::kGeneral) != vk::QueueFamilyIgnored)
            {
                CommandPoolPools_[VulkanCore_->GetQueueFamilyIndex(EQueueType::kGeneral)] = std::make_unique<FCommandPoolPool>(
                    8, 32, 5000, 60000, VulkanCore_->GetDevice(), VulkanCore_->GetQueueFamilyIndex(EQueueType::kGeneral));
            }
            if (VulkanCore_->GetQueueFamilyIndex(EQueueType::kCompute) != vk::QueueFamilyIgnored)
            {
                CommandPoolPools_[VulkanCore_->GetQueueFamilyIndex(EQueueType::kCompute)] = std::make_unique<FCommandPoolPool>(
                    4, 16, 5000, 60000, VulkanCore_->GetDevice(), VulkanCore_->GetQueueFamilyIndex(EQueueType::kCompute));
            }
            if (VulkanCore_->GetQueueFamilyIndex(EQueueType::kTransfer) != vk::QueueFamilyIgnored)
            {
                CommandPoolPools_[VulkanCore_->GetQueueFamilyIndex(EQueueType::kTransfer)] = std::make_unique<FCommandPoolPool>(
                    2, 8,  5000, 60000, VulkanCore_->GetDevice(), VulkanCore_->GetQueueFamilyIndex(EQueueType::kTransfer));
            }

            StagingBufferPools_[std::to_underlying(FStagingBufferPool::EPoolUsage::kSubmit)] = std::make_unique<FStagingBufferPool>(
                VulkanCore_->GetPhysicalDevice(), VulkanCore_->GetDevice(), VulkanCore_->GetVmaAllocator(),
                4, 64, 1000, 60000, FStagingBufferPool::EPoolUsage::kSubmit, true);

            StagingBufferPools_[std::to_underlying(FStagingBufferPool::EPoolUsage::kFetch)] = std::make_unique<FStagingBufferPool>(
                VulkanCore_->GetPhysicalDevice(), VulkanCore_->GetDevice(), VulkanCore_->GetVmaAllocator(),
                2, 8, 10000, 60000, FStagingBufferPool::EPoolUsage::kFetch, true);
        };

        // auto InitializeFormatProperties = [this]() -> void
        // {
        //     std::size_t Index = 0;
        //     for (vk::Format Format : magic_enum::enum_values<vk::Format>())
        //     {
        //         FormatProperties_[Index++] = VulkanCore_->GetPhysicalDevice().getFormatProperties(Format);
        //     }
        // };

        VulkanCore_->AddCreateDeviceCallback("InitializeResourcePool", InitializeResourcePool);
        // VulkanCore_->AddCreateDeviceCallback("InitializeFormatProperties", InitializeFormatProperties);
    }

    FVulkanContext::~FVulkanContext()
    {
        WaitIdle();
        RemoveRegisteredCallbacks();
    }

    void FVulkanContext::RegisterAutoRemovedCallbacks(ECallbackType Type, const std::string& Name, const std::function<void()>& Callback)
    {
        switch (Type)
        {
        case ECallbackType::kCreateSwapchain:
            AddCreateSwapchainCallback(Name, Callback);
            break;
        case ECallbackType::kDestroySwapchain:
            AddDestroySwapchainCallback(Name, Callback);
            break;
        case ECallbackType::kCreateDevice:
            AddCreateDeviceCallback(Name, Callback);
            break;
        case ECallbackType::kDestroyDevice:
            AddDestroyDeviceCallback(Name, Callback);
            break;
        default:
            break;
        }

        AutoRemovedCallbacks_.emplace_back(Type, Name);
    }

    void FVulkanContext::RemoveRegisteredCallbacks()
    {
        for (const auto& [Type, Name] : AutoRemovedCallbacks_)
        {
            switch (Type)
            {
            case ECallbackType::kCreateSwapchain:
                RemoveCreateSwapchainCallback(Name);
                break;
            case ECallbackType::kDestroySwapchain:
                RemoveDestroySwapchainCallback(Name);
                break;
            case ECallbackType::kCreateDevice:
                RemoveCreateDeviceCallback(Name);
                break;
            case ECallbackType::kDestroyDevice:
                RemoveDestroyDeviceCallback(Name);
                break;
            default:
                break;
            }
        }
    }

    vk::Result FVulkanContext::ExecuteCommands(EQueueType QueueType, vk::CommandBuffer CommandBuffer) const
    {
        FVulkanFence Fence(VulkanCore_->GetDevice());
        auto Queue = VulkanCore_->GetQueuePool().AcquireQueue(VulkanCore_->GetQueueFamilyProperties(QueueType).queueFlags);
        vk::SubmitInfo SubmitInfo({}, {}, CommandBuffer, {});
        try
        {
            Queue->submit(SubmitInfo, *Fence);
            Fence.Wait();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to execute command: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanContext::SubmitCommandBuffer(EQueueType QueueType, const vk::SubmitInfo& SubmitInfo, vk::Fence Fence) const
    {
        try
        {
            VulkanCore_->GetQueue(QueueType).submit(SubmitInfo, Fence);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to submit command buffer to queue: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanContext::SubmitCommandBuffer(EQueueType QueueType, vk::CommandBuffer Buffer, vk::Fence Fence) const
    {
        vk::SubmitInfo SubmitInfo({}, {}, Buffer, {});
        return SubmitCommandBuffer(QueueType, SubmitInfo, Fence);
    }

    vk::Result FVulkanContext::SubmitCommandBuffer(EQueueType QueueType, vk::CommandBuffer Buffer, vk::Semaphore WaitSemaphore,
                                                   vk::Semaphore SignalSemaphore, vk::Fence Fence, vk::PipelineStageFlags Flags) const
    {
        vk::SubmitInfo SubmitInfo = CreateSubmitInfo(Buffer, WaitSemaphore, SignalSemaphore, Flags);
        return SubmitCommandBuffer(QueueType, SubmitInfo, Fence);
    }

    vk::SampleCountFlagBits FVulkanContext::GetMaxUsableSampleCount() const
    {
        vk::SampleCountFlags Counts = VulkanCore_->GetPhysicalDeviceProperties().limits.framebufferColorSampleCounts &
            VulkanCore_->GetPhysicalDeviceProperties().limits.framebufferDepthSampleCounts;

        if (Counts & vk::SampleCountFlagBits::e64) return vk::SampleCountFlagBits::e64;
        if (Counts & vk::SampleCountFlagBits::e32) return vk::SampleCountFlagBits::e32;
        if (Counts & vk::SampleCountFlagBits::e16) return vk::SampleCountFlagBits::e16;
        if (Counts & vk::SampleCountFlagBits::e8)  return vk::SampleCountFlagBits::e8;
        if (Counts & vk::SampleCountFlagBits::e4)  return vk::SampleCountFlagBits::e4;
        if (Counts & vk::SampleCountFlagBits::e2)  return vk::SampleCountFlagBits::e2;

        return vk::SampleCountFlagBits::e1;
    }
} // namespace Npgs
