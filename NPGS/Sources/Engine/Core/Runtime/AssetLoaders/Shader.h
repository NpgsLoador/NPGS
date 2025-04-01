#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.hpp>
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.h"

namespace Npgs
{
    class FShader
    {
    public:
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
            std::vector<FVertexBufferInfo>    VertexBufferInfos;
            std::vector<FVertexAttributeInfo> VertexAttributeInfos;
            std::vector<FShaderBufferInfo>    ShaderBufferInfos;
            std::unordered_map<vk::ShaderStageFlagBits, std::vector<std::string>> PushConstantInfos;
        };

        struct FDescriptorBindingInfo
        {
            std::uint32_t        Binding{};
            vk::DescriptorType   Type{};
            std::uint32_t        Count{};
            vk::ShaderStageFlags Stage;
        };

        struct FDescriptorSetInfo
        {
            std::uint32_t  Set{};
            vk::DeviceSize Size{};
            std::vector<FDescriptorBindingInfo> Bindings;
        };

    private:
        struct FShaderInfo
        {
            std::vector<std::uint32_t> Code;
            vk::ShaderStageFlagBits    Stage{};
        };

        struct FShaderReflectionInfo
        {
            std::unordered_map<std::uint32_t, std::vector<vk::DescriptorSetLayoutBinding>> DescriptorSetBindings;
            std::vector<vk::VertexInputBindingDescription>   VertexInputBindings;
            std::vector<vk::VertexInputAttributeDescription> VertexInputAttributes;
            std::vector<vk::PushConstantRange>               PushConstants;
        };

    public:
        FShader(const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo);
        FShader(const FShader&) = delete;
        FShader(FShader&& Other) noexcept;
        ~FShader() = default;

        FShader& operator=(const FShader&) = delete;
        FShader& operator=(FShader&& Other) noexcept;

        std::vector<vk::PipelineShaderStageCreateInfo> CreateShaderStageCreateInfo() const;
        std::vector<vk::DescriptorSetLayout> GetDescriptorSetLayouts() const;
        std::vector<vk::PushConstantRange> GetPushConstantRanges() const;
        std::uint32_t GetPushConstantOffset(const std::string& Name) const;
        std::map<std::uint32_t, vk::DeviceSize> GetDescriptorSetSizes() const;

        const std::vector<vk::VertexInputBindingDescription>& GetVertexInputBindings() const;
        const std::vector<vk::VertexInputAttributeDescription>& GetVertexInputAttributes() const;
        const FDescriptorSetInfo& GetDescriptorSetInfo(std::uint32_t Set) const;

    private:
        void InitializeShaders(const std::vector<std::string>& ShaderFiles, const FResourceInfo& ResourceInfo);
        FShaderInfo LoadShader(const std::string& Filename);
        void ReflectShader(const FShaderInfo& ShaderInfo, const FResourceInfo& ResourceInfo);
        void AddDescriptorSetBindings(std::uint32_t Set, vk::DescriptorSetLayoutBinding& LayoutBinding);
        void CreateDescriptorSetLayouts();
        void GenerateDescriptorInfos();

    private:
        FShaderReflectionInfo                                                _ReflectionInfo;
        std::vector<std::pair<vk::ShaderStageFlagBits, FVulkanShaderModule>> _ShaderModules;
        std::unordered_map<std::string, std::uint32_t>                       _PushConstantOffsetsMap;  // [Name, Offset]
        std::map<std::uint32_t, FVulkanDescriptorSetLayout>                  _DescriptorSetLayoutsMap; // [Set,  Layout]
        std::map<std::uint32_t, FDescriptorSetInfo>                          _DescriptorSetInfos;      // [Set,  Info]
    };

} // namespace Npgs

#include "Shader.inl"
