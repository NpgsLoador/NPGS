#pragma once

#include <cstdint>
#include <concepts>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Engine/Core/Runtime/Graphics/Vulkan/Context.h"

namespace Npgs
{
    enum class EAssetType : std::uint8_t
    {
        kBinaryShader, // (deprecated)
        kDataTable,    // 数据表
        kFont,         // 字体
        kModel,        // 模型
        kShader,       // 着色器
        kTexture       // 纹理
    };

    std::string GetAssetFullPath(EAssetType Type, const std::string& Filename);

    class FTypeErasedDeleter
    {
    public:
        template <typename OriginalType>
        requires std::is_class_v<OriginalType>
        FTypeErasedDeleter(OriginalType*)
            : _Deleter([](void* Ptr) -> void { delete static_cast<OriginalType*>(Ptr); })
        {
        }

        void operator()(void* Ptr) const
        {
            _Deleter(Ptr);
        }

    private:
        void (*_Deleter)(void*);
    };

    template <typename AssetType>
    concept CAssetCompatible = std::is_class_v<AssetType> && std::movable<AssetType>;

    class FAssetManager
    {
    public:
        FAssetManager(FVulkanContext* VulkanContext);
        FAssetManager(const FAssetManager&) = delete;
        FAssetManager(FAssetManager&&)      = delete;
        ~FAssetManager();

        FAssetManager& operator=(const FAssetManager&) = delete;
        FAssetManager& operator=(FAssetManager&&)      = delete;

        template <typename AssetType>
        requires CAssetCompatible<AssetType>
        void AddAsset(const std::string& Name, AssetType&& Asset);

        template <typename AssetType, typename... Args>
        requires CAssetCompatible<AssetType>
        void AddAsset(const std::string& Name, Args&&... ConstructArgs);

        template <typename AssetType>
        requires CAssetCompatible<AssetType>
        AssetType* GetAsset(const std::string& Name);

        template <typename AssetType>
        requires CAssetCompatible<AssetType>
        std::vector<AssetType*> GetAssets();

        void RemoveAsset(const std::string& Name);
        void ClearAssets();

    private:
        using FManagedAsset = std::unique_ptr<void, FTypeErasedDeleter>;

        std::unordered_map<std::string, FManagedAsset> _Assets;
        FVulkanContext*                                _VulkanContext;
    };

} // namespace Npgs

#include "AssetManager.inl"
