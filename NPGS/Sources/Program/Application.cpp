#include "Application.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_aligned.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/Core/Base/Config/EngineConfig.h"
#include "Engine/Core/Math/NumericConstants.h"
#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/AssetLoaders/Shader.h"
#include "Engine/Core/Runtime/AssetLoaders/Texture.h"
#include "Engine/Core/Runtime/Graphics/Renderers/PipelineManager.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/ShaderResourceManager.h"
#include "Engine/Utils/Logger.h"
#include "DataStructures.h"
#include "Vertices.inc"

_NPGS_BEGIN

namespace Art    = Runtime::Asset;
namespace Grt    = Runtime::Graphics;
namespace SysSpa = System::Spatial;

FApplication::FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle,
                           bool bEnableVSync, bool bEnableFullscreen)
    :
    _VulkanContext(Grt::FVulkanContext::GetClassInstance()),
    _WindowTitle(WindowTitle),
    _WindowSize(WindowSize),
    _bEnableVSync(bEnableVSync),
    _bEnableFullscreen(bEnableFullscreen)
{
    if (!InitializeWindow())
    {
        NpgsCoreError("Failed to create application.");
    }
}

FApplication::~FApplication()
{
}

void FApplication::ExecuteMainRender()
{
    std::unique_ptr<Grt::FColorAttachment> ColorAttachment;
    std::unique_ptr<Grt::FColorAttachment> ResolveAttachment;
    std::unique_ptr<Grt::FDepthStencilAttachment> DepthStencilAttachment;
    std::unique_ptr<Grt::FDepthStencilAttachment> ShadowMapAttachment;

    vk::RenderingAttachmentInfo ColorAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setResolveMode(vk::ResolveModeFlagBits::eAverage)
        .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

    vk::RenderingAttachmentInfo DepthStencilAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

    vk::RenderingAttachmentInfo ShadowMapAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

    vk::RenderingAttachmentInfo PostProcessAttachmentInfo = vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));

    VmaAllocationCreateInfo AttachmentAllocationCreateInfo
    {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    vk::Extent2D ShadowMapExtent(4096, 4096);

    auto CreateFramebuffers = [&]() -> void
    {
        _VulkanContext->WaitIdle();
        auto SwapchainImageFormat = _VulkanContext->GetSwapchainCreateInfo().imageFormat;

        ColorAttachment = std::make_unique<Grt::FColorAttachment>(
            AttachmentAllocationCreateInfo, SwapchainImageFormat, _WindowSize, 1, vk::SampleCountFlagBits::e8);

        ResolveAttachment = std::make_unique<Grt::FColorAttachment>(
            AttachmentAllocationCreateInfo, SwapchainImageFormat, _WindowSize, 1,
            vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

        DepthStencilAttachment = std::make_unique<Grt::FDepthStencilAttachment>(
            AttachmentAllocationCreateInfo, vk::Format::eD32Sfloat, _WindowSize, 1, vk::SampleCountFlagBits::e8);

        ShadowMapAttachment = std::make_unique<Grt::FDepthStencilAttachment>(
            AttachmentAllocationCreateInfo, vk::Format::eD32Sfloat, ShadowMapExtent, 1,
            vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eSampled);

        ColorAttachmentInfo.setImageView(*ColorAttachment->GetImageView())
                           .setResolveImageView(*ResolveAttachment->GetImageView());
        DepthStencilAttachmentInfo.setImageView(*DepthStencilAttachment->GetImageView());
        ShadowMapAttachmentInfo.setImageView(*ShadowMapAttachment->GetImageView());
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

    // Create pipeline layout
    // ----------------------
    auto* AssetManager = Art::FAssetManager::GetInstance();

    Art::FShader::FResourceInfo SceneResourceInfo
    {
        {
            { 0, sizeof(FVertex), false },
            { 1, sizeof(FInstanceData), true }
        },
        {
            { 0, 0, offsetof(FVertex, Position) },
            { 0, 1, offsetof(FVertex, Normal) },
            { 0, 2, offsetof(FVertex, TexCoord) },
            { 1, 3, offsetof(FInstanceData, Model) }
        },
        {
            { 0, 0, false },
            { 0, 1, false }
        }
    };

    Art::FShader::FResourceInfo ShadowMapResourceInfo
    {
        {
            { 0, sizeof(FVertex), false },
            { 1, sizeof(FInstanceData), true }
        },
        {
            { 0, 0, offsetof(FVertex, Position) },
            { 1, 1, offsetof(FInstanceData, Model) }
        },
        { { 0, 0, false } },
    };

    Art::FShader::FResourceInfo SkyboxResourceInfo
    {
        { { 0, sizeof(FSkyboxVertex), false } },
        { { 0, 0, offsetof(FSkyboxVertex, Position) } },
        { { 0, 0, false } }
    };

    Art::FShader::FResourceInfo PostResourceInfo
    {
        { { 0, sizeof(FQuadVertex), false } },
        {
            { 0, 0, offsetof(FQuadVertex, Position) },
            { 0, 1, offsetof(FQuadVertex, TexCoord) }
        }
    };

    std::vector<std::string> SceneShaderFiles({ "Scene.vert.spv", "Scene.frag.spv" });
    std::vector<std::string> LampShaderFiles({ "Scene.vert.spv", "Scene_Lamp.frag.spv" });
    std::vector<std::string> ShadowMapShaderFiles({ "ShadowMap.vert.spv", "ShadowMap.frag.spv" });
    std::vector<std::string> SkyboxShaderFiles({ "Skybox.vert.spv", "Skybox.frag.spv" });
    std::vector<std::string> PostShaderFiles({ "PostProcess.vert.spv", "PostProcess.frag.spv" });

    VmaAllocationCreateInfo TextureAllocationCreateInfo
    {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    AssetManager->AddAsset<Art::FShader>("SceneShader", SceneShaderFiles, SceneResourceInfo);
    AssetManager->AddAsset<Art::FShader>("LampShader", LampShaderFiles, SceneResourceInfo);
    AssetManager->AddAsset<Art::FShader>("ShadowMapShader", ShadowMapShaderFiles, ShadowMapResourceInfo);
    AssetManager->AddAsset<Art::FShader>("SkyboxShader", SkyboxShaderFiles, SkyboxResourceInfo);
    AssetManager->AddAsset<Art::FShader>("PostShader", PostShaderFiles, PostResourceInfo);

    AssetManager->AddAsset<Art::FTexture2D>(
        "ContainerDiffuse", TextureAllocationCreateInfo, "BrickwallDiffuse.jpg",
        vk::Format::eR8G8B8A8Srgb, vk::Format::eR8G8B8A8Srgb, vk::ImageCreateFlagBits::eMutableFormat, true, false);

    AssetManager->AddAsset<Art::FTexture2D>(
        "ContainerNormal", TextureAllocationCreateInfo, "BrickwallNormal.jpg",
        vk::Format::eR8G8B8A8Srgb, vk::Format::eR8G8B8A8Srgb, vk::ImageCreateFlagBits::eMutableFormat, true, false);

    AssetManager->AddAsset<Art::FTexture2D>(
        "ContainerSpecular", TextureAllocationCreateInfo, "BrickwallDiffuse.jpg",
        vk::Format::eR8G8B8A8Srgb, vk::Format::eR8G8B8A8Srgb, vk::ImageCreateFlagBits::eMutableFormat, true, false);

    AssetManager->AddAsset<Art::FTextureCube>(
        "Skybox", TextureAllocationCreateInfo, "Skybox", vk::Format::eR8G8B8A8Srgb, vk::Format::eR8G8B8A8Srgb,
        vk::ImageCreateFlagBits::eMutableFormat, true, false);

    auto* SceneShader     = AssetManager->GetAsset<Art::FShader>("SceneShader");
    auto* LampShader      = AssetManager->GetAsset<Art::FShader>("LampShader");
    auto* ShadowMapShader = AssetManager->GetAsset<Art::FShader>("ShadowMapShader");
    auto* SkyboxShader    = AssetManager->GetAsset<Art::FShader>("SkyboxShader");
    auto* PostShader      = AssetManager->GetAsset<Art::FShader>("PostShader");

    auto* ContainerDiffuse  = AssetManager->GetAsset<Art::FTexture2D>("ContainerDiffuse");
    auto* ContainerNormal   = AssetManager->GetAsset<Art::FTexture2D>("ContainerNormal");
    auto* ContainerSpecular = AssetManager->GetAsset<Art::FTexture2D>("ContainerSpecular");
    auto* Skybox            = AssetManager->GetAsset<Art::FTextureCube>("Skybox");

    Grt::FShaderResourceManager::FUniformBufferCreateInfo MatricesCreateInfo
    {
        .Name    = "Matrices",
        .Fields  = { "View", "Projection", "LightSpaceMatrix" },
        .Set     = 0,
        .Binding = 0,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    Grt::FShaderResourceManager::FUniformBufferCreateInfo LightMaterialCreateInfo
    {
        .Name    = "LightMaterial",
        .Fields  = { "Material", "Light", "ViewPos" },
        .Set     = 0,
        .Binding = 1,
        .Usage   = vk::DescriptorType::eUniformBuffer
    };

    VmaAllocationCreateInfo UniformBufferAllocationCreateInfo
    {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    };

    auto ShaderResourceManager = Grt::FShaderResourceManager::GetInstance();
    ShaderResourceManager->CreateBuffers<FMatrices>(MatricesCreateInfo, &UniformBufferAllocationCreateInfo);
    ShaderResourceManager->CreateBuffers<FLightMaterial>(LightMaterialCreateInfo, &UniformBufferAllocationCreateInfo);

    // Bind descriptor sets
    // --------------------
    vk::SamplerCreateInfo SamplerCreateInfo = Art::FTextureBase::CreateDefaultSamplerCreateInfo();
    Grt::FVulkanSampler Sampler(SamplerCreateInfo);

    vk::DescriptorImageInfo SamplerInfo(*Sampler);
    SceneShader->WriteSharedDescriptors<vk::DescriptorImageInfo>(1, 0, vk::DescriptorType::eSampler, { SamplerInfo });

    std::vector<vk::DescriptorImageInfo> ImageInfos;
    ImageInfos.push_back(ContainerDiffuse->CreateDescriptorImageInfo(nullptr));
    SceneShader->WriteSharedDescriptors(1, 1, vk::DescriptorType::eSampledImage, ImageInfos);
    ImageInfos.clear();
    ImageInfos.push_back(ContainerNormal->CreateDescriptorImageInfo(nullptr));
    SceneShader->WriteSharedDescriptors(1, 2, vk::DescriptorType::eSampledImage, ImageInfos);
    ImageInfos.clear();
    ImageInfos.push_back(ContainerSpecular->CreateDescriptorImageInfo(nullptr));
    SceneShader->WriteSharedDescriptors(1, 3, vk::DescriptorType::eSampledImage, ImageInfos);

    SamplerCreateInfo
        .setMipmapMode(vk::SamplerMipmapMode::eNearest)
        .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
        .setAddressModeW(vk::SamplerAddressMode::eClampToEdge);

    Grt::FVulkanSampler SkyboxSampler(SamplerCreateInfo);

    ImageInfos.clear();
    ImageInfos.push_back(Skybox->CreateDescriptorImageInfo(SkyboxSampler));
    SkyboxShader->WriteSharedDescriptors(1, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

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

    Grt::FVulkanSampler FramebufferSampler(SamplerCreateInfo);

    auto CreatePostDescriptors = [&]() -> void
    {
        vk::DescriptorImageInfo ColorImageInfo(
            *FramebufferSampler, *ResolveAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo ShadowMapImageInfo(
            *FramebufferSampler, *ShadowMapAttachment->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

        ImageInfos.clear();
        ImageInfos.push_back(ColorImageInfo);
        ImageInfos.push_back(ShadowMapImageInfo);
        PostShader->WriteSharedDescriptors(0, 0, vk::DescriptorType::eCombinedImageSampler, ImageInfos);

        ImageInfos.clear();
        ImageInfos.push_back(ShadowMapImageInfo);
        SceneShader->WriteSharedDescriptors(1, 4, vk::DescriptorType::eCombinedImageSampler, ImageInfos);
    };

    CreatePostDescriptors();

    _VulkanContext->RegisterAutoRemovedCallbacks(
        Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "CreatePostDescriptor", CreatePostDescriptors);

    ShaderResourceManager->BindShadersToBuffers("Matrices", "SceneShader", "LampShader", "ShadowMapShader", "SkyboxShader");
    ShaderResourceManager->BindShaderToBuffers("LightMaterial", "SceneShader");

    // Init instance data
    // ------------------
    std::vector<FInstanceData> InstanceData;
    glm::mat4x4 Model(1.0f);
    // plane
    InstanceData.emplace_back(Model);

    // cube
    Model = glm::mat4x4(1.0f);
    Model = glm::translate(Model, glm::vec3(0.0f, 1.5f, 0.0f));
    InstanceData.emplace_back(Model);

    Model = glm::mat4x4(1.0f);
    Model = glm::translate(Model, glm::vec3(2.0f, 0.0f, 1.0f));
    InstanceData.emplace_back(Model);

    Model = glm::mat4x4(1.0f);
    Model = glm::translate(Model, glm::vec3(-1.0f, 0.0f, 2.0f));
    Model = glm::scale(Model, glm::vec3(0.5f));
    Model = glm::rotate(Model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
    InstanceData.emplace_back(Model);

    // lamp
    glm::vec3 LightPos(-2.0f, 4.0f, -1.0f);
    Model = glm::mat4x4(1.0f);
    Model = glm::scale(glm::translate(Model, LightPos), glm::vec3(0.2f));
    InstanceData.emplace_back(Model);

    // Create vertex buffers
    // ---------------------
    VmaAllocationCreateInfo VertexBufferAllocationCreateInfo
    {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    vk::BufferCreateInfo VertexBufferCreateInfo = vk::BufferCreateInfo()
        .setSize(CubeVertices.size() * sizeof(FVertex))
        .setUsage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

    Grt::FDeviceLocalBuffer CubeVertexBuffer(VertexBufferAllocationCreateInfo, VertexBufferCreateInfo);
    CubeVertexBuffer.CopyData(CubeVertices);

    VertexBufferCreateInfo.setSize(SkyboxVertices.size() * sizeof(FVertex));
    Grt::FDeviceLocalBuffer SkyboxVertexBuffer(VertexBufferAllocationCreateInfo, VertexBufferCreateInfo);
    SkyboxVertexBuffer.CopyData(SkyboxVertices);

    VertexBufferCreateInfo.setSize(QuadVertices.size() * sizeof(FQuadVertex));
    Grt::FDeviceLocalBuffer QuadVertexBuffer(VertexBufferAllocationCreateInfo, VertexBufferCreateInfo);
    QuadVertexBuffer.CopyData(QuadVertices);

    VertexBufferCreateInfo.setSize(PlaneVertices.size() * sizeof(FVertex));
    Grt::FDeviceLocalBuffer PlaneVertexBuffer(VertexBufferAllocationCreateInfo, VertexBufferCreateInfo);
    PlaneVertexBuffer.CopyData(PlaneVertices);

    VertexBufferCreateInfo.setSize(InstanceData.size() * sizeof(FInstanceData));
    Grt::FDeviceLocalBuffer InstanceBuffer(VertexBufferAllocationCreateInfo, VertexBufferCreateInfo);
    InstanceBuffer.CopyData(InstanceData);

    // Create graphics pipeline
    // ------------------------
    auto* PipelineManager = Grt::FPipelineManager::GetInstance();

    vk::PipelineColorBlendAttachmentState ColorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
        .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineRenderingCreateInfo SceneRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(_VulkanContext->GetSwapchainCreateInfo().imageFormat)
        .setDepthAttachmentFormat(vk::Format::eD32Sfloat);

    Grt::FGraphicsPipelineCreateInfoPack ScenePipelineCreateInfoPack;
    ScenePipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eViewport);
    ScenePipelineCreateInfoPack.DynamicStates.push_back(vk::DynamicState::eScissor);
    ScenePipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&SceneRenderingCreateInfo);
    ScenePipelineCreateInfoPack.InputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);

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

    ScenePipelineCreateInfoPack.DepthStencilStateCreateInfo.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
    PipelineManager->CreateGraphicsPipeline("SkyboxPipeline", "SkyboxShader", ScenePipelineCreateInfoPack);

    vk::PipelineRenderingCreateInfo PostRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(_VulkanContext->GetSwapchainCreateInfo().imageFormat);

    Grt::FGraphicsPipelineCreateInfoPack PostPipelineCreateInfoPack = ScenePipelineCreateInfoPack;

    PostPipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&PostRenderingCreateInfo);
    PostPipelineCreateInfoPack.DepthStencilStateCreateInfo = vk::PipelineDepthStencilStateCreateInfo();
    PostPipelineCreateInfoPack.MultisampleStateCreateInfo = vk::PipelineMultisampleStateCreateInfo();

    PipelineManager->CreateGraphicsPipeline("PostPipeline", "PostShader", PostPipelineCreateInfoPack);

    vk::PipelineRenderingCreateInfo ShadowMapRenderingCreateInfo = vk::PipelineRenderingCreateInfo()
        .setDepthAttachmentFormat(vk::Format::eD32Sfloat);

    Grt::FGraphicsPipelineCreateInfoPack ShadowMapPipelineCreateInfoPack = ScenePipelineCreateInfoPack;
    ShadowMapPipelineCreateInfoPack.GraphicsPipelineCreateInfo.setPNext(&ShadowMapRenderingCreateInfo);
    ShadowMapPipelineCreateInfoPack.MultisampleStateCreateInfo = vk::PipelineMultisampleStateCreateInfo();

    PipelineManager->CreateGraphicsPipeline("ShadowMapPipeline", "ShadowMapShader", ShadowMapPipelineCreateInfoPack);

    vk::Pipeline ScenePipeline;
    vk::Pipeline LampPipeline;
    vk::Pipeline ShadowMapPipeline;
    vk::Pipeline PostPipeline;
    vk::Pipeline SkyboxPipeline;

    auto GetPipelines = [&]() -> void
    {
        ScenePipeline     = PipelineManager->GetPipeline("ScenePipeline");
        LampPipeline      = PipelineManager->GetPipeline("LampPipeline");
        ShadowMapPipeline = PipelineManager->GetPipeline("ShadowMapPipeline");
        PostPipeline      = PipelineManager->GetPipeline("PostPipeline");
        SkyboxPipeline    = PipelineManager->GetPipeline("SkyboxPipeline");
    };

    GetPipelines();

    _VulkanContext->RegisterAutoRemovedCallbacks(
        Grt::FVulkanContext::ECallbackType::kCreateSwapchain, "GetPipelines", GetPipelines);

    auto ScenePipelineLayout     = PipelineManager->GetPipelineLayout("ScenePipeline");
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

    glm::mat4x4 LightProjection  = glm::infinitePerspective(glm::radians(60.0f), 1.0f, 1.0f);
    glm::mat4x4 LightView        = glm::lookAt(LightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4x4 LightSpaceMatrix = LightProjection * LightView;

    vk::ImageSubresourceRange ColorSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    vk::ImageSubresourceRange DepthSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1);

    std::array PlaneVertexBuffers{ *PlaneVertexBuffer.GetBuffer(), *InstanceBuffer.GetBuffer() };
    std::array CubeVertexBuffers{ *CubeVertexBuffer.GetBuffer(), *InstanceBuffer.GetBuffer() };
    std::array PlaneOffsets{ Offset, Offset };
    std::array CubeOffsets{ Offset, Offset + sizeof(glm::mat4x4) };
    std::array LampOffsets{ Offset, Offset + 4 * sizeof(glm::mat4x4) };

    while (!glfwWindowShouldClose(_Window))
    {
        while (glfwGetWindowAttrib(_Window, GLFW_ICONIFIED))
        {
            glfwWaitEvents();
        }

        //vk::Viewport Viewport(0.0f, static_cast<float>(_WindowSize.height), static_cast<float>(_WindowSize.width),
        //                      -static_cast<float>(_WindowSize.height), 0.0f, 1.0f);

        vk::Viewport Viewport(0.0f, 0.0f, static_cast<float>(_WindowSize.width),
                              static_cast<float>(_WindowSize.height), 0.0f, 1.0f);

        vk::Viewport ShadowMapViewport(0.0f, 0.0f, static_cast<float>(ShadowMapExtent.width),
                                       static_cast<float>(ShadowMapExtent.height), 0.0f, 1.0f);

        vk::Rect2D Scissor(vk::Offset2D(), _WindowSize);
        vk::Rect2D ShadowMapScissor(vk::Offset2D(), ShadowMapExtent);

        InFlightFences[CurrentFrame].WaitAndReset();

        // Uniform update
        // --------------
        float WindowAspect = static_cast<float>(_WindowSize.width) / static_cast<float>(_WindowSize.height);

        Matrices.View             = _FreeCamera->GetViewMatrix();
        Matrices.Projection       = _FreeCamera->GetProjectionMatrix(WindowAspect, 0.1f);
        Matrices.LightSpaceMatrix = LightSpaceMatrix;

        ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "Matrices", Matrices);

        LightMaterial.Material.Shininess = 64.0f;
        LightMaterial.Light.Position     = LightPos;
        LightMaterial.Light.Ambient      = glm::vec3(0.1f);
        LightMaterial.Light.Diffuse      = glm::vec3(1.0f);
        LightMaterial.Light.Specular     = glm::vec3(1.0f);
        LightMaterial.ViewPos            = _FreeCamera->GetCameraVector(SysSpa::FCamera::EVectorType::kPosition);

        ShaderResourceManager->UpdateEntrieBuffer(CurrentFrame, "LightMaterial", LightMaterial);

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
            *ResolveAttachment->GetImage(),
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
            *ShadowMapAttachment->GetImage(),
            DepthSubresourceRange
        );

        std::array InitBarriers{ InitSwapchainBarrier, InitColorAttachmentBarrier, InitDepthAttachmentBarrier };

        vk::DependencyInfo InitialDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(InitBarriers);

        CurrentBuffer->pipelineBarrier2(InitialDependencyInfo);

        CurrentBuffer->setViewport(0, ShadowMapViewport);
        CurrentBuffer->setScissor(0, ShadowMapScissor);

        vk::RenderingInfo ShadowMapRenderingInfo = vk::RenderingInfo()
            .setRenderArea(ShadowMapScissor)
            .setLayerCount(1)
            .setPDepthAttachment(&ShadowMapAttachmentInfo);

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
            *ShadowMapAttachment->GetImage(),
            DepthSubresourceRange
        );

        vk::DependencyInfo DepthRenderEndDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(DepthRenderEndBarrier);

        CurrentBuffer->pipelineBarrier2(DepthRenderEndDependencyInfo);

        CurrentBuffer->setViewport(0, Viewport);
        CurrentBuffer->setScissor(0, Scissor);

        vk::RenderingInfo SceneRenderingInfo = vk::RenderingInfo()
            .setRenderArea(Scissor)
            .setLayerCount(1)
            .setColorAttachments(ColorAttachmentInfo)
            .setPDepthAttachment(&DepthStencilAttachmentInfo);

        CurrentBuffer->beginRendering(SceneRenderingInfo);

        // Draw plane
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, ScenePipeline);
        CurrentBuffer->bindVertexBuffers(0, PlaneVertexBuffers, PlaneOffsets);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ScenePipelineLayout, 0,
                                          SceneShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(6, 1, 0, 0);

        // Draw cubes
        CurrentBuffer->bindVertexBuffers(0, CubeVertexBuffers, CubeOffsets);
        CurrentBuffer->draw(36, 3, 0, 0);

        // Draw lamp
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, LampPipeline);
        CurrentBuffer->bindVertexBuffers(0, CubeVertexBuffers, LampOffsets);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, LampPipelineLayout, 0,
                                          LampShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(36, 1, 0, 0);
        CurrentBuffer->endRendering();

        // Draw skybox
        ColorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);
        DepthStencilAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);

        CurrentBuffer->beginRendering(SceneRenderingInfo);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, SkyboxPipeline);
        CurrentBuffer->bindVertexBuffers(0, *SkyboxVertexBuffer.GetBuffer(), Offset);
        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, SkyboxPipelineLayout, 0,
                                          SkyboxShader->GetDescriptorSets(CurrentFrame), {});
        CurrentBuffer->draw(36, 1, 0, 0);
        CurrentBuffer->endRendering();

        ColorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);
        DepthStencilAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);

        vk::ImageMemoryBarrier2 ColorRenderEndBarrier(
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            *ResolveAttachment->GetImage(),
            ColorSubresourceRange
        );

        vk::DependencyInfo ColorRenderEndDependencyInfo = vk::DependencyInfo()
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
            .setImageMemoryBarriers(ColorRenderEndBarrier);

        CurrentBuffer->pipelineBarrier2(ColorRenderEndDependencyInfo);

        vk::RenderingInfo PostRenderingInfo = vk::RenderingInfo()
            .setRenderArea(Scissor)
            .setLayerCount(1)
            .setColorAttachments(PostProcessAttachmentInfo);

        PostProcessAttachmentInfo.setImageView(_VulkanContext->GetSwapchainImageView(ImageIndex));

        CurrentBuffer->beginRendering(PostRenderingInfo);
        CurrentBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, PostPipeline);
        CurrentBuffer->bindVertexBuffers(0, *QuadVertexBuffer.GetBuffer(), Offset);

        CurrentBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PostPipelineLayout, 0,
                                          PostShader->GetDescriptorSets(CurrentFrame), {});
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

    Terminate();
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

    _VulkanContext->AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    _VulkanContext->AddDeviceExtension(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);

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

    if (_VulkanContext->CreateDevice(0) != vk::Result::eSuccess ||
        _VulkanContext->CreateSwapchain(_WindowSize, _bEnableVSync) != vk::Result::eSuccess)
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
