#pragma once

#include <string>

namespace Npgs::RenderPasses::GbufferScene
{
    inline const std::string kShaderName           = "GbufferSceneShader";
    inline const std::string kPipelineName         = "GbufferScenePipeline";
    inline const std::string kPositionAoName       = "PositionAo";
    inline const std::string kNormalRoughName      = "NormalRough";
    inline const std::string kAlbedoMetalName      = "AlbedoMetal";
    inline const std::string kShadowName           = "Shadow";
    inline const std::string kDescriptorBufferName = "GbufferSceneDescriptorBuffer";
}
