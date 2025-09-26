#pragma once

#include <array>
#include <memory>
#include <random>

#include "Engine/Core/Types/Entries/Astro/Planet.hpp"
#include "Engine/Core/Types/Entries/Astro/Star.hpp"
#include "Engine/Utils/Random.hpp"

namespace Npgs
{
    class FCivilizationGenerator
    {
    public:
        struct FGenerationInfo
        {
            const std::seed_seq* SeedSequence{ nullptr };
            float                LifeOccurrenceProbability{ 0.0f };
            bool                 bEnableAsiFilter{ false };
            float                DestroyedByDisasterProbability{ 0.001f };
        };

    public:
        FCivilizationGenerator() = delete;
        FCivilizationGenerator(const FGenerationInfo& GenerationInfo);
        FCivilizationGenerator(const FCivilizationGenerator& Other);
        FCivilizationGenerator(FCivilizationGenerator&& Other) noexcept;
        ~FCivilizationGenerator() = default;

        FCivilizationGenerator& operator=(const FCivilizationGenerator& Other);
        FCivilizationGenerator& operator=(FCivilizationGenerator&& Other) noexcept;

        void GenerateCivilization(const Astro::AStar* Star, float PoyntingVector, Astro::APlanet* Planet);

    private:
        void GenerateLife(double StarAge, float PoyntingVector, Astro::APlanet* Planet);
        void GenerateCivilizationDetails(const Astro::AStar* Star, float PoyntingVector, Astro::APlanet* Planet);

    private:
        std::mt19937                     RandomEngine_;
        Util::TUniformRealDistribution<> CommonGenerator_;
        Util::TBernoulliDistribution<>   AsiFiltedProbability_;
        Util::TBernoulliDistribution<>   DestroyedByDisasterProbability_;
        Util::TBernoulliDistribution<>   LifeOccurrenceProbability_;

        static const std::array<float, 7> kProbabilityListForCenoziocEra_;
        static const std::array<float, 7> kProbabilityListForSatTeeTouyButAsi_;
    };
} // namespace Npgs
