#include "stdafx.h"
#include "Application.hpp"

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
#include <Windows.h>
#include <winrt/Windows.Devices.Display.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Display.h>

#include "Engine/Core/Base/Config/EngineConfig.hpp"
#include "Engine/Core/Math/NumericConstants.hpp"
#include "Engine/Core/Math/TangentSpaceTools.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Runtime/AssetLoaders/Texture.hpp"
#include "Engine/Runtime/Managers/AssetManager.hpp"
#include "Engine/Runtime/Managers/PipelineManager.hpp"
#include "Engine/Runtime/Managers/ShaderBufferManager.hpp"
#include "Engine/Runtime/Managers/ShaderManager.hpp"
#include "Engine/System/Services/EngineServices.hpp"

#include "Engine/Runtime/Managers/ShaderManager.hpp"

namespace Npgs
{
    namespace
    {
        struct FLuminancePack
        {
            float MaxAverageFullFrameLuminance{};
            float MaxLuminance{};
            float MinLuminance{};
        };

        winrt::fire_and_forget GetLuminanceAsync(std::promise<FLuminancePack>& Promise) // TODO: select monitor
        {
            winrt::hstring PropertyName = TEXT("System.Devices.DeviceInstanceId");

            auto DeviceInformations = co_await winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
                winrt::Windows::Devices::Display::DisplayMonitor::GetDeviceSelector(),
                winrt::single_threaded_vector<winrt::hstring>({ PropertyName }));

            for (const auto& DeviceInformation : DeviceInformations)
            {
                if (DeviceInformation.Kind() != winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface)
                {
                    continue;
                }

                auto DisplayMonitor = co_await winrt::Windows::Devices::Display::DisplayMonitor::FromIdAsync(
                    winrt::unbox_value<winrt::hstring>(DeviceInformation.Properties().Lookup(PropertyName)));

                FLuminancePack LuminancePack;
                LuminancePack.MaxAverageFullFrameLuminance = DisplayMonitor.MaxAverageFullFrameLuminanceInNits();
                LuminancePack.MaxLuminance = DisplayMonitor.MaxLuminanceInNits();
                LuminancePack.MinLuminance = DisplayMonitor.MinLuminanceInNits();

                Promise.set_value(LuminancePack);
                co_return;
            }

            co_return;
        }

        FLuminancePack GetLuminanceSync()
        {
            winrt::init_apartment();
            std::promise<FLuminancePack> Promise;
            std::future<FLuminancePack> Future = Promise.get_future();

            GetLuminanceAsync(Promise);
            return Future.get();
        }
    }

    FApplication::FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle,
                               bool bEnableVSync, bool bEnableFullscreen, bool bEnableHdr)
        : VulkanContext_(EngineCoreServices->GetVulkanContext())
        , ThreadPool_(EngineCoreServices->GetThreadPool())
        , WindowTitle_(WindowTitle)
        , WindowSize_(WindowSize)
        , bEnableVSync_(bEnableVSync)
        , bEnableFullscreen_(bEnableFullscreen)
        , bEnableHdr_(bEnableHdr)
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

        auto* AssetManager        = EngineCoreServices->GetAssetManager();
        auto* ShaderBufferManager = EngineResourceServices->GetShaderBufferManager();
        auto* ShaderManager       = EngineResourceServices->GetShaderManager();

        FShaderAcquireInfo::FShaderInfo PbrSceneVertShaderInfo
        {
            .Name = "PbrSceneVertShader",
            .NextStage = vk::ShaderStageFlagBits::eFragment
        };

        FShaderAcquireInfo::FShaderInfo PbrSceneGbufferShaderInfo
        {
            .Name = "PbrSceneGbufferShader"
        };

        FShaderAcquireInfo::FShaderInfo LampShaderInfo
        {
            .Name = "LampShader"
        };

        FShaderAcquireInfo PbrSceneGbufferShaderAcquireInfo;
        PbrSceneGbufferShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eVertex, PbrSceneVertShaderInfo);
        PbrSceneGbufferShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eFragment, PbrSceneGbufferShaderInfo);
        auto* PbrSceneGbufferShaderRes = ShaderManager->AcquireShaderResource(PbrSceneGbufferShaderAcquireInfo);

        FShaderAcquireInfo LampShaderAcquireInfo;
        LampShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eVertex, PbrSceneVertShaderInfo);
        LampShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eFragment, LampShaderInfo);
        auto* LampShaderRes = ShaderManager->AcquireShaderResource(LampShaderAcquireInfo);

        FShaderAcquireInfo::FShaderInfo SkyboxVertShaderInfo
        {
            .Name = "SkyboxVertShader",
            .NextStage = vk::ShaderStageFlagBits::eFragment
        };

        FShaderAcquireInfo::FShaderInfo SkyboxFragShaderInfo
        {
            .Name = "SkyboxFragShader"
        };

        FShaderAcquireInfo SkyboxShaderAcquireInfo;
        SkyboxShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eVertex, SkyboxVertShaderInfo);
        SkyboxShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eFragment, SkyboxFragShaderInfo);
        auto* SkyboxShaderRes = ShaderManager->AcquireShaderResource(SkyboxShaderAcquireInfo);

        FShaderAcquireInfo::FShaderInfo PbrSceneMergeShaderInfo
        {
            .Name = "PbrSceneMergeShader"
        };

        FShaderAcquireInfo PbrSceneMergeShaderAcquireInfo;
        PbrSceneMergeShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eCompute, PbrSceneMergeShaderInfo);
        auto* PbrSceneMergeShaderRes = ShaderManager->AcquireShaderResource(PbrSceneMergeShaderAcquireInfo);

        FShaderAcquireInfo::FShaderInfo PostVertShaderInfo
        {
            .Name = "PostVertShader",
            .NextStage = vk::ShaderStageFlagBits::eFragment
        };

        FShaderAcquireInfo::FShaderInfo PostFragShaderInfo
        {
            .Name = "PostFragShader"
        };

        FShaderAcquireInfo PostShaderAcquireInfo;
        PostShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eVertex, PostVertShaderInfo);
        PostShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eFragment, PostFragShaderInfo);
        auto* PostShaderRes = ShaderManager->AcquireShaderResource(PostShaderAcquireInfo);

        FShaderAcquireInfo::FShaderInfo DepthMapShaderInfo
        {
            .Name = "DepthMapShader"
        };

        FShaderAcquireInfo DepthMapShaderAcquireInfo;
        DepthMapShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eVertex, DepthMapShaderInfo);
        auto* DepthMapShaderRes = ShaderManager->AcquireShaderResource(DepthMapShaderAcquireInfo);
        
        //FShaderAcquireInfo DepthMapShaderAcquireInfo;
        //{
        //    FShaderAcquireInfo::FShaderInfo VertexShaderInfo;
        //    VertexShaderInfo.Shader = AssetManager->AcquireAsset<FShader>("DepthMapShader");
        //    DepthMapShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eVertex, VertexShaderInfo);
        //}

        //auto* DepthMapShader = ShaderManager->AcquireShaderResource(DepthMapShaderAcquireInfo);

        //auto* PipelineManager     = EngineResourceServices->GetPipelineManager();

        //auto PbrSceneGBufferShader = AssetManager->AcquireAsset<FShader>("PbrSceneGBufferShader");
        //auto PbrSceneMergeShader   = AssetManager->AcquireAsset<FShader>("PbrSceneMergeShader");
        //auto PbrSceneShader        = AssetManager->AcquireAsset<FShader>("PbrSceneShader");
        //auto LampShader            = AssetManager->AcquireAsset<FShader>("LampShader");
        //auto DepthMapShader        = AssetManager->AcquireAsset<FShader>("DepthMapShader");
        //// auto TerrainShader         = AssetManager->AcquireAsset<FShader>("TerrainShader");
        //auto SkyboxShader          = AssetManager->AcquireAsset<FShader>("SkyboxShader");
        //auto PostShader            = AssetManager->AcquireAsset<FShader>("PostShader");

        //vk::Pipeline PbrSceneGBufferPipeline;
        //vk::Pipeline PbrSceneMergePipeline;
        //vk::Pipeline PbrScenePipeline;
        //vk::Pipeline LampPipeline;
        //vk::Pipeline DepthMapPipeline;
        //vk::Pipeline TerrainPipeline;
        //vk::Pipeline PostPipeline;
        //vk::Pipeline SkyboxPipeline;

        //auto GetPipelines = [&]() -> void
        //{
        //    PbrSceneGBufferPipeline = PipelineManager->GetPipeline("PbrSceneGBufferPipeline");
        //    PbrSceneMergePipeline   = PipelineManager->GetPipeline("PbrSceneMergePipeline");
        //    PbrScenePipeline        = PipelineManager->GetPipeline("PbrScenePipeline");
        //    LampPipeline            = PipelineManager->GetPipeline("LampPipeline");
        //    DepthMapPipeline        = PipelineManager->GetPipeline("DepthMapPipeline");
        //    // TerrainPipeline         = PipelineManager->GetPipeline("TerrainPipeline");
        //    PostPipeline            = PipelineManager->GetPipeline("PostPipeline");
        //    SkyboxPipeline          = PipelineManager->GetPipeline("SkyboxPipeline");
        //};

        //GetPipelines();

        //VulkanContext_->RegisterRuntimeOnlyCallbacks(
        //    FVulkanContext::ECallbackType::kCreateSwapchain, "GetPipelines", GetPipelines);

        //auto PbrSceneGBufferPipelineLayout = PipelineManager->GetPipelineLayout("PbrSceneGBufferPipeline");
        //auto PbrSceneMergePipelineLayout   = PipelineManager->GetPipelineLayout("PbrSceneMergePipeline");
        //auto PbrScenePipelineLayout        = PipelineManager->GetPipelineLayout("PbrScenePipeline");
        //auto LampPipelineLayout            = PipelineManager->GetPipelineLayout("LampPipeline");
        //auto DepthMapPipelineLayout        = PipelineManager->GetPipelineLayout("DepthMapPipeline");
        //// auto TerrainPipelineLayout         = PipelineManager->GetPipelineLayout("TerrainPipeline");
        //auto PostPipelineLayout            = PipelineManager->GetPipelineLayout("PostPipeline");
        //auto SkyboxPipelineLayout          = PipelineManager->GetPipelineLayout("SkyboxPipeline");

        std::vector<FVulkanFence> InFlightFences;
        std::vector<FVulkanSemaphore> Semaphores_ImageAvailable;
        std::vector<FVulkanSemaphore> Semaphores_RenderFinished;
        std::vector<FVulkanCommandBuffer> CommandBuffers(Config::Graphics::kMaxFrameInFlight);
        std::vector<FVulkanCommandBuffer> DepthMapCommandBuffers(Config::Graphics::kMaxFrameInFlight);
        std::vector<FVulkanCommandBuffer> SceneGBufferCommandBuffers(Config::Graphics::kMaxFrameInFlight);
        std::vector<FVulkanCommandBuffer> SceneMergeCommandBuffers(Config::Graphics::kMaxFrameInFlight);
        std::vector<FVulkanCommandBuffer> FrontgroundCommandBuffers(Config::Graphics::kMaxFrameInFlight);
        std::vector<FVulkanCommandBuffer> PostProcessCommandBuffers(Config::Graphics::kMaxFrameInFlight);

        for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
        {
            std::string FenceName = "InFlightFence_Frame" + std::to_string(i);
            InFlightFences.emplace_back(VulkanContext_->GetDevice(), FenceName, vk::FenceCreateFlagBits::eSignaled);
        }

        std::uint32_t SwapchainImageCount = VulkanContext_->GetSwapchainImageCount();
        for (std::uint32_t i = 0; i != SwapchainImageCount; ++i)
        {
            std::string SemaphoreImageAvailableName = "ImageAvailableSemaphore_Image" + std::to_string(i);
            std::string SemaphoreRenderFinishedName = "RenderFinishedSemaphore_Image" + std::to_string(i);
            Semaphores_ImageAvailable.emplace_back(VulkanContext_->GetDevice(), SemaphoreImageAvailableName, vk::SemaphoreCreateFlags());
            Semaphores_RenderFinished.emplace_back(VulkanContext_->GetDevice(), SemaphoreRenderFinishedName, vk::SemaphoreCreateFlags());
        }

        FVulkanCommandPool CommandPool(
            VulkanContext_->GetDevice(), "MainCommandPool", VulkanContext_->GetQueueFamilyIndex(FVulkanContext::EQueueType::kGeneral),
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

        CommandPool.AllocateBuffers(vk::CommandBufferLevel::ePrimary, "CommandBuffer", CommandBuffers);
        CommandPool.AllocateBuffers(vk::CommandBufferLevel::eSecondary, "DepthMapCommandBuffer", DepthMapCommandBuffers);
        CommandPool.AllocateBuffers(vk::CommandBufferLevel::eSecondary, "SceneGBufferCommandBuffer", SceneGBufferCommandBuffers);
        CommandPool.AllocateBuffers(vk::CommandBufferLevel::eSecondary, "SceneMergeCommandBuffer", SceneMergeCommandBuffers);
        CommandPool.AllocateBuffers(vk::CommandBufferLevel::eSecondary, "FrontgroundCommandBuffer", FrontgroundCommandBuffers);
        CommandPool.AllocateBuffers(vk::CommandBufferLevel::eSecondary, "PostProcessCommandBuffer", PostProcessCommandBuffers);

        vk::DeviceSize Offset           = 0;
        std::uint32_t  DynamicOffset    = 0;
        std::uint32_t  CurrentFrame     = 0;
        std::uint32_t  CurrentSemaphore = 0;

        glm::vec3    LightPos(-2.0f, 4.0f, -1.0f);
        glm::mat4x4  LightProjection  = glm::infinitePerspective(glm::radians(60.0f), 1.0f, 1.0f);
        glm::mat4x4  LightView        = glm::lookAt(LightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4x4  LightSpaceMatrix = LightProjection * LightView;
        vk::Extent2D DepthMapExtent(8192, 8192);

        vk::ImageSubresourceRange ColorSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageSubresourceRange DepthSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1);

        std::array PlaneVertexBuffers{ *PlaneVertexBuffer_->GetBuffer(), *InstanceBuffer_->GetBuffer() };
        std::array CubeVertexBuffers{ *CubeVertexBuffer_->GetBuffer(), *InstanceBuffer_->GetBuffer() };
        std::array SphereVertexBuffers{ *SphereVertexBuffer_->GetBuffer(), *InstanceBuffer_->GetBuffer() };
        std::array PlaneOffsets{ Offset, Offset };
        std::array CubeOffsets{ Offset, Offset + sizeof(glm::mat4x4) };
        std::array SphereOffsets{ Offset, Offset + 4 * sizeof(glm::mat4x4) };
        std::array LampOffsets{ Offset, Offset + 5 * sizeof(glm::mat4x4) };

        // std::array<float, 4> TessArgs{};
        // TessArgs[0] = 50.0f;
        // TessArgs[1] = 1000.0f;
        // TessArgs[2] = 8.0f;
        // TessArgs[3] = 1.5f * VulkanContext_->GetPhysicalDevice().getProperties().limits.maxTessellationGenerationLevel;

        FMatrices    Matrices{};
        FMvpMatrices MvpMatrices{};
        FLightArgs   LightArgs{};

        auto CameraPosUpdater = ShaderBufferManager->GetFieldUpdaters<glm::aligned_vec3>("LightArgs", "CameraPos");

        LightArgs.LightPos   = LightPos;
        LightArgs.LightColor = glm::vec3(300.0f);

        ShaderBufferManager->UpdateDataBuffers("LightArgs", LightArgs);

        // Camera_->SetOrbitTarget(glm::vec3(0.0f));
        // Camera_->SetOrbitAxis(glm::vec3(0.0f, 1.0f, 0.0f));

        auto SetDefaultGraphicsState = [&](vk::CommandBuffer& CommandBuffer) -> void
        {
            // 这些状态在 PSO 时代是写在 CreateInfo 里的默认 False
            // 现在必须显式设置为 False
            CommandBuffer.setDepthClampEnableEXT(vk::False);
            CommandBuffer.setLogicOpEnableEXT(vk::False);
            CommandBuffer.setDepthBiasEnable(vk::False);

            // 如果你开启了 alphaToOne 特性
            CommandBuffer.setAlphaToOneEnableEXT(vk::False);

            // 栅格化丢弃通常也是关的
            CommandBuffer.setRasterizerDiscardEnable(vk::False);

            // 如果启用了 extendedDynamicState，Viewports/Scissors 也会报错
            // 见下文详解
        };

        // Record secondary commands
        // -------------------------
        auto RecordSecondaryCommands = [&]() -> void
        {
            vk::Extent2D SupersamplingExtent(WindowSize_.width * 2, WindowSize_.height * 2);

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
                .setColorAttachmentFormats(VulkanContext_->GetSwapchainCreateInfo().imageFormat);

            for (std::uint32_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
            {
                auto ResourceDescriptorBufferBindingInfo = ShaderBufferManager->GetResourceHeapBindingInfo(i);
                auto SamplerDescriptorBufferBindingInfo  = ShaderBufferManager->GetSamplerHeapBindingInfo(i);

                std::array DescriptorBufferBindingInfos{ ResourceDescriptorBufferBindingInfo, SamplerDescriptorBufferBindingInfo };;

                auto& DepthMapCommandBuffer = DepthMapCommandBuffers[i];

                vk::CommandBufferInheritanceInfo DepthMapInheritanceInfo = vk::CommandBufferInheritanceInfo()
                    .setPNext(&DepthMapInheritanceRenderingInfo);

                const auto& DepthMapPipelineLayout = *DepthMapShaderRes->PipelineLayout;

                DepthMapCommandBuffer.Begin(DepthMapInheritanceInfo, vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse);
                SetDefaultGraphicsState(*DepthMapCommandBuffer);
                // 1. Bind Shaders
                DepthMapCommandBuffer->bindShadersEXT(DepthMapShaderRes->Stages, DepthMapShaderRes->Handles);

                // 2. Set Dynamic States (对应原来的 PSO)
                // Viewport & Scissor
                DepthMapCommandBuffer->setViewportWithCount(DepthMapViewport);
                DepthMapCommandBuffer->setScissorWithCount(DepthMapScissor);

                // Vertex Input (新!)
                DepthMapCommandBuffer->setVertexInputEXT(DepthMapShaderRes->VertexInputBindings, DepthMapShaderRes->VertexInputAttributes);

                // Input Assembly
                DepthMapCommandBuffer->setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
                DepthMapCommandBuffer->setPrimitiveRestartEnable(vk::False);

                // Rasterization
                DepthMapCommandBuffer->setRasterizerDiscardEnable(vk::False);
                DepthMapCommandBuffer->setCullMode(vk::CullModeFlagBits::eBack);
                DepthMapCommandBuffer->setFrontFace(vk::FrontFace::eCounterClockwise);
                DepthMapCommandBuffer->setPolygonModeEXT(vk::PolygonMode::eFill);
                DepthMapCommandBuffer->setLineWidth(1.0f);
                DepthMapCommandBuffer->setDepthBiasEnable(vk::False);

                // Depth Stencil
                DepthMapCommandBuffer->setDepthTestEnable(vk::True);
                DepthMapCommandBuffer->setDepthWriteEnable(vk::True);
                DepthMapCommandBuffer->setDepthCompareOp(vk::CompareOp::eLess); // 注意：DepthMap 通常用 Less
                DepthMapCommandBuffer->setDepthBoundsTestEnable(vk::False);
                DepthMapCommandBuffer->setStencilTestEnable(vk::False);

                // Multisample (Shadow Map 通常不用 MSAA)
                DepthMapCommandBuffer->setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);
                DepthMapCommandBuffer->setSampleMaskEXT(vk::SampleCountFlagBits::e1, { 0xFFFFFFFF });
                DepthMapCommandBuffer->setAlphaToCoverageEnableEXT(vk::False);

                // Color Blend (无颜色附件，也要设置 Enable 为 False 以防万一？其实不需要，因为没附件)
                // DepthMapCommandBuffer->setColorBlendEnableEXT(0, {}); // 空数组

                // 3. Resources & Draw
                vk::DeviceSize MatricesAddress = ShaderBufferManager->GetDataBufferDeviceAddress(i, "Matrices");
                DepthMapCommandBuffer->pushConstants<vk::DeviceAddress>(DepthMapPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, MatricesAddress);

                DepthMapCommandBuffer->bindVertexBuffers(0, PlaneVertexBuffers, PlaneOffsets);
                DepthMapCommandBuffer->draw(6, 1, 0, 0);

                DepthMapCommandBuffer->bindVertexBuffers(0, CubeVertexBuffers, CubeOffsets);
                DepthMapCommandBuffer->draw(36, 3, 0, 0);

                DepthMapCommandBuffer.End();

                // ------------------------------------------------------------

                auto& SceneGBufferCommandBuffer = SceneGBufferCommandBuffers[i];

                vk::CommandBufferInheritanceInfo GBufferSceneInheritanceInfo = vk::CommandBufferInheritanceInfo()
                    .setPNext(&GBufferSceneInheritanceRenderingInfo);

                const auto& PbrSceneGBufferPipelineLayout = *PbrSceneGbufferShaderRes->PipelineLayout;

                SceneGBufferCommandBuffer.Begin(GBufferSceneInheritanceInfo, vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse);
                SetDefaultGraphicsState(*SceneGBufferCommandBuffer);
                // 1. Bind Shaders
                SceneGBufferCommandBuffer->bindShadersEXT(PbrSceneGbufferShaderRes->Stages, PbrSceneGbufferShaderRes->Handles);

                // 2. Set Dynamic States
                SceneGBufferCommandBuffer->setViewportWithCount(CommonViewport);
                SceneGBufferCommandBuffer->setScissorWithCount(CommonScissor);

                // Vertex Input
                SceneGBufferCommandBuffer->setVertexInputEXT(PbrSceneGbufferShaderRes->VertexInputBindings, PbrSceneGbufferShaderRes->VertexInputAttributes);

                // Input Assembly
                SceneGBufferCommandBuffer->setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
                SceneGBufferCommandBuffer->setPrimitiveRestartEnable(vk::False);

                // Rasterization
                SceneGBufferCommandBuffer->setRasterizerDiscardEnable(vk::False);
                SceneGBufferCommandBuffer->setCullMode(vk::CullModeFlagBits::eBack);
                SceneGBufferCommandBuffer->setFrontFace(vk::FrontFace::eCounterClockwise);
                SceneGBufferCommandBuffer->setPolygonModeEXT(vk::PolygonMode::eFill);
                SceneGBufferCommandBuffer->setLineWidth(1.0f);
                SceneGBufferCommandBuffer->setDepthBiasEnable(vk::False);

                // Depth Stencil
                SceneGBufferCommandBuffer->setDepthTestEnable(vk::True);
                SceneGBufferCommandBuffer->setDepthWriteEnable(vk::True);
                SceneGBufferCommandBuffer->setDepthCompareOp(vk::CompareOp::eLess);
                SceneGBufferCommandBuffer->setDepthBoundsTestEnable(vk::False);
                SceneGBufferCommandBuffer->setStencilTestEnable(vk::False);

                // Multisample
                SceneGBufferCommandBuffer->setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);
                SceneGBufferCommandBuffer->setSampleMaskEXT(vk::SampleCountFlagBits::e1, { 0xFFFFFFFF });
                SceneGBufferCommandBuffer->setAlphaToCoverageEnableEXT(vk::False);

                // Color Blend (4 Attachments)
                // 初始化 4 个默认的混合方程 (No Blend)
                vk::ColorBlendEquationEXT DefaultEquation; // 默认即为 Add, One, Zero...
                std::vector<vk::ColorBlendEquationEXT> Equations(4, DefaultEquation);
                SceneGBufferCommandBuffer->setColorBlendEquationEXT(0, Equations);

                // 初始化 4 个 WriteMask (RGBA)
                vk::ColorComponentFlags MaskRGBA = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
                std::vector<vk::ColorComponentFlags> Masks(4, MaskRGBA);
                SceneGBufferCommandBuffer->setColorWriteMaskEXT(0, Masks);

                // 初始化 4 个 Enable (False)
                std::vector<vk::Bool32> Enables(4, vk::False);
                SceneGBufferCommandBuffer->setColorBlendEnableEXT(0, Enables);

                // Logic Op
                SceneGBufferCommandBuffer->setLogicOpEnableEXT(vk::False);

                // 3. Resources & Draw
                SceneGBufferCommandBuffer->bindDescriptorBuffersEXT(DescriptorBufferBindingInfos);
                auto Offsets = ShaderBufferManager->GetDescriptorBindingOffsets("SceneGBufferDescriptorBuffer", 0, 1, 2);
                SceneGBufferCommandBuffer->setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, PbrSceneGBufferPipelineLayout, 0, { 1, 0, 0 }, Offsets);

                SceneGBufferCommandBuffer->bindVertexBuffers(0, PlaneVertexBuffers, PlaneOffsets);
                SceneGBufferCommandBuffer->draw(6, 1, 0, 0);

                SceneGBufferCommandBuffer->bindVertexBuffers(0, CubeVertexBuffers, CubeOffsets);
                SceneGBufferCommandBuffer->draw(36, 3, 0, 0);

                SceneGBufferCommandBuffer.End();

                // ------------------------------------------------------------

                auto& SceneMergeCommandBuffer = SceneMergeCommandBuffers[i];

                vk::CommandBufferInheritanceInfo GBufferMergeInheritanceInfo = vk::CommandBufferInheritanceInfo()
                    .setPNext(&GBufferMergeInheritanceRenderingInfo);

                std::uint32_t WorkgroupSizeX = (SupersamplingExtent.width  + 15) / 16;
                std::uint32_t WorkgroupSizeY = (SupersamplingExtent.height + 15) / 16;

                const auto& PbrSceneMergePipelineLayout = *PbrSceneMergeShaderRes->PipelineLayout;

                SceneMergeCommandBuffer.Begin(GBufferMergeInheritanceInfo, vk::CommandBufferUsageFlagBits::eSimultaneousUse);
                // SetDefaultGraphicsState(*SceneMergeCommandBuffer);
                // 1. Bind Shaders
                SceneMergeCommandBuffer->bindShadersEXT(PbrSceneMergeShaderRes->Stages, PbrSceneMergeShaderRes->Handles);

                // 2. Resources & Dispatch
                vk::DeviceAddress LightArgsAddress = ShaderBufferManager->GetDataBufferDeviceAddress(i, "LightArgs");
                SceneMergeCommandBuffer->pushConstants<vk::DeviceAddress>(PbrSceneMergePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, LightArgsAddress);

                SceneMergeCommandBuffer->bindDescriptorBuffersEXT(DescriptorBufferBindingInfos);
                Offsets = ShaderBufferManager->GetDescriptorBindingOffsets("SceneMergeDescriptorBuffer", 1, 2);
                SceneMergeCommandBuffer->setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eCompute, PbrSceneMergePipelineLayout, 1, { 0, 0 }, Offsets);

                SceneMergeCommandBuffer->dispatch(WorkgroupSizeX, WorkgroupSizeY, 1);

                SceneMergeCommandBuffer.End();

                // ------------------------------------------------------------

                auto& FrontgroundCommandBuffer = FrontgroundCommandBuffers[i];

                vk::CommandBufferInheritanceInfo FrontgroundInheritanceInfo = vk::CommandBufferInheritanceInfo()
                    .setPNext(&FrontgroundInheritanceRenderingInfo);

                const auto& LampPipelineLayout   = *LampShaderRes->PipelineLayout;
                const auto& SkyboxPipelineLayout = *SkyboxShaderRes->PipelineLayout;

                FrontgroundCommandBuffer.Begin(FrontgroundInheritanceInfo, vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse);
                SetDefaultGraphicsState(*FrontgroundCommandBuffer);
                // --- Global Dynamic States (Shared) ---
                FrontgroundCommandBuffer->setViewportWithCount(CommonViewport);
                FrontgroundCommandBuffer->setScissorWithCount(CommonScissor);
                FrontgroundCommandBuffer->setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
                FrontgroundCommandBuffer->setPrimitiveRestartEnable(vk::False);
                FrontgroundCommandBuffer->setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);
                FrontgroundCommandBuffer->setSampleMaskEXT(vk::SampleCountFlagBits::e1, { 0xFFFFFFFF });
                FrontgroundCommandBuffer->setAlphaToCoverageEnableEXT(vk::False);

                // Color Blend (1 Attachment)
                std::vector<vk::ColorBlendEquationEXT> OneEq(1, DefaultEquation);
                FrontgroundCommandBuffer->setColorBlendEquationEXT(0, OneEq);
                std::vector<vk::ColorComponentFlags> OneMask(1, MaskRGBA);
                FrontgroundCommandBuffer->setColorWriteMaskEXT(0, OneMask);
                std::vector<vk::Bool32> OneEnable(1, vk::False);
                FrontgroundCommandBuffer->setColorBlendEnableEXT(0, OneEnable);
                FrontgroundCommandBuffer->setLogicOpEnableEXT(vk::False);

                // --- Draw 1: Lamp ---
                {
                    // 1. Bind Shaders
                    FrontgroundCommandBuffer->bindShadersEXT(LampShaderRes->Stages, LampShaderRes->Handles);

                    // 2. Specific States (Lamp)
                    FrontgroundCommandBuffer->setVertexInputEXT(LampShaderRes->VertexInputBindings, LampShaderRes->VertexInputAttributes);

                    FrontgroundCommandBuffer->setRasterizerDiscardEnable(vk::False);
                    FrontgroundCommandBuffer->setCullMode(vk::CullModeFlagBits::eBack);
                    FrontgroundCommandBuffer->setFrontFace(vk::FrontFace::eCounterClockwise);
                    FrontgroundCommandBuffer->setPolygonModeEXT(vk::PolygonMode::eFill);
                    FrontgroundCommandBuffer->setLineWidth(1.0f);

                    FrontgroundCommandBuffer->setDepthTestEnable(vk::True);
                    FrontgroundCommandBuffer->setDepthWriteEnable(vk::True);
                    FrontgroundCommandBuffer->setDepthCompareOp(vk::CompareOp::eLess);
                    FrontgroundCommandBuffer->setDepthBoundsTestEnable(vk::False);
                    FrontgroundCommandBuffer->setStencilTestEnable(vk::False);

                    // 3. Resources
                    FrontgroundCommandBuffer->pushConstants<vk::DeviceAddress>(LampPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, MatricesAddress);
                    FrontgroundCommandBuffer->pushConstants<vk::DeviceAddress>(LampPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, LampShaderRes->GetPushConstantOffset("LightArgsAddress"), LightArgsAddress);

                    FrontgroundCommandBuffer->bindVertexBuffers(0, CubeVertexBuffers, LampOffsets);
                    FrontgroundCommandBuffer->draw(36, 1, 0, 0);
                }

                // --- Draw 2: Skybox ---
                {
                    // 1. Bind Shaders
                    FrontgroundCommandBuffer->bindShadersEXT(SkyboxShaderRes->Stages, SkyboxShaderRes->Handles);

                    // 2. Specific States (Skybox)
                    // Vertex Input 变了
                    FrontgroundCommandBuffer->setVertexInputEXT(SkyboxShaderRes->VertexInputBindings, SkyboxShaderRes->VertexInputAttributes);

                    // Rasterization (Cull Mode 可能不同)
                    // 如果 Skybox 配置里 Rasterization 是默认的，那么 CullMode 也是 Back
                    // 如果你之前没有 Reset Rasterization，它会继承 Lamp 的设置 (Back)
                    // 你的 Skybox 配置: SkyboxPipelineCreateInfoPack.RasterizationStateCreateInfo = vk::PipelineRasterizationStateCreateInfo(); (默认 Cull=None?? 需确认默认值)
                    // vk::PipelineRasterizationStateCreateInfo 默认构造函数的 cullMode 是 eNone (0)。
                    FrontgroundCommandBuffer->setCullMode(vk::CullModeFlagBits::eNone); // 假设 Skybox 不剔除

                    // Depth Stencil (Op 变了)
                    // Skybox 配置: DepthCompareOp = LessOrEqual
                    FrontgroundCommandBuffer->setDepthCompareOp(vk::CompareOp::eLessOrEqual);

                    // 3. Resources
                    FrontgroundCommandBuffer->pushConstants<vk::DeviceAddress>(SkyboxPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, MatricesAddress);

                    FrontgroundCommandBuffer->bindDescriptorBuffersEXT(DescriptorBufferBindingInfos);
                    vk::DeviceSize OffsetSet0 = ShaderBufferManager->GetDescriptorBindingOffset("SkyboxDescriptorBuffer", 0);
                    FrontgroundCommandBuffer->setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, SkyboxPipelineLayout, 0, { 0 }, OffsetSet0);

                    FrontgroundCommandBuffer->bindVertexBuffers(0, *SkyboxVertexBuffer_->GetBuffer(), Offset);
                    FrontgroundCommandBuffer->draw(36, 1, 0, 0);
                }

                FrontgroundCommandBuffer.End();

                // ------------------------------------------------------------

                auto& PostProcessCommandBuffer = PostProcessCommandBuffers[i];

                vk::CommandBufferInheritanceInfo PostInheritanceInfo = vk::CommandBufferInheritanceInfo()
                    .setPNext(&PostInheritanceRenderingInfo);

                const auto& PostPipelineLayout = *PostShaderRes->PipelineLayout;

                PostProcessCommandBuffer.Begin(PostInheritanceInfo, vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse);
                SetDefaultGraphicsState(*PostProcessCommandBuffer);
                // 1. Bind Shaders
                PostProcessCommandBuffer->bindShadersEXT(PostShaderRes->Stages, PostShaderRes->Handles);

                // 2. Set Dynamic States
                PostProcessCommandBuffer->setViewportWithCount(CommonViewport);
                PostProcessCommandBuffer->setScissorWithCount(CommonScissor);

                // Vertex Input
                PostProcessCommandBuffer->setVertexInputEXT(PostShaderRes->VertexInputBindings, PostShaderRes->VertexInputAttributes);

                // Input Assembly
                PostProcessCommandBuffer->setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
                PostProcessCommandBuffer->setPrimitiveRestartEnable(vk::False);

                // Rasterization (全默认 -> Cull None)
                PostProcessCommandBuffer->setRasterizerDiscardEnable(vk::False);
                PostProcessCommandBuffer->setCullMode(vk::CullModeFlagBits::eNone);
                PostProcessCommandBuffer->setFrontFace(vk::FrontFace::eCounterClockwise);
                PostProcessCommandBuffer->setPolygonModeEXT(vk::PolygonMode::eFill);
                PostProcessCommandBuffer->setLineWidth(1.0f);
                PostProcessCommandBuffer->setDepthBiasEnable(vk::False);

                // Depth Stencil (全默认 -> Disable)
                PostProcessCommandBuffer->setDepthTestEnable(vk::False);
                PostProcessCommandBuffer->setDepthWriteEnable(vk::False);
                PostProcessCommandBuffer->setDepthBoundsTestEnable(vk::False);
                PostProcessCommandBuffer->setStencilTestEnable(vk::False);

                // Multisample (默认 -> 1)
                PostProcessCommandBuffer->setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);
                PostProcessCommandBuffer->setSampleMaskEXT(vk::SampleCountFlagBits::e1, { 0xFFFFFFFF });
                PostProcessCommandBuffer->setAlphaToCoverageEnableEXT(vk::False);

                // Color Blend (1 Attachment)
                PostProcessCommandBuffer->setColorBlendEquationEXT(0, OneEq);
                PostProcessCommandBuffer->setColorWriteMaskEXT(0, OneMask);
                PostProcessCommandBuffer->setColorBlendEnableEXT(0, OneEnable); // 默认也是关

                // 3. Resources & Draw
                PostProcessCommandBuffer->bindDescriptorBuffersEXT(DescriptorBufferBindingInfos);
                PostProcessCommandBuffer->pushConstants<vk::Bool32>(PostPipelineLayout, vk::ShaderStageFlagBits::eFragment, PostShaderRes->GetPushConstantOffset("bEnableHdr"), static_cast<vk::Bool32>(bEnableHdr_));

                vk::DeviceSize OffsetSet0 = ShaderBufferManager->GetDescriptorBindingOffset("PostDescriptorBuffer", 0);
                PostProcessCommandBuffer->setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, PostPipelineLayout, 0, { 0 }, OffsetSet0);

                PostProcessCommandBuffer->bindVertexBuffers(0, *QuadVertexBuffer_->GetBuffer(), Offset);
                PostProcessCommandBuffer->draw(6, 1, 0, 0);

                PostProcessCommandBuffer.End();
            }
        };

        RecordSecondaryCommands();

        VulkanContext_->RegisterRuntimeOnlyCallbacks(
            FVulkanContext::ECallbackType::kCreateSwapchain, "RecordSecondaryCommands", RecordSecondaryCommands);

        auto PrepareResolveAttachment = [&]() -> void
        {
            vk::ImageMemoryBarrier2 PrepareBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *ResolveAttachment_->GetImage(),
                ColorSubresourceRange
            );

            vk::DependencyInfo PrepareDependencyInfo = vk::DependencyInfo()
                .setImageMemoryBarriers(PrepareBarrier);

            auto& PrepareCommandBuffer = CommandBuffers.front();
            PrepareCommandBuffer.Begin();
            PrepareCommandBuffer->pipelineBarrier2(PrepareDependencyInfo);
            PrepareCommandBuffer.End();
            VulkanContext_->ExecuteCommands(FVulkanContext::EQueueType::kGeneral, PrepareCommandBuffer);
        };

        PrepareResolveAttachment();

        VulkanContext_->RegisterRuntimeOnlyCallbacks(
            FVulkanContext::ECallbackType::kCreateSwapchain, "PrepareResolveAttachment", PrepareResolveAttachment);

        // Main rendering loop
        while (!glfwWindowShouldClose(Window_))
        {
            while (glfwGetWindowAttrib(Window_, GLFW_ICONIFIED))
            {
                glfwWaitEvents();
            }

            vk::Extent2D SupersamplingExtent(WindowSize_.width * 2, WindowSize_.height * 2);

            vk::Viewport CommonViewport(0.0f, 0.0f, static_cast<float>(SupersamplingExtent.width),
                                        static_cast<float>(SupersamplingExtent.height), 0.0f, 1.0f);

            vk::Viewport DepthMapViewport(0.0f, 0.0f, static_cast<float>(DepthMapExtent.width),
                                          static_cast<float>(DepthMapExtent.height), 0.0f, 1.0f);

            vk::Rect2D CommonScissor(vk::Offset2D(), SupersamplingExtent);
            vk::Rect2D DepthMapScissor(vk::Offset2D(), DepthMapExtent);

            InFlightFences[CurrentFrame].WaitAndReset();

            // Uniform update
            // --------------
            float WindowAspect = static_cast<float>(WindowSize_.width) / static_cast<float>(WindowSize_.height);

            Matrices.View             = Camera_->GetViewMatrix();
            Matrices.Projection       = Camera_->GetProjectionMatrix(WindowAspect, 0.001f);
            Matrices.LightSpaceMatrix = LightSpaceMatrix;

            // MvpMatrices.Model      = glm::mat4x4(1.0f);
            // MvpMatrices.View       = Camera_->GetViewMatrix();
            // MvpMatrices.Projection = Camera_->GetProjectionMatrix(WindowAspect, 0.001f);

            ShaderBufferManager->UpdateDataBuffer(CurrentFrame, "Matrices", Matrices);
            // ShaderBufferManager->UpdateDataBuffer(CurrentFrame, "MvpMatrices", MvpMatrices);

            CameraPosUpdater[CurrentFrame] << Camera_->GetVector(FCamera::EVector::kPosition);

            VulkanContext_->SwapImage(*Semaphores_ImageAvailable[CurrentSemaphore]);
            std::uint32_t ImageIndex = VulkanContext_->GetCurrentImageIndex();

            // Record commands
            // ---------------
            auto& CurrentBuffer = CommandBuffers[CurrentFrame];
            CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

            vk::ImageMemoryBarrier2 InitSwapchainBarrier(
                vk::PipelineStageFlagBits2::eBlit,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eBlit,
                vk::AccessFlagBits2::eTransferWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                VulkanContext_->GetSwapchainImage(ImageIndex),
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
                *PositionAoAttachment_->GetImage(),
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
                *NormalRoughAttachment_->GetImage(),
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
                *AlbedoMetalAttachment_->GetImage(),
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
                *ShadowAttachment_->GetImage(),
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
                *ColorAttachment_->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 InitResolveAttachmentBarrier(
                vk::PipelineStageFlagBits2::eBlit,
                vk::AccessFlagBits2::eTransferRead,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *ResolveAttachment_->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 InitDepthMapAttachmentBarrier(
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eEarlyFragmentTests,
                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *DepthMapAttachment_->GetImage(),
                DepthSubresourceRange
            );

            std::array InitBarriers
            {
                InitSwapchainBarrier,
                InitPositionAoBarrier,
                InitNormalRoughBarrier,
                InitShadowBarrier,
                InitAlbedoMetalBarrier,
                InitColorAttachmentBarrier,
                InitResolveAttachmentBarrier,
                InitDepthMapAttachmentBarrier
            };

            vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(InitBarriers);

            CurrentBuffer->pipelineBarrier2(InitialDependencyInfo);

            vk::RenderingInfo DepthMapRenderingInfo = vk::RenderingInfo()
                .setFlags(vk::RenderingFlagBits::eContentsSecondaryCommandBuffers)
                .setRenderArea(DepthMapScissor)
                .setLayerCount(1)
                .setPDepthAttachment(&DepthMapAttachmentInfo_);

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
                *DepthMapAttachment_->GetImage(),
                DepthSubresourceRange
            );

            vk::DependencyInfo DepthMapRenderEndDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(DepthMapRenderEndBarrier);

            CurrentBuffer->pipelineBarrier2(DepthMapRenderEndDependencyInfo);

            std::array GBufferAttachments
            {
                PositionAoAttachmentInfo_,
                NormalRoughAttachmentInfo_,
                AlbedoMetalAttachmentInfo_,
                ShadowAttachmentInfo_
            };

            vk::RenderingInfo SceneRenderingInfo = vk::RenderingInfo()
                .setFlags(vk::RenderingFlagBits::eContentsSecondaryCommandBuffers)
                .setRenderArea(CommonScissor)
                .setLayerCount(1)
                .setColorAttachments(GBufferAttachments)
                .setPDepthAttachment(&DepthStencilAttachmentInfo_);

            CurrentBuffer->beginRendering(SceneRenderingInfo);
            CurrentBuffer->executeCommands(*SceneGBufferCommandBuffers[CurrentFrame]);
            CurrentBuffer->endRendering();

            // CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, TerrainPipeline);
            // CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, TerrainPipelineLayout, 0,
            //                                   TerrainShader->GetDescriptorSets(CurrentFrame), {});
            // CurrentBuffer->pushConstants(TerrainPipelineLayout, vk::ShaderStageFlagBits::eTessellationControl, 0,
            //                              static_cast<std::uint32_t>(TessArgs.size()) * sizeof(float), TessArgs.data());
            // CurrentBuffer->bindVertexBuffers(0, *TerrainVertexBuffer_->GetBuffer(), Offset);
            // CurrentBuffer->draw(TessResolution_* TessResolution_ * 4, 1, 0, 0);

            // CurrentBuffer->endRendering();

            vk::ImageMemoryBarrier2 PositionAoRenderEndBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *PositionAoAttachment_->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 NormalRoughRenderEndBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *NormalRoughAttachment_->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 AlbedoMetalRenderEndBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *AlbedoMetalAttachment_->GetImage(),
                ColorSubresourceRange
            );

            vk::ImageMemoryBarrier2 ShadowRenderEndBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *ShadowAttachment_->GetImage(),
                ColorSubresourceRange
            );

            std::array GBufferRenderEndBarriers
            {
                PositionAoRenderEndBarrier,
                ShadowRenderEndBarrier,
                NormalRoughRenderEndBarrier,
                AlbedoMetalRenderEndBarrier
            };

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
                vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
                vk::ImageLayout::eGeneral,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *ColorAttachment_->GetImage(),
                ColorSubresourceRange
            );

            vk::DependencyInfo MergeEndDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(MergeEndBarrier);

            CurrentBuffer->pipelineBarrier2(MergeEndDependencyInfo);

            // Draw other objects
            SceneRenderingInfo.setColorAttachmentCount(1).setColorAttachments(ColorAttachmentInfo_);
            ColorAttachmentInfo_.setLoadOp(vk::AttachmentLoadOp::eLoad);
            DepthStencilAttachmentInfo_.setLoadOp(vk::AttachmentLoadOp::eLoad);

            CurrentBuffer->beginRendering(SceneRenderingInfo);
            CurrentBuffer->executeCommands(*FrontgroundCommandBuffers[CurrentFrame]);
            CurrentBuffer->endRendering();

            ColorAttachmentInfo_.setLoadOp(vk::AttachmentLoadOp::eClear);
            DepthStencilAttachmentInfo_.setLoadOp(vk::AttachmentLoadOp::eClear);

            vk::ImageMemoryBarrier2 PrePostBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eShaderRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *ColorAttachment_->GetImage(),
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
                .setColorAttachments(PostProcessAttachmentInfo_);

            CurrentBuffer->beginRendering(PostRenderingInfo);
            CurrentBuffer->executeCommands(*PostProcessCommandBuffers[CurrentFrame]);
            CurrentBuffer->endRendering();

            vk::ImageMemoryBarrier2 PreBlitBarrier(
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eBlit,
                vk::AccessFlagBits2::eTransferRead,
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                *ResolveAttachment_->GetImage(),
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
                { vk::Offset3D(0, 0, 0), vk::Offset3D(WindowSize_.width, WindowSize_.height, 1) }
            );

            CurrentBuffer->blitImage(*ResolveAttachment_->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                                     VulkanContext_->GetSwapchainImage(ImageIndex), vk::ImageLayout::eTransferDstOptimal,
                                     BlitRegion, vk::Filter::eLinear);

            vk::ImageMemoryBarrier2 PresentBarrier(
                vk::PipelineStageFlagBits2::eBlit,
                vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eBottomOfPipe,
                vk::AccessFlagBits2::eNone,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::ePresentSrcKHR,
                vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored,
                VulkanContext_->GetSwapchainImage(ImageIndex),
                ColorSubresourceRange
            );

            vk::DependencyInfo FinalDependencyInfo = vk::DependencyInfo()
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                .setImageMemoryBarriers(PresentBarrier);

            CurrentBuffer->pipelineBarrier2(FinalDependencyInfo);
            CurrentBuffer.End();

            VulkanContext_->SubmitCommandBuffer(FVulkanContext::EQueueType::kGeneral,
                                                *CurrentBuffer,
                                                *Semaphores_ImageAvailable[CurrentSemaphore],
                                                vk::PipelineStageFlagBits2::eBlit,
                                                *Semaphores_RenderFinished[ImageIndex],
                                                vk::PipelineStageFlagBits2::eBottomOfPipe,
                                                *InFlightFences[CurrentFrame], true);
            
            VulkanContext_->PresentImage(*Semaphores_RenderFinished[ImageIndex]);

            CurrentFrame = (CurrentFrame + 1) % Config::Graphics::kMaxFrameInFlight;
            CurrentSemaphore = (CurrentSemaphore + 1) % SwapchainImageCount;

            Camera_->ProcessEvent(DeltaTime_);

            ProcessInput();
            glfwPollEvents();
            ShowTitleFps();
        }

        VulkanContext_->WaitIdle();
        CommandPool.FreeBuffers(CommandBuffers);
    }

    void FApplication::CreateAttachments()
    {
        PositionAoAttachmentInfo_
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

        NormalRoughAttachmentInfo_
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

        AlbedoMetalAttachmentInfo_
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

        ShadowAttachmentInfo_
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

        ColorAttachmentInfo_
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            // .setResolveMode(vk::ResolveModeFlagBits::eAverage)
            // .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

        DepthStencilAttachmentInfo_
            .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

        DepthMapAttachmentInfo_
            .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

        PostProcessAttachmentInfo_
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

            vk::Extent2D AttachmentExtent(WindowSize_.width * 2, WindowSize_.height * 2);
            vk::Extent2D DepthMapExtent(8192, 8192);

            VulkanContext_->WaitIdle();
            auto Allocator = VulkanContext_->GetVmaAllocator();

            PositionAoAttachment_ = std::make_unique<FColorAttachment>(
                VulkanContext_, "PositionAoAttachment", Allocator, AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat,
                AttachmentExtent, 1, vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

            NormalRoughAttachment_ = std::make_unique<FColorAttachment>(
                VulkanContext_, "NormalRoughAttachment", Allocator, AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat,
                AttachmentExtent, 1, vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

            AlbedoMetalAttachment_ = std::make_unique<FColorAttachment>(
                VulkanContext_, "AlbedoMetalAttachment", Allocator, AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat,
                AttachmentExtent, 1, vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

            ShadowAttachment_ = std::make_unique<FColorAttachment>(
                VulkanContext_, "ShadowAttachment", Allocator, AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat,
                AttachmentExtent, 1, vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

            ColorAttachment_ = std::make_unique<FColorAttachment>(
                VulkanContext_, "MainColorAttachment", Allocator, AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat,
                AttachmentExtent, 1, vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);

            ResolveAttachment_ = std::make_unique<FColorAttachment>(
                VulkanContext_, "MainResolveAttachment", Allocator, AllocationCreateInfo, VulkanContext_->GetSwapchainCreateInfo().imageFormat,
                AttachmentExtent, 1, vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eTransferSrc);

            DepthStencilAttachment_ = std::make_unique<FDepthStencilAttachment>(
                VulkanContext_, "MainDepthStencilAttachment", Allocator, AllocationCreateInfo, vk::Format::eD32Sfloat,
                AttachmentExtent, 1, vk::SampleCountFlagBits::e1);

            DepthMapAttachment_ = std::make_unique<FDepthStencilAttachment>(
                VulkanContext_, "DepthMapAttachment", Allocator, AllocationCreateInfo, vk::Format::eD32Sfloat,
                DepthMapExtent, 1, vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

            PositionAoAttachmentInfo_.setImageView(*PositionAoAttachment_->GetImageView());
            NormalRoughAttachmentInfo_.setImageView(*NormalRoughAttachment_->GetImageView());
            AlbedoMetalAttachmentInfo_.setImageView(*AlbedoMetalAttachment_->GetImageView());
            ShadowAttachmentInfo_.setImageView(*ShadowAttachment_->GetImageView());
            ColorAttachmentInfo_.setImageView(*ColorAttachment_->GetImageView());
            //                     .setResolveImageView(*ResolveAttachment_->GetImageView

            DepthStencilAttachmentInfo_.setImageView(*DepthStencilAttachment_->GetImageView());
            DepthMapAttachmentInfo_.setImageView(*DepthMapAttachment_->GetImageView());
            PostProcessAttachmentInfo_.setImageView(*ResolveAttachment_->GetImageView());
        };

        auto DestroyFramebuffers = [VulkanContext = VulkanContext_]() -> void
        {
            VulkanContext->WaitIdle();
        };

        CreateFramebuffers();

        VulkanContext_->RegisterRuntimeOnlyCallbacks(
            FVulkanContext::ECallbackType::kCreateSwapchain, "CreateFramebuffers", CreateFramebuffers);
        VulkanContext_->RegisterRuntimeOnlyCallbacks(
            FVulkanContext::ECallbackType::kDestroySwapchain, "DestroyFramebuffers", DestroyFramebuffers);
    }

    void FApplication::LoadAssets()
    {
        auto* AssetManager = EngineCoreServices->GetAssetManager();

        FResourceInfo PbrSceneVertShaderResourceInfo
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
            { "MatricesAddress" }
        };

        FResourceInfo LampShaderResourceInfo
        {
            {}, {}, {}, { "LightArgsAddress" }
        };

        AssetManager->AddAsset<FShader>("PbrSceneVertShader", "PbrScene.vert.spv", PbrSceneVertShaderResourceInfo);
        AssetManager->AddAsset<FShader>("PbrSceneGbufferShader", "PbrSceneGBuffer.frag.spv", FResourceInfo{});
        AssetManager->AddAsset<FShader>("LampShader", "PbrScene_Lamp.frag.spv", LampShaderResourceInfo);

        //FResourceInfo PbrSceneResourceInfo
        //{
        //    {
        //        { 0, sizeof(FVertex), false },
        //        { 1, sizeof(FInstanceData), true }
        //    },
        //    {
        //        { 0, 0, offsetof(FVertex, Position) },
        //        { 0, 1, offsetof(FVertex, Normal) },
        //        { 0, 2, offsetof(FVertex, TexCoord) },
        //        { 0, 3, offsetof(FVertex, Tangent) },
        //        { 0, 4, offsetof(FVertex, Bitangent) },
        //        { 1, 5, offsetof(FInstanceData, Model) }
        //    },
        //    {},
        //    {
        //        { vk::ShaderStageFlagBits::eVertex, { "MatricesAddress" } },
        //        { vk::ShaderStageFlagBits::eFragment, { "LightArgsAddress" } }
        //    }
        //};

        FResourceInfo PbrSceneMergeResourceInfo
        {
            {}, {}, {}, { "LightArgsAddress" }
        };

        AssetManager->AddAsset<FShader>("PbrSceneMergeShader", "PbrSceneMerge.comp.spv", PbrSceneMergeResourceInfo);

        FResourceInfo DepthMapResourceInfo
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
            { "MatricesAddress" }
        };

        AssetManager->AddAsset<FShader>("DepthMapShader", "DepthMap.vert.spv", DepthMapResourceInfo);

        //FResourceInfo TerrainResourceInfo
        //{
        //    { { 0, sizeof(FPatchVertex), false } },
        //    {
        //        { 0, 0, offsetof(FPatchVertex, Position) },
        //        { 0, 1, offsetof(FPatchVertex, TexCoord) }
        //    },
        //    { { 0, 0, false } },
        //    { { vk::ShaderStageFlagBits::eTessellationControl, { "MinDistance", "MaxDistance", "MinTessLevel", "MaxTessLevel" } } }
        //};

        FResourceInfo SkyboxResourceInfo
        {
            { { 0, sizeof(FSkyboxVertex), false } },
            { { 0, 0, offsetof(FSkyboxVertex, Position) } },
            {},
            { "MatricesAddress" }
        };

        AssetManager->AddAsset<FShader>("SkyboxVertShader", "Skybox.vert.spv", SkyboxResourceInfo);
        AssetManager->AddAsset<FShader>("SkyboxFragShader", "Skybox.frag.spv", FResourceInfo{});

        FResourceInfo PostVertShaderResourceInfo
        {
            { { 0, sizeof(FQuadVertex), false } },
            {
                { 0, 0, offsetof(FQuadVertex, Position) },
                { 0, 1, offsetof(FQuadVertex, TexCoord) }
            },
            {}, {}
        };

        FResourceInfo PostFragShaderResourceInfo
        {
            {}, {}, {}, { "bEnableHdr" }
        };

        AssetManager->AddAsset<FShader>("PostVertShader", "PostProcess.vert.spv", PostVertShaderResourceInfo);
        AssetManager->AddAsset<FShader>("PostFragShader", "PostProcess.frag.spv", PostFragShaderResourceInfo);

        //std::vector<std::string> PbrSceneGBufferShaderFiles({ "PbrScene.vert.spv", "PbrSceneGBuffer.frag.spv" });
        //std::vector<std::string> PbrSceneMergeShaderFiles({ "PbrSceneMerge.comp.spv" });
        //std::vector<std::string> PbrSceneShaderFiles({ "PbrScene.vert.spv", "PbrScene.frag.spv" });
        //std::vector<std::string> LampShaderFiles({ "PbrScene.vert.spv", "PbrScene_Lamp.frag.spv" });
        //std::vector<std::string> DepthMapShaderFiles({ "DepthMap.vert.spv" });
        //// std::vector<std::string> TerrainShaderFiles({ "Terrain.vert.spv", "Terrain.tesc.spv", "Terrain.tese.spv", "Terrain.frag.spv" });
        //std::vector<std::string> SkyboxShaderFiles({ "Skybox.vert.spv", "Skybox.frag.spv" });
        //std::vector<std::string> PostShaderFiles({ "PostProcess.vert.spv", "PostProcess.frag.spv" });

        //AssetManager->AddAsset<FShader>("PbrSceneGBufferShader", PbrSceneGBufferShaderFiles, PbrSceneGBufferResourceInfo);
        //AssetManager->AddAsset<FShader>("PbrSceneMergeShader", PbrSceneMergeShaderFiles, PbrSceneMergeResourceInfo);
        //AssetManager->AddAsset<FShader>("PbrSceneShader", PbrSceneShaderFiles, PbrSceneResourceInfo);
        //AssetManager->AddAsset<FShader>("LampShader", LampShaderFiles, PbrSceneResourceInfo);
        //AssetManager->AddAsset<FShader>("DepthMapShader", DepthMapShaderFiles, DepthMapResourceInfo);
        //// AssetManager->AddAsset<FShader>("TerrainShader", TerrainShaderFiles, TerrainResourceInfo);
        //AssetManager->AddAsset<FShader>("SkyboxShader", SkyboxShaderFiles, SkyboxResourceInfo);
        //AssetManager->AddAsset<FShader>("PostShader", PostShaderFiles, PostResourceInfo);

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

#if !defined(_DEBUG) && 0
        TextureFiles =
        {
            "CliffSide/cliff_side_disp_4k.exr",
            "CliffSide/cliff_side_diff_4k.exr",
            "CliffSide/cliff_side_nor_dx_4k.exr",
            "CliffSide/cliff_side_arm_4k.exr"
        };

        InitialTextureFormats =
        {
            vk::Format::eR16G16B16A16Sfloat,
            vk::Format::eR16G16B16A16Sfloat,
            vk::Format::eR16G16B16A16Sfloat,
            vk::Format::eR16G16B16A16Sfloat
        };

        FinalTextureFormats =
        {
            vk::Format::eR16G16B16A16Sfloat,
            vk::Format::eR16G16B16A16Sfloat,
            vk::Format::eR16G16B16A16Sfloat,
            vk::Format::eR16G16B16A16Sfloat
        };
#endif

        auto Allocator = VulkanContext_->GetVmaAllocator();

        VmaAllocationCreateInfo TextureAllocationCreateInfo
        {
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_GPU_ONLY
        };

        AssetManager->AddAsset<FTextureCube>(
            "Skybox", Allocator, TextureAllocationCreateInfo, "Skybox",
            vk::Format::eR8G8B8A8Srgb, vk::Format::eR8G8B8A8Srgb, vk::ImageCreateFlags(), true);

        //AssetManager->AddAsset<FTexture2D>(
        //    "PureSky", TextureAllocationCreateInfo, "HDRI/autumn_field_puresky_16k.hdr",
        //    vk::Format::eR16G16B16A16Sfloat, vk::Format::eR16G16B16A16Sfloat, vk::ImageCreateFlags(), false);

        // AssetManager->AddAsset<FTexture2D>(
        //     "IceLand", TextureAllocationCreateInfo, "IceLandHeightMapLowRes.png",
        //     vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::ImageCreateFlags(), false);

        std::vector<std::future<void>> Futures;

        for (std::size_t i = 0; i != TextureNames.size(); ++i)
        {
            Futures.push_back(ThreadPool_->Submit([&, i]() -> void
            {
                AssetManager->AddAsset<FTexture2D>(
                    TextureNames[i], Allocator, TextureAllocationCreateInfo, TextureFiles[i],
                    InitialTextureFormats[i], FinalTextureFormats[i], vk::ImageCreateFlagBits::eMutableFormat, true);
            }));
        }

        for (auto& Future : Futures)
        {
            Future.get();
        }
    }

    void FApplication::CreateUniformBuffers()
    {
        FDataBufferCreateInfo MatricesCreateInfo
        {
            .Name    = "Matrices",
            .Fields  = { "View", "Projection", "LightSpaceMatrix" },
            .Set     = 0,
            .Binding = 0,
            .Usage   = vk::DescriptorType::eStorageBuffer
        };

        FDataBufferCreateInfo MvpMatricesCreateInfo
        {
            .Name    = "MvpMatrices",
            .Fields  = { "Model", "View", "Projection" },
            .Set     = 0,
            .Binding = 0,
            .Usage   = vk::DescriptorType::eStorageBuffer
        };

        FDataBufferCreateInfo LightArgsCreateInfo
        {
            .Name    = "LightArgs",
            .Fields  = { "LightPos", "LightColor", "CameraPos" },
            .Set     = 0,
            .Binding = 0,
            .Usage   = vk::DescriptorType::eStorageBuffer
        };

        auto* ShaderBufferManager = EngineResourceServices->GetShaderBufferManager();
        ShaderBufferManager->AllocateDataBuffers<FMatrices>(MatricesCreateInfo);
        ShaderBufferManager->AllocateDataBuffers<FMvpMatrices>(MvpMatricesCreateInfo);
        ShaderBufferManager->AllocateDataBuffers<FLightArgs>(LightArgsCreateInfo);
    }

    void FApplication::BindDescriptors()
    {
        auto* AssetManager = EngineCoreServices->GetAssetManager();

        //auto PbrSceneGBufferShader = AssetManager->AcquireAsset<FShader>("PbrSceneGBufferShader");
        //auto PbrSceneMergeShader   = AssetManager->AcquireAsset<FShader>("PbrSceneMergeShader");
        //auto PbrSceneShader        = AssetManager->AcquireAsset<FShader>("PbrSceneShader");
        //auto LampShader            = AssetManager->AcquireAsset<FShader>("LampShader");
        //auto DepthMapShader        = AssetManager->AcquireAsset<FShader>("DepthMapShader");
        //// auto TerrainShader         = AssetManager->AcquireAsset<FShader>("TerrainShader");
        //auto SkyboxShader          = AssetManager->AcquireAsset<FShader>("SkyboxShader");
        //auto PostShader            = AssetManager->AcquireAsset<FShader>("PostShader");

        auto PbrDisplacement = AssetManager->AcquireAsset<FTexture2D>("PbrDisplacement");
        auto PbrDiffuse      = AssetManager->AcquireAsset<FTexture2D>("PbrDiffuse");
        auto PbrNormal       = AssetManager->AcquireAsset<FTexture2D>("PbrNormal");
        auto PbrArm          = AssetManager->AcquireAsset<FTexture2D>("PbrArm");
        // auto IceLand         = AssetManager->AcquireAsset<FTexture2D>("IceLand");
        auto Skybox          = AssetManager->AcquireAsset<FTextureCube>("Skybox");

        auto* ShaderBufferManager = EngineResourceServices->GetShaderBufferManager();
        auto* ShaderManager = EngineResourceServices->GetShaderManager();

        FShaderAcquireInfo::FShaderInfo PbrSceneVertShaderInfo
        {
            .Name = "PbrSceneVertShader",
            .NextStage = vk::ShaderStageFlagBits::eFragment
        };

        FShaderAcquireInfo::FShaderInfo PbrSceneGbufferShaderInfo
        {
            .Name = "PbrSceneGbufferShader"
        };

        FShaderAcquireInfo::FShaderInfo LampShaderInfo
        {
            .Name = "LampShader"
        };

        FShaderAcquireInfo PbrSceneGbufferShaderAcquireInfo;
        PbrSceneGbufferShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eVertex, PbrSceneVertShaderInfo);
        PbrSceneGbufferShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eFragment, PbrSceneGbufferShaderInfo);
        auto* PbrSceneGbufferShaderRes = ShaderManager->AcquireShaderResource(PbrSceneGbufferShaderAcquireInfo);

        FShaderAcquireInfo::FShaderInfo SkyboxVertShaderInfo
        {
            .Name = "SkyboxVertShader",
            .NextStage = vk::ShaderStageFlagBits::eFragment
        };

        FShaderAcquireInfo::FShaderInfo SkyboxFragShaderInfo
        {
            .Name = "SkyboxFragShader"
        };

        FShaderAcquireInfo SkyboxShaderAcquireInfo;
        SkyboxShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eVertex, SkyboxVertShaderInfo);
        SkyboxShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eFragment, SkyboxFragShaderInfo);
        auto* SkyboxShaderRes = ShaderManager->AcquireShaderResource(SkyboxShaderAcquireInfo);

        FShaderAcquireInfo::FShaderInfo PbrSceneMergeShaderInfo
        {
            .Name = "PbrSceneMergeShader"
        };

        FShaderAcquireInfo PbrSceneMergeShaderAcquireInfo;
        PbrSceneMergeShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eCompute, PbrSceneMergeShaderInfo);
        auto* PbrSceneMergeShaderRes = ShaderManager->AcquireShaderResource(PbrSceneMergeShaderAcquireInfo);

        FShaderAcquireInfo::FShaderInfo PostVertShaderInfo
        {
            .Name = "PostVertShader",
            .NextStage = vk::ShaderStageFlagBits::eFragment
        };

        FShaderAcquireInfo::FShaderInfo PostFragShaderInfo
        {
            .Name = "PostFragShader"
        };

        FShaderAcquireInfo PostShaderAcquireInfo;
        PostShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eVertex, PostVertShaderInfo);
        PostShaderAcquireInfo.ShaderInfos.emplace(vk::ShaderStageFlagBits::eFragment, PostFragShaderInfo);
        auto* PostShaderRes = ShaderManager->AcquireShaderResource(PostShaderAcquireInfo);

        auto CreateAttachmentDescriptors = [=]() mutable -> void
        {
            FDescriptorBufferCreateInfo SceneGBufferDescriptorBufferCreateInfo;
            SceneGBufferDescriptorBufferCreateInfo.Name     = "SceneGBufferDescriptorBuffer";
            SceneGBufferDescriptorBufferCreateInfo.SetInfos = PbrSceneGbufferShaderRes->SetInfos;

            vk::SamplerCreateInfo SamplerCreateInfo = FTexture::CreateDefaultSamplerCreateInfo(VulkanContext_);
            static FVulkanSampler Sampler(VulkanContext_->GetDevice(), "CommonSampler", SamplerCreateInfo);
            SceneGBufferDescriptorBufferCreateInfo.SamplerInfos.emplace_back(0u, 0u, *Sampler);

            auto PbrDiffuseImageInfo = PbrDiffuse->CreateDescriptorImageInfo(nullptr);
            SceneGBufferDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(1u, 1u, PbrDiffuseImageInfo);

            auto PbrNormalImageInfo = PbrNormal->CreateDescriptorImageInfo(nullptr);
            SceneGBufferDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(1u, 2u, PbrNormalImageInfo);

            auto PbrArmImageInfo = PbrArm->CreateDescriptorImageInfo(nullptr);
            SceneGBufferDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(1u, 3u, PbrArmImageInfo);

            // auto HeightMapImageInfo = IceLand->CreateDescriptorImageInfo(kSampler);
            // TerrainShader->WriteSharedDescriptors<vk::DescriptorImageInfo>(1, 0, vk::DescriptorType::eCombinedImageSampler, HeightMapImageInfo);

            FDescriptorBufferCreateInfo SkyboxDescriptorBufferCreateInfo;
            SkyboxDescriptorBufferCreateInfo.Name     = "SkyboxDescriptorBuffer";
            SkyboxDescriptorBufferCreateInfo.SetInfos = SkyboxShaderRes->SetInfos;

            SamplerCreateInfo
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge);

            static FVulkanSampler SkyboxSampler(VulkanContext_->GetDevice(), "SkyboxSampler", SamplerCreateInfo);
            auto SkyboxImageInfo = Skybox->CreateDescriptorImageInfo(SkyboxSampler);
            SkyboxDescriptorBufferCreateInfo.CombinedImageSamplerInfos.emplace_back(0u, 0u, SkyboxImageInfo);

            FDescriptorBufferCreateInfo SceneMergeDescriptorBufferCreateInfo;
            SceneMergeDescriptorBufferCreateInfo.Name     = "SceneMergeDescriptorBuffer";
            SceneMergeDescriptorBufferCreateInfo.SetInfos = PbrSceneMergeShaderRes->SetInfos;

            FDescriptorBufferCreateInfo PostDescriptorBufferCreateInfo;
            PostDescriptorBufferCreateInfo.Name     = "PostDescriptorBuffer";
            PostDescriptorBufferCreateInfo.SetInfos = PostShaderRes->SetInfos;

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

            static FVulkanSampler FramebufferSampler(VulkanContext_->GetDevice(), "FramebufferSampler", SamplerCreateInfo);

            vk::DescriptorImageInfo ColorStorageImageInfo(
                *FramebufferSampler, *ColorAttachment_->GetImageView(), vk::ImageLayout::eGeneral);
            vk::DescriptorImageInfo ColorImageInfo(
                *FramebufferSampler, *ColorAttachment_->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::DescriptorImageInfo DepthMapImageInfo(
                *FramebufferSampler, *DepthMapAttachment_->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::DescriptorImageInfo PositionAoImageInfo(
                *FramebufferSampler, *PositionAoAttachment_->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::DescriptorImageInfo NormalRoughImageInfo(
                *FramebufferSampler, *NormalRoughAttachment_->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::DescriptorImageInfo AlbedoMetalImageInfo(
                *FramebufferSampler, *AlbedoMetalAttachment_->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::DescriptorImageInfo ShadowImageInfo(
                *FramebufferSampler, *ShadowAttachment_->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

            SceneGBufferDescriptorBufferCreateInfo.CombinedImageSamplerInfos.emplace_back(2u, 0u, DepthMapImageInfo);
            SceneMergeDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(2u, 0u, PositionAoImageInfo);
            SceneMergeDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(2u, 1u, NormalRoughImageInfo);
            SceneMergeDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(2u, 2u, AlbedoMetalImageInfo);
            SceneMergeDescriptorBufferCreateInfo.SampledImageInfos.emplace_back(2u, 3u, ShadowImageInfo);
            SceneMergeDescriptorBufferCreateInfo.StorageImageInfos.emplace_back(1u, 0u, ColorStorageImageInfo);
            PostDescriptorBufferCreateInfo.CombinedImageSamplerInfos.emplace_back(0u, 1u, ColorImageInfo);

            ShaderBufferManager->AllocateDescriptorBuffer(SceneGBufferDescriptorBufferCreateInfo);
            ShaderBufferManager->AllocateDescriptorBuffer(SceneMergeDescriptorBufferCreateInfo);
            ShaderBufferManager->AllocateDescriptorBuffer(SkyboxDescriptorBufferCreateInfo);
            ShaderBufferManager->AllocateDescriptorBuffer(PostDescriptorBufferCreateInfo);
        };

        auto DestroyAttachmentDescriptors = [ShaderBufferManager]() mutable -> void
        {
            ShaderBufferManager->FreeDescriptorBuffer("SceneGBufferDescriptorBuffer");
            ShaderBufferManager->FreeDescriptorBuffer("SceneMergeDescriptorBuffer");
            ShaderBufferManager->FreeDescriptorBuffer("SkyboxDescriptorBuffer");
            ShaderBufferManager->FreeDescriptorBuffer("PostDescriptorBuffer");
        };

        CreateAttachmentDescriptors();

        VulkanContext_->RegisterRuntimeOnlyCallbacks(
            FVulkanContext::ECallbackType::kCreateSwapchain, "CreateAttachmentDescriptors", CreateAttachmentDescriptors);
        VulkanContext_->RegisterRuntimeOnlyCallbacks(
            FVulkanContext::ECallbackType::kDestroySwapchain, "DestroyAttachmentDescriptors", DestroyAttachmentDescriptors);
    }

    void FApplication::InitializeInstanceData()
    {
        glm::mat4x4 Model(1.0f);
        // plane
        InstanceData_.emplace_back(Model);

        // cube
        Model = glm::mat4x4(1.0f);
        Model = glm::translate(Model, glm::vec3(0.0f, 1.5f, 0.0f));
        InstanceData_.emplace_back(Model);

        Model = glm::mat4x4(1.0f);
        Model = glm::translate(Model, glm::vec3(2.0f, 0.0f, 1.0f));
        InstanceData_.emplace_back(Model);

        Model = glm::mat4x4(1.0f);
        Model = glm::translate(Model, glm::vec3(-1.0f, 0.0f, 2.0f));
        Model = glm::scale(Model, glm::vec3(0.5f));
        Model = glm::rotate(Model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
        InstanceData_.emplace_back(Model);

        // sphere
        Model = glm::mat4x4(1.0f);
        InstanceData_.emplace_back(Model);

        // lamp
        glm::vec3 LightPos(-2.0f, 4.0f, -1.0f);
        Model = glm::mat4x4(1.0f);
        Model = glm::scale(glm::translate(Model, LightPos), glm::vec3(0.2f));
        InstanceData_.emplace_back(Model);
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
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        };

        Math::CalculateAllTangents(CubeVertices);
        Math::CalculateAllTangents(PlaneVertices);
        Math::CalculateTangentBitangent(kSegmentsX, kSegmentsY, SphereVertices);

        vk::BufferCreateInfo VertexBufferCreateInfo = vk::BufferCreateInfo()
            .setSize(SphereVertices.size() * sizeof(FVertex))
            .setUsage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

        auto Allocator = VulkanContext_->GetVmaAllocator();

        SphereVertexBuffer_ = std::make_unique<FDeviceLocalBuffer>(VulkanContext_, "SphereVertexBuffer", Allocator, AllocationCreateInfo, VertexBufferCreateInfo);
        SphereVertexBuffer_->CopyData(SphereVertices);

        VertexBufferCreateInfo.setSize(CubeVertices.size() * sizeof(FVertex));
        CubeVertexBuffer_ = std::make_unique<FDeviceLocalBuffer>(VulkanContext_, "CubeVertexBuffer", Allocator, AllocationCreateInfo, VertexBufferCreateInfo);
        CubeVertexBuffer_->CopyData(CubeVertices);

        VertexBufferCreateInfo.setSize(SkyboxVertices.size() * sizeof(FSkyboxVertex));
        SkyboxVertexBuffer_ = std::make_unique<FDeviceLocalBuffer>(VulkanContext_, "SkyboxVertexBuffer", Allocator, AllocationCreateInfo, VertexBufferCreateInfo);
        SkyboxVertexBuffer_->CopyData(SkyboxVertices);

        VertexBufferCreateInfo.setSize(PlaneVertices.size() * sizeof(FVertex));
        PlaneVertexBuffer_ = std::make_unique<FDeviceLocalBuffer>(VulkanContext_, "PlaneVertexBuffer", Allocator, AllocationCreateInfo, VertexBufferCreateInfo);
        PlaneVertexBuffer_->CopyData(PlaneVertices);

        VertexBufferCreateInfo.setSize(QuadVertices.size() * sizeof(FQuadVertex));
        QuadVertexBuffer_ = std::make_unique<FDeviceLocalBuffer>(VulkanContext_, "QuadVertexBuffer", Allocator, AllocationCreateInfo, VertexBufferCreateInfo);
        QuadVertexBuffer_->CopyData(QuadVertices);

        VertexBufferCreateInfo.setSize(InstanceData_.size() * sizeof(FInstanceData));
        InstanceBuffer_ = std::make_unique<FDeviceLocalBuffer>(VulkanContext_, "InstanceBuffer", Allocator, AllocationCreateInfo, VertexBufferCreateInfo);
        InstanceBuffer_->CopyData(InstanceData_);

        vk::BufferCreateInfo IndexBufferCreateInfo = vk::BufferCreateInfo()
            .setSize(SphereIndices.size() * sizeof(std::uint32_t))
            .setUsage(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);

        SphereIndexBuffer_ = std::make_unique<FDeviceLocalBuffer>(VulkanContext_, "SphereIndexBuffer", Allocator, AllocationCreateInfo, IndexBufferCreateInfo);
        SphereIndexBuffer_->CopyData(SphereIndices);
        SphereIndicesCount_ = static_cast<std::uint32_t>(SphereIndices.size());

#if 0
        // Create height map vertices
        // --------------------------
        auto* AssetManager = EngineCoreServices->GetAssetManager()
        auto* IceLand      = AssetManager->GetAsset<FTexture2D>("IceLand");

        int   ImageWidth  = static_cast<int>(IceLand->GetImageExtent().width);
        int   ImageHeight = static_cast<int>(IceLand->GetImageExtent().height);
        int   StartX      = -ImageWidth  / 2;
        int   StartZ      = -ImageHeight / 2;
        float TessWidth   =  ImageWidth  / static_cast<float>(TessResolution_);
        float TessHeight  =  ImageHeight / static_cast<float>(TessResolution_);

        std::vector<FPatchVertex> TerrainVertices;
        TerrainVertices.reserve(TessResolution_ * TessResolution_ * 4);
        for (int z = 0; z != TessResolution_; ++z)
        {
            for (int x = 0; x != TessResolution_; ++x)
            {
                float AxisX0 = StartX + TessWidth  *  x;
                float AxisX1 = StartX + TessWidth  * (x + 1);
                float AxisZ0 = StartZ + TessHeight *  z;
                float AxisZ1 = StartZ + TessHeight * (z + 1);

                float AxisU0 =  x      / static_cast<float>(TessResolution_);
                float AxisU1 = (x + 1) / static_cast<float>(TessResolution_);
                float AxisV0 =  z      / static_cast<float>(TessResolution_);
                float AxisV1 = (z + 1) / static_cast<float>(TessResolution_);

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
        TerrainVertexBuffer_ = std::make_unique<FDeviceLocalBuffer>(VulkanContext_, Allocator, AllocationCreateInfo, VertexBufferCreateInfo);
        TerrainVertexBuffer_->CopyData(TerrainVertices);
#endif
    }

    void FApplication::CreatePipelines()
    {
        // auto* PipelineManager = EngineResourceServices->GetPipelineManager();

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

        // PipelineManager->CreateGraphicsPipeline("PbrScenePipeline", "PbrSceneShader", ScenePipelineCreateInfoPack);
        // PipelineManager->CreateGraphicsPipeline("LampPipeline", "LampShader", ScenePipelineCreateInfoPack);

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

        // PipelineManager->CreateGraphicsPipeline("PbrSceneGBufferPipeline", "PbrSceneGBufferShader", SceneGBufferPipelineCreateInfoPack);

        // FGraphicsPipelineCreateInfoPack TerrainPipelineCreateInfoPack = ScenePipelineCreateInfoPack;
        // TerrainPipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::ePatchList);

        // vk::PipelineTessellationDomainOriginStateCreateInfo TessellationDomainOriginStateCreateInfo(vk::TessellationDomainOrigin::eUpperLeft);
        // TerrainPipelineCreateInfoPack.TessellationStateCreateInfo
        //     .setPNext(&TessellationDomainOriginStateCreateInfo)
        //     .setPatchControlPoints(4);

        // TerrainPipelineCreateInfoPack.RasterizationStateCreateInfo
        //     .setCullMode(vk::CullModeFlagBits::eBack)
        //     .setFrontFace(vk::FrontFace::eCounterClockwise)
        //     .setPolygonMode(vk::PolygonMode::eLine);

        // PipelineManager->CreateGraphicsPipeline("TerrainPipeline", "TerrainShader", TerrainPipelineCreateInfoPack);

        FGraphicsPipelineCreateInfoPack SkyboxPipelineCreateInfoPack = ScenePipelineCreateInfoPack;
        SkyboxPipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
        SkyboxPipelineCreateInfoPack.RasterizationStateCreateInfo = vk::PipelineRasterizationStateCreateInfo();
        SkyboxPipelineCreateInfoPack.DepthStencilStateCreateInfo.setDepthCompareOp(vk::CompareOp::eLessOrEqual);

        // PipelineManager->CreateGraphicsPipeline("SkyboxPipeline", "SkyboxShader", SkyboxPipelineCreateInfoPack);

        FGraphicsPipelineCreateInfoPack PostPipelineCreateInfoPack = ScenePipelineCreateInfoPack;

        vk::PipelineRenderingCreateInfo PostRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
            .setColorAttachmentCount(1)
            .setColorAttachmentFormats(VulkanContext_->GetSwapchainCreateInfo().imageFormat);

        PostPipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&PostRenderingCreateInfo);
        PostPipelineCreateInfoPack.DepthStencilStateCreateInfo  = vk::PipelineDepthStencilStateCreateInfo();
        PostPipelineCreateInfoPack.RasterizationStateCreateInfo = vk::PipelineRasterizationStateCreateInfo();
        PostPipelineCreateInfoPack.MultisampleStateCreateInfo   = vk::PipelineMultisampleStateCreateInfo();

        // PipelineManager->CreateGraphicsPipeline("PostPipeline", "PostShader", PostPipelineCreateInfoPack);

        FGraphicsPipelineCreateInfoPack DepthMapPipelineCreateInfoPack = ScenePipelineCreateInfoPack;

        vk::PipelineRenderingCreateInfo DepthMapRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
            .setDepthAttachmentFormat(vk::Format::eD32Sfloat);

        DepthMapPipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&DepthMapRenderingCreateInfo);
        DepthMapPipelineCreateInfoPack.RasterizationStateCreateInfo = vk::PipelineRasterizationStateCreateInfo();
        DepthMapPipelineCreateInfoPack.MultisampleStateCreateInfo   = vk::PipelineMultisampleStateCreateInfo();
        DepthMapPipelineCreateInfoPack.ColorBlendAttachmentStates.clear();

        // PipelineManager->CreateGraphicsPipeline("DepthMapPipeline", "DepthMapShader", DepthMapPipelineCreateInfoPack);

        vk::ComputePipelineCreateInfo PbrMergePipelineCreateInfo(vk::PipelineCreateFlagBits::eDescriptorBufferEXT);
        // PipelineManager->CreateComputePipeline("PbrSceneMergePipeline", "PbrSceneMergeShader", &PbrMergePipelineCreateInfo);
    }

    void FApplication::Terminate()
    {
        VulkanContext_->WaitIdle();
        glfwDestroyWindow(Window_);
        glfwTerminate();
        // FEngineServices::GetInstance()->ShutdownResourceServices();
    }

    bool FApplication::InitializeWindow()
    {
        if (glfwInit() == GLFW_FALSE)
        {
            NpgsCoreError("Failed to initialize GLFW.");
            return false;
        };

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        GLFWmonitor* PrimaryMonitor = bEnableFullscreen_ ? glfwGetPrimaryMonitor() : nullptr;
        if (PrimaryMonitor != nullptr)
        {
            const GLFWvidmode* Mode = glfwGetVideoMode(PrimaryMonitor);
            WindowSize_.width       = Mode->width;
            WindowSize_.height      = Mode->height;
        }

        Window_ = glfwCreateWindow(WindowSize_.width, WindowSize_.height, WindowTitle_.c_str(), PrimaryMonitor, nullptr);
        if (Window_ == nullptr)
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
            glfwDestroyWindow(Window_);
            glfwTerminate();
            return false;
        }

        for (std::uint32_t i = 0; i != ExtensionCount; ++i)
        {
            VulkanContext_->AddInstanceExtension(Extensions[i]);
        }

        if (vk::Result Result = VulkanContext_->CreateInstance(); Result != vk::Result::eSuccess)
        {
            NpgsCoreError("Failed to create Vulkan instance.");
            glfwDestroyWindow(Window_);
            glfwTerminate();
            return false;
        }

        vk::SurfaceKHR Surface;
        if (glfwCreateWindowSurface(VulkanContext_->GetInstance(), Window_, nullptr, reinterpret_cast<VkSurfaceKHR*>(&Surface)) != VK_SUCCESS)
        {
            NpgsCoreError("Failed to create window surface.");
            glfwDestroyWindow(Window_);
            glfwTerminate();
            return false;
        }
        VulkanContext_->SetSurface(Surface);

        // TODO config physical device select
        if (VulkanContext_->CreateDevice(0) != vk::Result::eSuccess)
        {
            NpgsCoreError("Failed to create Vulkan device.");
            glfwDestroyWindow(Window_);
            glfwTerminate();
            return false;
        }

        VulkanContext_->CreateSwapchain(WindowSize_, bEnableVSync_, bEnableHdr_);

        if (bEnableHdr_)
        {
            // Create temporary swapchain to query HDR support
            VulkanContext_->SetHdrMetadata(GetHdrMetadata());
            VulkanContext_->RecreateSwapchain();
        }

        FEngineServices::GetInstance()->InitializeResourceServices();

        Camera_ = std::make_unique<FCamera>(glm::vec3(0.0f, 0.0f, 3.0f));

        return true;
    }

    void FApplication::InitializeInputCallbacks()
    {
        glfwSetWindowUserPointer(Window_, this);
        glfwSetFramebufferSizeCallback(Window_, &FApplication::FramebufferSizeCallback);
        glfwSetScrollCallback(Window_, nullptr);
        glfwSetScrollCallback(Window_, &FApplication::ScrollCallback);
    }

    void FApplication::ShowTitleFps()
    {
        static double CurrentTime   = 0.0;
        static double PreviousTime  = glfwGetTime();
        static double LastFrameTime = 0.0;
        static int    FrameCount    = 0;

        CurrentTime   = glfwGetTime();
        DeltaTime_    = CurrentTime - LastFrameTime;
        LastFrameTime = CurrentTime;
        ++FrameCount;
        if (CurrentTime - PreviousTime >= 1.0)
        {
            glfwSetWindowTitle(Window_, (std::string(WindowTitle_) + " " + std::to_string(FrameCount)).c_str());
            FrameCount   = 0;
            PreviousTime = CurrentTime;
        }
    }

    void FApplication::ProcessInput()
    {
        if (glfwGetMouseButton(Window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            glfwSetCursorPosCallback(Window_, &FApplication::CursorPosCallback);
            glfwSetInputMode(Window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        if (glfwGetMouseButton(Window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
        {
            glfwSetCursorPosCallback(Window_, nullptr);
            glfwSetInputMode(Window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            bFirstMouse_ = true;
        }

        if (glfwGetKey(Window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(Window_, GLFW_TRUE);
        if (glfwGetKey(Window_, GLFW_KEY_W) == GLFW_PRESS)
            Camera_->ProcessKeyboard(FCamera::EMovement::kForward, DeltaTime_);
        if (glfwGetKey(Window_, GLFW_KEY_S) == GLFW_PRESS)
            Camera_->ProcessKeyboard(FCamera::EMovement::kBack, DeltaTime_);
        if (glfwGetKey(Window_, GLFW_KEY_A) == GLFW_PRESS)
            Camera_->ProcessKeyboard(FCamera::EMovement::kLeft, DeltaTime_);
        if (glfwGetKey(Window_, GLFW_KEY_D) == GLFW_PRESS)
            Camera_->ProcessKeyboard(FCamera::EMovement::kRight, DeltaTime_);
        if (glfwGetKey(Window_, GLFW_KEY_R) == GLFW_PRESS)
            Camera_->ProcessKeyboard(FCamera::EMovement::kUp, DeltaTime_);
        if (glfwGetKey(Window_, GLFW_KEY_F) == GLFW_PRESS)
            Camera_->ProcessKeyboard(FCamera::EMovement::kDown, DeltaTime_);
        if (glfwGetKey(Window_, GLFW_KEY_Q) == GLFW_PRESS)
            Camera_->ProcessKeyboard(FCamera::EMovement::kRollLeft, DeltaTime_);
        if (glfwGetKey(Window_, GLFW_KEY_E) == GLFW_PRESS)
            Camera_->ProcessKeyboard(FCamera::EMovement::kRollRight, DeltaTime_);
        if (glfwGetKey(Window_, GLFW_KEY_C) == GLFW_PRESS)
            Camera_->AlignCamera();
    }

    vk::HdrMetadataEXT FApplication::GetHdrMetadata()
    {
        IDXGIFactory6* Factory6 = nullptr;
        if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&Factory6))))
        {
            NpgsCoreError("Failed to create DXGI factory.");
            return {};
        }

        FLuminancePack LuminancePack = GetLuminanceSync();
        HMONITOR       Monitor       = MonitorFromWindow(glfwGetWin32Window(Window_), MONITOR_DEFAULTTOPRIMARY);
        IDXGIAdapter1* Adapter1      = nullptr;
        IDXGIOutput*   Output        = nullptr;
        IDXGIOutput6*  Output6       = nullptr;
        UINT           AdapterIndex  = 0;
        bool           bFound        = false;

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
                               .setMaxLuminance(LuminancePack.MaxLuminance)
                               .setMinLuminance(LuminancePack.MinLuminance)
                               .setMaxContentLightLevel(10000.0f)
                               .setMaxFrameAverageLightLevel(LuminancePack.MaxAverageFullFrameLuminance);
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

        App->WindowSize_.width  = Width;
        App->WindowSize_.height = Height;
        App->VulkanContext_->WaitIdle();
        App->VulkanContext_->RecreateSwapchain();
    }

    void FApplication::CursorPosCallback(GLFWwindow* Window, double PosX, double PosY)
    {
        auto* Application = static_cast<FApplication*>(glfwGetWindowUserPointer(Window));

        if (Application->bFirstMouse_)
        {
            Application->LastX_ = PosX;
            Application->LastY_ = PosY;
            Application->bFirstMouse_ = false;
        }

        glm::vec2 Offset(PosX - Application->LastX_, Application->LastY_ - PosY);

        Application->LastX_ = PosX;
        Application->LastY_ = PosY;
        Application->Camera_->SetTargetOffset(Offset);
    }

    void FApplication::ScrollCallback(GLFWwindow* Window, double OffsetX, double OffsetY)
    {
        auto* Application = static_cast<FApplication*>(glfwGetWindowUserPointer(Window));
        Application->Camera_->ProcessMouseScroll(OffsetY);
    }
} // namespace Npgs
