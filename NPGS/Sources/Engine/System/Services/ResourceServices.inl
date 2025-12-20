#include "ResourceServices.hpp"
#include "Engine/Core/Base/Base.hpp"

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

    NPGS_INLINE FRenderTargetManager* FResourceServices::GetRenderTargetManager() const
    {
        return RenderTargetManager_.get();
    }

    NPGS_INLINE FShaderBufferManager* FResourceServices::GetShaderBufferManager() const
    {
        return ShaderBufferManager_.get();
    }

    NPGS_INLINE FShaderManager* FResourceServices::GetShaderManager() const
    {
        return ShaderManager_.get();
    }
} // namespace Npgs
