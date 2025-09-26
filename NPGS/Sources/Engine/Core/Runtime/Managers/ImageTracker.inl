#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    inline void FImageTracker::TrackImage(vk::Image Image, const FImageState& ImageState)
    {
        ImageStateMap_[Image] = ImageState;
        ImageSet_.insert(Image);
    }

    inline void FImageTracker::TrackImage(vk::Image Image, const FImageMemoryMaskPack& ImageMemoryMaskPack)
    {
        FImageState ImageState
        {
            .StageMask   = ImageMemoryMaskPack.kStageMask,
            .AccessMask  = ImageMemoryMaskPack.kAccessMask,
            .ImageLayout = ImageMemoryMaskPack.kImageLayout
        };

        TrackImage(Image, ImageState);
    }

    inline void FImageTracker::TrackImage(vk::Image Image, const vk::ImageSubresourceRange& Range,
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

    inline void FImageTracker::FlushImageAllStates(vk::Image Image, const FImageMemoryMaskPack& ImageMemoryMaskPack)
    {
        FImageState ImageState
        {
            .StageMask   = ImageMemoryMaskPack.kStageMask,
            .AccessMask  = ImageMemoryMaskPack.kAccessMask,
            .ImageLayout = ImageMemoryMaskPack.kImageLayout
        };

        FlushImageAllStates(Image, ImageState);
    }

    inline bool FImageTracker::IsExisting(vk::Image Image)
    {
        return ImageSet_.find(Image) != ImageSet_.end();
    }

    NPGS_INLINE FImageTracker::FImageState FImageTracker::GetImageState(vk::Image Image) const
    {
        return ImageStateMap_.at(Image);
    }

    inline void FImageTracker::Reset(vk::Image Image)
    {
        auto& ImageState = ImageStateMap_.at(Image);
        ImageState = FImageState();
    }

    inline void FImageTracker::ResetAll()
    {
        ImageStateMap_.clear();
        ImageSet_.clear();
    }
} // namespace Npgs
