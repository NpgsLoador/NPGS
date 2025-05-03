#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE vk::PipelineLayout FPipelineManager::GetPipelineLayout(const std::string& Name) const
    {
        return *PipelineLayouts_.at(Name);
    }

    NPGS_INLINE vk::Pipeline FPipelineManager::GetPipeline(const std::string& Name) const
    {
        return *Pipelines_.at(Name);
    }
} // namespace Npgs
