#include <memory>
#include <utility>

#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    inline FCommandBufferPool::FBufferGuard FCommandBufferPool::AcquireBuffer(vk::CommandBufferLevel CommandBufferLevel)
    {
        FCommandBufferCreateInfo CreateInfo{ CommandBufferLevel, QueueFamilyIndex_ };
        return AcquireResource(CreateInfo, [](const std::unique_ptr<FCommandBufferInfo>&) -> bool { return true; });
    }

    NPGS_INLINE bool FCommandBufferPool::HandleResourceEmergency(FCommandBufferInfo& LowUsageResource, const FCommandBufferCreateInfo& CreateInfo)
    {
        return true;
    }
} // namespace Npgsg
