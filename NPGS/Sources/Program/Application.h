#pragma once

#include <memory>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/Graphics/Buffers/BufferStructs.h"
#include "Engine/Core/Runtime/Graphics/Resources/Attachment.h"
#include "Engine/Core/Runtime/Graphics/Resources/DeviceLocalBuffer.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"
#include "Engine/Core/Runtime/Threads/ThreadPool.h"
#include "Engine/Core/System/Spatial/Camera.h"

namespace Npgs
{
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
        void BindDescriptors();
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
        FVulkanContext* VulkanContext_;
        FThreadPool*    ThreadPool_;

        GLFWwindow*  Window_{ nullptr };
        std::string  WindowTitle_;
        vk::Extent2D WindowSize_;
        bool         bEnableVSync_;
        bool         bEnableFullscreen_;
        bool         bEnableHdr_;

        std::unique_ptr<FCamera> Camera_;
        double  DeltaTime_{};
        double  LastX_{};
        double  LastY_{};
        bool    bFirstMouse_{ true };

        vk::RenderingAttachmentInfo PositionAoAttachmentInfo_;
        vk::RenderingAttachmentInfo NormalRoughAttachmentInfo_;
        vk::RenderingAttachmentInfo AlbedoMetalAttachmentInfo_;
        vk::RenderingAttachmentInfo ShadowAttachmentInfo_;
        vk::RenderingAttachmentInfo ColorAttachmentInfo_;
        vk::RenderingAttachmentInfo ResolveAttachmentInfo_;
        vk::RenderingAttachmentInfo DepthStencilAttachmentInfo_;
        vk::RenderingAttachmentInfo DepthMapAttachmentInfo_;
        vk::RenderingAttachmentInfo PostProcessAttachmentInfo_;

        std::unique_ptr<FColorAttachment>        PositionAoAttachment_;
        std::unique_ptr<FColorAttachment>        NormalRoughAttachment_;
        std::unique_ptr<FColorAttachment>        AlbedoMetalAttachment_;
        std::unique_ptr<FColorAttachment>        ShadowAttachment_;
        std::unique_ptr<FColorAttachment>        ColorAttachment_;
        std::unique_ptr<FColorAttachment>        ResolveAttachment_;
        std::unique_ptr<FDepthStencilAttachment> DepthStencilAttachment_;
        std::unique_ptr<FDepthStencilAttachment> DepthMapAttachment_;

        std::vector<FInstanceData>          InstanceData_;
        std::unique_ptr<FDeviceLocalBuffer> InstanceBuffer_;

        std::unique_ptr<FDeviceLocalBuffer> SphereVertexBuffer_;
        std::unique_ptr<FDeviceLocalBuffer> SphereIndexBuffer_;
        std::unique_ptr<FDeviceLocalBuffer> CubeVertexBuffer_;
        std::unique_ptr<FDeviceLocalBuffer> SkyboxVertexBuffer_;
        std::unique_ptr<FDeviceLocalBuffer> PlaneVertexBuffer_;
        std::unique_ptr<FDeviceLocalBuffer> QuadVertexBuffer_;
        std::unique_ptr<FDeviceLocalBuffer> TerrainVertexBuffer_;

        std::uint32_t SphereIndicesCount_{};
        std::uint32_t TessResolution_{ 4 };
    };
} // namespace Npgs
