#include "ResourceServices.h"

#include <utility>

namespace Npgs
{
    NPGS_INLINE FImageTracker* FResourceServices::GetImageTracker() const
    {
        return _ImageTracker.get();
    }

    NPGS_INLINE FPipelineManager* FResourceServices::GetPipelineManager() const
    {
        return _PipelineManager.get();
    }

    NPGS_INLINE FShaderBufferManager* FResourceServices::GetShaderBufferManager() const
    {
        return _ShaderBufferManager.get();
    }
} // namespace Npgs
