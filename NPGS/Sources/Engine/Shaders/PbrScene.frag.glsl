#version 450
#pragma shader_stage(fragment)

#include "Common/NumericConstants.glsl"

layout(location = 0) out vec4 FragColor;

layout(location = 0) in _FragInput
{
	vec3 Normal;
	vec2 TexCoord;
	vec3 FragPos;
	mat3x3 TbnMatrix;
} FragInput;

layout(std140, set = 0, binding = 1) uniform LightArgs
{
	vec3 LightPos;
	vec3 LightColor;
	vec3 CameraPos;
} iLightArgs;

layout(set = 1, binding = 0) uniform sampler   iSampler;
layout(set = 1, binding = 1) uniform texture2D iAoTex;
layout(set = 1, binding = 2) uniform texture2D iDiffuseTex;
layout(set = 1, binding = 3) uniform texture2D iMetallicTex;
layout(set = 1, binding = 4) uniform texture2D iNormalTex;
layout(set = 1, binding = 5) uniform texture2D iRoughnessTex;

float TrowbridgeReitzGGX(vec3 Normal, vec3 HalfDir, float Roughness)
{
	float Alpha2 = pow(Roughness, 4);
	float NdotH  = max(dot(Normal, HalfDir), 0.0);
	float NdotH2 = pow(NdotH, 2);

	float Numerator   = Alpha2;
	float Denominator = kPi * pow((NdotH2 * (Alpha2 - 1.0) + 1.0), 2);

	return Numerator / Denominator;
}

float GeometrySchlickGGX(float CosTheta, float Roughness)
{
	float Kx = pow(Roughness + 1.0, 2) / 8.0;
	return CosTheta / (CosTheta * (1.0 - Kx) + Kx);
}

float GeometrySmith(vec3 Normal, vec3 ViewDir, vec3 LightDir, float Roughness)
{
	float NdotV = max(dot(Normal, ViewDir),  0.01);
	float NdotL = max(dot(Normal, LightDir), 0.01);
	float GGX1  = GeometrySchlickGGX(NdotV, Roughness);
	float GGX2  = GeometrySchlickGGX(NdotL, Roughness);

	return GGX1 * GGX2;
}

vec3 FresnelSchlick(float HdotV, vec3 Fx)
{
	return Fx + (1.0 - Fx) * pow(clamp(1.0 - HdotV, 0.0, 1.0), 5);
}

void main()
{
	vec3  TexAlbedo    = texture(sampler2D(iDiffuseTex,   iSampler), FragInput.TexCoord).rgb;
	vec3  TexNormal    = texture(sampler2D(iNormalTex,    iSampler), FragInput.TexCoord).rgb;
	float TexAo        = texture(sampler2D(iAoTex,        iSampler), FragInput.TexCoord).r;
	float TexMetallic  = texture(sampler2D(iMetallicTex,  iSampler), FragInput.TexCoord).b;
	float TexRoughness = texture(sampler2D(iRoughnessTex, iSampler), FragInput.TexCoord).r;

	vec3 Normal = normalize(TexNormal * 2.0 - 1.0);
	vec3 Fx     = vec3(0.04);
	Fx = mix(Fx, TexAlbedo, TexMetallic);

	vec3 RadianceSum = vec3(0.0);

	vec3 LightDir = normalize(FragInput.TbnMatrix * (iLightArgs.LightPos  - FragInput.FragPos));
	vec3 ViewDir  = normalize(FragInput.TbnMatrix * (iLightArgs.CameraPos - FragInput.FragPos));
	vec3 HalfDir  = normalize(LightDir + ViewDir);

	float ViewAngle         = dot(ViewDir, Normal);
	float AdjustedRoughness = mix(TexRoughness, max(TexRoughness, 0.3), 1.0 - smoothstep(0.0, 0.2, ViewAngle));

	float Distance    = length(iLightArgs.LightPos - FragInput.FragPos);
	float Attenuation = 1.0 / pow(Distance, 2);
	vec3  Radiance    = iLightArgs.LightColor * Attenuation;

	float NormalDistFunc  = TrowbridgeReitzGGX(Normal, HalfDir, AdjustedRoughness);
	float GemoertyFunc    = GeometrySmith(Normal, ViewDir, LightDir, AdjustedRoughness);
	vec3  FresnelEquation = FresnelSchlick(clamp(dot(HalfDir, ViewDir), 0.0, 1.0), Fx);

	vec3  Numerator        = NormalDistFunc * GemoertyFunc * FresnelEquation;
	float Denominator      = 4.0 * max(dot(ViewDir, Normal), 0.01) * max(dot(LightDir, Normal), 0.01);
	vec3  CookTorranceSpec = Numerator / (Denominator + 1e-3);

	vec3 Specular = FresnelEquation;
	vec3 Diffuse  = vec3(1.0) - Specular;
	Diffuse      *= 1.0 - TexMetallic;

	float NdotL = max(dot(Normal, LightDir), 0.0);
	RadianceSum += (Diffuse * TexAlbedo / kPi + CookTorranceSpec) * Radiance * NdotL;

	vec3 Ambient = vec3(0.03) * TexAlbedo * TexAo;
	vec3 Color   = Ambient + RadianceSum;

	FragColor = vec4(Color, 1.0);
}
