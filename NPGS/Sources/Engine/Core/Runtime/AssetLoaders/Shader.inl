#include "Shader.h"

#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE std::vector<vk::PushConstantRange> FShader::GetPushConstantRanges() const
    {
        return _ReflectionInfo.PushConstants;
    }

    NPGS_INLINE std::uint32_t FShader::GetPushConstantOffset(const std::string& Name) const
    {
        return _PushConstantOffsetsMap.at(Name);
    }

    NPGS_INLINE std::map<std::uint32_t, vk::DeviceSize> FShader::GetDescriptorSetSizes() const
    {
        std::map<std::uint32_t, vk::DeviceSize> SetSizes;
        for (const auto& [Set, Info] : _DescriptorSetInfos)
        {
            SetSizes.emplace(Set, Info.Size);
        }

        return SetSizes;
    }

    NPGS_INLINE const std::vector<vk::VertexInputBindingDescription>& FShader::GetVertexInputBindings() const
    {
        return _ReflectionInfo.VertexInputBindings;
    }

    NPGS_INLINE const std::vector<vk::VertexInputAttributeDescription>& FShader::GetVertexInputAttributes() const
    {
        return _ReflectionInfo.VertexInputAttributes;
    }

    NPGS_INLINE const FShader::FDescriptorSetInfo&
        FShader::GetDescriptorSetInfo(std::uint32_t Set) const
    {
        return _DescriptorSetInfos.at(Set);
    }
} // namespace Npgs
