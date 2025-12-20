#pragma once

#include <memory>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Managers/ImageTracker.hpp"
#include "Engine/Runtime/Managers/RenderTargetManager.hpp"
#include "Engine/Runtime/Managers/PipelineManager.hpp"
#include "Engine/Runtime/Managers/ShaderBufferManager.hpp"
#include "Engine/Runtime/Managers/ShaderManager.hpp"
#include "Engine/Runtime/Pools/StagingBufferPool.hpp"
#include "Engine/System/Services/CoreServices.hpp"

namespace Npgs
{
    class FResourceServices
    {
    public:
        FResourceServices(const FCoreServices& CoreServices);
        FResourceServices(const FResourceServices&) = delete;
        FResourceServices(FResourceServices&&)      = delete;
        ~FResourceServices()                        = default;

        FResourceServices& operator=(const FResourceServices&) = delete;
        FResourceServices& operator=(FResourceServices&&)      = delete;

        FImageTracker*        GetImageTracker() const;
        FPipelineManager*     GetPipelineManager() const;
        FRenderTargetManager* GetRenderTargetManager() const;
        FShaderBufferManager* GetShaderBufferManager() const;
        FShaderManager*       GetShaderManager() const;

    private:
        const FCoreServices*                  CoreServices_;
        std::unique_ptr<FImageTracker>        ImageTracker_;
        std::unique_ptr<FPipelineManager>     PipelineManager_;
        std::unique_ptr<FRenderTargetManager> RenderTargetManager_;
        std::unique_ptr<FShaderBufferManager> ShaderBufferManager_;
        std::unique_ptr<FShaderManager>       ShaderManager_;
    };
} // namespace Npgs

#include "ResourceServices.inl"
