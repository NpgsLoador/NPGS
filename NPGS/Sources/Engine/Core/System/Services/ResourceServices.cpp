#include "stdafx.h"
#include "ResourceServices.hpp"

namespace Npgs
{
    FResourceServices::FResourceServices(const FCoreServices& CoreServices)
        : CoreServices_(&CoreServices)
        , ImageTracker_(std::make_unique<FImageTracker>())
        , PipelineManager_(std::make_unique<FPipelineManager>(CoreServices_->GetVulkanContext(), CoreServices_->GetAssetManager()))
        , ShaderBufferManager_(std::make_unique<FShaderBufferManager>(CoreServices_->GetVulkanContext()))
    {
    }
} // namespace Npgs
