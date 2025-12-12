#include "stdafx.h"
#include "MaterialTemplate.hpp"

namespace Npgs
{
    IMaterialTemplate::IMaterialTemplate(FVulkanContext* VulkanContext)
        : VulkanContext_(VulkanContext)
    {
    }

    void IMaterialTemplate::Setup()
    {
    }
}
