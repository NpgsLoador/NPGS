namespace Npgs
{
    inline void FImageTracker::TrackImage(vk::Image Image, const FImageState& ImageState)
    {
        ImageStateMap_[Image] = ImageState;
    }

    inline FImageTracker::FImageState FImageTracker::GetImageState(vk::Image Image) const
    {
        return ImageStateMap_.at(Image);
    }

    inline void FImageTracker::Remove(vk::Image Image)
    {
        ImageStateMap_.erase(Image);
    }

    inline void FImageTracker::Reset(vk::Image Image)
    {
        ImageStateMap_.at(Image) = FImageState();
    }

    inline void FImageTracker::Clear()
    {
        ImageStateMap_.clear();
    }
} // namespace Npgs
