#pragma once

#include <memory>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_handles.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/Graphics/Buffers/BufferStructs.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Resources.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"
#include "Engine/Core/Runtime/Threads/ThreadPool.h"
#include "Engine/Core/System/Spatial/Camera.h"

_NPGS_BEGIN

class FApplication
{
public:
    FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle,
                 bool bEnableVSync, bool bEnableFullscreen, bool bEnableHdr);

    ~FApplication();

    void ExecuteMainRender();
    void CreateAttachments();
    void LoadAssets();
    void CreateUniformBuffers();
    void BindDescriptorSets();
    void InitializeInstanceData();
    void InitializeVerticesData();
    void CreatePipelines();
    void Terminate();

private:
    bool InitializeWindow();
    void InitializeInputCallbacks();
    void ShowTitleFps();
    void ProcessInput();

    vk::HdrMetadataEXT GetHdrMetadata();

    static void FramebufferSizeCallback(GLFWwindow* Window, int Width, int Height);
    static void CursorPosCallback(GLFWwindow* Window, double PosX, double PosY);
    static void ScrollCallback(GLFWwindow* Window, double OffsetX, double OffsetY);

private:
    Runtime::Graphics::FVulkanContext* _VulkanContext;
    Runtime::Thread::FThreadPool*      _ThreadPool;

    GLFWwindow*  _Window{ nullptr };
    std::string  _WindowTitle;
    vk::Extent2D _WindowSize;
    bool         _bEnableVSync;
    bool         _bEnableFullscreen;
    bool         _bEnableHdr;

    std::unique_ptr<System::Spatial::FCamera> _Camera;
    double  _DeltaTime{};
    double  _LastX{};
    double  _LastY{};
    bool    _bFirstMouse{ true };

    vk::RenderingAttachmentInfo _PositionAoAttachmentInfo;
    vk::RenderingAttachmentInfo _NormalRoughAttachmentInfo;
    vk::RenderingAttachmentInfo _AlbedoMetalAttachmentInfo;
    vk::RenderingAttachmentInfo _ShadowAttachmentInfo;
    vk::RenderingAttachmentInfo _ColorAttachmentInfo;
    vk::RenderingAttachmentInfo _ResolveAttachmentInfo;
    vk::RenderingAttachmentInfo _DepthStencilAttachmentInfo;
    vk::RenderingAttachmentInfo _DepthMapAttachmentInfo;
    vk::RenderingAttachmentInfo _PostProcessAttachmentInfo;

    std::unique_ptr<Runtime::Graphics::FColorAttachment>        _PositionAoAttachment;
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        _NormalRoughAttachment;
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        _AlbedoMetalAttachment;
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        _ShadowAttachment;
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        _ColorAttachment;
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        _ResolveAttachment;
    std::unique_ptr<Runtime::Graphics::FDepthStencilAttachment> _DepthStencilAttachment;
    std::unique_ptr<Runtime::Graphics::FDepthStencilAttachment> _DepthMapAttachment;

    std::vector<Runtime::Graphics::FInstanceData> _InstanceData;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _InstanceBuffer;

    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _SphereVertexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _SphereIndexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _CubeVertexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _SkyboxVertexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _PlaneVertexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _QuadVertexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _TerrainVertexBuffer;

    std::uint32_t _SphereIndicesCount{};
    std::uint32_t _TessResolution{ 4 };
};

_NPGS_END
