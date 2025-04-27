#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE FImageTracker* FResourceServices::GetImageTracker() const
    {
        return ImageTracker_.get();
    }

    NPGS_INLINE FPipelineManager* FResourceServices::GetPipelineManager() const
    {
        return PipelineManager_.get();
    }

    NPGS_INLINE FShaderBufferManager* FResourceServices::GetShaderBufferManager() const
    {
        return ShaderBufferManager_.get();
    }
} // namespace Npgs
