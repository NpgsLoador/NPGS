#pragma once

#include <memory>

#include "Engine/Core/System/Services/CoreServices.h"
#include "Engine/Core/System/Services/ResourceServices.h"

namespace Npgs
{
    class FEngineServices
    {
    public:
        void InitializeCoreServices();
        void InitializeResourceServices();
        void ShutdownCoreServices();
        void ShutdownResourceServices();

        FCoreServices* GetCoreServices() const;
        FResourceServices* GetResourceServices() const;
        
        static FEngineServices* GetInstance();

    private:
        FEngineServices();
        FEngineServices(const FEngineServices&) = delete;
        FEngineServices(FEngineServices&&)      = delete;
        ~FEngineServices();

        FEngineServices& operator=(const FEngineServices&) = delete;
        FEngineServices& operator=(FEngineServices&&)      = delete;
    
    private:
        std::unique_ptr<FCoreServices>     _CoreServices;
        std::unique_ptr<FResourceServices> _ResourceServices;
    };
} // namespace Npgs

#define EngineServicesGetCoreServices     ::Npgs::FEngineServices::GetInstance()->GetCoreServices()
#define EngineServicesGetResourceServices ::Npgs::FEngineServices::GetInstance()->GetResourceServices()

#include "EngineServices.inl"
