#version 450
#pragma shader_stage(tesseval)

layout(quads, fractional_odd_spacing, ccw) in;
layout(location = 0) in _TeseInput
{
	vec2  TexCoord;
	float HeightFactor;
} TeseInput[];

layout(location = 0) out _TeseOutput
{
	float HeightData;
	float HeightFactor;
	float bDrawFrame;
} TeseOutput;

layout(std140, set = 0, binding = 0) uniform MvpMatrices
{
	mat4x4 Model;
	mat4x4 View;
	mat4x4 Projection;
} iMvpMatrices;

layout(set = 1, binding = 0) uniform sampler2D iHeightMap;

void main()
{
	vec2 TexCoord00 = TeseInput[0].TexCoord;
	vec2 TexCoord01 = TeseInput[1].TexCoord;
	vec2 TexCoord11 = TeseInput[2].TexCoord;
	vec2 TexCoord10 = TeseInput[3].TexCoord;

	vec2 TexCoord0x = (TexCoord10 - TexCoord00) * gl_TessCoord.x + TexCoord00;
	vec2 TexCoord1x = (TexCoord11 - TexCoord01) * gl_TessCoord.x + TexCoord01;
	vec2 TexCoord   = (TexCoord1x - TexCoord0x) * gl_TessCoord.y + TexCoord0x;

	TeseOutput.HeightFactor = TeseInput[0].HeightFactor;
	TeseOutput.HeightData   = texture(iHeightMap, TexCoord).r * TeseOutput.HeightFactor;

	vec4 Vertex00 = gl_in[0].gl_Position;
	vec4 Vertex01 = gl_in[1].gl_Position;
	vec4 Vertex11 = gl_in[2].gl_Position;
	vec4 Vertex10 = gl_in[3].gl_Position;

	vec4 Tangent   = Vertex10 - Vertex00;
	vec4 Bitangent = Vertex01 - Vertex00;
	vec4 Normal    = -normalize(vec4(cross(Tangent.xyz, Bitangent.xyz), 1.0));

	vec4 Vertex0x = (Vertex10 - Vertex00) * gl_TessCoord.x + Vertex00;
	vec4 Vertex1x = (Vertex11 - Vertex01) * gl_TessCoord.x + Vertex01;
	vec4 Vertex   = (Vertex1x - Vertex0x) * gl_TessCoord.y + Vertex0x;

	float EdgeThreshold = 0.0000001;
	if (gl_TessCoord.x < EdgeThreshold || gl_TessCoord.x > 1.0 - EdgeThreshold ||
		gl_TessCoord.y < EdgeThreshold || gl_TessCoord.y > 1.0 - EdgeThreshold)
	{
		TeseOutput.bDrawFrame = 1.0;
	}
	else
	{
		TeseOutput.bDrawFrame = 0.0;
	}

	Vertex += Normal * TeseOutput.HeightData;
	gl_Position = iMvpMatrices.Projection * iMvpMatrices.View * iMvpMatrices.Model * Vertex;
}
