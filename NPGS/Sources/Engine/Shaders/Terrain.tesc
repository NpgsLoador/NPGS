#version 450
#pragma shader_stage(tesscontrol)

layout(vertices = 4) out;
layout(location = 0) in vec2 TexCoord[];
layout(location = 0) out _TescOutput
{
	vec2  TexCoord;
	float HeightFactor;
} TescOutput[];

layout(std140, set = 0, binding = 0) uniform MvpMatrices
{
	mat4x4 Model;
	mat4x4 View;
	mat4x4 Projection;
} iMvpMatrices;

layout(set = 1, binding = 0) uniform sampler2D iHeightMap;

layout(push_constant) uniform TessArgs
{
	float MinDistance;
	float MaxDistance;
	float MinTessLevel;
	float MaxTessLevel;
} iTessArgs;

float EstimateHeight(vec2 TexCoord)
{
	float HeightFactor = 512.0;
	TescOutput[gl_InvocationID].HeightFactor = HeightFactor;
	return texture(iHeightMap, TexCoord).r * HeightFactor;
}

float MultiLevelTessellation(float NormalizedDistance)
{
	float AdjustedDistance = sqrt(NormalizedDistance);
    if (AdjustedDistance < 0.2)
	{
        return mix(iTessArgs.MaxTessLevel, iTessArgs.MaxTessLevel * 0.75, smoothstep(0.0, 0.2, AdjustedDistance));
    } 
    else if (AdjustedDistance < 0.6)
	{
        return mix(iTessArgs.MaxTessLevel * 0.75, iTessArgs.MaxTessLevel * 0.3, smoothstep(0.2, 0.6, AdjustedDistance));
    }
    else
	{
        return mix(iTessArgs.MaxTessLevel * 0.3, iTessArgs.MinTessLevel, smoothstep(0.6, 1.0, AdjustedDistance));
    }
}

void main()
{
	gl_out[gl_InvocationID].gl_Position  = gl_in[gl_InvocationID].gl_Position;
	TescOutput[gl_InvocationID].TexCoord = TexCoord[gl_InvocationID];

	if (gl_InvocationID == 0)
	{
		TescOutput[gl_InvocationID].HeightFactor = 512.0;
		vec4  VertexViewSpace[4];
		float Distances[4];
		for (int i = 0; i != 4; ++i)
		{
			float EstimatedHeight = 1.0;

			vec4 AdjustedVertex = gl_in[i].gl_Position;
			AdjustedVertex.y += EstimatedHeight;

			VertexViewSpace[i] = iMvpMatrices.View * iMvpMatrices.Model * AdjustedVertex;

			float NormalizedDistance = smoothstep(iTessArgs.MinDistance, iTessArgs.MaxDistance, abs(VertexViewSpace[i].z));
			Distances[i] = NormalizedDistance;	
		}

		float Distance00 = Distances[0];
		float Distance01 = Distances[1];
		float Distance11 = Distances[2];
		float Distance10 = Distances[3];

		float TessLevel0 = MultiLevelTessellation(min(Distance01, Distance00));
		float TessLevel1 = MultiLevelTessellation(min(Distance00, Distance10));
		float TessLevel2 = MultiLevelTessellation(min(Distance10, Distance11));
		float TessLevel3 = MultiLevelTessellation(min(Distance11, Distance01));

		gl_TessLevelOuter[0] = TessLevel0;
		gl_TessLevelOuter[1] = TessLevel1;
		gl_TessLevelOuter[2] = TessLevel2;
		gl_TessLevelOuter[3] = TessLevel3;

		gl_TessLevelInner[0] = max(TessLevel1, TessLevel3);
		gl_TessLevelInner[1] = max(TessLevel0, TessLevel2);
	}
}
