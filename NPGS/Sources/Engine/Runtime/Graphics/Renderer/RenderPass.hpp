#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"
#include "Engine/Runtime/Pools/CommandPoolPool.hpp"

namespace Npgs
{
    class IRenderPass
    {
    public:
        IRenderPass(FVulkanContext* VulkanContext);
        IRenderPass(const IRenderPass&) = delete;
        IRenderPass(IRenderPass&&)      = delete;
        virtual ~IRenderPass()          = default;

        IRenderPass& operator=(const IRenderPass&) = delete;
        IRenderPass& operator=(IRenderPass&&)      = delete;

        void Setup();
        
        virtual std::vector<FVulkanCommandBuffer>
        RecordCommands(const FCommandPoolPool::FPoolGuard& CommandPool, vk::Viewport Viewport, vk::Rect2D Scissor) = 0;

    protected:
        virtual void BindDescriptors()    = 0;
        virtual void DeclareAttachments() = 0;

    protected:
        FVulkanContext* VulkanContext_;
    };
} // namespace Npgs
