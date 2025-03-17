#include "Texture.h"

#include <cmath>
#include <algorithm>
#include <exception>
#include <filesystem>
#include <format>
#include <ranges>
#include <type_traits>
#include <utility>

#include <gli/gli.hpp>
#include <Imath/ImathBox.h>
#include <ktx.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfTiledRgbaFile.h>
#include <stb_image.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Base/Assert.h"
#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

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

    Graphics::FFormatInfo FormatInfo(ImageFormat);
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

FImageLoader::FImageData FImageLoader::LoadCommonFormat(std::string_view Filename, Graphics::FFormatInfo FormatInfo)
{
    int   Width     = 0;
    int   Height    = 0;
    int   Channels  = 0;
    void* PixelData = nullptr;

    if (FormatInfo.RawDataType == Graphics::FFormatInfo::ERawDataType::kInteger)
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

FImageLoader::FImageData FImageLoader::LoadDdsFormat(std::string_view Filename, Graphics::FFormatInfo FormatInfo)
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

FImageLoader::FImageData FImageLoader::LoadExrFormat(std::string_view Filename, Graphics::FFormatInfo FormatInfo)
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
            int MipLevels       = InputFile.numLevels();
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

FImageLoader::FImageData FImageLoader::LoadHdrFormat(std::string_view Filename, Graphics::FFormatInfo FormatInfo)
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

FImageLoader::FImageData FImageLoader::LoadKtxFormat(std::string_view Filename, Graphics::FFormatInfo FormatInfo)
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

FTextureBase::FTextureBase(VmaAllocator Allocator, const VmaAllocationCreateInfo* AllocationCreateInfo)
    : _Allocator(Allocator), _AllocationCreateInfo(AllocationCreateInfo)
{
}

void FTextureBase::CreateTexture(const FImageData& ImageData, vk::Format InitialFormat, vk::Format FinalFormat,
                                 vk::ImageCreateFlags Flags, vk::ImageType ImageType, vk::ImageViewType ImageViewType,
                                 std::uint32_t ArrayLayers, bool bGenerateMipmaps)
{
    VmaAllocationCreateInfo StagingCreateInfo{ .usage = VMA_MEMORY_USAGE_CPU_TO_GPU };
    VmaAllocationCreateInfo* AllocationCreateInfo = _Allocator ? &StagingCreateInfo : nullptr;

    auto* StagingBufferPool = Graphics::FStagingBufferPool::GetInstance();
    auto* StagingBuffer     = StagingBufferPool->AcquireBuffer(ImageData.Size, AllocationCreateInfo);
    StagingBuffer->SubmitBufferData(0, 0, ImageData.Size, ImageData.Data.data());

    CreateTextureInternal(StagingBuffer, InitialFormat, FinalFormat, ImageType, ImageViewType,
                          ImageData.Extent, Flags, ArrayLayers, bGenerateMipmaps);

    StagingBufferPool->ReleaseBuffer(StagingBuffer);
}

void FTextureBase::CreateTextureInternal(Graphics::FStagingBuffer* StagingBuffer, vk::Format InitialFormat, vk::Format FinalFormat,
                                         vk::ImageType ImageType, vk::ImageViewType ImageViewType, vk::Extent3D Extent,
                                         vk::ImageCreateFlags Flags, std::uint32_t ArrayLayers, bool bGenerateMipmaps)
{

    std::uint32_t MipLevels = bGenerateMipmaps ? CalculateMipLevels(Extent) : 1;
    CreateImageMemory(ImageType, InitialFormat, Extent, MipLevels, ArrayLayers, Flags);
    CreateImageView(ImageViewType, InitialFormat, MipLevels, ArrayLayers);

    if (InitialFormat == FinalFormat)
    {
        CopyBlitGenerateTexture(*StagingBuffer->GetBuffer(), Extent, MipLevels, ArrayLayers, vk::Filter::eLinear,
                                *_ImageMemory->GetResource(), *_ImageMemory->GetResource());
    }
    else
    {
        Graphics::FVulkanImage* ConvertedImage = StagingBuffer->CreateAliasedImage(InitialFormat, FinalFormat, Extent);

        if (ConvertedImage)
        {
            BlitGenerateTexture(**ConvertedImage, Extent, MipLevels, ArrayLayers, vk::Filter::eLinear, *_ImageMemory->GetResource());
        }
        else
        {
            CopyBlitGenerateTexture(*StagingBuffer->GetBuffer(), Extent, MipLevels, ArrayLayers, vk::Filter::eLinear,
                                    *_ImageMemory->GetResource(), *_ImageMemory->GetResource());

            Graphics::FVulkanImageMemory VanillaImageMemory(std::move(*_ImageMemory));
            Graphics::FVulkanImageView VanillaImageView(std::move(*_ImageView));

            _ImageMemory.release();
            _ImageView.release();

            CreateImageMemory(ImageType, FinalFormat, Extent, MipLevels, ArrayLayers, Flags);
            CreateImageView(ImageViewType, FinalFormat, MipLevels, ArrayLayers);

            BlitGenerateTexture(*VanillaImageMemory.GetResource(), Extent, MipLevels, ArrayLayers, vk::Filter::eLinear, *_ImageMemory->GetResource());
        }
    }
}

void FTextureBase::CreateImageMemory(vk::ImageType ImageType, vk::Format Format, vk::Extent3D Extent,
                                     std::uint32_t MipLevels, std::uint32_t ArrayLayers, vk::ImageCreateFlags Flags)
{
    vk::ImageCreateInfo ImageCreateInfo = vk::ImageCreateInfo()
        .setFlags(Flags)
        .setImageType(ImageType)
        .setFormat(Format)
        .setExtent(Extent)
        .setMipLevels(MipLevels)
        .setArrayLayers(ArrayLayers)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);

    if (_Allocator != nullptr)
    {
        _ImageMemory = std::make_unique<Graphics::FVulkanImageMemory>(_Allocator, *_AllocationCreateInfo, ImageCreateInfo);
    }
    else
    {
        _ImageMemory = std::make_unique<Graphics::FVulkanImageMemory>(ImageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);
    }
}

void FTextureBase::CreateImageView(vk::ImageViewType ImageViewType, vk::Format Format, std::uint32_t MipLevels,
                                   std::uint32_t ArrayLayers, vk::ImageViewCreateFlags Flags)
{
    vk::ImageSubresourceRange ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, MipLevels, 0, ArrayLayers);

    _ImageView = std::make_unique<Graphics::FVulkanImageView>(_ImageMemory->GetResource(), ImageViewType, Format,
                                                              vk::ComponentMapping(), ImageSubresourceRange, Flags);

}

void FTextureBase::CopyBlitGenerateTexture(vk::Buffer SrcBuffer, vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                                           vk::Filter Filter, vk::Image DstImageSrcBlit, vk::Image DstImageDstBlit)
{
    static constexpr std::array kPostTransferStages
    {
        Graphics::FImageMemoryBarrierStagePack(
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eShaderReadOnlyOptimal
        ),

        Graphics::FImageMemoryBarrierStagePack()
    };

    bool bGenerateMipmaps = MipLevels > 1;
    bool bNeedBlit        = DstImageSrcBlit != DstImageDstBlit;

    auto* VulkanContext = Graphics::FVulkanContext::GetClassInstance();
    auto& CommandBuffer = VulkanContext->GetTransferCommandBuffer();
    CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, 0, 0, ArrayLayers);
    vk::Extent3D Extent3D = { Extent.width, Extent.height, 1 };

    vk::BufferImageCopy BufferImageCopy = vk::BufferImageCopy()
        .setImageSubresource(Subresource)
        .setImageExtent(Extent3D);

    Graphics::FImageMemoryBarrierStagePack CopyPreTransferStages(
        vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::AccessFlagBits2::eNone,
        vk::ImageLayout::eUndefined
    );

    CopyBufferToImage(CommandBuffer, SrcBuffer, CopyPreTransferStages, kPostTransferStages[bGenerateMipmaps || bNeedBlit], BufferImageCopy, DstImageSrcBlit);

    if (bNeedBlit)
    {
        vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, 0, 0, ArrayLayers);
        std::array<vk::Offset3D, 2> Offsets;
        Offsets[1] = vk::Offset3D(static_cast<std::int32_t>(Extent.width),
                                  static_cast<std::int32_t>(Extent.height),
                                  static_cast<std::int32_t>(Extent.width));

        vk::ImageBlit Region(Subresource, Offsets, Subresource, Offsets);

        Graphics::FImageMemoryBarrierStagePack BlitPreTransferStages(
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eUndefined
        );

        BlitImage(CommandBuffer, DstImageSrcBlit, BlitPreTransferStages, kPostTransferStages[bGenerateMipmaps], Region, Filter, DstImageDstBlit);
    }

    if (bGenerateMipmaps)
    {
        GenerateMipmaps(CommandBuffer, DstImageDstBlit, Extent, MipLevels, ArrayLayers, Filter, kPostTransferStages[0]);
    }

    CommandBuffer.End();
    VulkanContext->ExecuteGraphicsCommands(CommandBuffer);
}

void FTextureBase::BlitGenerateTexture(vk::Image SrcImage, vk::Extent3D Extent, std::uint32_t MipLevels,
                                       std::uint32_t ArrayLayers, vk::Filter Filter, vk::Image DstImage)
{
    static constexpr std::array kPostTransferStages
    {
        Graphics::FImageMemoryBarrierStagePack(
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eShaderReadOnlyOptimal
        ),

        Graphics::FImageMemoryBarrierStagePack()
    };

    bool bGenerateMipmaps = MipLevels > 1;
    bool bNeedBlit        = SrcImage != DstImage;
    if (bGenerateMipmaps || bNeedBlit)
    {
        auto* VulkanContext = Graphics::FVulkanContext::GetClassInstance();
        auto& CommandBuffer = VulkanContext->GetTransferCommandBuffer();
        CommandBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        if (bNeedBlit)
        {
            vk::ImageSubresourceLayers Subresource(vk::ImageAspectFlagBits::eColor, 0, 0, ArrayLayers);
            std::array<vk::Offset3D, 2> Offsets;
            Offsets[1] = vk::Offset3D(static_cast<std::int32_t>(Extent.width),
                                      static_cast<std::int32_t>(Extent.height),
                                      static_cast<std::int32_t>(Extent.depth));

            vk::ImageBlit Region(Subresource, Offsets, Subresource, Offsets);

            Graphics::FImageMemoryBarrierStagePack BlitPreTransferStages(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::ImageLayout::eUndefined
            );

            BlitImage(CommandBuffer, SrcImage, BlitPreTransferStages, kPostTransferStages[bGenerateMipmaps], Region, Filter, DstImage);
        }

        if (bGenerateMipmaps)
        {
            GenerateMipmaps(CommandBuffer, DstImage, Extent, MipLevels, ArrayLayers, Filter, kPostTransferStages[0]);
        }

        CommandBuffer.End();
        VulkanContext->ExecuteGraphicsCommands(CommandBuffer);
    }
}

void FTextureBase::CopyBufferToImage(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Buffer SrcBuffer,
                                     const Graphics::FImageMemoryBarrierStagePack& PreTransferStages,
                                     const Graphics::FImageMemoryBarrierStagePack& PostTransferStages,
                                     const vk::BufferImageCopy& Region, vk::Image DstImage)
{
    vk::ImageSubresourceRange ImageSubresourceRange(
        Region.imageSubresource.aspectMask,
        Region.imageSubresource.mipLevel,
        1,
        Region.imageSubresource.baseArrayLayer,
        Region.imageSubresource.layerCount
    );

    vk::ImageMemoryBarrier2 Barrier;
    vk::DependencyInfo DependencyInfo = vk::DependencyInfo()
        .setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    if (PreTransferStages.kbEnable)
    {
        Barrier.setSrcStageMask(PreTransferStages.kPipelineStageFlags)
               .setSrcAccessMask(PreTransferStages.kAccessFlags)
               .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer)
               .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
               .setOldLayout(PreTransferStages.kImageLayout)
               .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
               .setImage(DstImage)
               .setSubresourceRange(ImageSubresourceRange);

        DependencyInfo.setImageMemoryBarriers(Barrier);
        CommandBuffer->pipelineBarrier2(DependencyInfo);
    }

    CommandBuffer->copyBufferToImage(SrcBuffer, DstImage, vk::ImageLayout::eTransferDstOptimal, Region);

    if (PostTransferStages.kbEnable)
    {
        Barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
               .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
               .setDstStageMask(PostTransferStages.kPipelineStageFlags)
               .setDstAccessMask(PostTransferStages.kAccessFlags)
               .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
               .setNewLayout(PostTransferStages.kImageLayout);

        CommandBuffer->pipelineBarrier2(DependencyInfo);
    }
}

void FTextureBase::BlitImage(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Image SrcImage,
                             const Graphics::FImageMemoryBarrierStagePack& PreTransferStages,
                             const Graphics::FImageMemoryBarrierStagePack& PostTransferStages,
                             const vk::ImageBlit& Region, vk::Filter Filter, vk::Image DstImage)
{
    vk::ImageSubresourceRange SrcImageSubresourceRange(
        Region.srcSubresource.aspectMask,
        Region.srcSubresource.mipLevel,
        1,
        Region.srcSubresource.baseArrayLayer,
        Region.srcSubresource.layerCount
    );

    vk::ImageSubresourceRange DstImageSubresourceRange(
        Region.dstSubresource.aspectMask,
        Region.dstSubresource.mipLevel,
        1,
        Region.dstSubresource.baseArrayLayer,
        Region.dstSubresource.layerCount
    );

    if (PreTransferStages.kbEnable)
    {
        vk::ImageMemoryBarrier2 ConvertToTransferSrcBarrier(
            PreTransferStages.kPipelineStageFlags,
            PreTransferStages.kAccessFlags,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferRead,
            PreTransferStages.kImageLayout,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            SrcImage,
            SrcImageSubresourceRange
        );

        vk::ImageMemoryBarrier2 ConvertToTransferDstBarrier(
            PreTransferStages.kPipelineStageFlags,
            PreTransferStages.kAccessFlags,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            PreTransferStages.kImageLayout,
            vk::ImageLayout::eTransferDstOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            DstImage,
            DstImageSubresourceRange
        );

        std::array InitTransferBarriers{ ConvertToTransferSrcBarrier, ConvertToTransferDstBarrier };

        vk::DependencyInfo InitTransferDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(InitTransferBarriers);

        CommandBuffer->pipelineBarrier2(InitTransferDependencyInfo);
    }

    CommandBuffer->blitImage(SrcImage, vk::ImageLayout::eTransferSrcOptimal,
                             DstImage, vk::ImageLayout::eTransferDstOptimal, Region, Filter);

    if (PostTransferStages.kbEnable)
    {
        vk::ImageMemoryBarrier2 FinalBarrier(
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            PostTransferStages.kPipelineStageFlags,
            PostTransferStages.kAccessFlags,
            vk::ImageLayout::eTransferDstOptimal,
            PostTransferStages.kImageLayout,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            DstImage,
            DstImageSubresourceRange
        );

        vk::DependencyInfo FinalDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(FinalBarrier);

        CommandBuffer->pipelineBarrier2(FinalDependencyInfo);
    }
}

void FTextureBase::GenerateMipmaps(const Graphics::FVulkanCommandBuffer& CommandBuffer, vk::Image Image,
                                   vk::Extent3D Extent, std::uint32_t MipLevels, std::uint32_t ArrayLayers,
                                   vk::Filter Filter, const Graphics::FImageMemoryBarrierStagePack& FinalStages)
{
    if (ArrayLayers > 1)
    {
        std::vector<vk::ImageBlit> Regions(ArrayLayers);
        vk::ImageSubresourceRange InitialMipRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, ArrayLayers);

        vk::ImageMemoryBarrier2 InitImageBarrier(
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferRead,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            Image,
            InitialMipRange
        );

        vk::DependencyInfo InitDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(InitImageBarrier);

        CommandBuffer->pipelineBarrier2(InitDependencyInfo);

        for (std::uint32_t MipLevel = 1; MipLevel != MipLevels; ++MipLevel)
        {
            vk::Offset3D SrcExtent = MipmapExtent(Extent, MipLevel - 1);
            vk::Offset3D DstExtent = MipmapExtent(Extent, MipLevel);

            vk::ImageSubresourceRange CurrentMipRange(vk::ImageAspectFlagBits::eColor, MipLevel, 1, 0, ArrayLayers);

            vk::ImageMemoryBarrier2 ConvertToTransferDstBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                Image,
                CurrentMipRange
            );

            vk::DependencyInfo ConvertToTransferDstDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(ConvertToTransferDstBarrier);

            CommandBuffer->pipelineBarrier2(ConvertToTransferDstDependencyInfo);

            for (std::uint32_t Layer = 0; Layer != ArrayLayers; ++Layer)
            {
                Regions[Layer] = vk::ImageBlit(
                    vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel - 1, Layer, 1),
                    { vk::Offset3D(0, 0, 0), SrcExtent },
                    vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel,     Layer, 1),
                    { vk::Offset3D(0, 0, 0), DstExtent }
                );
            }

            CommandBuffer->blitImage(Image, vk::ImageLayout::eTransferSrcOptimal,
                                     Image, vk::ImageLayout::eTransferDstOptimal, Regions, Filter);

            vk::ImageMemoryBarrier2 ConvertToTransferSrcBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                Image,
                CurrentMipRange
            );

            vk::DependencyInfo ConvertToTransferSrcDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(ConvertToTransferSrcBarrier);

            CommandBuffer->pipelineBarrier2(ConvertToTransferSrcDependencyInfo);
        }
    }
    else
    {
        Graphics::FImageMemoryBarrierStagePack PreTransferStages(
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eUndefined
        );

        Graphics::FImageMemoryBarrierStagePack PostTransferStages(
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferRead,
            vk::ImageLayout::eTransferSrcOptimal
        );

        for (std::uint32_t MipLevel = 1; MipLevel != MipLevels; ++MipLevel)
        {
            vk::ImageBlit Region(
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel - 1, 0, 1),
                { vk::Offset3D(), MipmapExtent(Extent, MipLevel - 1) },
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, MipLevel,     0, 1),
                { vk::Offset3D(), MipmapExtent(Extent, MipLevel) }
            );

            BlitImage(CommandBuffer, Image, PreTransferStages, PostTransferStages, Region, Filter, Image);
        }
    }

    if (FinalStages.kbEnable)
    {
        vk::ImageSubresourceRange FinalMipRange(vk::ImageAspectFlagBits::eColor, 0, MipLevels, 0, ArrayLayers);

        vk::ImageMemoryBarrier2 FinalBarrier(
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eNone,
            FinalStages.kPipelineStageFlags,
            FinalStages.kAccessFlags,
            vk::ImageLayout::eTransferSrcOptimal,
            FinalStages.kImageLayout,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            Image,
            FinalMipRange
        );

        vk::DependencyInfo FinalDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(FinalBarrier);

        CommandBuffer->pipelineBarrier2(FinalDependencyInfo);
    }
}

FTexture2D::FTexture2D(std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                       vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    : Base(nullptr, nullptr)
{
    CreateTexture(GetAssetFullPath(EAssetType::kTexture, Filename.data()),
                  InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
}

FTexture2D::FTexture2D(const VmaAllocationCreateInfo& AllocationCreateInfo, std::string_view Filename,
                       vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    : FTexture2D(Graphics::FVulkanContext::GetClassInstance()->GetVmaAllocator(),
                 AllocationCreateInfo, Filename, InitialFormat, FinalFormat, Flags, bGenerateMipmaps)
{
}

FTexture2D::FTexture2D(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, std::string_view Filename,
                       vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    : Base(Allocator, &AllocationCreateInfo)
{
    CreateTexture(GetAssetFullPath(EAssetType::kTexture, Filename.data()), InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
}

FTexture2D::FTexture2D(FTexture2D&& Other) noexcept
    :
    Base(std::move(Other)),
    _ImageExtent(std::exchange(Other._ImageExtent, {}))
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
    Base::CreateTexture(ImageData, InitialFormat, FinalFormat, Flags, vk::ImageType::e2D,
                        vk::ImageViewType::e2D, 1, bGenerateMipmaps);
}

FTextureCube::FTextureCube(std::string_view Filename, vk::Format InitialFormat, vk::Format FinalFormat,
                           vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    : FTextureCube(nullptr, {}, Filename, InitialFormat, FinalFormat, Flags, bGenerateMipmaps)
{
}

FTextureCube::FTextureCube(const VmaAllocationCreateInfo& AllocationCreateInfo, std::string_view Filename,
                           vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    : FTextureCube(Graphics::FVulkanContext::GetClassInstance()->GetVmaAllocator(),
                   AllocationCreateInfo, Filename, InitialFormat, FinalFormat, Flags, bGenerateMipmaps)
{
}

FTextureCube::FTextureCube(VmaAllocator Allocator, const VmaAllocationCreateInfo& AllocationCreateInfo, std::string_view Filename,
                           vk::Format InitialFormat, vk::Format FinalFormat, vk::ImageCreateFlags Flags, bool bGenerateMipmaps)
    : Base(Allocator, &AllocationCreateInfo)
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
        CreateCubemap(GetAssetFullPath(EAssetType::kTexture, Filename.data()),
                      InitialFormat, FinalFormat, Flags, bGenerateMipmaps);
    }
}

FTextureCube::FTextureCube(FTextureCube&& Other) noexcept
    :
    Base(std::move(Other)),
    _ImageExtent(std::exchange(Other._ImageExtent, {}))
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
        FaceImages[i] = _ImageLoader->LoadImage(GetAssetFullPath(EAssetType::kTexture, Filenames[i]).c_str(), InitialFormat);

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

    Base::CreateTexture(ImageData, InitialFormat, FinalFormat, CubeFlags,
                        vk::ImageType::e2D, vk::ImageViewType::eCube, 6, bGenerateMipmaps);
}

_ASSET_END
_RUNTIME_END
_NPGS_END
