#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE vk::Queue* FQueuePool::FQueueGuard::operator->()
    {
        return &QueueInfo_.Queue;
    }

    NPGS_INLINE const vk::Queue* FQueuePool::FQueueGuard::operator->() const
    {
        return &QueueInfo_.Queue;
    }

    NPGS_INLINE vk::Queue& FQueuePool::FQueueGuard::operator*()
    {
        return QueueInfo_.Queue;
    }

    NPGS_INLINE const vk::Queue& FQueuePool::FQueueGuard::operator*() const
    {
        return QueueInfo_.Queue;
    }

    NPGS_INLINE std::size_t FQueuePool::FQueueHash::operator()(vk::QueueFlags QueueFlags) const
    {
        return std::hash<std::uint64_t>{}(static_cast<std::uint64_t>(static_cast<vk::QueueFlags::MaskType>(QueueFlags)));
    }
} // namespace Npgs
