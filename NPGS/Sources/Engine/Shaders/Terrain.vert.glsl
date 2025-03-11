#version 450
#pragma shader_stage(vertex)

layout(location = 0) in  vec3  Position;
layout(location = 0) out float Height;

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
	Height = Position.y;
	gl_Position = iVpMatrices.Projection * iVpMatrices.View * mat4x4(1.0) * vec4(Position, 1.0);
}
