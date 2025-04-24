#version 450
#pragma shader_stage(vertex)

#include "Common/BindlessExtensions.glsl"

layout(location = 0) in vec3 Position;
layout(location = 1) in mat4x4 Model;

layout(push_constant) uniform DeviceAddress
{
	uint64_t MatricesAddress;
} iDeviceAddress;

layout(buffer_reference, scalar) readonly buffer Matrices
{
	layout(offset = 128) mat4x4 LightSpaceMatrix;
};

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	Matrices iMatrices = Matrices(iDeviceAddress.MatricesAddress);
	gl_Position = iMatrices.LightSpaceMatrix * Model * vec4(Position, 1.0);
}
