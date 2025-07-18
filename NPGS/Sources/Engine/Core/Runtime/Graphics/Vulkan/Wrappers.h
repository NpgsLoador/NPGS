#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace Npgs
{
    struct FGraphicsPipelineCreateInfoPack
    {
    public:
        vk::GraphicsPipelineCreateInfo                     GraphicsPipelineCreateInfo;

        vk::PipelineVertexInputStateCreateInfo             VertexInputStateCreateInfo;
        vk::PipelineInputAssemblyStateCreateInfo           InputAssemblyStateCreateInfo;
        vk::PipelineTessellationStateCreateInfo            TessellationStateCreateInfo;
        vk::PipelineViewportStateCreateInfo                ViewportStateCreateInfo;
        vk::PipelineRasterizationStateCreateInfo           RasterizationStateCreateInfo;
        vk::PipelineMultisampleStateCreateInfo             MultisampleStateCreateInfo;
        vk::PipelineDepthStencilStateCreateInfo            DepthStencilStateCreateInfo;
        vk::PipelineColorBlendStateCreateInfo              ColorBlendStateCreateInfo;
        vk::PipelineDynamicStateCreateInfo                 DynamicStateCreateInfo;

        std::vector<vk::PipelineShaderStageCreateInfo>     ShaderStages;
        std::vector<vk::VertexInputBindingDescription>     VertexInputBindings;
        std::vector<vk::VertexInputAttributeDescription>   VertexInputAttributes;
        std::vector<vk::Viewport>                          Viewports;
        std::vector<vk::Rect2D>                            Scissors;
        std::vector<vk::PipelineColorBlendAttachmentState> ColorBlendAttachmentStates;
        std::vector<vk::DynamicState>                      DynamicStates;

        std::uint32_t                                      DynamicViewportCount{ 1 };
        std::uint32_t                                      DynamicScissorCount{ 1 };

    public:
        FGraphicsPipelineCreateInfoPack();
        FGraphicsPipelineCreateInfoPack(const FGraphicsPipelineCreateInfoPack&) = default;
        FGraphicsPipelineCreateInfoPack(FGraphicsPipelineCreateInfoPack&& Other) noexcept;
        ~FGraphicsPipelineCreateInfoPack() = default;

        FGraphicsPipelineCreateInfoPack& operator=(const FGraphicsPipelineCreateInfoPack&) = default;
        FGraphicsPipelineCreateInfoPack& operator=(FGraphicsPipelineCreateInfoPack&& Other) noexcept;

        operator vk::GraphicsPipelineCreateInfo& ();
        operator const vk::GraphicsPipelineCreateInfo& () const;

        void Update();

    private:
        void LinkToGraphicsPipelineCreateInfo();
        void UpdateAllInfoData();
    };

    struct FImageMemoryMaskPack
    {
        const vk::PipelineStageFlags2 kStageMask{ vk::PipelineStageFlagBits2::eNone };
        const vk::AccessFlags2        kAccessMask{ vk::AccessFlagBits2::eNone };
        const vk::ImageLayout         kImageLayout{ vk::ImageLayout::eUndefined };
        const bool                    kbEnable{ false };

        constexpr FImageMemoryMaskPack() noexcept = default;
        constexpr FImageMemoryMaskPack(vk::PipelineStageFlags2 StageMask, vk::AccessFlags2 AccessMask, vk::ImageLayout ImageLayout) noexcept
            : kStageMask(StageMask), kAccessMask(AccessMask), kImageLayout(ImageLayout), kbEnable(true)
        {
        }

        constexpr FImageMemoryMaskPack(vk::PipelineStageFlagBits2 StageMask, vk::AccessFlagBits2 AccessMask, vk::ImageLayout ImageLayout) noexcept
            : kStageMask(StageMask), kAccessMask(AccessMask), kImageLayout(ImageLayout), kbEnable(true)
        {
        }
    };

    struct FFormatInfo
    {
        enum class ERawDataType : std::uint8_t
        {
            kOther         = 0,
            kInteger       = 1,
            kFloatingPoint = 2
        };

        std::uint8_t ComponentCount{}; // 组件数量
        std::uint8_t ComponentSize{};  // 每个组件大小
        std::uint8_t PixelSize{};      // 单个像素大小 - 线性布局
        ERawDataType RawDataType{ ERawDataType::kOther };
        bool         bIsCompressed{ false };

        FFormatInfo(vk::Format Format);
        constexpr FFormatInfo(int ComponentCount, int ComponentSize, int PixelSize, ERawDataType RawDataType, bool bIsCompressed)
            : ComponentCount(ComponentCount)
            , ComponentSize(ComponentSize)
            , PixelSize(PixelSize)
            , RawDataType(RawDataType)
            , bIsCompressed(bIsCompressed)
        {
        }
    };

    FFormatInfo GetFormatInfo(vk::Format Format);
    vk::Format ConvertToFloat16(vk::Format Float32Format);

    // Copyright 2021-2024, Qiao YeCheng.
    // Maybe replaced by vulkan_format_traits.hpp
    constexpr FFormatInfo kFormatInfos[]
    {
        { 0, 0, 0, FFormatInfo::ERawDataType::kOther, false },          // vk::Format::eUndefined                = 0,

        { 2, 0, 1, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR4G4UnormPack8           = 1,

        { 4, 0, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR4G4B4A4UnormPack16      = 2,
        { 4, 0, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB4G4R4A4UnormPack16      = 3,

        { 3, 0, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR5G6B5UnormPack16        = 4,
        { 3, 0, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB5G6R5UnormPack16        = 5,

        { 4, 0, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR5G5B5A1UnormPack16      = 6,
        { 4, 0, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB5G5R5A1UnormPack16      = 7,
        { 4, 0, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA1R5G5B5UnormPack16      = 8,

        { 1, 1, 1, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8Unorm                  = 9,
        { 1, 1, 1, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8Snorm                  = 10,
        { 1, 1, 1, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8Uscaled                = 11,
        { 1, 1, 1, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8Sscaled                = 12,
        { 1, 1, 1, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8Uint                   = 13,
        { 1, 1, 1, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8Sint                   = 14,
        { 1, 1, 1, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8Srgb                   = 15,

        { 2, 1, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8Unorm                = 16,
        { 2, 1, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8Snorm                = 17,
        { 2, 1, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8Uscaled              = 18,
        { 2, 1, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8Sscaled              = 19,
        { 2, 1, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8Uint                 = 20,
        { 2, 1, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8Sint                 = 21,
        { 2, 1, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8Srgb                 = 22,

        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8Unorm              = 23,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8Snorm              = 24,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8Uscaled            = 25,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8Sscaled            = 26,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8Uint               = 27,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8Sint               = 28,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8Srgb               = 29,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8Unorm              = 30,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8Snorm              = 31,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8Uscaled            = 32,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8Sscaled            = 33,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8Uint               = 34,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8Sint               = 35,
        { 3, 1, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8SRGB               = 36,

        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8A8Unorm            = 37,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8A8Snorm            = 38,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8A8Uscaled          = 39,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8A8Sscaled          = 40,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8A8Uint             = 41,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8A8Sint             = 42,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR8G8B8A8Srgb             = 43,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8A8Unorm            = 44,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8A8Snorm            = 45,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8A8Uscaled          = 46,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8A8Sscaled          = 47,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8A8Uint             = 48,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8A8Sint             = 49,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eB8G8R8A8Srgb             = 50,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA8B8G8R8UnormPack32      = 51,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA8B8G8R8SnormPack32      = 52,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA8B8G8R8UscaledPack32    = 53,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA8B8G8R8SscaledPack32    = 54,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA8B8G8R8UintPack32       = 55,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA8B8G8R8SintPack32       = 56,
        { 4, 1, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA8B8G8R8SrgbPack32       = 57,

        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2R10G10B10UnormPack32   = 58,
        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2R10G10B10SnormPack32   = 59,
        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2R10G10B10UscaledPack32 = 60,
        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2R10G10B10SscaledPack32 = 61,
        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2R10G10B10UintPack32    = 62,
        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2R10G10B10SintPack32    = 63,
        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2B10G10R10UnormPack32   = 64,
        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2B10G10R10SnormPack32   = 65,
        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2B10G10R10UscaledPack32 = 66,
        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2B10G10R10SscaledPack32 = 67,
        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2B10G10R10UintPack32    = 68,
        { 4, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eA2B10G10R10SintPack32    = 69,

        { 1, 2, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16Unorm                 = 70,
        { 1, 2, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16Snorm                 = 71,
        { 1, 2, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16Uscaled               = 72,
        { 1, 2, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16Sscaled               = 73,
        { 1, 2, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16Uint                  = 74,
        { 1, 2, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16Sint                  = 75,
        { 1, 2, 2, FFormatInfo::ERawDataType::kFloatingPoint, false },  // vk::Format::eR16Sfloat                = 76,

        { 2, 2, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16Unorm              = 77,
        { 2, 2, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16Snorm              = 78,
        { 2, 2, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16Uscaled            = 79,
        { 2, 2, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16Sscaled            = 80,
        { 2, 2, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16Uint               = 81,
        { 2, 2, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16Sint               = 82,
        { 2, 2, 4, FFormatInfo::ERawDataType::kFloatingPoint, false },  // vk::Format::eR16G16Sfloat             = 83,

        { 3, 2, 6, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16Unorm           = 84,
        { 3, 2, 6, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16Snorm           = 85,
        { 3, 2, 6, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16Uscaled         = 86,
        { 3, 2, 6, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16Sscaled         = 87,
        { 3, 2, 6, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16Uint            = 88,
        { 3, 2, 6, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16Sint            = 89,
        { 3, 2, 6, FFormatInfo::ERawDataType::kFloatingPoint, false },  // vk::Format::eR16G16B16Sfloat          = 90,

        { 4, 2, 8, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16A16Unorm        = 91,
        { 4, 2, 8, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16A16Snorm        = 92,
        { 4, 2, 8, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16A16Uscaled      = 93,
        { 4, 2, 8, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16A16Sscaled      = 94,
        { 4, 2, 8, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16A16Uint         = 95,
        { 4, 2, 8, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR16G16B16A16Sint         = 96,
        { 4, 2, 8, FFormatInfo::ERawDataType::kFloatingPoint, false },  // vk::Format::eR16G16B16A16Sfloat       = 97,

        { 1, 4, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR32Uint                  = 98,
        { 1, 4, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR32Sint                  = 99,
        { 1, 4, 4, FFormatInfo::ERawDataType::kFloatingPoint, false },  // vk::Format::eR32Sfloat                = 100,

        { 2, 4, 8, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR32G32Uint               = 101,
        { 2, 4, 8, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR32G32Sint               = 102,
        { 2, 4, 8, FFormatInfo::ERawDataType::kFloatingPoint, false },  // vk::Format::eR32G32Sfloat             = 103,

        { 3, 4, 12, FFormatInfo::ERawDataType::kInteger, false },       // vk::Format::eR32G32B32Uint            = 104,
        { 3, 4, 12, FFormatInfo::ERawDataType::kInteger, false },       // vk::Format::eR32G32B32Sint            = 105,
        { 3, 4, 12, FFormatInfo::ERawDataType::kFloatingPoint, false }, // vk::Format::eR32G32B32Sfloat          = 106,

        { 4, 4, 16, FFormatInfo::ERawDataType::kInteger, false },       // vk::Format::eR32G32B32A32Uint         = 107,
        { 4, 4, 16, FFormatInfo::ERawDataType::kInteger, false },       // vk::Format::eR32G32B32A32Sint         = 108,
        { 4, 4, 16, FFormatInfo::ERawDataType::kFloatingPoint, false }, // vk::Format::eR32G32B32A32Sfloat       = 109,

        { 1, 8, 8, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR64Uint                  = 110,
        { 1, 8, 8, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eR64Sint                  = 111,
        { 1, 8, 8, FFormatInfo::ERawDataType::kFloatingPoint, false },  // vk::Format::eR64Sfloat                = 112,

        { 2, 8, 16, FFormatInfo::ERawDataType::kInteger, false },       // vk::Format::eR64G64Uint               = 113,
        { 2, 8, 16, FFormatInfo::ERawDataType::kInteger, false },       // vk::Format::eR64G64Sint               = 114,
        { 2, 8, 16, FFormatInfo::ERawDataType::kFloatingPoint, false }, // vk::Format::eR64G64Sfloat             = 115,

        { 3, 8, 24, FFormatInfo::ERawDataType::kInteger, false },       // vk::Format::eR64G64B64Uint            = 116,
        { 3, 8, 24, FFormatInfo::ERawDataType::kInteger, false },       // vk::Format::eR64G64B64Sint            = 117,
        { 3, 8, 24, FFormatInfo::ERawDataType::kFloatingPoint, false }, // vk::Format::eR64G64B64Sfloat          = 118,

        { 4, 8, 32, FFormatInfo::ERawDataType::kInteger, false },       // vk::Format::eR64G64B64A64Uint         = 119,
        { 4, 8, 32, FFormatInfo::ERawDataType::kInteger, false },       // vk::Format::eR64G64B64A64Sint         = 120,
        { 4, 8, 32, FFormatInfo::ERawDataType::kFloatingPoint, false }, // vk::Format::eR64G64B64A64Sfloat       = 121,

        { 3, 0, 4, FFormatInfo::ERawDataType::kFloatingPoint, false },  // vk::Format::eB10G11R11UfloatPack32    = 122,
        { 3, 0, 4, FFormatInfo::ERawDataType::kFloatingPoint, false },  // vk::Format::eE5B9G9R9UfloatPack32     = 123, // 'E' is a 5-bit shared exponent

        { 1, 2, 2, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eD16Unorm                 = 124,
        { 1, 3, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eX8D24UnormPack32         = 125, // 8 bits are unused therefore ComponentCount = 1, ComponentSize = 3
        { 1, 4, 4, FFormatInfo::ERawDataType::kFloatingPoint, false },  // vk::Format::eD32Sfloat                = 126,
        { 1, 1, 1, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eS8Uint                   = 127,
        { 2, 0, 3, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eD16UnormS8Uint           = 128,
        { 2, 0, 4, FFormatInfo::ERawDataType::kInteger, false },        // vk::Format::eD24UnormS8Uint           = 129,
        { 2, 0, 8, FFormatInfo::ERawDataType::kOther, false },          // vk::Format::eD32SfloatS8Uint          = 130, // 24 bits are unused if data is of linear tiling therefore PixelSize = 8

        { 3, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc1RgbUnormBlock         = 131,
        { 3, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc1RgbSrgbBlock          = 132,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc1RgbaUnormBlock        = 133,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc1RgbaSrgbBlock         = 134,

        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc2UnormBlock            = 135,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc2SrgbBlock             = 136,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc3UnormBlock            = 137,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc3SrgbBlock             = 138,

        { 1, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc4UnormBlock            = 139,
        { 1, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc4SnormBlock            = 140,
        { 2, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc5UnormBlock            = 141,
        { 2, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc5SnormBlock            = 142,
        { 3, 0, 0, FFormatInfo::ERawDataType::kFloatingPoint, true },   // vk::Format::eBc6HUfloatBlock          = 143,
        { 3, 0, 0, FFormatInfo::ERawDataType::kFloatingPoint, true },   // vk::Format::eBc6HSfloatBlock          = 144,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc7UnormBlock            = 145,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eBc7SrgbBlock             = 146,

        { 3, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eEtc2R8G8B8UnormBlock     = 147,
        { 3, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eEtc2R8G8B8SrgbBlock      = 148,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eEtc2R8G8B8A1UnormBlock   = 149,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eEtc2R8G8B8A1SrgbBlock    = 150,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eEtc2R8G8B8A8UnormBlock   = 151,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eEtc2R8G8B8A8SrgbBlock    = 152,

        { 1, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eEacR11UnormBlock         = 153,
        { 1, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eEacR11SnormBlock         = 154,
        { 2, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eEacR11G11UnormBlock      = 155,
        { 2, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eEacR11G11SnormBlock      = 156,

        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc4x4UnormBlock        = 157,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc4x4SrgbBlock         = 158,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc5x4UnormBlock        = 159,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc5x4SrgbBlock         = 160,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc5x5UnormBlock        = 161,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc5x5SrgbBlock         = 162,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc6x5UnormBlock        = 163,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc6x5SrgbBlock         = 164,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc6x6UnormBlock        = 165,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc6x6SrgbBlock         = 166,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc8x5UnormBlock        = 167,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc8x5SrgbBlock         = 168,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc8x6UnormBlock        = 169,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc8x6SrgbBlock         = 170,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc8x8UnormBlock        = 171,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc8x8SrgbBlock         = 172,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc10x5UnormBlock       = 173,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc10x5SrgbBlock        = 174,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc10x6UnormBlock       = 175,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc10x6SrgbBlock        = 176,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc10x8UnormBlock       = 177,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc10x8SrgbBlock        = 178,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc10x10UnormBlock      = 179,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc10x10SrgbBlock       = 180,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc12x10UnormBlock      = 181,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc12x10SrgbBlock       = 182,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc12x12UnormBlock      = 183,
        { 4, 0, 0, FFormatInfo::ERawDataType::kInteger, true },         // vk::Format::eAstc12x12SrgbBlock       = 184,
    };

    enum class EVulkanHandleReleaseMethod : std::uint8_t
    {
        kDestroy,
        kFree
    };

    template <typename HandleType>
    requires std::is_class_v<HandleType>
    class TVulkanHandleNoDestroy
    {
    public:
        TVulkanHandleNoDestroy() = default;
        explicit TVulkanHandleNoDestroy(HandleType Handle)
            : Handle_(Handle)
        {
        }

        TVulkanHandleNoDestroy(const TVulkanHandleNoDestroy&) = default;
        TVulkanHandleNoDestroy(TVulkanHandleNoDestroy&& Other) noexcept
            : Handle_(std::move(Other.Handle_))
        {
        }

        virtual ~TVulkanHandleNoDestroy() = default;

        TVulkanHandleNoDestroy& operator=(const TVulkanHandleNoDestroy&) = default;
        TVulkanHandleNoDestroy& operator=(TVulkanHandleNoDestroy&& Other) noexcept
        {
            if (this != &Other)
            {
                Handle_ = std::move(Other.Handle_);
            }

            return *this;
        }

        TVulkanHandleNoDestroy& operator=(HandleType Handle)
        {
            Handle_ = Handle;
            return *this;
        }

        HandleType* operator->()
        {
            return &Handle_;
        }

        const HandleType* operator->() const
        {
            return &Handle_;
        }

        HandleType& operator*()
        {
            return Handle_;
        }

        const HandleType& operator*() const
        {
            return Handle_;
        }

        explicit operator bool() const
        {
            return IsValid();
        }

        bool IsValid() const
        {
            return static_cast<bool>(Handle_);
        }

    protected:
        HandleType Handle_;
    };

    template <typename HandleType, bool bEnableReleaseInfoOutput = true,
              EVulkanHandleReleaseMethod ReleaseMethod = EVulkanHandleReleaseMethod::kDestroy>
    requires std::is_class_v<HandleType>
    class TVulkanHandle : public TVulkanHandleNoDestroy<HandleType>
    {
    public:
        using Base = TVulkanHandleNoDestroy<HandleType>;
        using Base::Base;

        TVulkanHandle() = delete;
        TVulkanHandle(vk::Device Device, HandleType Handle, const std::string& HandleName)
            : Base(Handle)
            , ReleaseInfo_(std::string(HandleName) + " destroyed successfully.")
            , Device_(Device)
            , Status_(vk::Result::eSuccess)
        {
        }

        explicit TVulkanHandle(vk::Device Device)
            : Device_(Device)
            , Status_(vk::Result::eSuccess)
        {
        }

        TVulkanHandle(const TVulkanHandle&) = default;
        TVulkanHandle(TVulkanHandle&& Other) noexcept
            : Base(std::move(Other))
            , ReleaseInfo_(std::move(Other.ReleaseInfo_))
            , Device_(std::move(Other.Device_))
            , Status_(std::exchange(Other.Status_, {}))
        {
        }

        ~TVulkanHandle() override
        {
            if (this->Handle_)
            {
                ReleaseHandle();
                if constexpr (bEnableReleaseInfoOutput)
                {
                    NpgsCoreTrace(ReleaseInfo_);
                }
            }
        }

        TVulkanHandle& operator=(const TVulkanHandle&) = default;
        TVulkanHandle& operator=(TVulkanHandle&& Other) noexcept
        {
            if (this != &Other)
            {
                if (this->Handle_)
                {
                    ReleaseHandle();
                }

                Base::operator=(std::move(Other));

                ReleaseInfo_ = std::move(Other.ReleaseInfo_);
                Device_      = std::move(Other.Device_);
                Status_      = std::exchange(Other.Status_, {});
            }

            return *this;
        }

        vk::Result GetStatus() const
        {
            return Status_;
        }

    protected:
        void ReleaseHandle()
        {
            if (this->Handle_)
            {
                if constexpr (ReleaseMethod == EVulkanHandleReleaseMethod::kDestroy)
                {
                    Device_.destroy(this->Handle_);
                }
                else if constexpr (ReleaseMethod == EVulkanHandleReleaseMethod::kFree)
                {
                    Device_.free(this->Handle_);
                }
            }
        }

    protected:
        std::string ReleaseInfo_;
        vk::Device  Device_;
        vk::Result  Status_;
    };

    // Wrapper for vk::CommandBuffer
    // -----------------------------
    class FVulkanCommandBuffer : public TVulkanHandleNoDestroy<vk::CommandBuffer>
    {
    public:
        using Base = TVulkanHandleNoDestroy<vk::CommandBuffer>;
        using Base::Base;

        vk::Result Begin(const vk::CommandBufferInheritanceInfo& InheritanceInfo, vk::CommandBufferUsageFlags Flags = {}) const;
        vk::Result Begin(vk::CommandBufferUsageFlags Flags = {}) const;
        vk::Result End() const;
    };

    // Wrapper for vk::CommandPool
    // ---------------------------
    class FVulkanCommandPool : public TVulkanHandle<vk::CommandPool>
    {
    public:
        using Base = TVulkanHandle<vk::CommandPool>;
        using Base::Base;

        FVulkanCommandPool(vk::Device Device, const vk::CommandPoolCreateInfo& CreateInfo);
        FVulkanCommandPool(vk::Device Device, std::uint32_t QueueFamilyIndex, vk::CommandPoolCreateFlags Flags = {});

        vk::Result AllocateBuffer(vk::CommandBufferLevel Level, vk::CommandBuffer& Buffer) const;
        vk::Result AllocateBuffer(vk::CommandBufferLevel Level, FVulkanCommandBuffer& Buffer) const;
        vk::Result AllocateBuffers(vk::CommandBufferLevel Level, std::vector<vk::CommandBuffer>& Buffers) const;
        vk::Result AllocateBuffers(vk::CommandBufferLevel Level, std::vector<FVulkanCommandBuffer>& Buffers) const;
        vk::Result FreeBuffer(vk::CommandBuffer& Buffer) const;
        vk::Result FreeBuffer(FVulkanCommandBuffer& Buffer) const;
        vk::Result FreeBuffers(const vk::ArrayProxy<const vk::CommandBuffer>& Buffers) const;
        vk::Result FreeBuffers(const vk::ArrayProxy<const FVulkanCommandBuffer>& Buffers) const;
        vk::Result Reset(vk::CommandPoolResetFlags Flags) const;

    private:
        vk::Result CreateCommandPool(const vk::CommandPoolCreateInfo& CreateInfo);
        vk::Result CreateCommandPool(std::uint32_t QueueFamilyIndex, vk::CommandPoolCreateFlags Flags);
    };

    // Wrapper for vk::DeviceMemory
    // ----------------------------
    class FVulkanDeviceMemory : public TVulkanHandle<vk::DeviceMemory, true, EVulkanHandleReleaseMethod::kFree>
    {
    public:
        using Base = TVulkanHandle<vk::DeviceMemory, true, EVulkanHandleReleaseMethod::kFree>;
        using Base::Base;

        FVulkanDeviceMemory(vk::Device Device, VmaAllocator Allocator, VmaAllocation Allocation,
                            const VmaAllocationInfo& AllocationInfo, vk::DeviceMemory Handle);

        FVulkanDeviceMemory(FVulkanDeviceMemory&& Other) noexcept;
        ~FVulkanDeviceMemory() override;

        FVulkanDeviceMemory& operator=(FVulkanDeviceMemory&& Other) noexcept;

        void SetPersistentMapping(bool bFlag);
        vk::Result MapMemoryForSubmit(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Target);
        vk::Result MapMemoryForFetch(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Data);
        vk::Result UnmapMemory(vk::DeviceSize Offset, vk::DeviceSize Size);
        vk::Result SubmitData(vk::DeviceSize MapOffset, vk::DeviceSize SubmitOffset, vk::DeviceSize Size, const void* Data);
        vk::Result FetchData(vk::DeviceSize MapOffset, vk::DeviceSize FetchOffset, vk::DeviceSize Size, void* Target);

        template <typename ContainerType>
        requires std::is_class_v<ContainerType>
        vk::Result SubmitData(const ContainerType& Data);

        template <typename ContainerType>
        requires std::is_class_v<ContainerType>
        vk::Result FetchData(ContainerType& Data);

        const void* GetMappedDataMemory() const;
        void* GetMappedTargetMemory();
        vk::DeviceSize GetAllocationSize() const;
        vk::MemoryPropertyFlags GetMemoryPropertyFlags() const;
        bool IsPereistentlyMapped() const;

    private:
        VmaAllocator            Allocator_;
        VmaAllocation           Allocation_;
        VmaAllocationInfo       AllocationInfo_;
        vk::MemoryPropertyFlags MemoryPropertyFlags_;
        void*                   MappedDataMemory_{ nullptr };
        void*                   MappedTargetMemory_{ nullptr };
        bool                    bPersistentlyMapped_{ false };
    };

    // Wrapper for vk::Buffer
    // ----------------------
    class FVulkanBuffer : public TVulkanHandle<vk::Buffer>
    {
    public:
        using Base = TVulkanHandle<vk::Buffer>;
        using Base::Base;

        FVulkanBuffer(vk::Device Device, VmaAllocator Allocator,
                      const VmaAllocationCreateInfo& AllocationCreateInfo,
                      const vk::BufferCreateInfo& CreateInfo);

        ~FVulkanBuffer() override;

        vk::Result BindMemory(const FVulkanDeviceMemory& DeviceMemory, vk::DeviceSize Offset = 0) const;
        vk::DeviceSize GetDeviceAddress() const;

        VmaAllocator GetAllocator() const;
        VmaAllocation GetAllocation() const;
        const VmaAllocationInfo& GetAllocationInfo() const;

    private:
        vk::Result CreateBuffer(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::BufferCreateInfo& CreateInfo);

    private:
        VmaAllocator      Allocator_;
        VmaAllocation     Allocation_{ nullptr };
        VmaAllocationInfo AllocationInfo_{};
    };

    // Wrapper for vk::BufferView
    // --------------------------
    class FVulkanBufferView : public TVulkanHandle<vk::BufferView>
    {
    public:
        using Base = TVulkanHandle<vk::BufferView>;
        using Base::Base;

        FVulkanBufferView(vk::Device Device, const vk::BufferViewCreateInfo& CreateInfo);
        FVulkanBufferView(vk::Device Device, const FVulkanBuffer& Buffer, vk::Format Format, vk::DeviceSize Offset = 0,
                          vk::DeviceSize Range = 0, vk::BufferViewCreateFlags Flags = {});

    private:
        vk::Result CreateBufferView(const vk::BufferViewCreateInfo& CreateInfo);
        vk::Result CreateBufferView(const FVulkanBuffer& Buffer, vk::Format Format, vk::DeviceSize Offset,
                                    vk::DeviceSize Range, vk::BufferViewCreateFlags Flags);
    };

    // Wrapper for vk::DescriptorSet
    // -----------------------------
    class FVulkanDescriptorSet : public TVulkanHandleNoDestroy<vk::DescriptorSet>
    {
    public:
        using Base = TVulkanHandleNoDestroy<vk::DescriptorSet>;
        using Base::Base;

        explicit FVulkanDescriptorSet(vk::Device Device);

        void Write(const vk::ArrayProxy<const vk::DescriptorImageInfo>& ImageInfos, vk::DescriptorType Type,
                   std::uint32_t BindingPoint, std::uint32_t ArrayElement = 0);

        void Write(const vk::ArrayProxy<const vk::DescriptorBufferInfo>& BufferInfos, vk::DescriptorType Type,
                   std::uint32_t BindingPoint, std::uint32_t ArrayElement = 0);

        void Write(const vk::ArrayProxy<const vk::BufferView>& BufferViews, vk::DescriptorType Type,
                   std::uint32_t BindingPoint, std::uint32_t ArrayElement = 0);

        void Write(const vk::ArrayProxy<const FVulkanBufferView>& BufferViews, vk::DescriptorType Type,
                   std::uint32_t BindingPoint, std::uint32_t ArrayElement = 0);

        void Update(const vk::ArrayProxy<const vk::WriteDescriptorSet>& Writes,
                    const vk::ArrayProxy<const vk::CopyDescriptorSet>& Copies = {});

    private:
        vk::Device Device_;
    };

    // Wrapper for vk::DescriptorSetLayout
    // -----------------------------------
    class FVulkanDescriptorSetLayout : public TVulkanHandle<vk::DescriptorSetLayout>
    {
    public:
        using Base = TVulkanHandle<vk::DescriptorSetLayout>;
        using Base::Base;

        FVulkanDescriptorSetLayout(vk::Device Device, const vk::DescriptorSetLayoutCreateInfo& CreateInfo);

        static std::vector<vk::DescriptorSetLayout> GetNativeTypeArray(const vk::ArrayProxy<const FVulkanDescriptorSetLayout>& WrappedTypeArray);

    private:
        vk::Result CreateDescriptorSetLayout(const vk::DescriptorSetLayoutCreateInfo& CreateInfo);
    };

    // Wrapper for vk::DescriptorPool
    // ------------------------------
    class FVulkanDescriptorPool : public TVulkanHandle<vk::DescriptorPool>
    {
    public:
        using Base = TVulkanHandle<vk::DescriptorPool>;
        using Base::Base;

        FVulkanDescriptorPool(vk::Device Device, const vk::DescriptorPoolCreateInfo& CreateInfo);
        FVulkanDescriptorPool(vk::Device Device, std::uint32_t MaxSets,
                              const vk::ArrayProxy<const vk::DescriptorPoolSize>& PoolSizes,
                              vk::DescriptorPoolCreateFlags Flags = {});

        vk::Result AllocateSets(const vk::ArrayProxy<const vk::DescriptorSetLayout>& Layouts, std::vector<vk::DescriptorSet>& Sets) const;
        vk::Result AllocateSets(const vk::ArrayProxy<const vk::DescriptorSetLayout>& Layouts, std::vector<FVulkanDescriptorSet>& Sets) const;
        vk::Result AllocateSets(const vk::ArrayProxy<const FVulkanDescriptorSetLayout>& Layouts, std::vector<vk::DescriptorSet>& Sets) const;
        vk::Result AllocateSets(const vk::ArrayProxy<const FVulkanDescriptorSetLayout>& Layouts, std::vector<FVulkanDescriptorSet>& Sets) const;
        vk::Result FreeSets(const vk::ArrayProxy<const vk::DescriptorSet>& Sets) const;
        vk::Result FreeSets(const vk::ArrayProxy<const FVulkanDescriptorSet>& Sets) const;

    private:
        vk::Result CreateDescriptorPool(const vk::DescriptorPoolCreateInfo& CreateInfo);
        vk::Result CreateDescriptorPool(std::uint32_t MaxSets, const vk::ArrayProxy<const vk::DescriptorPoolSize>& PoolSizes,
                                        vk::DescriptorPoolCreateFlags Flags);
    };

    // Wrapper for vk::Fence
    // ---------------------
    class FVulkanFence : public TVulkanHandle<vk::Fence, false>
    {
    public:
        using Base = TVulkanHandle<vk::Fence, false>;
        using Base::Base;

        FVulkanFence(vk::Device Device, const vk::FenceCreateInfo& CreateInfo);
        FVulkanFence(vk::Device Device, vk::FenceCreateFlags Flags = {});

        vk::Result Wait() const;
        vk::Result Reset() const;
        vk::Result WaitAndReset() const;
        vk::Result GetStatus() const;

    private:
        vk::Result CreateFence(const vk::FenceCreateInfo& CreateInfo);
        vk::Result CreateFence(vk::FenceCreateFlags Flags);
    };

    // Wrapper for vk::Framebuffer
    // ---------------------------
    class FVulkanFramebuffer : public TVulkanHandle<vk::Framebuffer>
    {
    public:
        using Base = TVulkanHandle<vk::Framebuffer>;
        using Base::Base;

        FVulkanFramebuffer(vk::Device Device, const vk::FramebufferCreateInfo& CreateInfo);

    private:
        vk::Result CreateFramebuffer(const vk::FramebufferCreateInfo& CreateInfo);
    };

    // Wrapper for vk::Image
    // ---------------------
    class FVulkanImage : public TVulkanHandle<vk::Image>
    {
    public:
        using Base = TVulkanHandle<vk::Image>;
        using Base::Base;

        FVulkanImage(vk::Device Device, VmaAllocator Allocator,
                     const VmaAllocationCreateInfo& AllocationCreateInfo,
                     const vk::ImageCreateInfo& CreateInfo);

        ~FVulkanImage() override;

        VmaAllocator GetAllocator() const;
        VmaAllocation GetAllocation() const;
        const VmaAllocationInfo& GetAllocationInfo() const;

        vk::Result BindMemory(const FVulkanDeviceMemory& DeviceMemory, vk::DeviceSize Offset = 0) const;

    private:
        vk::Result CreateImage(const VmaAllocationCreateInfo& AllocationCreateInfo, const vk::ImageCreateInfo& CreateInfo);

    private:
        VmaAllocator      Allocator_;
        VmaAllocation     Allocation_{ nullptr };
        VmaAllocationInfo AllocationInfo_{};
    };

    // Wrapper for vk::ImageView
    // -------------------------
    class FVulkanImageView : public TVulkanHandle<vk::ImageView>
    {
    public:
        using Base = TVulkanHandle<vk::ImageView>;
        using Base::Base;

        FVulkanImageView(vk::Device Device, const vk::ImageViewCreateInfo& CreateInfo);
        FVulkanImageView(vk::Device Device, const FVulkanImage& Image, vk::ImageViewType ViewType,
                         vk::Format Format, vk::ComponentMapping Components,
                         vk::ImageSubresourceRange SubresourceRange, vk::ImageViewCreateFlags Flags = {});

    private:
        vk::Result CreateImageView(const vk::ImageViewCreateInfo& CreateInfo);
        vk::Result CreateImageView(const FVulkanImage& Image, vk::ImageViewType ViewType, vk::Format Format,
                                   vk::ComponentMapping Components, vk::ImageSubresourceRange SubresourceRange,
                                   vk::ImageViewCreateFlags Flags);
    };

    // Wrapper for vk::PipelineCache
    // -----------------------------
    class FVulkanPipelineCache : public TVulkanHandle<vk::PipelineCache>
    {
    public:
        using Base = TVulkanHandle<vk::PipelineCache>;
        using Base::Base;

        FVulkanPipelineCache(vk::Device Device, vk::PipelineCacheCreateFlags Flags);
        FVulkanPipelineCache(vk::Device Device, vk::PipelineCacheCreateFlags Flags,
                             const vk::ArrayProxyNoTemporaries<const std::byte>& InitialData);

        FVulkanPipelineCache(vk::Device Device, const vk::PipelineCacheCreateInfo& CreateInfo);

    private:
        vk::Result CreatePipelineCache(vk::PipelineCacheCreateFlags Flags);
        vk::Result CreatePipelineCache(vk::PipelineCacheCreateFlags Flags,
                                       const vk::ArrayProxyNoTemporaries<const std::byte>& InitialData);

        vk::Result CreatePipelineCache(const vk::PipelineCacheCreateInfo& CreateInfo);
    };

    // Wrapper for vk::Pipeline
    // ------------------------
    class FVulkanPipeline : public TVulkanHandle<vk::Pipeline>
    {
    public:
        using Base = TVulkanHandle<vk::Pipeline>;
        using Base::Base;

        FVulkanPipeline(vk::Device Device, const vk::GraphicsPipelineCreateInfo& CreateInfo, const FVulkanPipelineCache* Cache = nullptr);
        FVulkanPipeline(vk::Device Device, const vk::ComputePipelineCreateInfo& CreateInfo, const FVulkanPipelineCache* Cache = nullptr);

    private:
        vk::Result CreateGraphicsPipeline(const vk::GraphicsPipelineCreateInfo& CreateInfo, const FVulkanPipelineCache* Cache = nullptr);
        vk::Result CreateComputePipeline(const vk::ComputePipelineCreateInfo& CreateInfo, const FVulkanPipelineCache* Cache = nullptr);
    };

    // Wrapper for vk::PipelineLayout
    // ------------------------------
    class FVulkanPipelineLayout : public TVulkanHandle<vk::PipelineLayout>
    {
    public:
        using Base = TVulkanHandle<vk::PipelineLayout>;
        using Base::Base;

        FVulkanPipelineLayout(vk::Device Device, const vk::PipelineLayoutCreateInfo& CreateInfo);

    private:
        vk::Result CreatePipelineLayout(const vk::PipelineLayoutCreateInfo& CreateInfo);
    };

    // Wrapper for vk::QueryPool
    // -------------------------
    class FVulkanQueryPool : public TVulkanHandle<vk::QueryPool>
    {
    public:
        using Base = TVulkanHandle<vk::QueryPool>;
        using Base::Base;

        FVulkanQueryPool(vk::Device Device, const vk::QueryPoolCreateInfo& CreateInfo);
        FVulkanQueryPool(vk::Device Device, vk::QueryType QueryType, std::uint32_t QueryCount,
                         vk::QueryPoolCreateFlags Flags = {}, vk::QueryPipelineStatisticFlags PipelineStatisticsFlags = {});

        template <typename DataType>
        std::vector<DataType> GetResult(std::uint32_t FirstQuery, std::uint32_t QueryCount,
                                        std::size_t DataSize, vk::DeviceSize Stride, vk::QueryResultFlags Flags);

        vk::Result Reset(std::uint32_t FirstQuery, std::uint32_t QueryCount);

    private:
        vk::Result CreateQueryPool(const vk::QueryPoolCreateInfo& CreateInfo);
    };

    // Wrapper for vk::RenderPass
    // --------------------------
    class FVulkanRenderPass : public TVulkanHandle<vk::RenderPass>
    {
    public:
        using Base = TVulkanHandle<vk::RenderPass>;
        using Base::Base;

        FVulkanRenderPass(vk::Device Device, const vk::RenderPassCreateInfo& CreateInfo);

        void CommandBegin(const FVulkanCommandBuffer& CommandBuffer, const vk::RenderPassBeginInfo& BeginInfo,
                          const vk::SubpassContents& SubpassContents = vk::SubpassContents::eInline) const;

        void CommandBegin(const FVulkanCommandBuffer& CommandBuffer, const FVulkanFramebuffer& Framebuffer,
                          const vk::Rect2D& RenderArea, const vk::ArrayProxy<const vk::ClearValue>& ClearValues,
                          const vk::SubpassContents& SubpassContents = vk::SubpassContents::eInline) const;

        void CommandNext(const FVulkanCommandBuffer& CommandBuffer,
                         const vk::SubpassContents& SubpassContents = vk::SubpassContents::eInline) const;

        void CommandEnd(const FVulkanCommandBuffer& CommandBuffer) const;

    private:
        vk::Result CreateRenderPass(const vk::RenderPassCreateInfo& CreateInfo);
    };

    // Wrapper for vk::Sampler
    // -----------------------
    class FVulkanSampler : public TVulkanHandle<vk::Sampler>
    {
    public:
        using Base = TVulkanHandle<vk::Sampler>;
        using Base::Base;

        FVulkanSampler(vk::Device Device, const vk::SamplerCreateInfo& CreateInfo);

    private:
        vk::Result CreateSampler(const vk::SamplerCreateInfo& CreateInfo);
    };

    // Wrapper for vk::Semaphore
    // -------------------------
    class FVulkanSemaphore : public TVulkanHandle<vk::Semaphore>
    {
    public:
        using Base = TVulkanHandle<vk::Semaphore>;
        using Base::Base;

        FVulkanSemaphore(vk::Device Device, const vk::SemaphoreCreateInfo& CreateInfo);
        FVulkanSemaphore(vk::Device Device, vk::SemaphoreCreateFlags Flags = {});

    private:
        vk::Result CreateSemaphore(const vk::SemaphoreCreateInfo& CreateInfo);
        vk::Result CreateSemaphore(vk::SemaphoreCreateFlags Flags);
    };

    // Wrapper for vk::ShaderModule
    // ----------------------------
    class FVulkanShaderModule : public TVulkanHandle<vk::ShaderModule>
    {
    public:
        using Base = TVulkanHandle<vk::ShaderModule>;
        using Base::Base;

        FVulkanShaderModule(vk::Device Device, const vk::ShaderModuleCreateInfo& CreateInfo);

    private:
        vk::Result CreateShaderModule(const vk::ShaderModuleCreateInfo& CreateInfo);
    };
    // -------------------
    // Native wrappers end

    template <typename ResourceType, typename MemoryType = FVulkanDeviceMemory>
    requires std::is_class_v<ResourceType>&& std::is_class_v<MemoryType>
    class TVulkanResourceMemory
    {
    public:
        TVulkanResourceMemory() = delete;
        TVulkanResourceMemory(std::unique_ptr<ResourceType>&& Resource, std::unique_ptr<MemoryType>&& Memory)
            : Resource_(std::move(Resource))
            , Memory_(std::move(Memory))
        {
        }

        TVulkanResourceMemory(const TVulkanResourceMemory&) = default;
        TVulkanResourceMemory(TVulkanResourceMemory&& Other) noexcept
            : Resource_(std::move(Other.Resource_))
            , Memory_(std::move(Other.Memory_))
        {
        }

        virtual ~TVulkanResourceMemory() = default;

        TVulkanResourceMemory& operator=(const TVulkanResourceMemory&) = default;
        TVulkanResourceMemory& operator=(TVulkanResourceMemory&& Other) noexcept
        {
            if (this != &Other)
            {
                Resource_ = std::move(Other.Resource_);
                Memory_   = std::move(Other.Memory_);
            }

            return *this;
        }

        operator MemoryType& ()
        {
            return GetMemory();
        }

        operator const MemoryType& () const
        {
            return GetMemory();
        }

        operator ResourceType& ()
        {
            return GetResource();
        }

        operator const ResourceType& () const
        {
            return GetResource();
        }

        explicit operator bool() const
        {
            return IsValid();
        }

        MemoryType& GetMemory()
        {
            return *Memory_;
        }

        const MemoryType& GetMemory() const
        {
            return *Memory_;
        }

        ResourceType& GetResource()
        {
            return *Resource_;
        }

        const ResourceType& GetResource() const
        {
            return *Resource_;
        }

        bool IsValid() const
        {
            return Resource_ && Memory_ && Resource_->operator bool() && Memory_->operator bool();
        }

    protected:
        std::unique_ptr<ResourceType> Resource_;
        std::unique_ptr<MemoryType>   Memory_;
    };

    class FVulkanBufferMemory : public TVulkanResourceMemory<FVulkanBuffer>
    {
    public:
        using Base = TVulkanResourceMemory<FVulkanBuffer>;
        using Base::Base;

        FVulkanBufferMemory(vk::Device Device, VmaAllocator Allocator,
                            const VmaAllocationCreateInfo& AllocationCreateInfo,
                            const vk::BufferCreateInfo& BufferCreateInfo);

        vk::Result MapMemoryForSubmit(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Target) const;
        vk::Result MapMemoryForFetch(vk::DeviceSize Offset, vk::DeviceSize Size, void*& Data) const;
        vk::Result UnmapMemory(vk::DeviceSize Offset, vk::DeviceSize Size) const;
        vk::Result SubmitBufferData(vk::DeviceSize MapOffset, vk::DeviceSize SubmitOffset, vk::DeviceSize Size, const void* Data) const;
        vk::Result FetchBufferData(vk::DeviceSize MapOffset, vk::DeviceSize FetchOffset, vk::DeviceSize Size, void* Target) const;

        template <typename ContainerType>
        requires std::is_class_v<ContainerType>
        vk::Result SubmitBufferData(const ContainerType& Data) const;

        template <typename ContainerType>
        requires std::is_class_v<ContainerType>
        vk::Result FetchBufferData(ContainerType& Data) const;
    };

    class FVulkanImageMemory : public TVulkanResourceMemory<FVulkanImage>
    {
    public:
        using Base = TVulkanResourceMemory<FVulkanImage>;
        using Base::Base;

        FVulkanImageMemory(vk::Device Device, VmaAllocator Allocator,
                           const VmaAllocationCreateInfo& AllocationCreateInfo,
                           const vk::ImageCreateInfo& ImageCreateInfo);
    };
} // namespace Npgs

#include "Wrappers.inl"
