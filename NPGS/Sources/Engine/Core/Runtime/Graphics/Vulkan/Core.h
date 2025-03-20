#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

// Validation macros
// -----------------
#define VulkanCheck(Target) if (VkResult Result = Target; Result != VK_SUCCESS) return static_cast<vk::Result>(Result);

#define VulkanCheckWithMessage(Target, Message)                                       \
if (VkResult Result = Target; Result != VK_SUCCESS)                                   \
{                                                                                     \
    NpgsCoreError("{}: {}", Message, vk::to_string(static_cast<vk::Result>(Result))); \
    return static_cast<vk::Result>(Result);                                           \
}

#define VulkanHppCheck(Target) if (vk::Result Result = Target; Result != vk::Result::eSuccess) return Result;

#define VulkanHppCheckWithMessage(Target, Message)              \
if (vk::Result Result = Target; Result != vk::Result::eSuccess) \
{                                                               \
    NpgsCoreError("{}: {}", Message, vk::to_string(Result));    \
    return Result;                                              \
}

class FVulkanCore
{
private:
    struct FQueueFamilyIndicesComplex
    {
        std::uint32_t GraphicsQueueFamilyIndex{ vk::QueueFamilyIgnored };
        std::uint32_t PresentQueueFamilyIndex{ vk::QueueFamilyIgnored };
        std::uint32_t ComputeQueueFamilyIndex{ vk::QueueFamilyIgnored };
    };

public:
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
    vk::Queue GetGraphicsQueue() const;
    vk::Queue GetPresentQueue() const;
    vk::Queue GetComputeQueue() const;
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

    VmaAllocator  GetVmaAllocator() const;
    std::uint32_t GetGraphicsQueueFamilyIndex() const;
    std::uint32_t GetPresentQueueFamilyIndex() const;
    std::uint32_t GetComputeQueueFamilyIndex() const;
    std::uint32_t GetCurrentImageIndex() const;
    std::uint32_t GetApiVersion() const;

    static FVulkanCore* GetClassInstance();

private:
    FVulkanCore();
    FVulkanCore(const FVulkanCore&) = delete;
    FVulkanCore(FVulkanCore&&)      = delete;
    ~FVulkanCore();

    FVulkanCore& operator=(const FVulkanCore&) = delete;
    FVulkanCore& operator=(FVulkanCore&&)      = delete;

    void AddElementChecked(const char* Element, std::vector<const char*>& Vector);

    vk::Result CheckInstanceLayers();
    vk::Result CheckInstanceExtensions();
    vk::Result CheckDeviceExtensions();

    vk::Result UseLatestApiVersion();
    vk::Result CreateDebugMessenger();
    vk::Result EnumeratePhysicalDevices();
    vk::Result DeterminePhysicalDevice(std::uint32_t Index, bool bEnableGraphicsQueue, bool bEnableComputeQueue);

    vk::Result ObtainQueueFamilyIndices(vk::PhysicalDevice PhysicalDevice, bool bEnableGraphicsQueue,
                                        bool bEnableComputeQueue, FQueueFamilyIndicesComplex& Indices);

    vk::Result ObtainPhysicalDeviceSurfaceFormats();
    vk::Result CreateSwapchainInternal();
    vk::Result InitializeVmaAllocator();

private:
    std::vector<std::pair<std::string, std::function<void()>>> _CreateDeviceCallbacks;
    std::vector<std::pair<std::string, std::function<void()>>> _DestroyDeviceCallbacks;
    std::vector<std::pair<std::string, std::function<void()>>> _CreateSwapchainCallbacks;
    std::vector<std::pair<std::string, std::function<void()>>> _DestroySwapchainCallbacks;

    std::vector<const char*>           _InstanceLayers;
    std::vector<const char*>           _InstanceExtensions;
    std::vector<const char*>           _DeviceExtensions;

    std::vector<vk::PhysicalDevice>    _AvailablePhysicalDevices;
    std::vector<vk::SurfaceFormatKHR>  _AvailableSurfaceFormats;
    std::vector<vk::Image>             _SwapchainImages;
    std::vector<vk::ImageView>         _SwapchainImageViews;

    vk::Instance                       _Instance;
    vk::DebugUtilsMessengerEXT         _DebugUtilsMessenger;
    vk::SurfaceKHR                     _Surface;
    vk::PhysicalDevice                 _PhysicalDevice;
    vk::Device                         _Device;
    vk::Queue                          _GraphicsQueue;
    vk::Queue                          _PresentQueue;
    vk::Queue                          _ComputeQueue;
    vk::SwapchainKHR                   _Swapchain;

    vk::PhysicalDeviceProperties       _PhysicalDeviceProperties;
    vk::PhysicalDeviceMemoryProperties _PhysicalDeviceMemoryProperties;
    vk::HdrMetadataEXT                 _HdrMetadata;
    vk::SwapchainCreateInfoKHR		   _SwapchainCreateInfo;
    vk::Extent2D                       _SwapchainExtent;

    VmaAllocator                       _VmaAllocator{ nullptr };
    std::uint32_t                      _GraphicsQueueFamilyIndex{ vk::QueueFamilyIgnored };
    std::uint32_t                      _PresentQueueFamilyIndex{ vk::QueueFamilyIgnored };
    std::uint32_t                      _ComputeQueueFamilyIndex{ vk::QueueFamilyIgnored };
    std::uint32_t                      _CurrentImageIndex{ std::numeric_limits<std::uint32_t>::max() };
    std::uint32_t                      _ApiVersion{ VK_API_VERSION_1_4 };
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "Core.inl"
