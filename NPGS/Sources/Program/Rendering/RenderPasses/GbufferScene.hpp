#pragma once

#include <array>
#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>
#include "Engine/Runtime/Graphics/Renderer/RenderPass.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"

namespace Npgs
{
    class FGbufferScene : public IRenderPass
    {
    public:
        void RecordCommands(FVulkanContext* VulkanContext) override;

    private:
        void LoadShaders() override;
        void SetupPipeline() override;
        void BindDescriptors() override;
        void DeclareAttachments() override;

    private:
        std::vector<FVulkanCommandBuffer> CommandBuffers_;
        std::array<vk::Format, 4>         GbufferAttachmentFormats_;
        vk::Format                        DepthAttachmentFormat_;
    };
} // namespace Npgs
