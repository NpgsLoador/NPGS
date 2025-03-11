#version 450
#pragma shader_stage(fragment)

layout(location = 0) in  vec3 TexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 1, binding = 0) uniform samplerCube iSkyboxTex;

void main()
{
	vec3 SkyboxColor = texture(iSkyboxTex, TexCoord).rgb;
	vec3 BrightColor = SkyboxColor * 1.0;
	FragColor = vec4(BrightColor, 1.0);
}
