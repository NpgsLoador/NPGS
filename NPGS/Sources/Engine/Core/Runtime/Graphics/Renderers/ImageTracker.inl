#include "ImageTracker.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

NPGS_INLINE void FImageTracker::TrackImage(vk::Image Image, FImageState ImageState)
{
    _ImageStateMap[Image] = ImageState;
}

NPGS_INLINE void FImageTracker::TrackImage(vk::Image Image, FImageMemoryMaskPack ImageMemoryMaskPack)
{
    FImageState ImageState
    {
        .StageMask   = ImageMemoryMaskPack.kStageMask,
        .AccessMask  = ImageMemoryMaskPack.kAccessMask,
        .ImageLayout = ImageMemoryMaskPack.kImageLayout
    };

    TrackImage(Image, ImageState);
}

NPGS_INLINE void FImageTracker::TrackImage(vk::Image Image, const vk::ImageSubresourceRange& Range, FImageState ImageState)
{
    FImageKey ImageKey = std::make_pair(Image, Range);
    _ImageStateMap[ImageKey] = ImageState;
}

NPGS_INLINE void FImageTracker::TrackImage(vk::Image Image, const vk::ImageSubresourceRange& Range, FImageMemoryMaskPack ImageMemoryMaskPack)
{
    FImageState ImageState
    {
        .StageMask   = ImageMemoryMaskPack.kStageMask,
        .AccessMask  = ImageMemoryMaskPack.kAccessMask,
        .ImageLayout = ImageMemoryMaskPack.kImageLayout
    };

    TrackImage(Image, Range, ImageState);
}

NPGS_INLINE FImageTracker::FImageState FImageTracker::GetImageState(vk::Image Image) const
{
    auto it = _ImageStateMap.find(Image);
    if (it == _ImageStateMap.end())
    {
        return {};
    }

    return it->second;
}

NPGS_INLINE FImageTracker::FImageState FImageTracker::GetImageState(vk::Image Image, const vk::ImageSubresourceRange& Range) const
{
    FImageKey ImageKey = std::make_pair(Image, Range);
    auto it = _ImageStateMap.find(ImageKey);
    if (it == _ImageStateMap.end())
    {
        return {};
    }

    return it->second;
}

NPGS_INLINE void FImageTracker::Reset(vk::Image Image)
{
    auto& ImageState = _ImageStateMap.at(Image);
    ImageState = FImageState();
}

NPGS_INLINE void FImageTracker::ResetAll()
{
    _ImageStateMap.clear();
}

_GRAPHICS_END
_RUNTIME_END
_NPGS_END
