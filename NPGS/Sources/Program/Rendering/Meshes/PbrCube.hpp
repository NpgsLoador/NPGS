#pragma once

#include "Engine/Runtime/Graphics/Renderer/Renderable.hpp"

namespace Npgs
{
    class APbrCube : public IRenderable
    {
    public:
        void LoadAssets(FAssetManager* Manager) override;
        void CreateGpuResources(FShaderBufferManager* Manager) override;
        void Render(const FVulkanCommandBuffer& CommandBuffer) override;
    };
} // namespace Npgs
