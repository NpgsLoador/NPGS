#pragma once

#include "Engine/Runtime/Graphics/Renderer/RenderPass.hpp"

namespace Npgs
{
    class FDepthMap : public IRenderPass
    {
    private:
        void BindDescriptors() override;
        void DeclareAttachments() override;
    };
} // namespace Npgs
