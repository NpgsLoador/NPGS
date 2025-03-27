#include "Shader.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

NPGS_INLINE std::vector<vk::PushConstantRange> FShader::GetPushConstantRanges() const
{
    return _ReflectionInfo.PushConstants;
}

NPGS_INLINE std::uint32_t FShader::GetPushConstantOffset(const std::string& Name) const
{
    return _PushConstantOffsetsMap.at(Name);
}

NPGS_INLINE const std::vector<vk::VertexInputBindingDescription>& FShader::GetVertexInputBindings() const
{
    return _ReflectionInfo.VertexInputBindings;
}

NPGS_INLINE const std::vector<vk::VertexInputAttributeDescription>& FShader::GetVertexInputAttributes() const
{
    return _ReflectionInfo.VertexInputAttributes;
}

NPGS_INLINE const std::vector<FShader::FDescriptorSetInfo>& FShader::GetDescriptorSetInfos() const
{
    return _DescriptorSetInfos;
}

_ASSET_END
_RUNTIME_END
_NPGS_END
