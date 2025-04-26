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
        _ResourceServices = std::make_unique<FResourceServices>(*_CoreServices);
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
