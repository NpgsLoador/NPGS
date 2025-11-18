#version 450
#pragma shader_stage(vertex)

#include "Common/BindlessExtensions.glsl"

layout(location = 0) in vec3 Position;
layout(location = 1) in mat4 Model;

layout(push_constant) uniform _DeviceAddress
{
	uint64_t MatricesAddress;
} DeviceAddress;

layout(buffer_reference, scalar) readonly buffer _Matrices
{
	layout(offset = 128) mat4x4 LightSpaceMatrix;
};

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	_Matrices Matrices = _Matrices(DeviceAddress.MatricesAddress);
	gl_Position = Matrices.LightSpaceMatrix * Model * vec4(Position, 1.0);
}
