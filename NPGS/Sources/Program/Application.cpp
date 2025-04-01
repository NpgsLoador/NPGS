#include "Application.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <array>
#include <future>

#include <dxgi1_6.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <vulkan/vulkan.hpp>
#include <Windows.h>

#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Math/NumericConstants.h"
#include "Engine/Core/Math/TangentSpaceTools.h"
#include "Engine/Core/Runtime/AssetLoaders/Shader.h"
#include "Engine/Core/Runtime/AssetLoaders/Texture.h"
#include "Engine/Core/Runtime/Graphics/Resources/Managers/PipelineManager.h"
#include "Engine/Core/Runtime/Graphics/Resources/Managers/ShaderBufferManager.h"
#include "Engine/Utils/Logger.h"

namespace Npgs
{
    FApplication::FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle,
                               bool bEnableVSync, bool bEnableFullscreen, bool bEnableHdr)
        : _VulkanContext(FVulkanContext::GetClassInstance())
        , _ThreadPool()
        , _WindowTitle(WindowTitle)
        , _WindowSize(WindowSize)
        , _bEnableVSync(bEnableVSync)
        , _bEnableFullscreen(bEnableFullscreen)
        , _bEnableHdr(bEnableHdr)
    {
        if (!InitializeWindow())
        {
            NpgsCoreError("Failed to create application.");
            std::exit(EXIT_FAILURE);
        }
    }

    FApplication::~FApplication()
    {
        Terminate();
    }

    void FApplication::ExecuteMainRender()
    {
        CreateAttachments();
        LoadAssets();
        CreateUniformBuffers();
        BindDescriptors();
        InitializeInstanceData();
        InitializeVerticesData();
        CreatePipelines();

        auto* AssetManager        = FAssetManager::GetInstance();
        auto* ShaderBufferManager = FShaderBufferManager::GetInstance();
        auto* PipelineManager     = FPipelineManager::GetInstance();

        auto* PbrSceneGBufferShader = AssetManager->GetAsset<FShader>("PbrSceneGBufferShader");
        auto* PbrSceneMergeShader   = AssetManager->GetAsset<FShader>("PbrSceneMergeShader");
        auto* PbrSceneShader        = AssetManager->GetAsset<FShader>("PbrSceneShader");
        auto* LampShader            = AssetManager->GetAsset<FShader>("LampShader");
        auto* DepthMapShader        = AssetManager->GetAsset<FShader>("DepthMapShader");
        auto* TerrainShader         = AssetManager->GetAsset<FShader>("TerrainShader");
        auto* SkyboxShader          = AssetManager->GetAsset<FShader>("SkyboxShader");
        auto* PostShader            = AssetManager->GetAsset<FShader>("PostShader");

        vk::Pipeline PbrSceneGBufferPipeline;
        vk::Pipeline PbrSceneMergePipeline;
        vk::Pipeline PbrScenePipeline;
        vk::Pipeline LampPipeline;
        vk::Pipeline DepthMapPipeline;
        vk::Pipeline TerrainPipeline;
        vk::Pipeline PostPipeline;
        vk::Pipeline SkyboxPipeline;

        auto GetPipelines = [&]() -> void
        {
            PbrSceneGBufferPipeline = PipelineManager->GetPipeline("PbrSceneGBufferPipeline");
            PbrSceneMergePipeline   = PipelineManager->GetPipeline("PbrSceneMergePipeline");
            PbrScenePipeline        = PipelineManager->GetPipeline("PbrScenePipeline");
            LampPipeline            = PipelineManager->GetPipeline("LampPipeline");
            DepthMapPipeline        = PipelineManager->GetPipeline("DepthMapPipeline");
            TerrainPipeline         = PipelineManager->GetPipeline("TerrainPipeline");
            PostPipeline            = PipelineManager->GetPipeline("PostPipeline");
            SkyboxPipeline          = PipelineManager->GetPipeline("SkyboxPipeline");
        };

        GetPipelines();

        _VulkanContext->RegisterAutoRemovedCallbacks(
            FVulkanContext::ECallbackType::kCreateSwapchain, "GetPipelines", GetPipelines);

        auto PbrSceneGBufferPipelineLayout = PipelineManager->GetPipelineLayout("PbrSceneGBufferPipeline");
        auto PbrSceneMergePipelineLayout   = PipelineManager->GetPipelineLayout("PbrSceneMergePipeline");
        auto PbrScenePipelineLayout        = PipelineManager->GetPipelineLayout("PbrScenePipeline");
        auto LampPipelineLayout            = PipelineManager->GetPipelineLayout("LampPipeline");
        auto DepthMapPipelineLayout        = PipelineManager->GetPipelineLayout("DepthMapPipeline");
        auto TerrainPipelineLayout         = PipelineManager->GetPipelineLayout("TerrainPipeline");
        auto PostPipelineLayout            = PipelineManager->GetPipelineLayout("PostPipeline");
        auto SkyboxPipelineLayout          = PipelineManager->GetPipelineLayout("SkyboxPipeline");

        std::vector<FVulkanFence> InFlightFences;
        std::vector<FVulkanSemaphore> Semaphores_ImageAvailable;
        std::vector<FVulkanSemaphore> Semaphores_RenderFinished;
        std::vector<FVulkanCommandBuffer> CommandBuffers(Config::Graphics::kMaxFrameInFlight);
        std::vector<FVulkanCommandBuffer> DepthMapCommandBuffers(Config::Graphics::kMaxFrameInFlight);
        std::vector<FVulkanCommandBuffer> SceneGBufferCommandBuffers(Config::Graphics::kMaxFrameInFlight);
        std::vector<FVulkanCommandBuffer> SceneMergeCommandBuffers(Config::Graphics::kMaxFrameInFlight);
        std::vector<FVulkanCommandBuffer> FrontgroundCommandBuffers(Config::Graphics::kMaxFrameInFlight);
        std::vector<FVulkanCommandBuffer> PostProcessCommandBuffers(Config::Graphics::kMaxFrameInFlight);
        for (std::size_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            InFlightFences.emplace_back(vk::FenceCreateFlagBits::eSignaled);
            Semaphores_ImageAvailable.emplace_back(vk::SemaphoreCreateFlags());
            Semaphores_RenderFinished.emplace_back(vk::SemaphoreCreateFlags());
        }

        const auto& GraphicsCommandPool = _VulkanContext->GetGraphicsCommandPool();

        GraphicsCommandPool.AllocateBuffers(vk::CommandBufferLevel::ePrimary, CommandBuffers);
        GraphicsCommandPool.AllocateBuffers(vk::CommandBufferLevel::eSecondary, DepthMapCommandBuffers);
        GraphicsCommandPool.AllocateBuffers(vk::CommandBufferLevel::eSecondary, SceneGBufferCommandBuffers);
        GraphicsCommandPool.AllocateBuffers(vk::CommandBufferLevel::eSecondary, SceneMergeCommandBuffers);
        GraphicsCommandPool.AllocateBuffers(vk::CommandBufferLevel::eSecondary, FrontgroundCommandBuffers);
        GraphicsCommandPool.AllocateBuffers(vk::CommandBufferLevel::eSecondary, PostProcessCommandBuffers);

        vk::DeviceSize Offset        = 0;
        std::uint32_t  DynamicOffset = 0;
        std::uint32_t  CurrentFrame  = 0;

        glm::vec3    LightPos(-2.0f, 4.0f, -1.0f);
        glm::mat4x4  LightProjection  = glm::infinitePerspective(glm::radians(60.0f), 1.0f, 1.0f);
        glm::mat4x4  LightView        = glm::lookAt(LightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4x4  LightSpaceMatrix = LightProjection * LightView;
        vk::Extent2D DepthMapExtent(8192, 8192);

        vk::ImageSubresourceRange ColorSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageSubresourceRange DepthSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1);

        std::array PlaneVertexBuffers{ *_PlaneVertexBuffer->GetBuffer(), *_InstanceBuffer->GetBuffer() };
        std::array CubeVertexBuffers{ *_CubeVertexBuffer->GetBuffer(), *_InstanceBuffer->GetBuffer() };
        std::array SphereVertexBuffers{ *_SphereVertexBuffer->GetBuffer(), *_InstanceBuffer->GetBuffer() };
        std::array PlaneOffsets{ Offset, Offset };
        std::array CubeOffsets{ Offset, Offset + sizeof(glm::mat4x4) };
        std::array SphereOffsets{ Offset, Offset + 4 * sizeof(glm::mat4x4) };
        std::array LampOffsets{ Offset, Offset + 5 * sizeof(glm::mat4x4) };

        // std::array<float, 4> TessArgs{};
        // TessArgs[0] = 50.0f;
        // TessArgs[1] = 1000.0f;
        // TessArgs[2] = 8.0f;
        // TessArgs[3] = 1.5f * _VulkanContext->GetPhysicalDevice().getProperties().limits.maxTessellationGenerationLevel;

        FMatrices    Matrices{};
        FMvpMatrices MvpMatrices{};
        FLightArgs   LightArgs{};

        auto CameraPosUpdater = ShaderBufferManager->GetFieldUpdaters<glm::aligned_vec3>("LightArgs", "CameraPos");

        LightArgs.LightPos   = LightPos;
        LightArgs.LightColor = glm::vec3(300.0f);

        ShaderBufferManager->UpdateDataBuffers("LightArgs", LightArgs);

        // _Camera->SetOrbitTarget(glm::vec3(0.0f));
        // _Camera->SetOrbitAxis(glm::vec3(0.0f, 1.0f, 0.0f));

        // Record secondary commands
        // -------------------------
        auto RecordSecondaryCommands = [&]() -> void
        {
            vk::Extent2D SupersamplingExtent(_WindowSize.width * 2, _WindowSize.height * 2);

            vk::Viewport CommonViewport(0.0f, 0.0f, static_cast<float>(SupersamplingExtent.width),
                                        static_cast<float>(SupersamplingExtent.height), 0.0f, 1.0f);

            vk::Viewport DepthMapViewport(0.0f, 0.0f, static_cast<float>(DepthMapExtent.width),
                                          static_cast<float>(DepthMapExtent.height), 0.0f, 1.0f);

            vk::Rect2D CommonScissor(vk::Offset2D(), SupersamplingExtent);
            vk::Rect2D DepthMapScissor(vk::Offset2D(), DepthMapExtent);

            std::array GBufferAttachmentFormats
            {
                vk::Format::eR16G16B16A16Sfloat,
                vk::Format::eR16G16B16A16Sfloat,
                vk::Format::eR16G16B16A16Sfloat,
                vk::Format::eR16G16B16A16Sfloat,
            };

            vk::Format ColorAttachmentFormat = vk::Format::eR16G16B16A16Sfloat;

            vk::CommandBufferInheritanceRenderingInfo DepthMapInheritanceRenderingInfo = vk::CommandBufferInheritanceRenderingInfo()
                .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
                .setRasterizationSamples(vk::SampleCountFlagBits::e1);

            vk::CommandBufferInheritanceRenderingInfo GBufferSceneInheritanceRenderingInfo = vk::CommandBufferInheritanceRenderingInfo()
                .setColorAttachmentCount(4)
                .setColorAttachmentFormats(GBufferAttachmentFormats)
                .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
                .setRasterizationSamples(vk::SampleCountFlagBits::e1);

            vk::CommandBufferInheritanceRenderingInfo GBufferMergeInheritanceRenderingInfo = vk::CommandBufferInheritanceRenderingInfo()
                .setColorAttachmentCount(1)
                .setColorAttachmentFormats(ColorAttachmentFormat)
                .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
                .setRasterizationSamples(vk::SampleCountFlagBits::e1);

            vk::CommandBufferInheritanceRenderingInfo FrontgroundInheritanceRenderingInfo = vk::CommandBufferInheritanceRenderingInfo()
                .setColorAttachmentCount(1)
                .setColorAttachmentFormats(ColorAttachmentFormat)
                .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
                .setRasterizationSamples(vk::SampleCountFlagBits::e1);

            vk::CommandBufferInheritanceRenderingInfo PostInheritanceRenderingInfo = vk::CommandBufferInheritanceRenderingInfo()
                .setColorAttachmentCount(1)
                .setColorAttachmentFormats(_VulkanContext->GetSwapchainCreateInfo().imageFormat);

            for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
            {
                const auto& SceneGBufferDescriptorBuffer = ShaderBufferManager->GetDescriptorBuffer(i, "SceneGBufferDescriptorBuffer");
                vk::DescriptorBufferBindingInfoEXT SceneGBufferDescriptorBufferBindingInfo(
                    SceneGBufferDescriptorBuffer.GetBuffer().GetDeviceAddress(), vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT);

                const auto& SceneMergeDescriptorBuffer = ShaderBufferManager->GetDescriptorBuffer(i, "SceneMergeDescriptorBuffer");
                vk::DescriptorBufferBindingInfoEXT SceneMergeDescriptorBufferBindingInfo(
                    SceneMergeDescriptorBuffer.GetBuffer().GetDeviceAddress(), vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT);

                const auto& SkyboxDescriptorBuffer = ShaderBufferManager->GetDescriptorBuffer(i, "SkyboxDescriptorBuffer");
                vk::DescriptorBufferBindingInfoEXT SkyboxDescriptorBufferBindingInfo(
                    SkyboxDescriptorBuffer.GetBuffer().GetDeviceAddress(), vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT);

                const auto& PostDescriptorBuffer = ShaderBufferManager->GetDescriptorBuffer(i, "PostDescriptorBuffer");
                vk::DescriptorBufferBindingInfoEXT PostDescriptorBufferBindingInfo(
                    PostDescriptorBuffer.GetBuffer().GetDeviceAddress(), vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT);

                auto& DepthMapCommandBuffer = DepthMapCommandBuffers[i];

                vk::CommandBufferInheritanceInfo DepthMapInheritanceInfo = vk::CommandBufferInheritanceInfo()
                    .setPNext(&DepthMapInheritanceRenderingInfo);

                DepthMapCommandBuffer.Begin(DepthMapInheritanceInfo, vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse);
                DepthMapCommandBuffer->setViewport(0, DepthMapViewport);
                DepthMapCommandBuffer->setScissor(0, DepthMapScissor);
                DepthMapCommandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, DepthMapPipeline);
                vk::DeviceSize MatricesAddress = ShaderBufferManager->GetDataBuffer(i, "Matrices").GetBuffer().GetDeviceAddress();
                DepthMapCommandBuffer->pushConstants<vk::DeviceAddress>(DepthMapPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, MatricesAddress);
                DepthMapCommandBuffer->bindVertexBuffers(0, PlaneVertexBuffers, PlaneOffsets);
                DepthMapCommandBuffer->draw(6, 1, 0, 0);
                DepthMapCommandBuffer->bindVertexBuffers(0, CubeVertexBuffers, CubeOffsets);
                DepthMapCommandBuffer->draw(36, 3, 0, 0);
                DepthMapCommandBuffer.End();

                auto& SceneGBufferCommandBuffer = SceneGBufferCommandBuffers[i];

                vk::CommandBufferInheritanceInfo GBufferSceneInheritanceInfo = vk::CommandBufferInheritanceInfo()
                    .setPNext(&GBufferSceneInheritanceRenderingInfo);

                SceneGBufferCommandBuffer.Begin(GBufferSceneInheritanceInfo, vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse);
                SceneGBufferCommandBuffer->setViewport(0, CommonViewport);
                SceneGBufferCommandBuffer->setScissor(0, CommonScissor);
                SceneGBufferCommandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, PbrSceneGBufferPipeline);
                SceneGBufferCommandBuffer->bindDescriptorBuffersEXT(SceneGBufferDescriptorBufferBindingInfo);
                auto Offsets = ShaderBufferManager->GetDescriptorBindingOffsets("SceneGBufferDescriptorBuffer", 0, 1, 2);
                SceneGBufferCommandBuffer->setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, PbrSceneGBufferPipelineLayout, 0, { 0, 0, 0 }, Offsets);
                SceneGBufferCommandBuffer->bindVertexBuffers(0, PlaneVertexBuffers, PlaneOffsets);
                SceneGBufferCommandBuffer->draw(6, 1, 0, 0);
                SceneGBufferCommandBuffer->bindVertexBuffers(0, CubeVertexBuffers, CubeOffsets);
                SceneGBufferCommandBuffer->draw(36, 3, 0, 0);
                SceneGBufferCommandBuffer.End();

                auto& SceneMergeCommandBuffer = SceneMergeCommandBuffers[i];

                vk::CommandBufferInheritanceInfo GBufferMergeInheritanceInfo = vk::CommandBufferInheritanceInfo()
                    .setPNext(&GBufferMergeInheritanceRenderingInfo);

                std::uint32_t WorkgroupSizeX = (SupersamplingExtent.width  + 15) / 16;
                std::uint32_t WorkgroupSizeY = (SupersamplingExtent.height + 15) / 16;

                SceneMergeCommandBuffer.Begin(GBufferMergeInheritanceInfo, vk::CommandBufferUsageFlagBits::eSimultaneousUse);
                SceneMergeCommandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, PbrSceneMergePipeline);
                vk::DeviceAddress LightArgsAddress = ShaderBufferManager->GetDataBuffer(i, "LightArgs").GetBuffer().GetDeviceAddress();
                SceneMergeCommandBuffer->pushConstants<vk::DeviceAddress>(PbrSceneMergePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, LightArgsAddress);
                SceneMergeCommandBuffer->bindDescriptorBuffersEXT(SceneMergeDescriptorBufferBindingInfo);
                Offsets = ShaderBufferManager->GetDescriptorBindingOffsets("SceneMergeDescriptorBuffer", 0, 1);
                SceneMergeCommandBuffer->setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eCompute, PbrSceneMergePipelineLayout, 0, { 0, 0 }, Offsets);
                SceneMergeCommandBuffer->dispatch(WorkgroupSizeX, WorkgroupSizeY, 1);
                SceneMergeCommandBuffer.End();

                auto& FrontgroundCommandBuffer = FrontgroundCommandBuffers[i];

                vk::CommandBufferInheritanceInfo FrontgroundInheritanceInfo = vk::CommandBufferInheritanceInfo()
                    .setPNext(&FrontgroundInheritanceRenderingInfo);

                FrontgroundCommandBuffer.Begin(FrontgroundInheritanceInfo, vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse);
                FrontgroundCommandBuffer->setScissor(0, CommonScissor);
                FrontgroundCommandBuffer->setViewport(0, CommonViewport);
                FrontgroundCommandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, LampPipeline);
                FrontgroundCommandBuffer->pushConstants<vk::DeviceAddress>(LampPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, MatricesAddress);
                FrontgroundCommandBuffer->pushConstants<vk::DeviceAddress>(LampPipelineLayout, vk::ShaderStageFlagBits::eFragment, LampShader->GetPushConstantOffset("LightArgsAddress"), LightArgsAddress);
                FrontgroundCommandBuffer->bindVertexBuffers(0, CubeVertexBuffers, LampOffsets);
                FrontgroundCommandBuffer->draw(36, 1, 0, 0);
                FrontgroundCommandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, SkyboxPipeline);
                FrontgroundCommandBuffer->pushConstants<vk::DeviceAddress>(SkyboxPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, MatricesAddress);
                FrontgroundCommandBuffer->bindDescriptorBuffersEXT(SkyboxDescriptorBufferBindingInfo);
                vk::DeviceSize OffsetSet0 = ShaderBufferManager->GetDescriptorBindingOffset("SkyboxDescriptorBuffer", 0, 0);
                FrontgroundCommandBuffer->setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, SkyboxPipelineLayout, 0, { 0 }, OffsetSet0);
                FrontgroundCommandBuffer->bindVertexBuffers(0, *_SkyboxVertexBuffer->GetBuffer(), Offset);
                FrontgroundCommandBuffer->draw(36, 1, 0, 0);
                FrontgroundCommandBuffer.End();

                auto& PostProcessCommandBuffer = PostProcessCommandBuffers[i];

                vk::CommandBufferInheritanceInfo PostInheritanceInfo = vk::CommandBufferInheritanceInfo()
                    .setPNext(&PostInheritanceRenderingInfo);

                PostProcessCommandBuffer.Begin(PostInheritanceInfo, vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse);
                PostProcessCommandBuffer->setViewport(0, CommonViewport);
                PostProcessCommandBuffer->setScissor(0, CommonScissor);
                PostProcessCommandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, PostPipeline);
                PostProcessCommandBuffer->bindDescriptorBuffersEXT(PostDescriptorBufferBindingInfo);
                PostProcessCommandBuffer->pushConstants<vk::Bool32>(PostPipelineLayout, vk::ShaderStageFlagBits::eFragment, PostShader->GetPushConstantOffset("bEnableHdr"), static_cast<vk::Bool32>(_bEnableHdr));
                OffsetSet0 = ShaderBufferManager->GetDescriptorBindingOffset("PostDescriptorBuffer", 0, 0);
                PostProcessCommandBuffer->setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, PostPipelineLayout, 0, { 0 }, OffsetSet0);
                PostProcessCommandBuffer->bindVertexBuffers(0, *_QuadVertexBuffer->GetBuffer(), Offset);
                PostProcessCommandBuffer->draw(6, 1, 0, 0);
                PostProcessCommandBuffer.End();
            }
        };

        RecordSecondaryCommands();

        _VulkanContext->RegisterAutoRemovedCallbacks(
            FVulkanContext::ECallbackType::kCreateSwapchain, "RecordSecondaryCommands", RecordSecondaryCommands);

        // Main rendering loop
        while (!glfwWindowShouldClose(_Window))
        {
            while (glfwGetWindowAttrib(_Window, GLFW_ICONIFIED))
            {
                glfwWaitEvents();
            }

            vk::Extent2D SupersamplingExtent(_WindowSize.width * 2, _WindowSize.height * 2);

            vk::Viewport CommonViewport(0.0f, 0.0f, static_cast<float>(SupersamplingExtent.width),
                                        static_cast<float>(SupersamplingExtent.height), 0.0f, 1.0f);

            vk::Viewport DepthMapViewport(0.0f, 0.0f, static_cast<float>(DepthMapExtent.width),
                                          static_cast<float>(DepthMapExtent.height), 0.0f, 1.0f);

            vk::Rect2D CommonScissor(vk::Offset2D(), SupersamplingExtent);
            vk::Rect2D DepthMapScissor(vk::Offset2D(), DepthMapExtent);

            InFlightFences[CurrentFrame].WaitAndReset();

            // Uniform update
            // --------------
            float WindowAspect = static_cast<float>(_WindowSize.width) / static_cast<float>(_WindowSize.height);

            Matrices.View             = _Camera->GetViewMatrix();
            Matrices.Projection       = _Camera->GetProjectionMatrix(WindowAspect, 0.001f);
            Matrices.LightSpaceMatrix = LightSpaceMatrix;

            // MvpMatrices.Model      = glm::mat4x4(1.0f);
            // MvpMatrices.View       = _Camera->GetViewMatrix();
            // MvpMatrices.Projection = _Camera->GetProjectionMatrix(WindowAspect, 0.001f);

            ShaderBufferManager->UpdateDataBuffer(CurrentFrame, "Matrices", Matrices);
            // ShaderBufferManager->UpdateDataBuffer(CurrentFrame, "MvpMatrices", MvpMatrices);

            CameraPosUpdater[CurrentFrame] << _Camera->GetVector(FCamera::EVector::kPosition);

            _VulkanContext->SwapImage(*Semaphores_ImageAvailable[CurrentFrame]);
            std::uint32_t ImageIndex = _VulkanContext->GetCurrentImageIndex();

            // Record commands
            // ---------------
            auto& CurrentBuffer = CommandBuffers[CurrentFrame];
            CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

            vk::ImageMemoryBarrier2 InitSwapchainBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                _VulkanContext->GetSwapchainImage(ImageIndex),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 InitPositionAoBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_PositionAoAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 InitNormalRoughBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_NormalRoughAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 InitAlbedoMetalBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_AlbedoMetalAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 InitShadowBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_ShadowAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 InitColorAttachmentBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eGeneral,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_ColorAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 InitResolveAttachmentBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_ResolveAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 InitDepthMapAttachmentBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eLateFragmentTests,
                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_DepthMapAttachment->GetImage(),
                DepthSubresourceRange
            );

            std::array InitBarriers{ InitSwapchainBarrier, InitPositionAoBarrier, InitNormalRoughBarrier, InitShadowBarrier,
                                     InitAlbedoMetalBarrier, InitColorAttachmentBarrier, InitResolveAttachmentBarrier, InitDepthMapAttachmentBarrier };

            vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(InitBarriers);

            CurrentBuffer->pipelineBarrier2(InitialDependencyInfo);

            vk::RenderingInfo DepthMapRenderingInfo = vk::RenderingInfo()
                .setFlags(vk::RenderingFlagBits::eContentsSecondaryCommandBuffers)
                .setRenderArea(DepthMapScissor)
                .setLayerCount(1)
                .setPDepthAttachment(&_DepthMapAttachmentInfo);

            CurrentBuffer->beginRendering(DepthMapRenderingInfo);
            // Draw scene for depth mapping
            CurrentBuffer->executeCommands(*DepthMapCommandBuffers[CurrentFrame]);
            CurrentBuffer->endRendering();

            vk::ImageMemoryBarrier2 DepthMapRenderEndBarrier(
                vk::PipelineStageFlagBits2::eLateFragmentTests,
                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eDepthAttachmentOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_DepthMapAttachment->GetImage(),
                DepthSubresourceRange
            );

            vk::DependencyInfo DepthMapRenderEndDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(DepthMapRenderEndBarrier);

            CurrentBuffer->pipelineBarrier2(DepthMapRenderEndDependencyInfo);

            std::array GBufferAttachments{ _PositionAoAttachmentInfo, _NormalRoughAttachmentInfo,
                                           _AlbedoMetalAttachmentInfo, _ShadowAttachmentInfo };

            vk::RenderingInfo SceneRenderingInfo = vk::RenderingInfo()
                .setFlags(vk::RenderingFlagBits::eContentsSecondaryCommandBuffers)
                .setRenderArea(CommonScissor)
                .setLayerCount(1)
                .setColorAttachments(GBufferAttachments)
                .setPDepthAttachment(&_DepthStencilAttachmentInfo);

            CurrentBuffer->beginRendering(SceneRenderingInfo);
            CurrentBuffer->executeCommands(*SceneGBufferCommandBuffers[CurrentFrame]);
            CurrentBuffer->endRendering();

            // CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, TerrainPipeline);
            // CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, TerrainPipelineLayout, 0,
            //                                   TerrainShader->GetDescriptorSets(CurrentFrame), {});
            // CurrentBuffer->pushConstants(TerrainPipelineLayout, vk::ShaderStageFlagBits::eTessellationControl, 0,
            //                              static_cast<std::uint32_t>(TessArgs.size()) * sizeof(float), TessArgs.data());
            // CurrentBuffer->bindVertexBuffers(0, *_TerrainVertexBuffer->GetBuffer(), Offset);
            // CurrentBuffer->draw(_TessResolution* _TessResolution * 4, 1, 0, 0);

            // CurrentBuffer->endRendering();

            vk::ImageMemoryBarrier2 PositionAoRenderEndBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_PositionAoAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 NormalRoughRenderEndBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_NormalRoughAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 AlbedoMetalRenderEndBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_AlbedoMetalAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 ShadowRenderEndBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_ShadowAttachment->GetImage(),
                ColorSubresourceRange
            );

            std::array GBufferRenderEndBarriers{ PositionAoRenderEndBarrier, ShadowRenderEndBarrier,
                                                 NormalRoughRenderEndBarrier, AlbedoMetalRenderEndBarrier };

            vk::DependencyInfo GBufferRenderEndDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(GBufferRenderEndBarriers);

            CurrentBuffer->pipelineBarrier2(GBufferRenderEndDependencyInfo);

            // Compute merge G-buffers
            CurrentBuffer->executeCommands(*SceneMergeCommandBuffers[CurrentFrame]);

            vk::ImageMemoryBarrier2 MergeEndBarrier(
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eGeneral,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_ColorAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::DependencyInfo MergeEndDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(MergeEndBarrier);

            CurrentBuffer->pipelineBarrier2(MergeEndDependencyInfo);

            // Draw other objects
            SceneRenderingInfo.setColorAttachmentCount(1).setColorAttachments(_ColorAttachmentInfo);
            _ColorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);
            _DepthStencilAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);

            CurrentBuffer->beginRendering(SceneRenderingInfo);
            CurrentBuffer->executeCommands(*FrontgroundCommandBuffers[CurrentFrame]);
            CurrentBuffer->endRendering();

            _ColorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);
            _DepthStencilAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);

            vk::ImageMemoryBarrier2 PrePostBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_ColorAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::DependencyInfo PrePostDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PrePostBarrier);

            CurrentBuffer->pipelineBarrier2(PrePostDependencyInfo);

            // Post process
            vk::RenderingInfo PostRenderingInfo = vk::RenderingInfo()
                .setFlags(vk::RenderingFlagBits::eContentsSecondaryCommandBuffers)
                .setRenderArea(CommonScissor)
                .setLayerCount(1)
                .setColorAttachments(_PostProcessAttachmentInfo);

            CurrentBuffer->beginRendering(PostRenderingInfo);
            CurrentBuffer->executeCommands(*PostProcessCommandBuffers[CurrentFrame]);
            CurrentBuffer->endRendering();

            vk::ImageMemoryBarrier2 PreBlitBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *_ResolveAttachment->GetImage(),
                ColorSubresourceRange
            );

            vk::DependencyInfo BlitDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PreBlitBarrier);

            CurrentBuffer->pipelineBarrier2(BlitDependencyInfo);

            // Blit SSAA to swapchain image
            vk::ImageBlit BlitRegion(
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                { vk::Offset3D(0, 0, 0), vk::Offset3D(SupersamplingExtent.width, SupersamplingExtent.height, 1) },
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                { vk::Offset3D(0, 0, 0), vk::Offset3D(_WindowSize.width, _WindowSize.height, 1) }
            );

            CurrentBuffer->blitImage(*_ResolveAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                                     _VulkanContext->GetSwapchainImage(ImageIndex), vk::ImageLayout::eTransferDstOptimal,
                                     BlitRegion, vk::Filter::eLinear);

            vk::ImageMemoryBarrier2 PresentBarrier(
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eBottomOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::ePresentSrcKHR,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                _VulkanContext->GetSwapchainImage(ImageIndex),
                ColorSubresourceRange
            );

            vk::DependencyInfo FinalDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PresentBarrier);

            CurrentBuffer->pipelineBarrier2(FinalDependencyInfo);
            CurrentBuffer.End();

            _VulkanContext->SubmitCommandBufferToGraphics(*CurrentBuffer, *Semaphores_ImageAvailable[CurrentFrame],
                                                          *Semaphores_RenderFinished[CurrentFrame], *InFlightFences[CurrentFrame]);
            _VulkanContext->PresentImage(*Semaphores_RenderFinished[CurrentFrame]);

            CurrentFrame = (CurrentFrame + 1) % Config::Graphics::kMaxFrameInFlight;

            _Camera->ProcessEvent(_DeltaTime);

            ProcessInput();
            glfwPollEvents();
            ShowTitleFps();
        }

        _VulkanContext->WaitIdle();
        _VulkanContext->GetGraphicsCommandPool().FreeBuffers(CommandBuffers);
    }

    void FApplication::CreateAttachments()
    {
        _PositionAoAttachmentInfo
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

        _NormalRoughAttachmentInfo
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

        _AlbedoMetalAttachmentInfo
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

        _ShadowAttachmentInfo
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

        _ColorAttachmentInfo
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            // .setResolveMode(vk::ResolveModeFlagBits::eAverage)
            // .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

        _DepthStencilAttachmentInfo
            .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

        _DepthMapAttachmentInfo
            .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

        _PostProcessAttachmentInfo
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

        auto CreateFramebuffers = [&]() -> void
        {
            VmaAllocationCreateInfo AllocationCreateInfo
            {
                .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            };

            vk::Extent2D AttachmentExtent(_WindowSize.width * 2, _WindowSize.height * 2);
            vk::Extent2D DepthMapExtent(8192, 8192);

            _VulkanContext->WaitIdle();

            _PositionAoAttachment = std::make_unique<FColorAttachment>(
                AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat, AttachmentExtent, 1,
                vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

            _NormalRoughAttachment = std::make_unique<FColorAttachment>(
                AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat, AttachmentExtent, 1,
                vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

            _AlbedoMetalAttachment = std::make_unique<FColorAttachment>(
                AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat, AttachmentExtent, 1,
                vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

            _ShadowAttachment = std::make_unique<FColorAttachment>(
                AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat, AttachmentExtent, 1,
                vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

            _ColorAttachment = std::make_unique<FColorAttachment>(
                AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat, AttachmentExtent, 1,
                vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);

            _ResolveAttachment = std::make_unique<FColorAttachment>(
                AllocationCreateInfo, _VulkanContext->GetSwapchainCreateInfo().imageFormat, AttachmentExtent, 1,
                vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eTransferSrc);

            _DepthStencilAttachment = std::make_unique<FDepthStencilAttachment>(
                AllocationCreateInfo, vk::Format::eD32Sfloat, AttachmentExtent, 1, vk::SampleCountFlagBits::e1);

            _DepthMapAttachment = std::make_unique<FDepthStencilAttachment>(
                AllocationCreateInfo, vk::Format::eD32Sfloat, DepthMapExtent, 1,
                vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

            _PositionAoAttachmentInfo.setImageView(*_PositionAoAttachment->GetImageView());
            _NormalRoughAttachmentInfo.setImageView(*_NormalRoughAttachment->GetImageView());
            _AlbedoMetalAttachmentInfo.setImageView(*_AlbedoMetalAttachment->GetImageView());
            _ShadowAttachmentInfo.setImageView(*_ShadowAttachment->GetImageView());
            _ColorAttachmentInfo.setImageView(*_ColorAttachment->GetImageView());
            //                     .setResolveImageView(*_ResolveAttachment->GetImageView

            _DepthStencilAttachmentInfo.setImageView(*_DepthStencilAttachment->GetImageView());
            _DepthMapAttachmentInfo.setImageView(*_DepthMapAttachment->GetImageView());
            _PostProcessAttachmentInfo.setImageView(*_ResolveAttachment->GetImageView());
        };

        auto DestroyFramebuffers = [&]() -> void
        {
            _VulkanContext->WaitIdle();
        };

        CreateFramebuffers();

        _VulkanContext->RegisterAutoRemovedCallbacks(
            FVulkanContext::ECallbackType::kCreateSwapchain, "CreateFramebuffers", CreateFramebuffers);
        _VulkanContext->RegisterAutoRemovedCallbacks(
            FVulkanContext::ECallbackType::kDestroySwapchain, "DestroyFramebuffers", DestroyFramebuffers);
    }

    void FApplication::LoadAssets()
    {
        FShader::FResourceInfo PbrSceneGBufferResourceInfo
        {
            {
                { 0, sizeof(FVertex), false },
                { 1, sizeof(FInstanceData), true }
            },
            {
                { 0, 0, offsetof(FVertex, Position) },
                { 0, 1, offsetof(FVertex, Normal) },
                { 0, 2, offsetof(FVertex, TexCoord) },
                { 0, 3, offsetof(FVertex, Tangent) },
                { 0, 4, offsetof(FVertex, Bitangent) },
                { 1, 5, offsetof(FInstanceData, Model) }
            },
            {},
            { { vk::ShaderStageFlagBits::eVertex, { "MatricesAddress" } } }
        };

        FShader::FResourceInfo PbrSceneResourceInfo
        {
            {
                { 0, sizeof(FVertex), false },
                { 1, sizeof(FInstanceData), true }
            },
            {
                { 0, 0, offsetof(FVertex, Position) },
                { 0, 1, offsetof(FVertex, Normal) },
                { 0, 2, offsetof(FVertex, TexCoord) },
                { 0, 3, offsetof(FVertex, Tangent) },
                { 0, 4, offsetof(FVertex, Bitangent) },
                { 1, 5, offsetof(FInstanceData, Model) }
            },
            {},
            {
                { vk::ShaderStageFlagBits::eVertex, { "MatricesAddress" } },
                { vk::ShaderStageFlagBits::eFragment, { "LightArgsAddress" } }
            }
        };

        FShader::FResourceInfo PbrSceneMergeResourceInfo
        {
            {}, {}, {}, { { vk::ShaderStageFlagBits::eCompute, { "LightArgsAddress" } } }
        };

        FShader::FResourceInfo DepthMapResourceInfo
        {
            {
                { 0, sizeof(FVertex), false },
                { 1, sizeof(FInstanceData), true }
            },
            {
                { 0, 0, offsetof(FVertex, Position) },
                { 1, 1, offsetof(FInstanceData, Model) }
            },
            {},
            { { vk::ShaderStageFlagBits::eVertex, { "MatricesAddress" } } }
        };

        FShader::FResourceInfo TerrainResourceInfo
        {
            { { 0, sizeof(FPatchVertex), false } },
            {
                { 0, 0, offsetof(FPatchVertex, Position) },
                { 0, 1, offsetof(FPatchVertex, TexCoord) }
            },
            { { 0, 0, false } },
            { { vk::ShaderStageFlagBits::eTessellationControl, { "MinDistance", "MaxDistance", "MinTessLevel", "MaxTessLevel" } } }
        };

        FShader::FResourceInfo SkyboxResourceInfo
        {
            { { 0, sizeof(FSkyboxVertex), false } },
            { { 0, 0, offsetof(FSkyboxVertex, Position) } },
            {},
            { { vk::ShaderStageFlagBits::eVertex, { "MatricesAddress" } } }
        };

        FShader::FResourceInfo PostResourceInfo
        {
            { { 0, sizeof(FQuadVertex), false } },
            {
                { 0, 0, offsetof(FQuadVertex, Position) },
                { 0, 1, offsetof(FQuadVertex, TexCoord) }
            },
            {},
            { { vk::ShaderStageFlagBits::eFragment, { "bEnableHdr" } } }
        };

        std::vector<std::string> PbrSceneGBufferShaderFiles({ "PbrScene.vert.spv", "PbrSceneGBuffer.frag.spv" });
        std::vector<std::string> PbrSceneMergeShaderFiles({ "PbrSceneMerge.comp.spv" });
        std::vector<std::string> PbrSceneShaderFiles({ "PbrScene.vert.spv", "PbrScene.frag.spv" });
        std::vector<std::string> LampShaderFiles({ "PbrScene.vert.spv", "PbrScene_Lamp.frag.spv" });
        std::vector<std::string> DepthMapShaderFiles({ "DepthMap.vert.spv", "DepthMap.frag.spv" });
        std::vector<std::string> TerrainShaderFiles({ "Terrain.vert.spv", "Terrain.tesc.spv", "Terrain.tese.spv", "Terrain.frag.spv" });
        std::vector<std::string> SkyboxShaderFiles({ "Skybox.vert.spv", "Skybox.frag.spv" });
        std::vector<std::string> PostShaderFiles({ "PostProcess.vert.spv", "PostProcess.frag.spv" });

        VmaAllocationCreateInfo TextureAllocationCreateInfo
        {
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_GPU_ONLY
        };

        auto* AssetManager = FAssetManager::GetInstance();
        AssetManager->AddAsset<FShader>("PbrSceneGBufferShader", PbrSceneGBufferShaderFiles, PbrSceneGBufferResourceInfo);
        AssetManager->AddAsset<FShader>("PbrSceneMergeShader", PbrSceneMergeShaderFiles, PbrSceneMergeResourceInfo);
        AssetManager->AddAsset<FShader>("PbrSceneShader", PbrSceneShaderFiles, PbrSceneResourceInfo);
        AssetManager->AddAsset<FShader>("LampShader", LampShaderFiles, PbrSceneResourceInfo);
        AssetManager->AddAsset<FShader>("DepthMapShader", DepthMapShaderFiles, DepthMapResourceInfo);
        AssetManager->AddAsset<FShader>("TerrainShader", TerrainShaderFiles, TerrainResourceInfo);
        AssetManager->AddAsset<FShader>("SkyboxShader", SkyboxShaderFiles, SkyboxResourceInfo);
        AssetManager->AddAsset<FShader>("PostShader", PostShaderFiles, PostResourceInfo);

        std::vector<std::string> TextureNames
        {
            "PbrDisplacement",
            "PbrDiffuse",
            "PbrNormal",
            "PbrArm"
        };

        std::vector<std::string> TextureFiles
        {
            "CliffSide/cliff_side_disp_4k.jpg",
            "CliffSide/cliff_side_diff_4k_mipmapped_bc6h_u.ktx2",
            "CliffSide/cliff_side_nor_dx_4k_mipmapped_bc6h_u.ktx2",
            "CliffSide/cliff_side_arm_4k_mipmapped_bc6h_u.ktx2"
        };

        std::vector<vk::Format> InitialTextureFormats
        {
            vk::Format::eR8G8B8A8Unorm,
            vk::Format::eBc6HUfloatBlock,
            vk::Format::eBc6HUfloatBlock,
            vk::Format::eBc6HUfloatBlock
        };

        std::vector<vk::Format> FinalTextureFormats
        {
            vk::Format::eR8G8B8A8Unorm,
            vk::Format::eBc6HUfloatBlock,
            vk::Format::eBc6HUfloatBlock,
            vk::Format::eBc6HUfloatBlock
        };

#if !defined(_DEBUG)
        // TextureFiles =
        // {
        //     "CliffSide/cliff_side_disp_4k.exr",
        //     "CliffSide/cliff_side_diff_4k.exr",
        //     "CliffSide/cliff_side_nor_dx_4k.exr",
        //     "CliffSide/cliff_side_arm_4k.exr"
        // };
#endif

        AssetManager->AddAsset<FTextureCube>(
            "Skybox", TextureAllocationCreateInfo, "Skybox", vk::Format::eR8G8B8A8Srgb, vk::Format::eR8G8B8A8Srgb, vk::ImageCreateFlags(), true);

        //AssetManager->AddAsset<FTexture2D>(
        //    "PureSky", TextureAllocationCreateInfo, "HDRI/autumn_field_puresky_16k.hdr",
        //    vk::Format::eR16G16B16A16Sfloat, vk::Format::eR16G16B16A16Sfloat, vk::ImageCreateFlags(), false);

        // AssetManager->AddAsset<FTexture2D>(
        //     "IceLand", TextureAllocationCreateInfo, "IceLandHeightMapLowRes.png",
        //     vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false);

        // std::vector<std::future<void>> Futures;

        for (std::size_t i = 0; i != TextureNames.size(); ++i)
        {
            // Futures.push_back(_ThreadPool->Submit([&, i]() -> void
            // {
            AssetManager->AddAsset<FTexture2D>(
                TextureNames[i], TextureAllocationCreateInfo, TextureFiles[i], InitialTextureFormats[i], FinalTextureFormats[i], vk::ImageCreateFlagBits::eMutableFormat, true);
            // }));
        }

        // for (auto& Future : Futures)
        // {
        //     Future.get();
        // }
    }

    void FApplication::CreateUniformBuffers()
    {
        FShaderBufferManager::FDataBufferCreateInfo MatricesCreateInfo
        {
            .Name    = "Matrices",
            .Fields  = { "View", "Projection", "LightSpaceMatrix" },
            .Set     = 0,
            .Binding = 0,
            .Usage   = vk::DescriptorType::eUniformBuffer
        };

        FShaderBufferManager::FDataBufferCreateInfo MvpMatricesCreateInfo
        {
            .Name    = "MvpMatrices",
            .Fields  = { "Model", "View", "Projection" },
            .Set     = 0,
            .Binding = 0,
            .Usage   = vk::DescriptorType::eUniformBuffer
        };

        FShaderBufferManager::FDataBufferCreateInfo LightArgsCreateInfo
        {
            .Name    = "LightArgs",
            .Fields  = { "LightPos", "LightColor", "CameraPos" },
            .Set     = 0,
            .Binding = 0,
            .Usage   = vk::DescriptorType::eUniformBuffer
        };

        VmaAllocatorCreateInfo AllocatorCreateInfo
        {
            .flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = _VulkanContext->GetPhysicalDevice(),
            .device         = _VulkanContext->GetDevice(),
            .instance       = _VulkanContext->GetInstance()
        };

        VmaAllocationCreateInfo AllocationCreateInfo
        {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        };

        auto* ShaderBufferManager = FShaderBufferManager::GetInstance();
        ShaderBufferManager->CreateDataBuffers<FMatrices>(MatricesCreateInfo, &AllocationCreateInfo);
        ShaderBufferManager->CreateDataBuffers<FMvpMatrices>(MvpMatricesCreateInfo, &AllocationCreateInfo);
        ShaderBufferManager->CreateDataBuffers<FLightArgs>(LightArgsCreateInfo, &AllocationCreateInfo);
    }

    void FApplication::BindDescriptors()
    {
        auto* AssetManager = FAssetManager::GetInstance();

        auto* PbrSceneGBufferShader = AssetManager->GetAsset<FShader>("PbrSceneGBufferShader");
        auto* PbrSceneMergeShader   = AssetManager->GetAsset<FShader>("PbrSceneMergeShader");
        auto* PbrSceneShader        = AssetManager->GetAsset<FShader>("PbrSceneShader");
        auto* LampShader            = AssetManager->GetAsset<FShader>("LampShader");
        auto* DepthMapShader        = AssetManager->GetAsset<FShader>("DepthMapShader");
        auto* TerrainShader         = AssetManager->GetAsset<FShader>("TerrainShader");
        auto* SkyboxShader          = AssetManager->GetAsset<FShader>("SkyboxShader");
        auto* PostShader            = AssetManager->GetAsset<FShader>("PostShader");

        auto* PbrDisplacement = AssetManager->GetAsset<FTexture2D>("PbrDisplacement");
        auto* PbrDiffuse      = AssetManager->GetAsset<FTexture2D>("PbrDiffuse");
        auto* PbrNormal       = AssetManager->GetAsset<FTexture2D>("PbrNormal");
        auto* PbrArm          = AssetManager->GetAsset<FTexture2D>("PbrArm");
        auto* IceLand         = AssetManager->GetAsset<FTexture2D>("IceLand");
        auto* Skybox          = AssetManager->GetAsset<FTextureCube>("Skybox");

        auto* ShaderBufferManager = FShaderBufferManager::GetInstance();

        auto CreateAttachmentDescriptors = [=]() mutable -> void
        {
            VmaAllocationCreateInfo AllocationCreateInfo
            {
                .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            };

            FShaderBufferManager::FDescriptorBufferCreateInfo SceneGBufferDescriptorBufferCreateInfo;
            SceneGBufferDescriptorBufferCreateInfo.Name = "SceneGBufferDescriptorBuffer";
            SceneGBufferDescriptorBufferCreateInfo.SetSizes = std::move(PbrSceneGBufferShader->GetDescriptorSetSizes());

            vk::SamplerCreateInfo SamplerCreateInfo = FTexture::CreateDefaultSamplerCreateInfo();
            static FVulkanSampler kSampler(SamplerCreateInfo);
            SceneGBufferDescriptorBufferCreateInfo.SamplerInfos.emplace_back(0u, 0u, *kSampler);

            auto PbrDiffuseImageInfo = PbrDiffuse->CreateDescriptorImageInfo(nullptr);
            SceneGBufferDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(1u, 0u, PbrDiffuseImageInfo);

            auto PbrNormalImageInfo = PbrNormal->CreateDescriptorImageInfo(nullptr);
            SceneGBufferDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(1u, 1u, PbrNormalImageInfo);

            auto PbrArmImageInfo = PbrArm->CreateDescriptorImageInfo(nullptr);
            SceneGBufferDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(1u, 2u, PbrArmImageInfo);

            // auto HeightMapImageInfo = IceLand->CreateDescriptorImageInfo(kSampler);
            // TerrainShader->WriteSharedDescriptors<vk::DescriptorImageInfo>(1, 0, vk::DescriptorType::eCombinedImageSampler, HeightMapImageInfo);

            FShaderBufferManager::FDescriptorBufferCreateInfo SkyboxDescriptorBufferCreateInfo;
            SkyboxDescriptorBufferCreateInfo.Name     = "SkyboxDescriptorBuffer";
            SkyboxDescriptorBufferCreateInfo.SetSizes = std::move(SkyboxShader->GetDescriptorSetSizes());

            SamplerCreateInfo
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge);

            static FVulkanSampler kSkyboxSampler(SamplerCreateInfo);
            auto SkyboxImageInfo = Skybox->CreateDescriptorImageInfo(kSkyboxSampler);
            SkyboxDescriptorBufferCreateInfo.CombinedImageSamplerInfos.emplace_back(0u, 0u, SkyboxImageInfo);

            FShaderBufferManager::FDescriptorBufferCreateInfo SceneMergeDescriptorBufferCreateInfo;
            SceneMergeDescriptorBufferCreateInfo.Name     = "SceneMergeDescriptorBuffer";
            SceneMergeDescriptorBufferCreateInfo.SetSizes = std::move(PbrSceneMergeShader->GetDescriptorSetSizes());

            FShaderBufferManager::FDescriptorBufferCreateInfo PostDescriptorBufferCreateInfo;
            PostDescriptorBufferCreateInfo.Name     = "PostDescriptorBuffer";
            PostDescriptorBufferCreateInfo.SetSizes = std::move(PostShader->GetDescriptorSetSizes());

            vk::SamplerCustomBorderColorCreateInfoEXT BorderColorCreateInfo(
                vk::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f), vk::Format::eR32G32B32A32Sfloat);

            SamplerCreateInfo
                .setPNext(&BorderColorCreateInfo)
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToBorder)
                .setAddressModeV(vk::SamplerAddressMode::eClampToBorder)
                .setAddressModeW(vk::SamplerAddressMode::eClampToBorder)
                .setAnisotropyEnable(vk::False)
                .setMinLod(0.0f)
                .setMaxLod(0.0f)
                .setBorderColor(vk::BorderColor::eFloatCustomEXT);

            static FVulkanSampler kFramebufferSampler(SamplerCreateInfo);

            vk::DescriptorImageInfo ColorStorageImageInfo(
                *kFramebufferSampler, *_ColorAttachment->GetImageView(), vk::ImageLayout::eGeneral);
            vk::DescriptorImageInfo ColorImageInfo(
                *kFramebufferSampler, *_ColorAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::DescriptorImageInfo DepthMapImageInfo(
                *kFramebufferSampler, *_DepthMapAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::DescriptorImageInfo PositionAoImageInfo(
                *kFramebufferSampler, *_PositionAoAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::DescriptorImageInfo NormalRoughImageInfo(
                *kFramebufferSampler, *_NormalRoughAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::DescriptorImageInfo AlbedoMetalImageInfo(
                *kFramebufferSampler, *_AlbedoMetalAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::DescriptorImageInfo ShadowImageInfo(
                *kFramebufferSampler, *_ShadowAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

            SceneGBufferDescriptorBufferCreateInfo.CombinedImageSamplerInfos.emplace_back(2u, 0u, DepthMapImageInfo);
            SceneMergeDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(0u, 0u, PositionAoImageInfo);
            SceneMergeDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(0u, 1u, NormalRoughImageInfo);
            SceneMergeDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(0u, 2u, AlbedoMetalImageInfo);
            SceneMergeDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(0u, 3u, ShadowImageInfo);
            SceneMergeDescriptorBufferCreateInfo.StorageImageInfos.emplace_back(1u, 0u, ColorStorageImageInfo);
            PostDescriptorBufferCreateInfo.CombinedImageSamplerInfos.emplace_back(0u, 0u, ColorImageInfo);

            ShaderBufferManager->CreateDescriptorBuffer(SceneGBufferDescriptorBufferCreateInfo, &AllocationCreateInfo);
            ShaderBufferManager->CreateDescriptorBuffer(SceneMergeDescriptorBufferCreateInfo, &AllocationCreateInfo);
            ShaderBufferManager->CreateDescriptorBuffer(SkyboxDescriptorBufferCreateInfo, &AllocationCreateInfo);
            ShaderBufferManager->CreateDescriptorBuffer(PostDescriptorBufferCreateInfo, &AllocationCreateInfo);
        };

        auto DestroyAttachmentDescriptors = [=]() mutable -> void
        {
            ShaderBufferManager->RemoveDescriptorBuffer("GBufferSceneDescriptorBuffer");
            ShaderBufferManager->RemoveDescriptorBuffer("GBufferMergeDescriptorBuffer");
            ShaderBufferManager->RemoveDescriptorBuffer("SkyboxDescriptorBuffer");
            ShaderBufferManager->RemoveDescriptorBuffer("PostDescriptorBuffer");
        };

        CreateAttachmentDescriptors();

        _VulkanContext->RegisterAutoRemovedCallbacks(
            FVulkanContext::ECallbackType::kCreateSwapchain, "CreateAttachmentDescriptors", CreateAttachmentDescriptors);
        _VulkanContext->RegisterAutoRemovedCallbacks(
            FVulkanContext::ECallbackType::kDestroySwapchain, "DestroyAttachmentDescriptors", DestroyAttachmentDescriptors);
    }

    void FApplication::InitializeInstanceData()
    {
        glm::mat4x4 Model(1.0f);
        // plane
        _InstanceData.emplace_back(Model);

        // cube
        Model = glm::mat4x4(1.0f);
        Model = glm::translate(Model, glm::vec3(0.0f, 1.5f, 0.0f));
        _InstanceData.emplace_back(Model);

        Model = glm::mat4x4(1.0f);
        Model = glm::translate(Model, glm::vec3(2.0f, 0.0f, 1.0f));
        _InstanceData.emplace_back(Model);

        Model = glm::mat4x4(1.0f);
        Model = glm::translate(Model, glm::vec3(-1.0f, 0.0f, 2.0f));
        Model = glm::scale(Model, glm::vec3(0.5f));
        Model = glm::rotate(Model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
        _InstanceData.emplace_back(Model);

        // sphere
        Model = glm::mat4x4(1.0f);
        _InstanceData.emplace_back(Model);

        // lamp
        glm::vec3 LightPos(-2.0f, 4.0f, -1.0f);
        Model = glm::mat4x4(1.0f);
        Model = glm::scale(glm::translate(Model, LightPos), glm::vec3(0.2f));
        _InstanceData.emplace_back(Model);
    }

    void FApplication::InitializeVerticesData()
    {
#include "Vertices.inc"

        // Create sphere vertices
        // ----------------------
        std::vector<glm::vec3> Positions;
        std::vector<glm::vec3> Normals;
        std::vector<glm::vec2> TexCoords;

        const std::uint32_t kSegmentsX = 4;
        const std::uint32_t kSegmentsY = 4;

        for (std::uint32_t x = 0; x <= kSegmentsX; ++x)
        {
            for (std::uint32_t y = 0; y <= kSegmentsY; ++y)
            {
                float SegmentX = static_cast<float>(x) / static_cast<float>(kSegmentsX);
                float SegmentY = static_cast<float>(y) / static_cast<float>(kSegmentsY);

                // 使用球面参数方程计算顶点坐标
                // x = cos(φ) * sin(θ)
                // y = cos(θ)
                // z = sin(φ) * sin(θ)
                // 其中 φ ∈ [0, 2π], θ ∈ [0, π]
                float PositionX = std::cos(SegmentX * 2.0f * Math::kPi) * std::sin(SegmentY * Math::kPi);
                float PositionY = std::cos(SegmentY * Math::kPi);
                float PositionZ = std::sin(SegmentX * 2.0f * Math::kPi) * std::sin(SegmentY * Math::kPi);

                Positions.emplace_back(PositionX, PositionY, PositionZ);
                Normals.emplace_back(PositionX, PositionY, PositionZ);
                TexCoords.emplace_back(SegmentX, SegmentY);
            }
        }

        std::vector<std::uint32_t> SphereIndices;
        bool bIsOddRow = false;
        for (std::uint32_t y = 0; y != kSegmentsY; ++y)
        {
            if (!bIsOddRow)
            {
                for (std::uint32_t x = 0; x <= kSegmentsX; ++x)
                {
                    SphereIndices.push_back(y * (kSegmentsX + 1) + x);
                    SphereIndices.push_back((y + 1) * (kSegmentsX + 1) + x);
                }
            }
            else
            {
                for (std::int32_t x = kSegmentsX; x >= 0; --x)
                {
                    SphereIndices.push_back((y + 1) * (kSegmentsX + 1) + x);
                    SphereIndices.push_back(y * (kSegmentsX + 1) + x);
                }
            }

            bIsOddRow = !bIsOddRow;
        }

        std::vector<FVertex> SphereVertices;
        for (std::size_t i = 0; i != Positions.size(); ++i)
        {
            FVertex Vertex{};
            Vertex.Position = Positions[i];
            Vertex.Normal = Normals[i];
            Vertex.TexCoord = TexCoords[i];

            SphereVertices.push_back(Vertex);
        }

        VmaAllocationCreateInfo AllocationCreateInfo
        {
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };

        Math::CalculateAllTangents(CubeVertices);
        Math::CalculateAllTangents(PlaneVertices);
        Math::CalculateTangentBitangent(kSegmentsX, kSegmentsY, SphereVertices);

        vk::BufferCreateInfo VertexBufferCreateInfo = vk::BufferCreateInfo()
            .setSize(SphereVertices.size() * sizeof(FVertex))
            .setUsage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

        _SphereVertexBuffer = std::make_unique<FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
        _SphereVertexBuffer->CopyData(SphereVertices);

        VertexBufferCreateInfo.setSize(CubeVertices.size() * sizeof(FVertex));
        _CubeVertexBuffer = std::make_unique<FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
        _CubeVertexBuffer->CopyData(CubeVertices);

        VertexBufferCreateInfo.setSize(SkyboxVertices.size() * sizeof(FSkyboxVertex));
        _SkyboxVertexBuffer = std::make_unique<FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
        _SkyboxVertexBuffer->CopyData(SkyboxVertices);

        VertexBufferCreateInfo.setSize(PlaneVertices.size() * sizeof(FVertex));
        _PlaneVertexBuffer = std::make_unique<FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
        _PlaneVertexBuffer->CopyData(PlaneVertices);

        VertexBufferCreateInfo.setSize(QuadVertices.size() * sizeof(FQuadVertex));
        _QuadVertexBuffer = std::make_unique<FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
        _QuadVertexBuffer->CopyData(QuadVertices);

        VertexBufferCreateInfo.setSize(_InstanceData.size() * sizeof(FInstanceData));
        _InstanceBuffer = std::make_unique<FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
        _InstanceBuffer->CopyData(_InstanceData);

        vk::BufferCreateInfo IndexBufferCreateInfo = vk::BufferCreateInfo()
            .setSize(SphereIndices.size() * sizeof(std::uint32_t))
            .setUsage(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);

        _SphereIndexBuffer = std::make_unique<FDeviceLocalBuffer>(AllocationCreateInfo, IndexBufferCreateInfo);
        _SphereIndexBuffer->CopyData(SphereIndices);
        _SphereIndicesCount = static_cast<std::uint32_t>(SphereIndices.size());

#if 0
        // Create height map vertices
        // --------------------------
        auto* AssetManager = FAssetManager::GetInstance();
        auto* IceLand      = AssetManager->GetAsset<FTexture2D>("IceLand");

        int   ImageWidth  = static_cast<int>(IceLand->GetImageExtent().width);
        int   ImageHeight = static_cast<int>(IceLand->GetImageExtent().height);
        int   StartX      = -ImageWidth  / 2;
        int   StartZ      = -ImageHeight / 2;
        float TessWidth   =  ImageWidth  / static_cast<float>(_TessResolution);
        float TessHeight  =  ImageHeight / static_cast<float>(_TessResolution);

        std::vector<FPatchVertex> TerrainVertices;
        TerrainVertices.reserve(_TessResolution * _TessResolution * 4);
        for (int z = 0; z != _TessResolution; ++z)
        {
            for (int x = 0; x != _TessResolution; ++x)
            {
                float AxisX0 = StartX + TessWidth  *  x;
                float AxisX1 = StartX + TessWidth  * (x + 1);
                float AxisZ0 = StartZ + TessHeight *  z;
                float AxisZ1 = StartZ + TessHeight * (z + 1);

                float AxisU0 =  x      / static_cast<float>(_TessResolution);
                float AxisU1 = (x + 1) / static_cast<float>(_TessResolution);
                float AxisV0 =  z      / static_cast<float>(_TessResolution);
                float AxisV1 = (z + 1) / static_cast<float>(_TessResolution);

                FPatchVertex PatchVertex00{ glm::vec3(AxisX0, 0.0f, AxisZ0), glm::vec2(AxisU0, AxisV0) };
                FPatchVertex PatchVertex01{ glm::vec3(AxisX0, 0.0f, AxisZ1), glm::vec2(AxisU0, AxisV1) };
                FPatchVertex PatchVertex11{ glm::vec3(AxisX1, 0.0f, AxisZ1), glm::vec2(AxisU1, AxisV1) };
                FPatchVertex PatchVertex10{ glm::vec3(AxisX1, 0.0f, AxisZ0), glm::vec2(AxisU1, AxisV0) };

                TerrainVertices.push_back(PatchVertex00);
                TerrainVertices.push_back(PatchVertex01);
                TerrainVertices.push_back(PatchVertex11);
                TerrainVertices.push_back(PatchVertex10);
            }
        }

        VertexBufferCreateInfo.setSize(TerrainVertices.size() * sizeof(FPatchVertex));
        _TerrainVertexBuffer = std::make_unique<FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
        _TerrainVertexBuffer->CopyData(TerrainVertices);
#endif
    }

    void FApplication::CreatePipelines()
    {
        auto* PipelineManager = FPipelineManager::GetInstance();

        FGraphicsPipelineCreateInfoPack ScenePipelineCreateInfoPack;
        ScenePipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eViewport);
        ScenePipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eScissor);

        vk::Format AttachmentFormat = vk::Format::eR16G16B16A16Sfloat;
        vk::PipelineRenderingCreateInfo SceneRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
            .setColorAttachmentCount(1)
            .setColorAttachmentFormats(AttachmentFormat)
            .setDepthAttachmentFormat(vk::Format::eD32Sfloat);

        ScenePipelineCreateInfoPack.GraphicsPipelineCreateInfo.setFlags(vk::PipelineCreateFlagBits::eDescriptorBufferEXT);
        ScenePipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&SceneRenderingCreateInfo);
        ScenePipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
        ScenePipelineCreateInfoPack.RasterizationStateCreateInfo
            .setCullMode(vk::CullModeFlagBits::eBack)
            .setFrontFace(vk::FrontFace::eCounterClockwise);

        //ScenePipelineCreateInfoPack.MultisampleStateCreateInfo
        //    .setRasterizationSamples(vk::SampleCountFlagBits::e8)
        //    .setSampleShadingEnable(vk::True)
        //    .setMinSampleShading(1.0f);

        ScenePipelineCreateInfoPack.DepthStencilStateCreateInfo
            .setDepthTestEnable(vk::True)
            .setDepthWriteEnable(vk::True)
            .setDepthCompareOp(vk::CompareOp::eLess)
            .setDepthBoundsTestEnable(vk::False)
            .setStencilTestEnable(vk::False);

        vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                               vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

        ScenePipelineCreateInfoPack.ColorBlendAttachmentStates.push_back(ColorBlendAttachmentState);

        PipelineManager->CreateGraphicsPipeline("PbrScenePipeline", "PbrSceneShader", ScenePipelineCreateInfoPack);
        PipelineManager->CreateGraphicsPipeline("LampPipeline", "LampShader", ScenePipelineCreateInfoPack);

        std::array GBufferAttachmentFormats{ AttachmentFormat, AttachmentFormat, AttachmentFormat, AttachmentFormat };

        vk::PipelineRenderingCreateInfo SceneGBufferRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
            .setColorAttachmentCount(4)
            .setColorAttachmentFormats(GBufferAttachmentFormats)
            .setDepthAttachmentFormat(vk::Format::eD32Sfloat);

        FGraphicsPipelineCreateInfoPack SceneGBufferPipelineCreateInfoPack = ScenePipelineCreateInfoPack;
        SceneGBufferPipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&SceneGBufferRenderingCreateInfo);

        for (int i = 0; i != 3; ++i)
        {
            SceneGBufferPipelineCreateInfoPack.ColorBlendAttachmentStates.push_back(ColorBlendAttachmentState);
        }

        PipelineManager->CreateGraphicsPipeline("PbrSceneGBufferPipeline", "PbrSceneGBufferShader", SceneGBufferPipelineCreateInfoPack);

        FGraphicsPipelineCreateInfoPack TerrainPipelineCreateInfoPack = ScenePipelineCreateInfoPack;
        TerrainPipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::ePatchList);

        vk::PipelineTessellationDomainOriginStateCreateInfo TessellationDomainOriginStateCreateInfo(vk::TessellationDomainOrigin::eUpperLeft);
        TerrainPipelineCreateInfoPack.TessellationStateCreateInfo
            .setPNext(&TessellationDomainOriginStateCreateInfo)
            .setPatchControlPoints(4);

        TerrainPipelineCreateInfoPack.RasterizationStateCreateInfo
            .setCullMode(vk::CullModeFlagBits::eBack)
            .setFrontFace(vk::FrontFace::eCounterClockwise)
            .setPolygonMode(vk::PolygonMode::eLine);

        PipelineManager->CreateGraphicsPipeline("TerrainPipeline", "TerrainShader", TerrainPipelineCreateInfoPack);

        FGraphicsPipelineCreateInfoPack SkyboxPipelineCreateInfoPack = ScenePipelineCreateInfoPack;
        SkyboxPipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
        SkyboxPipelineCreateInfoPack.RasterizationStateCreateInfo = vk::PipelineRasterizationStateCreateInfo();
        SkyboxPipelineCreateInfoPack.DepthStencilStateCreateInfo.setDepthCompareOp(vk::CompareOp::eLessOrEqual);

        PipelineManager->CreateGraphicsPipeline("SkyboxPipeline", "SkyboxShader", SkyboxPipelineCreateInfoPack);

        FGraphicsPipelineCreateInfoPack PostPipelineCreateInfoPack = ScenePipelineCreateInfoPack;

        vk::PipelineRenderingCreateInfo PostRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
            .setColorAttachmentCount(1)
            .setColorAttachmentFormats(_VulkanContext->GetSwapchainCreateInfo().imageFormat);

        PostPipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&PostRenderingCreateInfo);
        PostPipelineCreateInfoPack.DepthStencilStateCreateInfo  = vk::PipelineDepthStencilStateCreateInfo();
        PostPipelineCreateInfoPack.RasterizationStateCreateInfo = vk::PipelineRasterizationStateCreateInfo();
        PostPipelineCreateInfoPack.MultisampleStateCreateInfo   = vk::PipelineMultisampleStateCreateInfo();

        PipelineManager->CreateGraphicsPipeline("PostPipeline", "PostShader", PostPipelineCreateInfoPack);

        FGraphicsPipelineCreateInfoPack DepthMapPipelineCreateInfoPack = ScenePipelineCreateInfoPack;

        vk::PipelineRenderingCreateInfo DepthMapRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
            .setDepthAttachmentFormat(vk::Format::eD32Sfloat);

        DepthMapPipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&DepthMapRenderingCreateInfo);
        DepthMapPipelineCreateInfoPack.RasterizationStateCreateInfo = vk::PipelineRasterizationStateCreateInfo();
        DepthMapPipelineCreateInfoPack.MultisampleStateCreateInfo   = vk::PipelineMultisampleStateCreateInfo();

        PipelineManager->CreateGraphicsPipeline("DepthMapPipeline", "DepthMapShader", DepthMapPipelineCreateInfoPack);

        vk::ComputePipelineCreateInfo PbrMergePipelineCreateInfo;
        PbrMergePipelineCreateInfo.setFlags(vk::PipelineCreateFlagBits::eDescriptorBufferEXT);
        PipelineManager->CreateComputePipeline("PbrSceneMergePipeline", "PbrSceneMergeShader", &PbrMergePipelineCreateInfo);
    }

    void FApplication::Terminate()
    {
        _VulkanContext->WaitIdle();
        glfwDestroyWindow(_Window);
        glfwTerminate();
    }

    bool FApplication::InitializeWindow()
    {
        if (glfwInit() == GLFW_FALSE)
        {
            NpgsCoreError("Failed to initialize GLFW.");
            return false;
        };

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        _Window = glfwCreateWindow(_WindowSize.width, _WindowSize.height, _WindowTitle.c_str(), nullptr, nullptr);
        if (_Window == nullptr)
        {
            NpgsCoreError("Failed to create GLFW window.");
            glfwTerminate();
            return false;
        }

        InitializeInputCallbacks();

        std::uint32_t ExtensionCount = 0;
        const char** Extensions = glfwGetRequiredInstanceExtensions(&ExtensionCount);
        if (Extensions == nullptr)
        {
            NpgsCoreError("Failed to get required instance extensions.");
            glfwDestroyWindow(_Window);
            glfwTerminate();
            return false;
        }

        for (std::uint32_t i = 0; i != ExtensionCount; ++i)
        {
            _VulkanContext->AddInstanceExtension(Extensions[i]);
        }

        vk::Result Result;
        if ((Result = _VulkanContext->CreateInstance()) != vk::Result::eSuccess)
        {
            glfwDestroyWindow(_Window);
            glfwTerminate();
            return false;
        }

        vk::SurfaceKHR Surface;
        if (glfwCreateWindowSurface(_VulkanContext->GetInstance(), _Window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&Surface)) != VK_SUCCESS)
        {
            NpgsCoreError("Failed to create window surface.");
            glfwDestroyWindow(_Window);
            glfwTerminate();
            return false;
        }
        _VulkanContext->SetSurface(Surface);

        if (_bEnableHdr)
        {
            _VulkanContext->SetHdrMetadata(GetHdrMetadata());
        }

        if (_VulkanContext->CreateDevice(0) != vk::Result::eSuccess ||
            _VulkanContext->CreateSwapchain(_WindowSize, _bEnableVSync, _bEnableHdr) != vk::Result::eSuccess)
        {
            return false;
        }

        _Camera = std::make_unique<FCamera>(glm::vec3(0.0f, 0.0f, 3.0f));

        return true;
    }

    void FApplication::InitializeInputCallbacks()
    {
        glfwSetWindowUserPointer(_Window, this);
        glfwSetFramebufferSizeCallback(_Window, &FApplication::FramebufferSizeCallback);
        glfwSetScrollCallback(_Window, nullptr);
        glfwSetScrollCallback(_Window, &FApplication::ScrollCallback);
    }

    void FApplication::ShowTitleFps()
    {
        static double CurrentTime   = 0.0;
        static double PreviousTime  = glfwGetTime();
        static double LastFrameTime = 0.0;
        static int    FrameCount    = 0;

        CurrentTime   = glfwGetTime();
        _DeltaTime    = CurrentTime - LastFrameTime;
        LastFrameTime = CurrentTime;
        ++FrameCount;
        if (CurrentTime - PreviousTime >= 1.0)
        {
            glfwSetWindowTitle(_Window, (std::string(_WindowTitle) + " " + std::to_string(FrameCount)).c_str());
            FrameCount   = 0;
            PreviousTime = CurrentTime;
        }
    }

    void FApplication::ProcessInput()
    {
        if (glfwGetMouseButton(_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            glfwSetCursorPosCallback(_Window, &FApplication::CursorPosCallback);
            glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        if (glfwGetMouseButton(_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
        {
            glfwSetCursorPosCallback(_Window, nullptr);
            glfwSetInputMode(_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            _bFirstMouse = true;
        }

        if (glfwGetKey(_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(_Window, GLFW_TRUE);
        if (glfwGetKey(_Window, GLFW_KEY_W) == GLFW_PRESS)
            _Camera->ProcessKeyboard(FCamera::EMovement::kForward, _DeltaTime);
        if (glfwGetKey(_Window, GLFW_KEY_S) == GLFW_PRESS)
            _Camera->ProcessKeyboard(FCamera::EMovement::kBack, _DeltaTime);
        if (glfwGetKey(_Window, GLFW_KEY_A) == GLFW_PRESS)
            _Camera->ProcessKeyboard(FCamera::EMovement::kLeft, _DeltaTime);
        if (glfwGetKey(_Window, GLFW_KEY_D) == GLFW_PRESS)
            _Camera->ProcessKeyboard(FCamera::EMovement::kRight, _DeltaTime);
        if (glfwGetKey(_Window, GLFW_KEY_R) == GLFW_PRESS)
            _Camera->ProcessKeyboard(FCamera::EMovement::kUp, _DeltaTime);
        if (glfwGetKey(_Window, GLFW_KEY_F) == GLFW_PRESS)
            _Camera->ProcessKeyboard(FCamera::EMovement::kDown, _DeltaTime);
        if (glfwGetKey(_Window, GLFW_KEY_Q) == GLFW_PRESS)
            _Camera->ProcessKeyboard(FCamera::EMovement::kRollLeft, _DeltaTime);
        if (glfwGetKey(_Window, GLFW_KEY_E) == GLFW_PRESS)
            _Camera->ProcessKeyboard(FCamera::EMovement::kRollRight, _DeltaTime);
        if (glfwGetKey(_Window, GLFW_KEY_C) == GLFW_PRESS)
            _Camera->AlignCamera();
    }

    vk::HdrMetadataEXT FApplication::GetHdrMetadata()
    {
        IDXGIFactory6* Factory6 = nullptr;
        if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&Factory6))))
        {
            NpgsCoreError("Failed to create DXGI factory.");
            return {};
        }

        HMONITOR       Monitor      = MonitorFromWindow(glfwGetWin32Window(_Window), MONITOR_DEFAULTTOPRIMARY);
        IDXGIAdapter1* Adapter1     = nullptr;
        IDXGIOutput*   Output       = nullptr;
        IDXGIOutput6*  Output6      = nullptr;
        UINT           AdapterIndex = 0;
        bool           bFound       = false;

        while (Factory6->EnumAdapters1(AdapterIndex++, &Adapter1) != DXGI_ERROR_NOT_FOUND)
        {
            UINT OutputIndex = 0;
            while (Adapter1->EnumOutputs(OutputIndex++, &Output) != DXGI_ERROR_NOT_FOUND)
            {
                DXGI_OUTPUT_DESC Desc;
                Output->GetDesc(&Desc);
                if (Desc.Monitor == Monitor)
                {
                    bFound = true;
                    break;
                }
                Output->Release();
                Output = nullptr;
            }

            if (bFound)
            {
                break;
            }

            Adapter1->Release();
            Adapter1 = nullptr;
        }

        vk::HdrMetadataEXT HdrMetadata;
        if (bFound && Output != nullptr)
        {
            if (SUCCEEDED(Output->QueryInterface(IID_PPV_ARGS(&Output6))))
            {
                DXGI_OUTPUT_DESC1 Desc1;
                if (SUCCEEDED(Output6->GetDesc1(&Desc1)))
                {
                    HdrMetadata.setDisplayPrimaryRed({ Desc1.RedPrimary[0], Desc1.RedPrimary[1] })
                               .setDisplayPrimaryGreen({ Desc1.GreenPrimary[0], Desc1.GreenPrimary[1] })
                               .setDisplayPrimaryBlue({ Desc1.BluePrimary[0], Desc1.BluePrimary[1] })
                               .setWhitePoint({ Desc1.WhitePoint[0], Desc1.WhitePoint[1] })
                               .setMaxLuminance(Desc1.MaxLuminance)
                               .setMinLuminance(Desc1.MinLuminance)
                               .setMaxContentLightLevel(Desc1.MaxLuminance)
                               .setMaxFrameAverageLightLevel(Desc1.MaxFullFrameLuminance);
                }

                Output6->Release();
                Output6 = nullptr;
            }

            Output->Release();
            Output = nullptr;
        }

        if (Adapter1 != nullptr)
        {
            Adapter1->Release();
            Adapter1 = nullptr;
        }

        Factory6->Release();
        Factory6 = nullptr;
        return HdrMetadata;
    }

    void FApplication::FramebufferSizeCallback(GLFWwindow* Window, int Width, int Height)
    {
        auto* App = reinterpret_cast<FApplication*>(glfwGetWindowUserPointer(Window));

        if (Width == 0 || Height == 0)
        {
            return;
        }

        App->_WindowSize.width  = Width;
        App->_WindowSize.height = Height;
        App->_VulkanContext->WaitIdle();
        App->_VulkanContext->RecreateSwapchain();
    }

    void FApplication::CursorPosCallback(GLFWwindow* Window, double PosX, double PosY)
    {
        auto* Application = static_cast<FApplication*>(glfwGetWindowUserPointer(Window));

        if (Application->_bFirstMouse)
        {
            Application->_LastX = PosX;
            Application->_LastY = PosY;
            Application->_bFirstMouse = false;
        }

        glm::vec2 Offset(PosX - Application->_LastX, Application->_LastY - PosY);

        Application->_LastX = PosX;
        Application->_LastY = PosY;
        Application->_Camera->SetTargetOffset(Offset);
    }

    void FApplication::ScrollCallback(GLFWwindow* Window, double OffsetX, double OffsetY)
    {
        auto* Application = static_cast<FApplication*>(glfwGetWindowUserPointer(Window));
        Application->_Camera->ProcessMouseScroll(OffsetY);
    }
} // namespace Npgs
