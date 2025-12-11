#pragma once

#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"

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
        virtual void RecordCommands(FVulkanContext* VulkanContext) = 0;

    protected:
        virtual void LoadShaders()        = 0;
        virtual void SetupPipeline()      = 0;
        virtual void BindDescriptors()    = 0;
        virtual void DeclareAttachments() = 0;

    protected:
        FVulkanContext* VulkanContext_;
    };
} // namespace Npgs
