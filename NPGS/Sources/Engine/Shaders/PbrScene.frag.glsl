#version 450
#pragma shader_stage(fragment)

#include "Common/NumericConstants.glsl"

layout(location = 0) out vec4 FragColor;

layout(location = 0) in _FragInput
{
	mat3x3 TbnMatrix;
	vec2   TexCoord;
	vec3   WorldPos;
	vec3   Normal;
} FragInput;

layout(std140, set = 0, binding = 1) uniform LightArgs
{
	vec3 LightPos;
	vec3 LightColor;
	vec3 CameraPos;
} iLightArgs;

layout(set = 1, binding = 0) uniform sampler   iSampler;
layout(set = 1, binding = 1) uniform texture2D iDiffuseTex;
layout(set = 1, binding = 2) uniform texture2D iNormalTex;
layout(set = 1, binding = 3) uniform texture2D iArmTex;

float TrowbridgeReitzGGX(vec3 Normal, vec3 HalfDir, float Roughness)
{
	float Alpha2 = pow(Roughness, 4);
	float NdotH  = max(dot(Normal, HalfDir), 0.0);
	float NdotH2 = pow(NdotH, 2);

	float Numerator   = Alpha2;
	float Denominator = kPi * pow((NdotH2 * (Alpha2 - 1.0) + 1.0), 2);

	return Numerator / Denominator;
}

float GemoetrySchlickGGX(float CosTheta, float Roughness)
{
	float Kx = pow(Roughness + 1.0, 2) / 8.0;
	return CosTheta / (CosTheta * (1.0 - Kx) + Kx);
}

float GemoetyrSmithGGX(vec3 Normal, vec3 ViewDir, vec3 LightDir, float Roughness)
{
	float NdotV = dot(Normal, ViewDir);
	float NdotL = dot(Normal, LightDir);
	float GGX1  = GemoetrySchlickGGX(NdotV, Roughness);
	float GGX2  = GemoetrySchlickGGX(NdotL, Roughness);

	return GGX1 * GGX2;
}

vec3 FresnelSchlick(float HdotV, vec3 Fx)
{
	return Fx + (1.0 - Fx) * pow(clamp(1.0 - HdotV, 0.0, 1.0), 5);
}

void main()
{
    vec3 TexNormal = texture(sampler2D(iNormalTex, iSampler), FragInput.TexCoord).rgb;
	vec3 RgbNormal = TexNormal * 2.0 - 1.0;
    vec3 Normal    = normalize(RgbNormal);
	vec3 ViewDir   = FragInput.TbnMatrix * normalize(iLightArgs.CameraPos - FragInput.WorldPos);
	vec3 Albedo    = texture(sampler2D(iDiffuseTex, iSampler), FragInput.TexCoord).rgb;
	vec3 ArmColor  = texture(sampler2D(iArmTex, iSampler), FragInput.TexCoord).rgb;
	float AmbientOcclusion = ArmColor.r;
	float Roughness        = ArmColor.g;
	float Metallic         = ArmColor.b;

	vec3 Fx = vec3(0.04);
	Fx = mix(Fx, Albedo, Metallic);

	vec3 RadianceSum = vec3(0.0);

	vec3 LightDir = FragInput.TbnMatrix * normalize(iLightArgs.LightPos - FragInput.WorldPos);
	vec3 HalfDir  = normalize(LightDir + ViewDir);

	float Distance    = length(iLightArgs.LightPos - FragInput.WorldPos);
	float Attenuation = 1.0 / pow(Distance, 2);
	vec3  Radiance    = iLightArgs.LightColor * Attenuation;

	float NormalDistFunc  = TrowbridgeReitzGGX(Normal, HalfDir, Roughness);
	float GemoertyFunc    = GemoetyrSmithGGX(Normal, ViewDir, LightDir, Roughness);
	vec3  FresnelEquation = FresnelSchlick(clamp(dot(HalfDir, ViewDir), 0.0, 1.0), Fx);

	vec3  Numerator   = NormalDistFunc * GemoertyFunc * FresnelEquation;
	float Denominator = 4.0 * max(dot(ViewDir, Normal), 0.0) * max(dot(LightDir, Normal), 0.0);
	vec3  CookTorranceSpec = Numerator / (Denominator + 1e-3);

	vec3 Specular = FresnelEquation;
	vec3 Diffuse  = 1.0 - Specular;
	Diffuse      *= 1.0 - Metallic;

	float NdotL = max(dot(Normal, LightDir), 0.0);
	RadianceSum += (Diffuse * Albedo / kPi + CookTorranceSpec) * Radiance * NdotL;

	vec3 Ambient = vec3(0.03) * Albedo * AmbientOcclusion;
	vec3 Color   = Ambient + RadianceSum;

	FragColor = vec4(Color, 1.0);
}
