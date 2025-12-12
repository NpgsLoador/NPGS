#pragma once

#include <cstdint>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

// #include <magic_enum/magic_enum.hpp>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Runtime/Graphics/Vulkan/Core.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"
#include "Engine/Runtime/Pools/CommandPoolPool.hpp"
#include "Engine/Runtime/Pools/StagingBufferPool.hpp"

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

        using EQueueType = FVulkanCore::EQueueType;

    public:
        FVulkanContext();
        FVulkanContext(const FVulkanContext&) = delete;
        FVulkanContext(FVulkanContext&&)      = delete;
        ~FVulkanContext();

        FVulkanContext& operator=(const FVulkanContext&) = delete;
        FVulkanContext& operator=(FVulkanContext&&)      = delete;

        void AddCreateDeviceCallback(std::string_view Name, const std::function<void()>& Callback);
        void AddDestroyDeviceCallback(std::string_view Name, const std::function<void()>& Callback);
        void AddCreateSwapchainCallback(std::string_view Name, const std::function<void()>& Callback);
        void AddDestroySwapchainCallback(std::string_view Name, const std::function<void()>& Callback);
        void RemoveCreateDeviceCallback(std::string_view Name);
        void RemoveDestroyDeviceCallback(std::string_view Name);
        void RemoveCreateSwapchainCallback(std::string_view Name);
        void RemoveDestroySwapchainCallback(std::string_view Name);
        void RegisterRuntimeOnlyCallbacks(ECallbackType Type, std::string_view Name, const std::function<void()>& Callback);

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

        vk::Result ExecuteCommands(EQueueType QueueType, vk::CommandBuffer CommandBuffer, std::string_view FenceName = "") const;
        vk::Result ExecuteCommands(EQueueType QueueType, const FVulkanCommandBuffer& CommandBuffer) const;

        vk::Result SubmitCommandBuffer(EQueueType QueueType, const vk::SubmitInfo2& SubmitInfo, vk::Fence Fence, bool bUseFixedQueue) const;
        vk::Result SubmitCommandBuffer(EQueueType QueueType, const vk::SubmitInfo2& SubmitInfo, const FVulkanFence* Fence, bool bUseFixedQueue) const;
        vk::Result SubmitCommandBuffer(EQueueType QueueType, vk::CommandBuffer CommandBuffer, vk::Fence Fence, bool bUseFixedQueue) const;
        vk::Result SubmitCommandBuffer(EQueueType QueueType, const FVulkanCommandBuffer& CommandBuffer, const FVulkanFence* Fence, bool bUseFixedQueue) const;

        vk::Result SubmitCommandBuffer(EQueueType QueueType, vk::CommandBuffer CommandBuffer,
                                       vk::Semaphore WaitSemaphore, vk::PipelineStageFlags2 WaitStageMask,
                                       vk::Semaphore SignalSemaphore, vk::PipelineStageFlags2 SignalStageMask,
                                       vk::Fence Fence, bool bUseFixedQueue) const;

        vk::Result SubmitCommandBuffer(EQueueType QueueType, const FVulkanCommandBuffer& CommandBuffer,
                                       const FVulkanSemaphore* WaitSemaphore, vk::PipelineStageFlags2 WaitStageMask,
                                       const FVulkanSemaphore* SignalSemaphore, vk::PipelineStageFlags2 SignalStageMask,
                                       const FVulkanFence* Fence, bool bUseFixedQueue) const;

        vk::Result SwapImage(vk::Semaphore Semaphore);
        vk::Result SwapImage(const FVulkanSemaphore& Semaphore);
        vk::Result PresentImage(const vk::PresentInfoKHR& PresentInfo);
        vk::Result PresentImage(vk::Semaphore Semaphore);
        vk::Result PresentImage(const FVulkanSemaphore& Semaphore);
        vk::Result WaitIdle() const;

        template <typename... Types>
        bool CheckDeviceExtensionsSupported(Types&&... Extensions);

        vk::Instance GetInstance() const;
        vk::SurfaceKHR GetSurface() const;
        vk::PhysicalDevice GetPhysicalDevice() const;
        vk::Device GetDevice() const;
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

        std::uint32_t GetQueueFamilyIndex(EQueueType QueueType) const;
        std::uint32_t GetCurrentImageIndex() const;

        FCommandPoolPool::FPoolGuard AcquireCommandPool(
            EQueueType QueueType, vk::CommandPoolCreateFlags Flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

        FStagingBufferPool::FBufferGuard AcquireStagingBuffer(
            std::size_t Size, FStagingBufferPool::EPoolUsage Usage = FStagingBufferPool::EPoolUsage::kSubmit);

        // const vk::FormatProperties& GetFormatProperties(vk::Format Format) const;

        std::uint32_t GetApiVersion() const;

    private:
        void RemoveRuntimeOnlyCallbacks();

    private:
        FVulkanCore*                                                         VulkanCore_;
        std::unordered_map<std::uint32_t, std::unique_ptr<FCommandPoolPool>> CommandPoolPools_;
        std::array<FStagingBufferPool*, 2>                                   StagingBufferPools_;
        std::vector<std::pair<ECallbackType, std::string>>                   RuntimeOnlyCallbacks_;
        // std::array<vk::FormatProperties, magic_enum::enum_count<vk::Format>()> FormatProperties_;
    };
} // namespace Npgs

#include "Context.inl"
