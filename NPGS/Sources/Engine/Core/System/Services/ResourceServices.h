#pragma once

#include <array>
#include <memory>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Runtime/Graphics/Resources/Managers/ImageTracker.h"
#include "Engine/Core/Runtime/Graphics/Resources/Managers/PipelineManager.h"
#include "Engine/Core/Runtime/Graphics/Resources/Managers/ShaderBufferManager.h"
#include "Engine/Core/Runtime/Graphics/Resources/Pools/StagingBufferPool.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/System/Services/CoreServices.h"

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
        const FCoreServices*                  _CoreServices;
        std::unique_ptr<FImageTracker>        _ImageTracker;
        std::unique_ptr<FPipelineManager>     _PipelineManager;
        std::unique_ptr<FShaderBufferManager> _ShaderBufferManager;
    };
} // namespace Npgs

#include "ResourceServices.inl"
