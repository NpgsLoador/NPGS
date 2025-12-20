#include "Texture.hpp"
#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    NPGS_INLINE vk::DescriptorImageInfo FTexture::CreateDescriptorImageInfo(const FVulkanSampler& Sampler) const
    {
        return { *Sampler, **ImageView_, vk::ImageLayout::eShaderReadOnlyOptimal };
    }

    NPGS_INLINE vk::DescriptorImageInfo FTexture::CreateDescriptorImageInfo(const vk::Sampler& Sampler) const
    {
        return { Sampler, **ImageView_, vk::ImageLayout::eShaderReadOnlyOptimal };
    }

    NPGS_INLINE FVulkanImage& FTexture::GetImage()
    {
        return ImageMemory_->GetResource();
    }

    NPGS_INLINE const FVulkanImage& FTexture::GetImage() const
    {
        return ImageMemory_->GetResource();
    }

    NPGS_INLINE FVulkanImageView& FTexture::GetImageView()
    {
        return *ImageView_;
    }

    NPGS_INLINE const FVulkanImageView& FTexture::GetImageView() const
    {
        return *ImageView_;
    }

    NPGS_INLINE vk::SamplerCreateInfo FTexture::CreateDefaultSamplerCreateInfo(FVulkanContext* VulkanContext)
    {
        vk::SamplerCreateInfo DefaultSamplerCreateInfo(
            {},
            vk::Filter::eLinear,
            vk::Filter::eLinear,
            vk::SamplerMipmapMode::eLinear,
            vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode::eRepeat,
            vk::SamplerAddressMode::eRepeat,
            0.0f,
            vk::True,
            VulkanContext->GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy,
            vk::False,
            vk::CompareOp::eAlways,
            0.0f,
            vk::LodClampNone,
            vk::BorderColor::eFloatOpaqueWhite,
            vk::False
        );

        return DefaultSamplerCreateInfo;
    }

    NPGS_INLINE std::uint32_t FTexture2D::GetImageWidth() const
    {
        return ImageExtent_.width;
    }

    NPGS_INLINE std::uint32_t FTexture2D::GetImageHeight() const
    {
        return ImageExtent_.height;
    }

    NPGS_INLINE vk::Extent2D FTexture2D::GetImageExtent() const
    {
        return ImageExtent_;
    }

    NPGS_INLINE std::uint32_t FTextureCube::GetImageWidth() const
    {
        return ImageExtent_.width;
    }

    NPGS_INLINE std::uint32_t FTextureCube::GetImageHeight() const
    {
        return ImageExtent_.height;
    }

    NPGS_INLINE vk::Extent2D FTextureCube::GetImageExtent() const
    {
        return ImageExtent_;
    }
} // namespace Npgs
