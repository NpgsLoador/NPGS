#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <vulkan/vulkan.hpp>

#include "Engine/Core/Utils/Hash.hpp"
#include "Engine/Runtime/AssetLoaders/FileLoader.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"

namespace Npgs
{
    struct FVertexBufferInfo
    {
        std::uint32_t Binding{};
        std::uint32_t Stride{};
        bool bIsPerInstance{ false };
    };

    struct FVertexAttributeInfo
    {
        std::uint32_t Binding{};
        std::uint32_t Location{};
        std::uint32_t Offset{};
    };

    struct FShaderBufferInfo
    {
        std::uint32_t Set{};
        std::uint32_t Binding{};
        bool bIsDynamic{ false };
    };

    struct FResourceInfo
    {
        std::vector<FVertexBufferInfo>                           VertexBufferInfos;
        std::vector<FVertexAttributeInfo>                        VertexAttributeInfos;
        std::vector<FShaderBufferInfo>                           ShaderBufferInfos;
        std::vector<std::string>                                 PushConstantNames;
        ankerl::unordered_dense::map<std::uint32_t, std::string> SpecializationConstantInfos;
    };

    struct FDescriptorBindingInfo
    {
        std::uint32_t        Binding{};
        vk::DescriptorType   Type{};
        std::uint32_t        Count{};
        vk::ShaderStageFlags Stage;
        vk::DeviceSize       Offset{};
    };

    struct FDescriptorSetInfo
    {
        std::uint32_t  Set{};
        vk::DeviceSize Size{};
        ankerl::unordered_dense::map<std::uint32_t, FDescriptorBindingInfo> Bindings;
    };

    class FShader
    {
    public:
        FShader(FVulkanContext* VulkanContext, std::string_view Filename, const FResourceInfo& ResourceInfo);
        FShader(const FShader&) = delete;
        FShader(FShader&& Other) noexcept;
        ~FShader() = default;

        FShader& operator=(const FShader&) = delete;
        FShader& operator=(FShader&& Other) noexcept;

        std::vector<vk::PipelineShaderStageCreateInfo> CreateShaderStageCreateInfo() const;
        std::vector<vk::DescriptorSetLayout> GetDescriptorSetLayouts() const;
        const std::vector<vk::PushConstantRange>& GetPushConstantRanges() const;
        std::uint32_t GetPushConstantOffset(std::string_view Name) const;
        const Utils::TStringHeteroHashTable<std::string, std::uint32_t>& GetSpecializationConstantsInfo() const;
        std::uint32_t GetSpecializationConstantId(std::string_view Name) const;
        std::string_view GetFilename() const;
        vk::ShaderStageFlagBits GetShaderStage() const;
        const std::vector<std::uint32_t>& GetShaderCode() const;
        const Utils::TStringHeteroHashTable<std::string, std::uint32_t>& GetPushConstantOffsetsMap() const;
        const ankerl::unordered_dense::map<std::uint32_t, std::vector<vk::DescriptorSetLayoutBinding>>& GetSetLayoutBindings() const;
        const std::vector<vk::VertexInputBindingDescription2EXT>& GetVertexInputBindings() const;
        const std::vector<vk::VertexInputAttributeDescription2EXT>& GetVertexInputAttributes() const;
        const FDescriptorSetInfo& GetDescriptorSetInfo(std::uint32_t Set) const;
        const ankerl::unordered_dense::map<std::uint32_t, FDescriptorSetInfo>& GetDescriptorSetInfos() const;

    private:
        struct FShaderReflectionInfo
        {
            using FSetLayoutBindingMap = ankerl::unordered_dense::map<std::uint32_t, std::vector<vk::DescriptorSetLayoutBinding>>;
            
            FSetLayoutBindingMap                                      SetLayoutBindings;
            std::vector<vk::VertexInputBindingDescription2EXT>        VertexInputBindings;
            std::vector<vk::VertexInputAttributeDescription2EXT>      VertexInputAttributes;
            std::vector<vk::PushConstantRange>                        PushConstants;
            Utils::TStringHeteroHashTable<std::string, std::uint32_t> SpecializationConstants;
            vk::ShaderStageFlagBits                                   Stage{};
        };

    private:
        void InitializeShaders(std::string_view Filename, const FResourceInfo& ResourceInfo);
        void LoadShader(std::string Filename);
        void ReflectShader(const FResourceInfo& ResourceInfo);
        void AddPushConstantRange(vk::PushConstantRange NewRange);
        void AddDescriptorSetBindings(std::uint32_t Set, const vk::DescriptorSetLayoutBinding& LayoutBinding);
        void CreateDescriptorSetLayouts();
        void GenerateDescriptorInfos();

    private:
        FVulkanContext*                                                      VulkanContext_;
        FVulkanDescriptorSetLayout                                           EmptyDescriptorSetLayout_;
        FShaderReflectionInfo                                                ReflectionInfo_;
        std::string                                                          Filename_;
        std::vector<std::uint32_t>                                           ShaderCode_;
        std::vector<std::pair<vk::ShaderStageFlagBits, FVulkanShaderModule>> ShaderModules_;
        Utils::TStringHeteroHashTable<std::string, std::uint32_t>            PushConstantOffsetsMap_;  // [Name, Offset]
        ankerl::unordered_dense::map<std::uint32_t, FDescriptorSetInfo>      DescriptorSetInfos_;      // [Set,  Info]
        std::map<std::uint32_t, FVulkanDescriptorSetLayout>                  DescriptorSetLayoutsMap_; // [Set,  Layout]
    };

} // namespace Npgs

#include "Shader.inl"
