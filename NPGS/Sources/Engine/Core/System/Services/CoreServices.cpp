#include "CoreServices.h"

namespace Npgs
{
    FCoreServices::FCoreServices(const FCoreServicesEnableInfo& EnableInfo)
        : _VulkanContext(std::make_unique<FVulkanContext>())
        , _AssetManager(std::make_unique<FAssetManager>(_VulkanContext.get()))
        , _ThreadPool(std::make_unique<FThreadPool>(
            EnableInfo.ThreadPoolCreateInfo->MaxThreadCount, EnableInfo.ThreadPoolCreateInfo->bEnableHyperThread))
    {
    }
} // namespace Npgs
