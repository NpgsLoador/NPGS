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
} // namespace Npgs
