#include <memory>
#include <utility>

namespace Npgs
{
    NPGS_INLINE FCommandBufferPool::FBufferGuard FCommandBufferPool::AcquireBuffer(vk::CommandBufferLevel CommandBufferLevel)
    {
        FCommandBufferCreateInfo CreateInfo{ CommandBufferLevel, QueueFamilyIndex_ };
        return AcquireResource(CreateInfo, [](const std::unique_ptr<FCommandBufferInfo>&) -> bool { return true; });
    }

    NPGS_INLINE void FCommandBufferPool::CreateResource(const FCommandBufferCreateInfo& CreateInfo)
    {
        FVulkanCommandBuffer CommandBuffer;
        CommandPool_.AllocateBuffer(CreateInfo.CommandBufferLevel, CommandBuffer);
        auto ResourceInfoPtr               = std::make_unique<FCommandBufferInfo>();
        ResourceInfoPtr->Resource          = std::make_unique<FVulkanCommandBuffer>(std::move(CommandBuffer));
        ResourceInfoPtr->LastUsedTimestamp = GetCurrentTimeMs();
        ResourceInfoPtr->UsageCount        = 1;

        AvailableResources_.push_back(std::move(ResourceInfoPtr));
    }

    NPGS_INLINE bool FCommandBufferPool::HandleResourceEmergency(FCommandBufferInfo& LowUsageResource, const FCommandBufferCreateInfo& CreateInfo)
    {
        return true;
    }
} // namespace Npgsg
