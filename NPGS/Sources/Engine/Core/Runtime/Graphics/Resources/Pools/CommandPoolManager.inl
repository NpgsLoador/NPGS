#include "CommandPoolManager.h"

#include <memory>
#include <utility>

namespace Npgs
{
    NPGS_INLINE FCommandPoolManager::FPoolGuard FCommandPoolManager::AcquirePool()
    {
        FCommandPoolCreateInfo CreateInfo
        {
            .Flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .QueueFamilyIndex = _QueueFamilyIndex
        };

        return AcquireResource(CreateInfo, [](const std::unique_ptr<FResourceInfo>&) -> bool { return true; });
    }

    NPGS_INLINE void FCommandPoolManager::CreateResource(const FCommandPoolCreateInfo& CreateInfo)
    {
        auto ResourceInfoPtr = std::make_unique<FResourceInfo>();
        ResourceInfoPtr->Resource          = std::make_unique<FVulkanCommandPool>(_Device, CreateInfo.QueueFamilyIndex, CreateInfo.Flags);
        ResourceInfoPtr->LastUsedTimestamp = GetCurrentTimeMs();
        ResourceInfoPtr->UsageCount        = 1;

        _AvailableResources.push_back(std::move(ResourceInfoPtr));
    }

    NPGS_INLINE bool FCommandPoolManager::HandleResourceEmergency(FResourceInfo& LowUsageResource, const FCommandPoolCreateInfo& CreateInfo)
    {
        return true;
    }
} // namespace Npgsg
