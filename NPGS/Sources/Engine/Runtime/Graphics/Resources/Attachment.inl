#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    NPGS_INLINE vk::DescriptorImageInfo FAttachment::CreateDescriptorImageInfo(const FVulkanSampler& Sampler) const
    {
        return { *Sampler, **ImageView_, vk::ImageLayout::eShaderReadOnlyOptimal };
    }

    NPGS_INLINE vk::DescriptorImageInfo FAttachment::CreateDescriptorImageInfo(const vk::Sampler& Sampler) const
    {
        return { Sampler, **ImageView_, vk::ImageLayout::eShaderReadOnlyOptimal };
    }

    NPGS_INLINE FVulkanImage& FAttachment::GetImage()
    {
        return ImageMemory_->GetResource();
    }

    NPGS_INLINE const FVulkanImage& FAttachment::GetImage() const
    {
        return ImageMemory_->GetResource();
    }

    NPGS_INLINE FVulkanImageView& FAttachment::GetImageView()
    {
        return *ImageView_;
    }

    NPGS_INLINE const FVulkanImageView& FAttachment::GetImageView() const
    {
        return *ImageView_;
    }
} // namespace Npgs
