#pragma once

#include <memory>

#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"
#include "Engine/Core/Runtime/Threads/ThreadPool.h"

namespace Npgs
{
    class FCoreServices
    {
    public:
        struct FThreadPoolCreateInfo
        {
            int MaxThreadCount{};
            bool bEnableHyperThread{ false };
        };

        struct FCoreServicesEnableInfo
        {
            FThreadPoolCreateInfo* ThreadPoolCreateInfo{ nullptr };
        };

    public:
        FCoreServices(const FCoreServicesEnableInfo& EnableInfo);
        FCoreServices(const FCoreServices&) = delete;
        FCoreServices(FCoreServices&&)      = delete;
        ~FCoreServices()                    = default;

        FCoreServices& operator=(const FCoreServices&) = delete;
        FCoreServices& operator=(FCoreServices&&)      = delete;

        FVulkanContext* GetVulkanContext() const;
        FAssetManager* GetAssetManager() const;
        FThreadPool* GetThreadPool() const;

    private:
        std::unique_ptr<FVulkanContext> VulkanContext_;
        std::unique_ptr<FAssetManager>  AssetManager_;
        std::unique_ptr<FThreadPool>    ThreadPool_;
    };
} // namespace Npgs

#include "CoreServices.inl"
