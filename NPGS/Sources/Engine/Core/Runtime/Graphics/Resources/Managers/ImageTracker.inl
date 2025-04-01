#include "ImageTracker.h"

#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE void FImageTracker::TrackImage(vk::Image Image, const FImageState& ImageState)
    {
        _ImageStateMap[Image] = ImageState;
        _ImageSet.insert(Image);
    }

    NPGS_INLINE void FImageTracker::TrackImage(vk::Image Image, const FImageMemoryMaskPack& ImageMemoryMaskPack)
    {
        FImageState ImageState
        {
            .StageMask   = ImageMemoryMaskPack.kStageMask,
            .AccessMask  = ImageMemoryMaskPack.kAccessMask,
            .ImageLayout = ImageMemoryMaskPack.kImageLayout
        };

        TrackImage(Image, ImageState);
    }

    NPGS_INLINE void FImageTracker::TrackImage(vk::Image Image, const vk::ImageSubresourceRange& Range, const FImageState& ImageState)
    {
        FImageKey ImageKey   = std::make_pair(Image, Range);
        auto [it, bInserted] = _ImageStateMap.try_emplace(ImageKey, ImageState);
        if (bInserted)
        {
            auto ImageIt = _ImageStateMap.find(Image);
            if (ImageIt != _ImageStateMap.end())
            {
                _ImageStateMap.erase(ImageIt);
            }
        }

        it->second = ImageState;
        _ImageSet.insert(Image);
    }

    NPGS_INLINE void FImageTracker::TrackImage(vk::Image Image, const vk::ImageSubresourceRange& Range,
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

    NPGS_INLINE void FImageTracker::FlushImageAllStates(vk::Image Image, const FImageMemoryMaskPack& ImageMemoryMaskPack)
    {
        FImageState ImageState
        {
            .StageMask   = ImageMemoryMaskPack.kStageMask,
            .AccessMask  = ImageMemoryMaskPack.kAccessMask,
            .ImageLayout = ImageMemoryMaskPack.kImageLayout
        };

        FlushImageAllStates(Image, ImageState);
    }

    NPGS_INLINE bool FImageTracker::IsExisting(vk::Image Image)
    {
        return _ImageSet.find(Image) != _ImageSet.end();
    }

    NPGS_INLINE FImageTracker::FImageState FImageTracker::GetImageState(vk::Image Image) const
    {
        return _ImageStateMap.at(Image);
    }

    NPGS_INLINE void FImageTracker::Reset(vk::Image Image)
    {
        auto& ImageState = _ImageStateMap.at(Image);
        ImageState = FImageState();
    }

    NPGS_INLINE void FImageTracker::ResetAll()
    {
        _ImageStateMap.clear();
        _ImageSet.clear();
    }
} // namespace Npgs
