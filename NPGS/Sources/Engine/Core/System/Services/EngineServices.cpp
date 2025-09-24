#include "stdafx.h"
#include "EngineServices.h"

namespace Npgs
{
    FEngineServices::FEngineServices()
        : CoreServices_(nullptr)
        , ResourceServices_(nullptr)
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

        CoreServices_ = std::make_unique<FCoreServices>(CoreServiceEnableInfo);
    }

    void FEngineServices::InitializeResourceServices()
    {
        ResourceServices_ = std::make_unique<FResourceServices>(*CoreServices_);
    }

    void FEngineServices::ShutdownCoreServices()
    {
        CoreServices_.reset();
    }

    void FEngineServices::ShutdownResourceServices()
    {
        ResourceServices_.reset();
    }

    FEngineServices* FEngineServices::GetInstance()
    {
        static FEngineServices kInstance;
        return &kInstance;
    }
} // namespace Npgs
