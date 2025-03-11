#version 450
#pragma shader_stage(fragment)

layout(location = 0) in  float Height;
layout(location = 0) out vec4  FragColor;

void main()
{
	float Color = (Height + 16.0) / 32.0;
	FragColor   = vec4(vec3(Color), 1.0);
}
