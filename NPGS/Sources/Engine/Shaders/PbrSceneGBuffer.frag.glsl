#version 450
#pragma shader_stage(fragment)

layout(location = 0) out vec4  RgbPositionAAo;
layout(location = 1) out vec4  RgbNormalARough;
layout(location = 2) out vec4  RgbAlbedoAMetal;
layout(location = 3) out float Shadow;

layout(location = 0) in _FragInput
{
	mat3 TbnMatrix;
	vec2 TexCoord;
	vec3 FragPos;
	vec4 LightSpaceFragPos;
} FragInput;

layout(set = 0, binding = 0) uniform sampler   iSampler;
layout(set = 1, binding = 1) uniform texture2D iAlbdeoTex;
layout(set = 1, binding = 2) uniform texture2D iNormalTex;
layout(set = 1, binding = 3) uniform texture2D iArmTex;
layout(set = 2, binding = 0) uniform sampler2D iDepthMap;

float CalcShadow(vec4 FragPosLightSpace, float Bias)
{
    vec3 ProjectiveCoord = FragPosLightSpace.xyz / FragPosLightSpace.w;
    ProjectiveCoord.xy   = ProjectiveCoord.xy * 0.5 + 0.5;

    if (ProjectiveCoord.z > 1.0)
    {
        return 0.0;
    }

    float Shadow = 0.0;
    vec2 TexelSize = 1.0 / textureSize(iDepthMap, 0);
    
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float ClosestDepth = texture(iDepthMap, ProjectiveCoord.xy + vec2(x, y) * TexelSize).r;
            float CurrentDepth = ProjectiveCoord.z;
            Shadow += CurrentDepth - Bias > ClosestDepth ? 1.0 : 0.0;
        }
    }

    Shadow /= 9.0;

    return Shadow;
}

void main()
{
#ifdef LAMP_BOX
	RgbPositionAAo  = vec4(FragInput.FragPos, 0.0);
	RgbNormalARough = vec4(0.0);
	RgbAlbedoAMetal = vec4(0.0);
    Shadow          = 0.0;
#else
	vec3  TexAlbedo    = texture(sampler2D(iAlbdeoTex, iSampler), FragInput.TexCoord).rgb;
	vec3  TexNormal    = texture(sampler2D(iNormalTex, iSampler), FragInput.TexCoord).rgb;
    vec3  TexArm       = texture(sampler2D(iArmTex,    iSampler), FragInput.TexCoord).rgb;
	float TexAo        = TexArm.r;
	float TexRoughness = TexArm.g;
	float TexMetallic  = TexArm.b;

	vec3 Normal = normalize(transpose(FragInput.TbnMatrix) * normalize(TexNormal * 2.0 - 1.0));

	RgbPositionAAo  = vec4(FragInput.FragPos, TexAo);
	RgbNormalARough = vec4(Normal, TexRoughness);
	RgbAlbedoAMetal = vec4(pow(TexAlbedo, vec3(2.2)), TexMetallic);
    Shadow          = CalcShadow(FragInput.LightSpaceFragPos, 0.005);
#endif // LAMP_BOX
}
