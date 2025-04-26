#include "ResourceServices.h"

namespace Npgs
{
    FResourceServices::FResourceServices(const FCoreServices& CoreServices)
        : _CoreServices(&CoreServices)
        , _ImageTracker(std::make_unique<FImageTracker>())
        , _PipelineManager(std::make_unique<FPipelineManager>(_CoreServices->GetVulkanContext(), _CoreServices->GetAssetManager()))
        , _ShaderBufferManager(std::make_unique<FShaderBufferManager>(_CoreServices->GetVulkanContext()))
    {
    }
} // namespace Npgs
