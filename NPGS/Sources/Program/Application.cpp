#include "Application.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <future>

#include <dxgi1_6.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vulkan/vulkan.hpp>
#include <Windows.h>

#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Math/NumericConstants.h"
#include "Engine/Core/Math/TangentSpaceTools.h"
#include "Engine/Core/Runtime/AssetLoaders/Shader.h"
#include "Engine/Core/Runtime/AssetLoaders/Texture.h"
#include "Engine/Core/Runtime/Graphics/Renderers/PipelineManager.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/ShaderBufferManager.h"
#include "Engine/Utils/Logger.h"

_NPGS_BEGIN

namespace Art    = Runtime::Asset;
namespace Grt    = Runtime::Graphics;
namespace SysSpa = System::Spatial;

FApplication::FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle,
                           bool bEnableVSync, bool bEnableFullscreen, bool bEnableHdr)
    :
    _VulkanContext(Grt::FVulkanContext::GetClassInstance()),
    _ThreadPool(Runtime::Thread::FThreadPool::GetInstance()),
    _WindowTitle(WindowTitle),
    _WindowSize(WindowSize),
    _bEnableVSync(bEnableVSync),
    _bEnableFullscreen(bEnableFullscreen),
    _bEnableHdr(bEnableHdr)
{
    if (!InitializeWindow())
    {
        NpgsCoreError("Failed to create application.");
    }
}

FApplication::~FApplication()
{
    Terminate();
}

void FApplication::ExecuteMainRender()
{
    auto* AssetManager        = Art::FAssetManager::GetInstance();
    auto* ShaderBufferManager = Grt::FShaderBufferManager::GetInstance();
    auto* PipelineManager     = Grt::FPipelineManager::GetInstance();

    vk::Extent2D ShadowMapExtent(8192, 8192);

    CreateAttachments();
    LoadAssets();
    CreateUniformBuffers();
    BindDescriptorSets(AssetManager);
    InitInstanceData();
    InitVerticesData();
    CreatePipelines();

    auto* SceneShader     = AssetManager->GetAsset<Art::FShader>("SceneShader");
    auto* PbrSceneShader  = AssetManager->GetAsset<Art::FShader>("PbrSceneShader");
    auto* LampShader      = AssetManager->GetAsset<Art::FShader>("LampShader");
    auto* ShadowMapShader = AssetManager->GetAsset<Art::FShader>("ShadowMapShader");
    auto* SkyboxShader    = AssetManager->GetAsset<Art::FShader>("SkyboxShader");
    auto* PostShader      = AssetManager->GetAsset<Art::FShader>("PostShader");

    vk::Pipeline ScenePipeline;
    vk::Pipeline PbrScenePipeline;
    vk::Pipeline LampPipeline;
    vk::Pipeline ShadowMapPipeline;
    vk::Pipeline PostPipeline;
    vk::Pipeline SkyboxPipeline;

    auto GetPipelines = [&]() -> void
    {
        ScenePipeline     = PipelineManager->GetPipeline("ScenePipeline");
        PbrScenePipeline  = PipelineManager->GetPipeline("PbrScenePipeline");
        LampPipeline      = PipelineManager->GetPipeline("LampPipeline");
        ShadowMapPipeline = PipelineManager->GetPipeline("ShadowMapPipeline");
        PostPipeline      = PipelineManager->GetPipeline("PostPipeline");
        SkyboxPipeline    = PipelineManager->GetPipeline("SkyboxPipeline");
    };

    GetPipelines();

    _VulkanContext->RegisterAutoRemovedCallbacks(
        Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "GetPipelines", GetPipelines);

    auto ScenePipelineLayout     = PipelineManager->GetPipelineLayout("ScenePipeline");
    auto PbrScenePipelineLayout  = PipelineManager->GetPipelineLayout("PbrScenePipeline");
    auto LampPipelineLayout      = PipelineManager->GetPipelineLayout("LampPipeline");
    auto ShadowMapPipelineLayout = PipelineManager->GetPipelineLayout("ShadowMapPipeline");
    auto PostPipelineLayout      = PipelineManager->GetPipelineLayout("PostPipeline");
    auto SkyboxPipelineLayout    = PipelineManager->GetPipelineLayout("SkyboxPipeline");

    std::vector<Grt::FVulkanFence> InFlightFences;
    std::vector<Grt::FVulkanSemaphore> Semaphores_ImageAvailable;
    std::vector<Grt::FVulkanSemaphore> Semaphores_RenderFinished;
    std::vector<Grt::FVulkanCommandBuffer> CommandBuffers(Config::Graphics::kMaxFrameInFlight);
    for (std::size_t i = 0; i != Config::Graphics::kMaxFrameInFlight; ++i)
    {
        InFlightFences.emplace_back(vk::FenceCreateFlagBits::eSignaled);
        Semaphores_ImageAvailable.emplace_back(vk::SemaphoreCreateFlags());
        Semaphores_RenderFinished.emplace_back(vk::SemaphoreCreateFlags());
    }

    _VulkanContext->GetGraphicsCommandPool().AllocateBuffers(vk::CommandBufferLevel::ePrimary, CommandBuffers);

    vk::DeviceSize Offset        = 0;
    std::uint32_t  DynamicOffset = 0;
    std::uint32_t  CurrentFrame  = 0;

    glm::vec3   LightPos(-2.0f, 4.0f, -1.0f);
    glm::mat4x4 LightProjection  = glm::infinitePerspective(glm::radians(60.0f), 1.0f, 1.0f);
    glm::mat4x4 LightView        = glm::lookAt(LightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4x4 LightSpaceMatrix = LightProjection * LightView;

    vk::ImageSubresourceRange ColorSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    vk::ImageSubresourceRange DepthSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1);

    std::array PlaneVertexBuffers{ *_PlaneVertexBuffer->GetBuffer(), *_InstanceBuffer->GetBuffer() };
    std::array CubeVertexBuffers{ *_CubeVertexBuffer->GetBuffer(), *_InstanceBuffer->GetBuffer() };
    std::array SphereVertexBuffers{ *_SphereVertexBuffer->GetBuffer(), *_InstanceBuffer->GetBuffer() };
    std::array PlaneOffsets{ Offset, Offset };
    std::array CubeOffsets{ Offset, Offset + sizeof(glm::mat4x4) };
    std::array SphereOffsets{ Offset, Offset + 4 * sizeof(glm::mat4x4) };
    std::array LampOffsets{ Offset, Offset + 5 * sizeof(glm::mat4x4) };

    Grt::FMatrices      Matrices{};
    Grt::FLightMaterial LightMaterial{};
    Grt::FLightArgs     LightArgs{};

    auto CameraPosUpdater = ShaderBufferManager->GetFieldUpdaters<glm::aligned_vec3>("LightArgs", "CameraPos");

    LightArgs.LightPos   = LightPos;
    LightArgs.LightColor = glm::vec3(300.0f);

    ShaderBufferManager->UpdateEntrieBuffers("LightArgs", LightArgs);

    while (!glfwWindowShouldClose(_Window))
    {
        while (glfwGetWindowAttrib(_Window, GLFW_ICONIFIED))
        {
            glfwWaitEvents();
        }

        vk::Viewport CommonViewport(0.0f, 0.0f, static_cast<float>(_WindowSize.width),
                                    static_cast<float>(_WindowSize.height), 0.0f, 1.0f);

        vk::Viewport FlippedViewport(0.0f, static_cast<float>(_WindowSize.height), static_cast<float>(_WindowSize.width),
                                     -static_cast<float>(_WindowSize.height), 0.0f, 1.0f);

        vk::Viewport ShadowMapViewport(0.0f, 0.0f, static_cast<float>(ShadowMapExtent.width),
                                       static_cast<float>(ShadowMapExtent.height), 0.0f, 1.0f);

        vk::Rect2D CommonScissor(vk::Offset2D(), _WindowSize);
        vk::Rect2D ShadowMapScissor(vk::Offset2D(), ShadowMapExtent);

        InFlightFences[CurrentFrame].WaitAndReset();

        // Uniform update
        // --------------
        float WindowAspect = static_cast<float>(_WindowSize.width) / static_cast<float>(_WindowSize.height);

        Matrices.View             = _FreeCamera->GetViewMatrix();
        Matrices.Projection       = _FreeCamera->GetProjectionMatrix(WindowAspect, 0.001f);
        Matrices.LightSpaceMatrix = LightSpaceMatrix;

        LightMaterial.Material.Shininess = 64.0f;
        LightMaterial.Light.Position     = LightPos;
        LightMaterial.Light.Ambient      = glm::vec3(0.2f);
        LightMaterial.Light.Diffuse      = glm::vec3(12.5f);
        LightMaterial.Light.Specular     = glm::vec3(12.5f);
        LightMaterial.ViewPos            = _FreeCamera->GetCameraVector(SysSpa::FCamera::EVectorType::kPosition);

        ShaderBufferManager->UpdateEntrieBuffer(CurrentFrame, "Matrices", Matrices);
        ShaderBufferManager->UpdateEntrieBuffer(CurrentFrame, "LightMaterial", LightMaterial);

        CameraPosUpdater[CurrentFrame] << _FreeCamera->GetCameraVector(SysSpa::FCamera::EVectorType::kPosition);

        _VulkanContext->SwapImage(*Semaphores_ImageAvailable[CurrentFrame]);
        std::uint32_t ImageIndex = _VulkanContext->GetCurrentImageIndex();

        // Record commands
        // ---------------
        auto& CurrentBuffer = CommandBuffers[CurrentFrame];
        CurrentBuffer.Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        vk::ImageMemoryBarrier2 InitSwapchainBarrier(
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            _VulkanContext->GetSwapchainImage(ImageIndex),
            ColorSubresourceRange
        );

        vk::ImageMemoryBarrier2 InitColorAttachmentBarrier(
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

        vk::ImageMemoryBarrier2 InitDepthAttachmentBarrier(
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            *_ShadowMapAttachment->GetImage(),
            DepthSubresourceRange
        );

        std::array InitBarriers{ InitSwapchainBarrier, InitColorAttachmentBarrier, InitDepthAttachmentBarrier };

        vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(InitBarriers);

        CurrentBuffer->pipelineBarrier2(InitialDependencyInfo);

        // Set shadow map viewport
        CurrentBuffer->setViewport(0, ShadowMapViewport);
        CurrentBuffer->setScissor(0, ShadowMapScissor);

        vk::RenderingInfo ShadowMapRenderingInfo = vk::RenderingInfo()
            .setRenderArea(ShadowMapScissor)
            .setLayerCount(1)
            .setPDepthAttachment(&_ShadowMapAttachmentInfo);

        CurrentBuffer->beginRendering(ShadowMapRenderingInfo);
        // Draw scene for depth mapping
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, ShadowMapPipeline);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ShadowMapPipelineLayout, 0,
                                          ShadowMapShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->bindVertexBuffers(0, PlaneVertexBuffers, PlaneOffsets);
        CurrentBuffer->draw(6, 1, 0, 0);
        CurrentBuffer->bindVertexBuffers(0, CubeVertexBuffers, CubeOffsets);
        CurrentBuffer->draw(36, 3, 0, 0);
        CurrentBuffer->endRendering();

        vk::ImageMemoryBarrier2 DepthRenderEndBarrier(
            vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            *_ShadowMapAttachment->GetImage(),
            DepthSubresourceRange
        );

        vk::DependencyInfo DepthRenderEndDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(DepthRenderEndBarrier);

        CurrentBuffer->pipelineBarrier2(DepthRenderEndDependencyInfo);

        // Set scene viewport
        CurrentBuffer->setViewport(0, FlippedViewport);
        CurrentBuffer->setScissor(0, CommonScissor);

        vk::RenderingInfo SceneRenderingInfo = vk::RenderingInfo()
            .setRenderArea(CommonScissor)
            .setLayerCount(1)
            .setColorAttachments(_ColorAttachmentInfo)
            .setPDepthAttachment(&_DepthStencilAttachmentInfo);

        CurrentBuffer->beginRendering(SceneRenderingInfo);

        //CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, PbrScenePipeline);
        //CurrentBuffer->bindVertexBuffers(0, SphereVertexBuffers, SphereOffsets);
        //CurrentBuffer->bindIndexBuffer(*_SphereIndexBuffer->GetBuffer(), 0, vk::IndexType::eUint32);
        //CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PbrScenePipelineLayout, 0,
        //                                  PbrSceneShader->GetDescriptorSets(CurrentFrame), {});
        //CurrentBuffer->drawIndexed(_SphereIndicesCount, 1, 0, 0, 0);

        // Draw plane
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, PbrScenePipeline);
        CurrentBuffer->bindVertexBuffers(0, PlaneVertexBuffers, PlaneOffsets);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PbrScenePipelineLayout, 0,
                                          PbrSceneShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(6, 1, 0, 0);

        // Draw cube
        CurrentBuffer->bindVertexBuffers(0, CubeVertexBuffers, CubeOffsets);
        CurrentBuffer->draw(36, 3, 0, 0);

        // Draw lamp
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, LampPipeline);
        CurrentBuffer->bindVertexBuffers(0, CubeVertexBuffers, LampOffsets);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, LampPipelineLayout, 0,
                                          LampShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(36, 3, 0, 0);
        CurrentBuffer->endRendering();

        // Draw skybox
        _ColorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);
        _DepthStencilAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);

        CurrentBuffer->beginRendering(SceneRenderingInfo);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, SkyboxPipeline);
        CurrentBuffer->bindVertexBuffers(0, *_SkyboxVertexBuffer->GetBuffer(), Offset);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, SkyboxPipelineLayout, 0,
                                          SkyboxShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(36, 3, 0, 0);
        CurrentBuffer->endRendering();

        _ColorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);
        _DepthStencilAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);

        vk::ImageMemoryBarrier2 ColorRenderEndBarrier(
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            *_ResolveAttachment->GetImage(),
            ColorSubresourceRange
        );

        vk::DependencyInfo ColorRenderEndDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(ColorRenderEndBarrier);

        CurrentBuffer->pipelineBarrier2(ColorRenderEndDependencyInfo);

        // Post process
        CurrentBuffer->setViewport(0, CommonViewport);

        vk::RenderingInfo PostRenderingInfo = vk::RenderingInfo()
            .setRenderArea(CommonScissor)
            .setLayerCount(1)
            .setColorAttachments(_PostProcessAttachmentInfo);

        _PostProcessAttachmentInfo.setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex));

        CurrentBuffer->beginRendering(PostRenderingInfo);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, PostPipeline);
        CurrentBuffer->bindVertexBuffers(0, *_QuadVertexBuffer->GetBuffer(), Offset);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PostPipelineLayout, 0,
                                          PostShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->pushConstants(PostPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0,
                                     sizeof(vk::Bool32), reinterpret_cast<vk::Bool32*>(&_bEnableHdr));
        CurrentBuffer->draw(6, 1, 0, 0);
        CurrentBuffer->endRendering();

        vk::ImageMemoryBarrier2 PresentBarrier(
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eBottomOfPipe,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eColorAttachmentOptimal,
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

        ProcessInput();
        glfwPollEvents();
        ShowTitleFps();    
    }

    _VulkanContext->WaitIdle();
    _VulkanContext->GetGraphicsCommandPool().FreeBuffers(CommandBuffers);
}

void FApplication::CreateAttachments()
{
    _ColorAttachmentInfo
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setResolveMode(vk::ResolveModeFlagBits::eAverage)
        .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

    _DepthStencilAttachmentInfo
        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

    _ShadowMapAttachmentInfo
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

        vk::Extent2D ShadowMapExtent(8192, 8192);

        _VulkanContext->WaitIdle();

        _ColorAttachment = std::make_unique<Grt::FColorAttachment>(
            AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e8);

        _ResolveAttachment = std::make_unique<Grt::FColorAttachment>(
            AllocationCreateInfo, vk::Format::eR16G16B16A16Sfloat, _WindowSize, 1,
            vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

        _DepthStencilAttachment = std::make_unique<Grt::FDepthStencilAttachment>(
            AllocationCreateInfo, vk::Format::eD32Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e8);

        _ShadowMapAttachment = std::make_unique<Grt::FDepthStencilAttachment>(
            AllocationCreateInfo, vk::Format::eD32Sfloat, ShadowMapExtent, 1,
            vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

        _ColorAttachmentInfo.setImageView(*_ColorAttachment->GetImageView())
                            .setResolveImageView(*_ResolveAttachment->GetImageView());

        _DepthStencilAttachmentInfo.setImageView(*_DepthStencilAttachment->GetImageView());
        _ShadowMapAttachmentInfo.setImageView(*_ShadowMapAttachment->GetImageView());
    };

    auto DestroyFramebuffers = [&]() -> void
    {
        _VulkanContext->WaitIdle();
    };

    CreateFramebuffers();

    _VulkanContext->RegisterAutoRemovedCallbacks(
        Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreateFramebuffers", CreateFramebuffers);
    _VulkanContext->RegisterAutoRemovedCallbacks(
        Grt::FVulkanContext::ECallbackType::kDestroySwapchain, "DestroyFramebuffers", DestroyFramebuffers);
}

void FApplication::LoadAssets()
{
    Art::FShader::FResourceInfo SceneResourceInfo
    {
        {
            { 0, sizeof(Grt::FVertex), false },
            { 1, sizeof(Grt::FInstanceData), true }
        },
        {
            { 0, 0, offsetof(Grt::FVertex, Position) },
            { 0, 1, offsetof(Grt::FVertex, Normal) },
            { 0, 2, offsetof(Grt::FVertex, TexCoord) },
            { 0, 3, offsetof(Grt::FVertex, Tangent) },
            { 0, 4, offsetof(Grt::FVertex, Bitangent) },
            { 1, 5, offsetof(Grt::FInstanceData, Model) }
        },
        {
            { 0, 0, false },
            { 0, 1, false }
        }
    };

    Art::FShader::FResourceInfo ShadowMapResourceInfo
    {
        {
            { 0, sizeof(Grt::FVertex), false },
            { 1, sizeof(Grt::FInstanceData), true }
        },
        {
            { 0, 0, offsetof(Grt::FVertex, Position) },
            { 1, 1, offsetof(Grt::FInstanceData, Model) }
        },
        { { 0, 0, false } },
    };

    Art::FShader::FResourceInfo SkyboxResourceInfo
    {
        { { 0, sizeof(Grt::FSkyboxVertex), false } },
        { { 0, 0, offsetof(Grt::FSkyboxVertex, Position) } },
        { { 0, 0, false } }
    };

    Art::FShader::FResourceInfo PostResourceInfo
    {
        { { 0, sizeof(Grt::FQuadVertex), false } },
        {
            { 0, 0, offsetof(Grt::FQuadVertex, Position) },
            { 0, 1, offsetof(Grt::FQuadVertex, TexCoord) }
        },
        {},
        { { vk::ShaderStageFlagBits::eFragment, {"iEnableHdr"} } }
    };

    std::vector<std::string> SceneShaderFiles({ "Scene.vert.spv", "Scene.frag.spv" });
    std::vector<std::string> PbrSceneShaderFiles({ "PbrScene.vert.spv", "PbrScene.frag.spv" });
    std::vector<std::string> LampShaderFiles({ "Scene.vert.spv", "Scene_Lamp.frag.spv" });
    std::vector<std::string> ShadowMapShaderFiles({ "ShadowMap.vert.spv", "ShadowMap.frag.spv" });
    std::vector<std::string> SkyboxShaderFiles({ "Skybox.vert.spv", "Skybox.frag.spv" });
    std::vector<std::string> PostShaderFiles({ "PostProcess.vert.spv", "PostProcess.frag.spv" });

    VmaAllocationCreateInfo TextureAllocationCreateInfo
    {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    auto* AssetManager = Art::FAssetManager::GetInstance();
    AssetManager->AddAsset<Art::FShader>("SceneShader", SceneShaderFiles, SceneResourceInfo);
    AssetManager->AddAsset<Art::FShader>("PbrSceneShader", PbrSceneShaderFiles, SceneResourceInfo);
    AssetManager->AddAsset<Art::FShader>("LampShader", LampShaderFiles, SceneResourceInfo);
    AssetManager->AddAsset<Art::FShader>("ShadowMapShader", ShadowMapShaderFiles, ShadowMapResourceInfo);
    AssetManager->AddAsset<Art::FShader>("SkyboxShader", SkyboxShaderFiles, SkyboxResourceInfo);
    AssetManager->AddAsset<Art::FShader>("PostShader", PostShaderFiles, PostResourceInfo);

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
        "CliffSide/cliff_side_diff_4k.jpg",
        "CliffSide/cliff_side_nor_dx_4k.jpg",
        "CliffSide/cliff_side_arm_4k.jpg"
    };

    std::vector<vk::Format> TextureFormats
    {
        vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Srgb,
        vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR8G8B8A8Unorm
    };

#if !defined(_DEBUG)
    // TextureFiles =
    // {
    //     "CliffSide/cliff_side_disp_16k.jpg",
    //     "CliffSide/cliff_side_diff_16k.jpg",
    //     "CliffSide/cliff_side_nor_dx_16k.jpg",
    //     "CliffSide/cliff_side_arm_16k.jpg"
    // };
#endif

    AssetManager->AddAsset<Art::FTextureCube>(
        "Skybox", TextureAllocationCreateInfo, "Skybox", vk::Format::eR8G8B8A8Srgb,
        vk::Format::eR8G8B8A8Srgb, vk::ImageCreateFlags(), true, false);

    // std::vector<std::future<void>> Futures;

    for (std::size_t i = 0; i != TextureNames.size(); ++i)
    {
        // Futures.push_back(_ThreadPool->Submit([&, i]() -> void
        // {
            AssetManager->AddAsset<Art::FTexture2D>(TextureNames[i], TextureAllocationCreateInfo, TextureFiles[i],
                                                    TextureFormats[i], TextureFormats[i], vk::ImageCreateFlags(), true, false);
        // }));
    }

    // for (auto& Future : Futures)
    // {
    //     Future.get();
    // }
}

void FApplication::CreateUniformBuffers()
{
    Grt::FShaderBufferManager::FUniformBufferCreateInfo MatricesCreateInfo
    {
        .Name    = "Matrices",
        .Fields  = { "View", "Projection", "LightSpaceMatrix" },
        .Set     = 0,
        .Binding = 0,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    Grt::FShaderBufferManager::FUniformBufferCreateInfo LightMaterialCreateInfo
    {
        .Name    = "LightMaterial",
        .Fields  = { "Material", "Light", "ViewPos" },
        .Set     = 0,
        .Binding = 1,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    Grt::FShaderBufferManager::FUniformBufferCreateInfo LightArgsCreateInfo
    {
        .Name    = "LightArgs",
        .Fields  = { "LightPos", "LightColor", "CameraPos" },
        .Set     = 0,
        .Binding = 1,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    Grt::FShaderBufferManager::FUniformBufferCreateInfo PbrArgsCreateInfo
    {
        .Name    = "PbrArgs",
        .Fields  = { "Albedo", "Metallic", "Roughness", "AmbientOcclusion" },
        .Set     = 0,
        .Binding = 2,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    VmaAllocationCreateInfo AllocationCreateInfo
    {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    };

    auto* ShaderBufferManager = Grt::FShaderBufferManager::GetInstance();
    ShaderBufferManager->CreateBuffers<Grt::FMatrices>(MatricesCreateInfo, &AllocationCreateInfo);
    ShaderBufferManager->CreateBuffers<Grt::FLightMaterial>(LightMaterialCreateInfo, &AllocationCreateInfo);
    ShaderBufferManager->CreateBuffers<Grt::FLightArgs>(LightArgsCreateInfo, &AllocationCreateInfo);
    ShaderBufferManager->CreateBuffers<Grt::FPbrArgs>(PbrArgsCreateInfo, &AllocationCreateInfo);
}

void FApplication::BindDescriptorSets(Art::FAssetManager* AssetManager)
{
    auto* SceneShader     = AssetManager->GetAsset<Art::FShader>("SceneShader");
    auto* PbrSceneShader  = AssetManager->GetAsset<Art::FShader>("PbrSceneShader");
    auto* LampShader      = AssetManager->GetAsset<Art::FShader>("LampShader");
    auto* ShadowMapShader = AssetManager->GetAsset<Art::FShader>("ShadowMapShader");
    auto* SkyboxShader    = AssetManager->GetAsset<Art::FShader>("SkyboxShader");
    auto* PostShader      = AssetManager->GetAsset<Art::FShader>("PostShader");

    auto* PbrDisplacement = AssetManager->GetAsset<Art::FTexture2D>("PbrDisplacement");
    auto* PbrDiffuse      = AssetManager->GetAsset<Art::FTexture2D>("PbrDiffuse");
    auto* PbrNormal       = AssetManager->GetAsset<Art::FTexture2D>("PbrNormal");
    auto* PbrArm          = AssetManager->GetAsset<Art::FTexture2D>("PbrArm");
    auto* Skybox          = AssetManager->GetAsset<Art::FTextureCube>("Skybox");

    vk::SamplerCreateInfo SamplerCreateInfo = Art::FTextureBase::CreateDefaultSamplerCreateInfo();
    static Grt::FVulkanSampler kSampler(SamplerCreateInfo);

    vk::DescriptorImageInfo SamplerInfo(*kSampler);
    SceneShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eSampler, SamplerInfo);
    PbrSceneShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eSampler, SamplerInfo);

    //auto CliffSideDiffuseImageInfo = CliffSideDiffuse->CreateDescriptorImageInfo(nullptr);
    //SceneShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eSampledImage, CliffSideDiffuseImageInfo);

    //auto CliffSideNormalImageInfo = CliffSideNormal->CreateDescriptorImageInfo(nullptr);
    //SceneShader->WriteSharedDescriptors(1, 2, vk::DescriptorType::eSampledImage, CliffSideNormalImageInfo);

    //auto CliffSideDisplacementImageInfo = CliffSideDisplacement->CreateDescriptorImageInfo(nullptr);
    //SceneShader->WriteSharedDescriptors(1, 3, vk::DescriptorType::eSampledImage, CliffSideDisplacementImageInfo);

    auto PbrDiffuseImageInfo = PbrDiffuse->CreateDescriptorImageInfo(nullptr);
    PbrSceneShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eSampledImage, PbrDiffuseImageInfo);

    auto PbrNormalImageInfo = PbrNormal->CreateDescriptorImageInfo(nullptr);
    PbrSceneShader->WriteSharedDescriptors(1, 2, vk::DescriptorType::eSampledImage, PbrNormalImageInfo);

    auto PbrArmImageInfo = PbrArm->CreateDescriptorImageInfo(nullptr);
    PbrSceneShader->WriteSharedDescriptors(1, 3, vk::DescriptorType::eSampledImage, PbrArmImageInfo);

    SamplerCreateInfo
        .setMipmapMode(vk::SamplerMipmapMode::eNearest)
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeW(vk::SamplerAddressMode::eClampToEdge);

    static Grt::FVulkanSampler kSkyboxSampler(SamplerCreateInfo);

    auto SkyboxImageInfo = Skybox->CreateDescriptorImageInfo(kSkyboxSampler);
    SkyboxShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, SkyboxImageInfo);

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

    static Grt::FVulkanSampler kFramebufferSampler(SamplerCreateInfo);

    auto CreatePostDescriptors = [&, PostShader, SceneShader, PbrSceneShader]() -> void
    {
        vk::DescriptorImageInfo ColorImageInfo(
            *kFramebufferSampler, *_ResolveAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo ShadowMapImageInfo(
            *kFramebufferSampler, *_ShadowMapAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        PostShader->WriteSharedDescriptors(0, 0, vk::DescriptorType::eCombinedImageSampler, ColorImageInfo);
        SceneShader->WriteSharedDescriptors(1, 4, vk::DescriptorType::eCombinedImageSampler, ShadowMapImageInfo);
        PbrSceneShader->WriteSharedDescriptors(1, 4, vk::DescriptorType::eCombinedImageSampler, ShadowMapImageInfo);
    };

    CreatePostDescriptors();

    _VulkanContext->RegisterAutoRemovedCallbacks(
        Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreatePostDescriptor", CreatePostDescriptors);

    auto* ShaderBufferManager = Grt::FShaderBufferManager::GetInstance();
    ShaderBufferManager->BindShadersToBuffers("Matrices", "SceneShader", "PbrSceneShader", "LampShader", "ShadowMapShader", "SkyboxShader");
    ShaderBufferManager->BindShadersToBuffers("LightMaterial", "SceneShader", "LampShader");
    ShaderBufferManager->BindShaderToBuffers("LightArgs", "PbrSceneShader");
    // ShaderBufferManager->BindShaderToBuffers("PbrArgs", "PbrSceneShader");
}

void FApplication::InitInstanceData()
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

void FApplication::InitVerticesData()
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

    std::vector<Grt::FVertex> SphereVertices;
    for (std::size_t i = 0; i != Positions.size(); ++i)
    {
        Grt::FVertex Vertex{};
        Vertex.Position = Positions[i];
        Vertex.Normal   = Normals[i];
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
    Math::CalculateTangentBitangentSphere(SphereVertices, kSegmentsX, kSegmentsY);

    vk::BufferCreateInfo VertexBufferCreateInfo = vk::BufferCreateInfo()
        .setSize(SphereVertices.size() * sizeof(Grt::FVertex))
        .setUsage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

    _SphereVertexBuffer = std::make_unique<Grt::FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
    _SphereVertexBuffer->CopyData(SphereVertices);

    VertexBufferCreateInfo.setSize(CubeVertices.size() * sizeof(Grt::FVertex));
    _CubeVertexBuffer = std::make_unique<Grt::FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
    _CubeVertexBuffer->CopyData(CubeVertices);

    VertexBufferCreateInfo.setSize(SkyboxVertices.size() * sizeof(Grt::FSkyboxVertex));
    _SkyboxVertexBuffer = std::make_unique<Grt::FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
    _SkyboxVertexBuffer->CopyData(SkyboxVertices);

    VertexBufferCreateInfo.setSize(PlaneVertices.size() * sizeof(Grt::FVertex));
    _PlaneVertexBuffer = std::make_unique<Grt::FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
    _PlaneVertexBuffer->CopyData(PlaneVertices);

    VertexBufferCreateInfo.setSize(QuadVertices.size() * sizeof(Grt::FQuadVertex));
    _QuadVertexBuffer = std::make_unique<Grt::FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
    _QuadVertexBuffer->CopyData(QuadVertices);

    VertexBufferCreateInfo.setSize(_InstanceData.size() * sizeof(Grt::FInstanceData));
    _InstanceBuffer = std::make_unique<Grt::FDeviceLocalBuffer>(AllocationCreateInfo, VertexBufferCreateInfo);
    _InstanceBuffer->CopyData(_InstanceData);

    vk::BufferCreateInfo IndexBufferCreateInfo = vk::BufferCreateInfo()
        .setSize(SphereIndices.size() * sizeof(std::uint32_t))
        .setUsage(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);

    _SphereIndexBuffer = std::make_unique<Grt::FDeviceLocalBuffer>(AllocationCreateInfo, IndexBufferCreateInfo);
    _SphereIndexBuffer->CopyData(SphereIndices);
    _SphereIndicesCount = static_cast<std::uint32_t>(SphereIndices.size());
}

void FApplication::CreatePipelines()
{
    auto* PipelineManager = Grt::FPipelineManager::GetInstance();

    vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
        .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::Format AttachmentFormat = vk::Format::eR16G16B16A16Sfloat;

    vk::PipelineRenderingCreateInfo SceneRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(AttachmentFormat)
        .setDepthAttachmentFormat(vk::Format::eD32Sfloat);

    Grt::FGraphicsPipelineCreateInfoPack ScenePipelineCreateInfoPack;
    ScenePipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eViewport);
    ScenePipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eScissor);
    ScenePipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&SceneRenderingCreateInfo);
    ScenePipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    ScenePipelineCreateInfoPack.RasterizationStateCreateInfo
        .setCullMode(vk::CullModeFlagBits::eBack)
        .setFrontFace(vk::FrontFace::eCounterClockwise);

    ScenePipelineCreateInfoPack.MultisampleStateCreateInfo
        .setRasterizationSamples(vk::SampleCountFlagBits::e8)
        .setSampleShadingEnable(vk::True)
        .setMinSampleShading(1.0f);

    ScenePipelineCreateInfoPack.DepthStencilStateCreateInfo
        .setDepthTestEnable(vk::True)
        .setDepthWriteEnable(vk::True)
        .setDepthCompareOp(vk::CompareOp::eLess)
        .setDepthBoundsTestEnable(vk::False)
        .setStencilTestEnable(vk::False);

    ScenePipelineCreateInfoPack.ColorBlendAttachmentStates.emplace_back(ColorBlendAttachmentState);

    PipelineManager->CreateGraphicsPipeline("ScenePipeline", "SceneShader", ScenePipelineCreateInfoPack);
    PipelineManager->CreateGraphicsPipeline("LampPipeline", "LampShader", ScenePipelineCreateInfoPack);

    Grt::FGraphicsPipelineCreateInfoPack PbrScenePipelineCreateInfoPack = ScenePipelineCreateInfoPack;
    PbrScenePipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);

    PipelineManager->CreateGraphicsPipeline("PbrScenePipeline", "PbrSceneShader", PbrScenePipelineCreateInfoPack);

    ScenePipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    ScenePipelineCreateInfoPack.DepthStencilStateCreateInfo.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
    PipelineManager->CreateGraphicsPipeline("SkyboxPipeline", "SkyboxShader", ScenePipelineCreateInfoPack);

    vk::PipelineRenderingCreateInfo PostRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(_VulkanContext->GetSwapchainCreateInfo().imageFormat);

    Grt::FGraphicsPipelineCreateInfoPack PostPipelineCreateInfoPack = ScenePipelineCreateInfoPack;

    PostPipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&PostRenderingCreateInfo);
    PostPipelineCreateInfoPack.DepthStencilStateCreateInfo = vk::PipelineDepthStencilStateCreateInfo();
    PostPipelineCreateInfoPack.RasterizationStateCreateInfo = vk::PipelineRasterizationStateCreateInfo();
    PostPipelineCreateInfoPack.MultisampleStateCreateInfo = vk::PipelineMultisampleStateCreateInfo();

    PipelineManager->CreateGraphicsPipeline("PostPipeline", "PostShader", PostPipelineCreateInfoPack);

    vk::PipelineRenderingCreateInfo ShadowMapRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setDepthAttachmentFormat(vk::Format::eD32Sfloat);

    Grt::FGraphicsPipelineCreateInfoPack ShadowMapPipelineCreateInfoPack = ScenePipelineCreateInfoPack;
    ShadowMapPipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&ShadowMapRenderingCreateInfo);
    ShadowMapPipelineCreateInfoPack.RasterizationStateCreateInfo = vk::PipelineRasterizationStateCreateInfo();
    ShadowMapPipelineCreateInfoPack.MultisampleStateCreateInfo = vk::PipelineMultisampleStateCreateInfo();

    PipelineManager->CreateGraphicsPipeline("ShadowMapPipeline", "ShadowMapShader", ShadowMapPipelineCreateInfoPack);
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

    _VulkanContext->AddInstanceExtension(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
    _VulkanContext->AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    _VulkanContext->AddDeviceExtension(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
    _VulkanContext->AddDeviceExtension(VK_EXT_HDR_METADATA_EXTENSION_NAME);

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

    _FreeCamera = std::make_unique<SysSpa::FCamera>(glm::vec3(0.0f, 0.0f, 3.0f));

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
    if (glfwGetKey(_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(_Window, GLFW_TRUE);
    }

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

    if (glfwGetKey(_Window, GLFW_KEY_W) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kForward, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_S) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kBack, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_A) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kLeft, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_D) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRight, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_R) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kUp, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_F) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kDown, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_Q) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRollLeft, _DeltaTime);
    if (glfwGetKey(_Window, GLFW_KEY_E) == GLFW_PRESS)
        _FreeCamera->ProcessKeyboard(SysSpa::FCamera::EMovement::kRollRight, _DeltaTime);
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
                HdrMetadata
                    .setDisplayPrimaryRed({ Desc1.RedPrimary[0], Desc1.RedPrimary[1] })
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
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(Window));

    if (App->_bFirstMouse)
    {
        App->_LastX       = PosX;
        App->_LastY       = PosY;
        App->_bFirstMouse = false;
    }

    double OffsetX = PosX - App->_LastX;
    double OffsetY = App->_LastY - PosY;
    App->_LastX = PosX;
    App->_LastY = PosY;

    App->_FreeCamera->ProcessMouseMovement(OffsetX, OffsetY);
}

void FApplication::ScrollCallback(GLFWwindow* Window, double OffsetX, double OffsetY)
{
    auto* App = static_cast<FApplication*>(glfwGetWindowUserPointer(Window));
    App->_FreeCamera->ProcessMouseScroll(OffsetY);
}

_NPGS_END
