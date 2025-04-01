#include "TangentSpaceTools.h"

#include <cmath>
#include <glm/glm.hpp>

namespace Npgs::Math
{
    void CalculateTangentBitangent(std::size_t Index, std::vector<FVertex>& Vertices)
    {
        const auto& Vertex0 = Vertices[Index + 0];
        const auto& Vertex1 = Vertices[Index + 1];
        const auto& Vertex2 = Vertices[Index + 2];

        glm::vec3 Edge1 = Vertex1.Position - Vertex0.Position;
        glm::vec3 Edge2 = Vertex2.Position - Vertex0.Position;

        glm::vec2 DeltaUv1 = Vertex1.TexCoord - Vertex0.TexCoord;
        glm::vec2 DeltaUv2 = Vertex2.TexCoord - Vertex0.TexCoord;

        float Factor = 1.0f / (DeltaUv1.x * DeltaUv2.y - DeltaUv2.x * DeltaUv1.y);
        glm::vec3 Tangent   = glm::normalize(Factor * ( DeltaUv2.y * Edge1 - DeltaUv1.y * Edge2));
        glm::vec3 Bitangent = glm::normalize(Factor * (-DeltaUv2.x * Edge1 + DeltaUv1.x * Edge2));

        Vertices[Index + 0].Tangent += Tangent;
        Vertices[Index + 1].Tangent += Tangent;
        Vertices[Index + 2].Tangent += Tangent;

        Vertices[Index + 0].Bitangent += Bitangent;
        Vertices[Index + 1].Bitangent += Bitangent;
        Vertices[Index + 2].Bitangent += Bitangent;
    }

    void CalculateTangentBitangent(const std::vector<std::uint32_t>& Indices, std::vector<FVertex>& Vertices)
    {
        for (std::size_t i = 0; i < Indices.size(); i += 3)
        {
            const auto& Vertex0 = Vertices[Indices[i + 0]];
            const auto& Vertex1 = Vertices[Indices[i + 1]];
            const auto& Vertex2 = Vertices[Indices[i + 2]];

            glm::vec3 Edge1 = Vertex1.Position - Vertex0.Position;
            glm::vec3 Edge2 = Vertex2.Position - Vertex0.Position;

            glm::vec2 DeltaUv1 = Vertex1.TexCoord - Vertex0.TexCoord;
            glm::vec2 DeltaUv2 = Vertex2.TexCoord - Vertex0.TexCoord;

            float Factor = 1.0f / (DeltaUv1.x * DeltaUv2.y - DeltaUv2.x * DeltaUv1.y);
            glm::vec3 Tangent   = glm::normalize(Factor * ( DeltaUv2.y * Edge1 - DeltaUv1.y * Edge2));
            glm::vec3 Bitangent = glm::normalize(Factor * (-DeltaUv2.x * Edge1 + DeltaUv1.x * Edge2));

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

    void CalculateTangentBitangent(std::uint32_t SegmentsX, std::uint32_t SegmentsY, std::vector<FVertex>& Vertices)
    {
        // 初始化切线和副切线向量为零
        for (auto& Vertex : Vertices)
        {
            Vertex.Tangent   = glm::vec3(0.0f);
            Vertex.Bitangent = glm::vec3(0.0f);
        }

        auto ProcessTriangle = [&](const std::vector<std::size_t>& Indices) -> void
        {
            glm::vec3 Vertex0 = Vertices[Indices[0]].Position;
            glm::vec3 Vertex1 = Vertices[Indices[1]].Position;
            glm::vec3 Vertex2 = Vertices[Indices[2]].Position;

            glm::vec2 TexCoord0 = Vertices[Indices[0]].TexCoord;
            glm::vec2 TexCoord1 = Vertices[Indices[1]].TexCoord;
            glm::vec2 TexCoord2 = Vertices[Indices[2]].TexCoord;

            // 计算边和纹理坐标差值
            glm::vec3 Edge1    = Vertex1 - Vertex0;
            glm::vec3 Edge2    = Vertex2 - Vertex0;
            glm::vec2 DeltaUv1 = TexCoord1 - TexCoord0;
            glm::vec2 DeltaUv2 = TexCoord2 - TexCoord0;

            // 计算切线和副切线
            float Factor = 1.0f / (DeltaUv1.x * DeltaUv2.y - DeltaUv2.x * DeltaUv1.y);
            if (std::isfinite(Factor))
            {
                glm::vec3 Tangent   = glm::normalize(Factor * ( DeltaUv2.y * Edge1 - DeltaUv1.y * Edge2));
                glm::vec3 Bitangent = glm::normalize(Factor * (-DeltaUv2.x * Edge1 + DeltaUv1.x * Edge2));

                // 累加到顶点上
                Vertices[Indices[0]].Tangent   += Tangent;
                Vertices[Indices[1]].Tangent   += Tangent;
                Vertices[Indices[2]].Tangent   += Tangent;
                Vertices[Indices[0]].Bitangent += Bitangent;
                Vertices[Indices[1]].Bitangent += Bitangent;
                Vertices[Indices[2]].Bitangent += Bitangent;
            }
        };

        // 计算每个网格面片的切线空间
        for (std::uint32_t y = 0; y < SegmentsY; ++y)
        {
            for (std::uint32_t x = 0; x < SegmentsX; ++x)
            {
                // 获取形成四边形的四个顶点索引
                std::size_t Index0 =  y      * (SegmentsX + 1) +  x;      // 左上
                std::size_t Index1 =  y      * (SegmentsX + 1) + (x + 1); // 右上
                std::size_t Index2 = (y + 1) * (SegmentsX + 1) +  x;      // 左下
                std::size_t Index3 = (y + 1) * (SegmentsX + 1) + (x + 1); // 右下

                // 处理第一个三角形 (Index0, Index1, Index2)
                ProcessTriangle({ Index0, Index1, Index2 });

                // 处理第二个三角形 (Index1, Index3, Index2)
                ProcessTriangle({ Index1, Index3, Index2 });
            }
        }

        // 归一化所有顶点的切线和副切线，并确保正交性
        for (auto& Vertex : Vertices)
        {
            // 首先确保 Normal 是归一化的
            Vertex.Normal = glm::normalize(Vertex.Position);

            // 归一化切线和副切线
            Vertex.Tangent   = glm::normalize(Vertex.Tangent);
            Vertex.Bitangent = glm::normalize(Vertex.Bitangent);

            // 使用 Gram-Schmidt 正交化过程确保切线与法线垂直
            Vertex.Tangent = glm::normalize(Vertex.Tangent - Vertex.Normal * glm::dot(Vertex.Normal, Vertex.Tangent));

            // 确保副切线与法线和切线都垂直（右手系）
            Vertex.Bitangent = glm::cross(Vertex.Normal, Vertex.Tangent);
        }
    }

    void CalculateAllTangents(std::vector<FVertex>& Vertices)
    {
        for (std::size_t i = 0; i < Vertices.size(); i += 3)
        {
            CalculateTangentBitangent(i, Vertices);
        }

        for (auto& Vertex : Vertices)
        {
            Vertex.Tangent   = glm::normalize(Vertex.Tangent);
            Vertex.Bitangent = glm::normalize(Vertex.Bitangent);
        }
    }
} // namespace Npgs::Math
