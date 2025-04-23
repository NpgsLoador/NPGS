#include "ResourceServices.h"

namespace Npgs
{
    FResourceServices::FResourceServices(const FCoreServices& CoreServices, const FResourceServicesEnableInfo& EnableInfo)
        :
        _CoreServices(&CoreServices),
        _ImageTracker(std::make_unique<FImageTracker>()),
        _PipelineManager(std::make_unique<FPipelineManager>(_CoreServices->GetVulkanContext(), _CoreServices->GetAssetManager())),
        _ShaderBufferManager(std::make_unique<FShaderBufferManager>(_CoreServices->GetVulkanContext())),

        _CommandPoolManager(std::make_unique<FCommandPoolManager>(
            EnableInfo.CommandPoolManagerCreateInfo->MinAvailablePoolLimit,
            EnableInfo.CommandPoolManagerCreateInfo->MaxAllocatedPoolLimit,
            EnableInfo.CommandPoolManagerCreateInfo->PoolReclaimThresholdMs,
            EnableInfo.CommandPoolManagerCreateInfo->MaintenanceIntervalMs,
            EnableInfo.CommandPoolManagerCreateInfo->Device,
            EnableInfo.CommandPoolManagerCreateInfo->QueueFamilyIndex)
        ),

        _StagingBufferPools
        {
            std::make_unique<FStagingBufferPool>(
                EnableInfo.SubmitStagingBufferPoolCreateInfo->VulkanContext,
                EnableInfo.SubmitStagingBufferPoolCreateInfo->Allocator,
                EnableInfo.SubmitStagingBufferPoolCreateInfo->MinAvailableBufferLimit,
                EnableInfo.SubmitStagingBufferPoolCreateInfo->MaxAllocatedBufferLimit,
                EnableInfo.SubmitStagingBufferPoolCreateInfo->BufferReclaimThresholdMs,
                EnableInfo.SubmitStagingBufferPoolCreateInfo->MaintenanceIntervalMs,
                EnableInfo.SubmitStagingBufferPoolCreateInfo->PoolUsage,
                EnableInfo.SubmitStagingBufferPoolCreateInfo->bUsingVma
            ),

            std::make_unique<FStagingBufferPool>(
                EnableInfo.FetchStagingBufferPoolCreateInfo->VulkanContext,
                EnableInfo.FetchStagingBufferPoolCreateInfo->Allocator,
                EnableInfo.FetchStagingBufferPoolCreateInfo->MinAvailableBufferLimit,
                EnableInfo.FetchStagingBufferPoolCreateInfo->MaxAllocatedBufferLimit,
                EnableInfo.FetchStagingBufferPoolCreateInfo->BufferReclaimThresholdMs,
                EnableInfo.FetchStagingBufferPoolCreateInfo->MaintenanceIntervalMs,
                EnableInfo.FetchStagingBufferPoolCreateInfo->PoolUsage,
                EnableInfo.FetchStagingBufferPoolCreateInfo->bUsingVma
            ),
        }
    {
    }
} // namespace Npgs
