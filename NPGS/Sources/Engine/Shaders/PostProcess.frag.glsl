#version 450
#pragma shader_stage(fragment)

#include "Common/TransferFunctions.glsl"

layout(location = 0) in  vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D iTexture;

void main()
{
    FragColor = vec4(ScRgbToPq(texture(iTexture, TexCoord).rgb), 1.0);
}
