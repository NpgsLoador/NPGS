#include "PipelineManager.hpp"
#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    NPGS_INLINE void FPipelineManager::RemovePipeline(std::string_view Name)
    {
        Pipelines_.erase(Name);
    }

    NPGS_INLINE vk::PipelineLayout FPipelineManager::GetPipelineLayout(const std::string& Name) const
    {
        return *PipelineLayouts_.at(Name);
    }

    NPGS_INLINE vk::Pipeline FPipelineManager::GetPipeline(const std::string& Name) const
    {
        return *Pipelines_.at(Name);
    }
} // namespace Npgs
