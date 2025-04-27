#include "Engine/Core/Base/Base.h"

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

    NPGS_INLINE std::map<std::uint32_t, vk::DeviceSize> FShader::GetDescriptorSetSizes() const
    {
        std::map<std::uint32_t, vk::DeviceSize> SetSizes;
        for (const auto& [Set, Info] : DescriptorSetInfos_)
        {
            SetSizes.emplace(Set, Info.Size);
        }

        return SetSizes;
    }

    NPGS_INLINE const std::vector<vk::VertexInputBindingDescription>& FShader::GetVertexInputBindings() const
    {
        return ReflectionInfo_.VertexInputBindings;
    }

    NPGS_INLINE const std::vector<vk::VertexInputAttributeDescription>& FShader::GetVertexInputAttributes() const
    {
        return ReflectionInfo_.VertexInputAttributes;
    }

    NPGS_INLINE const FShader::FDescriptorSetInfo& FShader::GetDescriptorSetInfo(std::uint32_t Set) const
    {
        return DescriptorSetInfos_.at(Set);
    }
} // namespace Npgs
