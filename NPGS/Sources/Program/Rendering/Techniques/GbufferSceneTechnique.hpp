#pragma once

#include <array>
#include <vulkan/vulkan.hpp>

#include "Engine/Runtime/Graphics/Renderer/MaterialTemplate.hpp"

namespace Npgs
{
    class FGbufferSceneTechnique : public IMaterialTemplate
    {
    private:
        void LoadShaders() override;
        void SetupPipeline() override;

    private:
        std::array<vk::Format, 4> GbufferAttachmentFormats_;
        vk::Format                DepthAttachmentFormat_;
        vk::Pipeline              Pipeline_;
        vk::PipelineLayout        PipelineLayout_;
    };
}
