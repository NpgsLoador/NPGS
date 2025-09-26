#include "stdafx.h"
#include "Core.hpp"

#include <cstdlib>
#include <algorithm>
#include <ranges>

#include "Engine/Core/Runtime/Graphics/Vulkan/ExtFunctionsImpl.hpp"
#include "Engine/Utils/VulkanCheck.hpp"
#include "Engine/Utils/Utils.hpp"

namespace Npgs
{
    struct FVulkanCore::FQueueFamilyIndicesComplex
    {
        std::uint32_t GeneralQueueFamilyIndex{ vk::QueueFamilyIgnored };
        std::uint32_t ComputeQueueFamilyIndex{ vk::QueueFamilyIgnored };
        std::uint32_t TransferQueueFamilyIndex{ vk::QueueFamilyIgnored };
    };

    FVulkanCore::FVulkanCore()
    {
        UseLatestApiVersion();
    }

    FVulkanCore::~FVulkanCore()
    {
        if (Instance_)
        {
            if (Device_)
            {
                WaitIdle();
                if (VmaAllocator_)
                {
                    vmaDestroyAllocator(VmaAllocator_);
                    VmaAllocator_ = nullptr;
                    NpgsCoreInfo("Destroyed VMA allocator.");
                }

                if (Swapchain_)
                {
                    for (auto& Callback : DestroySwapchainCallbacks_)
                    {
                        Callback.second();
                    }
                    for (auto& ImageView : SwapchainImageViews_)
                    {
                        if (ImageView)
                        {
                            Device_.destroyImageView(ImageView);
                        }
                    }
                    SwapchainImageViews_.clear();
                    NpgsCoreInfo("Destroyed image views.");
                    Device_.destroySwapchainKHR(Swapchain_);
                    NpgsCoreInfo("Destroyed swapchain.");
                }

                for (auto& Callback : DestroyDeviceCallbacks_)
                {
                    Callback.second();
                }
                Device_.destroy();
                NpgsCoreInfo("Destroyed logical device.");
            }

            if (Surface_)
            {
                Instance_.destroySurfaceKHR(Surface_);
                NpgsCoreInfo("Destroyed surface.");
            }

            if (DebugUtilsMessenger_)
            {
                Instance_.destroyDebugUtilsMessengerEXT(DebugUtilsMessenger_);
                NpgsCoreInfo("Destroyed debug messenger.");
            }

            CreateSwapchainCallbacks_.clear();
            DestroySwapchainCallbacks_.clear();
            CreateDeviceCallbacks_.clear();
            DestroyDeviceCallbacks_.clear();

            DebugUtilsMessenger_ = vk::DebugUtilsMessengerEXT();
            Surface_             = vk::SurfaceKHR();
            PhysicalDevice_      = vk::PhysicalDevice();
            Device_              = vk::Device();
            Swapchain_           = vk::SwapchainKHR();
            SwapchainCreateInfo_ = vk::SwapchainCreateInfoKHR();

            Instance_.destroy();
            NpgsCoreInfo("Destroyed Vulkan instance.");
        }
    }

    vk::Result FVulkanCore::CreateInstance(vk::InstanceCreateFlags Flags)
    {
    #ifdef _DEBUG
        AddInstanceLayer("VK_LAYER_KHRONOS_validation");
        AddInstanceExtension(vk::EXTDebugUtilsExtensionName);
    #endif // _DEBUG

        AddInstanceExtension(vk::EXTSwapchainColorSpaceExtensionName);

    #pragma warning(push)
    #pragma warning(disable: 4996) // for deprecated api
        vk::ApplicationInfo ApplicationInfo("Von-Neumann in Galaxy Simulator", vk::makeVersion(1, 0, 0),
                                            "No Engine", vk::makeVersion(1, 0, 0), ApiVersion_);
    #pragma warning(pop)
        vk::InstanceCreateInfo InstanceCreateInfo(Flags, &ApplicationInfo, InstanceLayers_, InstanceExtensions_);

        VulkanHppCheck(CheckInstanceLayers());
        VulkanHppCheck(CheckInstanceExtensions());

        try
        {
            Instance_ = vk::createInstance(InstanceCreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create Vulkan instance: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        VulkanHppCheck(GetInstanceExtFunctionProcAddress());

    #ifdef _DEBUG
        VulkanHppCheck(CreateDebugMessenger());
    #endif // _DEBUG

        NpgsCoreInfo("Vulkan instance created successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::CreateDevice(std::uint32_t PhysicalDeviceIndex, vk::DeviceCreateFlags Flags)
    {
        AddDeviceExtension(vk::EXTCustomBorderColorExtensionName);
        AddDeviceExtension(vk::EXTDescriptorBufferExtensionName);    
        AddDeviceExtension(vk::EXTHdrMetadataExtensionName);
        AddDeviceExtension(vk::KHRMaintenance6ExtensionName);
        AddDeviceExtension(vk::KHRSwapchainExtensionName);
        AddDeviceExtension(vk::KHRUnifiedImageLayoutsExtensionName);

        VulkanHppCheck(EnumeratePhysicalDevices());
        VulkanHppCheck(DeterminePhysicalDevice(PhysicalDeviceIndex, true, true));
        VulkanHppCheck(CheckDeviceExtensions());

        float QueuePriority = 1.0f;
        std::vector<float> QueuePriorities;
        std::vector<vk::DeviceQueueCreateInfo> DeviceQueueCreateInfos;

        if (QueueFamilyIndices_.at(EQueueType::kGeneral) != vk::QueueFamilyIgnored)
        {
            const auto& QueueFamilyProperties = QueueFamilyProperties_[QueueFamilyIndices_.at(EQueueType::kGeneral)];
            QueuePriorities.assign(QueueFamilyProperties.queueCount, QueuePriority);
            DeviceQueueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), QueueFamilyIndices_.at(EQueueType::kGeneral), QueuePriorities);
        }
        if (QueueFamilyIndices_.at(EQueueType::kCompute) != vk::QueueFamilyIgnored)
        {
            const auto& QueueFamilyProperties = QueueFamilyProperties_[QueueFamilyIndices_.at(EQueueType::kCompute)];
            QueuePriorities.clear();
            QueuePriorities.assign(QueueFamilyProperties.queueCount, QueuePriority);
            DeviceQueueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), QueueFamilyIndices_.at(EQueueType::kCompute), QueuePriorities);
        }
        if (QueueFamilyIndices_.at(EQueueType::kTransfer) != vk::QueueFamilyIgnored)
        {
            const auto& QueueFamilyProperties = QueueFamilyProperties_[QueueFamilyIndices_.at(EQueueType::kTransfer)];
            QueuePriorities.clear();
            QueuePriorities.assign(QueueFamilyProperties.queueCount, QueuePriority);
            DeviceQueueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), QueueFamilyIndices_.at(EQueueType::kTransfer), QueuePriorities);
        }

        vk::PhysicalDeviceFeatures2 Features2;
        vk::PhysicalDeviceVulkan11Features Features11;
        vk::PhysicalDeviceVulkan12Features Features12;
        vk::PhysicalDeviceVulkan13Features Features13;
        vk::PhysicalDeviceVulkan14Features Features14;

        vk::PhysicalDeviceCustomBorderColorFeaturesEXT   CustomBorderColorFeatures(vk::True, vk::True);
        vk::PhysicalDeviceDescriptorBufferFeaturesEXT    DescriptorBufferFeatures(vk::True, vk::True, vk::True, vk::True);
        vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR UnifiedImageLayoutsFeatures(vk::True, vk::True);

        Features2.setPNext(&Features11);
        Features11.setPNext(&Features12);
        Features12.setPNext(&Features13);
        Features13.setPNext(&Features14);
        Features14.setPNext(&CustomBorderColorFeatures);
        CustomBorderColorFeatures.setPNext(&DescriptorBufferFeatures);
        DescriptorBufferFeatures.setPNext(&UnifiedImageLayoutsFeatures);

        PhysicalDevice_.getFeatures2(&Features2);

        void* pNext = &CustomBorderColorFeatures;
        vk::PhysicalDeviceFeatures PhysicalDeviceFeatures = Features2.features;

        if (ApiVersion_ >= vk::ApiVersion11)
        {
            Features11.setPNext(pNext);
            pNext = &Features11;
        }
        if (ApiVersion_ >= vk::ApiVersion12)
        {
            Features12.setPNext(pNext);
            pNext = &Features12;
        }
        if (ApiVersion_ >= vk::ApiVersion13)
        {
            Features13.setPNext(pNext);
            pNext = &Features13;
        }
        if (ApiVersion_ >= vk::ApiVersion14)
        {
            Features14.setPNext(pNext);
            pNext = &Features14;
        }

        vk::DeviceCreateInfo DeviceCreateInfo(Flags, DeviceQueueCreateInfos, {}, DeviceExtensions_, &PhysicalDeviceFeatures, pNext);

        try
        {
            Device_ = PhysicalDevice_.createDevice(DeviceCreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create logical device: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        QueuePool_.emplace(Device_);
        for (const auto& DeviceQueueCreateInfo : DeviceQueueCreateInfos)
        {
            const auto& QueueFamilyProperties = QueueFamilyProperties_[DeviceQueueCreateInfo.queueFamilyIndex];
            QueuePool_->Register(QueueFamilyProperties.queueFlags, DeviceQueueCreateInfo.queueFamilyIndex, DeviceQueueCreateInfo.queueCount);
        }

        // 常驻队列，用于渲染循环频繁提交
        if (QueueFamilyIndices_.at(EQueueType::kGeneral) != vk::QueueFamilyIgnored)
        {
            Queues_[EQueueType::kGeneral] = QueuePool_->AcquireQueue(
                QueueFamilyProperties_[QueueFamilyIndices_.at(EQueueType::kGeneral)].queueFlags).Release();
        }
        if (QueueFamilyIndices_.at(EQueueType::kCompute) != vk::QueueFamilyIgnored)
        {
            Queues_[EQueueType::kCompute] = QueuePool_->AcquireQueue(
                QueueFamilyProperties_[QueueFamilyIndices_.at(EQueueType::kCompute)].queueFlags).Release();
        }

        PhysicalDeviceProperties_       = PhysicalDevice_.getProperties();
        PhysicalDeviceMemoryProperties_ = PhysicalDevice_.getMemoryProperties();
        NpgsCoreInfo("Logical device created successfully.");
        NpgsCoreInfo("Renderer: {}", PhysicalDeviceProperties_.deviceName.data());

        VulkanHppCheck(GetDeviceExtFunctionProcAddress());
        VulkanHppCheck(InitializeVmaAllocator());

        for (auto& Callback : CreateDeviceCallbacks_)
        {
            Callback.second();
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::RecreateDevice(std::uint32_t PhysicalDeviceIndex, vk::DeviceCreateFlags Flags)
    {
        VulkanHppCheck(WaitIdle());

        if (Swapchain_)
        {
            for (auto& Callback : DestroySwapchainCallbacks_)
            {
                Callback.second();
            }
            for (auto& ImageView : SwapchainImageViews_)
            {
                if (ImageView)
                {
                    Device_.destroyImageView(ImageView);
                }
            }
            SwapchainImageViews_.clear();

            Device_.destroySwapchainKHR(Swapchain_);
            Swapchain_           = vk::SwapchainKHR();
            SwapchainCreateInfo_ = vk::SwapchainCreateInfoKHR();
        }

        for (auto& Callback : DestroyDeviceCallbacks_)
        {
            Callback.second();
        }
        if (Device_)
        {
            Device_.destroy();
            Device_ = vk::Device();
        }

        return CreateDevice(PhysicalDeviceIndex, Flags);
    }

    vk::Result FVulkanCore::SetSurfaceFormat(const vk::SurfaceFormatKHR& SurfaceFormat)
    {
        bool bFormatAvailable = false;
        if (SurfaceFormat.format == vk::Format::eUndefined)
        {
            for (const auto& AvailableSurfaceFormat : AvailableSurfaceFormats_)
            {
                if (AvailableSurfaceFormat.colorSpace == SurfaceFormat.colorSpace)
                {
                    SwapchainCreateInfo_.setImageFormat(AvailableSurfaceFormat.format)
                                        .setImageColorSpace(AvailableSurfaceFormat.colorSpace);
                    bFormatAvailable = true;
                    break;
                }
            }
        }
        else
        {
            for (const auto& AvailableSurfaceFormat : AvailableSurfaceFormats_)
            {
                if (AvailableSurfaceFormat.format     == SurfaceFormat.format &&
                    AvailableSurfaceFormat.colorSpace == SurfaceFormat.colorSpace)
                {
                    SwapchainCreateInfo_.setImageFormat(AvailableSurfaceFormat.format)
                                        .setImageColorSpace(AvailableSurfaceFormat.colorSpace);
                    bFormatAvailable = true;
                    break;
                }
            }
        }

        if (!bFormatAvailable)
        {
            return vk::Result::eErrorFormatNotSupported;
        }

        if (Swapchain_)
        {
            return RecreateSwapchain();
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::CreateSwapchain(vk::Extent2D Extent, bool bLimitFps, bool bEnableHdr, vk::SwapchainCreateFlagsKHR Flags)
    {
        // Swapchain 需要的信息：
        // 1.基本 Surface 能力（Swapchain 中图像的最小/最大数量、宽度和高度）
        vk::SurfaceCapabilitiesKHR SurfaceCapabilities;
        try
        {
            SurfaceCapabilities = PhysicalDevice_.getSurfaceCapabilitiesKHR(Surface_);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to get surface capabilities: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        vk::Extent2D SwapchainExtent;
        if (SurfaceCapabilities.currentExtent.width == std::numeric_limits<std::uint32_t>::max())
        {
            SwapchainExtent = vk::Extent2D
            {
                // 限制 Swapchain 的大小在支持的范围内
                std::clamp(Extent.width,  SurfaceCapabilities.minImageExtent.width,  SurfaceCapabilities.maxImageExtent.width),
                std::clamp(Extent.height, SurfaceCapabilities.minImageExtent.height, SurfaceCapabilities.maxImageExtent.height)
            };
        }
        else
        {
            SwapchainExtent = SurfaceCapabilities.currentExtent;
        }
        SwapchainExtent_ = SwapchainExtent;

        std::uint32_t MinImageCount =
            SurfaceCapabilities.minImageCount + (SurfaceCapabilities.maxImageCount > SurfaceCapabilities.minImageCount);
        SwapchainCreateInfo_.setFlags(Flags)
                            .setSurface(Surface_)
                            .setMinImageCount(MinImageCount)
                            .setImageExtent(SwapchainExtent)
                            .setImageArrayLayers(1)
                            .setImageSharingMode(vk::SharingMode::eExclusive)
                            .setPreTransform(SurfaceCapabilities.currentTransform)
                            .setClipped(vk::True);

        // 设置图像格式
        if (SurfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit) // 优先使用继承模式
        {
            SwapchainCreateInfo_.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eInherit);
        }
        else
        {
            // 找不到继承模式，随便挑一个凑合用的
            static const vk::CompositeAlphaFlagBitsKHR kCompositeAlphaFlags[]
            {
                vk::CompositeAlphaFlagBitsKHR::eOpaque,
                vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
                vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
            };

            for (auto CompositeAlphaFlag : kCompositeAlphaFlags)
            {
                if (SurfaceCapabilities.supportedCompositeAlpha & CompositeAlphaFlag)
                {
                    SwapchainCreateInfo_.setCompositeAlpha(CompositeAlphaFlag);
                    break;
                }
            }
        }

        SwapchainCreateInfo_.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
        if (SurfaceCapabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc)
        {
            SwapchainCreateInfo_.setImageUsage(SwapchainCreateInfo_.imageUsage | vk::ImageUsageFlagBits::eTransferSrc);
        }
        if (SurfaceCapabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst)
        {
            SwapchainCreateInfo_.setImageUsage(SwapchainCreateInfo_.imageUsage | vk::ImageUsageFlagBits::eTransferDst);
        }
        else
        {
            NpgsCoreError("Failed to get supported usage flags.");
            return vk::Result::eErrorFeatureNotPresent;
        }

        // 2.设置 Swapchain 像素格式和色彩空间
        if (AvailableSurfaceFormats_.empty())
        {
            VulkanHppCheck(ObtainPhysicalDeviceSurfaceFormats());
        }

        std::vector<vk::SurfaceFormatKHR> SurfaceFormats;
        if (bEnableHdr)
        {
            SurfaceFormats.emplace_back(vk::Format::eA2B10G10R10UnormPack32, vk::ColorSpaceKHR::eHdr10St2084EXT);
            SurfaceFormats.emplace_back(vk::Format::eA2R10G10B10UnormPack32, vk::ColorSpaceKHR::eHdr10St2084EXT);
            SurfaceFormats.emplace_back(vk::Format::eR16G16B16A16Sfloat, vk::ColorSpaceKHR::eExtendedSrgbLinearEXT);
            
            if (kVkSetHdrMetadataExt == nullptr)
            {
                SurfaceFormats.clear();
                HdrMetadata_ = vk::HdrMetadataEXT();
            }
        }

        SurfaceFormats.emplace_back(vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear);
        SurfaceFormats.emplace_back(vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear);

        if (SwapchainCreateInfo_.imageFormat == vk::Format::eUndefined)
        {
            for (const auto& SurfaceFormat : SurfaceFormats)
            {
                if (SetSurfaceFormat(SurfaceFormat) != vk::Result::eSuccess)
                {
                    SwapchainCreateInfo_.setImageFormat(AvailableSurfaceFormats_[0].format)
                                        .setImageColorSpace(AvailableSurfaceFormats_[0].colorSpace);
                    NpgsCoreWarn("Failed to select a four-component unsigned normalized surface format.");
                }
                else
                {
                    break;
                }
            }
        }

        // 3.设置 Swapchain 呈现模式
        std::vector<vk::PresentModeKHR> SurfacePresentModes;
        try
        {
            SurfacePresentModes = PhysicalDevice_.getSurfacePresentModesKHR(Surface_);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to get surface present modes: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        if (bLimitFps)
        {
            SwapchainCreateInfo_.setPresentMode(vk::PresentModeKHR::eFifo);
        }
        else
        {
            for (const auto& SurfacePresentMode : SurfacePresentModes)
            {
                if (SurfacePresentMode == vk::PresentModeKHR::eMailbox)
                {
                    SwapchainCreateInfo_.setPresentMode(vk::PresentModeKHR::eMailbox);
                    break;
                }
            }
        }

        VulkanHppCheck(CreateSwapchainInternal());

        for (auto& Callback : CreateSwapchainCallbacks_)
        {
            Callback.second();
        }

        NpgsCoreInfo("Swapchain created successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::RecreateSwapchain()
    {
        vk::SurfaceCapabilitiesKHR SurfaceCapabilities;
        try
        {
            SurfaceCapabilities = PhysicalDevice_.getSurfaceCapabilitiesKHR(Surface_);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to get surface capabilities: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        if (SurfaceCapabilities.currentExtent.width == 0 || SurfaceCapabilities.currentExtent.height == 0)
        {
            return vk::Result::eSuboptimalKHR;
        }
        SwapchainCreateInfo_.setImageExtent(SurfaceCapabilities.currentExtent);

        if (SwapchainCreateInfo_.oldSwapchain)
        {
            Device_.destroySwapchainKHR(SwapchainCreateInfo_.oldSwapchain);
        }
        SwapchainCreateInfo_.setOldSwapchain(Swapchain_);

        VulkanHppCheck(WaitIdle());

        for (auto& Callback : DestroySwapchainCallbacks_)
        {
            Callback.second();
        }
        for (auto& ImageView : SwapchainImageViews_)
        {
            if (ImageView)
            {
                Device_.destroyImageView(ImageView);
            }
        }
        SwapchainImageViews_.clear();

        VulkanHppCheck(CreateSwapchainInternal());

        for (auto& Callback : CreateSwapchainCallbacks_)
        {
            Callback.second();
        }

        NpgsCoreInfo("Swapchain recreated successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::SwapImage(vk::Semaphore Semaphore)
    {
        if (SwapchainCreateInfo_.oldSwapchain && SwapchainCreateInfo_.oldSwapchain != Swapchain_) [[unlikely]]
        {
            Device_.destroySwapchainKHR(SwapchainCreateInfo_.oldSwapchain);
            SwapchainCreateInfo_.setOldSwapchain(vk::SwapchainKHR());
        }

        vk::Result Result;
        while ((Result = Device_.acquireNextImageKHR(Swapchain_, std::numeric_limits<std::uint64_t>::max(),
                                                     Semaphore, vk::Fence(), &CurrentImageIndex_)) != vk::Result::eSuccess)
        {
            switch (Result)
            {
            case vk::Result::eSuboptimalKHR:
            case vk::Result::eErrorOutOfDateKHR:
                if ((Result = RecreateSwapchain()) != vk::Result::eSuccess)
                {
                    return Result;
                }
                break;
            default:
                NpgsCoreError("Failed to acquire next image: {}.", vk::to_string(Result));
                return Result;
            }
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::PresentImage(const vk::PresentInfoKHR& PresentInfo)
    {
        try
        {
            vk::Result Result = Queues_.at(EQueueType::kGeneral).presentKHR(PresentInfo);
            switch (Result)
            {
            case vk::Result::eSuccess:
                return Result;
            case vk::Result::eSuboptimalKHR:
                return RecreateSwapchain();
            default:
                NpgsCoreError("Failed to present image: {}.", vk::to_string(Result));
                return Result;
            }
        }
        catch (const vk::SystemError& e)
        {
            vk::Result ErrorResult = static_cast<vk::Result>(e.code().value());
            switch (ErrorResult)
            {
            case vk::Result::eErrorOutOfDateKHR:
                return RecreateSwapchain();
            default:
                NpgsCoreError("Failed to present image: {}.", e.what());
                return ErrorResult;
            }
        }
    }

    vk::Result FVulkanCore::PresentImage(vk::Semaphore Semaphore)
    {
        vk::PresentInfoKHR PresentInfo;
        if (Semaphore)
        {
            PresentInfo = vk::PresentInfoKHR(Semaphore, Swapchain_, CurrentImageIndex_);
        }
        else
        {
            PresentInfo = vk::PresentInfoKHR({}, Swapchain_, CurrentImageIndex_);
        }
        return PresentImage(PresentInfo);
    }

    vk::Result FVulkanCore::WaitIdle() const
    {
        try
        {
            Device_.waitIdle();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to wait for device to be idle: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        return vk::Result::eSuccess;
    }

    void FVulkanCore::AddElementChecked(const char* Element, std::vector<const char*>& Vector)
    {
        auto it = std::find_if(Vector.begin(), Vector.end(), [&Element](const char* ElementInVector)
        {
            return Util::Equal(Element, ElementInVector);
        });

        if (it == Vector.end())
        {
            Vector.push_back(Element);
        }
    }

    vk::Result FVulkanCore::CheckInstanceLayers()
    {
        std::vector<vk::LayerProperties> AvailableLayers;
        try
        {
            AvailableLayers = vk::enumerateInstanceLayerProperties();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to enumerate instance layers: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        if (AvailableLayers.empty())
        {
            InstanceLayers_.clear();
            return vk::Result::eSuccess;
        }

        for (const char*& RequestedLayer : InstanceLayers_)
        {
            bool bLayerFound = false;
            for (const auto& AvailableLayer : AvailableLayers)
            {
                if (Util::Equal(RequestedLayer, AvailableLayer.layerName))
                {
                    bLayerFound = true;
                    break;
                }
            }
            if (!bLayerFound)
            {
                RequestedLayer = nullptr;
            }
        }

        std::erase_if(InstanceLayers_, [](const char* Layer) -> bool
        {
            return Layer == nullptr;
        });

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::CheckInstanceExtensions()
    {
        std::vector<vk::ExtensionProperties> AvailableExtensions;
        try
        {
            AvailableExtensions = vk::enumerateInstanceExtensionProperties(nullptr);
        }
        catch (const vk::SystemError & e)
        {
            NpgsCoreError("Failed to enumerate instance extensions: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        for (const char* Layer : InstanceLayers_)
        {
            if (Layer == nullptr)
            {
                continue;
            }

            std::vector<vk::ExtensionProperties> LayerAvailableExtensions;
            try
            {
                LayerAvailableExtensions = vk::enumerateInstanceExtensionProperties(std::string(Layer));
                AvailableExtensions.append_range(LayerAvailableExtensions | std::views::as_rvalue);
            }
            catch (const vk::SystemError& e)
            {
                NpgsCoreError("Failed to enumerate instance extensions for layer {}: {}", Layer, e.what());
                return static_cast<vk::Result>(e.code().value());
            }
        }

        if (AvailableExtensions.empty())
        {
            InstanceExtensions_.clear();
            return vk::Result::eSuccess;
        }

        for (const char*& RequestedExtension : InstanceExtensions_)
        {
            bool bExtensionFound = false;
            for (const auto& AvailableExtension : AvailableExtensions)
            {
                if (Util::Equal(RequestedExtension, AvailableExtension.extensionName))
                {
                    bExtensionFound = true;
                    break;
                }
            }
            if (!bExtensionFound)
            {
                RequestedExtension = nullptr;
            }
        }

        std::erase_if(InstanceExtensions_, [](const char* Extension) -> bool
        {
            return Extension == nullptr;
        });

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::CheckDeviceExtensions()
    {
        std::vector<vk::ExtensionProperties> AvailableExtensions;
        try
        {
            AvailableExtensions = PhysicalDevice_.enumerateDeviceExtensionProperties();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to enumerate device extensions: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        if (AvailableExtensions.empty())
        {
            DeviceExtensions_.clear();
            return vk::Result::eSuccess;
        }

        for (const char*& RequestedExtension : DeviceExtensions_)
        {
            bool bExtensionFound = false;
            for (const auto& AvailableExtension : AvailableExtensions)
            {
                if (Util::Equal(RequestedExtension, AvailableExtension.extensionName))
                {
                    bExtensionFound = true;
                    break;
                }
            }
            if (!bExtensionFound)
            {
                RequestedExtension = nullptr;
            }
        }

        std::erase_if(DeviceExtensions_, [](const char* Extension) -> bool
        {
            return Extension == nullptr;
        });

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::UseLatestApiVersion()
    {
        if (reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion")))
        {
            try
            {
                ApiVersion_ = vk::enumerateInstanceVersion();
            }
            catch (const vk::SystemError& e)
            {
                NpgsCoreError("Failed to enumerate instance version: {}", e.what());
                return static_cast<vk::Result>(e.code().value());
            }
        }
        else
        {
            NpgsCoreError("Vulkan 1.1+ not available, the application only support Vulkan 1.3+ features. Please update your graphics driver, or replace compatibility hardware.");
            std::exit(EXIT_FAILURE);
        }

    #pragma warning(push)
    #pragma warning(disable: 4996) // for deprecated api
        NpgsCoreInfo("Vulkan API version: {}.{}.{}", vk::versionMajor(ApiVersion_), vk::versionMinor(ApiVersion_), vk::versionPatch(ApiVersion_));
    #pragma warning(pop)
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::GetInstanceExtFunctionProcAddress()
    {
    #ifdef _DEBUG
        kVkCreateDebugUtilsMessengerExt =
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(Instance_.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
        if (kVkCreateDebugUtilsMessengerExt == nullptr)
        {
            NpgsCoreError("Failed to get vkCreateDebugUtilsMessengerEXT function pointer.");
            return vk::Result::eErrorExtensionNotPresent;
        }

        kVkDestroyDebugUtilsMessengerExt =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(Instance_.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
        if (vkDestroyDebugUtilsMessengerEXT == nullptr)
        {
            NpgsCoreError("Failed to get vkDestroyDebugUtilsMessengerEXT function pointer.");
            return vk::Result::eErrorExtensionNotPresent;
        }
    #endif // _DEBUG

        kVkSetHdrMetadataExt = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(Instance_.getProcAddr("vkSetHdrMetadataEXT"));
        if (kVkSetHdrMetadataExt == nullptr)
        {
            NpgsCoreWarn("Failed to get vkSetHdrMetadataEXT function pointer.");
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::GetDeviceExtFunctionProcAddress()
    {
        kVkCmdBindDescriptorBuffersExt =
            reinterpret_cast<PFN_vkCmdBindDescriptorBuffersEXT>(Device_.getProcAddr("vkCmdBindDescriptorBuffersEXT"));
        if (kVkCmdBindDescriptorBuffersExt == nullptr)
        {
            NpgsCoreError("Failed to get vkCmdBindDescriptorBuffersEXT function pointer.");
            return vk::Result::eErrorExtensionNotPresent;
        }

        kVkCmdSetDescriptorBufferOffsetsExt =
            reinterpret_cast<PFN_vkCmdSetDescriptorBufferOffsetsEXT>(Device_.getProcAddr("vkCmdSetDescriptorBufferOffsetsEXT"));
        if (kVkCmdSetDescriptorBufferOffsetsExt == nullptr)
        {
            NpgsCoreError("Failed to get vkCmdSetDescriptorBufferOffsetsEXT function pointer.");
            return vk::Result::eErrorExtensionNotPresent;
        }

        kVkCmdSetDescriptorBufferOffsets2Ext =
            reinterpret_cast<PFN_vkCmdSetDescriptorBufferOffsets2EXT>(Device_.getProcAddr("vkCmdSetDescriptorBufferOffsets2EXT"));
        if (kVkCmdSetDescriptorBufferOffsets2Ext == nullptr)
        {
            NpgsCoreError("Failed to get vkCmdSetDescriptorBufferOffsets2EXT function pointer.");
            return vk::Result::eErrorExtensionNotPresent;
        }

        kVkGetDescriptorExt = reinterpret_cast<PFN_vkGetDescriptorEXT>(Device_.getProcAddr("vkGetDescriptorEXT"));
        if (kVkGetDescriptorExt == nullptr)
        {
            NpgsCoreError("Failed to get vkGetDescriptorEXT function pointer.");
            return vk::Result::eErrorExtensionNotPresent;
        }

        kVkGetDescriptorSetLayoutSizeExt =
            reinterpret_cast<PFN_vkGetDescriptorSetLayoutSizeEXT>(Device_.getProcAddr("vkGetDescriptorSetLayoutSizeEXT"));
        if (kVkGetDescriptorSetLayoutSizeExt == nullptr)
        {
            NpgsCoreError("Failed to get vkGetDescriptorSetLayoutSizeEXT function pointer.");
            return vk::Result::eErrorExtensionNotPresent;
        }

        kVkGetDescriptorSetLayoutBindingOffsetExt =
            reinterpret_cast<PFN_vkGetDescriptorSetLayoutBindingOffsetEXT>(Device_.getProcAddr("vkGetDescriptorSetLayoutBindingOffsetEXT"));
        if (kVkGetDescriptorSetLayoutBindingOffsetExt == nullptr)
        {
            NpgsCoreError("Failed to get vkGetDescriptorSetLayoutBindingOffsetEXT function pointer.");
            return vk::Result::eErrorExtensionNotPresent;
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::CreateDebugMessenger()
    {
        vk::PFN_DebugUtilsMessengerCallbackEXT DebugCallback =
        [](vk::DebugUtilsMessageSeverityFlagBitsEXT      MessageSeverity,
           vk::DebugUtilsMessageTypeFlagsEXT             MessageType,
           const vk::DebugUtilsMessengerCallbackDataEXT* CallbackData,
           void*                                         UserData) -> vk::Bool32
        {
            std::string Severity;
            if (MessageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) Severity = "VERBOSE";
            if (MessageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)    Severity = "INFO";
            if (MessageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) Severity = "WARNING";
            if (MessageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)   Severity = "ERROR";

            if      (Severity == "VERBOSE") NpgsCoreTrace("Validation layer: {}", CallbackData->pMessage);
            else if (Severity == "INFO")    NpgsCoreInfo("Validation layer: {}",  CallbackData->pMessage);
            else if (Severity == "ERROR")   NpgsCoreError("Validation layer: {}", CallbackData->pMessage);
            else if (Severity == "WARNING") NpgsCoreWarn("Validation layer: {}",  CallbackData->pMessage);

            // if (CallbackData->queueLabelCount > 0)
            // 	NpgsCoreTrace("Queue Labels: {}", CallbackData->queueLabelCount);
            // if (CallbackData->cmdBufLabelCount > 0)
            // 	NpgsCoreTrace("Command Buffer Labels: {}", CallbackData->cmdBufLabelCount);
            // if (CallbackData->objectCount > 0)
            // 	NpgsCoreTrace("Objects: {}", CallbackData->objectCount);

            return vk::False;
        };

        auto MessageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo    |
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        auto MessageType     = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral     |
                               vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation  |
                               vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

        vk::DebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCreateInfo({}, MessageSeverity, MessageType, DebugCallback);
        try
        {
            DebugUtilsMessenger_ = Instance_.createDebugUtilsMessengerEXT(DebugUtilsMessengerCreateInfo);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create debug messenger: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreInfo("Debug messenger created successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::EnumeratePhysicalDevices()
    {
        try
        {
            AvailablePhysicalDevices_ = Instance_.enumeratePhysicalDevices();
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to enumerate physical devices: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreInfo("Enumerate physical devices successfully, {} devices found.", AvailablePhysicalDevices_.size());
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::DeterminePhysicalDevice(std::uint32_t Index, bool bEnableGraphicsQueue, bool bEnableComputeQueue)
    {
        FQueueFamilyIndicesComplex Indices;
        if (vk::Result Result = ObtainQueueFamilyIndices(AvailablePhysicalDevices_[Index], bEnableGraphicsQueue, bEnableComputeQueue, Indices);
            Result != vk::Result::eSuccess)
        {
            return Result;
        }

        auto [GeneralQueueFamilyIndex, ComputeQueueFamilyIndex, TransferQueueFamilyIndex] = Indices;

        QueueFamilyIndices_[EQueueType::kGeneral]  = GeneralQueueFamilyIndex;
        QueueFamilyIndices_[EQueueType::kCompute]  = ComputeQueueFamilyIndex;
        QueueFamilyIndices_[EQueueType::kTransfer] = TransferQueueFamilyIndex;

        PhysicalDevice_ = AvailablePhysicalDevices_[Index];
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::ObtainQueueFamilyIndices(vk::PhysicalDevice PhysicalDevice, bool bEnableGraphicsQueue,
                                                     bool bEnableComputeQueue, FQueueFamilyIndicesComplex& Indices)
    {
        std::vector<vk::QueueFamilyProperties> QueueFamilyProperties = PhysicalDevice.getQueueFamilyProperties();
        if (QueueFamilyProperties.empty())
        {
            NpgsCoreError("Failed to get queue family properties.");
            return vk::Result::eErrorInitializationFailed;
        }

        auto& [GeneralQueueFamilyIndex, ComputeQueueFamilyIndex, TransferQueueFamilyIndex] = Indices;
        GeneralQueueFamilyIndex = ComputeQueueFamilyIndex = TransferQueueFamilyIndex = vk::QueueFamilyIgnored;

        for (std::uint32_t i = 0; i != QueueFamilyProperties.size(); ++i)
        {
            bool bSupportGraphics = bEnableGraphicsQueue && QueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics;
            bool bSupportPresent  = false;
            bool bSupportCompute  = bEnableComputeQueue  && QueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eCompute;
            bool bSupportTransfer =       static_cast<bool>(QueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eTransfer);

            if (Surface_)
            {
                try
                {
                    bSupportPresent = PhysicalDevice.getSurfaceSupportKHR(i, Surface_);
                }
                catch (const vk::SystemError& e)
                {
                    NpgsCoreError("Failed to determine if the queue family supports presentation: {}", e.what());
                    return static_cast<vk::Result>(e.code().value());
                }
            }

            if (Surface_ && bSupportGraphics && bSupportCompute && GeneralQueueFamilyIndex == vk::QueueFamilyIgnored)
            {
                if (bSupportPresent)
                {
                    GeneralQueueFamilyIndex = i;
                    continue;
                }
            }

            if (!bSupportGraphics && bSupportCompute && ComputeQueueFamilyIndex == vk::QueueFamilyIgnored)
            {
                ComputeQueueFamilyIndex = i;
            }
            if (!bSupportGraphics && !bSupportCompute && bSupportTransfer && TransferQueueFamilyIndex == vk::QueueFamilyIgnored)
            {
                TransferQueueFamilyIndex = i;
            }
        }

        if (TransferQueueFamilyIndex == vk::QueueFamilyIgnored)
        {
            if (QueueFamilyProperties[GeneralQueueFamilyIndex].queueFlags & vk::QueueFlagBits::eTransfer)
            {
                NpgsCoreInfo("No dedicated DMA transfer queue found, using general queue for transfer operations.");
                TransferQueueFamilyIndex = GeneralQueueFamilyIndex;
            }
            else if (QueueFamilyProperties[ComputeQueueFamilyIndex].queueFlags & vk::QueueFlagBits::eTransfer)
            {
                NpgsCoreInfo("No dedicated DMA transfer queue found, using compute queue for transfer operations.");
                TransferQueueFamilyIndex = ComputeQueueFamilyIndex;
            }
        }

        // 如果有任意一个需要启用的队列族的索引是 vk::QueueFamilyIgnored，报错
        if ((GeneralQueueFamilyIndex  == vk::QueueFamilyIgnored && bEnableGraphicsQueue) ||
            (ComputeQueueFamilyIndex  == vk::QueueFamilyIgnored && bEnableComputeQueue)  ||
            (TransferQueueFamilyIndex == vk::QueueFamilyIgnored))
        {
            NpgsCoreError("Failed to obtain queue family indices.");
            return vk::Result::eErrorFeatureNotPresent;
        }

        QueueFamilyProperties_ = std::move(QueueFamilyProperties);

        NpgsCoreInfo("Queue family indices obtained successfully.");
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::ObtainPhysicalDeviceSurfaceFormats()
    {
        try
        {
            AvailableSurfaceFormats_ = PhysicalDevice_.getSurfaceFormatsKHR(Surface_);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to get surface formats: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        NpgsCoreInfo("Surface formats obtained successfully, {} formats found.", AvailableSurfaceFormats_.size());
        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::CreateSwapchainInternal()
    {
        try
        {
            Swapchain_ = Device_.createSwapchainKHR(SwapchainCreateInfo_);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to create swapchain: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        if (HdrMetadata_.maxLuminance != 0.0f)
        {
            Device_.setHdrMetadataEXT(Swapchain_, HdrMetadata_);
        }

        try
        {
            SwapchainImages_ = Device_.getSwapchainImagesKHR(Swapchain_);
        }
        catch (const vk::SystemError& e)
        {
            NpgsCoreError("Failed to get swapchain images: {}", e.what());
            return static_cast<vk::Result>(e.code().value());
        }

        vk::ImageSubresourceRange ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageViewCreateInfo ImageViewCreateInfo = vk::ImageViewCreateInfo()
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(SwapchainCreateInfo_.imageFormat)
            .setSubresourceRange(ImageSubresourceRange);

        for (const auto& SwapchainImages : SwapchainImages_)
        {
            ImageViewCreateInfo.setImage(SwapchainImages);
            vk::ImageView ImageView;
            try
            {
                ImageView = Device_.createImageView(ImageViewCreateInfo);
            }
            catch (const vk::SystemError& e)
            {
                NpgsCoreError("Failed to create image view: {}", e.what());
                return static_cast<vk::Result>(e.code().value());
            }

            SwapchainImageViews_.push_back(ImageView);
        }

        return vk::Result::eSuccess;
    }

    vk::Result FVulkanCore::InitializeVmaAllocator()
    {
        VmaAllocatorCreateInfo AllocatorCreateInfo
        {
            .flags          = VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT,
            .physicalDevice = PhysicalDevice_,
            .device         = Device_,
            .instance       = Instance_,
        };

        if (vk::Result Result = static_cast<vk::Result>(vmaCreateAllocator(&AllocatorCreateInfo, &VmaAllocator_));
            Result != vk::Result::eSuccess)
        {
            NpgsCoreError("Failed to create VMA allocator: {}", vk::to_string(Result));
            return Result;
        }

        NpgsCoreInfo("VMA allocator created successfully.");
        return vk::Result::eSuccess;
    }
} // namespace Npgs
