#pragma once

#include <array>
#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>
#include "Engine/Runtime/Graphics/Renderer/RenderPass.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"

namespace Npgs
{
    class FGbufferScene : public IRenderPass
    {
    public:
        std::vector<FVulkanCommandBuffer>
        RecordCommands(const FCommandPoolPool::FPoolGuard& CommandPool, vk::Viewport Viewport, vk::Rect2D Scissor) override;

    private:
        void BindDescriptors() override;
        void DeclareAttachments() override;

    private:
        std::vector<FVulkanCommandBuffer> CommandBuffers_;
    };
} // namespace Npgs
