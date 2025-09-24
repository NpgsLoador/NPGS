#include "stdafx.h"
#include "Attachment.h"

#include <vulkan/vulkan_to_string.hpp>
#include "Engine/Utils/Logger.h"

namespace Npgs
{
    FAttachment::FAttachment(FVulkanContext* VulkanContext, VmaAllocator Allocator)
        : VulkanContext_(VulkanContext)
        , Allocator_(Allocator)
    {
    }

    FColorAttachment::FColorAttachment(FVulkanContext* VulkanContext, VmaAllocator Allocator,
                                       const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format,
                                       vk::Extent2D Extent, std::uint32_t LayerCount,
                                       vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage)
        : Base(VulkanContext, Allocator)
    {
        vk::Result Result = CreateAttachment(AllocationCreateInfo, Format, Extent, LayerCount, SampleCount, ExtraUsage);
        if (Result != vk::Result::eSuccess)
        {
            NpgsCoreError("Failed to create color attachment: {}", vk::to_string(Result));
        }
    }

    bool FColorAttachment::CheckFormatAvailability(vk::PhysicalDevice PhysicalDevice, vk::Format Format, bool bSupportBlend)
    {
        vk::FormatProperties FormatProperties = PhysicalDevice.getFormatProperties(Format);
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

    vk::Result FColorAttachment::CreateAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo,
                                                  vk::Format Format, vk::Extent2D Extent,
                                                  std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount,
                                                  vk::ImageUsageFlags ExtraUsage)
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

        ImageMemory_ = std::make_unique<FVulkanImageMemory>(
            VulkanContext_->GetDevice(), Allocator_, AllocationCreateInfo, ImageCreateInfo);

        if (!ImageMemory_->IsValid())
        {
            return vk::Result::eErrorInitializationFailed;
        }

        vk::ImageSubresourceRange SubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, LayerCount);
        ImageView_ = std::make_unique<FVulkanImageView>(
            VulkanContext_->GetDevice(), ImageMemory_->GetResource(),
            LayerCount > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D,
            Format, vk::ComponentMapping(), SubresourceRange);

        if (!ImageView_->IsValid())
        {
            return vk::Result::eErrorInitializationFailed;
        }

        return vk::Result::eSuccess;
    }

    FDepthStencilAttachment::FDepthStencilAttachment(FVulkanContext* VulkanContext, VmaAllocator Allocator,
                                                     const VmaAllocationCreateInfo& AllocationCreateInfo,
                                                     vk::Format Format, vk::Extent2D Extent,
                                                     std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount,
                                                     vk::ImageUsageFlags ExtraUsage, bool bStencilOnly)
        : Base(VulkanContext, Allocator)
    {
        vk::Result Result = CreateAttachment(AllocationCreateInfo, Format, Extent, LayerCount, SampleCount, ExtraUsage, bStencilOnly);
        if (Result != vk::Result::eSuccess)
        {
            NpgsCoreError("Failed to create depth-stencil attachment: {}", vk::to_string(Result));
        }
    }

    bool FDepthStencilAttachment::CheckFormatAvailability(vk::PhysicalDevice PhysicalDevice, vk::Format Format)
    {
        vk::FormatProperties FormatProperties = PhysicalDevice.getFormatProperties(Format);
        if (FormatProperties.bufferFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
            return true;
        }

        return false;
    }

    vk::Result FDepthStencilAttachment::CreateAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo,
                                                         vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount,
                                                         vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage,
                                                         bool bStencilOnly)
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

        ImageMemory_ = std::make_unique<FVulkanImageMemory>(
            VulkanContext_->GetDevice(), Allocator_, AllocationCreateInfo, ImageCreateInfo);

        if (!ImageMemory_->IsValid())
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
        ImageView_ = std::make_unique<FVulkanImageView>(
            VulkanContext_->GetDevice(), ImageMemory_->GetResource(),
            LayerCount > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D,
            Format, vk::ComponentMapping(), SubresourceRange);

        if (!ImageView_->IsValid())
        {
            return vk::Result::eErrorInitializationFailed;
        }

        return vk::Result::eSuccess;
    }
} // namespace Npgs
