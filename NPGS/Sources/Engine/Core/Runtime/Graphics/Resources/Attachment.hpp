#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.hpp"

namespace Npgs
{
    class FAttachment
    {
    public:
        FAttachment(FVulkanContext* VulkanContext, VmaAllocator Allocator);
		FAttachment(const FAttachment&) = delete;
		FAttachment(FAttachment&& Other) noexcept;
        virtual ~FAttachment() = default;

		FAttachment& operator=(const FAttachment&) = delete;
		FAttachment& operator=(FAttachment&& Other) noexcept;

        vk::DescriptorImageInfo CreateDescriptorImageInfo(const FVulkanSampler& Sampler) const;
        vk::DescriptorImageInfo CreateDescriptorImageInfo(const vk::Sampler& Sampler) const;

        FVulkanImage& GetImage();
        const FVulkanImage& GetImage() const;
        FVulkanImageView& GetImageView();
        const FVulkanImageView& GetImageView() const;

    protected:
        FVulkanContext*                     VulkanContext_;
        std::unique_ptr<FVulkanImageMemory> ImageMemory_;
        std::unique_ptr<FVulkanImageView>   ImageView_;
        VmaAllocator                        Allocator_;
    };

    class FColorAttachment : public FAttachment
    {
    public:
        using Base = FAttachment;
        using Base::Base;

        FColorAttachment(FVulkanContext* VulkanContext, VmaAllocator Allocator,
                         const VmaAllocationCreateInfo& AllocationCreateInfo,
                         vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount = 1,
                         vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                         vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0));

        static bool CheckFormatAvailability(vk::PhysicalDevice PhysicalDevice, vk::Format Format, bool bSupportBlend = true);

    private:
        vk::Result CreateAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format, vk::Extent2D Extent,
                                    std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount, vk::ImageUsageFlags ExtraUsage);
    };

    class FDepthStencilAttachment : public FAttachment
    {
    public:
        using Base = FAttachment;
        using Base::Base;

        FDepthStencilAttachment(FVulkanContext* VulkanContext, VmaAllocator Allocator,
                                const VmaAllocationCreateInfo& AllocationCreateInfo,
                                vk::Format Format, vk::Extent2D Extent, std::uint32_t LayerCount = 1,
                                vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1,
                                vk::ImageUsageFlags ExtraUsage = static_cast<vk::ImageUsageFlagBits>(0),
                                bool bStencilOnly = false);

        static bool CheckFormatAvailability(vk::PhysicalDevice PhysicalDevice, vk::Format Format);

    private:
        vk::Result CreateAttachment(const VmaAllocationCreateInfo& AllocationCreateInfo, vk::Format Format,
                                    vk::Extent2D Extent, std::uint32_t LayerCount, vk::SampleCountFlagBits SampleCount,
                                    vk::ImageUsageFlags ExtraUsage, bool bStencilOnly);
    };
} // namespace Npgs

#include "Attachment.inl"
