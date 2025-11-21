#pragma once

#include <cstdint>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Engine/Core/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Utils/Hash.hpp"

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
        FTypeErasedDeleter(OriginalType*);

        void operator()(void* Ptr) const;

    private:
        std::function<void(void*)> Deleter_;
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
        void AddAsset(std::string_view Name, AssetType&& Asset);

        template <typename AssetType, typename... Types>
        requires CAssetCompatible<AssetType>
        void AddAsset(std::string_view Name, Types&&... Args);

        template <typename AssetType>
        requires CAssetCompatible<AssetType>
        AssetType* GetAsset(std::string_view Name);

        template <typename AssetType>
        requires CAssetCompatible<AssetType>
        std::vector<AssetType*> GetAssets();

        void RemoveAsset(std::string_view Name);
        void ClearAssets();

    private:
        using FManagedAsset = std::unique_ptr<void, FTypeErasedDeleter>;
        using FAssetMap     = Utils::FStringHeteroHashTable<std::string, FManagedAsset>;

        FAssetMap       Assets_;
        FVulkanContext* VulkanContext_;
    };

} // namespace Npgs

#include "AssetManager.inl"
