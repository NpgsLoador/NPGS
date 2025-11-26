#pragma once

#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Pools/ThreadPool.hpp"

namespace Npgs
{
    class IMaterial
    {
    public:
        IMaterial(FVulkanContext* VulkanContext, FThreadPool* ThreadPool);
        virtual ~IMaterial() = default;

        virtual void LoadAssets() = 0;
        virtual void BindDescriptors() = 0;

    protected:
        FVulkanContext* VulkanContext_;
        FThreadPool*    ThreadPool_;
    };
} // namespace Npgs
