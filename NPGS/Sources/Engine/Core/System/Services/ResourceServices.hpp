#pragma once

#include <memory>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Core/Runtime/Managers/ImageTracker.hpp"
#include "Engine/Core/Runtime/Managers/PipelineManager.hpp"
#include "Engine/Core/Runtime/Managers/ShaderBufferManager.hpp"
#include "Engine/Core/Runtime/Pools/StagingBufferPool.hpp"
#include "Engine/Core/System/Services/CoreServices.hpp"

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

        FImageTracker* GetImageTracker() const;
        FPipelineManager* GetPipelineManager() const;
        FShaderBufferManager* GetShaderBufferManager() const;

    private:
        const FCoreServices*                  CoreServices_;
        std::unique_ptr<FImageTracker>        ImageTracker_;
        std::unique_ptr<FPipelineManager>     PipelineManager_;
        std::unique_ptr<FShaderBufferManager> ShaderBufferManager_;
    };
} // namespace Npgs

#include "ResourceServices.inl"
