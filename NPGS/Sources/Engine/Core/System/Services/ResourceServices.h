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
        struct FStagingBufferPoolCreateInfo
        {
            FVulkanContext* VulkanContext{ nullptr };
            VmaAllocator    Allocator{ nullptr };
            std::uint32_t   MinAvailableBufferLimit{};
            std::uint32_t   MaxAllocatedBufferLimit{};
            std::uint32_t   BufferReclaimThresholdMs{};
            std::uint32_t   MaintenanceIntervalMs{};
            FStagingBufferPool::EPoolUsage PoolUsage{ FStagingBufferPool::EPoolUsage::kSubmit };
            bool            bUsingVma{ true };
        };

        struct FResourceServicesEnableInfo
        {
            FStagingBufferPoolCreateInfo*  SubmitStagingBufferPoolCreateInfo{ nullptr };
            FStagingBufferPoolCreateInfo*  FetchStagingBufferPoolCreateInfo{ nullptr };
        };

    public:
        FResourceServices(const FCoreServices& CoreServices, const FResourceServicesEnableInfo& EnableInfo);
        FResourceServices(const FResourceServices&) = delete;
        FResourceServices(FResourceServices&&)      = delete;
        ~FResourceServices()                        = default;

        FResourceServices& operator=(const FResourceServices&) = delete;
        FResourceServices& operator=(FResourceServices&&)      = delete;

        FImageTracker* GetImageTracker() const;
        FPipelineManager* GetPipelineManager() const;
        FShaderBufferManager* GetShaderBufferManager() const;
        FStagingBufferPool* GetStagingBufferPool(FStagingBufferPool::EPoolUsage PoolUsage) const;

    private:
        const FCoreServices*                               _CoreServices;
        std::unique_ptr<FImageTracker>                     _ImageTracker;
        std::unique_ptr<FPipelineManager>                  _PipelineManager;
        std::unique_ptr<FShaderBufferManager>              _ShaderBufferManager;
        std::array<std::unique_ptr<FStagingBufferPool>, 2> _StagingBufferPools;
    };
} // namespace Npgs

#include "ResourceServices.inl"
