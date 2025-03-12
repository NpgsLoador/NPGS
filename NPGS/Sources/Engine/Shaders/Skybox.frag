#version 450
#pragma shader_stage(fragment)

#include "Common/TransferFunctions.glsl"

layout(location = 0) in  vec3 TexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 1, binding = 0) uniform samplerCube iSkyboxTex;

void main()
{
    vec3 SkyboxColor = texture(iSkyboxTex, TexCoord).rgb;
    vec3 BrightColor = (kSrgbToBt2020 * SkyboxColor) * 6.0;
    FragColor = vec4(BrightColor, 1.0);
}
