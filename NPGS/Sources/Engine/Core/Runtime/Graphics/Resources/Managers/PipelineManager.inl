#include "PipelineManager.h"

#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE vk::PipelineLayout FPipelineManager::GetPipelineLayout(const std::string& Name) const
    {
        return *_PipelineLayouts.at(Name);
    }

    NPGS_INLINE vk::Pipeline FPipelineManager::GetPipeline(const std::string& Name) const
    {
        return *_Pipelines.at(Name);
    }
} // namespace Npgs
