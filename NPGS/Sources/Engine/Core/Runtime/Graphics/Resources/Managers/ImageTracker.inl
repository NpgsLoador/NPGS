#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE void FImageTracker::TrackImage(vk::Image Image, const FImageState& ImageState)
    {
        ImageStateMap_[Image] = ImageState;
        ImageSet_.insert(Image);
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
        return ImageSet_.find(Image) != ImageSet_.end();
    }

    NPGS_INLINE FImageTracker::FImageState FImageTracker::GetImageState(vk::Image Image) const
    {
        return ImageStateMap_.at(Image);
    }

    NPGS_INLINE void FImageTracker::Reset(vk::Image Image)
    {
        auto& ImageState = ImageStateMap_.at(Image);
        ImageState = FImageState();
    }

    NPGS_INLINE void FImageTracker::ResetAll()
    {
        ImageStateMap_.clear();
        ImageSet_.clear();
    }
} // namespace Npgs
