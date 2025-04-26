#include "StagingBuffer.h"

#include <utility>
#include <vulkan/vulkan_to_string.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Core.h"

namespace Npgs
{
    namespace
    {
        // 格式描述，用于统一管理格式属性
        struct FFormatDescription
        {
            std::uint32_t Family;        // 格式所属族
            std::uint32_t BitDepth;      // 位深度
            bool          bIsSrgb;       // 是否为 sRGB 格式
            bool          bIsCompressed; // 是否为压缩格式
            bool          bIsDepth;      // 是否为深度格式
        };

        // 格式族枚举
        enum class EFormatFamily : std::uint8_t
        {
            kUnknown = 0,
            kR8,             // R8 系列
            kRG8,            // RG8 系列
            kRGBA8,          // RGBA8 系列
            kBGRA8,          // BGRA8 系列
            kDepth16,        // 16位深度格式
            kDepth24,        // 24位深度格式
            kDepth32,        // 32位深度格式
            kBC1,            // BC1压缩格式
            kBC2,            // BC2压缩格式
            kBC3,            // BC3压缩格式
            kBC4,            // BC4压缩格式
            kBC5,            // BC5压缩格式
            kBC6H,           // BC6H压缩格式
            kBC7,            // BC7压缩格式
            // 可以继续添加更多格式族...
        };

        // 获取格式描述符
        FFormatDescription GetFormatDescription(vk::Format Format)
        {
            switch (Format)
            {
                // R8 格式族
            case vk::Format::eR8Unorm:
                return { static_cast<uint32_t>(EFormatFamily::kR8), 8, false, false, false };
            case vk::Format::eR8Srgb:
                return { static_cast<uint32_t>(EFormatFamily::kR8), 8, true, false, false };

                // RG8 格式族
            case vk::Format::eR8G8Unorm:
                return { static_cast<uint32_t>(EFormatFamily::kRG8), 16, false, false, false };
            case vk::Format::eR8G8Srgb:
                return { static_cast<uint32_t>(EFormatFamily::kRG8), 16, true, false, false };

                // RGBA8 格式族
            case vk::Format::eR8G8B8A8Unorm:
                return { static_cast<uint32_t>(EFormatFamily::kRGBA8), 32, false, false, false };
            case vk::Format::eR8G8B8A8Srgb:
                return { static_cast<uint32_t>(EFormatFamily::kRGBA8), 32, true, false, false };

                // BGRA8 格式族
            case vk::Format::eB8G8R8A8Unorm:
                return { static_cast<uint32_t>(EFormatFamily::kBGRA8), 32, false, false, false };
            case vk::Format::eB8G8R8A8Srgb:
                return { static_cast<uint32_t>(EFormatFamily::kBGRA8), 32, true, false, false };

                // 深度格式
            case vk::Format::eD16Unorm:
                return { static_cast<uint32_t>(EFormatFamily::kDepth16), 16, false, false, true };
            case vk::Format::eD24UnormS8Uint:
                return { static_cast<uint32_t>(EFormatFamily::kDepth24), 32, false, false, true };
            case vk::Format::eD32Sfloat:
                return { static_cast<uint32_t>(EFormatFamily::kDepth32), 32, false, false, true };

                // 压缩格式
            case vk::Format::eBc1RgbaUnormBlock:
                return { static_cast<uint32_t>(EFormatFamily::kBC1), 64, false, true, false };
            case vk::Format::eBc1RgbaSrgbBlock:
                return { static_cast<uint32_t>(EFormatFamily::kBC1), 64, true, true, false };

            default:
                return { static_cast<uint32_t>(EFormatFamily::kUnknown), 0, false, false, false };
            }
        }

        bool IsFormatAliasingCompatible(vk::PhysicalDevice PhysicalDevice, vk::Format SrcFormat, vk::Format DstFormat)
        {
            FFormatDescription SrcDesc = GetFormatDescription(SrcFormat);
            FFormatDescription DstDesc = GetFormatDescription(DstFormat);

            if (SrcDesc.Family == static_cast<uint32_t>(EFormatFamily::kUnknown) ||
                DstDesc.Family == static_cast<uint32_t>(EFormatFamily::kUnknown) ||
                SrcDesc.Family        != DstDesc.Family        || // 必须是同一格式族
                SrcDesc.BitDepth      != DstDesc.BitDepth      || // 位深度必须相同
                SrcDesc.bIsCompressed != DstDesc.bIsCompressed || // 压缩状态必须相同
                SrcDesc.bIsSrgb       != DstDesc.bIsSrgb       || // sRGB 和非 sRGB 不能混叠
                SrcDesc.bIsDepth)                                 // 深度格式不允许混叠
            {
                return false;
            }

            vk::FormatProperties SrcFormatProperties = PhysicalDevice.getFormatProperties(SrcFormat);
            vk::FormatProperties DstFormatProperties = PhysicalDevice.getFormatProperties(DstFormat);

            bool bLinearTilingCompatible =
                (SrcFormatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage) &&
                (DstFormatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage);

            bool bOptimalTilingCompatible =
                (SrcFormatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage) &&
                (DstFormatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage);

            if (!bLinearTilingCompatible && !bOptimalTilingCompatible)
            {
                return false;
            }

            FFormatInfo SrcFormatInfo(SrcFormat);
            FFormatInfo DstFormatInfo(DstFormat);

            if (SrcDesc.bIsCompressed)
            {
                // 确保组件数量和每个组件的位深度相同
                const auto SrcComponents = SrcFormatInfo.ComponentCount;
                const auto DstComponents = SrcFormatInfo.ComponentCount;
                if (SrcComponents != DstComponents)
                {
                    return false;
                }
            }

            return true;
        }
    }

    FStagingBuffer::FStagingBuffer(vk::PhysicalDevice PhysicalDevice, vk::Device Device, vk::DeviceSize Size)
        : _PhysicalDevice(_PhysicalDevice)
        , _Device(Device)
        , _Allocator(nullptr)
    {
        Expand(Size);
    }

    FStagingBuffer::FStagingBuffer(vk::PhysicalDevice PhysicalDevice, vk::Device Device, VmaAllocator Allocator,
                                   const VmaAllocationCreateInfo* AllocationCreateInfo, const vk::BufferCreateInfo& BufferCreateInfo)
        : _PhysicalDevice(PhysicalDevice)
        , _Device(Device)
        , _Allocator(Allocator)
        , _AllocationCreateInfo(AllocationCreateInfo)
    {
        Expand(BufferCreateInfo.size);
    }

    FStagingBuffer::FStagingBuffer(FStagingBuffer&& Other) noexcept
        : _PhysicalDevice(std::move(Other._PhysicalDevice))
        , _Device(std::move(Other._Device))
        , _BufferMemory(std::move(Other._BufferMemory))
        , _AliasedImage(std::move(Other._AliasedImage))
        , _MemoryUsage(std::exchange(Other._MemoryUsage, 0))
        , _Allocator(std::exchange(Other._Allocator, nullptr))
        , _AllocationCreateInfo(std::exchange(Other._AllocationCreateInfo, nullptr))
    {
    }

    FStagingBuffer& FStagingBuffer::operator=(FStagingBuffer&& Other) noexcept
    {
        if (this != &Other)
        {
            _PhysicalDevice       = std::move(Other._PhysicalDevice);
            _Device               = std::move(Other._Device);
            _BufferMemory         = std::move(Other._BufferMemory);
            _AliasedImage         = std::move(Other._AliasedImage);
            _MemoryUsage          = std::exchange(Other._MemoryUsage, 0);
            _Allocator            = std::exchange(Other._Allocator, nullptr);
            _AllocationCreateInfo = std::exchange(Other._AllocationCreateInfo, nullptr);
        }

        return *this;
    }

    FVulkanImage* FStagingBuffer::CreateAliasedImage(vk::Format OriginFormat, const vk::ImageCreateInfo& ImageCreateInfo)
    {
        if (!IsFormatAliasingCompatible(_PhysicalDevice, OriginFormat, ImageCreateInfo.format))
        {
            return nullptr;
        }

        vk::FormatProperties FormatProperties = _PhysicalDevice.getFormatProperties(ImageCreateInfo.format);

        if (!(FormatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
        {
            return nullptr;
        }

        vk::Extent3D Extent = ImageCreateInfo.extent;
        vk::DeviceSize ImageDataSize = static_cast<vk::DeviceSize>(
            Extent.width * Extent.height * Extent.depth * GetFormatInfo(ImageCreateInfo.format).PixelSize);

        if (ImageDataSize > _BufferMemory->GetMemory().GetAllocationSize())
        {
            return nullptr;
        }

        vk::ImageFormatProperties ImageFormatProperties = _PhysicalDevice.getImageFormatProperties(
            ImageCreateInfo.format, vk::ImageType::e2D, vk::ImageTiling::eLinear, vk::ImageUsageFlagBits::eTransferSrc);
        if (Extent.width  > ImageFormatProperties.maxExtent.width  ||
            Extent.height > ImageFormatProperties.maxExtent.height ||
            Extent.depth  > ImageFormatProperties.maxExtent.depth  ||
            ImageDataSize > ImageFormatProperties.maxResourceSize)
        {
            return nullptr;
        }

        if (_Allocator == nullptr)
        {
            _AliasedImage = std::make_unique<FVulkanImage>(_Device, _PhysicalDevice.getMemoryProperties(), ImageCreateInfo);
        }
        else
        {
            _AliasedImage = std::make_unique<FVulkanImage>(_Device, _Allocator, *_AllocationCreateInfo, ImageCreateInfo);
        }

        vk::ImageSubresource ImageSubresource(vk::ImageAspectFlagBits::eColor, ImageCreateInfo.mipLevels, ImageCreateInfo.arrayLayers);
        vk::SubresourceLayout SubresourceLayout = _Device.getImageSubresourceLayout(**_AliasedImage, ImageSubresource);
        if (SubresourceLayout.size != ImageDataSize)
        {
            _AliasedImage.reset();
            return nullptr;
        }

        _AliasedImage->BindMemory(_BufferMemory->GetMemory(), 0);
        return _AliasedImage.get();
    }

    void FStagingBuffer::Expand(vk::DeviceSize Size)
    {
        if (_BufferMemory != nullptr && Size <= _BufferMemory->GetMemory().GetAllocationSize())
        {
            return;
        }

        Release();

        vk::BufferCreateInfo BufferCreateInfo({}, Size, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);

        if (_Allocator != nullptr)
        {
            _BufferMemory = std::make_unique<FVulkanBufferMemory>(_Device, _Allocator, *_AllocationCreateInfo, BufferCreateInfo);
        }
        else
        {
            _BufferMemory = std::make_unique<FVulkanBufferMemory>(
                _Device, _PhysicalDevice.getProperties(), _PhysicalDevice.getMemoryProperties(),
                BufferCreateInfo, vk::MemoryPropertyFlagBits::eHostVisible);
        }
    }
} // namespace Npgs
