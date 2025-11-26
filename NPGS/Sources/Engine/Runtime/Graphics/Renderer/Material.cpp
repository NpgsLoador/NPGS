#include "stdafx.h"
#include "Material.hpp"

namespace Npgs
{
    IMaterial::IMaterial(FVulkanContext* VulkanContext, FThreadPool* ThreadPool)
        : VulkanContext_(VulkanContext)
        , ThreadPool_(ThreadPool)
    {
        // LoadAssets();
    }
} // namespace Npgs
