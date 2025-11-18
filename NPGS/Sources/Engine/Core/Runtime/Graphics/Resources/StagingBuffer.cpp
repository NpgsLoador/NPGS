#include "stdafx.h"
#include "StagingBuffer.hpp"

#include <utility>
#include <vulkan/vulkan_to_string.hpp>

namespace Npgs
{
    namespace
    {
        // 格式描述，用于统一管理格式属性
        struct FFormatDescription
        {
            std::uint32_t Family{};      // 格式所属族
            std::uint32_t BitDepth{};    // 位深度
            bool bIsSrgb{ false };       // 是否为 sRGB 格式
            bool bIsCompressed{ false }; // 是否为压缩格式
            bool bIsDepth{ false };      // 是否为深度格式
        };

        // 格式族枚举
        enum class EFormatFamily : std::uint8_t
        {
            kUnknown = 0,
            kR8,             // R8 系列
            kRg8,            // RG8 系列
            kRgba8,          // RGBA8 系列
            kBgba8,          // BGRA8 系列
            kDepth16,        // 16 位深度格式
            kDepth24,        // 24 位深度格式
            kDepth32,        // 32 位深度格式
            kBc1,            // BC1 压缩格式
            kBc2,            // BC2 压缩格式
            kBc3,            // BC3 压缩格式
            kBc4,            // BC4 压缩格式
            kBc5,            // BC5 压缩格式
            kBc6H,           // BC6H 压缩格式
            kBC7,            // BC7 压缩格式
            // 可以继续添加更多格式族
        };

        // 获取格式描述符
        FFormatDescription GetFormatDescription(vk::Format Format)
        {
            switch (Format)
            {
                // R8 格式族
            case vk::Format::eR8Unorm:
                return { static_cast<std::uint32_t>(EFormatFamily::kR8), 8, false, false, false };
            case vk::Format::eR8Srgb:
                return { static_cast<std::uint32_t>(EFormatFamily::kR8), 8, true, false, false };

                // RG8 格式族
            case vk::Format::eR8G8Unorm:
                return { static_cast<std::uint32_t>(EFormatFamily::kRg8), 16, false, false, false };
            case vk::Format::eR8G8Srgb:
                return { static_cast<std::uint32_t>(EFormatFamily::kRg8), 16, true, false, false };

                // RGBA8 格式族
            case vk::Format::eR8G8B8A8Unorm:
                return { static_cast<std::uint32_t>(EFormatFamily::kRgba8), 32, false, false, false };
            case vk::Format::eR8G8B8A8Srgb:
                return { static_cast<std::uint32_t>(EFormatFamily::kRgba8), 32, true, false, false };

                // BGRA8 格式族
            case vk::Format::eB8G8R8A8Unorm:
                return { static_cast<std::uint32_t>(EFormatFamily::kBgba8), 32, false, false, false };
            case vk::Format::eB8G8R8A8Srgb:
                return { static_cast<std::uint32_t>(EFormatFamily::kBgba8), 32, true, false, false };

                // 深度格式
            case vk::Format::eD16Unorm:
                return { static_cast<std::uint32_t>(EFormatFamily::kDepth16), 16, false, false, true };
            case vk::Format::eD24UnormS8Uint:
                return { static_cast<std::uint32_t>(EFormatFamily::kDepth24), 32, false, false, true };
            case vk::Format::eD32Sfloat:
                return { static_cast<std::uint32_t>(EFormatFamily::kDepth32), 32, false, false, true };

                // 压缩格式
            case vk::Format::eBc1RgbUnormBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc1), 64, false, true, false };
            case vk::Format::eBc1RgbSrgbBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc1), 64, true, true, false };
            case vk::Format::eBc1RgbaUnormBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc1), 64, false, true, false };
            case vk::Format::eBc1RgbaSrgbBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc1), 64, true, true, false };
            case vk::Format::eBc2UnormBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc2), 128, false, true, false };
            case vk::Format::eBc2SrgbBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc2), 128, true, true, false };
            case vk::Format::eBc3UnormBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc3), 128, false, true, false };
            case vk::Format::eBc3SrgbBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc3), 128, true, true, false };
            case vk::Format::eBc4UnormBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc4), 64, false, true, false };
            case vk::Format::eBc4SnormBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc4), 64, false, true, false };
            case vk::Format::eBc5UnormBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc5), 128, false, true, false };
            case vk::Format::eBc5SnormBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc5), 128, false, true, false };
            case vk::Format::eBc6HUfloatBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc6H), 128, false, true, false };
            case vk::Format::eBc6HSfloatBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBc6H), 128, false, true, false };
            case vk::Format::eBc7UnormBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBC7), 128, false, true, false };
            case vk::Format::eBc7SrgbBlock:
                return { static_cast<std::uint32_t>(EFormatFamily::kBC7), 128, true, true, false };

            default:
                return { static_cast<std::uint32_t>(EFormatFamily::kUnknown), 0, false, false, false };
            }
        }

        bool IsFormatAliasingCompatible(vk::PhysicalDevice PhysicalDevice, vk::Format SrcFormat, vk::Format DstFormat)
        {
            FFormatDescription SrcDesc = GetFormatDescription(SrcFormat);
            FFormatDescription DstDesc = GetFormatDescription(DstFormat);

            if (SrcDesc.Family == static_cast<std::uint32_t>(EFormatFamily::kUnknown) ||
                DstDesc.Family == static_cast<std::uint32_t>(EFormatFamily::kUnknown) ||
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

    FStagingBuffer::FStagingBuffer(std::string_view Name, vk::PhysicalDevice PhysicalDevice,
                                   vk::Device Device, VmaAllocator Allocator,
                                   const VmaAllocationCreateInfo& AllocationCreateInfo,
                                   const vk::BufferCreateInfo& BufferCreateInfo)
        : PhysicalDevice_(PhysicalDevice)
        , Device_(Device)
        , Allocator_(Allocator)
        , AllocationCreateInfo_(AllocationCreateInfo)
        , Name_(Name)
    {
        Expand(BufferCreateInfo.size);
    }

    FStagingBuffer::FStagingBuffer(FStagingBuffer&& Other) noexcept
        : PhysicalDevice_(std::move(Other.PhysicalDevice_))
        , Device_(std::move(Other.Device_))
        , BufferMemory_(std::move(Other.BufferMemory_))
        , AliasedImage_(std::move(Other.AliasedImage_))
        , MemoryUsage_(std::exchange(Other.MemoryUsage_, 0))
        , Allocator_(std::exchange(Other.Allocator_, nullptr))
        , AllocationCreateInfo_(std::exchange(Other.AllocationCreateInfo_, {}))
        , Name_(std::move(Other.Name_))
    {
    }

    FStagingBuffer& FStagingBuffer::operator=(FStagingBuffer&& Other) noexcept
    {
        if (this != &Other)
        {
            PhysicalDevice_       = std::move(Other.PhysicalDevice_);
            Device_               = std::move(Other.Device_);
            BufferMemory_         = std::move(Other.BufferMemory_);
            AliasedImage_         = std::move(Other.AliasedImage_);
            MemoryUsage_          = std::exchange(Other.MemoryUsage_, 0);
            Allocator_            = std::exchange(Other.Allocator_, nullptr);
            AllocationCreateInfo_ = std::exchange(Other.AllocationCreateInfo_, {});
            Name_                 = std::move(Other.Name_);
        }

        return *this;
    }

	void* FStagingBuffer::MapMemory(vk::DeviceSize Size)
	{
		Expand(Size);

		void* Target = nullptr;
		BufferMemory_->MapMemoryForSubmit(0, Size, Target);
		MemoryUsage_ = Size;
		
        return Target;
	}

    FVulkanImage* FStagingBuffer::CreateAliasedImage(vk::Format OriginFormat, const vk::ImageCreateInfo& ImageCreateInfo)
    {
        if (!IsFormatAliasingCompatible(PhysicalDevice_, OriginFormat, ImageCreateInfo.format))
        {
            return nullptr;
        }

        vk::FormatProperties FormatProperties = PhysicalDevice_.getFormatProperties(ImageCreateInfo.format);

        if (!(FormatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
        {
            return nullptr;
        }

        vk::Extent3D Extent = ImageCreateInfo.extent;
        vk::DeviceSize ImageDataSize = static_cast<vk::DeviceSize>(
            Extent.width * Extent.height * Extent.depth * GetFormatInfo(ImageCreateInfo.format).PixelSize);

        if (ImageDataSize > BufferMemory_->GetMemory().GetAllocationSize())
        {
            return nullptr;
        }

        vk::ImageFormatProperties ImageFormatProperties = PhysicalDevice_.getImageFormatProperties(
            ImageCreateInfo.format, vk::ImageType::e2D, vk::ImageTiling::eLinear, vk::ImageUsageFlagBits::eTransferSrc);
        if (Extent.width  > ImageFormatProperties.maxExtent.width  ||
            Extent.height > ImageFormatProperties.maxExtent.height ||
            Extent.depth  > ImageFormatProperties.maxExtent.depth  ||
            ImageDataSize > ImageFormatProperties.maxResourceSize)
        {
            return nullptr;
        }

        std::string ImageName = Name_ + "_AlizsedImage";
        AliasedImage_ = std::make_unique<FVulkanImage>(Device_, Name_, Allocator_, AllocationCreateInfo_, ImageCreateInfo);

        vk::ImageSubresource ImageSubresource(vk::ImageAspectFlagBits::eColor, ImageCreateInfo.mipLevels, ImageCreateInfo.arrayLayers);
        vk::SubresourceLayout SubresourceLayout = Device_.getImageSubresourceLayout(**AliasedImage_, ImageSubresource);
        if (SubresourceLayout.size != ImageDataSize)
        {
            AliasedImage_.reset();
            return nullptr;
        }

        AliasedImage_->BindMemory(BufferMemory_->GetMemory(), 0);
        return AliasedImage_.get();
    }

    void FStagingBuffer::Expand(vk::DeviceSize Size)
    {
        if (BufferMemory_ != nullptr && Size <= BufferMemory_->GetMemory().GetAllocationSize())
        {
            return;
        }

        Release();

        std::string BufferName = Name_ + "_Buffer";
        std::string MemoryName = Name_ + "_Memory";
        vk::BufferCreateInfo BufferCreateInfo({}, Size, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);

        BufferMemory_ = std::make_unique<FVulkanBufferMemory>(
            Device_, BufferName, MemoryName, Allocator_, AllocationCreateInfo_, BufferCreateInfo);
    }
} // namespace Npgs
