#version 450
#pragma shader_stage(vertex)

layout(location = 0) in  vec3 Position;
layout(location = 1) in  vec2 TexCoord;
layout(location = 0) out vec2 TexCoordToTesc;

void main()
{
    TexCoordToTesc = TexCoord;
	gl_Position    = vec4(Position, 1.0);
}
