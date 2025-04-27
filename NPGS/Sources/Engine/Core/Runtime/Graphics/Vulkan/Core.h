#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include "Engine/Core/Runtime/Threads/QueuePool.h"
#include "Engine/Utils/Logger.h"

namespace Npgs
{
    class FVulkanCore
    {
    public:
        enum class EQueueType
        {
            kGraphics,
            kPresent,
            kCompute,
            kTransfer
        };

    private:
        struct FQueueFamilyIndicesComplex
        {
            std::uint32_t GraphicsQueueFamilyIndex{ vk::QueueFamilyIgnored };
            std::uint32_t PresentQueueFamilyIndex{ vk::QueueFamilyIgnored };
            std::uint32_t ComputeQueueFamilyIndex{ vk::QueueFamilyIgnored };
            std::uint32_t TransferQueueFamilyIndex{ vk::QueueFamilyIgnored };
        };

    public:
        FVulkanCore();
        FVulkanCore(const FVulkanCore&) = delete;
        FVulkanCore(FVulkanCore&&)      = delete;
        ~FVulkanCore();

        FVulkanCore& operator=(const FVulkanCore&) = delete;
        FVulkanCore& operator=(FVulkanCore&&)      = delete;

        void AddCreateDeviceCallback(const std::string& Name, const std::function<void()>& Callback);
        void AddDestroyDeviceCallback(const std::string& Name, const std::function<void()>& Callback);
        void AddCreateSwapchainCallback(const std::string& Name, const std::function<void()>& Callback);
        void AddDestroySwapchainCallback(const std::string& Name, const std::function<void()>& Callback);
        void RemoveCreateDeviceCallback(const std::string& Name);
        void RemoveDestroyDeviceCallback(const std::string& Name);
        void RemoveCreateSwapchainCallback(const std::string& Name);
        void RemoveDestroySwapchainCallback(const std::string& Name);

        void AddInstanceLayer(const char* Layer);
        void SetInstanceLayers(const std::vector<const char*>& Layers);
        void AddInstanceExtension(const char* Extension);
        void SetInstanceExtensions(const std::vector<const char*>& Extensions);
        void AddDeviceExtension(const char* Extension);
        void SetDeviceExtensions(const std::vector<const char*>& Extensions);

        vk::Result CreateInstance(vk::InstanceCreateFlags Flags);
        vk::Result CreateDevice(std::uint32_t PhysicalDeviceIndex, vk::DeviceCreateFlags Flags);
        vk::Result RecreateDevice(std::uint32_t PhysicalDeviceIndex, vk::DeviceCreateFlags Flags);
        void       SetSurface(vk::SurfaceKHR Surface);
        vk::Result SetSurfaceFormat(const vk::SurfaceFormatKHR& SurfaceFormat);
        void       SetHdrMetadata(const vk::HdrMetadataEXT& HdrMetadata);
        vk::Result CreateSwapchain(vk::Extent2D Extent, bool bLimitFps, bool bEnableHdr, vk::SwapchainCreateFlagsKHR Flags);
        vk::Result RecreateSwapchain();

        vk::Result SwapImage(vk::Semaphore Semaphore);
        vk::Result PresentImage(const vk::PresentInfoKHR& PresentInfo);
        vk::Result PresentImage(vk::Semaphore Semaphore);
        vk::Result WaitIdle() const;

        const std::vector<const char*>& GetInstanceLayers() const;
        const std::vector<const char*>& GetInstanceExtensions() const;
        const std::vector<const char*>& GetDeviceExtensions() const;

        vk::Instance GetInstance() const;
        vk::SurfaceKHR GetSurface() const;
        vk::PhysicalDevice GetPhysicalDevice() const;
        vk::Device GetDevice() const;
        vk::Queue GetQueue(EQueueType) const;
        vk::SwapchainKHR GetSwapchain() const;

        const vk::PhysicalDeviceProperties& GetPhysicalDeviceProperties() const;
        const vk::PhysicalDeviceMemoryProperties& GetPhysicalDeviceMemoryProperties() const;
        const vk::SwapchainCreateInfoKHR& GetSwapchainCreateInfo() const;

        std::uint32_t GetAvailablePhysicalDeviceCount() const;
        std::uint32_t GetAvailableSurfaceFormatCount() const;
        std::uint32_t GetSwapchainImageCount() const;
        std::uint32_t GetSwapchainImageViewCount() const;

        vk::PhysicalDevice GetAvailablePhysicalDevice(std::uint32_t Index) const;
        vk::Format GetAvailableSurfaceFormat(std::uint32_t Index) const;
        vk::ColorSpaceKHR GetAvailableSurfaceColorSpace(std::uint32_t Index) const;
        vk::Image GetSwapchainImage(std::uint32_t Index) const;
        vk::ImageView GetSwapchainImageView(std::uint32_t Index) const;

        const vk::QueueFamilyProperties& GetQueueFamilyProperties(EQueueType QueueType) const;

        FQueuePool&   GetQueuePool();
        VmaAllocator  GetVmaAllocator() const;
        std::uint32_t GetQueueFamilyIndex(EQueueType QueueType) const;
        std::uint32_t GetCurrentImageIndex() const;
        std::uint32_t GetApiVersion() const;

    private:
        void AddElementChecked(const char* Element, std::vector<const char*>& Vector);

        vk::Result CheckInstanceLayers();
        vk::Result CheckInstanceExtensions();
        vk::Result CheckDeviceExtensions();

        vk::Result UseLatestApiVersion();
        vk::Result GetInstanceExtFunctionProcAddress();
        vk::Result GetDeviceExtFunctionProcAddress();
        vk::Result CreateDebugMessenger();
        vk::Result EnumeratePhysicalDevices();
        vk::Result DeterminePhysicalDevice(std::uint32_t Index, bool bEnableGraphicsQueue, bool bEnableComputeQueue);

        vk::Result ObtainQueueFamilyIndices(vk::PhysicalDevice PhysicalDevice, bool bEnableGraphicsQueue,
                                            bool bEnableComputeQueue, FQueueFamilyIndicesComplex& Indices);

        vk::Result ObtainPhysicalDeviceSurfaceFormats();
        vk::Result CreateSwapchainInternal();
        vk::Result InitializeVmaAllocator();

    private:
        std::vector<std::pair<std::string, std::function<void()>>> CreateDeviceCallbacks_;
        std::vector<std::pair<std::string, std::function<void()>>> DestroyDeviceCallbacks_;
        std::vector<std::pair<std::string, std::function<void()>>> CreateSwapchainCallbacks_;
        std::vector<std::pair<std::string, std::function<void()>>> DestroySwapchainCallbacks_;

        std::vector<const char*>           InstanceLayers_;
        std::vector<const char*>           InstanceExtensions_;
        std::vector<const char*>           DeviceExtensions_;

        std::vector<vk::PhysicalDevice>    AvailablePhysicalDevices_;
        std::vector<vk::SurfaceFormatKHR>  AvailableSurfaceFormats_;
        std::vector<vk::Image>             SwapchainImages_;
        std::vector<vk::ImageView>         SwapchainImageViews_;

        std::vector<vk::QueueFamilyProperties> QueueFamilyProperties_;

        vk::Instance                       Instance_;
        vk::DebugUtilsMessengerEXT         DebugUtilsMessenger_;
        vk::SurfaceKHR                     Surface_;
        vk::PhysicalDevice                 PhysicalDevice_;
        vk::Device                         Device_;
        vk::SwapchainKHR                   Swapchain_;

        vk::PhysicalDeviceProperties       PhysicalDeviceProperties_;
        vk::PhysicalDeviceMemoryProperties PhysicalDeviceMemoryProperties_;
        vk::HdrMetadataEXT                 HdrMetadata_;
        vk::SwapchainCreateInfoKHR		   SwapchainCreateInfo_;
        vk::Extent2D                       SwapchainExtent_;

        std::optional<FQueuePool>          QueuePool_; // for stack allocation

        VmaAllocator                       VmaAllocator_{ nullptr };

        std::unordered_map<EQueueType, vk::Queue>     Queues_;
        std::unordered_map<EQueueType, std::uint32_t> QueueFamilyIndices_;

        std::uint32_t                      CurrentImageIndex_{ std::numeric_limits<std::uint32_t>::max() };
        std::uint32_t                      ApiVersion_{ vk::ApiVersion14 };
    };
} // namespace Npgs

#include "Core.inl"
