#pragma once

#include <array>
#include <memory>
#include <random>

#include "Engine/Core/Types/Entries/Astro/Planet.hpp"
#include "Engine/Core/Types/Entries/Astro/Star.hpp"
#include "Engine/Utils/Random.hpp"

namespace Npgs
{
    struct FCivilizationGenerationInfo
    {
        const std::seed_seq* SeedSequence{ nullptr };
        float                LifeOccurrenceProbability{ 0.0f };
        bool                 bEnableAsiFilter{ false };
        float                DestroyedByDisasterProbability{ 0.001f };
    };

    class FCivilizationGenerator
    {
    public:
        FCivilizationGenerator(const FCivilizationGenerationInfo& GenerationInfo);
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
        std::mt19937                      RandomEngine_;
        Utils::TUniformRealDistribution<> CommonGenerator_;
        Utils::TBernoulliDistribution<>   AsiFiltedProbability_;
        Utils::TBernoulliDistribution<>   DestroyedByDisasterProbability_;
        Utils::TBernoulliDistribution<>   LifeOccurrenceProbability_;

        static const std::array<float, 7> kProbabilityListForCenoziocEra_;
        static const std::array<float, 7> kProbabilityListForSatTeeTouyButAsi_;
    };
} // namespace Npgs
