#include "stdafx.h"
#include "GbufferScene.hpp"
#include "GbufferSceneNameLookup.hpp"

#include "Engine/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Runtime/Graphics/Buffers/BufferStructs.hpp"
#include "Engine/System/Services/EngineServices.hpp"
#include "Program/Rendering/NameLookup.hpp"

namespace Npgs
{
    void FGbufferScene::RecordCommands(FVulkanContext* VulkanContext)
    {
        auto CommandPool = VulkanContext->AcquireCommandPool(FVulkanContext::EQueueType::kGeneral);

        vk::CommandBufferInheritanceRenderingInfo InheritanceRenderingInfo = vk::CommandBufferInheritanceRenderingInfo()
            .setColorAttachmentCount(GbufferAttachmentFormats_.size())
            .setColorAttachmentFormats(GbufferAttachmentFormats_)
            .setDepthAttachmentFormat(DepthAttachmentFormat_)
            .setRasterizationSamples(vk::SampleCountFlagBits::e1); // TODO 全局设置 MSAA 抗锯齿

        vk::CommandBufferInheritanceInfo InheritanceInfo = vk::CommandBufferInheritanceInfo()
            .setPNext(&InheritanceRenderingInfo);
    }

    void FGbufferScene::LoadShaders()
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
                { 0, 1, offsetof(FVertex, Normal) },
                { 0, 2, offsetof(FVertex, TexCoord) },
                { 0, 3, offsetof(FVertex, Tangent) },
                { 0, 4, offsetof(FVertex, Bitangent) },
                { 1, 5, offsetof(FInstanceData, Model) }
            },
            .ShaderBufferInfos{},
            .PushConstantInfos
            {
                { vk::ShaderStageFlagBits::eVertex, { "MatricesAddress" } }
            }
        };

        std::vector<std::string> ShaderFiles({ "PbrScene.vert.spv", "PbrSceneGBuffer.frag.spv" });

        auto* AssetManager = EngineCoreServices->GetAssetManager();
        AssetManager->AddAsset<FShader>(RenderPasses::GbufferScene::kShaderName, ShaderFiles, ResourceInfo);
    }

    void FGbufferScene::SetupPipeline()
    {
        FGraphicsPipelineCreateInfoPack PipelineCreateInfoPack;

        PipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eScissor);
        PipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eViewport);

        vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                               vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        PipelineCreateInfoPack.ColorBlendAttachmentStates.assign(4, ColorBlendAttachmentState);

        GbufferAttachmentFormats_ =
        {
            vk::Format::eR16G16B16A16Sfloat,
            vk::Format::eR16G16B16A16Sfloat,
            vk::Format::eR16G16B16A16Sfloat,
            vk::Format::eR16G16B16A16Sfloat
        };

        DepthAttachmentFormat_ = vk::Format::eD32Sfloat;

        vk::PipelineRenderingCreateInfo RenderingCreateInfo(0, GbufferAttachmentFormats_, DepthAttachmentFormat_);

        PipelineCreateInfoPack.GraphicsPipelineCreateInfo.setFlags(vk::PipelineCreateFlagBits::eDescriptorBufferEXT);
        PipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&RenderingCreateInfo);
        PipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
        PipelineCreateInfoPack.RasterizationStateCreateInfo
            .setCullMode(vk::CullModeFlagBits::eBack)
            .setFrontFace(vk::FrontFace::eCounterClockwise);

        auto* PipelineManager = EngineResourceServices->GetPipelineManager();
        PipelineManager->CreateGraphicsPipeline(
            RenderPasses::GbufferScene::kPipelineName, RenderPasses::GbufferScene::kShaderName, PipelineCreateInfoPack);
    }

    void FGbufferScene::BindDescriptors()
    {
        auto* AssetManager = EngineCoreServices->GetAssetManager();
        auto  Shader       = AssetManager->AcquireAsset<FShader>(RenderPasses::GbufferScene::kShaderName);

        FDescriptorBufferCreateInfo DescriptorBufferCreateInfo;
        DescriptorBufferCreateInfo.Name     = RenderPasses::GbufferScene::kDescriptorBufferName;
        DescriptorBufferCreateInfo.SetInfos = Shader->GetDescriptorSetInfos();

        auto        FramebufferSampler  = AssetManager->AcquireAsset<FVulkanSampler>(Public::Samplers::kFramebufferSamplerName);
        auto*       RenderTargetManager = EngineResourceServices->GetRenderTargetManager();
        const auto& DepthMapAttachment  = RenderTargetManager->GetManagedTarget(Public::Attachments::kDepthMapAttachmentName);

        vk::DescriptorImageInfo DepthMapImageInfo(
            **FramebufferSampler, DepthMapAttachment.GetImageView(), DepthMapAttachment.GetImageLayout());
        DescriptorBufferCreateInfo.CombinedImageSamplerInfos.emplace_back(2u, 0u, DepthMapImageInfo);

        auto* ShaderBufferManager = EngineResourceServices->GetShaderBufferManager();
        ShaderBufferManager->AllocateDescriptorBuffer(DescriptorBufferCreateInfo);
    }

    void FGbufferScene::DeclareAttachments()
    {
        FRenderTargetDescription PositionAoDescription
        {
            .ImageFormat = vk::Format::eR16G16B16A16Sfloat,
            .ImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .ImageUsage  = vk::ImageUsageFlagBits::eColorAttachment,
            .LoadOp      = vk::AttachmentLoadOp::eClear,
            .StoreOp     = vk::AttachmentStoreOp::eStore,
            .ClearValue  = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)
        };

        FRenderTargetDescription NormalRoughDescription = PositionAoDescription;
        FRenderTargetDescription AlbedoMetalDescription = PositionAoDescription;
        FRenderTargetDescription ShadowDescription      = PositionAoDescription;

        auto* RenderTargetManager = EngineResourceServices->GetRenderTargetManager();
        RenderTargetManager->DeclareAttachment(RenderPasses::GbufferScene::kPositionAoName,  PositionAoDescription);
        RenderTargetManager->DeclareAttachment(RenderPasses::GbufferScene::kNormalRoughName, NormalRoughDescription);
        RenderTargetManager->DeclareAttachment(RenderPasses::GbufferScene::kAlbedoMetalName, AlbedoMetalDescription);
        RenderTargetManager->DeclareAttachment(RenderPasses::GbufferScene::kShadowName,      ShadowDescription);
    }
} // namespace Npgs
