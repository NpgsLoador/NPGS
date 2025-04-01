#include "CommandPoolManager.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <thread>

#include "Engine/Core/Base/Assert.h"

namespace Npgs
{
    NPGS_INLINE FCommandPoolManager::FPoolGuard FCommandPoolManager::AcquirePool()
    {
        FCommandPoolCreateInfo CreateInfo
        {
            .Device           = _Device,
            .Flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .QueueFamilyIndex = _QueueFamilyIndex
        };

        return AcquireResource(CreateInfo, [](const FResourceInfo&) -> bool { return true; });
    }

    NPGS_INLINE void FCommandPoolManager::CreateResource(const FCommandPoolCreateInfo& CreateInfo)
    {
        FResourceInfo ResourceInfo
        {
            .Resource          = std::make_unique<FVulkanCommandPool>(CreateInfo.Device, CreateInfo.QueueFamilyIndex, CreateInfo.Flags),
            .LastUsedTimestamp = GetCurrentTimeMs(),
            .UsageCount        = 1
        };

        _AvailableResources.push_back(std::move(ResourceInfo));
    }

    NPGS_INLINE bool FCommandPoolManager::HandleResourceEmergency(FResourceInfo& LowUsageResource, const FCommandPoolCreateInfo& CreateInfo)
    {
        return true;
    }
} // namespace Npgsg
