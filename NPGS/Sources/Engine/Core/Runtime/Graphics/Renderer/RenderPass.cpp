#include "stdafx.h"
#include "RenderPass.hpp"

namespace Npgs
{
    void IRenderPass::Setup()
    {
        LoadShaders();
        SetupPipeline();
        BindDescriptors();
        DeclareAttachments();
    }
} // namespace Npgs
