#include "stdafx.h"
#include "RenderPass.hpp"

namespace Npgs
{
    IRenderPass::IRenderPass(FVulkanContext* VulkanContext)
        : VulkanContext_(VulkanContext)
    {
    }

    void IRenderPass::Setup()
    {
        LoadShaders();
        SetupPipeline();
        BindDescriptors();
        DeclareAttachments();
        RecordCommands(VulkanContext_);
    }
} // namespace Npgs
