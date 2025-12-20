#include "stdafx.h"
#include "GbufferSceneTechnique.hpp"
#include "GbufferSceneTechniqueNameLookup.hpp"

#include <vector>

#include "Engine/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Runtime/Graphics/Buffers/BufferStructs.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"
#include "Engine/System/Services/EngineServices.hpp"

namespace Npgs
{
    void FGbufferSceneTechnique::LoadShaders()
    {
        //FResourceInfo ResourceInfo
        //{
        //    .VertexBufferInfos
        //    {
        //        { 0, sizeof(FVertex), false },
        //        { 1, sizeof(FInstanceData), true }
        //    },
        //    .VertexAttributeInfos
        //    {
        //        { 0, 0, offsetof(FVertex, Position) },
        //        { 0, 1, offsetof(FVertex, Normal) },
        //        { 0, 2, offsetof(FVertex, TexCoord) },
        //        { 0, 3, offsetof(FVertex, Tangent) },
        //        { 0, 4, offsetof(FVertex, Bitangent) },
        //        { 1, 5, offsetof(FInstanceData, Model) }
        //    },
        //    .ShaderBufferInfos{},
        //    .PushConstantInfos
        //    {
        //        { vk::ShaderStageFlagBits::eVertex, { "MatricesAddress" } }
        //    }
        //};

        //std::vector<std::string> ShaderFiles({ "PbrScene.vert.spv", "PbrSceneGBuffer.frag.spv" });

        //auto* AssetManager = EngineCoreServices->GetAssetManager();
        //AssetManager->AddAsset<FShader>(Techniques::GbufferScene::kShaderName, ShaderFiles, ResourceInfo);
    }

    void FGbufferSceneTechnique::SetupPipeline()
    {
    //    FGraphicsPipelineCreateInfoPack PipelineCreateInfoPack;

    //    PipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eScissor);
    //    PipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eViewport);

    //    vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
    //        .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
    //                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    //    PipelineCreateInfoPack.ColorBlendAttachmentStates.assign(4, ColorBlendAttachmentState);

    //    GbufferAttachmentFormats_ =
    //    {
    //        vk::Format::eR16G16B16A16Sfloat,
    //        vk::Format::eR16G16B16A16Sfloat,
    //        vk::Format::eR16G16B16A16Sfloat,
    //        vk::Format::eR16G16B16A16Sfloat
    //    };

    //    DepthAttachmentFormat_ = vk::Format::eD32Sfloat;

    //    vk::PipelineRenderingCreateInfo RenderingCreateInfo(0, GbufferAttachmentFormats_, DepthAttachmentFormat_);

    //    PipelineCreateInfoPack.GraphicsPipelineCreateInfo.setFlags(vk::PipelineCreateFlagBits::eDescriptorBufferEXT);
    //    PipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&RenderingCreateInfo);
    //    PipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    //    PipelineCreateInfoPack.RasterizationStateCreateInfo
    //        .setCullMode(vk::CullModeFlagBits::eBack)
    //        .setFrontFace(vk::FrontFace::eCounterClockwise);

    //    auto* PipelineManager = EngineResourceServices->GetPipelineManager();
    //    PipelineManager->CreateGraphicsPipeline(
    //        Techniques::GbufferScene::kPipelineName, Techniques::GbufferScene::kShaderName, PipelineCreateInfoPack);

    //    auto GetPipeline = [&]() -> void
    //    {
    //        Pipeline_ = PipelineManager->GetPipeline(Techniques::GbufferScene::kPipelineName);
    //    };

    //    GetPipeline();
    //    VulkanContext_->RegisterRuntimeOnlyCallbacks(FVulkanContext::ECallbackType::kCreateSwapchain, "GetPipeline", GetPipeline);

    //    PipelineLayout_ = PipelineManager->GetPipelineLayout(Techniques::GbufferScene::kPipelineName);
    }
}
