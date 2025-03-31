#include "Attachments.h"

#include <vulkan/vulkan_to_string.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Core.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

FAttachment::FAttachment(VmaAllocator Allocator)
    : _Allocator(Allocator)
{
}

FColorAttachment::FColorAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                   std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage)
    : FColorAttachment(FVulkanCore::GetClassInstance()->GetVmaAllocator(), AllocationCreateInfo,
                       Format, Extent, LayerCount, SampleCount, ExtraUsage)
{
}

FColorAttachment::FColorAttachment(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format,
                                   vk::Extent2D Extent, std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage)
    : Base(Allocator)
{
    vk::Result Result = CreateAttachment(&AllocationCreateInfo, Format, Extent, LayerCount, SampleCount, ExtraUsage);
    if (Result != vk::Result::eSuccess)
    {
        NpgsCoreError("Failed to create color attachment: {}", vk::to_string(Result));
    }
}

FColorAttachment::FColorAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount,
                                   vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage)
    : Base(nullptr)
{
    vk::Result Result = CreateAttachment(nullptr, Format, Extent, LayerCount, SampleCount, ExtraUsage);
    if (Result != vk::Result::eSuccess)
    {
        NpgsCoreError("Failed to create color attachment: {}", vk::to_string(Result));
    }
}

bool FColorAttachment::CheckFormatAvailability(vk::Format Format, bool bSupportBlend)
{
    vk::FormatProperties FormatProperties = FVulkanCore::GetClassInstance()->GetPhysicalDevice().getFormatProperties(Format);
    if (bSupportBlend)
    {
        if (FormatProperties.optimalTilingFeatures & (vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eColorAttachmentBlend))
        {
            return true;
        }
    }
    else
    {
        if (FormatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment)
        {
            return true;
        }
    }

    return false;
}

vk::Result FColorAttachment::CreateAttachment(const VmaAllocationCreateInfo* AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                              std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage)
{
    vk::ImageCreateInfo ImageCreateInfo;
    ImageCreateInfo.setImageType(vk::ImageType::e2D)
                   .setFormat(Format)
                   .setExtent({ Extent.width, Extent.height, 1 })
                   .setMipLevels(1)
                   .setArrayLayers(LayerCount)
                   .setSamples(SampleCount)
                   .setUsage(vk::ImageUsageFlagBits::eColorAttachment | ExtraUsage);

    vk::MemoryPropertyFlags MemoryPropertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
    if (ExtraUsage & vk::ImageUsageFlagBits::eTransientAttachment)
    {
        MemoryPropertyFlags |= vk::MemoryPropertyFlagBits::eLazilyAllocated;
    }

    _ImageMemory = AllocationCreateInfo
                 ? std::make_unique<FVulkanImageMemory>(_Allocator, *AllocationCreateInfo, ImageCreateInfo)
                 : std::make_unique<FVulkanImageMemory>(ImageCreateInfo, MemoryPropertyFlags);
    
    if (!_ImageMemory->IsValid())
    {
        return vk::Result::eErrorInitializationFailed;
    }

    vk::ImageSubresourceRange SubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, LayerCount);
    _ImageView = std::make_unique<FVulkanImageView>(_ImageMemory->GetResource(),
                                                    LayerCount > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D,
                                                    Format, vk::ComponentMapping(), SubresourceRange);
    if (!_ImageView->IsValid())
    {
        return vk::Result::eErrorInitializationFailed;
    }

    return vk::Result::eSuccess;
}

FDepthStencilAttachment::FDepthStencilAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format,
                                                 vk::Extent2D Extent, std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount,
                                                 vk::ImageUsageFlags ExtraUsage, bool bStencilOnly)
    : FDepthStencilAttachment(FVulkanCore::GetClassInstance()->GetVmaAllocator(), AllocationCreateInfo,
                              Format, Extent, LayerCount, SampleCount, ExtraUsage, bStencilOnly)
{
}

FDepthStencilAttachment::FDepthStencilAttachment(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo,
                                                 vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount,
                                                 vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage, bool bStencilOnly)
    : Base(Allocator)
{
    vk::Result Result = CreateAttachment(&AllocationCreateInfo, Format, Extent, LayerCount, SampleCount, ExtraUsage, bStencilOnly);
    if (Result != vk::Result::eSuccess)
    {
        NpgsCoreError("Failed to create depth-stencil attachment: {}", vk::to_string(Result));
    }
}

FDepthStencilAttachment::FDepthStencilAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount,
                                                 vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage, bool bStencilOnly)
    : Base(nullptr)
{
    vk::Result Result = CreateAttachment(nullptr, Format, Extent, LayerCount, SampleCount, ExtraUsage, bStencilOnly);
    if (Result != vk::Result::eSuccess)
    {
        NpgsCoreError("Failed to create depth-stencil attachment: {}", vk::to_string(Result));
    }
}

bool FDepthStencilAttachment::CheckFormatAvailability(vk::Format Format)
{
    vk::FormatProperties FormatProperties = FVulkanCore::GetClassInstance()->GetPhysicalDevice().getFormatProperties(Format);
    if (FormatProperties.bufferFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
    {
        return true;
    }

    return false;
}

vk::Result FDepthStencilAttachment::CreateAttachment(const VmaAllocationCreateInfo* AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                                     std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage, bool bStencilOnly)
{
    vk::ImageCreateInfo ImageCreateInfo;
    ImageCreateInfo.setImageType(vk::ImageType::e2D)
                   .setFormat(Format)
                   .setExtent({ Extent.width, Extent.height, 1 })
                   .setMipLevels(1)
                   .setArrayLayers(LayerCount)
                   .setSamples(SampleCount)
                   .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment | ExtraUsage);

    vk::MemoryPropertyFlags MemoryPropertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
    if (ExtraUsage & vk::ImageUsageFlagBits::eTransientAttachment)
    {
        MemoryPropertyFlags |= vk::MemoryPropertyFlagBits::eLazilyAllocated;
    }

    _ImageMemory = AllocationCreateInfo
                 ? std::make_unique<FVulkanImageMemory>(_Allocator, *AllocationCreateInfo, ImageCreateInfo)
                 : std::make_unique<FVulkanImageMemory>(ImageCreateInfo, MemoryPropertyFlags);
    if (!_ImageMemory->IsValid())
    {
        return vk::Result::eErrorInitializationFailed;
    }

    vk::ImageAspectFlags AspectFlags = bStencilOnly ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eDepth;
    if (Format > vk::Format::eS8Uint)
    {
        AspectFlags |= vk::ImageAspectFlagBits::eStencil;
    }
    else if (Format == vk::Format::eS8Uint)
    {
        AspectFlags = vk::ImageAspectFlagBits::eStencil;
    }

    vk::ImageSubresourceRange SubresourceRange(AspectFlags, 0, 1, 0, LayerCount);
    _ImageView = std::make_unique<FVulkanImageView>(_ImageMemory->GetResource(),
                                                    LayerCount > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D,
                                                    Format, vk::ComponentMapping(), SubresourceRange);
    if (!_ImageView->IsValid())
    {
        return vk::Result::eErrorInitializationFailed;
    }

    return vk::Result::eSuccess;
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
