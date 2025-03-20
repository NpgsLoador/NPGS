#pragma once

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

#include <vulkan/vulkan.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_GRAPHICS_BEGIN

class FImageTracker
{
public:
    struct FImageState
    {
        vk::PipelineStageFlags2 StageMask{ vk::PipelineStageFlagBits2::eTopOfPipe };
        vk::AccessFlags2        AccessMask{ vk::AccessFlagBits2::eNone };
        vk::ImageLayout         ImageLayout{ vk::ImageLayout::eUndefined };

        bool operator==(const FImageState& Other) const;
        bool operator!=(const FImageState& Other) const;
    };

private:
    using FImageKey = std::variant<vk::Image, std::pair<vk::Image, vk::ImageSubresourceRange>>;

    struct FImageHash
    {
        std::size_t operator()(const FImageKey& Key) const;
    };

public:
    void TrackImage(vk::Image Image, const FImageState& ImageState);
    void TrackImage(vk::Image Image, const FImageMemoryMaskPack& ImageMemoryMaskPack);
    void TrackImage(vk::Image Image, const vk::ImageSubresourceRange& Range, const FImageState& ImageState);
    void TrackImage(vk::Image Image, const vk::ImageSubresourceRange& Range, const FImageMemoryMaskPack& ImageMemoryMaskPack);
    void FlushImageAllStates(vk::Image Image, const FImageState& ImageState);
    void FlushImageAllStates(vk::Image Image, const FImageMemoryMaskPack& ImageMemoryMaskPack);
    bool IsExisting(vk::Image Image);
    FImageState GetImageState(vk::Image Image) const;
    FImageState GetImageState(vk::Image Image, const vk::ImageSubresourceRange& Range);
    vk::ImageMemoryBarrier2 CreateBarrier(vk::Image Image, const vk::ImageSubresourceRange& Range, FImageState DstState);
    void Reset(vk::Image Image);
    void ResetAll();

    static FImageTracker* GetInstance();

private:
    FImageTracker()                     = default;
    FImageTracker(const FImageTracker&) = delete;
    FImageTracker(FImageTracker&&)      = delete;
    ~FImageTracker()                    = default;

    FImageTracker& operator=(const FImageTracker&) = delete;
    FImageTracker& operator=(FImageTracker&&)      = delete;

private:
    std::unordered_map<FImageKey, FImageState, FImageHash> _ImageStateMap;
    std::unordered_set<vk::Image, FImageHash>              _ImageSet;
};

_GRAPHICS_END
_RUNTIME_END
_NPGS_END

#include "ImageTracker.inl"
