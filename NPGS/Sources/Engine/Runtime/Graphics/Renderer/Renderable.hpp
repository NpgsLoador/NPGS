#pragma once

#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"
#include "Engine/Runtime/Managers/AssetManager.hpp"
// #include "Engine/Runtime/Managers/PipelineManager.hpp"
#include "Engine/Runtime/Managers/ShaderBufferManager.hpp"

namespace Npgs
{
    class IRenderable
    {
    public:
        virtual ~IRenderable() = default;

        virtual void LoadAssets(FAssetManager* Manager)                = 0;
        virtual void CreateGpuResources(FShaderBufferManager* Manager) = 0;
        virtual void Render(const FVulkanCommandBuffer& CommandBuffer) = 0;
    };
} // namespace Npgs
