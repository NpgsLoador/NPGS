#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"
#include "Engine/Runtime/Managers/AssetManager.hpp"

namespace Npgs
{
    struct FSpecializationConstantBuffer
    {
    public:
        std::vector<vk::SpecializationMapEntry> Entries;
        std::vector<std::byte>                  DataBuffer;

        bool operator==(const FSpecializationConstantBuffer&) const = default;

    private:
        friend struct FShaderAcquireInfo;

        template <typename Ty>
        requires std::is_standard_layout_v<Ty> && std::is_trivial_v<Ty>
        void AddConstant(std::uint32_t Id, const Ty& Value);
    };

    struct FShaderAcquireInfo
    {
    public:
        struct FShaderInfo
        {
            std::string Name;
            vk::ShaderStageFlagBits NextStage{};

            bool operator==(const FShaderInfo&) const = default;
        };

        ankerl::unordered_dense::map<vk::ShaderStageFlagBits, FShaderInfo> ShaderInfos;

        bool operator==(const FShaderAcquireInfo& Other) const;

        template <typename Ty>
        requires std::is_standard_layout_v<Ty>&& std::is_trivial_v<Ty>
        void AddSpecializationConstant(vk::ShaderStageFlagBits Stage, std::string_view Name, const Ty& Value);

    private:
        friend struct FShaderAcquireInfoHash;
        friend class  FShaderManager;

        FSpecializationConstantBuffer                            SpecializationConstantBuffer_;
        std::vector<std::function<void(FAssetManager*)>>         ConstantCommitList_;
        ankerl::unordered_dense::map<std::uint32_t, std::string> UsedSpecializationConstantIds_;
    };

    struct FShaderAcquireInfoHash
    {
        std::size_t operator()(const FShaderAcquireInfo& Info) const noexcept;
    };

    struct FShaderResource
    {
    public:
        std::vector<vk::ShaderStageFlagBits>                            Stages;
        std::vector<vk::ShaderEXT>                                      Handles;
        std::vector<vk::VertexInputBindingDescription2EXT>              VertexInputBindings;
        std::vector<vk::VertexInputAttributeDescription2EXT>            VertexInputAttributes;
        FVulkanPipelineLayout                                           PipelineLayout;
        ankerl::unordered_dense::map<std::uint32_t, FDescriptorSetInfo> SetInfos;

    public:
        FShaderResource()                           = default;
        FShaderResource(const FShaderResource&)     = delete;
        FShaderResource(FShaderResource&&) noexcept = default;
        ~FShaderResource()                          = default;

        FShaderResource& operator=(const FShaderResource&)     = delete;
        FShaderResource& operator=(FShaderResource&&) noexcept = default;

        std::uint32_t GetPushConstantOffset(std::string_view Name) const;

    private:
        friend class FShaderManager;

        void ApplyHandles();

        std::vector<FVulkanShader>                                StoragedHandles_;
        Utils::TStringHeteroHashTable<std::string, std::uint32_t> PushConstantOffsetsMap_;
    };

    class FShaderManager
    {
    public:
        FShaderManager(FVulkanContext* VulkanContext, FAssetManager* AssetManager);
        FShaderManager(const FShaderManager&) = delete;
        FShaderManager(FShaderManager&&)      = delete;
        ~FShaderManager()                     = default;

        FShaderManager& operator=(const FShaderManager&) = delete;
        FShaderManager& operator=(FShaderManager&&)      = delete;

        FShaderResource* AcquireShaderResource(const FShaderAcquireInfo& AcquireInfo);

    private:
        using FSetLayoutBindingMap    = ankerl::unordered_dense::map<std::uint32_t, std::vector<vk::DescriptorSetLayoutBinding>>;
        using FSetLayoutArray         = std::vector<FVulkanDescriptorSetLayout>;
        using FShaderResourceMap      = ankerl::unordered_dense::map<FShaderAcquireInfo, std::unique_ptr<FShaderResource>, FShaderAcquireInfoHash>;
        using FShaderHandle           = TAssetHandle<FShader>;
        using FShaderStagePair        = std::pair<FShaderHandle, vk::ShaderStageFlagBits>;
        using FShaderStagePairArray   = std::vector<FShaderStagePair>;
        using FPushConstantOffsetsMap = Utils::TStringHeteroHashTable<std::string, std::uint32_t>;
        using FDescriptorSetInfoMap   = ankerl::unordered_dense::map<std::uint32_t, FDescriptorSetInfo>;

    private:
        FSetLayoutBindingMap MergeSetLayoutBindings(const FShaderStagePairArray& Shaders) const;
        FSetLayoutArray SetupDescriptorSetLayouts(const FSetLayoutBindingMap& MergedSetLayoutBindings) const;
        std::vector<vk::PushConstantRange> MergePushConstantRanges(const FShaderStagePairArray& Shaders) const;
        FPushConstantOffsetsMap GeneratePushConstantOffsetsMap(const FShaderStagePairArray& Shaders);
        FDescriptorSetInfoMap GenerateDescriptorSetInfos(const FSetLayoutBindingMap& SetLayoutBindings, const FSetLayoutArray& SetLayouts);

    private:
        FVulkanContext*    VulkanContext_;
        FAssetManager*     AssetManager_;
        FShaderResourceMap ShaderResourceCache_;
        std::mutex         CacheMutex_;
        std::shared_mutex  SharedCacheMutex_;
    };
} // namespace Npgs

#include "ShaderManager.inl"
