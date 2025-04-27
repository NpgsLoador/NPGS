#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

namespace Npgs
{
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
            FFormatInfo              FormatInfo{ kFormatInfos[0] };
        };

    public:
        FImageLoader() = default;
        ~FImageLoader() = default;

        FImageData LoadImage(std::string_view Filename, vk::Format ImageFormat);

    private:
        FImageData LoadCommonFormat(std::string_view Filename, FFormatInfo FormatInfo);
        FImageData LoadDdsFormat(std::string_view Filename,    FFormatInfo FormatInfo);
        FImageData LoadExrFormat(std::string_view Filename,    FFormatInfo FormatInfo);
        FImageData LoadHdrFormat(std::string_view Filename,    FFormatInfo FormatInfo);
        FImageData LoadKtxFormat(std::string_view Filename,    FFormatInfo FormatInfo);
    };

    class FTexture
    {
    protected:
        using FImageData = FImageLoader::FImageData;

    public:
        vk::DescriptorImageInfo CreateDescriptorImageInfo(const FVulkanSampler& Sampler) const;
        vk::DescriptorImageInfo CreateDescriptorImageInfo(const vk::Sampler& Sampler) const;

        FVulkanImage& GetImage();
        const FVulkanImage& GetImage() const;
        FVulkanImageView& GetImageView();
        const FVulkanImageView& GetImageView() const;

        static vk::SamplerCreateInfo CreateDefaultSamplerCreateInfo(FVulkanContext* VulkanContext);

    protected:
        FTexture(FVulkanContext* VulkanContext, VmaAllocator Allocator, const VmaAllocationCreateInfo* AllocationCreateInfo);
        FTexture(const FTexture&) = delete;
        FTexture(FTexture&& Other) noexcept;
        virtual ~FTexture() = default;

        FTexture& operator=(const FTexture&) = delete;
        FTexture& operator=(FTexture&& Other) noexcept;

        void CreateTexture(const FImageData& ImageData, vk::ImageCreateFlags Flags, vk::ImageType ImageType,
                           vk::ImageViewType ImageViewType, vk::Format InitialFormat, vk::Format FinalFormat,
                           std::uint32_t ArrayLayers, bool bGenerateMipmaps);

    private:
        void CreateTextureInternal(const FImageData& ImageData, vk::ImageCreateFlags Flags, vk::ImageType ImageType,
                                   vk::ImageViewType ImageViewType, vk::Format InitialFormat, vk::Format FinalFormat,
                                   std::uint32_t ArrayLayers, bool bGenerateMipmaps);

        void CreateImageMemory(vk::ImageCreateFlags Flags, vk::ImageType ImageType, vk::Format Format,
                               vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers);

        void CreateImageView(vk::ImageViewCreateFlags Flags, vk::ImageViewType ImageViewType, vk::Format Format,
                             std::uint32_t MipLevels, std::uint32_t ArrayLayers);

        void CopyBlitGenerateTexture(vk::Buffer SrcBuffer, vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                                     vk::Filter Filter, vk::Image DstImageSrcBlit, vk::Image DstImageDstBlit);

        void CopyBlitApplyTexture(vk::Buffer SrcBuffer, vk::Extent3D Extent, std::uint32_t MipLevels,
                                  const std::vector<std::size_t>& LevelOffsets,
                                  std::uint32_t ArrayLayers, vk::Filter Filter, vk::Image DstImage);

        void BlitGenerateTexture(vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                                 vk::Filter Filter, vk::Image SrcImage, vk::Image DstImage);

        void BlitApplyTexture(vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                              vk::Filter Filter, vk::Image SrcImage, vk::Image DstImage);

        void CopyBufferToImage(const FVulkanCommandBuffer& CommandBuffer, vk::Buffer SrcBuffer, vk::Image DstImage,
                               const FImageMemoryMaskPack& PostTransferState, const vk::ArrayProxy<vk::BufferImageCopy>& Regions);

        void BlitImage(const FVulkanCommandBuffer& CommandBuffer,
                       vk::Image SrcImage, const FImageMemoryMaskPack& SrcPostTransferState,
                       vk::Image DstImage, const FImageMemoryMaskPack& DstPostTransferState,
                       const vk::ArrayProxy<vk::ImageBlit>& Regions, vk::Filter Filter);

        void GenerateMipmaps(const FVulkanCommandBuffer& CommandBuffer, vk::Image Image, const FImageMemoryMaskPack& FinalState,
                             vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers, vk::Filter Filter);

    protected:
        FVulkanContext*                     VulkanContext_;
        std::unique_ptr<FImageLoader>       ImageLoader_;
        std::unique_ptr<FVulkanImageMemory> ImageMemory_;
        std::unique_ptr<FVulkanImageView>   ImageView_;
        VmaAllocator                        Allocator_;
        const VmaAllocationCreateInfo*      AllocationCreateInfo_;
    };

    class FTexture2D : public FTexture
    {
    public:
        using Base = FTexture;
        using Base::Base;

        FTexture2D(FVulkanContext* VulkanContext, std::string_view Filename, vk::Format InitialFormat,
                   vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

        FTexture2D(FVulkanContext* VulkanContext, VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, std::string_view Filename,
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
        vk::Extent2D ImageExtent_;
    };

    class FTextureCube : public FTexture
    {
    public:
        using Base = FTexture;
        using Base::Base;

        FTextureCube(FVulkanContext* VulkanContext, std::string_view Filename, vk::Format InitialFormat,
                     vk::Format FinalFormat, vk::ImageCreateFlags Flags = {}, bool bGenerateMipmaps = true);

        FTextureCube(FVulkanContext* VulkanContext, VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, std::string_view Filename,
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
        vk::Extent2D ImageExtent_;
    };
} // namespace Npgs

#include "Texture.inl"
