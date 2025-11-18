#include "stdafx.h"
#include "Texture.hpp"

#include <cmath>
#include <algorithm>
#include <filesystem>
#include <ranges>
#include <stdexcept>
#include <utility>

#include <gli/gli.hpp>
#include <Imath/half.h>
#include <Imath/ImathBox.h>
#include <ktx.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfTiledRgbaFile.h>
#include <stb_image.h>

#include "Engine/Core/Runtime/Managers/AssetManager.hpp"
#include "Engine/Core/System/Services/EngineServices.hpp"
#include "Engine/Utils/Logger.hpp"

namespace Npgs
{
    namespace
    {
        constexpr std::array kPostTransferStates
        {
            FImageMemoryMaskPack(
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderSampledRead,
                vk::ImageLayout::eShaderReadOnlyOptimal
            ),

            FImageMemoryMaskPack()
        };

        std::uint32_t CalculateMipLevels(vk::Extent3D Extent)
        {
            return static_cast<std::uint32_t>(
                std::floor(std::log2(std::max(std::max(Extent.width, Extent.height), Extent.depth)))) + 1;
        }

        std::uint32_t MipmapSize(std::uint32_t Size, std::uint32_t MipLevel)
        {
            return std::max(1u, Size >> MipLevel);
        }

        vk::Offset3D MipmapExtent(vk::Extent3D Extent, std::uint32_t MipLevel)
        {
            return vk::Offset3D(static_cast<std::int32_t>(MipmapSize(Extent.width,  MipLevel)),
                                static_cast<std::int32_t>(MipmapSize(Extent.height, MipLevel)),
                                static_cast<std::int32_t>(MipmapSize(Extent.depth,  MipLevel)));
        }

        std::vector<vk::BufferImageCopy2> MakeCopyRegions(vk::Extent3D Extent, std::uint32_t MipLevels,
                                                          const std::vector<std::size_t>& LevelOffsets,
                                                          std::uint32_t ArrayLayers)
        {
            std::vector<vk::BufferImageCopy2> Regions;
            for (std::uint32_t MipLevel = 0; MipLevel != MipLevels; ++MipLevel)
            {
                vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, MipLevel, 0, ArrayLayers);
                vk::Offset3D MipmapOffset = MipmapExtent(Extent, MipLevel);
                vk::BufferImageCopy2 Region = vk::BufferImageCopy2()
                    .setBufferOffset(LevelOffsets[MipLevel])
                    .setImageSubresource(Subresource)
                    .setImageExtent(vk::Extent3D(MipmapOffset.x, MipmapOffset.y, MipmapOffset.z));

                Regions.push_back(std::move(Region));
            }

            return Regions;
        }

        vk::ImageBlit2 MakeSingleBlitRegion(vk::Extent3D Extent, vk::ImageSubresourceLayers Subresource)
        {
            std::array<vk::Offset3D, 2> Offsets;
            Offsets[1] = vk::Offset3D(static_cast<std::int32_t>(Extent.width),
                                      static_cast<std::int32_t>(Extent.height),
                                      static_cast<std::int32_t>(Extent.depth));

            vk::ImageBlit2 BlitRegion(Subresource, Offsets, Subresource, Offsets);
            return BlitRegion;
        }

        std::vector<vk::ImageBlit2> MakeBlitRegions(vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers)
        {
            std::vector<vk::ImageBlit2> Regions;
            for (std::uint32_t MipLevel = 0; MipLevel != MipLevels; ++MipLevel)
            {
                vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, MipLevel, 0, ArrayLayers);
                vk::Offset3D MipmapOffset = MipmapExtent(Extent, MipLevel);
                std::array<vk::Offset3D, 2> Offsets;
                Offsets[1] = vk::Offset3D(MipmapOffset);

                Regions.emplace_back(Subresource, Offsets, Subresource, Offsets);
            }

            return Regions;
        }

        FImageData LoadCommonFormat(std::string_view Filename, FFormatInfo FormatInfo)
        {
            int   Width     = 0;
            int   Height    = 0;
            int   Channels  = 0;
            void* PixelData = nullptr;

            if (FormatInfo.RawDataType == FFormatInfo::ERawDataType::kInteger)
            {
                if (FormatInfo.ComponentSize == 1)
                {
                    PixelData = stbi_load(Filename.data(), &Width, &Height, &Channels, FormatInfo.ComponentCount);
                }
                else
                {
                    PixelData = stbi_load_16(Filename.data(), &Width, &Height, &Channels, FormatInfo.ComponentCount);
                }
            }
            else
            {
                PixelData = stbi_loadf(Filename.data(), &Width, &Height, &Channels, FormatInfo.ComponentCount);
            }

            if (PixelData == nullptr)
            {
                NpgsCoreError("Failed to load image: \"{}\".", Filename.data());
                return {};
            }

            FImageData ImageData;
            ImageData.Size   = static_cast<vk::DeviceSize>(Width) * Height * FormatInfo.PixelSize;
            ImageData.Extent = vk::Extent3D(Width, Height, 1);
            ImageData.Data.resize(ImageData.Size);
            std::ranges::copy_n(static_cast<std::byte*>(PixelData), ImageData.Size, ImageData.Data.data());

            return ImageData;
        }

        FImageData LoadDdsFormat(std::string_view Filename, FFormatInfo FormatInfo)
        {
            gli::texture Texture(gli::load(Filename.data()));
            if (Texture.empty())
            {
                NpgsCoreError("Failed to load DirectDraw Surface image: \"{}\".", Filename.data());
                return {};
            }

            std::size_t DesiredComponentCount = gli::component_count(Texture.format());
            if (FormatInfo.ComponentCount != DesiredComponentCount)
            {
                NpgsCoreWarn("Component count mismatch: Expected {}, got {}.", FormatInfo.ComponentCount, DesiredComponentCount);
            }

            gli::extent3d Extent    = Texture.extent();
            std::size_t   Size      = Texture.size();
            std::uint32_t MipLevels = static_cast<std::uint32_t>(Texture.levels());
            void*         PixelData = Texture.data();

            FImageData ImageData;
            ImageData.Extent    = vk::Extent3D(Extent.x, Extent.y, Extent.z);
            ImageData.Size      = Size;
            ImageData.MipLevels = MipLevels;
            for (std::size_t MipLevel = 0; MipLevel != ImageData.MipLevels; ++MipLevel)
            {
                ImageData.LevelOffsets.push_back(static_cast<std::byte*>(Texture.data(0, 0, MipLevel)) - static_cast<std::byte*>(Texture.data()));
            }

            ImageData.Data.resize(Size);
            std::ranges::copy_n(static_cast<std::byte*>(PixelData), Size, ImageData.Data.data());

            return ImageData;
        }

        FImageData LoadExrFormat(std::string_view Filename, FFormatInfo FormatInfo)
        {
            bool bIsTiled = false;
            {
                Imf::InputFile Test(Filename.data());
                bIsTiled = Test.header().hasTileDescription();
            }

            FImageData ImageData;
            int Width = 0, Height = 0;
            std::vector<Imf::Rgba> Pixels;

            if (bIsTiled)
            {
                Imf::TiledRgbaInputFile InputFile(Filename.data());
                const auto& Header   = InputFile.header();
                const auto& TileDesc = Header.tileDescription();
                Imath::Box2i DataWindow(InputFile.dataWindow());
                Width  = DataWindow.max.x - DataWindow.min.x + 1;
                Height = DataWindow.max.y - DataWindow.min.y + 1;

                ImageData.Extent = vk::Extent3D(Width, Height, 1);

                if (TileDesc.mode == Imf::ONE_LEVEL)
                {
                    Pixels.resize(static_cast<std::size_t>(Width) * Height);
                    InputFile.setFrameBuffer(Pixels.data() - (DataWindow.min.x + DataWindow.min.y * Width), 1, Width);
                    InputFile.readTiles(0, InputFile.numXTiles() - 1, 0, InputFile.numYTiles() - 1);

                    ImageData.Size = static_cast<vk::DeviceSize>(Width) * Height * FormatInfo.PixelSize;
                    ImageData.Data.resize(ImageData.Size);
                    std::ranges::copy_n(reinterpret_cast<std::byte*>(Pixels.data()), ImageData.Size, ImageData.Data.data());
                }
                else if (TileDesc.mode == Imf::MIPMAP_LEVELS)
                {
                    int MipLevels = InputFile.numLevels();
                    ImageData.MipLevels = MipLevels;
                    NpgsCoreTrace("OpenEXR image \"{}\": {} mipmap levels found.", Filename.data(), MipLevels);

                    vk::DeviceSize TotalSize = 0;
                    std::vector<std::size_t>  LevelOffsets(MipLevels);
                    std::vector<vk::Extent2D> LevelExtents(MipLevels);
                    for (int MipLevel = 0; MipLevel != MipLevels; ++MipLevel)
                    {
                        std::uint32_t LevelWidth  = MipmapSize(Width,  MipLevel);
                        std::uint32_t LevelHeight = MipmapSize(Height, MipLevel);
                        LevelExtents[MipLevel] = vk::Extent2D(LevelWidth, LevelHeight);
                        LevelOffsets[MipLevel] = TotalSize;
                        TotalSize += static_cast<vk::DeviceSize>(LevelWidth) * LevelHeight * FormatInfo.PixelSize;
                    }

                    ImageData.Size = TotalSize;
                    ImageData.Data.resize(TotalSize);
                    std::vector<Imf::Rgba> LevelPixels;

                    for (int MipLevel = 0; MipLevel != MipLevels; ++MipLevel)
                    {
                        int LevelWidth  = static_cast<int>(LevelExtents[MipLevel].width);
                        int LevelHeight = static_cast<int>(LevelExtents[MipLevel].height);

                        Pixels.resize(static_cast<std::size_t>(LevelWidth) * LevelHeight);
                        InputFile.setFrameBuffer(Pixels.data() - (DataWindow.min.x + DataWindow.min.y * Width), 1, Width);
                        InputFile.readTiles(0, InputFile.numXTiles(MipLevel) - 1, 0, InputFile.numYTiles(MipLevel) - 1, MipLevel, MipLevel);

                        LevelPixels.resize(TotalSize);
                        std::ranges::copy(Pixels, LevelPixels.begin() + LevelOffsets[MipLevel]);
                    }

                    ImageData.LevelOffsets = std::move(LevelOffsets);
                    std::ranges::copy_n(reinterpret_cast<std::byte*>(Pixels.data()), ImageData.Size, ImageData.Data.data());
                }
            }
            else
            {
                Imf::RgbaInputFile InputFile(Filename.data());
                Imath::Box2i DataWindow(InputFile.dataWindow());
                Width  = DataWindow.max.x - DataWindow.min.x + 1;
                Height = DataWindow.max.y - DataWindow.min.y + 1;

                Pixels.resize(static_cast<std::size_t>(Width) * Height);
                InputFile.setFrameBuffer(Pixels.data() - (DataWindow.min.x + DataWindow.min.y * Width), 1, Width);
                InputFile.readPixels(DataWindow.min.y, DataWindow.max.y);

                ImageData.Extent = vk::Extent3D(Width, Height, 1);
                ImageData.Size   = static_cast<vk::DeviceSize>(Width) * Height * FormatInfo.PixelSize;
                ImageData.Data.resize(ImageData.Size);
                std::ranges::copy_n(reinterpret_cast<std::byte*>(Pixels.data()), ImageData.Size, ImageData.Data.data());
            }

            return ImageData;
        }

        FImageData LoadHdrFormat(std::string_view Filename, FFormatInfo FormatInfo)
        {
            int Width    = 0;
            int Height   = 0;
            int Channels = 0;

            float* PixelData = stbi_loadf(Filename.data(), &Width, &Height, &Channels, STBI_default);
            if (PixelData == nullptr)
            {
                NpgsCoreError("Failed to load Radiance HDR image: \"{}\".", Filename.data());
                return {};
            }

            FImageData ImageData;
            ImageData.Extent = vk::Extent3D(Width, Height, 1);
            ImageData.Size   = static_cast<vk::DeviceSize>(Width) * Height * 16;
            ImageData.Data.resize(ImageData.Size);
            if (Channels == 4)
            {
                std::ranges::copy_n(reinterpret_cast<std::byte*>(PixelData), ImageData.Size, ImageData.Data.data());
            }
            else
            {
                std::size_t PixelCount = ImageData.Size / 16;
                float* DstData = reinterpret_cast<float*>(ImageData.Data.data());
                for (std::size_t i = 0; i != PixelCount; ++i)
                {
                    DstData[i * 4 + 0] = PixelData[i * 3 + 0];
                    DstData[i * 4 + 1] = PixelData[i * 3 + 1];
                    DstData[i * 4 + 2] = PixelData[i * 3 + 2];
                    DstData[i * 4 + 3] = 1.0f;
                }
            }

            stbi_image_free(PixelData);

            if (FormatInfo.ComponentSize == 2)
            {
                std::vector<std::byte> HalfData(ImageData.Size / 2);
                std::vector<std::byte> FloatData(std::move(ImageData.Data));
                std::uint16_t* HalfDataPtr  = reinterpret_cast<std::uint16_t*>(HalfData.data());
                float*         FloatDataPtr = reinterpret_cast<float*>(FloatData.data());

                for (std::size_t i = 0; i != ImageData.Size / sizeof(float); ++i)
                {
                    HalfDataPtr[i] = Imath::half(FloatDataPtr[i]).bits();
                }

                ImageData.Size /= 2;
                ImageData.Data = std::move(HalfData);
            }

            return ImageData;
        }

        FImageData LoadKtxFormat(std::string_view Filename, FFormatInfo FormatInfoHint)
        {
            bool bIsKtx2 = Filename.ends_with("ktx2");
            if (!bIsKtx2)
            {
                gli::texture Texture(gli::load(Filename.data()));
                if (Texture.empty())
                {
                    NpgsCoreError("Failed to load Khronos Texture image: \"{}\".", Filename.data());
                    return {};
                }

                gli::extent3d Extent    = Texture.extent();
                std::size_t   Size      = Texture.size();
                std::uint32_t MipLevels = static_cast<std::uint32_t>(Texture.levels());
                void*         PixelData = Texture.data();

                FImageData ImageData;
                ImageData.Extent    = vk::Extent3D(Extent.x, Extent.y, Extent.z);
                ImageData.Size      = Size;
                ImageData.MipLevels = MipLevels;
                for (std::size_t MipLevel = 0; MipLevel != ImageData.MipLevels; ++MipLevel)
                {
                    ImageData.LevelOffsets.push_back(static_cast<std::byte*>(Texture.data(0, 0, MipLevel)) - static_cast<std::byte*>(Texture.data()));
                }

                ImageData.Data.resize(Size);
                std::ranges::copy_n(static_cast<std::byte*>(PixelData), Size, ImageData.Data.data());

                return ImageData;
            }

            ktxTexture2* Texture = nullptr;
            KTX_error_code Result =
                ktxTexture2_CreateFromNamedFile(Filename.data(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &Texture);
            if (Result != KTX_SUCCESS)
            {
                NpgsCoreError("Failed to load Khronos Texture image: \"{}\": \"{}\".", Filename.data(), ktxErrorString(Result));
                return {};
            }

            if (ktxTexture2_NeedsTranscoding(Texture))
            {
                ktx_transcode_fmt_e TranscodeFormat;
                if (FormatInfoHint.ComponentSize == 1)
                {
                    TranscodeFormat = KTX_TTF_BC7_RGBA;
                }
                else
                {
                    TranscodeFormat = KTX_TTF_RGBA32;
                }

                KTX_error_code Result = ktxTexture2_TranscodeBasis(Texture, TranscodeFormat, KTX_TF_HIGH_QUALITY);
                if (Result != KTX_SUCCESS)
                {
                    NpgsCoreError("Failed to transcode basis: \"{}\".", ktxErrorString(Result));
                    ktxTexture_Destroy(ktxTexture(Texture));
                    return {};
                }
            }

            FImageData ImageData;

            ktx_uint32_t Width     = Texture->baseWidth;
            ktx_uint32_t Height    = Texture->baseHeight;
            ktx_uint32_t Depth     = Texture->baseDepth;
            ktx_size_t   Size      = ktxTexture_GetDataSize(ktxTexture(Texture));
            ktx_uint8_t* PixelData = ktxTexture_GetData(ktxTexture(Texture));

            ImageData.Extent    = vk::Extent3D(Width, Height, Depth ? Depth : 1);
            ImageData.Size      = Size;
            ImageData.MipLevels = Texture->numLevels;
            for (std::uint32_t MipLevel = 0; MipLevel != ImageData.MipLevels; ++MipLevel)
            {
                ktx_size_t Offset = 0;
                KTX_error_code Result = ktxTexture_GetImageOffset(ktxTexture(Texture), MipLevel, 0, 0, &Offset);
                if (Result != KTX_SUCCESS)
                {
                    NpgsCoreError("Failed to get Khronos Texture image offset: \"{}\".", ktxErrorString(Result));
                    ktxTexture_Destroy(ktxTexture(Texture));
                    return {};
                }

                ImageData.LevelOffsets.push_back(Offset);
            }

            ImageData.Data.resize(Size);
            std::ranges::copy_n(reinterpret_cast<std::byte*>(PixelData), Size, ImageData.Data.data());
            ImageData.FormatInfo = FFormatInfo(static_cast<vk::Format>(Texture->vkFormat));

            ktxTexture_Destroy(ktxTexture(Texture));
            return ImageData;
        }

        FImageData LoadImage(std::string_view Filename, vk::Format ImageFormat)
        {
            FFormatInfo FormatInfo(ImageFormat);
            FImageData ImageData;
            if (Filename.ends_with(".dds"))
            {
                ImageData = LoadDdsFormat(Filename, FormatInfo);
                ImageData.FormatInfo = FormatInfo;
            }
            else if (Filename.ends_with(".exr"))
            {
                try
                {
                    ImageData = LoadExrFormat(Filename, FormatInfo);
                    ImageData.FormatInfo = FormatInfo;
                }
                catch (const Iex::BaseExc& e)
                {
                    NpgsCoreError("Failed to load OpenEXR image: \"{}\": \"{}\".", Filename.data(), e.what());
                    return {};
                }
            }
            else if (Filename.ends_with(".hdr"))
            {
                ImageData = LoadHdrFormat(Filename, FormatInfo);
                ImageData.FormatInfo = FormatInfo;
            }
            else if (Filename.ends_with(".kmg") || (Filename.ends_with(".ktx") || Filename.ends_with("ktx2")))
            {
                ImageData = LoadKtxFormat(Filename, FormatInfo);
                if (!Filename.ends_with("ktx2"))
                {
                    ImageData.FormatInfo = FormatInfo;
                }
            }
            else
            {
                ImageData = LoadCommonFormat(Filename, FormatInfo);
                ImageData.FormatInfo = FormatInfo;
            }

            return ImageData;
        }
    }

    FTexture::FTexture(FVulkanContext* VulkanContext, VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo)
        : VulkanContext_(VulkanContext)
        , Allocator_(Allocator)
        , AllocationCreateInfo_(AllocationCreateInfo)
    {
    }

    FTexture::FTexture(FTexture&& Other) noexcept
        : VulkanContext_(std::exchange(Other.VulkanContext_, nullptr))
        , ImageMemory_(std::move(Other.ImageMemory_))
        , ImageView_(std::move(Other.ImageView_))
        , Allocator_(std::exchange(Other.Allocator_, nullptr))
        , AllocationCreateInfo_(std::exchange(Other.AllocationCreateInfo_, {}))
        , TextureName_(std::move(Other.TextureName_))
    {
    }

    FTexture& FTexture::operator=(FTexture&& Other) noexcept
    {
        if (this != &Other)
        {
            VulkanContext_        = std::exchange(Other.VulkanContext_, nullptr);
            ImageMemory_          = std::move(Other.ImageMemory_);
            ImageView_            = std::move(Other.ImageView_);
            Allocator_            = std::exchange(Other.Allocator_, nullptr);
            AllocationCreateInfo_ = std::exchange(Other.AllocationCreateInfo_, {});
            TextureName_          = std::move(Other.TextureName_);
        }

        return *this;
    }

    void FTexture::CreateTexture(const FImageData& ImageData, vk::ImageCreateFlags Flags, vk::ImageType ImageType,
                                 vk::ImageViewType ImageViewType, vk::Format InitialFormat, vk::Format FinalFormat,
                                 std::uint32_t ArrayLayers, bool bGenerateMipmaps)
    {
        auto StagingBuffer = VulkanContext_->AcquireStagingBuffer(ImageData.Size);
        StagingBuffer->SubmitBufferData(0, 0, ImageData.Size, ImageData.Data.data());

        vk::Extent3D  Extent          = ImageData.Extent;
        std::uint32_t MipLevels       = 0;
        bool          bImageMipmapped = ImageData.MipLevels != 0 ? true : false;
        if (bGenerateMipmaps)
        {
            MipLevels = bImageMipmapped ? ImageData.MipLevels : CalculateMipLevels(Extent);
        }
        else
        {
            MipLevels = 1;
        }

        CreateImageMemory(Flags, ImageType, InitialFormat, Extent, MipLevels, ArrayLayers);
        CreateImageView({}, ImageViewType, InitialFormat, MipLevels, ArrayLayers);

        auto CommandPool = VulkanContext_->AcquireCommandPool(FVulkanContext::EQueueType::kGeneral);
        FVulkanFence Fence(VulkanContext_->GetDevice(), "CreateTexture_Fence");

        if (InitialFormat == FinalFormat)
        {
            if (!bImageMipmapped)
            {
                CopyBlitGenerateTexture(*CommandPool, *StagingBuffer->GetBuffer(), Extent, MipLevels, ArrayLayers,
                                        vk::Filter::eLinear, *ImageMemory_->GetResource(), {}, &Fence);
            }
            else
            {
                CopyBlitApplyTexture(*CommandPool, *StagingBuffer->GetBuffer(), Extent, MipLevels, ImageData.LevelOffsets,
                                     ArrayLayers, vk::Filter::eLinear, *ImageMemory_->GetResource(), {}, &Fence);
            }
        }
        else
        {
            vk::ImageCreateInfo ImageCreateInfo = vk::ImageCreateInfo()
                .setFlags(Flags)
                .setImageType(ImageType)
                .setFormat(FinalFormat)
                .setExtent(Extent)
                .setMipLevels(MipLevels)
                .setArrayLayers(ArrayLayers)
                .setSamples(vk::SampleCountFlagBits::e1)

                .setUsage(vk::ImageUsageFlagBits::eTransferSrc |
                          vk::ImageUsageFlagBits::eTransferDst |
                          vk::ImageUsageFlagBits::eSampled)

                .setInitialLayout(vk::ImageLayout::eUndefined);

            FVulkanImage* ConvertedImage = StagingBuffer->CreateAliasedImage(InitialFormat, ImageCreateInfo);

            if (ConvertedImage)
            {
                if (!bImageMipmapped)
                {
                    CopyBlitGenerateTexture(*CommandPool, {}, Extent, MipLevels, ArrayLayers, vk::Filter::eLinear,
                                            **ConvertedImage, *ImageMemory_->GetResource(), &Fence);
                }
                else
                {
                    CopyBlitApplyTexture(*CommandPool, {}, Extent, MipLevels, {}, ArrayLayers, vk::Filter::eLinear,
                                         **ConvertedImage, *ImageMemory_->GetResource(), &Fence);
                }
            }
            else
            {
                FVulkanImageMemory VanillaImageMemory(std::move(*ImageMemory_));
                FVulkanImageView VanillaImageView(std::move(*ImageView_));

                ImageMemory_.release();
                ImageView_.release();

                CreateImageMemory(Flags, ImageType, FinalFormat, Extent, MipLevels, ArrayLayers);
                CreateImageView({}, ImageViewType, FinalFormat, MipLevels, ArrayLayers);

                if (!bImageMipmapped)
                {
                    CopyBlitGenerateTexture(*CommandPool, *StagingBuffer->GetBuffer(), Extent, MipLevels, ArrayLayers, vk::Filter::eLinear,
                                            *VanillaImageMemory.GetResource(), *ImageMemory_->GetResource(), &Fence);
                }
                else
                {
                    CopyBlitApplyTexture(*CommandPool, *StagingBuffer->GetBuffer(), Extent, MipLevels,
                                         ImageData.LevelOffsets, ArrayLayers, vk::Filter::eLinear,
                                         *VanillaImageMemory.GetResource(), *ImageMemory_->GetResource(), &Fence);
                }
            }
        }

        Fence.Wait();
    }

    void FTexture::CreateImageMemory(vk::ImageCreateFlags Flags, vk::ImageType ImageType, vk::Format Format,
                                     vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers)
    {
        vk::ImageCreateInfo ImageCreateInfo = vk::ImageCreateInfo()
            .setFlags(Flags)
            .setImageType(ImageType)
            .setFormat(Format)
            .setExtent(Extent)
            .setMipLevels(MipLevels)
            .setArrayLayers(ArrayLayers)
            .setSamples(vk::SampleCountFlagBits::e1)

            .setUsage(vk::ImageUsageFlagBits::eTransferSrc |
                      vk::ImageUsageFlagBits::eTransferDst |
                      vk::ImageUsageFlagBits::eSampled)

            .setInitialLayout(vk::ImageLayout::eUndefined);

        std::string ImageName  = TextureName_ + "_Image";
        std::string MemoryName = TextureName_ + "_Memory";

        ImageMemory_ = std::make_unique<FVulkanImageMemory>(
            VulkanContext_->GetDevice(), ImageName, MemoryName, Allocator_, AllocationCreateInfo_, ImageCreateInfo);
    }

    void FTexture::CreateImageView(vk::ImageViewCreateFlags Flags, vk::ImageViewType ImageViewType,
                                   vk::Format Format, std::uint32_t MipLevels, std::uint32_t ArrayLayers)
    {
        std::string ImageViewName = TextureName_ + "_ImageView";
        vk::ImageSubresourceRange ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, MipLevels, 0, ArrayLayers);

        ImageView_ = std::make_unique<FVulkanImageView>(
            VulkanContext_->GetDevice(),
            ImageViewName,
            ImageMemory_->GetResource(),
            ImageViewType,
            Format,
            vk::ComponentMapping(),
            ImageSubresourceRange,
            Flags
        );
    }

    void FTexture::CopyBlitGenerateTexture(const FVulkanCommandPool& CommandPool, vk::Buffer SrcBuffer, vk::Extent3D Extent,
                                           std::uint32_t MipLevels, std::uint32_t ArrayLayers, vk::Filter Filter,
                                           vk::Image DstImageSrcBlit, vk::Image DstImageDstBlit, const FVulkanFence* Fence)
    {
        bool bGenerateMipmaps = MipLevels > 1;
        bool bNeedBlit        = (DstImageSrcBlit != DstImageDstBlit) && DstImageDstBlit;
        bool bNeedCopy        = static_cast<bool>(SrcBuffer);

        FVulkanCommandBuffer CommandBuffer;
        CommandPool.AllocateBuffer(vk::CommandBufferLevel::ePrimary, "CopyBlitGenerateTexture_CommandBuffer", CommandBuffer);
        CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, 0, 0, ArrayLayers);

        if (bNeedCopy)
        {
            vk::BufferImageCopy2 CopyRegion = vk::BufferImageCopy2()
                .setImageSubresource(Subresource)
                .setImageExtent(Extent);

            CopyBufferToImage(CommandBuffer, SrcBuffer, DstImageSrcBlit,
                              kPostTransferStates[bGenerateMipmaps || bNeedBlit], CopyRegion);
        }

        if (bNeedBlit)
        {
            vk::ImageBlit2 BlitRegion = MakeSingleBlitRegion(Extent, Subresource);
            BlitImage(CommandBuffer, DstImageSrcBlit, {}, DstImageDstBlit,
                      kPostTransferStates[bGenerateMipmaps], BlitRegion, Filter);
        }

        if (bGenerateMipmaps)
        {
            vk::Image TargetImage = bNeedBlit ? DstImageDstBlit : DstImageSrcBlit;
            GenerateMipmaps(CommandBuffer, TargetImage, kPostTransferStates[0], Extent, MipLevels, ArrayLayers, Filter);
        }

        CommandBuffer.End();
        VulkanContext_->SubmitCommandBuffer(FVulkanContext::EQueueType::kGeneral, CommandBuffer, Fence, false);
    }

    void FTexture::CopyBlitApplyTexture(const FVulkanCommandPool& CommandPool, vk::Buffer SrcBuffer, vk::Extent3D Extent,
                                        std::uint32_t MipLevels, const std::vector<std::size_t>& LevelOffsets,
                                        std::uint32_t ArrayLayers, vk::Filter Filter,
                                        vk::Image DstImageSrcBlit, vk::Image DstImageDstBlit, const FVulkanFence* Fence)
    {
        bool bNeedBlit = (DstImageSrcBlit != DstImageDstBlit) && DstImageDstBlit;
        bool bNeedCopy = static_cast<bool>(SrcBuffer) && !LevelOffsets.empty();

        if (bNeedCopy && LevelOffsets.empty())
        {
            throw std::invalid_argument("Level offsets are required for applying buffer to image with multiple mip levels.");
        }

        FVulkanCommandBuffer CommandBuffer;
        CommandPool.AllocateBuffer(vk::CommandBufferLevel::ePrimary, "CopyBlitApplyTexture_CommandBuffer", CommandBuffer);
        CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        if (bNeedCopy)
        {
            auto CopyRegions = MakeCopyRegions(Extent, MipLevels, LevelOffsets, ArrayLayers);
            CopyBufferToImage(CommandBuffer, SrcBuffer, DstImageSrcBlit, kPostTransferStates[bNeedBlit], CopyRegions);
        }

        if (bNeedBlit)
        {
            auto BlitRegions = MakeBlitRegions(Extent, MipLevels, ArrayLayers);
            BlitImage(CommandBuffer, DstImageSrcBlit, {}, DstImageDstBlit, kPostTransferStates[0], BlitRegions, Filter);
        }

        CommandBuffer.End();
        VulkanContext_->SubmitCommandBuffer(FVulkanContext::EQueueType::kGeneral, CommandBuffer, Fence, false);
    }

    void FTexture::CopyBufferToImage(const FVulkanCommandBuffer& CommandBuffer,
                                     vk::Buffer SrcBuffer, vk::Image DstImage,
                                     const FImageMemoryMaskPack& PostTransferState,
                                     const vk::ArrayProxy<const vk::BufferImageCopy2>& Regions)
    {
        vk::BufferImageCopy2 Region = *(Regions.begin());
        vk::ImageSubresourceRange ImageSubresourceRange(
            Region.imageSubresource.aspectMask,
            Region.imageSubresource.mipLevel,
            Regions.size(),
            Region.imageSubresource.baseArrayLayer,
            Region.imageSubresource.layerCount
        );

        FImageMemoryMaskPack DstState(
            vk::PipelineStageFlagBits2::eCopy,
            vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eTransferDstOptimal
        );

        auto* ImageTracker        = EngineResourceServices->GetImageTracker();
        auto  InitDstImageBarrier = ImageTracker->MakeBarrier(DstImage, ImageSubresourceRange, DstState);

        vk::DependencyInfo CopyDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(InitDstImageBarrier);

        CommandBuffer->pipelineBarrier2(CopyDependencyInfo);

        vk::CopyBufferToImageInfo2 CopyBufferToImageInfo(SrcBuffer, DstImage, InitDstImageBarrier.newLayout, Regions);
        CommandBuffer->copyBufferToImage2(CopyBufferToImageInfo);

        if (PostTransferState.kbEnable)
        {
            auto PostTransferBarrier = ImageTracker->MakeBarrier(DstImage, ImageSubresourceRange, PostTransferState);

            CopyDependencyInfo.setImageMemoryBarriers(PostTransferBarrier);
            CommandBuffer->pipelineBarrier2(CopyDependencyInfo);

            ImageTracker->CollapseImageStates(DstImage, PostTransferState);
        }
        else
        {
            ImageTracker->CollapseImageStates(DstImage, DstState);
        }
    }

    void FTexture::BlitImage(const FVulkanCommandBuffer& CommandBuffer,
                             vk::Image SrcImage, const FImageMemoryMaskPack& SrcPostTransferState,
                             vk::Image DstImage, const FImageMemoryMaskPack& DstPostTransferState,
                             const vk::ArrayProxy<const vk::ImageBlit2>& Regions, vk::Filter Filter)
    {
        vk::ImageBlit2 Region = *(Regions.begin());
        vk::ImageSubresourceRange SrcImageSubresourceRange(
            Region.srcSubresource.aspectMask,
            Region.srcSubresource.mipLevel,
            Regions.size(),
            Region.srcSubresource.baseArrayLayer,
            Region.srcSubresource.layerCount
        );

        vk::ImageSubresourceRange DstImageSubresourceRange(
            Region.dstSubresource.aspectMask,
            Region.dstSubresource.mipLevel,
            Regions.size(),
            Region.dstSubresource.baseArrayLayer,
            Region.dstSubresource.layerCount
        );

        auto* ImageTracker = EngineResourceServices->GetImageTracker();

        FImageMemoryMaskPack SrcImageDstState(
            vk::PipelineStageFlagBits2::eBlit,
            vk::AccessFlagBits2::eTransferRead,
            vk::ImageLayout::eTransferSrcOptimal
        );
        auto SrcPreTransferBarrier = ImageTracker->MakeBarrier(SrcImage, SrcImageSubresourceRange, SrcImageDstState);

        FImageMemoryMaskPack DstImageDstState(
            vk::PipelineStageFlagBits2::eBlit,
            vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eTransferDstOptimal
        );
        auto DstPreTransferBarrier = ImageTracker->MakeBarrier(DstImage, DstImageSubresourceRange, DstImageDstState);

        std::array PreTransferBarriers{ SrcPreTransferBarrier, DstPreTransferBarrier };

        vk::DependencyInfo PreTransferDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(PreTransferBarriers);

        CommandBuffer->pipelineBarrier2(PreTransferDependencyInfo);

        vk::BlitImageInfo2 BlitImageInfo(SrcImage, SrcPreTransferBarrier.newLayout,
                                         DstImage, DstPreTransferBarrier.newLayout, Regions, Filter);

        CommandBuffer->blitImage2(BlitImageInfo);

        if (SrcPostTransferState.kbEnable || DstPostTransferState.kbEnable)
        {
            std::vector<vk::ImageMemoryBarrier2> PostTransferBarriers;

            if (SrcPostTransferState.kbEnable)
            {
                auto PostTransferBarrier = ImageTracker->MakeBarrier(SrcImage, SrcImageSubresourceRange, SrcPostTransferState);
                PostTransferBarriers.push_back(PostTransferBarrier);
            }

            if (DstPostTransferState.kbEnable)
            {
                auto PostTransferBarrier = ImageTracker->MakeBarrier(DstImage, DstImageSubresourceRange, DstPostTransferState);
                PostTransferBarriers.push_back(PostTransferBarrier);
            }

            vk::DependencyInfo PostTransferDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PostTransferBarriers);

            CommandBuffer->pipelineBarrier2(PostTransferDependencyInfo);

            if (SrcPostTransferState.kbEnable)
            {
                ImageTracker->CollapseImageStates(SrcImage, SrcPostTransferState);
                if (!DstPostTransferState.kbEnable)
                {
                    ImageTracker->CollapseImageStates(DstImage, DstImageDstState);
                }
            }

            if (DstPostTransferState.kbEnable)
            {
                ImageTracker->CollapseImageStates(DstImage, DstPostTransferState);
                if (!SrcPostTransferState.kbEnable)
                {
                    ImageTracker->CollapseImageStates(SrcImage, SrcImageDstState);
                }
            }
        }
        else
        {
            ImageTracker->CollapseImageStates(SrcImage, SrcImageDstState);
            ImageTracker->CollapseImageStates(DstImage, DstImageDstState);
        }
    }

    void FTexture::GenerateMipmaps(const FVulkanCommandBuffer& CommandBuffer, vk::Image Image, const FImageMemoryMaskPack& FinalState,
                                   vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers, vk::Filter Filter)
    {
        if (!FinalState.kbEnable)
        {
            throw std::runtime_error("FinalState must be enabled in GenerateMipmaps. \
                                      This stage must be the final step in texture loading.");
        }

        for (std::uint32_t MipLevel = 1; MipLevel != MipLevels; ++MipLevel)
        {
            vk::ImageBlit2 Region(
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel - 1, 0, ArrayLayers),
                { vk::Offset3D(), MipmapExtent(Extent, MipLevel - 1) },
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel,     0, ArrayLayers),
                { vk::Offset3D(), MipmapExtent(Extent, MipLevel) }
            );

            BlitImage(CommandBuffer, Image, FinalState, Image, {}, Region, Filter);
        }

        vk::ImageSubresourceRange LastMipRange(vk::ImageAspectFlagBits::eColor, MipLevels - 1, 1, 0, ArrayLayers);

        auto* ImageTracker     = EngineResourceServices->GetImageTracker();
        auto  LastFinalBarrier = ImageTracker->MakeBarrier(Image, LastMipRange, FinalState);

        vk::DependencyInfo FinalDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(LastFinalBarrier);

        CommandBuffer->pipelineBarrier2(FinalDependencyInfo);

        ImageTracker->CollapseImageStates(Image, FinalState);
    }

    FTexture2D::FTexture2D(FVulkanContext* VulkanContext, VmaAllocator Allocator,
                           const VmaAllocationCreateInfo& AllocationCreateInfo,
                           std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                           vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
        : Base(VulkanContext, Allocator, AllocationCreateInfo)
    {
        TextureName_ = Filename;

        CreateTexture(GetAssetFullPath(EAssetType::kTexture, Filename.data()),
                      InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
    }

    FTexture2D::FTexture2D(FTexture2D&& Other) noexcept
        : Base(std::move(Other))
        , ImageExtent_(std::exchange(Other.ImageExtent_, {}))
    {
    }

    FTexture2D& FTexture2D::operator=(FTexture2D&& Other) noexcept
    {
        if (this != &Other)
        {
            Base::operator=(std::move(Other));
            ImageExtent_ = std::exchange(Other.ImageExtent_, {});
        }

        return *this;
    }

    void FTexture2D::CreateTexture(std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                                   vk::ImageCreateFlags Flags, bool bGenreteMipmaps)
    {
        FImageData ImageData = LoadImage(Filename, InitialFormat);
        CreateTexture(ImageData, InitialFormat, FinalFormat, Flags, bGenreteMipmaps);
    }

    void FTexture2D::CreateTexture(const FImageData& ImageData, vk::Format InitialFormat, vk::Format FinalFormat,
                                   vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    {
        ImageExtent_ = vk::Extent2D(ImageData.Extent.width, ImageData.Extent.height);
        Base::CreateTexture(ImageData, Flags, vk::ImageType::e2D, vk::ImageViewType::e2D,
                            InitialFormat, FinalFormat, 1, bGenerateMipmaps);
    }

    FTextureCube::FTextureCube(FVulkanContext* VulkanContext, VmaAllocator Allocator,
                               const VmaAllocationCreateInfo& AllocationCreateInfo,
                               std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                               vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
        : Base(VulkanContext, Allocator, AllocationCreateInfo)
    {
        TextureName_ = Filename;
        std::string FullPath = GetAssetFullPath(EAssetType::kTexture, Filename.data());
        if (std::filesystem::is_directory(FullPath))
        {
            std::array<std::string, 6> Filenames{ "PosX", "NegX", "PosY", "NegY", "PosZ", "NegZ" };
            std::size_t Index = 0;
            for (const auto& Entry : std::filesystem::directory_iterator(FullPath))
            {
                if (Entry.is_regular_file())
                {
                    std::string Extension = Entry.path().extension().string();
                    Filenames[Index] = std::string(Filename) + "/" + Filenames[Index] + Extension;
                }
                ++Index;
            }

            CreateCubemap(Filenames, InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
        }
        else
        {
            CreateCubemap(GetAssetFullPath(EAssetType::kTexture, Filename.data()),
                          InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
        }
    }

    FTextureCube::FTextureCube(FTextureCube&& Other) noexcept
        : Base(std::move(Other))
        , ImageExtent_(std::exchange(Other.ImageExtent_, {}))
    {
    }

    FTextureCube& FTextureCube::operator=(FTextureCube&& Other) noexcept
    {
        if (this != &Other)
        {
            Base::operator=(std::move(Other));
            ImageExtent_ = std::exchange(Other.ImageExtent_, {});
        }

        return *this;
    }

    void FTextureCube::CreateCubemap(std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                                     vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    {
        FImageData ImageData = LoadImage(Filename, InitialFormat);
        CreateCubemap(ImageData, InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
    }

    void FTextureCube::CreateCubemap(const std::array<std::string, 6>& Filenames, vk::Format InitialFormat,
                                     vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    {
        std::array<FImageData, 6> FaceImages;

        for (int i = 0; i != 6; ++i)
        {
            FaceImages[i] = LoadImage(GetAssetFullPath(EAssetType::kTexture, Filenames[i]), InitialFormat);

            if (i == 0)
            {
                ImageExtent_ = vk::Extent2D(FaceImages[i].Extent.width, FaceImages[i].Extent.height);
            }
            else if (FaceImages[i].Extent.width != ImageExtent_.width || FaceImages[i].Extent.height != ImageExtent_.height)
            {
                NpgsCoreError("Cubemap faces must have same dimensions. Face {} has different size.", i);
                return;
            }
        }

        vk::DeviceSize TotalSize = FaceImages[0].Size * 6;

        std::vector<std::byte> CubemapData;
        CubemapData.reserve(TotalSize);
        for (int i = 0; i != 6; ++i)
        {
            CubemapData.append_range(FaceImages[i].Data | std::views::as_rvalue);
        }

        FImageData CubemapImageData;
        CubemapImageData.Size   = TotalSize;
        CubemapImageData.Extent = vk::Extent3D(ImageExtent_.width, ImageExtent_.height, 1);
        CubemapImageData.Data   = std::move(CubemapData);

        CreateCubemap(CubemapImageData, InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
    }

    void FTextureCube::CreateCubemap(const FImageData& ImageData, vk::Format InitialFormat, vk::Format FinalFormat,
                                     vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    {
        ImageExtent_ = vk::Extent2D(ImageData.Extent.width, ImageData.Extent.height);
        vk::ImageCreateFlags CubeFlags = Flags | vk::ImageCreateFlagBits::eCubeCompatible;

        Base::CreateTexture(ImageData, CubeFlags, vk::ImageType::e2D, vk::ImageViewType::eCube,
                            InitialFormat, FinalFormat, 6, bGenerateMipmaps);
    }
} // namespace Npgs
