#version 450
#pragma shader_stage(fragment)

layout(location = 0) in  float Height;
layout(location = 1) in  float HeightFactor;
layout(location = 2) in  float bDrawFrame;
layout(location = 0) out vec4  FragColor;

void main()
{
	float Color = Height / HeightFactor;
	if (bDrawFrame > 0.5)
	{
		FragColor = vec4(1.0, 0.0, 0.0, 1.0);
	}
	else
	{
		FragColor = vec4(vec3(Color), 1.0);
	}
}
