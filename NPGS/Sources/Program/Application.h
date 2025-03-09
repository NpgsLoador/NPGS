#pragma once

#include <memory>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Resources.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

#define GLM_FORCE_ALIGNED_GENTYPES
#include "Engine/Core/System/Spatial/Camera.h"

_NPGS_BEGIN

class FApplication
{
private:
    struct FRenderer
    {
        std::vector<Runtime::Graphics::FVulkanFramebuffer> Framebuffers;
        std::unique_ptr<Runtime::Graphics::FVulkanRenderPass> RenderPass;
    };

public:
    FApplication(const vk::Extent2D& WindowSize, const std::string& WindowTitle,
                 bool bEnableVSync, bool bEnableFullscreen, bool bEnableHdr);

    ~FApplication();

    void ExecuteMainRender();
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
    Runtime::Graphics::FVulkanContext*        _VulkanContext;

    GLFWwindow*                               _Window{ nullptr };
    std::string                               _WindowTitle;
    vk::Extent2D                              _WindowSize;
    bool                                      _bEnableVSync;
    bool                                      _bEnableFullscreen;
    bool                                      _bEnableHdr;

    std::unique_ptr<System::Spatial::FCamera> _FreeCamera;
    double                                    _DeltaTime{};
    double                                    _LastX{};
    double                                    _LastY{};
    bool                                      _bFirstMouse{ true };
};

_NPGS_END
