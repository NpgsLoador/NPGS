#include <algorithm>
#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    NPGS_INLINE void FVulkanContext::AddCreateDeviceCallback(std::string_view Name, const std::function<void()>& Callback)
    {
        VulkanCore_->AddCreateDeviceCallback(Name, Callback);
    }

    NPGS_INLINE void FVulkanContext::AddDestroyDeviceCallback(std::string_view Name, const std::function<void()>& Callback)
    {
        VulkanCore_->AddDestroyDeviceCallback(Name, Callback);
    }

    NPGS_INLINE void FVulkanContext::AddCreateSwapchainCallback(std::string_view Name, const std::function<void()>& Callback)
    {
        VulkanCore_->AddCreateSwapchainCallback(Name, Callback);
    }

    NPGS_INLINE void FVulkanContext::AddDestroySwapchainCallback(std::string_view Name, const std::function<void()>& Callback)
    {
        VulkanCore_->AddDestroySwapchainCallback(Name, Callback);
    }

    NPGS_INLINE void FVulkanContext::RemoveCreateDeviceCallback(std::string_view Name)
    {
        VulkanCore_->RemoveCreateDeviceCallback(Name);
    }

    NPGS_INLINE void FVulkanContext::RemoveDestroyDeviceCallback(std::string_view Name)
    {
        VulkanCore_->RemoveDestroyDeviceCallback(Name);
    }

    NPGS_INLINE void FVulkanContext::RemoveCreateSwapchainCallback(std::string_view Name)
    {
        VulkanCore_->RemoveCreateSwapchainCallback(Name);
    }

    NPGS_INLINE void FVulkanContext::RemoveDestroySwapchainCallback(std::string_view Name)
    {
        VulkanCore_->RemoveDestroySwapchainCallback(Name);
    }

    NPGS_INLINE void FVulkanContext::AddInstanceLayer(const char* Layer)
    {
        VulkanCore_->AddInstanceLayer(Layer);
    }

    NPGS_INLINE void FVulkanContext::SetInstanceLayers(const std::vector<const char*>& Layers)
    {
        VulkanCore_->SetInstanceLayers(Layers);
    }

    NPGS_INLINE void FVulkanContext::AddInstanceExtension(const char* Extension)
    {
        VulkanCore_->AddInstanceExtension(Extension);
    }

    NPGS_INLINE void FVulkanContext::SetInstanceExtensions(const std::vector<const char*>& Extensions)
    {
        VulkanCore_->SetInstanceExtensions(Extensions);
    }

    NPGS_INLINE void FVulkanContext::AddDeviceExtension(const char* Extension)
    {
        VulkanCore_->AddDeviceExtension(Extension);
    }

    NPGS_INLINE void FVulkanContext::SetDeviceExtensions(const std::vector<const char*>& Extensions)
    {
        VulkanCore_->SetDeviceExtensions(Extensions);
    }

    NPGS_INLINE vk::Result FVulkanContext::CreateInstance(vk::InstanceCreateFlags Flags)
    {
        return VulkanCore_->CreateInstance(Flags);
    }

    NPGS_INLINE vk::Result FVulkanContext::CreateDevice(std::uint32_t PhysicalDeviceIndex, vk::DeviceCreateFlags Flags)
    {
        return VulkanCore_->CreateDevice(PhysicalDeviceIndex, Flags);
    }

    NPGS_INLINE vk::Result FVulkanContext::RecreateDevice(std::uint32_t PhysicalDeviceIndex, vk::DeviceCreateFlags Flags)
    {
        return VulkanCore_->RecreateDevice(PhysicalDeviceIndex, Flags);
    }

    NPGS_INLINE void FVulkanContext::SetSurface(vk::SurfaceKHR Surface)
    {
        VulkanCore_->SetSurface(Surface);
    }

    NPGS_INLINE vk::Result FVulkanContext::SetSurfaceFormat(const vk::SurfaceFormatKHR& SurfaceFormat)
    {
        return VulkanCore_->SetSurfaceFormat(SurfaceFormat);
    }

    NPGS_INLINE void FVulkanContext::SetHdrMetadata(const vk::HdrMetadataEXT& HdrMetadata)
    {
        VulkanCore_->SetHdrMetadata(HdrMetadata);
    }

    NPGS_INLINE vk::Result FVulkanContext::CreateSwapchain(vk::Extent2D Extent, bool bLimitFps, bool bEnableHdr, vk::SwapchainCreateFlagsKHR Flags)
    {
        return VulkanCore_->CreateSwapchain(Extent, bLimitFps, bEnableHdr, Flags);
    }

    NPGS_INLINE vk::Result FVulkanContext::RecreateSwapchain()
    {
        return VulkanCore_->RecreateSwapchain();
    }

    NPGS_INLINE vk::Result FVulkanContext::ExecuteCommands(EQueueType QueueType, const FVulkanCommandBuffer& CommandBuffer) const
    {
        return ExecuteCommands(QueueType, *CommandBuffer, CommandBuffer.GetHandleName() + "_TemporaryFence");
    }

    NPGS_INLINE vk::Result
    FVulkanContext::SubmitCommandBuffer(EQueueType QueueType, const vk::SubmitInfo2& SubmitInfo,
                                        const FVulkanFence* Fence, bool bUseFixedQueue) const
    {
        return SubmitCommandBuffer(QueueType, SubmitInfo, Fence ? **Fence : vk::Fence(), bUseFixedQueue);
    }

    NPGS_INLINE vk::Result
    FVulkanContext::SubmitCommandBuffer(EQueueType QueueType, const FVulkanCommandBuffer& CommandBuffer,
                                        const FVulkanFence* Fence, bool bUseFixedQueue) const
    {
        return SubmitCommandBuffer(QueueType, *CommandBuffer, Fence ? **Fence : vk::Fence(), bUseFixedQueue);
    }

    NPGS_INLINE vk::Result
    FVulkanContext::SubmitCommandBuffer(EQueueType QueueType, const FVulkanCommandBuffer& CommandBuffer,
                                        const FVulkanSemaphore* WaitSemaphore, vk::PipelineStageFlags2 WaitStageMask,
                                        const FVulkanSemaphore* SignalSemaphore, vk::PipelineStageFlags2 SignalStageMask,
                                        const FVulkanFence* Fence, bool bUseFixedQueue) const
    {
        return SubmitCommandBuffer(QueueType, *CommandBuffer,
                                   WaitSemaphore   ? **WaitSemaphore   : vk::Semaphore(), WaitStageMask,
                                   SignalSemaphore ? **SignalSemaphore : vk::Semaphore(), SignalStageMask,
                                   Fence           ? **Fence           : vk::Fence(),     bUseFixedQueue);
    }

    NPGS_INLINE vk::Result FVulkanContext::SwapImage(vk::Semaphore Semaphore)
    {
        return VulkanCore_->SwapImage(Semaphore);
    }

    NPGS_INLINE vk::Result FVulkanContext::SwapImage(const FVulkanSemaphore& Semaphore)
    {
        return SwapImage(*Semaphore);
    }

    NPGS_INLINE vk::Result FVulkanContext::PresentImage(const vk::PresentInfoKHR& PresentInfo)
    {
        return VulkanCore_->PresentImage(PresentInfo);
    }

    NPGS_INLINE vk::Result FVulkanContext::PresentImage(vk::Semaphore Semaphore)
    {
        return VulkanCore_->PresentImage(Semaphore);
    }

    NPGS_INLINE vk::Result FVulkanContext::PresentImage(const FVulkanSemaphore& Semaphore)
    {
        return PresentImage(*Semaphore);
    }

    NPGS_INLINE vk::Result FVulkanContext::WaitIdle() const
    {
        return VulkanCore_->WaitIdle();
    }

    template <typename... Types>
    inline bool FVulkanContext::CheckDeviceExtensionsSupported(Types&&... Extensions)
    {
        std::array  ExtensionArray{ Extensions... };
        const auto& EnabledDeviceExtensions = VulkanCore_->GetDeviceExtensions();
        for (const auto& Extension : ExtensionArray)
        {
            if (std::ranges::find(EnabledDeviceExtensions, Extension) == EnabledDeviceExtensions.end())
            {
                return false;
            }
        }

        return true;
    }

    NPGS_INLINE vk::Instance FVulkanContext::GetInstance() const
    {
        return VulkanCore_->GetInstance();
    }

    NPGS_INLINE vk::SurfaceKHR FVulkanContext::GetSurface() const
    {
        return VulkanCore_->GetSurface();
    }

    NPGS_INLINE vk::PhysicalDevice FVulkanContext::GetPhysicalDevice() const
    {
        return VulkanCore_->GetPhysicalDevice();
    }

    NPGS_INLINE vk::Device FVulkanContext::GetDevice() const
    {
        return VulkanCore_->GetDevice();
    }

    NPGS_INLINE vk::SwapchainKHR FVulkanContext::GetSwapchain() const
    {
        return VulkanCore_->GetSwapchain();
    }

    NPGS_INLINE VmaAllocator FVulkanContext::GetVmaAllocator() const
    {
        return VulkanCore_->GetVmaAllocator();
    }

    NPGS_INLINE const vk::PhysicalDeviceProperties& FVulkanContext::GetPhysicalDeviceProperties() const
    {
        return VulkanCore_->GetPhysicalDeviceProperties();
    }

    NPGS_INLINE const vk::PhysicalDeviceMemoryProperties& FVulkanContext::GetPhysicalDeviceMemoryProperties() const
    {
        return VulkanCore_->GetPhysicalDeviceMemoryProperties();
    }

    NPGS_INLINE const vk::SwapchainCreateInfoKHR& FVulkanContext::GetSwapchainCreateInfo() const
    {
        return VulkanCore_->GetSwapchainCreateInfo();
    }

    NPGS_INLINE std::uint32_t FVulkanContext::GetAvailablePhysicalDeviceCount() const
    {
        return VulkanCore_->GetAvailablePhysicalDeviceCount();
    }

    NPGS_INLINE std::uint32_t FVulkanContext::GetAvailableSurfaceFormatCount() const
    {
        return VulkanCore_->GetAvailableSurfaceFormatCount();
    }

    NPGS_INLINE std::uint32_t FVulkanContext::GetSwapchainImageCount() const
    {
        return VulkanCore_->GetSwapchainImageCount();
    }

    NPGS_INLINE std::uint32_t FVulkanContext::GetSwapchainImageViewCount() const
    {
        return VulkanCore_->GetSwapchainImageViewCount();
    }

    NPGS_INLINE vk::PhysicalDevice FVulkanContext::GetAvailablePhysicalDevice(std::uint32_t Index) const
    {
        return VulkanCore_->GetAvailablePhysicalDevice(Index);
    }

    NPGS_INLINE vk::Format FVulkanContext::GetAvailableSurfaceFormat(std::uint32_t Index) const
    {
        return VulkanCore_->GetAvailableSurfaceFormat(Index);
    }

    NPGS_INLINE vk::ColorSpaceKHR FVulkanContext::GetAvailableSurfaceColorSpace(std::uint32_t Index) const
    {
        return VulkanCore_->GetAvailableSurfaceColorSpace(Index);
    }

    NPGS_INLINE vk::Image FVulkanContext::GetSwapchainImage(std::uint32_t Index) const
    {
        return VulkanCore_->GetSwapchainImage(Index);
    }

    NPGS_INLINE vk::ImageView FVulkanContext::GetSwapchainImageView(std::uint32_t Index) const
    {
        return VulkanCore_->GetSwapchainImageView(Index);
    }

    NPGS_INLINE std::uint32_t FVulkanContext::GetQueueFamilyIndex(EQueueType QueueType) const
    {
        return VulkanCore_->GetQueueFamilyIndex(QueueType);
    }

    NPGS_INLINE std::uint32_t FVulkanContext::GetCurrentImageIndex() const
    {
        return VulkanCore_->GetCurrentImageIndex();
    }

    NPGS_INLINE FCommandPoolPool::FPoolGuard
    FVulkanContext::AcquireCommandPool(EQueueType QueueType, vk::CommandPoolCreateFlags Flags)
    {
        return CommandPoolPools_.at(std::to_underlying(QueueType))->AcquirePool(Flags);
    }

    NPGS_INLINE FStagingBufferPool::FBufferGuard
    FVulkanContext::AcquireStagingBuffer(std::size_t Size, FStagingBufferPool::EPoolUsage Usage)
    {
        return StagingBufferPools_[std::to_underlying(Usage)]->AcquireBuffer(Size);
    }

    // NPGS_INLINE const vk::FormatProperties& FVulkanContext::GetFormatProperties(vk::Format Format) const
    // {
    //     auto Index = magic_enum::enum_index(Format);
    //     if (!Index.has_value())
    //     {
    //         return FormatProperties_[0]; // vk::Format::eUndefined
    //     }
    // 
    //     return FormatProperties_[Index.value()];
    // }

    NPGS_INLINE std::uint32_t FVulkanContext::GetApiVersion() const
    {
        return VulkanCore_->GetApiVersion();
    }
} // namespace Npgs
