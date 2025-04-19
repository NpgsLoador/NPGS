#include "Attachment.h"

#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE vk::DescriptorImageInfo FAttachment::CreateDescriptorImageInfo(const FVulkanSampler& Sampler) const
    {
        return { *Sampler, **_ImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
    }

    NPGS_INLINE vk::DescriptorImageInfo FAttachment::CreateDescriptorImageInfo(const vk::Sampler& Sampler) const
    {
        return { Sampler, **_ImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
    }

    NPGS_INLINE FVulkanImage& FAttachment::GetImage()
    {
        return _ImageMemory->GetResource();
    }

    NPGS_INLINE const FVulkanImage& FAttachment::GetImage() const
    {
        return _ImageMemory->GetResource();
    }

    NPGS_INLINE FVulkanImageView& FAttachment::GetImageView()
    {
        return *_ImageView;
    }

    NPGS_INLINE const FVulkanImageView& FAttachment::GetImageView() const
    {
        return *_ImageView;
    }
} // namespace Npgs
