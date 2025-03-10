#include "TangentSpaceTools.h"

#include "Engine/Core/Math/NumericConstants.h"

_NPGS_BEGIN
_MATH_BEGIN

void CalculateTangentBitangent(std::vector<Runtime::Graphics::FVertex>& Vertices, std::size_t Index)
{
    const auto& Vertex0 = Vertices[Index + 0];
    const auto& Vertex1 = Vertices[Index + 1];
    const auto& Vertex2 = Vertices[Index + 2];

    const glm::vec3 Edge1 = Vertex1.Position - Vertex0.Position;
    const glm::vec3 Edge2 = Vertex2.Position - Vertex0.Position;

    const glm::vec2 DeltaUv1 = Vertex1.TexCoord - Vertex0.TexCoord;
    const glm::vec2 DeltaUv2 = Vertex2.TexCoord - Vertex0.TexCoord;

    const float Factor = 1.0f / (DeltaUv1.x * DeltaUv2.y - DeltaUv2.x * DeltaUv1.y);
    const glm::vec3 Tangent   = glm::normalize(Factor * ( DeltaUv2.y * Edge1 - DeltaUv1.y * Edge2));
    const glm::vec3 Bitangent = glm::normalize(Factor * (-DeltaUv2.x * Edge1 + DeltaUv1.x * Edge2));

    Vertices[Index + 0].Tangent += Tangent;
    Vertices[Index + 1].Tangent += Tangent;
    Vertices[Index + 2].Tangent += Tangent;

    Vertices[Index + 0].Bitangent += Bitangent;
    Vertices[Index + 1].Bitangent += Bitangent;
    Vertices[Index + 2].Bitangent += Bitangent;
}

void CalculateTangentBitangentWithIndices(std::vector<Runtime::Graphics::FVertex>& Vertices,
                                          const std::vector<std::uint32_t> Indices)
{
    for (std::size_t i = 0; i != Indices.size(); ++i)
    {
        const auto& Vertex0 = Vertices[Indices[i + 0]];
        const auto& Vertex1 = Vertices[Indices[i + 1]];
        const auto& Vertex2 = Vertices[Indices[i + 2]];

        const glm::vec3 Edge1 = Vertex1.Position - Vertex0.Position;
        const glm::vec3 Edge2 = Vertex2.Position - Vertex0.Position;

        const glm::vec2 DeltaUv1 = Vertex1.TexCoord - Vertex0.TexCoord;
        const glm::vec2 DeltaUv2 = Vertex2.TexCoord - Vertex0.TexCoord;

        const float Factor = 1.0f / (DeltaUv1.x * DeltaUv2.y - DeltaUv2.x * DeltaUv1.y);
        const glm::vec3 Tangent   = glm::normalize(Factor * ( DeltaUv2.y * Edge1 - DeltaUv1.y * Edge2));
        const glm::vec3 Bitangent = glm::normalize(Factor * (-DeltaUv2.x * Edge1 + DeltaUv1.x * Edge2));

        Vertices[Indices[i + 0]].Tangent += Tangent;
        Vertices[Indices[i + 1]].Tangent += Tangent;
        Vertices[Indices[i + 2]].Tangent += Tangent;

        Vertices[Indices[i + 0]].Bitangent += Bitangent;
        Vertices[Indices[i + 1]].Bitangent += Bitangent;
        Vertices[Indices[i + 2]].Bitangent += Bitangent;
    }

    for (auto& Vertex : Vertices)
    {
        Vertex.Tangent   = glm::normalize(Vertex.Tangent);
        Vertex.Bitangent = glm::normalize(Vertex.Bitangent);
    }
}

void CalculateTangentBitangentSphere(std::vector<Runtime::Graphics::FVertex>& Vertices,
                                     std::uint32_t SegmentsX, std::uint32_t SegmentsY)
{
    for (std::uint32_t y = 0; y <= SegmentsY; ++y)
    {
        // θ ∈ [0, π]
        float Theta = static_cast<float>(y) / static_cast<float>(SegmentsY) * kPi;

        for (std::uint32_t x = 0; x <= SegmentsX; ++x)
        {
            // φ ∈ [0, 2π] 
            float Phi = static_cast<float>(x) / static_cast<float>(SegmentsX) * 2.0f * kPi;

            std::size_t Index = y * (SegmentsX + 1) + x;

            // 球面上一点的位置向量就是其法线方向
            // Normal = normalize(P)
            glm::vec3 Normal = glm::normalize(Vertices[Index].Position);

            // 切线方向是 φ 方向的偏导数
            // T = normalize(∂P/∂φ) = normalize((-sin(φ)×sin(θ), 0, cos(φ)×sin(θ)))
            glm::vec3 Tangent(-std::sin(Phi) * std::sin(Theta), 0, std::cos(Phi) * std::sin(Theta));
            Tangent = glm::normalize(Tangent);

            // 副切线方向是 θ 方向的偏导数
            // B = normalize(∂P/∂θ) = normalize((cos(φ)×cos(θ), -sin(θ), sin(φ)×cos(θ)))
            glm::vec3 Bitangent(std::cos(Phi) * std::cos(Theta), -std::sin(Theta), std::sin(Phi) * std::cos(Theta));
            Bitangent = glm::normalize(Bitangent);

            // 确保 TBN 是右手坐标系
            // 检查 B 是否等于 N×T
            glm::vec3 CrossProduct = glm::cross(Normal, Tangent);
            if (glm::dot(CrossProduct, Bitangent) < 0.0f)
            {
                Tangent = -Tangent;
            }

            Vertices[Index].Normal    = Normal;
            Vertices[Index].Tangent   = Tangent;
            Vertices[Index].Bitangent = Bitangent;
        }
    }
}

void CalculateAllTangents(std::vector<Runtime::Graphics::FVertex>& Vertices)
{
    for (std::size_t i = 0; i < Vertices.size(); i += 3)
    {
        CalculateTangentBitangent(Vertices, i);
    }

    for (auto& Vertex : Vertices)
    {
        Vertex.Tangent   = glm::normalize(Vertex.Tangent);
        Vertex.Bitangent = glm::normalize(Vertex.Bitangent);
    }
}

_MATH_END
_NPGS_END
