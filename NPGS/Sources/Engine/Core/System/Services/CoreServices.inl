#include "CoreServices.h"

namespace Npgs
{
    NPGS_INLINE FVulkanContext* Npgs::FCoreServices::GetVulkanContext() const
    {
        return _VulkanContext.get();
    }

    NPGS_INLINE FAssetManager* FCoreServices::GetAssetManager() const
    {
        return _AssetManager.get();
    }

    NPGS_INLINE FThreadPool* FCoreServices::GetThreadPool() const
    {
        return _ThreadPool.get();
    }
} // namespace Npgs
