#pragma once

#include <glm/glm.hpp>

namespace Npgs
{
    struct FPlane
    {
        glm::vec3 Normal{};
        float     Distance{};
    };
} // namespace Npgs;
