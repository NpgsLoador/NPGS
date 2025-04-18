#include "AssetManager.h"

#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    template <typename AssetType>
    requires CAssetCompatible<AssetType>
    inline void FAssetManager::AddAsset(const std::string& Name, AssetType&& Asset)
    {
        _Assets.emplace(Name, FManagedAsset(
            static_cast<void*>(new AssetType(std::move(Asset))),
            FTypeErasedDeleter(static_cast<AssetType*>(nullptr))
        ));
    }

    template <typename AssetType, typename... Args>
    requires CAssetCompatible<AssetType>
    inline void FAssetManager::AddAsset(const std::string& Name, Args&&... ConstructArgs)
    {
        _Assets.emplace(Name, FManagedAsset(
            static_cast<void*>(new AssetType(_VulkanContext, std::forward<Args>(ConstructArgs)...)),
            FTypeErasedDeleter(static_cast<AssetType*>(nullptr))
        ));
    }

    template <typename AssetType>
    requires CAssetCompatible<AssetType>
    inline AssetType* FAssetManager::GetAsset(const std::string& Name)
    {
        if (auto it = _Assets.find(Name); it != _Assets.end())
        {
            return static_cast<AssetType*>(it->second.get());
        }

        return nullptr;
    }

    template <typename AssetType>
    requires CAssetCompatible<AssetType>
    inline std::vector<AssetType*> FAssetManager::GetAssets()
    {
        std::vector<AssetType*> Result;
        for (const auto& [Name, Asset] : _Assets)
        {
            if (auto* AssetPtr = dynamic_cast<AssetType*>(Asset.get()))
            {
                Result.push_back(AssetPtr);
            }
        }

        return Result;
    }

    NPGS_INLINE void FAssetManager::RemoveAsset(const std::string& Name)
    {
        _Assets.erase(Name);
    }

    NPGS_INLINE void FAssetManager::ClearAssets()
    {
        _Assets.clear();
    }

} // namespace Npgs
