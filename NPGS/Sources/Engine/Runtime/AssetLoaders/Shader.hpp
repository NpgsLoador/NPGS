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

// Forward declaration
namespace spv_reflect
{
    class ShaderModule;
}

namespace Npgs
{
    struct FVertexBufferInfo
    {
        std::uint32_t Binding{};
        std::uint32_t Stride{};
        bool bIsPerInstance{ false };
        std::uint32_t Divisor{ 1 };
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
        using FSpecializationConstantInfoMap = ankerl::unordered_dense::map<std::uint32_t, std::string>;

        std::vector<FVertexBufferInfo>    VertexBufferInfos;
        std::vector<FVertexAttributeInfo> VertexAttributeInfos;
        std::vector<FShaderBufferInfo>    ShaderBufferInfos;
        std::vector<std::string>          PushConstantNames;
        FSpecializationConstantInfoMap    SpecializationConstantInfos;
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
        using FSetBindingMap = ankerl::unordered_dense::map<std::uint32_t, FDescriptorBindingInfo>;

        std::uint32_t  Set{};
        vk::DeviceSize Size{};
        FSetBindingMap Bindings;
    };

    class FShader
    {
    public:
        using FSetLayoutBindingMap         = ankerl::unordered_dense::map<std::uint32_t, std::vector<vk::DescriptorSetLayoutBinding>>;
        using FPushConstantOffsetsMap      = Utils::TStringHeteroHashTable<std::string, std::uint32_t>;
        using FSpecializationConstantIdMap = FPushConstantOffsetsMap;

    public:
        FShader(FVulkanContext* VulkanContext, std::string_view Filename, const FResourceInfo& ResourceInfo);
        FShader(const FShader&) = delete;
        FShader(FShader&& Other) noexcept;
        ~FShader() = default;

        FShader& operator=(const FShader&) = delete;
        FShader& operator=(FShader&& Other) noexcept;

        const std::vector<vk::PushConstantRange>& GetPushConstantRanges() const;
        std::uint32_t GetSpecializationConstantId(std::string_view Name) const;
        std::string_view GetFilename() const;
        vk::ShaderStageFlagBits GetShaderStage() const;
        const std::vector<std::uint32_t>& GetShaderCode() const;
        const FPushConstantOffsetsMap& GetPushConstantOffsetsMap() const;
        const FSetLayoutBindingMap& GetSetLayoutBindings() const;
        const std::vector<vk::VertexInputBindingDescription2EXT>& GetVertexInputBindings() const;
        const std::vector<vk::VertexInputAttributeDescription2EXT>& GetVertexInputAttributes() const;

    private:
        struct FShaderReflectionInfo
        {
            FSetLayoutBindingMap                                 SetLayoutBindings;
            std::vector<vk::VertexInputBindingDescription2EXT>   VertexInputBindings;
            std::vector<vk::VertexInputAttributeDescription2EXT> VertexInputAttributes;
            std::vector<vk::PushConstantRange>                   PushConstants;
            FSpecializationConstantIdMap                         SpecializationConstants;
            vk::ShaderStageFlagBits                              Stage{};
        };

    private:
        void InitializeShaders(std::string_view Filename, const FResourceInfo& ResourceInfo);
        void LoadShader(std::string Filename);
        void ReflectShader(const FResourceInfo& ResourceInfo);
        void ReflectPushConstants(const FResourceInfo& ResourceInfo, const spv_reflect::ShaderModule& ShaderModule);
        void ReflectSpecializationConstants(const FResourceInfo& ResourceInfo, const spv_reflect::ShaderModule& ShaderModule);
        void ReflectDescriptorSets(const FResourceInfo& ResourceInfo, const spv_reflect::ShaderModule& ShaderModule);
        void ReflectVertexInput(const FResourceInfo& ResourceInfo, const spv_reflect::ShaderModule& ShaderModule);

    private:
        using FPushConstantOffsetsMap = Utils::TStringHeteroHashTable<std::string, std::uint32_t>; // [Name, Offset]

        FVulkanContext*            VulkanContext_;
        FShaderReflectionInfo      ReflectionInfo_;
        std::string                Filename_;
        std::vector<std::uint32_t> ShaderCode_;
        FPushConstantOffsetsMap    PushConstantOffsetsMap_;
    };

} // namespace Npgs

#include "Shader.inl"
