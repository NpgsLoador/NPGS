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
            if (_VulkanCore->GetGraphicsQueueFamilyIndex() != vk::QueueFamilyIgnored)
            {
                _GraphicsCommandBufferPool = std::make_shared<FCommandBufferPool>(
                    8, 32, 5000, 60000, _VulkanCore->GetDevice(), _VulkanCore->GetGraphicsQueueFamilyIndex());
            }
            if (_VulkanCore->GetComputeQueueFamilyIndex() != vk::QueueFamilyIgnored)
            {
                if (_VulkanCore->GetComputeQueueFamilyIndex() != _VulkanCore->GetGraphicsQueueFamilyIndex())
                {
                    _ComputeCommandBufferPool = std::make_shared<FCommandBufferPool>(
                        4, 16, 5000, 60000, _VulkanCore->GetDevice(), _VulkanCore->GetComputeQueueFamilyIndex());
                }
                else
                {
                    _ComputeCommandBufferPool = _GraphicsCommandBufferPool;
                }
            }
            if (_VulkanCore->GetPresentQueueFamilyIndex() != vk::QueueFamilyIgnored &&
                _VulkanCore->GetPresentQueueFamilyIndex() != _VulkanCore->GetGraphicsQueueFamilyIndex() &&
                _VulkanCore->GetSwapchainCreateInfo().imageSharingMode == vk::SharingMode::eExclusive)
            {
                if (_VulkanCore->GetPresentQueueFamilyIndex() != _VulkanCore->GetGraphicsQueueFamilyIndex())
                {
                    _PresentCommandBufferPool = std::make_shared<FCommandBufferPool>(
                        2, 8, 5000, 60000, _VulkanCore->GetDevice(), _VulkanCore->GetPresentQueueFamilyIndex());
                }
                else
                {
                    _PresentCommandBufferPool = _GraphicsCommandBufferPool;
                }
            }
            if (_VulkanCore->GetTransferQueueFamilyIndex() != vk::QueueFamilyIgnored)
            {
                if (_VulkanCore->GetTransferQueueFamilyIndex() != _VulkanCore->GetGraphicsQueueFamilyIndex())
                {
                    _TransferCommandBufferPool = std::make_shared<FCommandBufferPool>(
                        2, 8, 5000, 60000, _VulkanCore->GetDevice(), _VulkanCore->GetTransferQueueFamilyIndex());
                }
                else
                {
                    _TransferCommandBufferPool = _GraphicsCommandBufferPool;
                }
            }
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

    vk::Result FVulkanContext::ExecuteGraphicsCommands(vk::CommandBuffer CommandBuffer) const
    {
        FVulkanFence Fence(_VulkanCore->GetDevice());
        auto Queue = _VulkanCore->GetQueuePool().AcquireQueue(_VulkanCore->GetGraphicsQueueFamilyProperties().queueFlags);
        vk::SubmitInfo SubmitInfo({}, {}, CommandBuffer, {});
        try
        {
            Queue->submit(SubmitInfo, *Fence);
            Fence.Wait();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to execute graphics command: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanContext::ExecutePresentCommands(vk::CommandBuffer CommandBuffer) const
    {
        FVulkanFence Fence(_VulkanCore->GetDevice());
        auto Queue = _VulkanCore->GetQueuePool().AcquireQueue(_VulkanCore->GetPresentQueueFamilyProperties().queueFlags);
        vk::SubmitInfo SubmitInfo({}, {}, CommandBuffer, {});
        try
        {
            Queue->submit(SubmitInfo, *Fence);
            Fence.Wait();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to execute present command: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanContext::ExecuteComputeCommands(vk::CommandBuffer CommandBuffer) const
    {
        FVulkanFence Fence(_VulkanCore->GetDevice());
        auto Queue = _VulkanCore->GetQueuePool().AcquireQueue(_VulkanCore->GetComputeQueueFamilyProperties().queueFlags);
        vk::SubmitInfo SubmitInfo({}, {}, CommandBuffer, {});
        try
        {
            Queue->submit(SubmitInfo, *Fence);
            Fence.Wait();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to execute compute command: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanContext::ExecuteTransferCommands(vk::CommandBuffer CommandBuffer) const
    {
        FVulkanFence Fence(_VulkanCore->GetDevice());
        auto Queue = _VulkanCore->GetQueuePool().AcquireQueue(_VulkanCore->GetTransferQueueFamilyProperties().queueFlags);
        vk::SubmitInfo SubmitInfo({}, {}, CommandBuffer, {});
        try
        {
            Queue->submit(SubmitInfo, *Fence);
            Fence.Wait();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to execute transfer command: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanContext::SubmitCommandBufferToGraphics(const vk::SubmitInfo& SubmitInfo, vk::Fence Fence) const
    {
        try
        {
            _VulkanCore->GetGraphicsQueue().submit(SubmitInfo, Fence);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to submit command buffer to graphics queue: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanContext::SubmitCommandBufferToGraphics(vk::CommandBuffer Buffer, vk::Fence Fence) const
    {
        vk::SubmitInfo SubmitInfo({}, {}, Buffer, {});
        return SubmitCommandBufferToGraphics(SubmitInfo, Fence);
    }

    vk::Result FVulkanContext::SubmitCommandBufferToGraphics(vk::CommandBuffer Buffer, vk::Semaphore WaitSemaphore,
                                                             vk::Semaphore SignalSemaphore, vk::Fence Fence, vk::PipelineStageFlags Flags) const
    {
        vk::SubmitInfo SubmitInfo = CreateSubmitInfo(Buffer, WaitSemaphore, SignalSemaphore, Flags);
        return SubmitCommandBufferToGraphics(SubmitInfo, Fence);
    }

    vk::Result FVulkanContext::SubmitCommandBufferToPresent(const vk::SubmitInfo& SubmitInfo, vk::Fence Fence) const
    {
        try
        {
            _VulkanCore->GetPresentQueue().submit(SubmitInfo, Fence);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to submit command buffer to present queue: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanContext::SubmitCommandBufferToPresent(vk::CommandBuffer Buffer, vk::Fence Fence) const
    {
        vk::SubmitInfo SubmitInfo({}, {}, Buffer, {});
        return SubmitCommandBufferToPresent(SubmitInfo, Fence);
    }

    vk::Result FVulkanContext::SubmitCommandBufferToPresent(vk::CommandBuffer Buffer, vk::Semaphore WaitSemaphore,
                                                            vk::Semaphore SignalSemaphore, vk::Fence Fence) const
    {
        vk::SubmitInfo SubmitInfo = CreateSubmitInfo(Buffer, WaitSemaphore, SignalSemaphore, vk::PipelineStageFlagBits::eAllCommands);
        return SubmitCommandBufferToPresent(SubmitInfo, Fence);
    }

    vk::Result FVulkanContext::SubmitCommandBufferToCompute(const vk::SubmitInfo& SubmitInfo, vk::Fence Fence) const
    {
        try
        {
            _VulkanCore->GetComputeQueue().submit(SubmitInfo, Fence);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to submit command buffer to compute queue: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanContext::SubmitCommandBufferToCompute(vk::CommandBuffer Buffer, vk::Fence Fence) const
    {
        vk::SubmitInfo SubmitInfo({}, {}, Buffer, {});
        return SubmitCommandBufferToCompute(SubmitInfo, Fence);
    }

    vk::Result FVulkanContext::SubmitCommandBufferToCompute(vk::CommandBuffer Buffer, vk::Semaphore WaitSemaphore,
                                                            vk::Semaphore SignalSemaphore, vk::Fence Fence, vk::PipelineStageFlags Flags) const
    {
        vk::SubmitInfo SubmitInfo = CreateSubmitInfo(Buffer, WaitSemaphore, SignalSemaphore, Flags);
        return SubmitCommandBufferToCompute(SubmitInfo, Fence);
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
            _VulkanCore->GetGraphicsQueueFamilyIndex(),
            _VulkanCore->GetPresentQueueFamilyIndex(),
            _VulkanCore->GetSwapchainImage(_VulkanCore->GetCurrentImageIndex()),
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
        );

        vk::DependencyInfo DependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(ImageMemoryBarrier);

        PresentCommandBuffer.pipelineBarrier2(DependencyInfo);
    }
} // namespace Npgs
