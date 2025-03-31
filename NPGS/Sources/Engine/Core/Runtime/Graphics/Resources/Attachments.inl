#include "Attachments.h"

#include "Engine/Core/Base/Assert.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

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

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
