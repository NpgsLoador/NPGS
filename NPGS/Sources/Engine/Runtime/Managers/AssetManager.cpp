#include "stdafx.h"
#include "AssetManager.hpp"

#include "Engine/Core/Base/Assert.hpp"

namespace Npgs
{
    std::string GetAssetFullPath(EAssetType Type, std::string_view Filename)
    {
        std::string RootFolderName = "Assets/";
#ifdef _RELEASE
        RootFolderName = std::string("../") + RootFolderName;
#endif // _RELEASE

        std::string AssetFolderName;
        switch (Type)
        {
        case EAssetType::kBinaryPipeline:
            AssetFolderName = "Cache/";
            break;
        case EAssetType::kDataTable:
            AssetFolderName = "DataTables/";
            break;
        case EAssetType::kFont:
            AssetFolderName = "Fonts/";
            break;
        case EAssetType::kModel:
            AssetFolderName = "Models/";
            break;
        case EAssetType::kShader:
            AssetFolderName = "Shaders/";
            break;
        case EAssetType::kTexture:
            AssetFolderName = "Textures/";
            break;
        default:
            NpgsAssert(false, "Invalid asset type");
        };

        return RootFolderName + AssetFolderName + std::string(Filename);
    }

    FAssetEntry::FAssetEntry()
        : Payload(nullptr, FTypeErasedDeleter())
        , RefCount(nullptr)
        , bIsEvictable(std::make_unique<std::atomic<bool>>(false))
    {
    }

    FAssetManager::FAssetManager(FVulkanContext* VulkanContext)
        : VulkanContext_(VulkanContext)
        , LivenessToken_(std::make_shared<bool>())
    {
    }

    void FAssetManager::PinAsset(std::string_view Name)
    {
        auto it = Assets_.find(Name);
        if (it == Assets_.end())
        {
            return;
        }

        it->second->RefCount->fetch_add(1, std::memory_order::relaxed);
    }

    void FAssetManager::UnpinAsset(std::string_view Name)
    {
        auto it = Assets_.find(Name);
        if (it == Assets_.end())
        {
            return;
        }

        it->second->RefCount->fetch_sub(1, std::memory_order::relaxed);
    }

    void FAssetManager::RequestRemoveAsset(std::string_view Name)
    {
        std::shared_lock Lock(SharedMutex_);
        auto it = Assets_.find(Name);
        if (it != Assets_.end())
        {
            it->second->bIsEvictable->store(true, std::memory_order::relaxed);
        }
    }

    void FAssetManager::CollectGarbage()
    {
        std::unique_lock Lock(Mutex_);

        for (auto it = Assets_.begin(); it != Assets_.end();)
        {
            if (it->second->RefCount->load(std::memory_order::relaxed) == 0)
            {
                it = Assets_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
} // namespace Npgs
