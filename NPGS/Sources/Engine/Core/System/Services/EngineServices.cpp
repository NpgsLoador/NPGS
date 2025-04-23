#include "EngineServices.h"

namespace Npgs
{
    FEngineServices::FEngineServices()
        : _CoreServices(nullptr)
        , _ResourceServices(nullptr)
    {
    }

    FEngineServices::~FEngineServices()
    {
        ShutdownResourceServices();
        ShutdownCoreServices();
    }

    void FEngineServices::InitializeCoreServices()
    {
        FCoreServices::FThreadPoolCreateInfo ThreadPoolCreateInfo
        {
            .MaxThreadCount     = 8,
            .bEnableHyperThread = false
        };

        FCoreServices::FCoreServicesEnableInfo CoreServiceEnableInfo
        {
            .ThreadPoolCreateInfo = &ThreadPoolCreateInfo
        };

        _CoreServices = std::make_unique<FCoreServices>(CoreServiceEnableInfo);
    }

    void FEngineServices::InitializeResourceServices()
    {
        FResourceServices::FCommandPoolManagerCreateInfo CommandPoolManagerCreateInfo
        {
            .Device                 = _CoreServices->GetVulkanContext()->GetDevice(),
            .MinAvailablePoolLimit  = 2,
            .MaxAllocatedPoolLimit  = 32,
            .PoolReclaimThresholdMs = 1000,
            .MaintenanceIntervalMs  = 5000,
            .QueueFamilyIndex       = _CoreServices->GetVulkanContext()->GetGraphicsQueueFamilyIndex()
        };

        FResourceServices::FStagingBufferPoolCreateInfo SubmitStagingBufferPoolCreateInfo
        {
            .VulkanContext            = _CoreServices->GetVulkanContext(),
            .Allocator                = _CoreServices->GetVulkanContext()->GetVmaAllocator(),
            .MinAvailableBufferLimit  = 4,
            .MaxAllocatedBufferLimit  = 64,
            .BufferReclaimThresholdMs = 1000,
            .MaintenanceIntervalMs    = 5000,
            .PoolUsage                = FStagingBufferPool::EPoolUsage::kSubmit,
            .bUsingVma                = true
        };

        FResourceServices::FStagingBufferPoolCreateInfo FetchStagingBufferPoolCreateInfo
        {
            .VulkanContext            = _CoreServices->GetVulkanContext(),
            .Allocator                = _CoreServices->GetVulkanContext()->GetVmaAllocator(),
            .MinAvailableBufferLimit  = 2,
            .MaxAllocatedBufferLimit  = 10,
            .BufferReclaimThresholdMs = 10000,
            .MaintenanceIntervalMs    = 600000,
            .PoolUsage                = FStagingBufferPool::EPoolUsage::kFetch,
            .bUsingVma                = true
        };

        FResourceServices::FResourceServicesEnableInfo ResourceServiceEnableInfo
        {
            .CommandPoolManagerCreateInfo      = &CommandPoolManagerCreateInfo,
            .SubmitStagingBufferPoolCreateInfo = &SubmitStagingBufferPoolCreateInfo,
            .FetchStagingBufferPoolCreateInfo  = &FetchStagingBufferPoolCreateInfo,
        };

        _ResourceServices = std::make_unique<FResourceServices>(*_CoreServices, ResourceServiceEnableInfo);
    }

    void FEngineServices::ShutdownCoreServices()
    {
        _CoreServices.reset();
    }

    void FEngineServices::ShutdownResourceServices()
    {
        _ResourceServices.reset();
    }

    FEngineServices* FEngineServices::GetInstance()
    {
        static FEngineServices kInstance;
        return &kInstance;
    }
} // namespace Npgs
