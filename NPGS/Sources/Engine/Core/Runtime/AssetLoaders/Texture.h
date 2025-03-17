#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <OpenEXR/ImfRgba.h>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_handles.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Resources.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

class FImageLoader
{
public:
    struct FImageData
    {
        std::vector<std::byte>   Data;
        std::vector<std::size_t> LevelOffsets;
        vk::DeviceSize           Size{};
        vk::Extent3D             Extent{};
        std::uint32_t            MipLevels{};
        Graphics::FFormatInfo    FormatInfo{ Graphics::kFormatInfos[0] };
    };

public:
    FImageLoader() = default;
    ~FImageLoader() = default;

    FImageData LoadImage(std::string_view Filename, vk::Format ImageFormat);

private:
    FImageData LoadCommonFormat(std::string_view Filename, Graphics::FFormatInfo FormatInfo);
    FImageData LoadDdsFormat(std::string_view Filename, Graphics::FFormatInfo FormatInfo);
    FImageData LoadExrFormat(std::string_view Filename, Graphics::FFormatInfo FormatInfo);
    FImageData LoadHdrFormat(std::string_view Filename, Graphics::FFormatInfo FormatInfo);
    FImageData LoadKtxFormat(std::string_view Filename, Graphics::FFormatInfo FormatInfo);
private:
    FImageData _ImageData;
};

class FTextureBase
{
protected:
    using FImageData = FImageLoader::FImageData;

public:
    vk::DescriptorImageInfo CreateDescriptorImageInfo(const Graphics::FVulkanSampler& Sampler) const;
    vk::DescriptorImageInfo CreateDescriptorImageInfo(const vk::Sampler& Sampler) const;

    Graphics::FVulkanImage& GetImage();
    const Graphics::FVulkanImage& GetImage() const;
    Graphics::FVulkanImageView& GetImageView();
    const Graphics::FVulkanImageView& GetImageView() const;

    static vk::SamplerCreateInfo CreateDefaultSamplerCreateInfo();

protected:
    FTextureBase(VmaAllocator Allocator, const VmaAllocationCreateInfo* AllocationCreateInfo);
    FTextureBase(const FTextureBase&)     = delete;
    FTextureBase(FTextureBase&&) noexcept = default;
    virtual ~FTextureBase()               = default;

    FTextureBase& operator=(const FTextureBase&)     = delete;
    FTextureBase& operator=(FTextureBase&&) noexcept = default;

    void CreateTexture(const FImageData& ImageData, vk::Format InitialFormat, vk::Format FinalFormat,
                       vk::ImageCreateFlags Flags, vk::ImageType ImageType, vk::ImageViewType ImageViewType,
                       std::uint32_t ArrayLayers, bool bGenerateMipmaps);

    void CreateTextureInternal(Graphics::FStagingBuffer* StagingBuffer, vk::Format InitialFormat, vk::Format FinalFormat,
                               vk::ImageType ImageType, vk::ImageViewType ImageViewType, vk::Extent3D Extent,
                               vk::ImageCreateFlags Flags, std::uint32_t ArrayLayers, bool bGenerateMipmaps);

    void CreateImageMemory(vk::ImageType ImageType, vk::Format Format, vk::Extent3D Extent, std::uint32_t MipLevels,
                           std::uint32_t ArrayLayers, vk::ImageCreateFlags Flags = {});

    void CreateImageView(vk::ImageViewType ImageViewType, vk::Format Format, std::uint32_t MipLevels,
                         std::uint32_t ArrayLayers, vk::ImageViewCreateFlags Flags = {});

    void CopyBlitGenerateTexture(vk::Buffer SrcBuffer, vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                                 vk::Filter Filter, vk::Image DstImageSrcBlit, vk::Image DstImageDstBlit);

    void BlitGenerateTexture(vk::Image SrcImage, vk::Extent3D Extent, std::uint32_t MipLevels,
                             std::uint32_t ArrayLayers, vk::Filter Filter, vk::Image DstImage);

private:
    void CopyBufferToImage(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Buffer SrcBuffer,
                           const Graphics::FImageMemoryBarrierStagePack& PreTransferStages,
                           const Graphics::FImageMemoryBarrierStagePack& PostTransferStages,
                           const vk::BufferImageCopy& Region, vk::Image DstImage);

    void BlitImage(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Image SrcImage,
                   const Graphics::FImageMemoryBarrierStagePack& PreTransferStages,
                   const Graphics::FImageMemoryBarrierStagePack& PostTransferStages,
                   const vk::ImageBlit& Region, vk::Filter Filter, vk::Image DstImage);


    void GenerateMipmaps(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Image Image,
                         vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                         vk::Filter Filter, const Graphics::FImageMemoryBarrierStagePack& FinalStages);

protected:
    std::unique_ptr<FImageLoader>                 _ImageLoader;
    std::unique_ptr<Graphics::FVulkanImageMemory> _ImageMemory;
    std::unique_ptr<Graphics::FVulkanImageView>   _ImageView;
    VmaAllocator                                  _Allocator;
    const VmaAllocationCreateInfo*                _AllocationCreateInfo;
};

class FTexture2D : public FTextureBase
{
public:
    using Base = FTextureBase;
    using Base::Base;

    FTexture2D(std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat,
               vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

    FTexture2D(const VmaAllocationCreateInfo& AllocationCreateInfo, std::string_view Filename, vk::Format InitialFormat,
               vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

    FTexture2D(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, std::string_view Filename,
               vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

    FTexture2D(const FTexture2D&) = delete;
    FTexture2D(FTexture2D&& Other) noexcept;

    FTexture2D& operator=(const FTexture2D&) = delete;
    FTexture2D& operator=(FTexture2D&& Other) noexcept;

    std::uint32_t GetImageWidth()  const;
    std::uint32_t GetImageHeight() const;
    vk::Extent2D  GetImageExtent() const;

private:
    void CreateTexture(std::string_view Filename, vk::Format  InitialFormat,
                       vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenreteMipmaps);

    void CreateTexture(const FImageData& ImageData, vk::Format InitialFormat,
                       vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps);

private:
    vk::Extent2D _ImageExtent;
};

class FTextureCube : public FTextureBase
{
public:
    using Base = FTextureBase;
    using Base::Base;

    FTextureCube(std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                 vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

    FTextureCube(const VmaAllocationCreateInfo& AllocationCreateInfo, std::string_view Filename, vk::Format InitialFormat,
                 vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

    FTextureCube(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, std::string_view Filename,
                 vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

    FTextureCube(const FTextureCube&) = delete;
    FTextureCube(FTextureCube&& Other) noexcept;

    FTextureCube& operator=(const FTextureCube&) = delete;
    FTextureCube& operator=(FTextureCube&& Other) noexcept;

    std::uint32_t GetImageWidth()  const;
    std::uint32_t GetImageHeight() const;
    vk::Extent2D  GetImageExtent() const;

private:
    void CreateCubemap(std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                       vk::ImageCreateFlags Flags, bool bGenerateMipmaps);

    void CreateCubemap(const std::array<std::string, 6>& Filenames, vk::Format InitialFormat, vk::Format FinalFormat,
                       vk::ImageCreateFlags Flags, bool bGenerateMipmaps);

    void CreateCubemap(const FImageData& ImageData, vk::Format InitialFormat, vk::Format FinalFormat,
                       vk::ImageCreateFlags Flags, bool bGenerateMipmaps);

private:
    vk::Extent2D _ImageExtent;
};


_ASSET_END
_RUNTIME_END
_NPGS_END

#include "Texture.inl"
