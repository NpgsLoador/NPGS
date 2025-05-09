#pragma once

#include <cstddef>
#include <array>
#include <memory>
#include <random>
#include <tuple>
#include <vector>

#include "Engine/Core/System/Generators/CivilizationGenerator.h"
#include "Engine/Core/Types/Entries/Astro/Planet.h"
#include "Engine/Core/Types/Entries/Astro/StellarSystem.h"
#include "Engine/Utils/Random.hpp"

namespace Npgs
{
    class FOrbitalGenerator
    {
    public:
        struct FGenerationInfo
        {
            const std::seed_seq* SeedSequence{ nullptr };
            float UniverseAge{ 1.38e10f };
            float BinaryPeriodMean{ 5.03f };
            float BinaryPeriodSigma{ 2.28f };
            float CoilTemperatureLimit{ 1514.114f };
            float AsteroidUpperLimit{ 1e21f };
            float RingsParentLowerLimit{ 1e23f };
            float LifeOccurrenceProbability{ 0.0114514f };
            bool  bContainUltravioletHabitableZone{ false };
            bool  bEnableAsiFilter{ true };
        };

    private:
        struct FPlanetaryDisk
        {
            float InnerRadiusAu{};
            float OuterRadiusAu{};
            float DiskMassSol{};
            float DustMassSol{};
        };

    public:
        FOrbitalGenerator() = delete;
        FOrbitalGenerator(const FGenerationInfo& GenerationInfo);
        FOrbitalGenerator(const FOrbitalGenerator& Other);
        FOrbitalGenerator(FOrbitalGenerator&& Other) noexcept;
        ~FOrbitalGenerator() = default;

        FOrbitalGenerator& operator=(const FOrbitalGenerator& Other);
        FOrbitalGenerator& operator=(FOrbitalGenerator&& Other) noexcept;

        void GenerateOrbitals(Astro::FStellarSystem& System);

    private:
        void GenerateBinaryOrbit(Astro::FStellarSystem& System);
        void GeneratePlanets(std::size_t StarIndex, Astro::FOrbit::FOrbitalDetails& ParentStar, Astro::FStellarSystem& System);
        void GenerateOrbitElements(Astro::FOrbit& Orbit);

        std::size_t JudgeLargePlanets(std::size_t StarIndex, const std::vector<std::unique_ptr<Astro::AStar>>& StarData,
                                      float BinarySemiMajorAxis, float InnerHabitableZoneRadiusAu, float FrostLineAu,
                                      std::vector<float>& CoreMassesSol, std::vector<float>& NewCoreMassesSol,
                                      std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits,
                                      std::vector<std::unique_ptr<Astro::APlanet>>& Planets);

        float CalculatePlanetMass(float CoreMass, float NewCoreMass, float SemiMajorAxisAu,
                                  const FPlanetaryDisk& PlanetaryDiskTempData, const Astro::AStar* Star, Astro::APlanet* Planet);

        void CalculatePlanetRadius(float MassEarth, Astro::APlanet* Planet);
        void GenerateSpin(float SemiMajorAxis, const Astro::FOrbit::FOrbitalObject& Parent, Astro::APlanet* Planet);
        void CalculateTemperature(const Astro::FOrbit::EObjectType ParentType, float PoyntingVector, Astro::APlanet* Planet);

        void GenerateMoons(std::size_t PlanetIndex, float FrostLineAu, const Astro::AStar* Star, float PoyntingVector,
                           const std::pair<float, float>& HabitableZoneAu, Astro::FOrbit::FOrbitalDetails& ParentPlanet,
                           std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits, std::vector<std::unique_ptr<Astro::APlanet>>& Planets);

        void GenerateRings(std::size_t PlanetIndex, float FrostLineAu, const Astro::AStar* Star,
                           Astro::FOrbit::FOrbitalDetails& ParentPlanet, std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits,
                           std::vector<std::unique_ptr<Astro::AAsteroidCluster>>& AsteroidClusters);

        void GenerateTerra(const Astro::AStar* Star, float PoyntingVector, const std::pair<float, float>& HabitableZoneAu,
                           const Astro::FOrbit* Orbit, Astro::APlanet* Planet);

        void GenerateTrojan(const Astro::AStar* Star, float FrostLineAu, Astro::FOrbit* Orbit,
                            Astro::FOrbit::FOrbitalDetails& ParentPlanet,
                            std::vector<std::unique_ptr<Astro::AAsteroidCluster>>& AsteroidClusters);

        void GenerateCivilization(const Astro::AStar* Star, float PoyntingVector, const std::pair<float, float>& HabitableZoneAu,
                                  const Astro::FOrbit* Orbit, Astro::APlanet* Planet);

        void CalculateOrbitalPeriods(std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits);

    private:
        std::mt19937                                  RandomEngine_;
        std::array<Util::TBernoulliDistribution<>, 2> RingsProbabilities_;
        Util::TNormalDistribution<>                   BinaryPeriodDistribution_;
        Util::TUniformRealDistribution<>              CommonGenerator_;
        Util::TBernoulliDistribution<>                AsteroidBeltProbability_;
        Util::TBernoulliDistribution<>                MigrationProbability_;
        Util::TBernoulliDistribution<>                ScatteringProbability_;
        Util::TBernoulliDistribution<>                WalkInProbability_;

        std::unique_ptr<FCivilizationGenerator>       CivilizationGenerator_;

        float                                         AsteroidUpperLimit_;
        float                                         CoilTemperatureLimit_;
        float                                         RingsParentLowerLimit_;
        float                                         UniverseAge_;
        bool                                          bContainUltravioletHabitableZone_;
    };
} // namespace Npgs
