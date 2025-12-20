#include "Core.hpp"
#include "Engine/Core/Base/Base.hpp"

#define CompareCallback [Name](const auto& Callback) -> bool { return Name == Callback.first; }

namespace Npgs
{
    NPGS_INLINE void FVulkanCore::AddCreateDeviceCallback(std::string_view Name, const std::function<void()>& Callback)
    {
        CreateDeviceCallbacks_.emplace_back(Name, Callback);
    }

    NPGS_INLINE void FVulkanCore::AddDestroyDeviceCallback(std::string_view Name, const std::function<void()>& Callback)
    {
        DestroyDeviceCallbacks_.emplace_back(Name, Callback);
    }

    NPGS_INLINE void FVulkanCore::AddCreateSwapchainCallback(std::string_view Name, const std::function<void()>& Callback)
    {
        CreateSwapchainCallbacks_.emplace_back(Name, Callback);
    }

    NPGS_INLINE void FVulkanCore::AddDestroySwapchainCallback(std::string_view Name, const std::function<void()>& Callback)
    {
        DestroySwapchainCallbacks_.emplace_back(Name, Callback);
    }

    NPGS_INLINE void FVulkanCore::RemoveCreateDeviceCallback(std::string_view Name)
    {
        std::erase_if(CreateDeviceCallbacks_, CompareCallback);
    }

    NPGS_INLINE void FVulkanCore::RemoveDestroyDeviceCallback(std::string_view Name)
    {
        std::erase_if(DestroyDeviceCallbacks_, CompareCallback);
    }

    NPGS_INLINE void FVulkanCore::RemoveCreateSwapchainCallback(std::string_view Name)
    {
        std::erase_if(CreateSwapchainCallbacks_, CompareCallback);
    }

    NPGS_INLINE void FVulkanCore::RemoveDestroySwapchainCallback(std::string_view Name)
    {
        std::erase_if(DestroySwapchainCallbacks_, CompareCallback);
    }

    NPGS_INLINE void FVulkanCore::AddInstanceLayer(const char* Layer)
    {
        AddElementChecked(Layer, InstanceLayers_);
    }

    NPGS_INLINE void FVulkanCore::SetInstanceLayers(std::vector<const char*> Layers)
    {
        InstanceLayers_ = std::move(Layers);
    }

    NPGS_INLINE void FVulkanCore::AddInstanceExtension(const char* Extension)
    {
        AddElementChecked(Extension, InstanceExtensions_);
    }

    NPGS_INLINE void FVulkanCore::SetInstanceExtensions(std::vector<const char*> Extensions)
    {
        InstanceExtensions_ = std::move(Extensions);
    }

    NPGS_INLINE void FVulkanCore::AddDeviceExtension(const char* Extension)
    {
        AddElementChecked(Extension, DeviceExtensions_);
    }

    NPGS_INLINE void FVulkanCore::SetDeviceExtensions(std::vector<const char*> Extensions)
    {
        DeviceExtensions_ = std::move(Extensions);
    }

    NPGS_INLINE void FVulkanCore::SetSurface(vk::SurfaceKHR Surface)
    {
        Surface_ = Surface;
    }

    NPGS_INLINE void FVulkanCore::SetHdrMetadata(const vk::HdrMetadataEXT& HdrMetadata)
    {
        HdrMetadata_ = HdrMetadata;
    }

    NPGS_INLINE const std::vector<const char*>& FVulkanCore::GetInstanceLayers() const
    {
        return InstanceLayers_;
    }

    NPGS_INLINE const std::vector<const char*>& FVulkanCore::GetInstanceExtensions() const
    {
        return InstanceExtensions_;
    }

    NPGS_INLINE const std::vector<const char*>& FVulkanCore::GetDeviceExtensions() const
    {
        return DeviceExtensions_;
    }

    NPGS_INLINE vk::Instance FVulkanCore::GetInstance() const
    {
        return Instance_;
    }

    NPGS_INLINE vk::SurfaceKHR FVulkanCore::GetSurface() const
    {
        return Surface_;
    }

    NPGS_INLINE vk::PhysicalDevice FVulkanCore::GetPhysicalDevice() const
    {
        return PhysicalDevice_;
    }

    NPGS_INLINE vk::Device FVulkanCore::GetDevice() const
    {
        return Device_;
    }

    NPGS_INLINE vk::Queue FVulkanCore::GetQueue(EQueueType QueueType) const
    {
        return Queues_.at(QueueType);
    }

    NPGS_INLINE vk::SwapchainKHR FVulkanCore::GetSwapchain() const
    {
        return Swapchain_;
    }

    NPGS_INLINE const vk::PhysicalDeviceProperties& FVulkanCore::GetPhysicalDeviceProperties() const
    {
        return PhysicalDeviceProperties_;
    }

    NPGS_INLINE const vk::PhysicalDeviceMemoryProperties& FVulkanCore::GetPhysicalDeviceMemoryProperties() const
    {
        return PhysicalDeviceMemoryProperties_;
    }

    NPGS_INLINE const vk::SwapchainCreateInfoKHR& FVulkanCore::GetSwapchainCreateInfo() const
    {
        return SwapchainCreateInfo_;
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetAvailablePhysicalDeviceCount() const
    {
        return static_cast<std::uint32_t>(AvailablePhysicalDevices_.size());
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetAvailableSurfaceFormatCount() const
    {
        return static_cast<std::uint32_t>(AvailableSurfaceFormats_.size());
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetSwapchainImageCount() const
    {
        return static_cast<std::uint32_t>(SwapchainImages_.size());
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetSwapchainImageViewCount() const
    {
        return static_cast<std::uint32_t>(SwapchainImageViews_.size());
    }

    NPGS_INLINE vk::PhysicalDevice FVulkanCore::GetAvailablePhysicalDevice(std::uint32_t Index) const
    {
        return AvailablePhysicalDevices_[Index];
    }

    NPGS_INLINE vk::Format FVulkanCore::GetAvailableSurfaceFormat(std::uint32_t Index) const
    {
        return AvailableSurfaceFormats_[Index].format;
    }

    NPGS_INLINE vk::ColorSpaceKHR FVulkanCore::GetAvailableSurfaceColorSpace(std::uint32_t Index) const
    {
        return AvailableSurfaceFormats_[Index].colorSpace;
    }

    NPGS_INLINE vk::Image FVulkanCore::GetSwapchainImage(std::uint32_t Index) const
    {
        return SwapchainImages_[Index];
    }

    NPGS_INLINE vk::ImageView FVulkanCore::GetSwapchainImageView(std::uint32_t Index) const
    {
        return SwapchainImageViews_[Index];
    }

    NPGS_INLINE const vk::QueueFamilyProperties& FVulkanCore::GetQueueFamilyProperties(EQueueType QueueType) const
    {
        return QueueFamilyProperties_.at(QueueFamilyIndices_.at(QueueType));
    }

    NPGS_INLINE FQueuePool& FVulkanCore::GetQueuePool()
    {
        return QueuePool_.value();
    }

    NPGS_INLINE VmaAllocator FVulkanCore::GetVmaAllocator() const
    {
        return VmaAllocator_;
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetQueueFamilyIndex(EQueueType QueueType) const
    {
        return QueueFamilyIndices_.at(QueueType);
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetCurrentImageIndex() const
    {
        return CurrentImageIndex_;
    }

    NPGS_INLINE std::uint32_t FVulkanCore::GetApiVersion() const
    {
        return ApiVersion_;
    }
} // namespace Npgs

#undef CompareCallback
