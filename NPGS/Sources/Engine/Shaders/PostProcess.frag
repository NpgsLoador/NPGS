#version 450
#pragma shader_stage(fragment)

#include "Common/TransferFunctions.glsl"

layout(location = 0) in  vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D iTexture;

layout(push_constant) uniform HdrArgs
{
    bool bEnableHdr;
} iHdrArgs;

void main()
{
    vec3 FrameColor = texture(iTexture, TexCoord).rgb;
    if (iHdrArgs.bEnableHdr)
    {
        FrameColor = ScRgbToPqWithGamut(FrameColor);
    }
    else
    {
        FrameColor = FrameColor / (FrameColor + vec3(1.0));
    }

    FragColor = vec4(FrameColor, 1.0);
}
