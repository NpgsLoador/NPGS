#pragma once

#include <string>

namespace Npgs
{
    // Public resources
    namespace Public
    {
        namespace Samplers
        {
            inline const std::string kPbrTextureSamplerName  = "PbrTextureSampler";
            inline const std::string kFramebufferSamplerName = "FramebufferSampler";
        } // namespace Samplers

        namespace Attachments
        {
            inline const std::string kDepthMapAttachmentName = "DepthMapAttachment";
        } // namespace Attachments
    } // namespace Public

    // RenderPasses
    namespace RenderPasses
    {
        namespace DepthMap
        {
            inline const std::string kShaderName     = "DepthMapShader";
            inline const std::string kPipelineName   = "DepthMapPipeline";
        } // namespace DepthMap

        namespace GbufferScene
        {
            inline const std::string kShaderName      = "GbufferSceneShader";
            inline const std::string kPipelineName    = "GbufferScenePipeline";
            inline const std::string kPositionAoName  = "PositionAo";
            inline const std::string kNormalRoughName = "NormalRough";
            inline const std::string kAlbedoMetalName = "AlbedoMetal";
            inline const std::string kShadowName      = "Shadow";
        } // namespace GbufferScene
    } // namespace RenderPasses

    // Materials
    namespace Materials
    {
        namespace StandardPbr
        {
            inline const std::string kAlbedoName = "Albedo";
            inline const std::string kNormalName = "Normal";
            inline const std::string kArmName    = "AoRoughMetal";

            inline const std::string kDescriptorBufferName = "StandardPbrMaterialDescriptorBuffer";
        } // namespace StandardPbrMaterial
    } // namespace Materials
} // namespace Npgs
