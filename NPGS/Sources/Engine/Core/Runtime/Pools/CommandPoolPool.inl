#include <memory>
#include <utility>

#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    NPGS_INLINE FCommandPoolPool::FPoolGuard FCommandPoolPool::AcquirePool(vk::CommandPoolCreateFlags Flags)
    {
        FCommandPoolCreateInfo CreateInfo{ Flags };
        return AcquireResource(CreateInfo, [](const std::unique_ptr<FCommandPoolInfo>&) -> bool { return true; });
    }

    NPGS_INLINE bool FCommandPoolPool::HandleResourceEmergency(FCommandPoolInfo& LowUsageResource, const FCommandPoolCreateInfo& CreateInfo)
    {
        return true;
    }

    NPGS_INLINE void Npgs::FCommandPoolPool::OnReleaseResource(FCommandPoolInfo& ResourceInfo)
    {
        ResourceInfo.Resource->Reset(vk::CommandPoolResetFlagBits::eReleaseResources);
    }
} // namespace Npgs
