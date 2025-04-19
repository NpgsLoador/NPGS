#include "Texture.h"

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"

namespace Npgs
{
    NPGS_INLINE vk::DescriptorImageInfo FTexture::CreateDescriptorImageInfo(const FVulkanSampler& Sampler) const
    {
        return { *Sampler, **_ImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
    }

    NPGS_INLINE vk::DescriptorImageInfo FTexture::CreateDescriptorImageInfo(const vk::Sampler& Sampler) const
    {
        return { Sampler, **_ImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
    }

    NPGS_INLINE FVulkanImage& FTexture::GetImage()
    {
        return _ImageMemory->GetResource();
    }

    NPGS_INLINE const FVulkanImage& FTexture::GetImage() const
    {
        return _ImageMemory->GetResource();
    }

    NPGS_INLINE FVulkanImageView& FTexture::GetImageView()
    {
        return *_ImageView;
    }

    NPGS_INLINE const FVulkanImageView& FTexture::GetImageView() const
    {
        return *_ImageView;
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
        return _ImageExtent.width;
    }

    NPGS_INLINE std::uint32_t FTexture2D::GetImageHeight() const
    {
        return _ImageExtent.height;
    }

    NPGS_INLINE vk::Extent2D FTexture2D::GetImageExtent() const
    {
        return _ImageExtent;
    }

    NPGS_INLINE std::uint32_t FTextureCube::GetImageWidth() const
    {
        return _ImageExtent.width;
    }

    NPGS_INLINE std::uint32_t FTextureCube::GetImageHeight() const
    {
        return _ImageExtent.height;
    }

    NPGS_INLINE vk::Extent2D FTextureCube::GetImageExtent() const
    {
        return _ImageExtent;
    }
} // namespace Npgs
