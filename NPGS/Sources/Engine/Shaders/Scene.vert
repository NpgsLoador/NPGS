#version 450
#pragma shader_stage(vertex)

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
	mat3x3 TbnMatrix = mat3x3(normalize(vec3(Model * vec4(Tangent,   0.0))),
						      normalize(vec3(Model * vec4(Bitangent, 0.0))),
						      normalize(vec3(Model * vec4(Normal,    0.0))));

	VertOutput.TbnMatrix = TbnMatrix;
	VertOutput.TexCoord  = TexCoord;
	VertOutput.FragPos   = vec3(Model * vec4(Position, 1.0));
	VertOutput.LightSpaceFragPos = iMatrices.LightSpaceMatrix * vec4(VertOutput.FragPos, 1.0);

	gl_Position = iMatrices.Projection * iMatrices.View * Model * vec4(Position, 1.0);
}
