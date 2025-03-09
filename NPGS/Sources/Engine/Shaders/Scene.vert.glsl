#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3   Position;
layout(location = 1) in vec3   Normal;
layout(location = 2) in vec2   TexCoord;
layout(location = 3) in mat4x4 Model;

layout(location = 0) out _VertOutput
{
	vec3 Normal;
	vec2 TexCoord;
	vec3 FragPos;
	vec4 FragPosLightSpace;
} VertOutput;

layout(std140, set = 0, binding = 0) uniform Matrices
{
	mat4x4 View;
	mat4x4 Projection;
	mat4x4 LightSpaceMatrix;
} iMatrices;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	VertOutput.TexCoord = TexCoord;
	VertOutput.Normal   = vec3(transpose(inverse(Model)) * vec4(Normal, 1.0));
	VertOutput.FragPos  = vec3(Model * vec4(Position, 1.0));
	VertOutput.FragPosLightSpace = iMatrices.LightSpaceMatrix * vec4(VertOutput.FragPos, 1.0);

	gl_Position = iMatrices.Projection * iMatrices.View * Model * vec4(Position, 1.0);
}
