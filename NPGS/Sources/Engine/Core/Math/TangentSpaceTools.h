#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Buffers/BufferStructs.h"

_NPGS_BEGIN
_MATH_BEGIN

void CalculateTangentBitangent(std::vector<Runtime::Graphics::FVertex>& Vertices, std::size_t Index);

void CalculateTangentBitangentWithIndices(std::vector<Runtime::Graphics::FVertex>& Vertices,
                                          const std::vector<std::uint32_t> Indices);

void CalculateTangentBitangentSphere(std::vector<Runtime::Graphics::FVertex>& Vertices,
                                     std::uint32_t SegmentsX, std::uint32_t SegmentsY);

void CalculateAllTangents(std::vector<Runtime::Graphics::FVertex>& Vertices);

_MATH_END
_NPGS_END
