#pragma once

#include <string>
#include "Engine/Core/Runtime/Graphics/Renderer/RenderPass.hpp"

namespace Npgs
{
    class FGbufferScene : public IRenderPass
    {
    private:
        void LoadShaders() override;
        void SetupPipeline() override;
        void BindDescriptors() override;
        void DeclareAttachments() override;
    };
} // namespace Npgs
