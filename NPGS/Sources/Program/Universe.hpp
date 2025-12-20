#pragma once

#include <cstddef>
#include <memory>
#include <random>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/Core/Math/Random.hpp"
#include "Engine/Core/Types/Entries/Astro/Star.hpp"
#include "Engine/Core/Types/Entries/Astro/OrbitalSystem.hpp"
#include "Engine/Runtime/Pools/ThreadPool.hpp"
#include "Engine/System/Generators/StellarGenerator.hpp"
#include "Engine/System/Spatial/Octree.hpp"

namespace Npgs
{
    class FUniverse
    {
    public:
        FUniverse() = delete;
        FUniverse(std::uint32_t Seed, std::size_t StarCount, std::size_t ExtraGiantCount = 0, std::size_t ExtraMassiveStarCount = 0,
                  std::size_t ExtraNeutronStarCount = 0, std::size_t ExtraBlackHoleCount = 0, std::size_t ExtraMergeStarCount   = 0,
                  float UniverseAge = 1.38e10f);

        ~FUniverse() = default;

        void FillUniverse();
        void ReplaceStar(std::size_t DistanceRank, const Astro::AStar& StarData);
        void CountStars();

    private:
        void GenerateStars(int MaxThread);
        void FillStellarSystem(int MaxThread);

        std::vector<Astro::AStar> InterpolateStars(int MaxThread, std::vector<FStellarGenerator>& Generators,
                                                   std::vector<FStellarBasicProperties>& BasicProperties);

        void GenerateSlots(float MinDistance, std::size_t SampleCount, float Density);
        void OctreeLinkToStellarSystems(std::vector<Astro::AStar>& Stars, std::vector<glm::vec3>& Slots);
        void GenerateBinaryStars(int MaxThread);

    private:
        using FNodeType = TOctree<Astro::FOrbitalSystem>::FNodeType;

    private:
        std::mt19937                                    RandomEngine_;
        std::vector<Astro::FOrbitalSystem>              OrbitalSystems_;
        Math::TUniformIntDistribution<std::uint32_t>    SeedGenerator_;
        Math::TUniformRealDistribution<>                CommonGenerator_;
        std::unique_ptr<TOctree<Astro::FOrbitalSystem>> Octree_;
        FThreadPool*                                    ThreadPool_;

        std::size_t StarCount_;
        std::size_t ExtraGiantCount_;
        std::size_t ExtraMassiveStarCount_;
        std::size_t ExtraNeutronStarCount_;
        std::size_t ExtraBlackHoleCount_;
        std::size_t ExtraMergeStarCount_;
        float       UniverseAge_;
    };
} // namespace Npgs
