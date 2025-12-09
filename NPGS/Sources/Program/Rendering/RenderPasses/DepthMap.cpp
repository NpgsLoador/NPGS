#include "stdafx.h"
#include "DepthMap.hpp"

#include "Engine/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Runtime/Graphics/Buffers/BufferStructs.hpp"
#include "Engine/System/Services/EngineServices.hpp"
#include "Program/Rendering/NameLookup.hpp"

namespace Npgs
{
    void FDepthMap::LoadShaders()
    {
        FResourceInfo ResourceInfo
        {
            .VertexBufferInfos
            {
                { 0, sizeof(FVertex), false },
                { 1, sizeof(FInstanceData), true }
            },
            .VertexAttributeInfos
            {
                { 0, 0, offsetof(FVertex, Position) },
                { 1, 1, offsetof(FInstanceData, Model) }
            },
            .ShaderBufferInfos{},
            .PushConstantInfos{ { vk::ShaderStageFlagBits::eVertex, { "MatricesAddress" } } }
        };

        std::vector<std::string> ShaderFiles({ "DepthMap.vert.spv" });

        auto* AssetManager = EngineCoreServices->GetAssetManager();
        AssetManager->AddAsset<FShader>(RenderPasses::DepthMap::kShaderName, ShaderFiles, ResourceInfo);
    }

    void FDepthMap::SetupPipeline()
    {
        FGraphicsPipelineCreateInfoPack PipelineCreateInfoPack;

        PipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eScissor);
        PipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eViewport);

        vk::PipelineRenderingCreateInfo RenderingCreateInfo = vk::PipelineRenderingCreateInfo()
            .setDepthAttachmentFormat(vk::Format::eD32Sfloat);

        PipelineCreateInfoPack.GraphicsPipelineCreateInfo.setFlags(vk::PipelineCreateFlagBits::eDescriptorBufferEXT);
        PipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&RenderingCreateInfo);
        PipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);

        auto* PipelineManager = EngineResourceServices->GetPipelineManager();
        PipelineManager->CreateGraphicsPipeline(
            RenderPasses::DepthMap::kPipelineName, RenderPasses::DepthMap::kShaderName, PipelineCreateInfoPack);
    }

    void FDepthMap::BindDescriptors()
    {
    }

    void FDepthMap::DeclareAttachments()
    {
        FRenderTargetDescription DepthMapDescription
        {
            .ImageFormat = vk::Format::eD32Sfloat,
            .ImageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .ImageUsage  = vk::ImageUsageFlagBits::eDepthStencilAttachment,
            .LoadOp      = vk::AttachmentLoadOp::eClear,
            .StoreOp     = vk::AttachmentStoreOp::eStore,
            .ClearValue  = vk::ClearDepthStencilValue(1.0f, 0)
        };

        auto* RenderTargetManager = EngineResourceServices->GetRenderTargetManager();
        RenderTargetManager->DeclareAttachment(Public::Attachments::kDepthMapAttachmentName, DepthMapDescription);
    }
} // namespace Npgs
