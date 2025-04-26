#include "Core.h"

#include "Engine/Core/Base/Base.h"

#define CompareCallback [&Name](const auto& Callback) -> bool { return Name == Callback.first; }

namespace Npgs
{
    NPGS_INLINE void FVulkanCore::AddCreateDeviceCallback(const std::string& Name, const std::function<void()>& Callback)
    {
        _CreateDeviceCallbacks.emplace_back(Name, Callback);
    }

    NPGS_INLINE void FVulkanCore::AddDestroyDeviceCallback(const std::string& Name, const std::function<void()>& Callback)
    {
        _DestroyDeviceCallbacks.emplace_back(Name, Callback);
    }

    NPGS_INLINE void FVulkanCore::AddCreateSwapchainCallback(const std::string& Name, const std::function<void()>& Callback)
    {
        _CreateSwapchainCallbacks.emplace_back(Name, Callback);
    }

    NPGS_INLINE void FVulkanCore::AddDestroySwapchainCallback(const std::string& Name, const std::function<void()>& Callback)
    {
        _DestroySwapchainCallbacks.emplace_back(Name, Callback);
    }

    NPGS_INLINE void FVulkanCore::RemoveCreateDeviceCallback(const std::string& Name)
    {
        std::erase_if(_CreateDeviceCallbacks, CompareCallback);
    }

    NPGS_INLINE void FVulkanCore::RemoveDestroyDeviceCallback(const std::string& Name)
    {
        std::erase_if(_DestroyDeviceCallbacks, CompareCallback);
    }

    NPGS_INLINE void FVulkanCore::RemoveCreateSwapchainCallback(const std::string& Name)
    {
        std::erase_if(_CreateSwapchainCallbacks, CompareCallback);
    }

    NPGS_INLINE void FVulkanCore::RemoveDestroySwapchainCallback(const std::string& Name)
    {
        std::erase_if(_DestroySwapchainCallbacks, CompareCallback);
    }

    NPGS_INLINE void FVulkanCore::AddInstanceLayer(const char* Layer)
    {
        AddElementChecked(Layer, _InstanceLayers);
    }

    NPGS_INLINE void FVulkanCore::SetInstanceLayers(const std::vector<const char*>& Layers)
    {
        _InstanceLayers = Layers;
    }

    NPGS_INLINE void FVulkanCore::AddInstanceExtension(const char* Extension)
    {
        AddElementChecked(Extension, _InstanceExtensions);
    }

    NPGS_INLINE void FVulkanCore::SetInstanceExtensions(const std::vector<const char*>& Extensions)
    {
        _InstanceExtensions = Extensions;
    }

    NPGS_INLINE void FVulkanCore::AddDeviceExtension(const char* Extension)
    {
        AddElementChecked(Extension, _DeviceExtensions);
    }

    NPGS_INLINE void FVulkanCore::SetDeviceExtensions(const std::vector<const char*>& Extensions)
    {
        _DeviceExtensions = Extensions;
    }

    NPGS_INLINE void FVulkanCore::SetSurface(vk::SurfaceKHR Surface)
    {
        _Surface = Surface;
    }

    NPGS_INLINE void FVulkanCore::SetHdrMetadata(const vk::HdrMetadataEXT& HdrMetadata)
    {
        _HdrMetadata = HdrMetadata;
    }

    NPGS_INLINE const std::vector<const char*>& FVulkanCore::GetInstanceLayers() const
    {
        return _InstanceLayers;
    }

    NPGS_INLINE const std::vector<const char*>& FVulkanCore::GetInstanceExtensions() const
    {
        return _InstanceExtensions;
    }

    NPGS_INLINE const std::vector<const char*>& FVulkanCore::GetDeviceExtensions() const
    {
        return _DeviceExtensions;
    }

    NPGS_INLINE vk::Instance FVulkanCore::GetInstance() const
    {
        return _Instance;
    }

    NPGS_INLINE vk::SurfaceKHR FVulkanCore::GetSurface() const
    {
        return _Surface;
    }

    NPGS_INLINE vk::PhysicalDevice FVulkanCore::GetPhysicalDevice() const
    {
        return _PhysicalDevice;
    }

    NPGS_INLINE vk::Device FVulkanCore::GetDevice() const
    {
        return _Device;
    }

    NPGS_INLINE vk::SwapchainKHR FVulkanCore::GetSwapchain() const
    {
        return _Swapchain;
    }

    NPGS_INLINE vk::Queue FVulkanCore::GetGraphicsQueue() const
    {
        return _GraphicsQueue;
    }

    NPGS_INLINE vk::Queue FVulkanCore::GetPresentQueue() const
    {
        return _PresentQueue;
    }

    NPGS_INLINE vk::Queue FVulkanCore::GetComputeQueue() const
    {
        return _ComputeQueue;
    }

    NPGS_INLINE const vk::PhysicalDeviceProperties& FVulkanCore::GetPhysicalDeviceProperties() const
    {
        return _PhysicalDeviceProperties;
    }

    NPGS_INLINE const vk::PhysicalDeviceMemoryProperties& FVulkanCore::GetPhysicalDeviceMemoryProperties() const
    {
        return _PhysicalDeviceMemoryProperties;
    }

    NPGS_INLINE const vk::SwapchainCreateInfoKHR& FVulkanCore::GetSwapchainCreateInfo() const
    {
        return _SwapchainCreateInfo;
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetAvailablePhysicalDeviceCount() const
    {
        return static_cast<std::uint32_t>(_AvailablePhysicalDevices.size());
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetAvailableSurfaceFormatCount() const
    {
        return static_cast<std::uint32_t>(_AvailableSurfaceFormats.size());
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetSwapchainImageCount() const
    {
        return static_cast<std::uint32_t>(_SwapchainImages.size());
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetSwapchainImageViewCount() const
    {
        return static_cast<std::uint32_t>(_SwapchainImageViews.size());
    }

    NPGS_INLINE vk::PhysicalDevice FVulkanCore::GetAvailablePhysicalDevice(std::uint32_t Index) const
    {
        return _AvailablePhysicalDevices[Index];
    }

    NPGS_INLINE vk::Format FVulkanCore::GetAvailableSurfaceFormat(std::uint32_t Index) const
    {
        return _AvailableSurfaceFormats[Index].format;
    }

    NPGS_INLINE vk::ColorSpaceKHR FVulkanCore::GetAvailableSurfaceColorSpace(std::uint32_t Index) const
    {
        return _AvailableSurfaceFormats[Index].colorSpace;
    }

    NPGS_INLINE vk::Image FVulkanCore::GetSwapchainImage(std::uint32_t Index) const
    {
        return _SwapchainImages[Index];
    }

    NPGS_INLINE vk::ImageView FVulkanCore::GetSwapchainImageView(std::uint32_t Index) const
    {
        return _SwapchainImageViews[Index];
    }

    NPGS_INLINE const vk::QueueFamilyProperties& FVulkanCore::GetGraphicsQueueFamilyProperties() const
    {
        return _QueueFamilyProperties[_GraphicsQueueFamilyIndex];
    }

    NPGS_INLINE const vk::QueueFamilyProperties& FVulkanCore::GetPresentQueueFamilyProperties() const
    {
        return _QueueFamilyProperties[_PresentQueueFamilyIndex];
    }

    NPGS_INLINE const vk::QueueFamilyProperties& FVulkanCore::GetComputeQueueFamilyProperties() const
    {
        return _QueueFamilyProperties[_ComputeQueueFamilyIndex];
    }

    NPGS_INLINE const vk::QueueFamilyProperties& FVulkanCore::GetTransferQueueFamilyProperties() const
    {
        return _QueueFamilyProperties[_TransferQueueFamilyIndex];
    }

    NPGS_INLINE FQueuePool& FVulkanCore::GetQueuePool()
    {
        return _QueuePool.value();
    }

    NPGS_INLINE VmaAllocator FVulkanCore::GetVmaAllocator() const
    {
        return _VmaAllocator;
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetGraphicsQueueFamilyIndex() const
    {
        return _GraphicsQueueFamilyIndex;
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetPresentQueueFamilyIndex() const
    {
        return _PresentQueueFamilyIndex;
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetComputeQueueFamilyIndex() const
    {
        return _ComputeQueueFamilyIndex;
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetTransferQueueFamilyIndex() const
    {
        return _TransferQueueFamilyIndex;
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetCurrentImageIndex() const
    {
        return _CurrentImageIndex;
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetApiVersion() const
    {
        return _ApiVersion;
    }
} // namespace Npgs

#undef CompareCallback
