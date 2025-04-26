#pragma once

// #include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

// #include <magic_enum/magic_enum.hpp>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Resources/Pools/CommandBufferPool.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Core.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

namespace Npgs
{
    class FVulkanContext
    {
    public:
        enum class ECallbackType : std::uint8_t
        {
            kCreateSwapchain,
            kDestroySwapchain,
            kCreateDevice,
            kDestroyDevice
        };

    public:
        FVulkanContext();
        FVulkanContext(const FVulkanContext&) = delete;
        FVulkanContext(FVulkanContext&&)      = delete;
        ~FVulkanContext();

        FVulkanContext& operator=(const FVulkanContext&) = delete;
        FVulkanContext& operator=(FVulkanContext&&)      = delete;

        void AddCreateDeviceCallback(const std::string& Name, const std::function<void()>& Callback);
        void AddDestroyDeviceCallback(const std::string& Name, const std::function<void()>& Callback);
        void AddCreateSwapchainCallback(const std::string& Name, const std::function<void()>& Callback);
        void AddDestroySwapchainCallback(const std::string& Name, const std::function<void()>& Callback);
        void RemoveCreateDeviceCallback(const std::string& Name);
        void RemoveDestroyDeviceCallback(const std::string& Name);
        void RemoveCreateSwapchainCallback(const std::string& Name);
        void RemoveDestroySwapchainCallback(const std::string& Name);
        void RegisterAutoRemovedCallbacks(ECallbackType Type, const std::string& Name, const std::function<void()>& Callback);
        void RemoveRegisteredCallbacks();

        void AddInstanceLayer(const char* Layer);
        void SetInstanceLayers(const std::vector<const char*>& Layers);
        void AddInstanceExtension(const char* Extension);
        void SetInstanceExtensions(const std::vector<const char*>& Extensions);
        void AddDeviceExtension(const char* Extension);
        void SetDeviceExtensions(const std::vector<const char*>& Extensions);

        vk::Result CreateInstance(vk::InstanceCreateFlags Flags = {});
        vk::Result CreateDevice(std::uint32_t PhysicalDeviceIndex = 0, vk::DeviceCreateFlags Flags = {});
        vk::Result RecreateDevice(std::uint32_t PhysicalDeviceIndex = 0, vk::DeviceCreateFlags Flags = {});
        void       SetSurface(vk::SurfaceKHR Surface);
        vk::Result SetSurfaceFormat(const vk::SurfaceFormatKHR& SurfaceFormat);
        void       SetHdrMetadata(const vk::HdrMetadataEXT& HdrMetadata);
        vk::Result CreateSwapchain(vk::Extent2D Extent, bool bLimitFps, bool bEnableHdr, vk::SwapchainCreateFlagsKHR Flags = {});
        vk::Result RecreateSwapchain();

        vk::Result ExecuteGraphicsCommands(vk::CommandBuffer CommandBuffer) const;
        vk::Result ExecuteGraphicsCommands(const FVulkanCommandBuffer& CommandBuffer) const;
        vk::Result ExecutePresentCommands(vk::CommandBuffer CommandBuffer) const;
        vk::Result ExecutePresentCommands(const FVulkanCommandBuffer& CommandBuffer) const;
        vk::Result ExecuteComputeCommands(vk::CommandBuffer CommandBuffer) const;
        vk::Result ExecuteComputeCommands(const FVulkanCommandBuffer& CommandBuffer) const;
        vk::Result ExecuteTransferCommands(vk::CommandBuffer CommandBuffer) const;
        vk::Result ExecuteTransferCommands(const FVulkanCommandBuffer& CommandBuffer) const;
        vk::Result SubmitCommandBufferToGraphics(const vk::SubmitInfo& SubmitInfo, vk::Fence Fence) const;
        vk::Result SubmitCommandBufferToGraphics(const vk::SubmitInfo& SubmitInfo, const FVulkanFence* Fence) const;
        vk::Result SubmitCommandBufferToGraphics(vk::CommandBuffer Buffer, vk::Fence Fence) const;
        vk::Result SubmitCommandBufferToGraphics(const FVulkanCommandBuffer& Buffer, const FVulkanFence* Fence) const;

        vk::Result SubmitCommandBufferToGraphics(vk::CommandBuffer Buffer, vk::Semaphore WaitSemaphore, vk::Semaphore SignalSemaphore,
                                                 vk::Fence Fence, vk::PipelineStageFlags Flags = vk::PipelineStageFlagBits::eColorAttachmentOutput) const;

        vk::Result SubmitCommandBufferToGraphics(const FVulkanCommandBuffer& Buffer, const FVulkanSemaphore* WaitSemaphore,
                                                 const FVulkanSemaphore* SignalSemaphore, const FVulkanFence* Fence,
                                                 vk::PipelineStageFlags Flags = vk::PipelineStageFlagBits::eColorAttachmentOutput) const;

        vk::Result SubmitCommandBufferToPresent(const vk::SubmitInfo& SubmitInfo, vk::Fence Fence) const;
        vk::Result SubmitCommandBufferToPresent(const vk::SubmitInfo& SubmitInfo, const FVulkanFence* Fence) const;
        vk::Result SubmitCommandBufferToPresent(vk::CommandBuffer Buffer, vk::Fence Fence) const;
        vk::Result SubmitCommandBufferToPresent(const FVulkanCommandBuffer& Buffer, const FVulkanFence* Fence) const;

        vk::Result SubmitCommandBufferToPresent(vk::CommandBuffer Buffer, vk::Semaphore WaitSemaphore,
                                                vk::Semaphore SignalSemaphore, vk::Fence Fence) const;

        vk::Result SubmitCommandBufferToPresent(const FVulkanCommandBuffer& Buffer, const FVulkanSemaphore* WaitSemaphore,
                                                const FVulkanSemaphore* SignalSemaphore, const FVulkanFence* Fence) const;

        vk::Result SubmitCommandBufferToCompute(const vk::SubmitInfo& SubmitInfo, vk::Fence Fence) const;
        vk::Result SubmitCommandBufferToCompute(const vk::SubmitInfo& SubmitInfo, const FVulkanFence* Fence) const;
        vk::Result SubmitCommandBufferToCompute(vk::CommandBuffer Buffer, vk::Fence Fence) const;
        vk::Result SubmitCommandBufferToCompute(const FVulkanCommandBuffer& Buffer, const FVulkanFence* Fence) const;

        vk::Result SubmitCommandBufferToCompute(vk::CommandBuffer Buffer, vk::Semaphore WaitSemaphore, vk::Semaphore SignalSemaphore,
                                                vk::Fence Fence, vk::PipelineStageFlags Flags = vk::PipelineStageFlagBits::eComputeShader) const;

        vk::Result SubmitCommandBufferToCompute(const FVulkanCommandBuffer& Buffer, const FVulkanSemaphore* WaitSemaphore,
                                                const FVulkanSemaphore* SignalSemaphore, const FVulkanFence* Fence,
                                                vk::PipelineStageFlags Flags = vk::PipelineStageFlagBits::eComputeShader) const;

        vk::Result TransferImageOwnershipToPresent(vk::CommandBuffer PresentCommandBuffer) const;
        vk::Result TransferImageOwnershipToPresent(const FVulkanCommandBuffer& PresentCommandBuffer) const;

        vk::Result SwapImage(vk::Semaphore Semaphore);
        vk::Result SwapImage(const FVulkanSemaphore& Semaphore);
        vk::Result PresentImage(const vk::PresentInfoKHR& PresentInfo);
        vk::Result PresentImage(vk::Semaphore Semaphore);
        vk::Result PresentImage(const FVulkanSemaphore& Semaphore);
        vk::Result WaitIdle() const;

        vk::Instance GetInstance() const;
        vk::SurfaceKHR GetSurface() const;
        vk::PhysicalDevice GetPhysicalDevice() const;
        vk::Device GetDevice() const;
        vk::Queue GetGraphicsQueue() const;
        vk::Queue GetPresentQueue() const;
        vk::Queue GetComputeQueue() const;
        vk::SwapchainKHR GetSwapchain() const;
        VmaAllocator GetVmaAllocator() const;

        const vk::PhysicalDeviceProperties& GetPhysicalDeviceProperties() const;
        const vk::PhysicalDeviceMemoryProperties& GetPhysicalDeviceMemoryProperties() const;
        const vk::SwapchainCreateInfoKHR& GetSwapchainCreateInfo() const;

        std::uint32_t GetAvailablePhysicalDeviceCount() const;
        std::uint32_t GetAvailableSurfaceFormatCount() const;
        std::uint32_t GetSwapchainImageCount() const;
        std::uint32_t GetSwapchainImageViewCount() const;
        vk::SampleCountFlagBits GetMaxUsableSampleCount() const;

        vk::PhysicalDevice GetAvailablePhysicalDevice(std::uint32_t Index) const;
        vk::Format GetAvailableSurfaceFormat(std::uint32_t Index) const;
        vk::ColorSpaceKHR GetAvailableSurfaceColorSpace(std::uint32_t Index) const;
        vk::Image GetSwapchainImage(std::uint32_t Index) const;
        vk::ImageView GetSwapchainImageView(std::uint32_t Index) const;

        std::uint32_t GetGraphicsQueueFamilyIndex() const;
        std::uint32_t GetPresentQueueFamilyIndex() const;
        std::uint32_t GetComputeQueueFamilyIndex() const;
        std::uint32_t GetCurrentImageIndex() const;

        FCommandBufferPool::FBufferGuard GetGraphicsCommandBuffer(vk::CommandBufferLevel Level = vk::CommandBufferLevel::ePrimary);
        FCommandBufferPool::FBufferGuard GetComputeCommandBuffer(vk::CommandBufferLevel Level = vk::CommandBufferLevel::ePrimary);
        FCommandBufferPool::FBufferGuard GetPresentCommandBuffer(vk::CommandBufferLevel Level = vk::CommandBufferLevel::ePrimary);
        FCommandBufferPool::FBufferGuard GetTransferCommandBuffer(vk::CommandBufferLevel Level = vk::CommandBufferLevel::ePrimary);

        // const vk::FormatProperties& GetFormatProperties(vk::Format Format) const;

        std::uint32_t GetApiVersion() const;

    private:
        void TransferImageOwnershipToPresentImpl(vk::CommandBuffer PresentCommandBuffer) const;

    private:
        std::unique_ptr<FVulkanCore> _VulkanCore;
        std::shared_ptr<FCommandBufferPool> _GraphicsCommandBufferPool;
        std::shared_ptr<FCommandBufferPool> _PresentCommandBufferPool;
        std::shared_ptr<FCommandBufferPool> _ComputeCommandBufferPool;
        std::shared_ptr<FCommandBufferPool> _TransferCommandBufferPool;

        std::vector<std::pair<ECallbackType, std::string>> _AutoRemovedCallbacks;
        // std::array<vk::FormatProperties, magic_enum::enum_count<vk::Format>()> _FormatProperties;
    };
} // namespace Npgs

#include "Context.inl"
