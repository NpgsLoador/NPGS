#version 450
#pragma shader_stage(compute)
#extension GL_EXT_samplerless_texture_functions : require

#include "Common/BindlessExtensions.glsl"
#include "Common/NumericConstants.glsl"

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(push_constant) uniform _DeviceAddress
{
	uint64_t LightArgsAddress;
} DeviceAddress;

layout(buffer_reference, scalar) readonly buffer _LightArgs
{
	vec3 LightPos;
	vec3 LightColor;
	vec3 CameraPos;
};

layout(set = 2, binding = 0) uniform texture2D iRgbPositionAAo;
layout(set = 2, binding = 1) uniform texture2D iRgbNormalARough;
layout(set = 2, binding = 2) uniform texture2D iRgbAlbedoAMetal;
layout(set = 2, binding = 3) uniform texture2D iRShadowGbaNull;
layout(set = 1, binding = 0, rgba16f) uniform writeonly image2D iStorageImage;

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
	ivec2 TexelCoord = ivec2(gl_GlobalInvocationID.xy);

	vec4 RgbPositionAAo  = texelFetch(iRgbPositionAAo,  TexelCoord, 0);
	vec4 RgbNormalARough = texelFetch(iRgbNormalARough, TexelCoord, 0);
	vec4 RgbAlbedoAMetal = texelFetch(iRgbAlbedoAMetal, TexelCoord, 0);

	vec3  TexFragPos   = RgbPositionAAo.rgb;
	float TexAo        = RgbPositionAAo.a;
	vec3  TexNormal    = RgbNormalARough.rgb;
	float TexRoughness = RgbNormalARough.a;
	vec3  TexAlbedo    = RgbAlbedoAMetal.rgb;
	float TexMetallic  = RgbAlbedoAMetal.a;

	vec3 Fx = vec3(0.04);
	Fx = mix(Fx, TexAlbedo, TexMetallic);

	vec3 RadianceSum = vec3(0.0);

	_LightArgs LightArgs = _LightArgs(DeviceAddress.LightArgsAddress);

	vec3 LightDir = normalize(LightArgs.LightPos  - TexFragPos);
	vec3 ViewDir  = normalize(LightArgs.CameraPos - TexFragPos);
	vec3 HalfDir  = normalize(LightDir + ViewDir);

	float ViewAngle         = dot(ViewDir, TexNormal);
	float RoughnessTarget   = mix(0.3, 0.1, TexMetallic);
	float AdjustedRoughness = mix(TexRoughness, max(TexRoughness, RoughnessTarget), 1.0 - smoothstep(0.0, 0.2, ViewAngle));

	float Distance    = length(LightArgs.LightPos - TexFragPos);
	float Attenuation = 1.0 / pow(Distance, 2);
	vec3  Radiance    = LightArgs.LightColor * Attenuation;

	float NormalDistFunc  = TrowbridgeReitzGGX(TexNormal, HalfDir, AdjustedRoughness);
	float GeometryFunc    = GeometrySmith(TexNormal, ViewDir, LightDir, AdjustedRoughness);
	vec3  FresnelEquation = FresnelSchlick(clamp(dot(HalfDir, ViewDir), 0.0, 1.0), Fx);

	vec3  Numerator        = NormalDistFunc * GeometryFunc * FresnelEquation;
	float Denominator      = 4.0 * max(dot(ViewDir, TexNormal), 0.01) * max(dot(LightDir, TexNormal), 0.01);
	vec3  CookTorranceSpec = Numerator / (Denominator + 1e-3);

	vec3 Specular = FresnelEquation;
	vec3 Diffuse  = vec3(1.0) - Specular;
	Diffuse      *= 1.0 - TexMetallic;

	float NdotL  = max(dot(TexNormal, LightDir), 0.0);
	RadianceSum += (Diffuse * TexAlbedo / kPi + CookTorranceSpec) * Radiance * NdotL;

	float Shadow = texelFetch(iRShadowGbaNull, TexelCoord, 0).r;

	vec3 Ambient = vec3(0.2) * TexAlbedo * TexAo;
	vec3 Color   = Ambient + (1.1 - Shadow) * RadianceSum;

	imageStore(iStorageImage, TexelCoord, vec4(Color, 1.0));
}
