#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

namespace Npgs
{
    class FAttachment
    {
    public:
        FAttachment(VmaAllocator Allocator);
        virtual ~FAttachment() = default;

        vk::DescriptorImageInfo CreateDescriptorImageInfo(const FVulkanSampler& Sampler) const;
        vk::DescriptorImageInfo CreateDescriptorImageInfo(const vk::Sampler& Sampler) const;

        FVulkanImage& GetImage();
        const FVulkanImage& GetImage() const;
        FVulkanImageView& GetImageView();
        const FVulkanImageView& GetImageView() const;

    protected:
        std::unique_ptr<FVulkanImageMemory> _ImageMemory;
        std::unique_ptr<FVulkanImageView>   _ImageView;
        VmaAllocator                        _Allocator;
    };

    class FColorAttachment : public FAttachment
    {
    public:
        using Base = FAttachment;
        using Base::Base;

        FColorAttachment() = delete;
        FColorAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                         std::uint32_t LayerCount = 1, vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                         vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0));

        FColorAttachment(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format,
                         vk::Extent2D Extent, std::uint32_t LayerCount = 1, vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                         vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0));

        FColorAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount = 1,
                         vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                         vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0));

        static bool CheckFormatAvailability(vk::Format Format, bool bSupportBlend = true);

    private:
        vk::Result CreateAttachment(const VmaAllocationCreateInfo* AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                    std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage);
    };

    class FDepthStencilAttachment : public FAttachment
    {
    public:
        using Base = FAttachment;
        using Base::Base;

        FDepthStencilAttachment() = delete;
        FDepthStencilAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                std::uint32_t LayerCount = 1, vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                                vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0), bool bStencilOnly = false);

        FDepthStencilAttachment(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format,
                                vk::Extent2D Extent, std::uint32_t LayerCount = 1, vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                                vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0), bool bStencilOnly = false);

        FDepthStencilAttachment(vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount = 1,
                                vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                                vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0),
                                bool bStencilOnly = false);

        static bool CheckFormatAvailability(vk::Format Format);

    private:
        vk::Result CreateAttachment(const VmaAllocationCreateInfo* AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                    std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage, bool bStencilOnly);
    };
} // namespace Npgs

#include "Attachments.inl"
