#pragma once

#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"

namespace Npgs
{
    class IMaterialTemplate
    {
    public:
        IMaterialTemplate(FVulkanContext* VulkanContext);
        virtual ~IMaterialTemplate() = default;

        void Setup();

    protected:
        virtual void LoadShaders()   = 0;
        virtual void SetupPipeline() = 0;

    protected:
        FVulkanContext* VulkanContext_;
    };
} // namespace Npgs
