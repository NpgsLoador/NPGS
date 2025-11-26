#include "stdafx.h"
#include "CoreServices.hpp"

namespace Npgs
{
    FCoreServices::FCoreServices(const FCoreServicesEnableInfo& EnableInfo)
        : VulkanContext_(std::make_unique<FVulkanContext>())
        , AssetManager_(std::make_unique<FAssetManager>(VulkanContext_.get()))
        , ThreadPool_(std::make_unique<FThreadPool>(
            EnableInfo.ThreadPoolCreateInfo->MaxThreadCount, EnableInfo.ThreadPoolCreateInfo->bEnableHyperThread))
    {
    }
} // namespace Npgs
