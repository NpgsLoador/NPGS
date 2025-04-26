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
        : _VulkanCore(std::make_unique<FVulkanCore>())
    {
        auto InitializeCommandPool = [this]() -> void
        {
            if (_VulkanCore->GetQueueFamilyIndex(EQueueType::kGraphics) != vk::QueueFamilyIgnored)
            {
                _CommandBufferPools[_VulkanCore->GetQueueFamilyIndex(EQueueType::kGraphics)] = std::make_shared<FCommandBufferPool>(
                    8, 32, 5000, 60000, _VulkanCore->GetDevice(), _VulkanCore->GetQueueFamilyIndex(EQueueType::kGraphics));
            }
            if (_VulkanCore->GetQueueFamilyIndex(EQueueType::kCompute) != vk::QueueFamilyIgnored)
            {
                if (_VulkanCore->GetQueueFamilyIndex(EQueueType::kCompute) != _VulkanCore->GetQueueFamilyIndex(EQueueType::kGraphics))
                {
                    _CommandBufferPools[_VulkanCore->GetQueueFamilyIndex(EQueueType::kCompute)] = std::make_shared<FCommandBufferPool>(
                        4, 16, 5000, 60000, _VulkanCore->GetDevice(), _VulkanCore->GetQueueFamilyIndex(EQueueType::kCompute));
                }
                else
                {
                    _CommandBufferPools[_VulkanCore->GetQueueFamilyIndex(EQueueType::kCompute)] =
                        _CommandBufferPools.at(_VulkanCore->GetQueueFamilyIndex(EQueueType::kGraphics));
                }
            }
            if (_VulkanCore->GetQueueFamilyIndex(EQueueType::kPresent) != vk::QueueFamilyIgnored &&
                _VulkanCore->GetQueueFamilyIndex(EQueueType::kPresent) != _VulkanCore->GetQueueFamilyIndex(EQueueType::kGraphics) &&
                _VulkanCore->GetSwapchainCreateInfo().imageSharingMode == vk::SharingMode::eExclusive)
            {
                if (_VulkanCore->GetQueueFamilyIndex(EQueueType::kPresent) != _VulkanCore->GetQueueFamilyIndex(EQueueType::kGraphics))
                {
                    _CommandBufferPools[_VulkanCore->GetQueueFamilyIndex(EQueueType::kPresent)] = std::make_shared<FCommandBufferPool>(
                        2, 8, 5000, 60000, _VulkanCore->GetDevice(), _VulkanCore->GetQueueFamilyIndex(EQueueType::kPresent));
                }
                else
                {
                    _CommandBufferPools[_VulkanCore->GetQueueFamilyIndex(EQueueType::kPresent)] =
                        _CommandBufferPools.at(_VulkanCore->GetQueueFamilyIndex(EQueueType::kGraphics));
                }
            }
            if (_VulkanCore->GetQueueFamilyIndex(EQueueType::kTransfer) != vk::QueueFamilyIgnored)
            {
                if (_VulkanCore->GetQueueFamilyIndex(EQueueType::kTransfer) != _VulkanCore->GetQueueFamilyIndex(EQueueType::kGraphics))
                {
                    _CommandBufferPools[_VulkanCore->GetQueueFamilyIndex(EQueueType::kTransfer)] = std::make_shared<FCommandBufferPool>(
                        2, 8, 5000, 60000, _VulkanCore->GetDevice(), _VulkanCore->GetQueueFamilyIndex(EQueueType::kTransfer));
                }
                else
                {
                    _CommandBufferPools[_VulkanCore->GetQueueFamilyIndex(EQueueType::kTransfer)] =
                        _CommandBufferPools.at(_VulkanCore->GetQueueFamilyIndex(EQueueType::kGraphics));
                }
            }

            _StagingBufferPools[std::to_underlying(FStagingBufferPool::EPoolUsage::kSubmit)] = std::make_unique<FStagingBufferPool>(
                _VulkanCore->GetPhysicalDevice(), _VulkanCore->GetDevice(), _VulkanCore->GetVmaAllocator(),
                4, 64, 1000, 5000, FStagingBufferPool::EPoolUsage::kSubmit, true);

            _StagingBufferPools[std::to_underlying(FStagingBufferPool::EPoolUsage::kFetch)] = std::make_unique<FStagingBufferPool>(
                _VulkanCore->GetPhysicalDevice(), _VulkanCore->GetDevice(), _VulkanCore->GetVmaAllocator(),
                2, 8, 10000, 60000, FStagingBufferPool::EPoolUsage::kFetch, true);
        };

        // auto InitializeFormatProperties = [this]() -> void
        // {
        //     std::size_t Index = 0;
        //     for (vk::Format Format : magic_enum::enum_values<vk::Format>())
        //     {
        //         _FormatProperties[Index++] = _VulkanCore->GetPhysicalDevice().getFormatProperties(Format);
        //     }
        // };

        _VulkanCore->AddCreateDeviceCallback("InitializeCommandPool", InitializeCommandPool);
        // _VulkanCore->AddCreateDeviceCallback("InitializeFormatProperties", InitializeFormatProperties);
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

        _AutoRemovedCallbacks.emplace_back(Type, Name);
    }

    void FVulkanContext::RemoveRegisteredCallbacks()
    {
        for (const auto& [Type, Name] : _AutoRemovedCallbacks)
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
        FVulkanFence Fence(_VulkanCore->GetDevice());
        auto Queue = _VulkanCore->GetQueuePool().AcquireQueue(_VulkanCore->GetQueueFamilyProperties(QueueType).queueFlags);
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
            _VulkanCore->GetQueue(QueueType).submit(SubmitInfo, Fence);
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

    vk::Result FVulkanContext::TransferImageOwnershipToPresent(vk::CommandBuffer PresentCommandBuffer) const
    {
        // vk::CommandBufferBeginInfo CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        // try
        // {
        //     PresentCommandBuffer.begin(CommandBufferBeginInfo);
        // }
        // catch (const vk::SystemError& e)
        // {
        //     NpgsCoreError("Failed to begin present command buffer: {}", e.what());
        //     return static_cast<vk::Result>(e.code().value());
        // }

        TransferImageOwnershipToPresentImpl(PresentCommandBuffer);
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanContext::TransferImageOwnershipToPresent(const FVulkanCommandBuffer& PresentCommandBuffer) const
    {
        // VulkanHppCheck(PresentCommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        TransferImageOwnershipToPresentImpl(*PresentCommandBuffer);
        return vk::Result::eSuccess;
    }

    void FVulkanContext::TransferImageOwnershipToPresentImpl(vk::CommandBuffer PresentCommandBuffer) const
    {
        vk::ImageMemoryBarrier2 ImageMemoryBarrier(
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eBottomOfPipe,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            _VulkanCore->GetQueueFamilyIndex(EQueueType::kGraphics),
            _VulkanCore->GetQueueFamilyIndex(EQueueType::kPresent),
            _VulkanCore->GetSwapchainImage(_VulkanCore->GetCurrentImageIndex()),
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
        );

        vk::DependencyInfo DependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(ImageMemoryBarrier);

        PresentCommandBuffer.pipelineBarrier2(DependencyInfo);
    }
} // namespace Npgs
