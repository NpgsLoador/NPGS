#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    template <typename AssetType>
    requires CAssetCompatible<AssetType>
    inline void FAssetManager::AddAsset(const std::string& Name, AssetType&& Asset)
    {
        Assets_.emplace(Name, FManagedAsset(
            static_cast<void*>(new AssetType(std::move(Asset))),
            FTypeErasedDeleter(static_cast<AssetType*>(nullptr))
        ));
    }

    template <typename AssetType, typename... Types>
    requires CAssetCompatible<AssetType>
    inline void FAssetManager::AddAsset(const std::string& Name, Types&&... Args)
    {
        Assets_.emplace(Name, FManagedAsset(
            static_cast<void*>(new AssetType(VulkanContext_, std::forward<Types>(Args)...)),
            FTypeErasedDeleter(static_cast<AssetType*>(nullptr))
        ));
    }

    template <typename AssetType>
    requires CAssetCompatible<AssetType>
    inline AssetType* FAssetManager::GetAsset(const std::string& Name)
    {
        if (auto it = Assets_.find(Name); it != Assets_.end())
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
        for (const auto& [Name, Asset] : Assets_)
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
        Assets_.erase(Name);
    }

    NPGS_INLINE void FAssetManager::ClearAssets()
    {
        Assets_.clear();
    }

} // namespace Npgs
