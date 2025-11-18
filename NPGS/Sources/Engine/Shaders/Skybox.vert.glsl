#version 450
#pragma shader_stage(vertex)

#include "Common/BindlessExtensions.glsl"

layout(location = 0) in  vec3 Position;
layout(location = 0) out vec3 TexCoord;

layout(push_constant) uniform _DeviceAddress
{
	uint64_t MatricesAddress;
} DeviceAddress;

layout(buffer_reference, scalar) readonly buffer _VpMatrices
{
	mat4x4 View;
	mat4x4 Projection;
};

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	TexCoord = Position;

	_VpMatrices VpMatrices = _VpMatrices(DeviceAddress.MatricesAddress);
	gl_Position = (VpMatrices.Projection * mat4(mat3(VpMatrices.View)) * vec4(Position, 1.0)).xyww;
}
