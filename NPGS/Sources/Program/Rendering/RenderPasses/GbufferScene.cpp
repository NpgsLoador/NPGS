#include "stdafx.h"
#include "GbufferScene.hpp"
#include "GbufferSceneNameLookup.hpp"

#include <cstdint>

#include "Program/Rendering/NameLookup.hpp"

namespace Npgs
{
    std::vector<FVulkanCommandBuffer>
    FGbufferScene::RecordCommands(const FCommandPoolPool::FPoolGuard& CommandPool, vk::Viewport Viewport, vk::Rect2D Scissor)
    {
        //vk::CommandBufferInheritanceRenderingInfo InheritanceRenderingInfo = vk::CommandBufferInheritanceRenderingInfo()
        //    .setColorAttachmentCount(4)
        //    .setColorAttachmentFormats(GbufferAttachmentFormats_)
        //    .setDepthAttachmentFormat(DepthAttachmentFormat_)
        //    .setRasterizationSamples(vk::SampleCountFlagBits::e1); // TODO 全局设置 MSAA 抗锯齿

        //vk::CommandBufferInheritanceInfo InheritanceInfo = vk::CommandBufferInheritanceInfo()
        //    .setPNext(&InheritanceRenderingInfo);

        //std::vector<FVulkanCommandBuffer> CommandBuffers(Config::Graphics::kMaxFrameInFlight);
        //CommandPool->AllocateBuffers(vk::CommandBufferLevel::eSecondary, "GbufferSceneCommandBuffer", CommandBuffers);

        //for (std::size_t i = 0; i != CommandBuffers.size(); ++i)
        //{
        //    const auto& CommandBuffer = CommandBuffers[i];

        //    auto CommandBufferUsage = vk::CommandBufferUsageFlagBits::eRenderPassContinue |
        //                              vk::CommandBufferUsageFlagBits::eSimultaneousUse;

        //    CommandBuffer.Begin(InheritanceInfo, CommandBufferUsage);
        //    CommandBuffer->setViewport(0, Viewport);
        //    CommandBuffer->setScissor(0, Scissor);
        //}

        return {};
    }

    void FGbufferScene::BindDescriptors()
    {
        //auto* AssetManager = EngineCoreServices->GetAssetManager();
        //auto  Shader       = AssetManager->AcquireAsset<FShader>(RenderPasses::GbufferScene::kShaderName);

        //FDescriptorBufferCreateInfo DescriptorBufferCreateInfo;
        //DescriptorBufferCreateInfo.Name     = RenderPasses::GbufferScene::kDescriptorBufferName;
        //DescriptorBufferCreateInfo.SetInfos = Shader->GetDescriptorSetInfos();

        //auto        FramebufferSampler  = AssetManager->AcquireAsset<FVulkanSampler>(Public::Samplers::kFramebufferSamplerName);
        //auto*       RenderTargetManager = EngineResourceServices->GetRenderTargetManager();
        //const auto& DepthMapAttachment  = RenderTargetManager->GetManagedTarget(Public::Attachments::kDepthMapAttachmentName);

        //vk::DescriptorImageInfo DepthMapImageInfo(
        //    **FramebufferSampler, DepthMapAttachment.GetImageView(), DepthMapAttachment.GetImageLayout());
        //DescriptorBufferCreateInfo.CombinedImageSamplerInfos.emplace_back(2u, 0u, DepthMapImageInfo);

        //auto* ShaderBufferManager = EngineResourceServices->GetShaderBufferManager();
        //ShaderBufferManager->AllocateDescriptorBuffer(DescriptorBufferCreateInfo);
    }

    void FGbufferScene::DeclareAttachments()
    {
        //FRenderTargetDescription PositionAoDescription
        //{
        //    .ImageFormat = vk::Format::eR16G16B16A16Sfloat,
        //    .ImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        //    .ImageUsage  = vk::ImageUsageFlagBits::eColorAttachment,
        //    .LoadOp      = vk::AttachmentLoadOp::eClear,
        //    .StoreOp     = vk::AttachmentStoreOp::eStore,
        //    .ClearValue  = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)
        //};

        //FRenderTargetDescription NormalRoughDescription = PositionAoDescription;
        //FRenderTargetDescription AlbedoMetalDescription = PositionAoDescription;
        //FRenderTargetDescription ShadowDescription      = PositionAoDescription;

        //auto* RenderTargetManager = EngineResourceServices->GetRenderTargetManager();
        //RenderTargetManager->DeclareAttachment(RenderPasses::GbufferScene::kPositionAoName,  PositionAoDescription);
        //RenderTargetManager->DeclareAttachment(RenderPasses::GbufferScene::kNormalRoughName, NormalRoughDescription);
        //RenderTargetManager->DeclareAttachment(RenderPasses::GbufferScene::kAlbedoMetalName, AlbedoMetalDescription);
        //RenderTargetManager->DeclareAttachment(RenderPasses::GbufferScene::kShadowName,      ShadowDescription);
    }
} // namespace Npgs
