#include "Texture.h"

#include <cmath>
#include <algorithm>
#include <filesystem>
#include <ranges>
#include <utility>

#include <gli/gli.hpp>
#include <Imath/half.h>
#include <Imath/ImathBox.h>
#include <ktx.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfTiledRgbaFile.h>
#include <stb_image.h>

#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/System/Services/EngineServices.h"
#include "Engine/Utils/Logger.h"

namespace Npgs
{
    namespace
    {
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
    }

    FImageLoader::FImageData FImageLoader::LoadImage(std::string_view Filename, vk::Format ImageFormat)
    {
        bool bIsDds = Filename.ends_with(".dds");
        bool bIsExr = Filename.ends_with(".exr");
        bool bIsHdr = Filename.ends_with(".hdr");
        bool bIsKmg = Filename.ends_with(".kmg");
        bool bIsKtx = Filename.ends_with(".ktx") || Filename.ends_with("ktx2");

        FFormatInfo FormatInfo(ImageFormat);
        FImageData ImageData;
        if (bIsDds)
        {
            ImageData = LoadDdsFormat(Filename, FormatInfo);
        }
        else if (bIsExr)
        {
            try
            {
                ImageData = LoadExrFormat(Filename, FormatInfo);
            }
            catch (const Iex::BaseExc& e)
            {
                NpgsCoreError("Failed to load OpenEXR image: \"{}\": \"{}\".", Filename.data(), e.what());
                return {};
            }
        }
        else if (bIsHdr)
        {
            ImageData = LoadHdrFormat(Filename, FormatInfo);
        }
        else if (bIsKmg || bIsKtx)
        {
            ImageData = LoadKtxFormat(Filename, FormatInfo);
        }
        else
        {
            ImageData = LoadCommonFormat(Filename, FormatInfo);
        }

        ImageData.FormatInfo = FormatInfo;
        return ImageData;
    }

    FImageLoader::FImageData FImageLoader::LoadCommonFormat(std::string_view Filename, FFormatInfo FormatInfo)
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
        std::copy(static_cast<std::byte*>(PixelData), static_cast<std::byte*>(PixelData) + ImageData.Size, ImageData.Data.data());

        return ImageData;
    }

    FImageLoader::FImageData FImageLoader::LoadDdsFormat(std::string_view Filename, FFormatInfo FormatInfo)
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
        std::copy(static_cast<std::byte*>(PixelData), static_cast<std::byte*>(PixelData) + Size, ImageData.Data.data());

        return ImageData;
    }

    FImageLoader::FImageData FImageLoader::LoadExrFormat(std::string_view Filename, FFormatInfo FormatInfo)
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
                std::copy(reinterpret_cast<std::byte*>(Pixels.data()),
                          reinterpret_cast<std::byte*>(Pixels.data()) + ImageData.Size,
                          ImageData.Data.data());
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
                    std::copy(Pixels.begin(), Pixels.end(), LevelPixels.begin() + LevelOffsets[MipLevel]);
                }

                ImageData.LevelOffsets = std::move(LevelOffsets);
                std::copy(reinterpret_cast<std::byte*>(Pixels.data()),
                          reinterpret_cast<std::byte*>(Pixels.data()) + ImageData.Size,
                          ImageData.Data.data());
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
            std::copy(reinterpret_cast<std::byte*>(Pixels.data()),
                      reinterpret_cast<std::byte*>(Pixels.data()) + ImageData.Size,
                      ImageData.Data.data());
        }

        return ImageData;
    }

    FImageLoader::FImageData FImageLoader::LoadHdrFormat(std::string_view Filename, FFormatInfo FormatInfo)
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
            std::copy(reinterpret_cast<std::byte*>(PixelData),
                      reinterpret_cast<std::byte*>(PixelData) + ImageData.Size,
                      ImageData.Data.data());
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

    FImageLoader::FImageData FImageLoader::LoadKtxFormat(std::string_view Filename, FFormatInfo FormatInfo)
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
            std::copy(static_cast<std::byte*>(PixelData), static_cast<std::byte*>(PixelData) + Size, ImageData.Data.data());

            return ImageData;
        }

        ktxTexture2* Texture = nullptr;
        KTX_error_code Result = ktxTexture2_CreateFromNamedFile(Filename.data(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &Texture);
        if (Result != KTX_SUCCESS)
        {
            NpgsCoreError("Failed to load Khronos Texture image: \"{}\": \"{}\".", Filename.data(), ktxErrorString(Result));
            return {};
        }

        if (ktxTexture2_NeedsTranscoding(Texture))
        {
            ktx_transcode_fmt_e TranscodeFormat;
            if (FormatInfo.ComponentSize == 1)
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
        std::copy(reinterpret_cast<std::byte*>(PixelData), reinterpret_cast<std::byte*>(PixelData) + Size, ImageData.Data.data());

        return ImageData;
    }

    FTexture::FTexture(FVulkanContext* VulkanContext, VmaAllocator Allocator, const VmaAllocationCreateInfo* AllocationCreateInfo)
        : _VulkanContext(VulkanContext)
        , _Allocator(Allocator)
        , _AllocationCreateInfo(AllocationCreateInfo)
    {
    }

    FTexture::FTexture(FTexture&& Other) noexcept
        : _VulkanContext(std::exchange(Other._VulkanContext, nullptr))
        , _Allocator(std::exchange(Other._Allocator, nullptr))
        , _AllocationCreateInfo(std::exchange(Other._AllocationCreateInfo, {}))
    {
    }

    FTexture& FTexture::operator=(FTexture&& Other) noexcept
    {
        if (this != &Other)
        {
            _VulkanContext        = std::exchange(Other._VulkanContext, nullptr);
            _Allocator            = std::exchange(Other._Allocator, nullptr);
            _AllocationCreateInfo = std::exchange(Other._AllocationCreateInfo, {});
        }

        return *this;
    }

    void FTexture::CreateTexture(const FImageData& ImageData, vk::ImageCreateFlags Flags, vk::ImageType ImageType,
                                 vk::ImageViewType ImageViewType, vk::Format InitialFormat, vk::Format FinalFormat,
                                 std::uint32_t ArrayLayers, bool bGenerateMipmaps)
    {
        CreateTextureInternal(ImageData, Flags, ImageType, ImageViewType, InitialFormat, FinalFormat, ArrayLayers, bGenerateMipmaps);
    }

    void FTexture::CreateTextureInternal(const FImageData& ImageData, vk::ImageCreateFlags Flags, vk::ImageType ImageType,
                                         vk::ImageViewType ImageViewType, vk::Format InitialFormat, vk::Format FinalFormat,
                                         std::uint32_t ArrayLayers, bool bGenerateMipmaps)
    {
        auto StagingBuffer = _VulkanContext->AcquireStagingBuffer(ImageData.Size);
        StagingBuffer->SubmitBufferData(0, 0, ImageData.Size, ImageData.Data.data());

        vk::Extent3D  Extent          = ImageData.Extent;
        std::uint32_t MipLevels       = 0;
        bool          bImageMipmapped = ImageData.MipLevels != 0 ? true : false;
        if (bGenerateMipmaps)
        {
            MipLevels = ImageData.MipLevels != 0 ? ImageData.MipLevels : CalculateMipLevels(Extent);
        }
        else
        {
            MipLevels = 1;
        }

        CreateImageMemory(Flags, ImageType, InitialFormat, Extent, MipLevels, ArrayLayers);
        CreateImageView({}, ImageViewType, InitialFormat, MipLevels, ArrayLayers);

        if (InitialFormat == FinalFormat)
        {
            if (!bImageMipmapped)
            {
                CopyBlitGenerateTexture(*StagingBuffer->GetBuffer(), Extent, MipLevels, ArrayLayers, vk::Filter::eLinear,
                                        *_ImageMemory->GetResource(), *_ImageMemory->GetResource());
            }
            else
            {
                CopyBlitApplyTexture(*StagingBuffer->GetBuffer(), Extent, MipLevels, ImageData.LevelOffsets,
                                     ArrayLayers, vk::Filter::eLinear, *_ImageMemory->GetResource());
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
                .setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
                .setInitialLayout(vk::ImageLayout::eUndefined);

            FVulkanImage* ConvertedImage = StagingBuffer->CreateAliasedImage(InitialFormat, ImageCreateInfo);

            if (ConvertedImage)
            {
                if (!bImageMipmapped)
                {
                    BlitGenerateTexture(Extent, MipLevels, ArrayLayers, vk::Filter::eLinear, **ConvertedImage, *_ImageMemory->GetResource());
                }
                else
                {
                    BlitApplyTexture(Extent, MipLevels, ArrayLayers, vk::Filter::eLinear, **ConvertedImage, *_ImageMemory->GetResource());
                }
            }
            else
            {
                if (!bImageMipmapped)
                {
                    CopyBlitGenerateTexture(*StagingBuffer->GetBuffer(), Extent, MipLevels, ArrayLayers, vk::Filter::eLinear,
                                            *_ImageMemory->GetResource(), *_ImageMemory->GetResource());
                }
                else
                {
                    CopyBlitApplyTexture(*StagingBuffer->GetBuffer(), Extent, MipLevels, ImageData.LevelOffsets,
                                         ArrayLayers, vk::Filter::eLinear, *_ImageMemory->GetResource());
                }

                FVulkanImageMemory VanillaImageMemory(std::move(*_ImageMemory));
                FVulkanImageView VanillaImageView(std::move(*_ImageView));

                _ImageMemory.release();
                _ImageView.release();

                CreateImageMemory(Flags, ImageType, FinalFormat, Extent, MipLevels, ArrayLayers);
                CreateImageView({}, ImageViewType, FinalFormat, MipLevels, ArrayLayers);

                if (!bImageMipmapped)
                {
                    BlitGenerateTexture(Extent, MipLevels, ArrayLayers, vk::Filter::eLinear,
                                        *VanillaImageMemory.GetResource(), *_ImageMemory->GetResource());
                }
                else
                {
                    BlitApplyTexture(Extent, MipLevels, ArrayLayers, vk::Filter::eLinear,
                                     *VanillaImageMemory.GetResource(), *_ImageMemory->GetResource());
                }
            }
        }
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
            .setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
            .setInitialLayout(vk::ImageLayout::eUndefined);

        if (_Allocator != nullptr)
        {
            _ImageMemory = std::make_unique<FVulkanImageMemory>(
                _VulkanContext->GetDevice(), _Allocator, *_AllocationCreateInfo, ImageCreateInfo);
        }
        else
        {
            _ImageMemory = std::make_unique<FVulkanImageMemory>(
                _VulkanContext->GetDevice(), _VulkanContext->GetPhysicalDeviceProperties(),
                _VulkanContext->GetPhysicalDeviceMemoryProperties(), ImageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);
        }
    }

    void FTexture::CreateImageView(vk::ImageViewCreateFlags Flags, vk::ImageViewType ImageViewType, vk::Format Format,
                                   std::uint32_t MipLevels, std::uint32_t ArrayLayers)
    {
        vk::ImageSubresourceRange ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, MipLevels, 0, ArrayLayers);

        _ImageView = std::make_unique<FVulkanImageView>(
            _VulkanContext->GetDevice(), _ImageMemory->GetResource(), ImageViewType, Format, vk::ComponentMapping(), ImageSubresourceRange, Flags);
    }

    void FTexture::CopyBlitGenerateTexture(vk::Buffer SrcBuffer, vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                                           vk::Filter Filter, vk::Image DstImageSrcBlit, vk::Image DstImageDstBlit)
    {
        static constexpr std::array kPostTransferStates
        {
            FImageMemoryMaskPack(
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eShaderReadOnlyOptimal
            ),

            FImageMemoryMaskPack()
        };

        auto* ImageTracker = EngineServicesGetResourceServices->GetImageTracker();
        ImageTracker->TrackImage(DstImageSrcBlit, FImageTracker::FImageState());
        ImageTracker->TrackImage(DstImageDstBlit, FImageTracker::FImageState());

        bool bGenerateMipmaps = MipLevels > 1;
        bool bNeedBlit        = DstImageSrcBlit != DstImageDstBlit;

        auto  BufferGuard   = _VulkanContext->AcquireCommandBuffer(FVulkanContext::EQueueType::kGraphics);
        auto& CommandBuffer = *BufferGuard;
        CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, 0, 0, ArrayLayers);

        vk::BufferImageCopy Region = vk::BufferImageCopy()
            .setImageSubresource(Subresource)
            .setImageExtent(Extent);

        CopyBufferToImage(CommandBuffer, SrcBuffer, DstImageSrcBlit, kPostTransferStates[bGenerateMipmaps || bNeedBlit], Region);

        if (bNeedBlit)
        {
            std::array<vk::Offset3D, 2> Offsets;
            Offsets[1] = vk::Offset3D(static_cast<std::int32_t>(Extent.width),
                                      static_cast<std::int32_t>(Extent.height),
                                      static_cast<std::int32_t>(Extent.width));

            vk::ImageBlit Region(Subresource, Offsets, Subresource, Offsets);
            BlitImage(CommandBuffer, DstImageSrcBlit, kPostTransferStates[1], DstImageDstBlit, kPostTransferStates[bGenerateMipmaps], Region, Filter);
        }

        if (bGenerateMipmaps)
        {
            GenerateMipmaps(CommandBuffer, DstImageDstBlit, kPostTransferStates[0], Extent, MipLevels, ArrayLayers, Filter);
        }

        CommandBuffer.End();
        _VulkanContext->ExecuteCommands(FVulkanContext::EQueueType::kGraphics, CommandBuffer);
    }

    void FTexture::CopyBlitApplyTexture(vk::Buffer SrcBuffer, vk::Extent3D Extent, std::uint32_t MipLevels,
                                        const std::vector<std::size_t>& LevelOffsets,
                                        std::uint32_t ArrayLayers, vk::Filter Filter, vk::Image DstImage)
    {
        auto  BufferGuard   = _VulkanContext->AcquireCommandBuffer(FVulkanContext::EQueueType::kGraphics);
        auto& CommandBuffer = *BufferGuard;
        CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        FImageMemoryMaskPack PostTransferState(
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eShaderReadOnlyOptimal
        );

        auto* ImageTracker = EngineServicesGetResourceServices->GetImageTracker();

        std::vector<vk::BufferImageCopy> Regions;
        for (std::uint32_t MipLevel = 0; MipLevel != MipLevels; ++MipLevel)
        {
            vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, MipLevel, 0, ArrayLayers);

            vk::Offset3D MipmapOffset = MipmapExtent(Extent, MipLevel);
            vk::BufferImageCopy Region = vk::BufferImageCopy()
                .setBufferOffset(LevelOffsets[MipLevel])
                .setImageSubresource(Subresource)
                .setImageExtent(vk::Extent3D(MipmapOffset.x, MipmapOffset.y, MipmapOffset.z));

            Regions.push_back(std::move(Region));
        }

        ImageTracker->TrackImage(DstImage, FImageTracker::FImageState());
        CopyBufferToImage(CommandBuffer, SrcBuffer, DstImage, PostTransferState, Regions);

        CommandBuffer.End();
        _VulkanContext->ExecuteCommands(FVulkanContext::EQueueType::kGraphics, CommandBuffer);
    }

    void FTexture::BlitGenerateTexture(vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                                       vk::Filter Filter, vk::Image SrcImage, vk::Image DstImage)
    {
        static constexpr std::array kPostTransferStates
        {
            FImageMemoryMaskPack(
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eShaderReadOnlyOptimal
            ),

            FImageMemoryMaskPack()
        };

        auto* ImageTracker = EngineServicesGetResourceServices->GetImageTracker();
        if (!ImageTracker->IsExisting(SrcImage))
        {
            ImageTracker->TrackImage(SrcImage, FImageTracker::FImageState());
        }
        if (!ImageTracker->IsExisting(DstImage))
        {
            ImageTracker->TrackImage(DstImage, FImageTracker::FImageState());
        }

        bool bGenerateMipmaps = MipLevels > 1;
        bool bNeedBlit = SrcImage != DstImage;

        auto  BufferGuard   = _VulkanContext->AcquireCommandBuffer(FVulkanContext::EQueueType::kGraphics);
        auto& CommandBuffer = *BufferGuard;
        CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        if (bNeedBlit)
        {
            vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, 0, 0, ArrayLayers);
            std::array<vk::Offset3D, 2> Offsets;
            Offsets[1] = vk::Offset3D(static_cast<std::int32_t>(Extent.width),
                                      static_cast<std::int32_t>(Extent.height),
                                      static_cast<std::int32_t>(Extent.depth));

            vk::ImageBlit Region(Subresource, Offsets, Subresource, Offsets);
            BlitImage(CommandBuffer, SrcImage, kPostTransferStates[bGenerateMipmaps], DstImage, kPostTransferStates[bGenerateMipmaps], Region, Filter);
        }

        if (bGenerateMipmaps)
        {
            GenerateMipmaps(CommandBuffer, DstImage, kPostTransferStates[0], Extent, MipLevels, ArrayLayers, Filter);
        }

        CommandBuffer.End();
        _VulkanContext->ExecuteCommands(FVulkanContext::EQueueType::kGraphics, CommandBuffer);
    }

    void FTexture::BlitApplyTexture(vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                                    vk::Filter Filter, vk::Image SrcImage, vk::Image DstImage)
    {
        auto* ImageTracker = EngineServicesGetResourceServices->GetImageTracker();
        if (!ImageTracker->IsExisting(SrcImage))
        {
            ImageTracker->TrackImage(SrcImage, FImageTracker::FImageState());
        }
        if (!ImageTracker->IsExisting(DstImage))
        {
            ImageTracker->TrackImage(DstImage, FImageTracker::FImageState());
        }

        auto  BufferGuard   = _VulkanContext->AcquireCommandBuffer(FVulkanContext::EQueueType::kGraphics);
        auto& CommandBuffer = *BufferGuard;
        CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        std::vector<vk::ImageBlit> Regions;
        for (std::uint32_t MipLevel = 0; MipLevel != MipLevels; ++MipLevel)
        {
            vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, MipLevel, 0, ArrayLayers);
            vk::Offset3D MipmapOffset = MipmapExtent(Extent, MipLevel);
            std::array<vk::Offset3D, 2> Offsets;
            Offsets[1] = vk::Offset3D(MipmapOffset);

            Regions.emplace_back(Subresource, Offsets, Subresource, Offsets);
        }

        FImageMemoryMaskPack PostTransferState(
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eShaderReadOnlyOptimal
        );

        BlitImage(CommandBuffer, SrcImage, PostTransferState, DstImage, PostTransferState, Regions, Filter);

        CommandBuffer.End();
        _VulkanContext->ExecuteCommands(FVulkanContext::EQueueType::kGraphics, CommandBuffer);
    }

    void FTexture::CopyBufferToImage(const FVulkanCommandBuffer& CommandBuffer,
                                     vk::Buffer SrcBuffer, vk::Image DstImage,
                                     const FImageMemoryMaskPack& PostTransferState,
                                     const vk::ArrayProxy<vk::BufferImageCopy>& Regions)
    {
        vk::BufferImageCopy Region = *(Regions.begin());
        vk::ImageSubresourceRange ImageSubresourceRange(
            Region.imageSubresource.aspectMask,
            Region.imageSubresource.mipLevel,
            Regions.size(),
            Region.imageSubresource.baseArrayLayer,
            Region.imageSubresource.layerCount
        );

        auto* ImageTracker = EngineServicesGetResourceServices->GetImageTracker();
        auto  ImageState   = ImageTracker->GetImageState(DstImage, ImageSubresourceRange);

        vk::ImageMemoryBarrier2 InitDstImageBarrier(
            ImageState.StageMask,
            ImageState.AccessMask,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            ImageState.ImageLayout,
            vk::ImageLayout::eTransferDstOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            DstImage,
            ImageSubresourceRange
        );

        ImageState.StageMask   = vk::PipelineStageFlagBits2::eTransfer;
        ImageState.AccessMask  = vk::AccessFlagBits2::eTransferWrite;
        ImageState.ImageLayout = vk::ImageLayout::eTransferDstOptimal;
        ImageTracker->FlushImageAllStates(DstImage, ImageState);

        vk::DependencyInfo CopyDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(InitDstImageBarrier);

        CommandBuffer->pipelineBarrier2(CopyDependencyInfo);
        CommandBuffer->copyBufferToImage(SrcBuffer, DstImage, vk::ImageLayout::eTransferDstOptimal, Regions);

        if (PostTransferState.kbEnable)
        {
            InitDstImageBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
                .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
                .setDstStageMask(PostTransferState.kStageMask)
                .setDstAccessMask(PostTransferState.kAccessMask)
                .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                .setNewLayout(PostTransferState.kImageLayout);

            ImageTracker->FlushImageAllStates(DstImage, PostTransferState);

            CopyDependencyInfo.setImageMemoryBarriers(InitDstImageBarrier);
            CommandBuffer->pipelineBarrier2(CopyDependencyInfo);
        }
    }

    void FTexture::BlitImage(const FVulkanCommandBuffer& CommandBuffer,
                             vk::Image SrcImage, const FImageMemoryMaskPack& SrcPostTransferState,
                             vk::Image DstImage, const FImageMemoryMaskPack& DstPostTransferState,
                             const vk::ArrayProxy<vk::ImageBlit>& Regions, vk::Filter Filter)
    {
        vk::ImageBlit Region = *(Regions.begin());
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

        auto* ImageTracker = EngineServicesGetResourceServices->GetImageTracker();
        auto SrcImageState = ImageTracker->GetImageState(SrcImage, SrcImageSubresourceRange);
        auto DstImageState = ImageTracker->GetImageState(DstImage, DstImageSubresourceRange);

        vk::ImageMemoryBarrier2 SrcPreTransferBarrier(
            SrcImageState.StageMask,
            SrcImageState.AccessMask,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferRead,
            SrcImageState.ImageLayout,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            SrcImage,
            SrcImageSubresourceRange
        );

        SrcImageState.StageMask   = vk::PipelineStageFlagBits2::eTransfer;
        SrcImageState.AccessMask  = vk::AccessFlagBits2::eTransferRead;
        SrcImageState.ImageLayout = vk::ImageLayout::eTransferSrcOptimal;
        ImageTracker->TrackImage(SrcImage, SrcImageSubresourceRange, SrcImageState);

        vk::ImageMemoryBarrier2 DstPreTransferBarrier(
            DstImageState.StageMask,
            DstImageState.AccessMask,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            DstImageState.ImageLayout,
            vk::ImageLayout::eTransferDstOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            DstImage,
            DstImageSubresourceRange
        );

        DstImageState.StageMask   = vk::PipelineStageFlagBits2::eTransfer;
        DstImageState.AccessMask  = vk::AccessFlagBits2::eTransferWrite;
        DstImageState.ImageLayout = vk::ImageLayout::eTransferDstOptimal;
        ImageTracker->TrackImage(DstImage, DstImageSubresourceRange, DstImageState);

        std::array PreTransferBarriers{ SrcPreTransferBarrier, DstPreTransferBarrier };

        vk::DependencyInfo PreTransferDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(PreTransferBarriers);

        CommandBuffer->pipelineBarrier2(PreTransferDependencyInfo);
        CommandBuffer->blitImage(SrcImage, vk::ImageLayout::eTransferSrcOptimal,
                                 DstImage, vk::ImageLayout::eTransferDstOptimal, Regions, Filter);

        if (SrcPostTransferState.kbEnable || DstPostTransferState.kbEnable)
        {
            std::vector<vk::ImageMemoryBarrier2> PostTransferBarriers;

            if (SrcPostTransferState.kbEnable)
            {
                PostTransferBarriers.emplace_back(
                    vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferRead,
                    SrcPostTransferState.kStageMask,
                    SrcPostTransferState.kAccessMask,
                    vk::ImageLayout::eTransferSrcOptimal,
                    SrcPostTransferState.kImageLayout,
                    vk::QueueFamilyIgnored,
                    vk::QueueFamilyIgnored,
                    SrcImage,
                    SrcImageSubresourceRange
                );

                ImageTracker->TrackImage(SrcImage, SrcImageSubresourceRange, SrcPostTransferState);
            }

            if (DstPostTransferState.kbEnable)
            {
                PostTransferBarriers.emplace_back(
                    vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferWrite,
                    DstPostTransferState.kStageMask,
                    DstPostTransferState.kAccessMask,
                    vk::ImageLayout::eTransferDstOptimal,
                    DstPostTransferState.kImageLayout,
                    vk::QueueFamilyIgnored,
                    vk::QueueFamilyIgnored,
                    DstImage,
                    DstImageSubresourceRange
                );

                ImageTracker->TrackImage(DstImage, DstImageSubresourceRange, DstPostTransferState);
            }

            vk::DependencyInfo PostTransferDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PostTransferBarriers);

            CommandBuffer->pipelineBarrier2(PostTransferDependencyInfo);
        }
    }

    void FTexture::GenerateMipmaps(const FVulkanCommandBuffer& CommandBuffer, vk::Image Image, const FImageMemoryMaskPack& FinalState,
                                   vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers, vk::Filter Filter)
    {
        FImageMemoryMaskPack PostTransferStages;

        for (std::uint32_t MipLevel = 1; MipLevel != MipLevels; ++MipLevel)
        {
            vk::ImageBlit Region(
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel - 1, 0, ArrayLayers),
                { vk::Offset3D(), MipmapExtent(Extent, MipLevel - 1) },
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel, 0, ArrayLayers),
                { vk::Offset3D(), MipmapExtent(Extent, MipLevel) }
            );

            BlitImage(CommandBuffer, Image, PostTransferStages, Image, PostTransferStages, Region, Filter);
        }

        if (FinalState.kbEnable)
        {
            vk::ImageSubresourceRange PartMipRange(vk::ImageAspectFlagBits::eColor, 0, MipLevels - 1, 0, ArrayLayers);
            vk::ImageSubresourceRange LastMipRange(vk::ImageAspectFlagBits::eColor, MipLevels - 1, 1, 0, ArrayLayers);

            vk::ImageMemoryBarrier2 PartFinalBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                FinalState.kStageMask,
                FinalState.kAccessMask,
                vk::ImageLayout::eTransferSrcOptimal,
                FinalState.kImageLayout,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                Image,
                PartMipRange
            );

            vk::ImageMemoryBarrier2 LastFinalBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                FinalState.kStageMask,
                FinalState.kAccessMask,
                vk::ImageLayout::eTransferDstOptimal,
                FinalState.kImageLayout,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                Image,
                LastMipRange
            );

            auto* ImageTracker = EngineServicesGetResourceServices->GetImageTracker();
            ImageTracker->FlushImageAllStates(Image, FinalState);

            std::array FinalBarriers{ PartFinalBarrier, LastFinalBarrier };

            vk::DependencyInfo FinalDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(FinalBarriers);

            CommandBuffer->pipelineBarrier2(FinalDependencyInfo);
        }
    }

    FTexture2D::FTexture2D(FVulkanContext* VulkanContext, std::string_view Filename, vk::Format InitialFormat,
                           vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
        : Base(VulkanContext, nullptr, nullptr)
    {
        CreateTexture(GetAssetFullPath(EAssetType::kTexture, Filename.data()), InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
    }

    FTexture2D::FTexture2D(FVulkanContext* VulkanContext, VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo,
                           std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
        : Base(VulkanContext, Allocator, &AllocationCreateInfo)
    {
        CreateTexture(GetAssetFullPath(EAssetType::kTexture, Filename.data()), InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
    }

    FTexture2D::FTexture2D(FTexture2D&& Other) noexcept
        : Base(std::move(Other))
        , _ImageExtent(std::exchange(Other._ImageExtent, {}))
    {
    }

    FTexture2D& FTexture2D::operator=(FTexture2D&& Other) noexcept
    {
        if (this != &Other)
        {
            Base::operator=(std::move(Other));
            _ImageExtent = std::exchange(Other._ImageExtent, {});
        }

        return *this;
    }

    void FTexture2D::CreateTexture(std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                                   vk::ImageCreateFlags Flags, bool bGenreteMipmaps)
    {
        FImageData ImageData = _ImageLoader->LoadImage(Filename, InitialFormat);
        CreateTexture(ImageData, InitialFormat, FinalFormat, Flags, bGenreteMipmaps);
    }

    void FTexture2D::CreateTexture(const FImageData& ImageData, vk::Format InitialFormat, vk::Format FinalFormat,
                                   vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    {
        _ImageExtent = vk::Extent2D(ImageData.Extent.width, ImageData.Extent.height);
        Base::CreateTexture(ImageData, Flags, vk::ImageType::e2D, vk::ImageViewType::e2D, InitialFormat, FinalFormat, 1, bGenerateMipmaps);
    }

    FTextureCube::FTextureCube(FVulkanContext* VulkanContext, std::string_view Filename, vk::Format InitialFormat,
                               vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
        : FTextureCube(VulkanContext, nullptr, {}, Filename, InitialFormat, FinalFormat, Flags, bGenerateMipmaps)
    {
    }

    FTextureCube::FTextureCube(FVulkanContext* VulkanContext, VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, std::string_view Filename,
                               vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
        : Base(VulkanContext, Allocator, &AllocationCreateInfo)
    {
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
            CreateCubemap(GetAssetFullPath(EAssetType::kTexture, Filename.data()), InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
        }
    }

    FTextureCube::FTextureCube(FTextureCube&& Other) noexcept
        : Base(std::move(Other))
        , _ImageExtent(std::exchange(Other._ImageExtent, {}))
    {
    }

    FTextureCube& FTextureCube::operator=(FTextureCube&& Other) noexcept
    {
        if (this != &Other)
        {
            Base::operator=(std::move(Other));
            _ImageExtent = std::exchange(Other._ImageExtent, {});
        }

        return *this;
    }

    void FTextureCube::CreateCubemap(std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                                     vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    {
        FImageData ImageData = _ImageLoader->LoadImage(Filename, InitialFormat);
        CreateCubemap(ImageData, InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
    }

    void FTextureCube::CreateCubemap(const std::array<std::string, 6>& Filenames, vk::Format InitialFormat,
                                     vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    {
        std::array<FImageData, 6> FaceImages;

        for (int i = 0; i != 6; ++i)
        {
            FaceImages[i] = _ImageLoader->LoadImage(GetAssetFullPath(EAssetType::kTexture, Filenames[i]), InitialFormat);

            if (i == 0)
            {
                _ImageExtent = vk::Extent2D(FaceImages[i].Extent.width, FaceImages[i].Extent.height);
            }
            else if (FaceImages[i].Extent.width != _ImageExtent.width || FaceImages[i].Extent.height != _ImageExtent.height)
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
        CubemapImageData.Extent = vk::Extent3D(_ImageExtent.width, _ImageExtent.height, 1);
        CubemapImageData.Data   = std::move(CubemapData);

        CreateCubemap(CubemapImageData, InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
    }

    void FTextureCube::CreateCubemap(const FImageData& ImageData, vk::Format InitialFormat, vk::Format FinalFormat,
                                     vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    {
        _ImageExtent = vk::Extent2D(ImageData.Extent.width, ImageData.Extent.height);
        vk::ImageCreateFlags CubeFlags = Flags | vk::ImageCreateFlagBits::eCubeCompatible;

        Base::CreateTexture(ImageData, CubeFlags, vk::ImageType::e2D, vk::ImageViewType::eCube, InitialFormat, FinalFormat, 6, bGenerateMipmaps);
    }
} // namespace Npgs
