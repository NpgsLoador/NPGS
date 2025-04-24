#version 450
#pragma shader_stage(fragment)

layout(location = 0) in _FragInput
{
	float HeightData;
	float HeightFactor;
	float bDrawFrame;
} FragInput;

layout(location = 0) out vec4 FragColor;

void main()
{
	float Color = FragInput.HeightData / FragInput.HeightFactor;
	if (FragInput.bDrawFrame > 0.5)
	{
		FragColor = vec4(1.0, 0.0, 0.0, 1.0);
	}
	else
	{
		FragColor = vec4(vec3(Color), 1.0);
	}
}
