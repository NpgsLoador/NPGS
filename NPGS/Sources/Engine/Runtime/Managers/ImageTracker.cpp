#include "stdafx.h"
#include "ImageTracker.hpp"

#include <algorithm>
#include "Engine/Core/Utils/VulkanUtils.hpp"

namespace Npgs
{
    std::size_t FImageTracker::FImageHash::operator()(const FImageKey& Key) const
    {
        if (std::holds_alternative<vk::Image>(Key))
        {
            const vk::Image& Image = std::get<vk::Image>(Key);
            return std::hash<std::uint64_t>{}(reinterpret_cast<std::uint64_t>(static_cast<VkImage>(Image)));
        }
        else
        {
            const auto& [Image, Range] = std::get<std::pair<vk::Image, vk::ImageSubresourceRange>>(Key);
            std::size_t Hx = std::hash<std::uint64_t>{}(reinterpret_cast<std::uint64_t>(static_cast<VkImage>(Image)));
            std::size_t Ix = std::hash<std::uint32_t>{}(Range.aspectMask.operator std::uint32_t() ^ Range.baseMipLevel ^
                                                        Range.levelCount ^ Range.baseArrayLayer ^ Range.layerCount);
            return Hx ^ (Ix << 1);
        }
    }

    FImageTracker::FImageTracker(FVulkanContext* VulkanContext)
        : bUnifiedImageLayouts_(VulkanContext->CheckDeviceExtensionsSupported(vk::KHRUnifiedImageLayoutsExtensionName))
    {
        // Temp
        bUnifiedImageLayouts_ = false;
    }

    void FImageTracker::TrackImage(vk::Image Image, const FImageMemoryMaskPack& ImageMemoryMaskPack)
    {
        FImageState ImageState
        {
            .StageMask   = ImageMemoryMaskPack.kStageMask,
            .AccessMask  = ImageMemoryMaskPack.kAccessMask,
            .ImageLayout = ImageMemoryMaskPack.kImageLayout
        };

        TrackImage(Image, ImageState);
    }

    void FImageTracker::TrackImage(vk::Image Image, const vk::ImageSubresourceRange& Range, const FImageState& ImageState)
    {
        FImageKey ImageKey = std::make_pair(Image, Range);
        auto [it, bInserted] = ImageStateMap_.try_emplace(ImageKey, ImageState);
        if (bInserted)
        {
            auto ImageIt = ImageStateMap_.find(Image);
            if (ImageIt != ImageStateMap_.end())
            {
                ImageStateMap_.erase(ImageIt);
            }
        }

        it->second = ImageState;
    }

    void FImageTracker::TrackImage(vk::Image Image, const vk::ImageSubresourceRange& Range,
                                   const FImageMemoryMaskPack& ImageMemoryMaskPack)
    {
        FImageState ImageState
        {
            .StageMask   = ImageMemoryMaskPack.kStageMask,
            .AccessMask  = ImageMemoryMaskPack.kAccessMask,
            .ImageLayout = ImageMemoryMaskPack.kImageLayout
        };

        TrackImage(Image, Range, ImageState);
    }

    void FImageTracker::CollapseImageStates(vk::Image Image, const FImageState& ImageState)
    {
        std::erase_if(ImageStateMap_, [Image](const auto& ImageKey) -> bool
        {
            if (std::holds_alternative<std::pair<vk::Image, vk::ImageSubresourceRange>>(ImageKey.first))
            {
                return std::get<std::pair<vk::Image, vk::ImageSubresourceRange>>(ImageKey.first).first == Image;
            }
            return false;
        });

        ImageStateMap_[Image] = ImageState;
    }

    void FImageTracker::CollapseImageStates(vk::Image Image, const FImageMemoryMaskPack& ImageMemoryMaskPack)
    {
        FImageState ImageState
        {
            .StageMask   = ImageMemoryMaskPack.kStageMask,
            .AccessMask  = ImageMemoryMaskPack.kAccessMask,
            .ImageLayout = ImageMemoryMaskPack.kImageLayout
        };

        CollapseImageStates(Image, ImageState);
    }

    FImageState FImageTracker::GetImageState(vk::Image Image, const vk::ImageSubresourceRange& Range)
    {
        FImageKey ImageKey = std::make_pair(Image, Range);
        auto it = ImageStateMap_.find(ImageKey);
        if (it != ImageStateMap_.end())
        {
            return it->second;
        }

        auto ImageIt = ImageStateMap_.find(Image);
        if (ImageIt != ImageStateMap_.end())
        {
            FImageState ImageState = ImageIt->second;
            ImageStateMap_[ImageKey] = ImageState;
            ImageStateMap_.erase(ImageIt);
            return ImageState;
        }

        return {};
    }

    vk::ImageMemoryBarrier2
    FImageTracker::MakeBarrier(vk::Image Image, const vk::ImageSubresourceRange& Range, const FImageState& DstState)
    {
        FImageState SrcState = GetImageState(Image, Range);

        vk::ImageLayout SrcLayout = SrcState.ImageLayout;
        vk::ImageLayout DstLayout = DstState.ImageLayout;

        if (bUnifiedImageLayouts_)
        {
            if (!Utils::IsSpecialLayout(SrcLayout))
            {
                SrcLayout = vk::ImageLayout::eGeneral;
            }
            if (!Utils::IsSpecialLayout(DstLayout))
            {
                DstLayout = vk::ImageLayout::eGeneral;
            }
        }

        vk::ImageMemoryBarrier2 Barrier(SrcState.StageMask, SrcState.AccessMask, DstState.StageMask, DstState.AccessMask,
                                        SrcLayout, DstLayout, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, Image, Range);

        FImageState NewInternalState =
        {
            .StageMask   = DstState.StageMask,
            .AccessMask  = DstState.AccessMask,
            .ImageLayout = DstState.ImageLayout
        };

        TrackImage(Image, Range, NewInternalState);

        return Barrier;
    }

    vk::ImageMemoryBarrier2 FImageTracker::MakeBarrier(vk::Image Image, const vk::ImageSubresourceRange& Range,
                                                       const FImageMemoryMaskPack& ImageMemoryMaskPack)
    {
        FImageState DstState =
        {
            .StageMask   = ImageMemoryMaskPack.kStageMask,
            .AccessMask  = ImageMemoryMaskPack.kAccessMask,
            .ImageLayout = ImageMemoryMaskPack.kImageLayout
        };

        return MakeBarrier(Image, Range, DstState);
    }
} // namespace Npgs
