#include "ImageTracker.h"

#include <bit>

namespace Npgs
{
    bool FImageTracker::FImageState::operator==(const FImageState& Other) const
    {
        return StageMask == Other.StageMask && AccessMask == Other.AccessMask && ImageLayout == Other.ImageLayout;
    }

    bool FImageTracker::FImageState::operator!=(const FImageState& Other) const
    {
        return !(*this == Other);
    }

    std::size_t FImageTracker::FImageHash::operator()(const FImageKey& Key) const
    {
        if (std::holds_alternative<vk::Image>(Key))
        {
            return std::hash<std::uint64_t>{}(static_cast<std::uint64_t>(std::bit_cast<std::uint64_t>(std::get<vk::Image>(Key))));
        }
        else
        {
            const auto& [Image, Range] = std::get<std::pair<vk::Image, vk::ImageSubresourceRange>>(Key);
            std::size_t Hx = std::hash<std::uint64_t>{}(static_cast<std::uint64_t>(std::bit_cast<std::uint64_t>(Image)));
            std::size_t Ix = std::hash<std::uint32_t>{}(Range.aspectMask.operator std::uint32_t() ^ Range.baseMipLevel ^
                                                        Range.levelCount ^ Range.baseArrayLayer ^ Range.layerCount);
            return Hx ^ (Ix << 1);
        }
    }

    void FImageTracker::FlushImageAllStates(vk::Image Image, const FImageState& ImageState)
    {
        for (auto& [Key, State] : _ImageStateMap)
        {
            if (std::holds_alternative<vk::Image>(Key))
            {
                State = ImageState;
                continue;
            }

            if (std::holds_alternative<std::pair<vk::Image, vk::ImageSubresourceRange>>(Key))
            {
                auto& [KeyImage, KeyRange] = std::get<std::pair<vk::Image, vk::ImageSubresourceRange>>(Key);
                if (KeyImage == Image)
                {
                    State = ImageState;
                }
            }
        }
    }

    FImageTracker::FImageState FImageTracker::GetImageState(vk::Image Image, const vk::ImageSubresourceRange& Range)
    {
        FImageKey ImageKey = std::make_pair(Image, Range);
        auto it = _ImageStateMap.find(ImageKey);
        if (it != _ImageStateMap.end())
        {
            return it->second;
        }

        auto ImageIt = _ImageStateMap.find(Image);
        if (ImageIt != _ImageStateMap.end())
        {
            FImageState ImageState = ImageIt->second;
            _ImageStateMap[ImageKey] = ImageState;
            _ImageStateMap.erase(ImageIt);
            return ImageState;
        }

        return {};
    }

    vk::ImageMemoryBarrier2 FImageTracker::CreateBarrier(vk::Image Image, const vk::ImageSubresourceRange& Range, FImageState DstState)
    {
        auto SubresourceKey = std::make_pair(Image, Range);
        auto it = _ImageStateMap.find(SubresourceKey);
        if (it == _ImageStateMap.end())
        {
            it = _ImageStateMap.find(Image);
        }

        vk::ImageMemoryBarrier2 Barrier;
        if (it != _ImageStateMap.end())
        {
            Barrier.setSrcStageMask(it->second.StageMask)
                   .setSrcAccessMask(it->second.AccessMask)
                   .setOldLayout(it->second.ImageLayout);
        }
        else
        {
            Barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
                   .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                   .setOldLayout(vk::ImageLayout::eUndefined);
        }

        Barrier.setDstStageMask(DstState.StageMask)
               .setDstAccessMask(DstState.AccessMask)
               .setNewLayout(DstState.ImageLayout)
               .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
               .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
               .setImage(Image)
               .setSubresourceRange(Range);

        return Barrier;
    }

    FImageTracker* FImageTracker::GetInstance()
    {
        static FImageTracker kInstance;
        return &kInstance;
    }
} // namespace Npgs
