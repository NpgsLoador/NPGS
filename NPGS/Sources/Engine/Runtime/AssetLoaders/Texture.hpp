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

#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"

namespace Npgs
{
    struct FImageData
    {
        std::vector<std::byte>   Data;
        std::vector<std::size_t> LevelOffsets;
        vk::DeviceSize           Size{};
        vk::Extent3D             Extent{};
        std::uint32_t            MipLevels{};
        FFormatInfo              FormatInfo;

        FImageData();
    };

    class FTexture
    {
    public:
        class FUploadResult
        {
        public:
            FUploadResult()                         = default;
            FUploadResult(const FUploadResult&)     = delete;
            FUploadResult(FUploadResult&&) noexcept = default;
            ~FUploadResult()                        = default;

            FUploadResult& operator=(const FUploadResult&)     = delete;
            FUploadResult& operator=(FUploadResult&&) noexcept = default;

            void Wait() const;

        private:
            friend class FTexture;

            FUploadResult(FVulkanFence&& UploadFence,
                          FCommandPoolPool::FPoolGuard&& CommandPool,
                          FStagingBufferPool::FBufferGuard&& StagingBuffer);

        private:
            FVulkanFence                     UploadFence_;
            FCommandPoolPool::FPoolGuard     CommandPool_;
            FStagingBufferPool::FBufferGuard StagingBuffer_;
        };

    public:
        vk::DescriptorImageInfo CreateDescriptorImageInfo(const FVulkanSampler& Sampler) const;
        vk::DescriptorImageInfo CreateDescriptorImageInfo(const vk::Sampler& Sampler) const;

        FVulkanImage& GetImage();
        const FVulkanImage& GetImage() const;
        FVulkanImageView& GetImageView();
        const FVulkanImageView& GetImageView() const;

        static vk::SamplerCreateInfo CreateDefaultSamplerCreateInfo(FVulkanContext* VulkanContext);

    protected:
        FTexture(FVulkanContext* VulkanContext, VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo);
        FTexture(const FTexture&) = delete;
        FTexture(FTexture&& Other) noexcept;
        virtual ~FTexture() = default;

        FTexture& operator=(const FTexture&) = delete;
        FTexture& operator=(FTexture&& Other) noexcept;

        FUploadResult CreateTexture(const FImageData& ImageData, vk::ImageCreateFlags Flags,
                                    vk::ImageType ImageType, vk::ImageViewType ImageViewType,
                                    vk::Format InitialFormat, vk::Format FinalFormat,
                                    std::uint32_t ArrayLayers, bool bGenerateMipmaps);

    protected:
        std::string TextureName_;

    private:
        void CreateImageMemory(vk::ImageCreateFlags Flags, vk::ImageType ImageType, vk::Format Format,
                               vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers);

        void CreateImageView(vk::ImageViewCreateFlags Flags, vk::ImageViewType ImageViewType, vk::Format Format,
                             std::uint32_t MipLevels, std::uint32_t ArrayLayers);

        FVulkanFence CopyBlitGenerateTexture(const FVulkanCommandPool& CommandPool, vk::Buffer SrcBuffer,
                                             vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                                             vk::Filter Filter, vk::Image DstImageSrcBlit, vk::Image DstImageDstBlit);

        FVulkanFence CopyBlitApplyTexture(const FVulkanCommandPool& CommandPool, vk::Buffer SrcBuffer,
                                          vk::Extent3D Extent, std::uint32_t MipLevels,
                                          const std::vector<std::size_t>& LevelOffsets,
                                          std::uint32_t ArrayLayers, vk::Filter Filter,
                                          vk::Image DstImageSrcBlit, vk::Image DstImageDstBlit);

        void CopyBufferToImage(const FVulkanCommandBuffer& CommandBuffer, vk::Buffer SrcBuffer, vk::Image DstImage,
                               const FImageMemoryMaskPack& PostTransferState, const vk::ArrayProxy<const vk::BufferImageCopy2>& Regions);

        void BlitImage(const FVulkanCommandBuffer& CommandBuffer,
                       vk::Image SrcImage, const FImageMemoryMaskPack& SrcPostTransferState,
                       vk::Image DstImage, const FImageMemoryMaskPack& DstPostTransferState,
                       const vk::ArrayProxy<const vk::ImageBlit2>& Regions, vk::Filter Filter);

        void GenerateMipmaps(const FVulkanCommandBuffer& CommandBuffer, vk::Image Image, const FImageMemoryMaskPack& FinalState,
                             vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers, vk::Filter Filter);

    private:
        FVulkanContext*                     VulkanContext_;
        std::unique_ptr<FVulkanImageMemory> ImageMemory_;
        std::unique_ptr<FVulkanImageView>   ImageView_;
        VmaAllocator                        Allocator_;
        VmaAllocationCreateInfo             AllocationCreateInfo_;
    };

    class FTexture2D : public FTexture
    {
    public:
        using Base = FTexture;
        using Base::Base;

        FTexture2D(FVulkanContext* VulkanContext, VmaAllocator Allocator,
                   const VmaAllocationCreateInfo& AllocationCreateInfo,
                   std::string_view Filename, vk::ImageCreateFlags Flags,
                   vk::Format InitialFormat, vk::Format FinalFormat,
                   bool bGenerateMipmaps, FUploadResult* UploadResult);

        FTexture2D(const FTexture2D&) = delete;
        FTexture2D(FTexture2D&& Other) noexcept;
        ~FTexture2D() override = default;

        FTexture2D& operator=(const FTexture2D&) = delete;
        FTexture2D& operator=(FTexture2D&& Other) noexcept;

        std::uint32_t GetImageWidth()  const;
        std::uint32_t GetImageHeight() const;
        vk::Extent2D  GetImageExtent() const;

    private:
        FUploadResult CreateTexture(std::string_view Filename, vk::ImageCreateFlags Flags,
                                    vk::Format  InitialFormat, vk::Format FinalFormat, bool bGenreteMipmaps);

        FUploadResult CreateTexture(const FImageData& ImageData, vk::ImageCreateFlags Flags,
                                    vk::Format InitialFormat, vk::Format FinalFormat, bool bGenerateMipmaps);

    private:
        vk::Extent2D ImageExtent_;
    };

    class FTextureCube : public FTexture
    {
    public:
        using Base = FTexture;
        using Base::Base;

        FTextureCube(FVulkanContext* VulkanContext, VmaAllocator Allocator,
                     const VmaAllocationCreateInfo& AllocationCreateInfo,
                     std::string_view Filename, vk::ImageCreateFlags Flags,
                     vk::Format InitialFormat, vk::Format FinalFormat,
                     bool bGenerateMipmaps, FUploadResult* UploadResult);

        FTextureCube(const FTextureCube&) = delete;
        FTextureCube(FTextureCube&& Other) noexcept;
        ~FTextureCube() override = default;

        FTextureCube& operator=(const FTextureCube&) = delete;
        FTextureCube& operator=(FTextureCube&& Other) noexcept;

        std::uint32_t GetImageWidth()  const;
        std::uint32_t GetImageHeight() const;
        vk::Extent2D  GetImageExtent() const;

    private:
        FUploadResult CreateCubemap(std::string_view Filename, vk::ImageCreateFlags Flags,
                                    vk::Format InitialFormat, vk::Format FinalFormat, bool bGenerateMipmaps);

        FUploadResult CreateCubemap(const std::array<std::string, 6>& Filenames, vk::ImageCreateFlags Flags,
                                    vk::Format InitialFormat, vk::Format FinalFormat, bool bGenerateMipmaps);

        FUploadResult CreateCubemap(const FImageData& ImageData, vk::ImageCreateFlags Flags,
                                    vk::Format InitialFormat, vk::Format FinalFormat, bool bGenerateMipmaps);

    private:
        vk::Extent2D ImageExtent_;
    };
} // namespace Npgs

#include "Texture.inl"
