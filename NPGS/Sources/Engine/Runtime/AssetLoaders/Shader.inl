#include "Shader.hpp"
#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    NPGS_INLINE const std::vector<vk::PushConstantRange>& FShader::GetPushConstantRanges() const
    {
        return ReflectionInfo_.PushConstants;
    }

    NPGS_INLINE std::uint32_t FShader::GetPushConstantOffset(std::string_view Name) const
    {
        return PushConstantOffsetsMap_.at(Name);
    }

    NPGS_INLINE const Utils::TStringHeteroHashTable<std::string, std::uint32_t>& FShader::GetSpecializationConstantsInfo() const
    {
        return ReflectionInfo_.SpecializationConstants;
    }

    NPGS_INLINE std::uint32_t FShader::GetSpecializationConstantId(std::string_view Name) const
    {
        return ReflectionInfo_.SpecializationConstants.at(Name);
    }

    NPGS_INLINE std::string_view FShader::GetFilename() const
    {
        return Filename_;
    }

    NPGS_INLINE vk::ShaderStageFlagBits FShader::GetShaderStage() const
    {
        return ReflectionInfo_.Stage;
    }

    NPGS_INLINE const std::vector<std::uint32_t>& FShader::GetShaderCode() const
    {
        return ShaderCode_;
    }

    NPGS_INLINE const Utils::TStringHeteroHashTable<std::string, std::uint32_t>& FShader::GetPushConstantOffsetsMap() const
    {
        return PushConstantOffsetsMap_;
    }

    NPGS_INLINE const ankerl::unordered_dense::map<std::uint32_t, std::vector<vk::DescriptorSetLayoutBinding>>&
    FShader::GetSetLayoutBindings() const
    {
        return ReflectionInfo_.SetLayoutBindings;
    }

    NPGS_INLINE const std::vector<vk::VertexInputBindingDescription2EXT>& FShader::GetVertexInputBindings() const
    {
        return ReflectionInfo_.VertexInputBindings;
    }

    NPGS_INLINE const std::vector<vk::VertexInputAttributeDescription2EXT>& FShader::GetVertexInputAttributes() const
    {
        return ReflectionInfo_.VertexInputAttributes;
    }

    NPGS_INLINE const FDescriptorSetInfo& FShader::GetDescriptorSetInfo(std::uint32_t Set) const
    {
        return DescriptorSetInfos_.at(Set);
    }

    NPGS_INLINE const ankerl::unordered_dense::map<std::uint32_t, FDescriptorSetInfo>& FShader::GetDescriptorSetInfos() const
    {
        return DescriptorSetInfos_;
    }
} // namespace Npgs
