#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    NPGS_INLINE FVulkanContext* Npgs::FCoreServices::GetVulkanContext() const
    {
        return VulkanContext_.get();
    }

    NPGS_INLINE FAssetManager* FCoreServices::GetAssetManager() const
    {
        return AssetManager_.get();
    }

    NPGS_INLINE FThreadPool* FCoreServices::GetThreadPool() const
    {
        return ThreadPool_.get();
    }
} // namespace Npgs
