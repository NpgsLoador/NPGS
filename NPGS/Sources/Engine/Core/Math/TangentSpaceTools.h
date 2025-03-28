#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/Graphics/Buffers/BufferStructs.h"

_NPGS_BEGIN
_MATH_BEGIN

void CalculateTangentBitangent(std::size_t Index, std::vector<Runtime::Graphics::FVertex>& Vertices);
void CalculateTangentBitangent(const std::vector<std::uint32_t>& Indices, std::vector<Runtime::Graphics::FVertex>& Vertices);
void CalculateTangentBitangent(std::uint32_t SegmentsX, std::uint32_t SegmentsY, std::vector<Runtime::Graphics::FVertex>& Vertices);
void CalculateAllTangents(std::vector<Runtime::Graphics::FVertex>& Vertices);

_MATH_END
_NPGS_END
