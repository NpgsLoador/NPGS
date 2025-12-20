#include "stdafx.h"
#include "ResourceServices.hpp"

namespace Npgs
{
    FResourceServices::FResourceServices(const FCoreServices& CoreServices)
        : CoreServices_(&CoreServices)
        , ImageTracker_(std::make_unique<FImageTracker>(CoreServices_->GetVulkanContext()))
        , ShaderBufferManager_(std::make_unique<FShaderBufferManager>(CoreServices_->GetVulkanContext()))
        , ShaderManager_(std::make_unique<FShaderManager>(CoreServices_->GetVulkanContext(), CoreServices_->GetAssetManager()))
    {
    }
} // namespace Npgs
