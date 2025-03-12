#version 450
#pragma shader_stage(vertex)

layout(location = 0) in  vec3 Position;
layout(location = 0) out vec3 TexCoord;

layout(std140, set = 0, binding = 0) uniform VpMatrices
{
	mat4x4 View;
	mat4x4 Projection;
} iVpMatrices;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	TexCoord    = Position;
	gl_Position = (iVpMatrices.Projection * mat4x4(mat3x3(iVpMatrices.View)) * vec4(Position, 1.0)).xyww;
}
