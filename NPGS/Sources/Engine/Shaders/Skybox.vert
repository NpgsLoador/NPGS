#version 450
#pragma shader_stage(vertex)

#include "Common/BindlessExtensions.glsl"

layout(location = 0) in  vec3 Position;
layout(location = 0) out vec3 TexCoord;

layout(push_constant) uniform DeviceAddress
{
	uint64_t MatricesAddress;
} iDeviceAddress;

layout(buffer_reference, scalar) readonly buffer VpMatrices
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

	VpMatrices iVpMatrices = VpMatrices(iDeviceAddress.MatricesAddress);
	gl_Position = (iVpMatrices.Projection * mat4x4(mat3x3(iVpMatrices.View)) * vec4(Position, 1.0)).xyww;
}
