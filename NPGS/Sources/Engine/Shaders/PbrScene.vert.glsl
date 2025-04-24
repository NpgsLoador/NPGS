#version 450
#pragma shader_stage(vertex)

#include "Common/BindlessExtensions.glsl"

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;
layout(location = 3) in vec3 Tangent;
layout(location = 4) in vec3 Bitangent;
layout(location = 5) in mat4x4 Model;

layout(location = 0) out _VertOutput
{
	mat3x3 TbnMatrix;
	vec2   TexCoord;
	vec3   FragPos;
	vec4   LightSpaceFragPos;
} VertOutput;

layout(push_constant) uniform DeviceAddress
{
	uint64_t MatricesAddress;
} iDeviceAddress;

layout(buffer_reference, scalar) readonly buffer Matrices
{
	mat4x4 View;
	mat4x4 Projection;
	mat4x4 LightSpaceMatrix;
};

// layout(set = 1, binding = 0) uniform sampler   iSampler;
// layout(set = 1, binding = 4) uniform texture2D iDisplacementTex;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	VertOutput.TexCoord = TexCoord;

	// float Displacement = texture(sampler2D(iDisplacementTex, iSampler), TexCoord).r;
	// vec3  NewPosition  = Position + Normal * Displacement;
	VertOutput.FragPos = vec3(Model * vec4(Position, 1.0));

	mat3x3 NormalMatrix = transpose(inverse(mat3x3(Model)));
	vec3 T = normalize(vec3(Model * vec4(Tangent,               0.0)));
	vec3 B = normalize(vec3(Model * vec4(Bitangent,             0.0)));
	vec3 N = normalize(vec3(Model * vec4(NormalMatrix * Normal, 0.0)));

	mat3x3 TbnMatrix = mat3x3(T, B, N);
	VertOutput.TbnMatrix = transpose(TbnMatrix);

	Matrices iMatrices = Matrices(iDeviceAddress.MatricesAddress);
	VertOutput.LightSpaceFragPos = iMatrices.LightSpaceMatrix * vec4(VertOutput.FragPos, 1.0);

	gl_Position = iMatrices.Projection * iMatrices.View * vec4(VertOutput.FragPos, 1.0);
}
