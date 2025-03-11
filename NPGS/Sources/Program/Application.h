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
    void BindDescriptorSets(Runtime::Asset::FAssetManager* AssetManager);
    void InitInstanceData();
    void InitVerticesData();
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

    std::unique_ptr<System::Spatial::FCamera> _FreeCamera;
    double  _DeltaTime{};
    double  _LastX{};
    double  _LastY{};
    bool    _bFirstMouse{ true };

    vk::RenderingAttachmentInfo _ColorAttachmentInfo;
    vk::RenderingAttachmentInfo _ResolveAttachmentInfo;
    vk::RenderingAttachmentInfo _DepthStencilAttachmentInfo;
    vk::RenderingAttachmentInfo _ShadowMapAttachmentInfo;
    vk::RenderingAttachmentInfo _PostProcessAttachmentInfo;

    std::unique_ptr<Runtime::Graphics::FColorAttachment>        _ColorAttachment;
    std::unique_ptr<Runtime::Graphics::FColorAttachment>        _ResolveAttachment;
    std::unique_ptr<Runtime::Graphics::FDepthStencilAttachment> _DepthStencilAttachment;
    std::unique_ptr<Runtime::Graphics::FDepthStencilAttachment> _ShadowMapAttachment;

    std::vector<Runtime::Graphics::FInstanceData> _InstanceData;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _InstanceBuffer;

    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _SphereVertexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _SphereIndexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _CubeVertexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _SkyboxVertexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _PlaneVertexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _QuadVertexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _TerrainVertexBuffer;
    std::unique_ptr<Runtime::Graphics::FDeviceLocalBuffer> _TerrainIndexBuffer;

    std::uint32_t _SphereIndicesCount{};
    std::uint32_t _TerrainIndicesCount{};
    std::uint32_t _NumStrips{};
    std::uint32_t _NumVertsPerStrip{};
};

_NPGS_END
