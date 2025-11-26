#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Engine/Runtime/Graphics/Buffers/BufferStructs.hpp"

namespace Npgs::Math
{
    void CalculateTangentBitangent(std::size_t Index, std::vector<FVertex>& Vertices);
    void CalculateTangentBitangent(const std::vector<std::uint32_t>& Indices, std::vector<FVertex>& Vertices);
    void CalculateTangentBitangent(std::uint32_t SegmentsX, std::uint32_t SegmentsY, std::vector<FVertex>& Vertices);
    void CalculateAllTangents(std::vector<FVertex>& Vertices);
} // namespace Npgs::Math
