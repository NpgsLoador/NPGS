#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Engine/Core/Utils/Hash.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"

namespace Npgs
{
    enum class EAssetType : std::uint8_t
    {
        kBinaryPipeline, // PSO Cache
        kDataTable,      // 数据表
        kFont,           // 字体
        kModel,          // 模型
        kShader,         // 着色器
        kTexture         // 纹理
    };

    std::string GetAssetFullPath(EAssetType Type, const std::string& Filename);

    class FTypeErasedDeleter
    {
    public:
        FTypeErasedDeleter() = default;

        template <typename OriginalType>
        requires std::is_class_v<OriginalType>
        FTypeErasedDeleter(OriginalType*);

        void operator()(void* Ptr) const;

    private:
        using FTypeDeleter = void(*)(void*);
        FTypeDeleter Deleter_{ nullptr };
    };

    struct FAssetEntry
    {
        std::unique_ptr<void, FTypeErasedDeleter> Payload;
        std::unique_ptr<std::atomic<std::size_t>> RefCount;

        FAssetEntry();
        FAssetEntry(const FAssetEntry&)     = delete;
        FAssetEntry(FAssetEntry&&) noexcept = default;
        ~FAssetEntry()                      = default;

        FAssetEntry& operator=(const FAssetEntry&)     = delete;
        FAssetEntry& operator=(FAssetEntry&&) noexcept = default;
    };

    template <typename AssetType>
    concept CAssetCompatible = std::is_class_v<AssetType> && std::movable<AssetType>;

    class FAssetManager;
    template <CAssetCompatible AssetType>
    class TAssetHandle
    {
    public:
        TAssetHandle() = default;
        TAssetHandle(FAssetManager* Manager, FAssetEntry* Asset);
        TAssetHandle(const TAssetHandle& Other);
        TAssetHandle(TAssetHandle&& Other) noexcept;
        ~TAssetHandle();

        TAssetHandle& operator=(const TAssetHandle& Other);
        TAssetHandle& operator=(TAssetHandle&& Other) noexcept;

        AssetType* operator->();
        const AssetType* operator->() const;
        AssetType& operator*();
        const AssetType& operator*() const;
        explicit operator bool() const;

        AssetType* Get();
        const AssetType* Get() const;

    private:
        FAssetManager*      Manager_{ nullptr };
        FAssetEntry*        Asset_{ nullptr };
        std::weak_ptr<bool> ManagerLiveness_;
    };

    class FAssetManager
    {
    public:
        FAssetManager(FVulkanContext* VulkanContext);
        FAssetManager(const FAssetManager&) = delete;
        FAssetManager(FAssetManager&&)      = delete;
        ~FAssetManager()                    = default;

        FAssetManager& operator=(const FAssetManager&) = delete;
        FAssetManager& operator=(FAssetManager&&)      = delete;

        template <CAssetCompatible AssetType>
        void AddAsset(std::string_view Name, AssetType&& Asset);

        template <CAssetCompatible AssetType, typename... Types>
        void AddAsset(std::string_view Name, Types&&... Args);

        template <CAssetCompatible AssetType>
        TAssetHandle<AssetType> AcquireAsset(std::string_view Name);

        void PinAsset(std::string_view Name);
        void UnpinAsset(std::string_view Name);
        void RemoveAssetImmediately(std::string_view Name);
        void CollectGarbage();

    private:
        template <CAssetCompatible AssetType>
        friend class TAssetHandle;

        using FAssetMap = Utils::FStringHeteroHashTable<std::string, std::unique_ptr<FAssetEntry>>;

    private:
        void ReleaseAsset(FAssetEntry* Asset);

    private:
        FVulkanContext*       VulkanContext_;
        FAssetMap             Assets_;
        std::shared_ptr<bool> LivenessToken_;
    };
} // namespace Npgs

#include "AssetManager.inl"
