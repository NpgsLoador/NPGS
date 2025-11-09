#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    NPGS_INLINE std::vector<vk::PushConstantRange> FShader::GetPushConstantRanges() const
    {
        return ReflectionInfo_.PushConstants;
    }

    NPGS_INLINE std::uint32_t FShader::GetPushConstantOffset(const std::string& Name) const
    {
        return PushConstantOffsetsMap_.at(Name);
    }

    NPGS_INLINE const std::vector<vk::VertexInputBindingDescription>& FShader::GetVertexInputBindings() const
    {
        return ReflectionInfo_.VertexInputBindings;
    }

    NPGS_INLINE const std::vector<vk::VertexInputAttributeDescription>& FShader::GetVertexInputAttributes() const
    {
        return ReflectionInfo_.VertexInputAttributes;
    }

    NPGS_INLINE const FDescriptorSetInfo& FShader::GetDescriptorSetInfo(std::uint32_t Set) const
    {
        return DescriptorSetInfos_.at(Set);
    }

    NPGS_INLINE const std::unordered_map<std::uint32_t, FDescriptorSetInfo>& FShader::GetDescriptorSetInfos() const
    {
        return DescriptorSetInfos_;
    }
} // namespace Npgs
