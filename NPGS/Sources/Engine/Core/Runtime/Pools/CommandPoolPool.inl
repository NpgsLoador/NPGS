#include <memory>
#include <utility>

#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    inline FCommandPoolPool::FPoolGuard FCommandPoolPool::AcquirePool(vk::CommandPoolCreateFlags Flags)
    {
        FCommandPoolCreateInfo CreateInfo{ Flags };
        return AcquireResource(CreateInfo, [](const std::unique_ptr<FCommandPoolInfo>&) -> bool { return true; });
    }

    NPGS_INLINE bool FCommandPoolPool::HandleResourceEmergency(FCommandPoolInfo& LowUsageResource, const FCommandPoolCreateInfo& CreateInfo)
    {
        return true;
    }
} // namespace Npgsg
