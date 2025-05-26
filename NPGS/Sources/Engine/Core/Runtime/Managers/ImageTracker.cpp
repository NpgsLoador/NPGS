#include "ImageTracker.h"

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
        ImageSet_.insert(Image);
    }

    void FImageTracker::FlushImageAllStates(vk::Image Image, const FImageState& ImageState)
    {
        for (auto& [Key, State] : ImageStateMap_)
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

    vk::ImageMemoryBarrier2 FImageTracker::CreateBarrier(vk::Image Image, const vk::ImageSubresourceRange& Range, FImageState DstState)
    {
        auto SubresourceKey = std::make_pair(Image, Range);
        auto it = ImageStateMap_.find(SubresourceKey);
        if (it == ImageStateMap_.end())
        {
            it = ImageStateMap_.find(Image);
        }

        vk::ImageMemoryBarrier2 Barrier;
        if (it != ImageStateMap_.end())
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
} // namespace Npgs
