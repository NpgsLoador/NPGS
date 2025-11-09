#include "stdafx.h"
#include "OrbitalGenerator.hpp"

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <numeric>
#include <print>
#include <ranges>
#include <utility>

#include <boost/multiprecision/cpp_int.hpp>
#include <glm/glm.hpp>

#include "Engine/Core/Base/Assert.hpp"
#include "Engine/Core/Base/Base.hpp"
#include "Engine/Core/Math/NumericConstants.hpp"
#include "Engine/Core/Types/Properties/StellarClass.hpp"
#include "Engine/Utils/Utils.hpp"

#define DEBUG_OUTPUT // Temp

namespace Npgs
{
    // Tool functions
    // --------------
    namespace
    {
        float CalculatePrevMainSequenceLuminosity(float StarInitialMassSol)
        {
            float CommonCoefficient = (std::pow(10.0f, 2.0f - StarInitialMassSol) + 1.0f) * static_cast<float>(kSolarLuminosity);
            float Luminosity = 0.0f;

            if (StarInitialMassSol >= 0.075f && StarInitialMassSol < 0.43f)
            {
                Luminosity = CommonCoefficient * (0.23f * std::pow(StarInitialMassSol, 2.3f));
            }
            else if (StarInitialMassSol >= 0.43f && StarInitialMassSol < 2.0f)
            {
                Luminosity = CommonCoefficient * std::pow(StarInitialMassSol, 4.0f);
            }
            else if (StarInitialMassSol >= 2.0f && StarInitialMassSol <= 12.0f)
            {
                Luminosity = CommonCoefficient * (1.5f * std::pow(StarInitialMassSol, 3.5f));
            }

            return Luminosity;
        }

        std::unique_ptr<Astro::AAsteroidCluster> PlanetToAsteroidCluster(const Astro::APlanet* Planet)
        {
            Astro::AAsteroidCluster AsteroidCluster;

            if (Planet->GetPlanetType() == Astro::APlanet::EPlanetType::kRockyAsteroidCluster)
            {
                AsteroidCluster.SetAsteroidType(Astro::AAsteroidCluster::EAsteroidType::kRocky);
            }
            else
            {
                AsteroidCluster.SetAsteroidType(Astro::AAsteroidCluster::EAsteroidType::kRockyIce);
            }

            AsteroidCluster.SetMass(Planet->GetCoreMassStruct());
            AsteroidCluster.SetMassZ(Planet->GetCoreMassZ());
            AsteroidCluster.SetMassVolatiles(Planet->GetCoreMassVolatiles());
            AsteroidCluster.SetMassEnergeticNuclide(Planet->GetCoreMassEnergeticNuclide());

            return std::make_unique<Astro::AAsteroidCluster>(AsteroidCluster);
        }
    }

    // OrbitalGenerator implementations
    // --------------------------------
    FOrbitalGenerator::FOrbitalGenerator(const FOrbitalGenerationInfo& GenerationInfo)
        : RandomEngine_(*GenerationInfo.SeedSequence)
        , RingsProbabilities_{ Utils::TBernoulliDistribution<>(0.5), Utils::TBernoulliDistribution<>(0.2) }
        , BinaryPeriodDistribution_(GenerationInfo.BinaryPeriodMean, GenerationInfo.BinaryPeriodSigma)
        , CommonGenerator_(0.0f, 1.0f)
        , AsteroidBeltProbability_(0.4)
        , MigrationProbability_(0.1)
        , ScatteringProbability_(0.15)
        , WalkInProbability_(0.8)
        , CivilizationGenerator_(nullptr)
        , AsteroidUpperLimit_(GenerationInfo.AsteroidUpperLimit)
        , CoilTemperatureLimit_(GenerationInfo.CoilTemperatureLimit)
        , RingsParentLowerLimit_(GenerationInfo.RingsParentLowerLimit)
        , UniverseAge_(GenerationInfo.UniverseAge)
        , bContainUltravioletHabitableZone_(GenerationInfo.bContainUltravioletHabitableZone)
    {
        std::vector<std::uint32_t> Seeds((*GenerationInfo.SeedSequence).size());
        GenerationInfo.SeedSequence->param(Seeds.begin());
        std::ranges::shuffle(Seeds, RandomEngine_);
        std::seed_seq ShuffledSeeds(Seeds.begin(), Seeds.end());

        FCivilizationGenerationInfo CivilizationGenerationInfo
        {
            .SeedSequence              = &ShuffledSeeds,
            .LifeOccurrenceProbability = GenerationInfo.LifeOccurrenceProbability,
            .bEnableAsiFilter          = GenerationInfo.bEnableAsiFilter
        };

        CivilizationGenerator_ = std::make_unique<FCivilizationGenerator>(CivilizationGenerationInfo);
    }

    FOrbitalGenerator::FOrbitalGenerator(const FOrbitalGenerator& Other)
        : RandomEngine_(Other.RandomEngine_)
        , RingsProbabilities_{ Other.RingsProbabilities_[0], Other.RingsProbabilities_[1] }
        , BinaryPeriodDistribution_(Other.BinaryPeriodDistribution_)
        , CommonGenerator_(Other.CommonGenerator_)
        , AsteroidBeltProbability_(Other.AsteroidBeltProbability_)
        , MigrationProbability_(Other.MigrationProbability_)
        , ScatteringProbability_(Other.ScatteringProbability_)
        , WalkInProbability_(Other.WalkInProbability_)
        , AsteroidUpperLimit_(Other.AsteroidUpperLimit_)
        , CoilTemperatureLimit_(Other.CoilTemperatureLimit_)
        , RingsParentLowerLimit_(Other.RingsParentLowerLimit_)
        , UniverseAge_(Other.UniverseAge_)
        , bContainUltravioletHabitableZone_(Other.bContainUltravioletHabitableZone_)
    {
        if (Other.CivilizationGenerator_ != nullptr)
        {
            CivilizationGenerator_ = std::make_unique<FCivilizationGenerator>(*Other.CivilizationGenerator_);
        }
    }

    FOrbitalGenerator::FOrbitalGenerator(FOrbitalGenerator&& Other) noexcept
        : RandomEngine_(std::move(Other.RandomEngine_))
        , RingsProbabilities_(std::move(Other.RingsProbabilities_))
        , BinaryPeriodDistribution_(std::move(Other.BinaryPeriodDistribution_))
        , CommonGenerator_(std::move(Other.CommonGenerator_))
        , AsteroidBeltProbability_(std::move(Other.AsteroidBeltProbability_))
        , MigrationProbability_(std::move(Other.MigrationProbability_))
        , ScatteringProbability_(std::move(Other.ScatteringProbability_))
        , WalkInProbability_(std::move(Other.WalkInProbability_))
        , CivilizationGenerator_(std::move(Other.CivilizationGenerator_))
        , AsteroidUpperLimit_(std::exchange(Other.AsteroidUpperLimit_, 0.0f))
        , CoilTemperatureLimit_(std::exchange(Other.CoilTemperatureLimit_, 0.0f))
        , RingsParentLowerLimit_(std::exchange(Other.RingsParentLowerLimit_, 0.0f))
        , UniverseAge_(std::exchange(Other.UniverseAge_, 0.0f))
        , bContainUltravioletHabitableZone_(std::exchange(Other.bContainUltravioletHabitableZone_, false))
    {
    }

    FOrbitalGenerator& FOrbitalGenerator::operator=(const FOrbitalGenerator& Other)
    {
        if (this != &Other)
        {
            RandomEngine_                     = Other.RandomEngine_;
            RingsProbabilities_[0]            = Other.RingsProbabilities_[0];
            RingsProbabilities_[1]            = Other.RingsProbabilities_[1];
            BinaryPeriodDistribution_         = Other.BinaryPeriodDistribution_;
            CommonGenerator_                  = Other.CommonGenerator_;
            AsteroidBeltProbability_          = Other.AsteroidBeltProbability_;
            MigrationProbability_             = Other.MigrationProbability_;
            ScatteringProbability_            = Other.ScatteringProbability_;
            WalkInProbability_                = Other.WalkInProbability_;
            AsteroidUpperLimit_               = Other.AsteroidUpperLimit_;
            CoilTemperatureLimit_             = Other.CoilTemperatureLimit_;
            RingsParentLowerLimit_            = Other.RingsParentLowerLimit_;
            UniverseAge_                      = Other.UniverseAge_;
            bContainUltravioletHabitableZone_ = Other.bContainUltravioletHabitableZone_;

            if (Other.CivilizationGenerator_ != nullptr)
            {
                CivilizationGenerator_ = std::make_unique<FCivilizationGenerator>(*Other.CivilizationGenerator_);
            }
        }

        return *this;
    }

    FOrbitalGenerator& FOrbitalGenerator::operator=(FOrbitalGenerator&& Other) noexcept
    {
        if (this != &Other)
        {
            RandomEngine_                     = std::move(Other.RandomEngine_);
            RingsProbabilities_[0]            = std::move(Other.RingsProbabilities_[0]);
            RingsProbabilities_[1]            = std::move(Other.RingsProbabilities_[1]);
            BinaryPeriodDistribution_         = std::move(Other.BinaryPeriodDistribution_);
            CommonGenerator_                  = std::move(Other.CommonGenerator_);
            AsteroidBeltProbability_          = std::move(Other.AsteroidBeltProbability_);
            MigrationProbability_             = std::move(Other.MigrationProbability_);
            ScatteringProbability_            = std::move(Other.ScatteringProbability_);
            WalkInProbability_                = std::move(Other.WalkInProbability_);
            CivilizationGenerator_            = std::move(Other.CivilizationGenerator_);
            AsteroidUpperLimit_               = std::exchange(Other.AsteroidUpperLimit_, 0.0f);
            CoilTemperatureLimit_             = std::exchange(Other.CoilTemperatureLimit_, 0.0f);
            RingsParentLowerLimit_            = std::exchange(Other.RingsParentLowerLimit_, 0.0f);
            UniverseAge_                      = std::exchange(Other.UniverseAge_, 0.0f);
            bContainUltravioletHabitableZone_ = std::exchange(Other.bContainUltravioletHabitableZone_, false);
        }

        return *this;
    }

    void FOrbitalGenerator::GenerateOrbitals(Astro::FStellarSystem& System)
    {
        if (System.StarsData().size() == 2)
        {
            GenerateBinaryOrbit(System);
            Astro::AStar* Star1 = System.StarsData()[0].get();
            Astro::AStar* Star2 = System.StarsData()[1].get();

            for (auto& Star : System.StarsData())
            {
                Astro::AStar* Current  = Star.get();
                Astro::AStar* TheOther = Star.get() == Star1 ? Star2 : Star1;

                if (Current->GetMass() > 12 * kSolarMass)
                {
                    Current->SetHasPlanets(false);
                }
                else
                {
                    if ((Current->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kNeutronStar ||
                         Current->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kBlackHole) &&
                        Current->GetStarFrom() != Astro::AStar::EStarFrom::kWhiteDwarfMerge)
                    {
                        Current->SetHasPlanets(false);
                    }
                    else
                    {
                        if (TheOther->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kNeutronStar ||
                            TheOther->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kBlackHole)
                        {
                            if (TheOther->GetFeH() >= -2.0f)
                            {
                                if (Current->GetAge() > TheOther->GetAge())
                                {
                                    Current->SetHasPlanets(false);
                                }
                            }
                            else
                            {
                                if (TheOther->GetInitialMass() <= 40  * kSolarMass ||
                                    TheOther->GetInitialMass() >= 140 * kSolarMass)
                                {
                                    if (Current->GetAge() > TheOther->GetAge())
                                    {
                                        Current->SetHasPlanets(false);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            Astro::AStar* Star = System.StarsData().front().get();

            auto ZeroOrbit = std::make_unique<Astro::FOrbit>();
            Astro::FOrbit::FOrbitalDetails MainStar(Star, Astro::FOrbit::EObjectType::kStar, ZeroOrbit.get());
            ZeroOrbit->ObjectsData().push_back(std::move(MainStar));
            ZeroOrbit->SetParent(System.GetBaryCenter(), Astro::FOrbit::EObjectType::kBaryCenter);
            System.OrbitsData().push_back(std::move(ZeroOrbit));

            float NearStarSemiMajorAxis = static_cast<float>(
                std::sqrt(Star->GetLuminosity() / (4 * Math::kPi * kStefanBoltzmann * std::pow(CoilTemperatureLimit_, 4))));
            auto NearStarOrbit = std::make_unique<Astro::FOrbit>();

            NearStarOrbit->SetParent(System.GetBaryCenter(), Astro::FOrbit::EObjectType::kBaryCenter);
            NearStarOrbit->SetNormal(System.GetBaryNormal());
            NearStarOrbit->SetSemiMajorAxis(NearStarSemiMajorAxis);
            System.OrbitsData().push_back(std::move(NearStarOrbit));

#ifdef DEBUG_OUTPUT
            std::println("");
            std::println("Near star orbit: {} AU", NearStarSemiMajorAxis / kAuToMeter);
            std::println("");
#endif // DEBUG_OUTPUT

            if (Star->GetMass() > 12 * kSolarMass)
            {
                Star->SetHasPlanets(false);
            }
            else
            {
                if (Star->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kNeutronStar ||
                    Star->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kBlackHole)
                {
                    if (Star->GetStarFrom() != Astro::AStar::EStarFrom::kWhiteDwarfMerge)
                    {
                        Star->SetHasPlanets(false);
                    }
                }
            }
        }

        for (std::size_t i = 0; i != System.StarsData().size(); ++i)
        {
            if (System.StarsData()[i]->HasPlanets())
            {
                GeneratePlanets(i, System.OrbitsData()[i]->ObjectsData().front(), System);
            }
        }
    }

    void FOrbitalGenerator::GenerateBinaryOrbit(Astro::FStellarSystem& System)
    {
        auto* SystemBaryCenter = System.GetBaryCenter();

        std::array<Astro::FOrbit, 2> OrbitData;

        for (int i = 0; i != 2; ++i)
        {
            OrbitData[i].SetParent(SystemBaryCenter, Astro::FOrbit::EObjectType::kBaryCenter);
            GenerateOrbitElements(OrbitData[i]);
        }

        float MassSol1 = static_cast<float>(System.StarsData().front()->GetMass() / kSolarMass);
        float MassSol2 = static_cast<float>(System.StarsData().back()->GetMass()  / kSolarMass);

        float LogPeriodDays       = 0.0f;
        float CommonCoefficient   = 365 * std::pow(MassSol1 + MassSol2, 0.3f);
        float LogPeriodLowerLimit = std::log10(50   * CommonCoefficient);
        float LogPeriodUpperLimit = std::log10(2500 * CommonCoefficient);

        do {
            LogPeriodDays = BinaryPeriodDistribution_(RandomEngine_);
        } while (LogPeriodDays > LogPeriodUpperLimit || LogPeriodDays < LogPeriodLowerLimit);

        float Period = std::pow(10.0f, LogPeriodDays) * kDayToSecond;
        float BinarySemiMajorAxis = static_cast<float>(std::pow(
            (kGravityConstant * kSolarMass * (MassSol1 + MassSol2) * std::pow(Period, 2)) / (4 * std::pow(Math::kPi, 2)),
            1.0 / 3.0
        ));

        float SemiMajorAxis1 = BinarySemiMajorAxis * MassSol2 / (MassSol1 + MassSol2);
        float SemiMajorAxis2 = BinarySemiMajorAxis - SemiMajorAxis1;

        OrbitData[0].SetSemiMajorAxis(SemiMajorAxis1);
        OrbitData[1].SetSemiMajorAxis(SemiMajorAxis2);
        OrbitData[0].SetPeriod(Period);
        OrbitData[1].SetPeriod(Period);

        float Random = CommonGenerator_(RandomEngine_) * 1.2f;
        float Eccentricity = 0.0f;
        if (Period / kDayToSecond < 10)
        {
            Eccentricity = Random * 0.01f;
        }
        else if (Period / kDayToSecond < 1e6f)
        {
            Eccentricity = Random * (0.1975f * std::log10(Period / kDayToSecond) - 0.385f);
        }
        else
        {
            Eccentricity = Random * 0.8f;
        }

        OrbitData[0].SetEccentricity(Eccentricity);
        OrbitData[1].SetEccentricity(Eccentricity);
        OrbitData[0].SetNormal(System.GetBaryNormal());
        OrbitData[1].SetNormal(System.GetBaryNormal());

        std::array<glm::vec2, 2> StarNormals{};

        for (int i = 0; i != 2; ++i)
        {
            StarNormals[i] = glm::vec2(OrbitData[i].GetNormal() + glm::vec2(
                -0.09f + CommonGenerator_(RandomEngine_) * 0.18f,
                -0.09f + CommonGenerator_(RandomEngine_) * 0.18f
            ));

            if (StarNormals[i].x > 2 * Math::kPi)
            {
                StarNormals[i].x -= 2 * Math::kPi;
            }
            else if (StarNormals[i].x < 0.0f)
            {
                StarNormals[i].x += 2 * Math::kPi;
            }

            if (StarNormals[i].y > Math::kPi)
            {
                StarNormals[i].y -= Math::kPi;
            }
            else if (StarNormals[i].y < 0.0f)
            {
                StarNormals[i].y += Math::kPi;
            }
        }

        System.StarsData().front()->SetNormal(StarNormals[0]);
        System.StarsData().back()->SetNormal(StarNormals[1]);

        Random = CommonGenerator_(RandomEngine_) * 2.0f * Math::kPi;
        float ArgumentOfPeriapsis1 = Random;
        float ArgumentOfPeriapsis2 = 0.0f;
        if (ArgumentOfPeriapsis1 >= Math::kPi)
        {
            ArgumentOfPeriapsis2 = ArgumentOfPeriapsis1 - Math::kPi;
        }
        else
        {
            ArgumentOfPeriapsis2 = ArgumentOfPeriapsis1 + Math::kPi;
        }

        OrbitData[0].SetArgumentOfPeriapsis(ArgumentOfPeriapsis1);
        OrbitData[1].SetArgumentOfPeriapsis(ArgumentOfPeriapsis2);

        Random = CommonGenerator_(RandomEngine_) * 2 * Math::kPi;
        float InitialTrueAnomaly1 = Random;
        float InitialTrueAnomaly2 = 0.0f;
        if (InitialTrueAnomaly1 >= Math::kPi)
        {
            InitialTrueAnomaly2 = InitialTrueAnomaly1 - Math::kPi;
        }
        else
        {
            InitialTrueAnomaly2 = InitialTrueAnomaly1 + Math::kPi;
        }

        auto Orbit1 = std::make_unique<Astro::FOrbit>(OrbitData[0]);
        auto Orbit2 = std::make_unique<Astro::FOrbit>(OrbitData[1]);

        Astro::FOrbit::FOrbitalDetails Star1(
            System.StarsData().front().get(), Astro::FOrbit::EObjectType::kStar, Orbit1.get(), InitialTrueAnomaly1);
        Astro::FOrbit::FOrbitalDetails Star2(
            System.StarsData().back().get(),  Astro::FOrbit::EObjectType::kStar, Orbit2.get(), InitialTrueAnomaly2);

        Orbit1->ObjectsData().push_back(std::move(Star1));
        Orbit2->ObjectsData().push_back(std::move(Star2));

        System.OrbitsData().push_back(std::move(Orbit1));
        System.OrbitsData().push_back(std::move(Orbit2));

        std::array<Astro::FOrbit, 2> NearStarOrbits;

        for (std::size_t i = 0; i != 2; ++i)
        {
            Astro::AStar* Current  = System.StarsData()[i].get();
            Astro::AStar* TheOther = System.StarsData()[1 - i].get();

            float NearStarSemiMajorAxis = static_cast<float>(
                std::sqrt(Current->GetLuminosity() / (4 * Math::kPi * ((kStefanBoltzmann * std::pow(CoilTemperatureLimit_, 4)) -
                                                                       TheOther->GetLuminosity() / (4 * Math::kPi * std::pow(BinarySemiMajorAxis, 2))))));

            std::unique_ptr<Astro::FOrbit> NearStarOrbit = std::make_unique<Astro::FOrbit>();
            NearStarOrbit->SetParent(Current, Astro::FOrbit::EObjectType::kStar);
            NearStarOrbit->SetNormal(Current->GetNormal());
            NearStarOrbit->SetSemiMajorAxis(NearStarSemiMajorAxis);

            NearStarOrbits[i] = *NearStarOrbit;

            System.OrbitsData().push_back(std::move(NearStarOrbit));
        }

#ifdef DEBUG_OUTPUT
        std::println("Semi-major axis of binary stars: {} AU",          BinarySemiMajorAxis / kAuToMeter);
        std::println("Semi-major axis of binary first star: {} AU",     OrbitData[0].GetSemiMajorAxis() / kAuToMeter);
        std::println("Semi-major axis of binary second star: {} AU",    OrbitData[1].GetSemiMajorAxis() / kAuToMeter);
        std::println("Period of binary: {} days",                       Period / kDayToSecond);
        std::println("Eccentricity of binary: {}",                      Eccentricity);
        std::println("Argument of periapsis of binary first star: {}",  ArgumentOfPeriapsis1);
        std::println("Argument of periapsis of binary second star: {}", ArgumentOfPeriapsis2);
        std::println("Initial true anomaly of binary first star: {}",   InitialTrueAnomaly1);
        std::println("Initial true anomaly of binary second star: {}",  InitialTrueAnomaly2);
        std::println("Normal of binary first star: ({}, {})",           StarNormals[0].x, StarNormals[1].y);
        std::println("Normal of binary second star: ({}, {})",          StarNormals[0].x, StarNormals[1].y);
        std::println("Near star semi-major axis of first star: {} AU",  NearStarOrbits[0].GetSemiMajorAxis() / kAuToMeter);
        std::println("Near star semi-major axis of second star: {} AU", NearStarOrbits[1].GetSemiMajorAxis() / kAuToMeter);
        std::println("");
#endif // DEBUG_OUTPUT
    }

    void FOrbitalGenerator::GeneratePlanets(std::size_t StarIndex, Astro::FOrbit::FOrbitalDetails& ParentStar, Astro::FStellarSystem& System)
    {
        // 变量名未标注单位均为国际单位制
        Astro::AStar* Star = System.StarsData()[StarIndex].get();
        if (Star->GetFeH() < -2.0f)
        {
            return;
        }

        float BinarySemiMajorAxis = 0.0f;
        if (System.StarsData().size() > 1)
        {
            BinarySemiMajorAxis = System.OrbitsData()[0]->GetSemiMajorAxis() + System.OrbitsData()[1]->GetSemiMajorAxis();
        }

        // 生成原行星盘数据
        auto ExpectedPlanetaryDiskData = GeneratePlanetaryDisk(Star);
        if (!ExpectedPlanetaryDiskData.has_value())
        {
            return;
        }
        
        auto& PlanetaryDisk      = ExpectedPlanetaryDiskData.value();
        float StarInitialMassSol = Star->GetInitialMass() / kSolarMass;
        auto  StellarType        = Star->GetStellarClass().GetStellarType();

        // 生成行星们
        std::size_t PlanetCount = GeneratePlanetCount(Star);

        std::vector<std::unique_ptr<Astro::APlanet>> Planets;
        std::vector<std::unique_ptr<Astro::AAsteroidCluster>> AsteroidClusters;

        Planets.reserve(PlanetCount);
        for (std::size_t i = 0; i < PlanetCount; ++i)
        {
            Planets.push_back(std::make_unique<Astro::APlanet>());
        }

        // 初始化核心质量
        std::vector<float> CoreMassesSol = GenerateCoreMassesSol(PlanetaryDisk, PlanetCount);

        // 初始化轨道
        std::vector<std::unique_ptr<Astro::FOrbit>> Orbits = GenerateOrbits(Star, CoreMassesSol, PlanetaryDisk, PlanetCount);

        // 计算原行星盘年龄
        float DiskAge = CalculatePlanetaryDiskAgeAndDetermineProtoplanetTypes(Star, Orbits, PlanetCount, CoreMassesSol, Planets);

        // 抹掉位于（双星）稳定区域以外的行星
        if (System.StarsData().size() > 1)
        {
            EraseUnstablePlanets(System, StarIndex, BinarySemiMajorAxis, PlanetCount, CoreMassesSol, Orbits, Planets);
        }

        std::vector<float> NewCoreMassesSol(PlanetCount); // 吸积核心质量，单位太阳
        float MigratedOriginSemiMajorAxisAu = 0.0f;       // 原有的半长轴，用于计算内迁行星

        if (StellarType != Astro::FStellarClass::EStellarType::kNeutronStar &&
            StellarType != Astro::FStellarClass::EStellarType::kBlackHole)
        {
            // 宜居带半径，单位 AU
            std::pair<float, float> HabitableZoneAu = CalculateHabitableZone(System, StarIndex, BinarySemiMajorAxis);

            // 冻结线半径，单位 AU
            float FrostLineAu = CalculateFrostLine(System, StarIndex, StarInitialMassSol, BinarySemiMajorAxis);

            // 判断大行星
            PlanetCount = JudgeLargePlanets(StarIndex, System.StarsData(), BinarySemiMajorAxis, HabitableZoneAu.first,
                                            FrostLineAu, CoreMassesSol, NewCoreMassesSol, Orbits, Planets);

#ifdef DEBUG_OUTPUT
            for (std::size_t i = 0; i < PlanetCount; ++i)
            {
                std::println("Before migration: planet {} semi-major axis: {} AU, initial core mass: {} earth, new core mass: {} earth, core radius: {} earth, type: {}",
                             i + 1, Orbits[i]->GetSemiMajorAxis() / kAuToMeter, CoreMassesSol[i] * kSolarMassToEarth, NewCoreMassesSol[i] * kSolarMassToEarth, Planets[i]->GetRadius() / kEarthRadius, std::to_underlying(Planets[i]->GetPlanetType()));
            }

            std::println("");
#endif // DEBUG_OUTPUT

            // 巨行星内迁
            MigratePlanets(Star, PlanetaryDisk, PlanetCount, MigratedOriginSemiMajorAxisAu, CoreMassesSol, NewCoreMassesSol, Orbits, Planets);

            // 抹掉内迁坠入恒星或恒星膨胀过程中吞掉的行星，并判断是否有冥府行星产生
            DevourPlanets(Star, PlanetCount, CoreMassesSol, NewCoreMassesSol, Orbits, Planets);

            // 处理白矮星引力散射
            for (std::size_t i = 0; i != PlanetCount; ++i)
            {
                if (Star->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kWhiteDwarf &&
                    Star->GetAge() > 1e6)
                {
                    if (Planets[i]->GetPlanetType() == Astro::APlanet::EPlanetType::kRocky)
                    {
                        if (ScatteringProbability_(RandomEngine_))
                        {
                            float Random = 4.0f + CommonGenerator_(RandomEngine_) * 16.0f; // 4.0 Rsun 高于洛希极限
                            Orbits[i]->SetSemiMajorAxis(Random * kSolarRadius);
                            break;
                        }
                    }
                }
            }

#ifdef DEBUG_OUTPUT
            for (std::size_t i = 0; i < PlanetCount; ++i)
            {
                std::println("Final orbits: planet {} semi-major axis: {} AU, initial core mass: {} earth, new core mass: {} earth, core radius: {} earth, type: {}",
                             i + 1, Orbits[i]->GetSemiMajorAxis() / kAuToMeter, CoreMassesSol[i] * kSolarMassToEarth, NewCoreMassesSol[i] * kSolarMassToEarth, Planets[i]->GetRadius() / kEarthRadius, std::to_underlying(Planets[i]->GetPlanetType()));
            }

            std::println("");
#endif // DEBUG_OUTPUT

            for (std::size_t i = 0; i < PlanetCount; ++i)
            {
                Planets[i]->SetAge(DiskAge);
            }

            auto CalculatePlanetMassByIndex = [&, this](std::size_t Index) -> float
            {
                return CalculatePlanetMass(
                    kSolarMass * CoreMassesSol[Index],
                    kSolarMass * NewCoreMassesSol[Index],
                    Planets[Index]->IsMigrated()
                    ? MigratedOriginSemiMajorAxisAu
                    : Orbits[Index]->GetSemiMajorAxis() / kAuToMeter,
                    PlanetaryDisk, Star, Planets[Index].get());
            };

            for (std::size_t i = 0; i < PlanetCount; ++i)
            {
                float PlanetMassEarth = 0.0f;
                auto  PlanetType      = Planets[i]->GetPlanetType();

                switch (PlanetType)
                {
                case Astro::APlanet::EPlanetType::kRocky:
                case Astro::APlanet::EPlanetType::kIcePlanet:
                case Astro::APlanet::EPlanetType::kOceanic:
                case Astro::APlanet::EPlanetType::kGasGiant:
                case Astro::APlanet::EPlanetType::kRockyAsteroidCluster:
                case Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster:
                    CalculatePlanetRadius(CalculatePlanetMassByIndex(i), Planets[i].get());
                    break;
                case Astro::APlanet::EPlanetType::kIceGiant:
                    if ((PlanetMassEarth = CalculatePlanetMassByIndex(i)) < 10.0f)
                    {
                        Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kSubIceGiant);
                    }
                    CalculatePlanetRadius(PlanetMassEarth, Planets[i].get());
                    break;
                default:
                    break;
                }

                PlanetType           = Planets[i]->GetPlanetType();
                float PoyntingVector = 0.0f;

                if (System.StarsData().size() > 1)
                {
                    const Astro::AStar* Current  = System.StarsData()[StarIndex].get();
                    const Astro::AStar* TheOther = System.StarsData()[1 - StarIndex].get();
                    PoyntingVector =
                        static_cast<float>(Current->GetLuminosity())  / (4 * Math::kPi * std::pow(Orbits[i]->GetSemiMajorAxis(), 2.0f)) +
                        static_cast<float>(TheOther->GetLuminosity()) / (4 * Math::kPi * std::pow(BinarySemiMajorAxis, 2.0f));
                }
                else
                {
                    PoyntingVector =
                        static_cast<float>(Star->GetLuminosity()) / (4 * Math::kPi * std::pow(Orbits[i]->GetSemiMajorAxis(), 2.0f));
                }

#ifdef DEBUG_OUTPUT
                std::println("Planet {} poynting vector: {} W/m^2", i + 1, PoyntingVector);
#endif // DEBUG_OUTPUT
                // 判断热木星
                if (PoyntingVector >= 10000)
                {
                    if (PlanetType == Astro::APlanet::EPlanetType::kGasGiant)
                    {
                        Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kHotGasGiant);
                    }
                    else if (PlanetType == Astro::APlanet::EPlanetType::kIceGiant)
                    {
                        Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kHotIceGiant);
                    }
                    else if (PlanetType == Astro::APlanet::EPlanetType::kSubIceGiant)
                    {
                        Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kHotSubIceGiant);
                    }
                }

                PlanetType = Planets[i]->GetPlanetType();

                if (PlanetType == Astro::APlanet::EPlanetType::kHotIceGiant    ||
                    PlanetType == Astro::APlanet::EPlanetType::kHotSubIceGiant ||
                    PlanetType == Astro::APlanet::EPlanetType::kHotGasGiant)
                {
                    Planets[i]->SetRadius(Planets[i]->GetRadius() * std::pow(PoyntingVector / 10000.0f, 0.094f));
                }

                if (PlanetType == Astro::APlanet::EPlanetType::kOceanic &&
                    HabitableZoneAu.second <= Orbits[i]->GetSemiMajorAxis() / kAuToMeter)
                {
                    Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kIcePlanet);
                }

                // 计算自转周期和扁率
                Astro::FOrbit::FOrbitalDetails Parent(Star, Astro::FOrbit::EObjectType::kStar, Orbits[i].get());
                GenerateSpin(Orbits[i]->GetSemiMajorAxis(), Parent.GetOrbitalObject(), Planets[i].get());

                // 计算类地行星、次生大气层和地壳矿脉
                if (Star->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kNormalStar)
                {
                    GenerateTerra(Star, PoyntingVector, HabitableZoneAu, Orbits[i].get(), Planets[i].get());
                }

                // 计算平衡温度
                CalculateTemperature(Astro::FOrbit::EObjectType::kStar, PoyntingVector, Planets[i].get());
                float BalanceTemperature = Planets[i]->GetBalanceTemperature();
                // 判断有没有被烧似
                if (((PlanetType != Astro::APlanet::EPlanetType::kRockyAsteroidCluster &&
                      PlanetType != Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster) && BalanceTemperature >= 2700) ||
                    ((PlanetType == Astro::APlanet::EPlanetType::kRockyAsteroidCluster ||
                      PlanetType == Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster) && PoyntingVector > 1e6f))
                {
                    Planets.erase(Planets.begin() + i);
                    CoreMassesSol.erase(CoreMassesSol.begin() + i);
                    NewCoreMassesSol.erase(NewCoreMassesSol.begin() + i);
                    Orbits.erase(Orbits.begin() + i);
                    --PlanetCount;
                    --i;
                    continue;
                }

                // 生成卫星和行星环
                Astro::FOrbit::FOrbitalDetails Planet(Planets[i].get(), Astro::FOrbit::EObjectType::kPlanet, Orbits[i].get());
                GenerateMoons(i, FrostLineAu, Star, PoyntingVector, HabitableZoneAu, Planet, Orbits, Planets);

                if (PlanetType != Astro::APlanet::EPlanetType::kRockyAsteroidCluster    &&
                    PlanetType != Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster &&
                    Planets[i]->GetMassDigital<float>() > RingsParentLowerLimit_)
                {
                    GenerateRings(i, std::numeric_limits<float>::infinity(), Star, Planet, Orbits, AsteroidClusters);
                }

                // 生成生命和文明
                PlanetType = Planets[i]->GetPlanetType();
                if (PlanetType == Astro::APlanet::EPlanetType::kTerra)
                {
                    GenerateCivilization(Star, PoyntingVector, HabitableZoneAu, Orbits[i].get(), Planets[i].get());
                }

                // 生成特洛伊带
                GenerateTrojan(Star, FrostLineAu, Orbits[i].get(), Planet, AsteroidClusters);

                Orbits[i]->ObjectsData().push_back(std::move(Planet));
            }

            // 生成柯伊伯带
            if (System.StarsData().size() == 1)
            {
                GenerateKuiperBelt(Star, FrostLineAu, PlanetaryDisk, Orbits, AsteroidClusters);
            }
        }
        else
        {
            PlanetCount = JudgeLargePlanets(StarIndex, System.StarsData(), BinarySemiMajorAxis,
                                            std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
                                            CoreMassesSol, NewCoreMassesSol, Orbits, Planets);

            for (std::size_t i = 0; i < PlanetCount; ++i)
            {
                Planets[i]->SetAge(Star->GetAge());
            }

            for (std::size_t i = 0; i < PlanetCount; ++i)
            {
#ifdef DEBUG_OUTPUT
                float PlanetMassEarth = Planets[i]->GetMassDigital<float>() / kEarthMass;
                std::println("Final system: planet {} semi-major axis: {} AU, mass: {} earth, radius: {} earth, type: {}",
                             i + 1, Orbits[i]->GetSemiMajorAxis() / kAuToMeter, PlanetMassEarth, Planets[i]->GetRadius() / kEarthRadius, std::to_underlying(Planets[i]->GetPlanetType()));
#endif // DEBUG_OUTPUT
                CalculatePlanetRadius(CoreMassesSol[i] * kSolarMassToEarth, Planets[i].get());

                Astro::FOrbit::FOrbitalDetails Parent(Star, Astro::FOrbit::EObjectType::kStar, Orbits[i].get());
                GenerateSpin(Orbits[i]->GetSemiMajorAxis(), Parent.GetOrbitalObject(), Planets[i].get());

                float PoyntingVector = static_cast<float>(Star->GetLuminosity()) /
                    (4 * Math::kPi * std::pow(Orbits[i]->GetSemiMajorAxis(), 2.0f));
                CalculateTemperature(Astro::FOrbit::EObjectType::kStar, PoyntingVector, Planets[i].get());
                float BalanceTemperature = Planets[i]->GetBalanceTemperature();
                // 判断有没有被烧似
                auto PlanetType = Planets[i]->GetPlanetType();
                if (((PlanetType != Astro::APlanet::EPlanetType::kRockyAsteroidCluster &&
                      PlanetType != Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster) && BalanceTemperature >= 2700) ||
                    ((PlanetType == Astro::APlanet::EPlanetType::kRockyAsteroidCluster ||
                      PlanetType == Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster) && PoyntingVector > 1e6f))
                {
                    Planets.erase(Planets.begin() + i);
                    CoreMassesSol.erase(CoreMassesSol.begin() + i);
                    NewCoreMassesSol.erase(NewCoreMassesSol.begin() + i);
                    Orbits.erase(Orbits.begin() + i);
                    --PlanetCount;
                    --i;
                    continue;
                }

                Astro::FOrbit::FOrbitalDetails Planet(Planets[i].get(), Astro::FOrbit::EObjectType::kPlanet, Orbits[i].get());
                GenerateMoons(i, std::numeric_limits<float>::infinity(), Star, PoyntingVector, {}, Planet, Orbits, Planets);

                if (PlanetType != Astro::APlanet::EPlanetType::kRockyAsteroidCluster    &&
                    PlanetType != Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster &&
                    Planets[i]->GetMassDigital<float>() > RingsParentLowerLimit_)
                {
                    GenerateRings(i, std::numeric_limits<float>::infinity(), Star, Planet, Orbits, AsteroidClusters);
                }

                // 生成特洛伊带
                GenerateTrojan(Star, std::numeric_limits<float>::infinity(), Orbits[i].get(), Planet, AsteroidClusters);

                Orbits[i]->ObjectsData().push_back(std::move(Planet));
            }
        }

        // 将被开除的行星移动到小行星带数组
        for (auto& Orbit : Orbits)
        {
            auto& OrbitalDetail = Orbit->ObjectsData().front();
            auto& OrbitalObject = OrbitalDetail.GetOrbitalObject();
            if (OrbitalObject.GetObjectType() != Astro::FOrbit::EObjectType::kPlanet)
            {
                continue;
            }

            auto* PlanetPtr = OrbitalDetail.GetOrbitalObject().GetObject<Astro::APlanet>();
            if (OrbitalDetail.GetOrbitalObject().GetObjectType() == Astro::FOrbit::EObjectType::kPlanet)
            {
                if (PlanetPtr->GetPlanetType() ==
                    Astro::APlanet::EPlanetType::kRockyAsteroidCluster ||
                    PlanetPtr->GetPlanetType() ==
                    Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster)
                {
                    auto AsteroidCluster = PlanetToAsteroidCluster(PlanetPtr);
                    OrbitalDetail.SetOrbitalObject(AsteroidCluster.get(), Astro::FOrbit::EObjectType::kAsteroidCluster);
                    AsteroidClusters.push_back(std::move(AsteroidCluster));
                    // 为了输出调试信息，删除将在输出后进行
                    // --PlanetCount;
                    // Planets.erase(std::find_if(Planets.begin(), Planets.end(), [&](const auto& Planet) -> bool
                    // {
                    //     return Planet.get() == PlanetPtr;
                    // }));
                }
            }
        }

        CalculateOrbitalPeriods(Orbits);

#ifdef DEBUG_OUTPUT
        std::println("");

        for (std::size_t i = 0; i != PlanetCount; ++i)
        {
            auto& Planet                         = Planets[i];
            auto  PlanetType                     = Planet->GetPlanetType();
            float PlanetMass                     = Planet->GetMassDigital<float>();
            float PlanetMassEarth                = PlanetMass / kEarthMass;
            float PlanetRadius                   = Planet->GetRadius();
            float PlanetRadiusEarth              = PlanetRadius / kEarthRadius;
            float AtmosphereMassZ                = Planet->GetAtmosphereMassZDigital<float>();
            float AtmosphereMassVolatiles        = Planet->GetAtmosphereMassVolatilesDigital<float>();
            float AtmosphereMassEnergeticNuclide = Planet->GetAtmosphereMassEnergeticNuclideDigital<float>();
            float CoreMassZ                      = Planet->GetCoreMassZDigital<float>();
            float CoreMassVolatiles              = Planet->GetCoreMassVolatilesDigital<float>();
            float CoreMassEnergeticNuclide       = Planet->GetCoreMassEnergeticNuclideDigital<float>();
            float OceanMassZ                     = Planet->GetOceanMassZDigital<float>();;
            float OceanMassVolatiles             = Planet->GetOceanMassVolatilesDigital<float>();
            float OceanMassEnergeticNuclide      = Planet->GetOceanMassEnergeticNuclideDigital<float>();
            float CrustMineralMass               = Planet->GetCrustMineralMassDigital<float>();
            float AtmospherePressure             = (kGravityConstant * PlanetMass * (AtmosphereMassZ + AtmosphereMassVolatiles + AtmosphereMassEnergeticNuclide)) / (4 * Math::kPi * std::pow(Planets[i]->GetRadius(), 4.0f));
            float Oblateness                     = Planet->GetOblateness();
            float Spin                           = Planet->GetSpin();
            float BalanceTemperature             = Planet->GetBalanceTemperature();

            if (PlanetType != Astro::APlanet::EPlanetType::kRockyAsteroidCluster &&
                PlanetType != Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster)
            {
                std::println("Planet {} details:", i + 1);
                std::println("semi-major axis: {} AU, period: {} days, mass: {} earth, radius: {} earth, type: {}",
                             Orbits[i]->GetSemiMajorAxis() / kAuToMeter, Orbits[i]->GetPeriod() / kDayToSecond, PlanetMassEarth, PlanetRadiusEarth, std::to_underlying(PlanetType));
                std::println("rotation period: {} h, oblateness: {}, balance temperature: {} K",
                             Spin / 3600, Oblateness, BalanceTemperature);
                std::println("atmo  mass z: {:.2E} kg, atmo  mass vol: {:.2E} kg, atmo  mass nuc: {:.2E} kg",
                             AtmosphereMassZ, AtmosphereMassVolatiles, AtmosphereMassEnergeticNuclide);
                std::println("core  mass z: {:.2E} kg, core  mass vol: {:.2E} kg, core  mass nuc: {:.2E} kg",
                             CoreMassZ, CoreMassVolatiles, CoreMassEnergeticNuclide);
                std::println("ocean mass z: {:.2E} kg, ocean mass vol: {:.2E} kg, ocean mass nuc: {:.2E} kg",
                             OceanMassZ, OceanMassVolatiles, OceanMassEnergeticNuclide);
                std::println("crust mineral mass : {:.2E} kg, atmo pressure : {:.2f} atm",
                             CrustMineralMass, AtmospherePressure / kPascalToAtm);
            }
            else
            {
                std::println("Asteroid belt (origin planet {}) details:", i + 1);
                std::println("semi-major axis: {} AU, period: {} days, mass: {} moon, type: {}",
                             Orbits[i]->GetSemiMajorAxis() / kAuToMeter, Orbits[i]->GetPeriod() / kDayToSecond, PlanetMass / kMoonMass, std::to_underlying(PlanetType));
                std::println("mass z: {:.2E} kg, mass vol: {:.2E} kg, mass nuc: {:.2E} kg",
                             CoreMassZ, CoreMassVolatiles, CoreMassEnergeticNuclide);
            }

            std::println("");
        }
#endif // DEBUG_OUTPUT

        for (std::size_t i = 0; i < PlanetCount; ++i) // 删除被移动到小行星数组的行星
        {
            if (Planets[i]->GetPlanetType() == Astro::APlanet::EPlanetType::kRockyAsteroidCluster ||
                Planets[i]->GetPlanetType() == Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster)
            {
                Planets.erase(Planets.begin() + i);
                --i;
                --PlanetCount;
            }
        }

        for (auto& Orbit : Orbits)
        {
            if (Orbit->GetParent().GetObjectType() == Astro::FOrbit::EObjectType::kStar)
            {
                ParentStar.DirectOrbitsData().push_back(Orbit.get());
            }
        }

        System.PlanetsData().reserve(System.PlanetsData().size() + Planets.size());
        System.PlanetsData().append_range(Planets | std::views::as_rvalue);

        System.OrbitsData().reserve(System.OrbitsData().size() + Orbits.size());
        System.OrbitsData().append_range(Orbits | std::views::as_rvalue);

        System.AsteroidClustersData().reserve(System.AsteroidClustersData().size() + AsteroidClusters.size());
        System.AsteroidClustersData().append_range(AsteroidClusters | std::views::as_rvalue);
    }

    std::expected<FOrbitalGenerator::FPlanetaryDisk, int> FOrbitalGenerator::GeneratePlanetaryDisk(const Astro::AStar* Star)
    {
        FPlanetaryDisk PlanetaryDisk;
        float DiskBase           = 1.0f + CommonGenerator_(RandomEngine_); // 基准随机数，1-2 之间
        float StarInitialMassSol = Star->GetInitialMass() / kSolarMass;
        auto  StellarType        = Star->GetStellarClass().GetStellarType();
        if (StellarType != Astro::FStellarClass::EStellarType::kNeutronStar &&
            StellarType != Astro::FStellarClass::EStellarType::kBlackHole)
        {
            float DiskMassSol     = DiskBase * StarInitialMassSol * std::pow(10.0f, -2.05f + 0.1214f * StarInitialMassSol - 0.02669f * std::pow(StarInitialMassSol, 2.0f) - 0.2274f * std::log(StarInitialMassSol));
            float DustMassSol     = DiskMassSol * 0.0142f * 0.4f * std::pow(10.0f, Star->GetFeH());
            float OuterRadiusAu   = StarInitialMassSol >= 1 ? 45.0f * StarInitialMassSol : 45.0f * std::pow(StarInitialMassSol, 2.0f);
            float InnerRadiusAu   = 0.0f;
            float DiskCoefficient = 0.0f;
            if (StarInitialMassSol < 0.6f)
            {
                DiskCoefficient = 2100;
            }
            else if (StarInitialMassSol < 1.5f)
            {
                DiskCoefficient = 1400;
            }
            else
            {
                DiskCoefficient = 1700;
            }

            float CommonCoefficient =
                (std::pow(10.0f, 2.0f - StarInitialMassSol) + 1.0f) *
                (static_cast<float>(kSolarLuminosity) / (4 * Math::kPi * kStefanBoltzmann * std::pow(DiskCoefficient, 4.0f)));

            float InnerRadiusAuSquared = 0.0f;
            if (StarInitialMassSol >= 0.075f && StarInitialMassSol < 0.43f)
            {
                InnerRadiusAuSquared = CommonCoefficient * (0.23f * std::pow(StarInitialMassSol, 2.3f));
            }
            else if (StarInitialMassSol >= 0.43f && StarInitialMassSol < 2.0f)
            {
                InnerRadiusAuSquared = CommonCoefficient * std::pow(StarInitialMassSol, 4.0f);
            }
            else if (StarInitialMassSol >= 2.0f && StarInitialMassSol <= 12.0f)
            {
                InnerRadiusAuSquared = CommonCoefficient * (1.5f * std::pow(StarInitialMassSol, 3.5f));
            }

            InnerRadiusAu = std::sqrt(InnerRadiusAuSquared) / kAuToMeter; // 转化为 AU

            PlanetaryDisk.InnerRadiusAu = InnerRadiusAu;
            PlanetaryDisk.OuterRadiusAu = OuterRadiusAu;
            PlanetaryDisk.DiskMassSol = DiskMassSol;
            PlanetaryDisk.DustMassSol = DustMassSol;
        }
        else if (Star->GetStarFrom() == Astro::AStar::EStarFrom::kWhiteDwarfMerge)
        {
            DiskBase = std::pow(10.0f, -1.0f) + CommonGenerator_(RandomEngine_) * (1.0f - std::pow(10.0f, -1.0f));
            float StarMassSol = static_cast<float>(Star->GetMass() / kSolarMass);
            float DiskMassSol = DiskBase * 1e-5f * StarMassSol;
            PlanetaryDisk.InnerRadiusAu = 0.02f; // 高于洛希极限
            PlanetaryDisk.OuterRadiusAu = 1.0f;
            PlanetaryDisk.DiskMassSol = DiskMassSol;
            PlanetaryDisk.DustMassSol = DiskMassSol;
        }
        else
        {
            return std::unexpected(0);
        }

#ifdef DEBUG_OUTPUT
        std::println("Planetary disk inner radius: {} AU", PlanetaryDisk.InnerRadiusAu);
        std::println("Planetary disk outer radius: {} AU", PlanetaryDisk.OuterRadiusAu);
        std::println("Planetary disk mass: {} solar",      PlanetaryDisk.DiskMassSol);
        std::println("Planetary disk dust mass: {} solar", PlanetaryDisk.DustMassSol);
        std::println("");
#endif // DEUB_OUTPUT

        return PlanetaryDisk;
    }

    std::size_t FOrbitalGenerator::GeneratePlanetCount(const Astro::AStar* Star)
    {
        std::size_t PlanetCount        = 0;
        float       StarInitialMassSol = Star->GetInitialMass() / kSolarMass;
        auto        StellarType        = Star->GetStellarClass().GetStellarType();
        if (StellarType != Astro::FStellarClass::EStellarType::kNeutronStar &&
            StellarType != Astro::FStellarClass::EStellarType::kBlackHole)
        {
            if (StarInitialMassSol < 0.6f)
            {
                PlanetCount = static_cast<std::size_t>(4.0f + CommonGenerator_(RandomEngine_) * 4.0f);
            }
            else if (StarInitialMassSol < 0.9f)
            {
                PlanetCount = static_cast<std::size_t>(5.0f + CommonGenerator_(RandomEngine_) * 5.0f);
            }
            else if (StarInitialMassSol < 3.0f)
            {
                PlanetCount = static_cast<std::size_t>(6.0f + CommonGenerator_(RandomEngine_) * 6.0f);
            }
            else
            {
                PlanetCount = static_cast<std::size_t>(4.0f + CommonGenerator_(RandomEngine_) * 4.0f);
            }
        }
        else if (Star->GetStarFrom() == Astro::AStar::EStarFrom::kWhiteDwarfMerge)
        {
            PlanetCount = static_cast<std::size_t>(2.0f + CommonGenerator_(RandomEngine_) * 2.0f);
        }

        return PlanetCount;
    }

    std::vector<float> FOrbitalGenerator::GenerateCoreMassesSol(const FPlanetaryDisk& PlanetaryDisk, std::size_t PlanetCount)
    {
        // 生成行星初始核心质量
        std::vector<float> CoreBase(PlanetCount, 0.0f);
        for (float& Num : CoreBase)
        {
            Num = CommonGenerator_(RandomEngine_) * 3.0f;
        }

        float CoreBaseSum = 0.0f;
        for (float& Num : CoreBase)
        {
            CoreBaseSum += std::pow(10.0f, Num);
        }

        std::vector<float> CoreMassesSol(PlanetCount); // 初始核心质量，单位太阳
        for (std::size_t i = 0; i < PlanetCount; ++i)
        {
            CoreMassesSol[i] = PlanetaryDisk.DustMassSol * std::pow(10.0f, CoreBase[i]) / CoreBaseSum;

#ifdef DEBUG_OUTPUT
            std::println("Generate initial core mass: planet {} initial core mass: {} earth\n",
                         i + 1, CoreMassesSol[i] * kSolarMassToEarth);
#endif // DEBUG_OUTPUT
        }

        return CoreMassesSol;
    }

    std::vector<std::unique_ptr<Astro::FOrbit>>
    FOrbitalGenerator::GenerateOrbits(Astro::AStar* Star, const std::vector<float>& CoreMassesSol,
                                      const FPlanetaryDisk& PlanetaryDisk, std::size_t PlanetCount)
    {
        std::vector<std::unique_ptr<Astro::FOrbit>> Orbits;
        for (std::size_t i = 0; i != PlanetCount; ++i)
        {
            Orbits.push_back(std::make_unique<Astro::FOrbit>());
        }

        for (auto& Orbit : Orbits)
        {
            Orbit->SetParent(Star, Astro::FOrbit::EObjectType::kStar);
        }

        // 生成初始轨道半长轴
        std::vector<float> DiskBoundariesAu(PlanetCount + 1);
        DiskBoundariesAu[0] = PlanetaryDisk.InnerRadiusAu;

        float CoreMassSum = std::accumulate(CoreMassesSol.begin(), CoreMassesSol.end(), 0.0f,
        [](float Sum, float Value) -> float
        {
            return Sum + std::pow(Value, 0.1f);
        });

        std::vector<float> PartCoreMassSums(PlanetCount + 1, 0.0f);
        for (std::size_t i = 1; i <= PlanetCount; ++i)
        {
            PartCoreMassSums[i] = PartCoreMassSums[i - 1] + std::pow(CoreMassesSol[i - 1], 0.1f);
        }

        for (std::size_t i = 0; i < PlanetCount; ++i)
        {
            DiskBoundariesAu[i + 1] =
                PlanetaryDisk.InnerRadiusAu * std::pow(PlanetaryDisk.OuterRadiusAu / PlanetaryDisk.InnerRadiusAu,
                                                       PartCoreMassSums[i + 1] / CoreMassSum);

            float SemiMajorAxis = kAuToMeter * (DiskBoundariesAu[i] + DiskBoundariesAu[i + 1]) / 2.0f;
            Orbits[i]->SetSemiMajorAxis(SemiMajorAxis);
            GenerateOrbitElements(*Orbits[i].get()); // 生成剩余的根数
#ifdef DEBUG_OUTPUT
            std::println("Generate initial semi-major axis: planet {} initial semi-major axis: {} AU\n",
                         i + 1, Orbits[i]->GetSemiMajorAxis() / kAuToMeter);
#endif // DEBUG_OUTPUT
        }

        return Orbits;
    }

    float FOrbitalGenerator::CalculatePlanetaryDiskAgeAndDetermineProtoplanetTypes(
        const Astro::AStar* Star, const std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits,
        std::size_t PlanetCount, std::vector<float>& CoreMassesSol,
        std::vector<std::unique_ptr<Astro::APlanet>>& Planets)
    {
        float StarInitialMassSol = Star->GetInitialMass() / kSolarMass;
        float DiskAge = 8.15e6f + 8.3e5f * StarInitialMassSol - 33854 *
                        std::pow(StarInitialMassSol, 2.0f) - 5.031e6f * std::log(StarInitialMassSol);

        if (std::to_underlying(Star->GetEvolutionPhase()) <= 9 && DiskAge >= Star->GetAge())
        {
            for (auto& Planet : Planets)
            {
                Planet->SetPlanetType(Astro::APlanet::EPlanetType::kRockyAsteroidCluster);
            }
        }
        else
        {
            Utils::TBernoulliDistribution ConstructFailedProbability(0.5f);
            for (std::size_t i = 0; i != PlanetCount; ++i)
            {
                if (CoreMassesSol[i] * kSolarMassToEarth < 0.5f)
                {
                    if (ConstructFailedProbability(RandomEngine_))
                    {
                        float StarAge             = static_cast<float>(Star->GetAge());
                        float Exponent            = StarAge / (5e9f * (Orbits[i]->GetSemiMajorAxis() / kAuToMeter) / 2.7f);
                        float DiscountCoefficient = std::max(0.001f, std::pow(0.01f, Exponent));
                        CoreMassesSol[i]         *= DiscountCoefficient;
                    }
                }
            }
        }

        return DiskAge;
    }

    void FOrbitalGenerator::EraseUnstablePlanets(Astro::FStellarSystem& System, std::size_t StarIndex, float BinarySemiMajorAxis,
                                                 std::size_t PlanetCount, std::vector<float>& CoreMassesSol,
                                                 std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits,
                                                 std::vector<std::unique_ptr<Astro::APlanet>>& Planets)
    {
        const Astro::AStar* Current  = System.StarsData()[StarIndex].get();
        const Astro::AStar* TheOther = System.StarsData()[1 - StarIndex].get();

        float Eccentricity = System.OrbitsData()[0]->GetEccentricity();
        float Mu = static_cast<float>(TheOther->GetMass() / (Current->GetMass() + TheOther->GetMass()));
        float StableBoundaryLimit = BinarySemiMajorAxis *
            (0.464f - 0.38f * Mu - 0.361f * Eccentricity + 0.586f * Mu * Eccentricity +
             0.15f * std::pow(Eccentricity, 2.0f) - 0.198f * Mu * std::pow(Eccentricity, 2.0f));

        for (std::size_t i = 0; i < PlanetCount; ++i)
        {
            if (Orbits[i]->GetSemiMajorAxis() > StableBoundaryLimit)
            {
                Planets.erase(Planets.begin() + i, Planets.end());
                Orbits.erase(Orbits.begin() + i, Orbits.end());
                CoreMassesSol.erase(CoreMassesSol.begin() + i, CoreMassesSol.end());
                PlanetCount = Planets.size();
            }
        }
    }

    void FOrbitalGenerator::EraseLimitedPlanets(float Limit, std::size_t& PlanetCount,
                                                std::vector<float>& CoreMassesSol, std::vector<float>& NewCoreMassesSol,
                                                std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits,
                                                std::vector<std::unique_ptr<Astro::APlanet>>& Planets)
    {
        for (std::size_t i = 0; i < PlanetCount; ++i)
        {
            if (Orbits[0]->GetSemiMajorAxis() < Limit)
            {
                Planets.erase(Planets.begin());
                Orbits.erase(Orbits.begin());
                CoreMassesSol.erase(CoreMassesSol.begin());
                NewCoreMassesSol.erase(NewCoreMassesSol.begin());
                --PlanetCount;
            }
        }
    }

    std::pair<float, float>
    FOrbitalGenerator::CalculateHabitableZone(Astro::FStellarSystem& System, std::size_t StarIndex, float BinarySemiMajorAxis)
    {
        // 宜居带半径，单位 AU
        std::pair<float, float> HabitableZoneAu;

        if (System.StarsData().size() > 1)
        {
            const Astro::AStar* Current  = System.StarsData()[StarIndex].get();
            const Astro::AStar* TheOther = System.StarsData()[1 - StarIndex].get();

            float CurrentLuminosity  = static_cast<float>(Current->GetLuminosity());
            float TheOtherLuminoisty = static_cast<float>(TheOther->GetLuminosity());

            HabitableZoneAu.first  = std::sqrt(CurrentLuminosity / (4 * Math::kPi * (3000 - TheOtherLuminoisty / (4 * Math::kPi * std::pow(BinarySemiMajorAxis, 2.0f))))) / kAuToMeter;
            HabitableZoneAu.second = std::sqrt(CurrentLuminosity / (4 * Math::kPi * (600  - TheOtherLuminoisty / (4 * Math::kPi * std::pow(BinarySemiMajorAxis, 2.0f))))) / kAuToMeter;
        }
        else
        {
            const Astro::AStar* Star = System.StarsData()[StarIndex].get();
            float StarLuminosity     = static_cast<float>(Star->GetLuminosity());
            HabitableZoneAu.first    = std::sqrt(StarLuminosity / (4 * Math::kPi * 3000)) / kAuToMeter;
            HabitableZoneAu.second   = std::sqrt(StarLuminosity / (4 * Math::kPi * 600))  / kAuToMeter;
        }

#ifdef DEBUG_OUTPUT
        std::println("Circumstellar habitable zone: {} - {} AU", HabitableZoneAu.first, HabitableZoneAu.second);
        std::println("");
#endif // DEBUG_OUTPUT

        return HabitableZoneAu;
    }

    float FOrbitalGenerator::CalculateFrostLine(Astro::FStellarSystem& System, std::size_t StarIndex, float StarInitialMassSol, float BinarySemiMajorAxis)
    {
        float FrostLineAu        = 0.0f;
        float FrostLineAuSquared = 0.0f;
        if (System.StarsData().size() > 1)
        {
            const Astro::AStar* Current  = System.StarsData()[StarIndex].get();
            const Astro::AStar* TheOther = System.StarsData()[1 - StarIndex].get();

            float CurrentPrevMainSequenceLuminosity  = CalculatePrevMainSequenceLuminosity(Current->GetInitialMass()  / kSolarMass);
            float TheOtherPrevMainSequenceLuminosity = CalculatePrevMainSequenceLuminosity(TheOther->GetInitialMass() / kSolarMass);

            FrostLineAuSquared = (CurrentPrevMainSequenceLuminosity /
                                  (4 * Math::kPi * ((kStefanBoltzmann * std::pow(270.0f, 4.0f)) - TheOtherPrevMainSequenceLuminosity /
                                                    (4 * Math::kPi * std::pow(BinarySemiMajorAxis, 2.0f)))));
        }
        else
        {
            float PrevMainSequenceLuminosity = CalculatePrevMainSequenceLuminosity(StarInitialMassSol);
            FrostLineAuSquared = (PrevMainSequenceLuminosity / (4 * Math::kPi * kStefanBoltzmann * std::pow(270.0f, 4.0f)));
        }

        FrostLineAu = std::sqrt(FrostLineAuSquared) / kAuToMeter;

#ifdef DEBUG_OUTPUT
        std::println("Frost line: {} AU", FrostLineAu);
        std::println("");
#endif // DEBUG_OUTPUT

        return FrostLineAu;
    }

    void FOrbitalGenerator::MigratePlanets(const Astro::AStar* Star, const FPlanetaryDisk& PlanetaryDisk,
                                           std::size_t PlanetCount, float MigratedOriginSemiMajorAxisAu,
                                           std::vector<float>& CoreMassesSol, std::vector<float>& NewCoreMassesSol,
                                           std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits,
                                           std::vector<std::unique_ptr<Astro::APlanet>>& Planets)
    {
        float StarInitialMassSol = Star->GetInitialMass() / kSolarMass;
        for (std::size_t i = 1; i < PlanetCount; ++i)
        {
            auto PlanetType = Planets[i]->GetPlanetType();
            if (PlanetType == Astro::APlanet::EPlanetType::kIceGiant || PlanetType == Astro::APlanet::EPlanetType::kGasGiant)
            {
                if (MigrationProbability_(RandomEngine_))
                {
                    int MigrationIndex = 0;
                    if (WalkInProbability_(RandomEngine_))
                    {
                        // 夺舍，随机生成在该行星之前的位置
                        MigrationIndex = static_cast<int>(CommonGenerator_(RandomEngine_) * (i - 1));
                    }
                    else
                    {
                        // 不夺舍，直接迁移到最近轨道
                        float Coefficient = 0.0f;
                        float StarMassSol = static_cast<float>(Star->GetMass() / kSolarMass);
                        if (StarMassSol < 0.6)
                        {
                            Coefficient = 2.0f;
                        }
                        else if (StarMassSol < 1.2)
                        {
                            Coefficient = 10.0f;
                        }
                        else
                        {
                            Coefficient = 7.0f;
                        }
                        float Lower = std::log10(PlanetaryDisk.InnerRadiusAu / Coefficient);
                        float Upper = std::log10(PlanetaryDisk.InnerRadiusAu * 0.67f);
                        float Exponent = Lower + CommonGenerator_(RandomEngine_) * (Upper - Lower);
                        Orbits[0]->SetSemiMajorAxis(std::pow(10.0f, Exponent) * kAuToMeter);
                    }

                    // 迁移到指定位置
                    Planets[i]->SetMigration(true);
                    Planets[MigrationIndex] = std::move(Planets[i]);
                    CoreMassesSol[MigrationIndex] = CoreMassesSol[i];
                    NewCoreMassesSol[MigrationIndex] = NewCoreMassesSol[i];
                    MigratedOriginSemiMajorAxisAu = Orbits[i]->GetSemiMajorAxis() / kAuToMeter;
                    // 抹掉内迁途中的经过的其他行星
                    Planets.erase(Planets.begin() + MigrationIndex + 1, Planets.begin() + i + 1);
                    Orbits.erase(Orbits.begin() + MigrationIndex + 1, Orbits.begin() + i + 1);
                    CoreMassesSol.erase(CoreMassesSol.begin() + MigrationIndex + 1, CoreMassesSol.begin() + i + 1);
                    NewCoreMassesSol.erase(NewCoreMassesSol.begin() + MigrationIndex + 1, NewCoreMassesSol.begin() + i + 1);

                    PlanetCount = Planets.size();
                    break; // 只内迁一个行星
                }
                else
                {
                    break; // 给你机会你不中用
                }
            }
        }
    }

    void FOrbitalGenerator::DevourPlanets(const Astro::AStar* Star, std::size_t PlanetCount,
                                          std::vector<float>& CoreMassesSol, std::vector<float>& NewCoreMassesSol,
                                          std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits,
                                          std::vector<std::unique_ptr<Astro::APlanet>>& Planets)
    {
        float StarRadiusMaxSol   = 0.0f; // 恒星膨胀过程中达到的最大半径
        float StarInitialMassSol = Star->GetInitialMass() / kSolarMass;
        if (std::to_underlying(Star->GetEvolutionPhase()) <= 1)
        {
            EraseLimitedPlanets(Star->GetRadius(), PlanetCount, CoreMassesSol, NewCoreMassesSol, Orbits, Planets);
        }
        else
        {
            if (StarInitialMassSol < 0.75f)
            {
                StarRadiusMaxSol = 104 * std::pow(2.0f * StarInitialMassSol, 3.0f) + 0.1f;
            }
            else
            {
                StarRadiusMaxSol = 400 * std::pow(StarInitialMassSol - 0.75f, 1.0f / 3.0f);
            }

#ifdef DEBUG_OUTPUT
            std::println("Max star radius: {} solar", StarRadiusMaxSol);
            std::println("");
#endif // DEBUG_OUTPUT

            EraseLimitedPlanets(StarRadiusMaxSol * kSolarRadius, PlanetCount, CoreMassesSol, NewCoreMassesSol, Orbits, Planets);
        }

        // 同时判断被烤焦的冥府行星
        for (std::size_t i = 0; i != PlanetCount; ++i)
        {
            if ((Planets[i]->GetPlanetType() == Astro::APlanet::EPlanetType::kGasGiant ||
                 Planets[i]->GetPlanetType() == Astro::APlanet::EPlanetType::kIceGiant) &&
                Star->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kWhiteDwarf &&
                Orbits[i]->GetSemiMajorAxis() < 2.0f * StarRadiusMaxSol * kSolarRadius)
            {
                Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kChthonian);
                NewCoreMassesSol[i] = CoreMassesSol[i];
                CalculatePlanetRadius(CoreMassesSol[i] * kSolarMassToEarth, Planets[i].get());
            }
        }
    }

    void FOrbitalGenerator::GenerateOrbitElements(Astro::FOrbit& Orbit)
    {
        if (!Orbit.GetEccentricity())
        {
            Orbit.SetEccentricity(CommonGenerator_(RandomEngine_) * 0.05f);
        }

        if (!Orbit.GetInclination())
        {
            Orbit.SetInclination(CommonGenerator_(RandomEngine_) * 4.0f - 2.0f);
        }

        if (!Orbit.GetLongitudeOfAscendingNode())
        {
            Orbit.SetLongitudeOfAscendingNode(CommonGenerator_(RandomEngine_) * 2 * Math::kPi);
        }

        if (!Orbit.GetArgumentOfPeriapsis())
        {
            Orbit.SetArgumentOfPeriapsis(CommonGenerator_(RandomEngine_) * 2 * Math::kPi);
        }

        if (!Orbit.GetTrueAnomaly())
        {
            Orbit.SetTrueAnomaly(CommonGenerator_(RandomEngine_) * 2 * Math::kPi);
        }
    }

    std::size_t FOrbitalGenerator::JudgeLargePlanets(std::size_t StarIndex, const std::vector<std::unique_ptr<Astro::AStar>>& StarData,
                                                     float BinarySemiMajorAxis, float InnerHabitableZoneRadiusAu, float FrostLineAu,
                                                     std::vector<float>& CoreMassesSol, std::vector<float>& NewCoreMassesSol,
                                                     std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits,
                                                     std::vector<std::unique_ptr<Astro::APlanet>>& Planets)
    {
        const Astro::AStar* Star        = StarData[StarIndex].get();
        auto                StellarType = Star->GetStellarClass().GetStellarType();
        std::size_t         PlanetCount = CoreMassesSol.size();

        for (std::size_t i = 0; i < PlanetCount; ++i)
        {
            if (Planets[i]->GetPlanetType() != Astro::APlanet::EPlanetType::kRockyAsteroidCluster &&
                Orbits[i]->GetSemiMajorAxis() / kAuToMeter > FrostLineAu)
            {
                NewCoreMassesSol[i] = CoreMassesSol[i] * 2.35f;
            }
            else
            {
                NewCoreMassesSol[i] = CoreMassesSol[i];
            }

            // 计算前主序平衡温度
            float PlanetBalanceTemperatureWhenStarAtPrevMainSequence            = 0.0f;
            float PlanetBalanceTemperatureWhenStarAtPrevMainSequenceQuadraticed = 0.0f;

            if (StarData.size() == 1)
            {
                float PrevMainSequenceLuminosity = CalculatePrevMainSequenceLuminosity(Star->GetInitialMass() / kSolarMass);
                PlanetBalanceTemperatureWhenStarAtPrevMainSequenceQuadraticed =
                    PrevMainSequenceLuminosity / (4 * Math::kPi * std::pow(Orbits[i]->GetSemiMajorAxis(), 2.0f)) / kStefanBoltzmann;
            }
            else
            {
                const Astro::AStar* Current  = StarData[StarIndex].get();
                const Astro::AStar* TheOther = StarData[1 - StarIndex].get();

                float CurrentPrevMainSequenceLuminosity  =
                    CalculatePrevMainSequenceLuminosity(Current->GetInitialMass() / kSolarMass);
                float TheOtherPrevMainSequenceLuminosity =
                    CalculatePrevMainSequenceLuminosity(TheOther->GetInitialMass() / kSolarMass);

                PlanetBalanceTemperatureWhenStarAtPrevMainSequenceQuadraticed =
                    (CurrentPrevMainSequenceLuminosity  / (4 * Math::kPi * std::pow(Orbits[i]->GetSemiMajorAxis(), 2.0f)) +
                     TheOtherPrevMainSequenceLuminosity / (4 * Math::kPi * std::pow(BinarySemiMajorAxis, 2.0f))) / kStefanBoltzmann;
            }

            PlanetBalanceTemperatureWhenStarAtPrevMainSequence =
                std::pow(PlanetBalanceTemperatureWhenStarAtPrevMainSequenceQuadraticed, 0.25f);

            // 开除大行星
            if (NewCoreMassesSol[i] * kSolarMass < AsteroidUpperLimit_ ||
                Planets[i]->GetPlanetType() == Astro::APlanet::EPlanetType::kRockyAsteroidCluster)
            {
                if (NewCoreMassesSol[i] * kSolarMass < 1e19f)
                {
                    Orbits.erase(Orbits.begin() + i);
                    Planets.erase(Planets.begin() + i);
                    NewCoreMassesSol.erase(NewCoreMassesSol.begin() + i);
                    CoreMassesSol.erase(CoreMassesSol.begin() + i);
                    --i;
                    --PlanetCount;
                    continue;
                }

                if (std::to_underlying(Star->GetEvolutionPhase()) < 1 && Orbits[i]->GetSemiMajorAxis() / kAuToMeter > FrostLineAu)
                {
                    Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster);
                }
                else
                {
                    Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kRockyAsteroidCluster);
                }
            }
            else
            {
                if (Planets[i]->GetPlanetType() != Astro::APlanet::EPlanetType::kRockyAsteroidCluster    &&
                    Planets[i]->GetPlanetType() != Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster &&
                    CoreMassesSol[i] * kSolarMassToEarth < 0.1f && AsteroidBeltProbability_(RandomEngine_))
                {
                    if (std::to_underlying(Star->GetEvolutionPhase()) < 1 && Orbits[i]->GetSemiMajorAxis() / kAuToMeter > FrostLineAu)
                    {
                        Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster);
                    }
                    else
                    {
                        Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kRockyAsteroidCluster);
                    }

                    float Exponent            = -3.0f + CommonGenerator_(RandomEngine_) * 3.0f;
                    float DiscountCoefficient = std::pow(10.0f, Exponent);
                    CoreMassesSol[i]    *= DiscountCoefficient; // 对核心质量打个折扣
                    NewCoreMassesSol[i] *= DiscountCoefficient;
                }
                else
                {
                    // 计算初始核心半径
                    if (Orbits[i]->GetSemiMajorAxis() / kAuToMeter < FrostLineAu)
                    {
                        Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kRocky);
                    }
                    else
                    {
                        Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kIcePlanet);
                    }

                    CalculatePlanetRadius(NewCoreMassesSol[i] * kSolarMassToEarth, Planets[i].get());
                    if (Star->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kBlackHole ||
                        Star->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kNeutronStar)
                    {
                        continue;
                    }

                    float CommonCoefficient = PlanetBalanceTemperatureWhenStarAtPrevMainSequence * 4.638759e16f;

                    if ((NewCoreMassesSol[i] * kSolarMass / Planets[i]->GetRadius()) > (CommonCoefficient / 4.0f))
                    {
                        Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kGasGiant);
                    }
                    else if ((NewCoreMassesSol[i] * kSolarMass / Planets[i]->GetRadius()) > (CommonCoefficient / 8.0f))
                    {
                        Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kIceGiant);
                    }
                    else
                    {
                        if ((CoreMassesSol[i] * kSolarMass / Planets[i]->GetRadius()) > (CommonCoefficient / 18.0f) &&
                            Orbits[i]->GetSemiMajorAxis() / kAuToMeter > InnerHabitableZoneRadiusAu &&
                            Orbits[i]->GetSemiMajorAxis() / kAuToMeter < FrostLineAu &&
                            std::to_underlying(Star->GetEvolutionPhase()) < 1)
                        {
                            Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kOceanic);
                        }
                        else
                        {
                            if (Orbits[i]->GetSemiMajorAxis() / kAuToMeter > FrostLineAu)
                            {
                                Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kIcePlanet);
                            }
                            else
                            {
                                Planets[i]->SetPlanetType(Astro::APlanet::EPlanetType::kRocky);
                            }

                            CalculatePlanetRadius(NewCoreMassesSol[i] * kSolarMassToEarth, Planets[i].get());
                        }
                    }
                }
            }
        }

        return PlanetCount;
    }

    float FOrbitalGenerator::CalculatePlanetMass(float CoreMass, float NewCoreMass, float SemiMajorAxisAu,
                                                 const FPlanetaryDisk& PlanetaryDisk,
                                                 const Astro::AStar* Star, Astro::APlanet* Planet)
    {
        float Random1 = 0.0f;
        float Random2 = 0.0f;
        float Random3 = 0.0f;

        float AtmosphereMassVolatiles        = 0.0f;
        float AtmosphereMassEnergeticNuclide = 0.0f;
        float AtmosphereMassZ                = 0.0f;
        float CoreMassVolatiles              = 0.0f;
        float CoreMassEnergeticNuclide       = 0.0f;
        float CoreMassZ                      = 0.0f;
        float OceanMassVolatiles             = 0.0f;
        float OceanMassEnergeticNuclide      = 0.0f;
        float OceanMassZ                     = 0.0f;

        auto CalculateRockyMass = [&]() -> float
        {
            Random2 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;
            Random3 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;

            CoreMassVolatiles        = CoreMass * 1e-4f * Random2;
            CoreMassEnergeticNuclide = CoreMass * 2e-7f * Random3;
            CoreMassZ                = CoreMass - CoreMassVolatiles - CoreMassEnergeticNuclide;

            Planet->SetCoreMass({
                boost::multiprecision::uint128_t(CoreMassZ),
                boost::multiprecision::uint128_t(CoreMassVolatiles),
                boost::multiprecision::uint128_t(CoreMassEnergeticNuclide)
            });

            return (CoreMassVolatiles + CoreMassEnergeticNuclide + CoreMassZ) / kEarthMass;
        };

        auto CalculateIcePlanetMass = [&]() -> float
        {
            Random2 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;
            Random3 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;

            if (std::to_underlying(Star->GetEvolutionPhase()) < 1)
            {
                OceanMassVolatiles        = CoreMass * 0.15f;
                OceanMassEnergeticNuclide = CoreMass * 0.15f * 5e-5f;
                OceanMassZ                = CoreMass * 1.35f - OceanMassVolatiles - OceanMassEnergeticNuclide;
            }
            else
            {
                Planet->SetPlanetType(Astro::APlanet::EPlanetType::kRocky);
            }

            CoreMassVolatiles        = CoreMass * 1e-4f * Random2;
            CoreMassEnergeticNuclide = CoreMass * 5e-6f * Random3;
            CoreMassZ                = CoreMass - CoreMassVolatiles - CoreMassEnergeticNuclide;

            Planet->SetOceanMass({
                boost::multiprecision::uint128_t(OceanMassZ),
                boost::multiprecision::uint128_t(OceanMassVolatiles),
                boost::multiprecision::uint128_t(OceanMassEnergeticNuclide)
            });

            Planet->SetCoreMass({
                boost::multiprecision::uint128_t(CoreMassZ),
                boost::multiprecision::uint128_t(CoreMassVolatiles),
                boost::multiprecision::uint128_t(CoreMassEnergeticNuclide)
            });

            return (OceanMassVolatiles + OceanMassEnergeticNuclide + OceanMassZ +
                    CoreMassVolatiles  + CoreMassEnergeticNuclide  + CoreMassZ) / kEarthMass;
        };

        auto CalculateOceanicMass = [&]() -> float
        {
            Random1 =        CommonGenerator_(RandomEngine_) * 1.35f;
            Random2 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;
            Random3 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;

            if (std::to_underlying(Star->GetEvolutionPhase()) < 1)
            {
                OceanMassVolatiles        = (CoreMass * Random1) / 9.0f;
                OceanMassEnergeticNuclide = 5e-5f * OceanMassVolatiles;
                OceanMassZ                = CoreMass * Random1 - OceanMassVolatiles - OceanMassEnergeticNuclide;
            }
            else
            {
                Planet->SetPlanetType(Astro::APlanet::EPlanetType::kRocky);
            }

            CoreMassVolatiles        = CoreMass * 1e-4f * Random2;
            CoreMassEnergeticNuclide = CoreMass * 5e-6f * Random3;
            CoreMassZ                = CoreMass - CoreMassVolatiles - CoreMassEnergeticNuclide;

            Planet->SetOceanMass({
                boost::multiprecision::uint128_t(OceanMassZ),
                boost::multiprecision::uint128_t(OceanMassVolatiles),
                boost::multiprecision::uint128_t(OceanMassEnergeticNuclide)
            });

            Planet->SetCoreMass({
                boost::multiprecision::uint128_t(CoreMassZ),
                boost::multiprecision::uint128_t(CoreMassVolatiles),
                boost::multiprecision::uint128_t(CoreMassEnergeticNuclide)
            });

            return (OceanMassVolatiles + OceanMassEnergeticNuclide + OceanMassZ +
                    CoreMassVolatiles  + CoreMassEnergeticNuclide  + CoreMassZ) / kEarthMass;
        };

        auto CalculateIceGiantMass = [&]() -> float
        {
            Random1 = 2.0f + CommonGenerator_(RandomEngine_) * (std::log10(20.0f) - std::log10(2.0f));
            Random2 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;
            Random3 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;

            float CommonCoefficient = (0.5f + 0.5f * (SemiMajorAxisAu - PlanetaryDisk.InnerRadiusAu) /
                                       (PlanetaryDisk.OuterRadiusAu - PlanetaryDisk.InnerRadiusAu)) * Random1;

            AtmosphereMassVolatiles        = (NewCoreMass - CoreMass) / 9.0f + CoreMass * CommonCoefficient / 6.0f;
            AtmosphereMassEnergeticNuclide = 5e-5f * AtmosphereMassVolatiles;
            AtmosphereMassZ                = CoreMass * CommonCoefficient + (NewCoreMass - CoreMass) - AtmosphereMassVolatiles - AtmosphereMassEnergeticNuclide;

            CoreMassVolatiles        = CoreMass * 1e-4f * Random2;
            CoreMassEnergeticNuclide = CoreMass * 5e-6f * Random3;
            CoreMassZ                = CoreMass - CoreMassVolatiles - CoreMassEnergeticNuclide;

            Planet->SetAtmosphereMass({
                boost::multiprecision::uint128_t(AtmosphereMassZ),
                boost::multiprecision::uint128_t(AtmosphereMassVolatiles),
                boost::multiprecision::uint128_t(AtmosphereMassEnergeticNuclide)
            });

            Planet->SetCoreMass({
                boost::multiprecision::uint128_t(CoreMassZ),
                boost::multiprecision::uint128_t(CoreMassVolatiles),
                boost::multiprecision::uint128_t(CoreMassEnergeticNuclide)
            });

            Planet->SetPlanetType(Astro::APlanet::EPlanetType::kIceGiant);

            return (AtmosphereMassVolatiles + AtmosphereMassEnergeticNuclide + AtmosphereMassZ +
                    CoreMassVolatiles       + CoreMassEnergeticNuclide       + CoreMassZ) / kEarthMass;
        };

        auto CalculateGasGiantMass = [&]() -> float
        {
            Random1 = 7.0f + CommonGenerator_(RandomEngine_) * (std::min(50.0f, 1.0f / 0.0142f * std::pow(10.0f, Star->GetFeH())) - 7.0f);
            Random2 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;
            Random3 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;

            float CommonCoefficient = (0.5f + 0.5f * (SemiMajorAxisAu - PlanetaryDisk.InnerRadiusAu) /
                                       (PlanetaryDisk.OuterRadiusAu - PlanetaryDisk.InnerRadiusAu)) * Random1;

            AtmosphereMassZ                = (0.0142f * std::pow(10.0f, Star->GetFeH())) * CoreMass * CommonCoefficient + (1.0f - (1.0f + 5e-5f) / 9.0f) * (NewCoreMass - CoreMass);
            AtmosphereMassEnergeticNuclide = 5e-5f * (CoreMass * CommonCoefficient + (NewCoreMass - CoreMass) / 9.0f);
            AtmosphereMassVolatiles        = CoreMass * CommonCoefficient + (NewCoreMass - CoreMass) - AtmosphereMassZ - AtmosphereMassEnergeticNuclide;

            CoreMassVolatiles        = CoreMass * 1e-4f * Random2;
            CoreMassEnergeticNuclide = CoreMass * 5e-6f * Random3;
            CoreMassZ                = CoreMass - CoreMassVolatiles - CoreMassEnergeticNuclide;

            Planet->SetAtmosphereMass({
                boost::multiprecision::uint128_t(AtmosphereMassZ),
                boost::multiprecision::uint128_t(AtmosphereMassVolatiles),
                boost::multiprecision::uint128_t(AtmosphereMassEnergeticNuclide)
            });

            Planet->SetCoreMass({
                boost::multiprecision::uint128_t(CoreMassZ),
                boost::multiprecision::uint128_t(CoreMassVolatiles),
                boost::multiprecision::uint128_t(CoreMassEnergeticNuclide)
            });

            Planet->SetPlanetType(Astro::APlanet::EPlanetType::kGasGiant);

            return (AtmosphereMassVolatiles + AtmosphereMassEnergeticNuclide + AtmosphereMassZ +
                    CoreMassVolatiles       + CoreMassEnergeticNuclide       + CoreMassZ) / kEarthMass;
        };

        auto CalculateRockyAsteroidMass = [&]() -> float
        {
            Random3 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;

            CoreMassVolatiles        = 0.0f;
            CoreMassEnergeticNuclide = CoreMass * 5e-6f * Random3;
            CoreMassZ                = CoreMass - CoreMassVolatiles - CoreMassEnergeticNuclide;

            Planet->SetCoreMass({
                boost::multiprecision::uint128_t(CoreMassZ),
                boost::multiprecision::uint128_t(CoreMassVolatiles),
                boost::multiprecision::uint128_t(CoreMassEnergeticNuclide)
            });

            return (CoreMassVolatiles + CoreMassEnergeticNuclide + CoreMassZ) / kEarthMass;
        };

        auto CalculateRockyIceAsteroidMass = [&]() -> float
        {
            Random2 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;
            Random3 = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;

            CoreMassVolatiles        = CoreMass *    0.15f * Random2;
            CoreMassEnergeticNuclide = CoreMass * 1.25e-5f * Random3;
            CoreMassZ                = NewCoreMass - CoreMassVolatiles - CoreMassEnergeticNuclide;

            Planet->SetCoreMass({
                boost::multiprecision::uint128_t(CoreMassZ),
                boost::multiprecision::uint128_t(CoreMassVolatiles),
                boost::multiprecision::uint128_t(CoreMassEnergeticNuclide)
            });

            return (CoreMassVolatiles + CoreMassEnergeticNuclide + CoreMassZ) / kEarthMass;
        };

        auto PlanetType = Planet->GetPlanetType();
        switch (PlanetType)
        {
        case Astro::APlanet::EPlanetType::kRocky:
            return CalculateRockyMass();
        case Astro::APlanet::EPlanetType::kIcePlanet:
            return CalculateIcePlanetMass();
        case Astro::APlanet::EPlanetType::kOceanic:
            return CalculateOceanicMass();
        case Astro::APlanet::EPlanetType::kIceGiant:
            return CalculateIceGiantMass();
        case Astro::APlanet::EPlanetType::kGasGiant:
            return CalculateGasGiantMass();
        case Astro::APlanet::EPlanetType::kRockyAsteroidCluster:
            return CalculateRockyAsteroidMass();
        case Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster:
            return CalculateRockyIceAsteroidMass();
        default:
            return 0.0f;
        }
    }

    void FOrbitalGenerator::CalculatePlanetRadius(float MassEarth, Astro::APlanet* Planet)
    {
        float RadiusEarth = 0.0f;
        auto  PlanetType  = Planet->GetPlanetType();

        switch (PlanetType)
        {
        case Astro::APlanet::EPlanetType::kRocky:
        case Astro::APlanet::EPlanetType::kTerra:
        case Astro::APlanet::EPlanetType::kChthonian:
            if (MassEarth < 1.0f)
            {
                RadiusEarth = 1.94935f * std::pow(10.0f, (std::log10(MassEarth) / 3 - 0.0804f * std::pow(MassEarth, 0.394f) - 0.20949f));
            }
            else
            {
                RadiusEarth = std::pow(MassEarth, 1.0f / 3.7f);
            }
            break;
        case Astro::APlanet::EPlanetType::kIcePlanet:
        case Astro::APlanet::EPlanetType::kOceanic:
            if (MassEarth < 1.0f)
            {
                RadiusEarth = 2.53536f * std::pow(10.0f, (std::log10(MassEarth) / 3 - 0.0807f * std::pow(MassEarth, 0.375f) - 0.209396f));
            }
            else
            {
                RadiusEarth = 1.3f * std::pow(MassEarth, 1.0f / 3.905f);
            }
            break;
        case Astro::APlanet::EPlanetType::kIceGiant:
        case Astro::APlanet::EPlanetType::kSubIceGiant:
        case Astro::APlanet::EPlanetType::kGasGiant:
            if (MassEarth < 6.2f)
            {
                RadiusEarth = 1.41f * std::pow(MassEarth, 1.0f / 3.905f);
            }
            else if (MassEarth < 15.0f)
            {
                RadiusEarth = 0.6f * std::pow(MassEarth, 0.72f);
            }
            else
            {
                float CommonCoefficient = MassEarth / (kJupiterMass / kEarthMass);
                RadiusEarth = 11.0f * (0.96f + 0.21f * std::log10(CommonCoefficient) -
                                       0.2f * std::pow(std::log10(CommonCoefficient), 2.0f) +
                                       0.1f * std::pow(CommonCoefficient, 0.215f));
            }
            break;
        default:
            break;
        }

        float Radius = RadiusEarth * kEarthRadius;
        Planet->SetRadius(Radius);
    }

    void FOrbitalGenerator::GenerateSpin(float SemiMajorAxis, const Astro::FOrbit::FOrbitalObject& Parent, Astro::APlanet* Planet)
    {
        auto PlanetType = Planet->GetPlanetType();
        if (PlanetType == Astro::APlanet::EPlanetType::kRockyAsteroidCluster ||
            PlanetType == Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster)
        {
            return;
        }

        float PlanetMass = Planet->GetMassDigital<float>();
        auto  ParentType = Parent.GetObjectType();

        float TimeToTidalLock = 0.0f;
        float ViscosityCoefficient = 1e12f;
        if (PlanetType == Astro::APlanet::EPlanetType::kIcePlanet || PlanetType == Astro::APlanet::EPlanetType::kOceanic)
        {
            ViscosityCoefficient = 4e9f;
        }
        else if (PlanetType == Astro::APlanet::EPlanetType::kRocky || PlanetType == Astro::APlanet::EPlanetType::kTerra || PlanetType == Astro::APlanet::EPlanetType::kChthonian)
        {
            ViscosityCoefficient = 3e10f;
        }

        float ParentAge    = 0.0f;
        float ParentMass   = 0.0f;
        float PlanetRadius = Planet->GetRadius();

        switch (ParentType)
        {
        case Astro::FOrbit::EObjectType::kStar:
            ParentAge  = static_cast<float>(Parent.GetObject<Astro::AStar>()->GetAge());
            ParentMass = static_cast<float>(Parent.GetObject<Astro::AStar>()->GetMass());
            break;
        case Astro::FOrbit::EObjectType::kPlanet:
            ParentAge  = static_cast<float>(Parent.GetObject<Astro::APlanet>()->GetAge());
            ParentMass = static_cast<float>(Parent.GetObject<Astro::APlanet>()->GetMass());
            break;
        }

        // 计算潮汐锁定时标
        double Term1    = 0.61435 * PlanetMass * std::pow(SemiMajorAxis, 6);
        double Term2    = 1 + (5.963361e11 * ViscosityCoefficient * std::pow(PlanetRadius, 4)) / std::pow(PlanetMass, 2);
        double Term3    = std::pow(ParentMass, 2) * std::pow(PlanetRadius, 3);
        TimeToTidalLock = static_cast<float>((Term1 * Term2) / Term3);

        float Spin = 0.0f;

        if (TimeToTidalLock < ParentAge)
        {
            Spin = -1.0f; // 使用 -1.0 来标记潮汐锁定
        }

        // 计算自转
        if (Spin != -1.0f)
        {
            float OrbitalPeriod = 2.0f * Math::kPi * std::sqrt(std::pow(SemiMajorAxis, 3.0f) / (kGravityConstant * ParentMass));
            float InitialSpin   = 0.0f;
            if (PlanetType == Astro::APlanet::EPlanetType::kGasGiant ||
                PlanetType == Astro::APlanet::EPlanetType::kHotGasGiant)
            {
                InitialSpin = 21600.0f + CommonGenerator_(RandomEngine_) * (43200.0f - 21600.0f);
            }
            else
            {
                InitialSpin = 28800.0f + CommonGenerator_(RandomEngine_) * (86400.0f - 28800.0f);
            }
            Spin = InitialSpin + (OrbitalPeriod - InitialSpin) * static_cast<float>(std::pow(ParentAge / TimeToTidalLock, 2.35));

            float Oblateness = 4.0f * std::pow(Math::kPi, 2.0f) * std::pow(PlanetRadius, 3.0f);
            Oblateness /= (std::pow(Spin, 2.0f) * kGravityConstant * PlanetMass);
            Planet->SetOblateness(Oblateness);
        }

        Planet->SetSpin(Spin);
    }

    void FOrbitalGenerator::CalculateTemperature(const Astro::FOrbit::EObjectType ParentType, float PoyntingVector, Astro::APlanet* Planet)
    {
        auto PlanetType = Planet->GetPlanetType();
        if (PlanetType == Astro::APlanet::EPlanetType::kRockyAsteroidCluster ||
            PlanetType == Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster)
        {
            return;
        }

        float Albedo     = 0.0f;
        float Emissivity = 0.0f;
        float PlanetMass = Planet->GetMassDigital<float>();
        float Spin       = Planet->GetSpin();

        if (PlanetType == Astro::APlanet::EPlanetType::kSubIceGiant    ||
            PlanetType == Astro::APlanet::EPlanetType::kIceGiant       ||
            PlanetType == Astro::APlanet::EPlanetType::kGasGiant       ||
            PlanetType == Astro::APlanet::EPlanetType::kHotSubIceGiant ||
            PlanetType == Astro::APlanet::EPlanetType::kHotIceGiant    ||
            PlanetType == Astro::APlanet::EPlanetType::kHotGasGiant)
        {
            if (PoyntingVector <= 170)
            {
                Albedo = 0.34f;
            }
            else if (PoyntingVector <= 200)
            {
                Albedo = 0.0156667f * PoyntingVector - 2.32333f;
            }
            else if (PoyntingVector <= 3470)
            {
                Albedo = 0.75f;
            }
            else if (PoyntingVector <= 3790)
            {
                Albedo = 7.58156f - 0.00196875f * PoyntingVector;
            }
            else if (PoyntingVector <= 103500)
            {
                Albedo = 0.12f;
            }
            else if (PoyntingVector <= 150000)
            {
                Albedo = 0.320323f - 1.93548e-6f * PoyntingVector;
            }
            else if (PoyntingVector <= 654000)
            {
                Albedo = 0.03f;
            }
            else if (PoyntingVector <= 1897000)
            {
                Albedo = 4.18343e-7f * PoyntingVector - 0.243596f;
            }
            else
            {
                Albedo = 0.55f;
            }

            Emissivity = 0.98f;
        }
        else if (!Utils::Equal(Planet->GetAtmosphereMassDigital<float>(), 0.0f))
        {
            float AtmospherePressureAtm = (kGravityConstant * PlanetMass * Planet->GetAtmosphereMassDigital<float>()) /
                                          (4 * Math::kPi * std::pow(Planet->GetRadius(), 4.0f)) / kPascalToAtm;
            float Random                = 0.9f + CommonGenerator_(RandomEngine_) * 0.2f;
            float TidalLockCoefficient  = 0.0f;
            if (ParentType == Astro::FOrbit::EObjectType::kStar)
            {
                TidalLockCoefficient = Utils::Equal(Spin, -1.0f) ? 2.0f : 1.0f;
            }
            else
            {
                TidalLockCoefficient = 1.0f;
            }

            if (PlanetType == Astro::APlanet::EPlanetType::kRocky || PlanetType == Astro::APlanet::EPlanetType::kChthonian)
            {
                Albedo = Random * std::min(0.7f, 0.12f + 0.2f * std::sqrt(TidalLockCoefficient * AtmospherePressureAtm));
                Emissivity = std::max(0.012f, 0.95f - 0.35f * std::pow(AtmospherePressureAtm, 0.25f));
            }
            else if (PlanetType == Astro::APlanet::EPlanetType::kOceanic || PlanetType == Astro::APlanet::EPlanetType::kTerra)
            {
                Albedo = Random * std::min(0.7f, 0.07f + 0.2f * std::sqrt(TidalLockCoefficient * AtmospherePressureAtm));
                Emissivity = std::max(0.1f, 0.98f - 0.35f * std::pow(AtmospherePressureAtm, 0.25f));
            }
            else if (PlanetType == Astro::APlanet::EPlanetType::kIcePlanet)
            {
                Albedo = Random * std::max(0.2f, 0.4f - 0.1f * std::sqrt(AtmospherePressureAtm));
                Emissivity = std::max(0.1f, 0.98f - 0.35f * std::pow(AtmospherePressureAtm, 0.25f));
            }
        }
        else if (Utils::Equal(Planet->GetAtmosphereMassDigital<float>(), 0.0f))
        {
            if (PlanetType == Astro::APlanet::EPlanetType::kRocky || PlanetType == Astro::APlanet::EPlanetType::kChthonian)
            {
                Albedo = 0.12f * (0.9f + CommonGenerator_(RandomEngine_) * 0.2f);
                Emissivity = 0.95f;
            }
            else if (PlanetType == Astro::APlanet::EPlanetType::kIcePlanet)
            {
                Albedo = 0.4f + CommonGenerator_(RandomEngine_) * (0.98f - 0.4f);
                Emissivity = 0.98f;
            }
        }

        // 计算平衡温度
        float BalanceTemperature = std::pow((PoyntingVector * (1.0f - Albedo)) / (4.0f * kStefanBoltzmann * Emissivity), 0.25f);
        float CosmosMicrowaveBackground = (3.76119e10f) / UniverseAge_;
        if (BalanceTemperature < CosmosMicrowaveBackground)
        {
            BalanceTemperature = CosmosMicrowaveBackground;
        }

        Planet->SetBalanceTemperature(BalanceTemperature);
    }

    void FOrbitalGenerator::GenerateMoons(std::size_t PlanetIndex, float FrostLineAu, const Astro::AStar* Star, float PoyntingVector,
                                          const std::pair<float, float>& HabitableZoneAu, Astro::FOrbit::FOrbitalDetails& ParentPlanet,
                                          std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits, std::vector<std::unique_ptr<Astro::APlanet>>& Planets)
    {
        auto* Planet     = ParentPlanet.GetOrbitalObject().GetObject<Astro::APlanet>();
        auto  PlanetType = Planet->GetPlanetType();
        if (PlanetType == Astro::APlanet::EPlanetType::kRockyAsteroidCluster ||
            PlanetType == Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster)
        {
            return;
        }

        float PlanetMass        = Planet->GetMassDigital<float>();
        float PlanetMassEarth   = PlanetMass / kEarthMass;
        float LiquidRocheRadius = 2.02373e7f * std::pow(PlanetMassEarth, 1.0f / 3.0f);
        float HillSphereRadius  = Orbits[PlanetIndex]->GetSemiMajorAxis() * std::pow(3 * PlanetMass / static_cast<float>(Star->GetMass()), 1.0f / 3.0f);

        std::size_t MoonCount = 0;
        if (std::to_underlying(Star->GetEvolutionPhase()) < 1)
        {
            if (Planet->GetMassDigital<float>() > 10 * kEarthMass && HillSphereRadius / 3 - 2 * LiquidRocheRadius > 1e9)
            {
                MoonCount = static_cast<std::size_t>(CommonGenerator_(RandomEngine_) * 3.0f);
            }
            else
            {
                if (Planet->GetMassDigital<float>() > 100 * AsteroidUpperLimit_ &&
                    HillSphereRadius / 3 - 2 * LiquidRocheRadius > 3e8f)
                {
                    Utils::TBernoulliDistribution MoonProbability(
                        std::min(0.5f, 0.1f * (HillSphereRadius / 3 - 2 * LiquidRocheRadius) / 3e8f));
                    if (MoonProbability(RandomEngine_))
                    {
                        MoonCount = 1;
                    }
                }
            }
        }

        std::vector<std::unique_ptr<Astro::FOrbit>> MoonOrbits;

        if (MoonCount == 0)
        {
            return;
        }
        else if (MoonCount == 1)
        {
            Astro::FOrbit MoonOrbitData;
            MoonOrbitData.SetParent(Planet, Astro::FOrbit::EObjectType::kPlanet);
            MoonOrbitData.SetSemiMajorAxis(2 * LiquidRocheRadius + CommonGenerator_(RandomEngine_) *
                                           (std::min(1e9f, HillSphereRadius / 3 - 1e8f) - 2 * LiquidRocheRadius));
            GenerateOrbitElements(MoonOrbitData);

            glm::vec2 MoonNormal(Planet->GetNormal() + glm::vec2(
                -0.09f + CommonGenerator_(RandomEngine_) * 0.18f,
                -0.09f + CommonGenerator_(RandomEngine_) * 0.18f
            ));

            if (MoonNormal.x > 2 * Math::kPi)
            {
                MoonNormal.x -= 2 * Math::kPi;
            }
            else if (MoonNormal.x < 0.0f)
            {
                MoonNormal.x += 2 * Math::kPi;
            }

            if (MoonNormal.y > Math::kPi)
            {
                MoonNormal.y -= Math::kPi;
            }
            else if (MoonNormal.y < 0.0f)
            {
                MoonNormal.y += Math::kPi;
            }

            MoonOrbitData.SetNormal(MoonNormal);

            MoonOrbits.push_back(std::make_unique<Astro::FOrbit>(MoonOrbitData));
        }
        else if (MoonCount == 2)
        {
            std::array<Astro::FOrbit, 2> MoonOrbitData;
            MoonOrbitData[0].SetSemiMajorAxis(2 * LiquidRocheRadius + CommonGenerator_(RandomEngine_) *
                                              (7e8f - 2 * LiquidRocheRadius));
            GenerateOrbitElements(MoonOrbitData[0]);

            float Probability = CommonGenerator_(RandomEngine_);
            if (Probability >= 0.0f && Probability < 0.1f)
            {
                MoonOrbitData[1].SetSemiMajorAxis(1.587401f * MoonOrbitData[0].GetSemiMajorAxis());
            }
            else if (Probability >= 0.1f && Probability < 0.2f)
            {
                MoonOrbitData[1].SetSemiMajorAxis(2.080084f * MoonOrbitData[0].GetSemiMajorAxis());
            }
            else
            {
                MoonOrbitData[1].SetSemiMajorAxis(MoonOrbitData[0].GetSemiMajorAxis() + 2e8f + CommonGenerator_(RandomEngine_) *
                                                  (std::min(2e9f, HillSphereRadius / 3 - 1e8f) -
                                                   (MoonOrbitData[0].GetSemiMajorAxis() + 2e8f)));
            }

            GenerateOrbitElements(MoonOrbitData[1]);

            std::array<glm::vec2, 2> MoonNormals{};

            for (int i = 0; i != 2; ++i)
            {
                MoonOrbitData[i].SetParent(Planet, Astro::FOrbit::EObjectType::kPlanet);
                MoonNormals[i] = glm::vec2(Planet->GetNormal() + glm::vec2(
                    -0.09f + CommonGenerator_(RandomEngine_) * 0.18f,
                    -0.09f + CommonGenerator_(RandomEngine_) * 0.18f
                ));

                if (MoonNormals[i].x > 2 * Math::kPi)
                {
                    MoonNormals[i].x -= 2 * Math::kPi;
                }
                else if (MoonNormals[i].x < 0.0f)
                {
                    MoonNormals[i].x += 2 * Math::kPi;
                }

                if (MoonNormals[i].y > Math::kPi)
                {
                    MoonNormals[i].y -= Math::kPi;
                }
                else if (MoonNormals[i].y < 0.0f)
                {
                    MoonNormals[i].y += Math::kPi;
                }
            }

            MoonOrbitData[0].SetNormal(MoonNormals[0]);
            MoonOrbitData[1].SetNormal(MoonNormals[1]);

            MoonOrbits.push_back(std::make_unique<Astro::FOrbit>(MoonOrbitData[0]));
            MoonOrbits.push_back(std::make_unique<Astro::FOrbit>(MoonOrbitData[1]));
        }

        for (std::size_t i = 0; i != MoonCount; ++i)
        {
            ParentPlanet.DirectOrbitsData().push_back(MoonOrbits[i].get());
        }

        float ParentCoreMass        = Planet->GetCoreMassZDigital<float>();
        float LogCoreMassLowerLimit = std::log10(std::max(AsteroidUpperLimit_, ParentCoreMass / 600));
        float LogCoreMassUpperLimit = std::log10(ParentCoreMass / 30.0f);

        std::vector<std::unique_ptr<Astro::APlanet>> Moons;
        Moons.reserve(MoonCount);

        for (std::size_t i = 0; i != MoonCount; ++i)
        {
            Moons.push_back(std::make_unique<Astro::APlanet>());

            float Exponent = LogCoreMassLowerLimit + CommonGenerator_(RandomEngine_) * (LogCoreMassUpperLimit - LogCoreMassLowerLimit);
            boost::multiprecision::uint128_t InitialCoreMass(std::pow(10.0f, Exponent));

            int VolatilesRate        = 9000    + static_cast<int>(CommonGenerator_(RandomEngine_)) + 2000;
            int EnergeticNuclideRate = 4500000 + static_cast<int>(CommonGenerator_(RandomEngine_)) * 1000000;

            Astro::FComplexMass CoreMass;

            CoreMass.Volatiles        = InitialCoreMass / VolatilesRate;
            CoreMass.EnergeticNuclide = InitialCoreMass / EnergeticNuclideRate;
            CoreMass.Z                = InitialCoreMass - CoreMass.Volatiles - CoreMass.EnergeticNuclide;

            Moons[i]->SetCoreMass(CoreMass);

            if (MoonOrbits[i]->GetSemiMajorAxis() > 5 * LiquidRocheRadius)
            {
                if (Orbits[PlanetIndex]->GetSemiMajorAxis() / kAuToMeter > FrostLineAu)
                {
                    Moons[i]->SetPlanetType(Astro::APlanet::EPlanetType::kIcePlanet);
                    CalculatePlanetMass(Moons[i]->GetCoreMassDigital<float>(), 0, 0, {}, Star, Moons[i].get()); // 0, 0, {}: 这些参数只在计算气态行星时有用，此处省略
                }
                else
                {
                    Moons[i]->SetPlanetType(Astro::APlanet::EPlanetType::kRocky);
                }
            }
            else
            {
                Moons[i]->SetPlanetType(Astro::APlanet::EPlanetType::kRocky);
            }

            CalculatePlanetRadius(Moons[i]->GetCoreMassDigital<float>() / kEarthMass, Moons[i].get());

            Astro::FOrbit::FOrbitalDetails Parent(Planet, Astro::FOrbit::EObjectType::kPlanet, MoonOrbits[i].get());
            GenerateSpin(MoonOrbits[i]->GetSemiMajorAxis(), Parent.GetOrbitalObject(), Moons[i].get());

            CalculateTemperature(Astro::FOrbit::EObjectType::kPlanet, PoyntingVector, Moons[i].get());

            if (Star->GetStellarClass().GetStellarType() == Astro::FStellarClass::EStellarType::kNormalStar)
            {
                GenerateTerra(Star, PoyntingVector, HabitableZoneAu, Orbits[i].get(), Moons[i].get());
            }

            if (Moons[i]->GetPlanetType() == Astro::APlanet::EPlanetType::kTerra)
            {
                GenerateCivilization(Star, PoyntingVector, HabitableZoneAu, Orbits[i].get(), Moons[i].get());
            }
        }

        CalculateOrbitalPeriods(MoonOrbits);

#ifdef DEBUG_OUTPUT
        std::println("");

        for (std::size_t i = 0; i != MoonCount; ++i)
        {
            auto& Moon                           = Moons[i];
            auto  MoonType                       = Moon->GetPlanetType();
            float MoonMass                       = Moon->GetMassDigital<float>();
            float MoonMassEarth                  = MoonMass / kEarthMass;
            float MoonMassMoon                   = MoonMass / kMoonMass;
            float MoonRadius                     = Moons[i]->GetRadius();
            float MoonRadiusEarth                = MoonRadius / kEarthRadius;
            float MoonRadiusMoon                 = MoonRadius / kMoonRadius;
            float AtmosphereMassZ                = Moon->GetAtmosphereMassZDigital<float>();
            float AtmosphereMassVolatiles        = Moon->GetAtmosphereMassVolatilesDigital<float>();
            float AtmosphereMassEnergeticNuclide = Moon->GetAtmosphereMassEnergeticNuclideDigital<float>();
            float CoreMassZ                      = Moon->GetCoreMassZDigital<float>();
            float CoreMassVolatiles              = Moon->GetCoreMassVolatilesDigital<float>();
            float CoreMassEnergeticNuclide       = Moon->GetCoreMassEnergeticNuclideDigital<float>();
            float OceanMassZ                     = Moon->GetOceanMassZDigital<float>();;
            float OceanMassVolatiles             = Moon->GetOceanMassVolatilesDigital<float>();
            float OceanMassEnergeticNuclide      = Moon->GetOceanMassEnergeticNuclideDigital<float>();
            float CrustMineralMass               = Moon->GetCrustMineralMassDigital<float>();
            float AtmospherePressure             = (kGravityConstant * MoonMass * (AtmosphereMassZ + AtmosphereMassVolatiles + AtmosphereMassEnergeticNuclide)) / (4 * Math::kPi * std::pow(Moons[i]->GetRadius(), 4.0f));
            float Oblateness                     = Moon->GetOblateness();
            float Spin                           = Moon->GetSpin();
            float BalanceTemperature             = Moon->GetBalanceTemperature();
            std::println("Moon generated, details:");
            std::println("parent planet: {}", PlanetIndex + 1);
            std::println("semi-major axis: {} km, period: {} days, mass: {} earth ({} moon), radius: {} earth ({} moon), type: {}",
                         MoonOrbits[i]->GetSemiMajorAxis() / 1000, MoonOrbits[i]->GetPeriod() / kDayToSecond, MoonMassEarth, MoonMassMoon, MoonRadiusEarth, MoonRadiusMoon, std::to_underlying(MoonType));
            std::println("rotation period: {} h, oblateness: {}, balance temperature: {} K",
                         Spin / 3600, Oblateness, BalanceTemperature);
            std::println("atmo  mass z: {:.2E} kg, atmo  mass vol: {:.2E} kg, atmo  mass nuc: {:.2E} kg",
                         AtmosphereMassZ, AtmosphereMassVolatiles, AtmosphereMassEnergeticNuclide);
            std::println("core  mass z: {:.2E} kg, core  mass vol: {:.2E} kg, core  mass nuc: {:.2E} kg",
                         CoreMassZ, CoreMassVolatiles, CoreMassEnergeticNuclide);
            std::println("ocean mass z: {:.2E} kg, ocean mass vol: {:.2E} kg, ocean mass nuc: {:.2E} kg",
                         OceanMassZ, OceanMassVolatiles, OceanMassEnergeticNuclide);
            std::println("crust mineral mass : {:.2E} kg, atmo pressure : {:.2f} atm",
                         CrustMineralMass, AtmospherePressure / kPascalToAtm);
            std::println("");
        }
#endif // DEBUG_OUTPUT

        for (std::size_t i = 0; i != MoonCount; ++i)
        {
            Astro::FOrbit::FOrbitalDetails Moon(Moons[i].get(), Astro::FOrbit::EObjectType::kPlanet,
                                                MoonOrbits[i].get(), CommonGenerator_(RandomEngine_) * 2 * Math::kPi);
            MoonOrbits[i]->ObjectsData().push_back(std::move(Moon));
        }

        Orbits.reserve(Orbits.size() + MoonOrbits.size());
        Orbits.append_range(MoonOrbits | std::views::as_rvalue);

        Planets.reserve(Planets.size() + Moons.size());
        Planets.append_range(Moons | std::views::as_rvalue);
    }

    void FOrbitalGenerator::GenerateRings(std::size_t PlanetIndex, float FrostLineAu, const Astro::AStar* Star,
                                          Astro::FOrbit::FOrbitalDetails& ParentPlanet,
                                          std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits,
                                          std::vector<std::unique_ptr<Astro::AAsteroidCluster>>& AsteroidClusters)
    {
        auto* Planet     = ParentPlanet.GetOrbitalObject().GetObject<Astro::APlanet>();
        auto  PlanetType = Planet->GetPlanetType();
        if (PlanetType == Astro::APlanet::EPlanetType::kRockyAsteroidCluster ||
            PlanetType == Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster)
        {
            return;
        }

        float PlanetMass        = Planet->GetMassDigital<float>();
        float PlanetMassEarth   = PlanetMass / kEarthMass;
        float LiquidRocheRadius = 2.02373e7f * std::pow(PlanetMassEarth, 1.0f / 3.0f);
        float HillSphereRadius  = Orbits[PlanetIndex]->GetSemiMajorAxis() * std::pow(3.0f * PlanetMass / static_cast<float>(Star->GetMass()), 1.0f / 3.0f);

        Utils::TDistribution<double>* RingsProbability = nullptr;
        if (LiquidRocheRadius < HillSphereRadius / 3.0f && LiquidRocheRadius > Planet->GetRadius())
        {
            if (PlanetType == Astro::APlanet::EPlanetType::kGasGiant ||
                PlanetType == Astro::APlanet::EPlanetType::kIceGiant)
            {
                RingsProbability = &RingsProbabilities_[0];
            }
            else
            {
                RingsProbability = &RingsProbabilities_[1];
            }
        }

        if (RingsProbability == nullptr || !(*RingsProbability)(RandomEngine_))
        {
            return;
        }

        Astro::AAsteroidCluster::EAsteroidType AsteroidType{};
        float Exponent                  = -4.0f + CommonGenerator_(RandomEngine_) * 4.0f;
        float Random                    = std::pow(10.0f, Exponent);
        float RingsMass                 = Random * 1e20f * std::pow(LiquidRocheRadius / 1e8f, 2.0f);
        float RingsMassZ                = 0.0f;
        float RingsMassVolatiles        = 0.0f;
        float RingsMassEnergeticNuclide = 0.0f;

        if (Orbits[PlanetIndex]->GetSemiMajorAxis() / kAuToMeter >= FrostLineAu && std::to_underlying(Star->GetEvolutionPhase()) < 1)
        {
            RingsMassEnergeticNuclide = RingsMass * 5e-6f * 0.064f;
            RingsMassVolatiles        = RingsMass * 0.064f;
            RingsMassZ                = RingsMass - RingsMassVolatiles - RingsMassEnergeticNuclide;
            AsteroidType              = Astro::AAsteroidCluster::EAsteroidType::kRockyIce;
        }
        else
        {
            RingsMassEnergeticNuclide = RingsMass * 5e-6f;
            RingsMassZ                = RingsMass - RingsMassEnergeticNuclide;
            AsteroidType              = Astro::AAsteroidCluster::EAsteroidType::kRocky;
        }

        auto RingsOrbit = std::make_unique<Astro::FOrbit>();

        Astro::AAsteroidCluster* RingsPtr = AsteroidClusters.emplace_back(std::make_unique<Astro::AAsteroidCluster>()).get();
        RingsPtr->SetMassEnergeticNuclide(RingsMassEnergeticNuclide);
        RingsPtr->SetMassVolatiles(RingsMassVolatiles);
        RingsPtr->SetMassZ(RingsMassZ);

        Astro::FOrbit::FOrbitalDetails Rings(RingsPtr, Astro::FOrbit::EObjectType::kAsteroidCluster, RingsOrbit.get());

        GenerateOrbitElements(*RingsOrbit);
        float Inaccuracy    = -0.1f + CommonGenerator_(RandomEngine_) * 0.2f;
        float SemiMajorAxis =  0.6f * LiquidRocheRadius * (1.0f + Inaccuracy);

        RingsOrbit->SetParent(Planet, Astro::FOrbit::EObjectType::kPlanet);
        RingsOrbit->SetSemiMajorAxis(SemiMajorAxis);
        RingsOrbit->ObjectsData().push_back(std::move(Rings));

        ParentPlanet.DirectOrbitsData().push_back(RingsOrbit.get());
        Orbits.push_back(std::move(RingsOrbit));

#ifdef DEBUG_OUTPUT
        std::println("");
        std::println("Rings generated, details:");
        std::println("parent planet: {}", PlanetIndex + 1);
        std::println("semi-major axis: {} km, mass: {} kg, type: {}",
                     SemiMajorAxis / 1000, RingsMass, std::to_underlying(RingsPtr->GetAsteroidType()));
        std::println("mass z: {:.2E} kg, mass vol: {:.2E} kg, mass nuc: {:.2E} kg",
                     RingsMassZ, RingsMassVolatiles, RingsMassEnergeticNuclide);
        std::println("");
#endif // DEBUG_OUTPUT
    }

    void FOrbitalGenerator::GenerateTerra(const Astro::AStar* Star, float PoyntingVector,
                                          const std::pair<float, float>& HabitableZoneAu,
                                          const Astro::FOrbit* Orbit, Astro::APlanet* Planet)
    {
        auto PlanetType = Planet->GetPlanetType();
        if (PlanetType == Astro::APlanet::EPlanetType::kRockyAsteroidCluster ||
            PlanetType == Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster)
        {
            return;
        }

        float PlanetMass        = Planet->GetMassDigital<float>();
        float PlanetMassEarth   = Planet->GetMassDigital<float>() / kEarthMass;
        float CoreMass          = Planet->GetCoreMassDigital<float>();
        float Term1             = 1.6567e15f * static_cast<float>(std::pow(Star->GetLuminosity() / (4.0 * Math::kPi * kStefanBoltzmann * std::pow(Orbit->GetSemiMajorAxis(), 2.0f)), 0.25));
        float Term2             = PlanetMass / Planet->GetRadius();
        float MaxTerm           = std::max(1.0f, Term1 / Term2);
        float EscapeCoefficient = std::pow(10.0f, 1.0f - MaxTerm);

        if (PlanetType == Astro::APlanet::EPlanetType::kRocky &&
            Orbit->GetSemiMajorAxis() / kAuToMeter > HabitableZoneAu.first &&
            Orbit->GetSemiMajorAxis() / kAuToMeter < HabitableZoneAu.second &&
            EscapeCoefficient > 0.1f && std::to_underlying(Star->GetEvolutionPhase()) < 1)
        {
            // 判断类地行星
            Planet->SetPlanetType(Astro::APlanet::EPlanetType::kTerra);
            // 计算新的海洋质量
            float Exponent                     = -0.5f + CommonGenerator_(RandomEngine_) * 1.5f;
            float Random                       = std::pow(10.0f, Exponent);
            float NewOceanMass                 = CoreMass * Random * 1e-4f;
            float NewOceanMassVolatiles        = NewOceanMass / 9.0f;
            float NewOceanMassEnergeticNuclide = NewOceanMass * 5e-5f / 9.0f;
            float NewOceanMassZ                = NewOceanMass - NewOceanMassVolatiles - NewOceanMassEnergeticNuclide;

            Planet->SetOceanMass({
                boost::multiprecision::uint128_t(NewOceanMassZ),
                boost::multiprecision::uint128_t(NewOceanMassVolatiles),
                boost::multiprecision::uint128_t(NewOceanMassEnergeticNuclide)
            });
        }

        PlanetType = Planet->GetPlanetType();

        // 计算地壳矿脉
        float Random = 0.0f;
        if (PlanetType == Astro::APlanet::EPlanetType::kRocky)
        {
            Random = 0.1f + CommonGenerator_(RandomEngine_) * 0.9f;
        }
        else if (PlanetType == Astro::APlanet::EPlanetType::kTerra)
        {
            Random = 1.0f + CommonGenerator_(RandomEngine_) * 9.0f;
        }
        float CrustMineralMass = Random * 1e-9f * std::pow(PlanetMass / kEarthMass, 2.0f) * kEarthMass;
        Planet->SetCrustMineralMass(CrustMineralMass);

        // 计算次生大气
        if (std::to_underlying(Star->GetEvolutionPhase()) < 1)
        {
            if (PlanetType == Astro::APlanet::EPlanetType::kRocky   ||
                PlanetType == Astro::APlanet::EPlanetType::kTerra   ||
                PlanetType == Astro::APlanet::EPlanetType::kOceanic ||
                PlanetType == Astro::APlanet::EPlanetType::kIcePlanet)
            {
                float Exponent = CommonGenerator_(RandomEngine_);
                Random = std::pow(10.0f, Exponent);
                float NewAtmosphereMass = EscapeCoefficient * PlanetMass * Random * 1e-5f;
                if (PlanetType == Astro::APlanet::EPlanetType::kTerra)
                {
                    NewAtmosphereMass *= 0.035f;
                }
                else if (PlanetType == Astro::APlanet::EPlanetType::kIcePlanet)
                {
                    if (PoyntingVector > 8) // 保证氮气不会液化
                    {
                        NewAtmosphereMass = std::pow(EscapeCoefficient, 2.0f) * PlanetMass * Random * 1e-5f;
                    }
                    else
                    {
                        NewAtmosphereMass = 0.0f;
                    }
                }

                float NewAtmosphereMassVolatiles        = 0.0f;
                float NewAtmosphereMassEnergeticNuclide = 0.0f;
                float NewAtmosphereMassZ                = 0.0f;
                if (NewAtmosphereMass > 1e16f)
                {
                    NewAtmosphereMassVolatiles        = NewAtmosphereMass * 1e-2f;
                    NewAtmosphereMassEnergeticNuclide = 0.0f;
                    NewAtmosphereMassZ                = NewAtmosphereMass - NewAtmosphereMassVolatiles - NewAtmosphereMassEnergeticNuclide;
                    Planet->SetAtmosphereMassZ(NewAtmosphereMassZ);
                    Planet->SetAtmosphereMassVolatiles(NewAtmosphereMassVolatiles);
                    Planet->SetAtmosphereMassEnergeticNuclide(NewAtmosphereMassEnergeticNuclide);
                }
                else
                {
                    // 只调整核心质量（核心就是整个星球）
                    float CoreMassVolatiles        = Planet->GetCoreMassVolatilesDigital<float>();
                    float CoreMassEnergeticNuclide = Planet->GetCoreMassEnergeticNuclideDigital<float>();
                    CoreMassVolatiles             += 33.1f    * std::pow(Planet->GetRadius(), 2.0f);
                    CoreMassEnergeticNuclide      += 3.31e-4f * std::pow(Planet->GetRadius(), 2.0f);
                    Planet->SetCoreMassVolatiles(CoreMassVolatiles);
                    Planet->SetCoreMassEnergeticNuclide(CoreMassEnergeticNuclide);
                }
            }
        }
    }

    void FOrbitalGenerator::GenerateTrojan(const Astro::AStar* Star, float FrostLineAu, Astro::FOrbit* Orbit,
                                           Astro::FOrbit::FOrbitalDetails& ParentPlanet,
                                           std::vector<std::unique_ptr<Astro::AAsteroidCluster>>& AsteroidClusters)
    {
        auto* Planet     = ParentPlanet.GetOrbitalObject().GetObject<Astro::APlanet>();
        auto  PlanetType = Planet->GetPlanetType();
        if (PlanetType == Astro::APlanet::EPlanetType::kRockyAsteroidCluster ||
            PlanetType == Astro::APlanet::EPlanetType::kRockyIceAsteroidCluster)
        {
            return;
        }

        float PlanetMass        = Planet->GetMassDigital<float>();
        float PlanetMassEarth   = PlanetMass / kEarthMass;
        float LiquidRocheRadius = 2.02373e7f * std::pow(PlanetMassEarth, 1.0f / 3.0f);
        float HillSphereRadius  = Orbit->GetSemiMajorAxis() * std::pow(3.0f * PlanetMass / static_cast<float>(Star->GetMass()), 1.0f / 3.0f);
        float Random            = 1.0f + CommonGenerator_(RandomEngine_);
        float Term1             = 1e-9f * PlanetMassEarth * (HillSphereRadius / 3.11e9f);
        float Term2             = PlanetMassEarth * 1e-3f / 2.0f;
        float TrojanMass        = Random * std::max(Term1, Term1) * kEarthMass;

        if (TrojanMass < 1e14f)
        {
            return;
        }

        bool bDerivedFromRings = false;
        auto TrojanBelt        = std::make_unique<Astro::AAsteroidCluster>();

        for (auto* NextOrbit : ParentPlanet.DirectOrbitsData()) // 检查是否有合适的环
        {
            auto& OrbitalObject = NextOrbit->ObjectsData().front().GetOrbitalObject();
            if (OrbitalObject.GetObjectType() == Astro::FOrbit::EObjectType::kAsteroidCluster)
            {
                // 从环中获取数据，让特洛伊带的成分与环相同
                auto* PlanetRings           = OrbitalObject.GetObject<Astro::AAsteroidCluster>();
                auto  PlanetRingsType       = PlanetRings->GetAsteroidType();
                float RingsMass             = PlanetRings->GetMassDigital<float>();
                float RingsVolatiles        = PlanetRings->GetMassVolatilesDigital<float>();
                float RingsEnergeticNuclide = PlanetRings->GetMassEnergeticNuclideDigital<float>();
                float RingsZ                = PlanetRings->GetMassZDigital<float>();

                TrojanBelt->SetAsteroidType(PlanetRingsType);
                TrojanBelt->SetMassEnergeticNuclide(RingsEnergeticNuclide / RingsMass * TrojanMass);
                TrojanBelt->SetMassVolatiles(RingsVolatiles / RingsMass * TrojanMass);
                TrojanBelt->SetMassZ(RingsZ / RingsMass * TrojanMass);
                bDerivedFromRings = true;
            }
        }

        if (!bDerivedFromRings)
        {
            float TrojanMassEnergeticNuclide = 0.0f;
            float TrojanMassVolatiles        = 0.0f;
            float TrojanMassZ                = 0.0f;
            Astro::AAsteroidCluster::EAsteroidType AsteroidType;

            if (Orbit->GetSemiMajorAxis() / kAuToMeter >= FrostLineAu && std::to_underlying(Star->GetEvolutionPhase()) < 1)
            {
                TrojanMassEnergeticNuclide = TrojanMass * 5e-6f * 0.064f;
                TrojanMassVolatiles        = TrojanMass * 0.064f;
                TrojanMassZ                = TrojanMass - TrojanMassVolatiles - TrojanMassEnergeticNuclide;
                AsteroidType               = Astro::AAsteroidCluster::EAsteroidType::kRockyIce;
            }
            else
            {
                TrojanMassEnergeticNuclide = TrojanMass * 5e-6f;
                TrojanMassZ                = TrojanMass - TrojanMassEnergeticNuclide;
                AsteroidType               = Astro::AAsteroidCluster::EAsteroidType::kRocky;
            }

            TrojanBelt->SetAsteroidType(AsteroidType);
            TrojanBelt->SetMassEnergeticNuclide(TrojanMassEnergeticNuclide);
            TrojanBelt->SetMassVolatiles(TrojanMassVolatiles);
            TrojanBelt->SetMassZ(TrojanMassZ);
        }

#ifdef DEBUG_OUTPUT
        std::println("");
        std::println("Trojan belt details:");
        std::println("semi-major axis: {} AU, mass: {} moon, type: {}",
                     Orbit->GetSemiMajorAxis() / kAuToMeter, TrojanMass / kMoonMass, std::to_underlying(TrojanBelt->GetAsteroidType()));
        std::println("mass z: {:.2E} kg, mass vol: {:.2E} kg, mass nuc: {:.2E} kg",
                     TrojanBelt->GetMassZDigital<float>(), TrojanBelt->GetMassVolatilesDigital<float>(), TrojanBelt->GetMassEnergeticNuclideDigital<float>());
        std::println("");
#endif // DEBUG_OUTPUT

        Astro::FOrbit::FOrbitalDetails MyBelt(TrojanBelt.get(), Astro::FOrbit::EObjectType::kAsteroidCluster, Orbit);
        Orbit->ObjectsData().push_back(std::move(MyBelt));

        AsteroidClusters.push_back(std::move(TrojanBelt));
    }

    void FOrbitalGenerator::GenerateKuiperBelt(Astro::AStar* Star, float FrostLineAu, const FPlanetaryDisk& PlanetaryDisk,
                                               std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits,
                                               std::vector<std::unique_ptr<Astro::AAsteroidCluster>>& AsteroidClusters)
    {
        AsteroidClusters.push_back(std::make_unique<Astro::AAsteroidCluster>());
        float Exponent                       = 1.0f + CommonGenerator_(RandomEngine_);
        float KuiperBeltMass                 = PlanetaryDisk.DustMassSol * std::pow(10.0f, Exponent) * 1e-4f * kSolarMass;
        float KuiperBeltRadiusAu             = PlanetaryDisk.OuterRadiusAu * (1.0f + CommonGenerator_(RandomEngine_) * 0.5f);
        float KuiperBeltMassVolatiles        = 0.0f;
        float KuiperBeltMassEnergeticNuclide = 0.0f;
        float KuiperBeltMassZ                = 0.0f;

        if (std::to_underlying(Star->GetEvolutionPhase()) < 1 && KuiperBeltRadiusAu > FrostLineAu)
        {
            AsteroidClusters.back()->SetAsteroidType(Astro::AAsteroidCluster::EAsteroidType::kRockyIce);
            KuiperBeltMassVolatiles = KuiperBeltMass * 0.064f;
            KuiperBeltMassEnergeticNuclide = KuiperBeltMass * 0.064f * 5e-6f;
            KuiperBeltMassZ = KuiperBeltMass - KuiperBeltMassVolatiles - KuiperBeltMassEnergeticNuclide;
        }
        else
        {
            AsteroidClusters.back()->SetAsteroidType(Astro::AAsteroidCluster::EAsteroidType::kRocky);
            KuiperBeltMassEnergeticNuclide = KuiperBeltMass * 5e-6f;
            KuiperBeltMassZ = KuiperBeltMass - KuiperBeltMassEnergeticNuclide;
        }

        auto KuiperBeltOrbit = std::make_unique<Astro::FOrbit>();
        Astro::FOrbit::FOrbitalDetails KuiperBelt(
            AsteroidClusters.back().get(), Astro::FOrbit::EObjectType::kAsteroidCluster, KuiperBeltOrbit.get());

        KuiperBeltOrbit->ObjectsData().push_back(std::move(KuiperBelt));
        KuiperBeltOrbit->SetParent(Star, Astro::FOrbit::EObjectType::kStar);
        KuiperBeltOrbit->SetSemiMajorAxis(KuiperBeltRadiusAu * kAuToMeter);

        GenerateOrbitElements(*KuiperBeltOrbit);

#ifdef DEBUG_OUTPUT
        std::println("");
        std::println("Kuiper belt details:");
        std::println("semi-major axis: {} AU, mass: {} moon, type: {}",
                     KuiperBeltOrbit->GetSemiMajorAxis() / kAuToMeter, KuiperBeltMass / kMoonMass, std::to_underlying(AsteroidClusters.back()->GetAsteroidType()));
        std::println("mass z: {:.2E} kg, mass vol: {:.2E} kg, mass nuc: {:.2E} kg",
                     KuiperBeltMassZ, KuiperBeltMassVolatiles, KuiperBeltMassEnergeticNuclide);
        std::println("");
#endif // DEBUG_OUTPUT

        Orbits.push_back(std::move(KuiperBeltOrbit));
    }

    void FOrbitalGenerator::GenerateCivilization(const Astro::AStar* Star, float PoyntingVector,
                                                 const std::pair<float, float>& HabitableZoneAu,
                                                 const Astro::FOrbit* Orbit, Astro::APlanet* Planet)
    {
        bool bHasLife = false;
        if (Star->GetAge() > 5e8)
        {
            if (Orbit->GetSemiMajorAxis() / kAuToMeter > HabitableZoneAu.first &&
                Orbit->GetSemiMajorAxis() / kAuToMeter < HabitableZoneAu.second)
            {
                if (bContainUltravioletHabitableZone_)
                {
                    double StarMassSol = Star->GetMass() / kSolarMass;
                    if (StarMassSol > 0.75 && StarMassSol < 1.5)
                    {
                        bHasLife = true;
                    }
                }
                else
                {
                    bHasLife = true;
                }
            }
        }

        if (bHasLife)
        {
            CivilizationGenerator_->GenerateCivilization(Star, PoyntingVector, Planet);
        }
    }

    void FOrbitalGenerator::CalculateOrbitalPeriods(std::vector<std::unique_ptr<Astro::FOrbit>>& Orbits)
    {
        for (auto& Orbit : Orbits)
        {
            if (Orbit->GetPeriod())
            {
                continue;
            }

            float SemiMajorAxis = Orbit->GetSemiMajorAxis();
            float CenterMass    = 0.0f;
            if (Orbit->GetParent().GetObjectType() == Astro::FOrbit::EObjectType::kStar)
            {
                CenterMass = static_cast<float>(Orbit->GetParent().GetObject<Astro::AStar>()->GetMass());
            }
            else if (Orbit->GetParent().GetObjectType() == Astro::FOrbit::EObjectType::kPlanet)
            {
                CenterMass = Orbit->GetParent().GetObject<Astro::APlanet>()->GetMassDigital<float>();
            }

            float Period =
                static_cast<float>(std::sqrt(4.0 * std::pow(Math::kPi, 2.0) * std::pow(SemiMajorAxis, 3.0) / (kGravityConstant * CenterMass)));
            Orbit->SetPeriod(Period);

            for (auto& Object : Orbit->ObjectsData())
            {
                if (Object.GetOrbitalObject().GetObjectType() == Astro::FOrbit::EObjectType::kPlanet)
                {
                    if (Object.GetOrbitalObject().GetObject<Astro::APlanet>()->GetSpin() <= 0)
                    {
                        Object.GetOrbitalObject().GetObject<Astro::APlanet>()->SetSpin(Orbit->GetPeriod());
                    }
                }
            }
        }
    }
} // namespace Npgs
