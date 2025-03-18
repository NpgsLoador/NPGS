#include "ImageTracker.h"

#include <bit>

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

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

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
