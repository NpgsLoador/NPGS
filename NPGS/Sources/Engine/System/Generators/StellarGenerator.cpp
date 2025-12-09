#include "stdafx.h"
#include "StellarGenerator.hpp"

#include <cmath>
#include <algorithm>
#include <charconv>
#include <exception>
#include <filesystem>
#include <format>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "Engine/Core/Utils/Utils.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Runtime/AssetLoaders/CommaSeparatedValues.hpp"
#include "Engine/Runtime/Managers/AssetManager.hpp"
#include "Engine/System/Services/EngineServices.hpp"

namespace Npgs
{
    // Tool functions
    // --------------
    namespace
    {
        std::unexpected<Astro::AStar> GenerateDeathStarPlaceholder(double Lifetime)
        {
            Astro::FSpectralType DeathStarClass
            {
                .HSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                .bIsAmStar       = false,
                .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                .Subclass        = 0.0f,
                .AmSubclass      = 0.0f
            };

            Astro::AStar DeathStar;
            DeathStar.SetStellarClass(Astro::FStellarClass(Astro::EStellarType::kDeathStarPlaceholder, DeathStarClass));
            DeathStar.SetLifetime(Lifetime);
            return std::unexpected(DeathStar);
        }

        float CalculateWNxhMassThreshold(float FeH)
        {
            float BaseMass =  60.0f;
            float Exponent = -0.31f;

            float FeHRatio  = std::pow(10.0f, FeH);
			float Threshold = BaseMass * std::pow(FeHRatio, Exponent);

            return std::clamp(Threshold, 45.0f, 300.0f);
        }

        float DefaultAgePdf(glm::vec3, float Age, float UniverseAge)
        {
            float Probability = 0.0f;
            if (Age - (UniverseAge - 13.8f) < 8.0f)
            {
                Probability = std::exp((Age - (UniverseAge - 13.8f) / 8.4f));
            }
            else
            {
                Probability = 2.6f * std::exp((-0.5f * std::pow((Age - (UniverseAge - 13.8f)) - 8.0f, 2.0f)) / (std::pow(1.5f, 2.0f)));
            }

            return static_cast<float>(Probability);
        }

        float DefaultLogMassPdfSingleStar(float LogMassSol)
        {
            float Probability = 0.0f;

            if (std::pow(10.0f, LogMassSol) <= 1.0f)
            {
                Probability = 0.158f * std::exp(-1.0f * std::pow(LogMassSol + 1.0f, 2.0f) / 1.101128f);
            }
            else
            {
                Probability = 0.06371598f * std::pow(std::pow(10.0f, LogMassSol), -0.8f);
            }

            return Probability;
        }

        float DefaultLogMassPdfBinaryStar(float LogMassSol)
        {
            float Probability = 0.0f;

            if (std::pow(10.0, LogMassSol) <= 1.0f)
            {
                Probability = 0.086f * std::exp(-1.0f * std::pow(LogMassSol + 0.65757734f, 2.0f) / 1.101128f);
            }
            else
            {
                Probability = 0.058070157f * std::pow(std::pow(10.0f, LogMassSol), -0.65f);
            }

            return Probability;
        }
    }

    // FStellarGenerator implementations
    // ---------------------------------
    FStellarGenerator::FStellarGenerator(const FStellarGenerationInfo& GenerationInfo)
        :
        RandomEngine_(*GenerationInfo.SeedSequence),
        MagneticGenerators_
        {
            Math::TUniformRealDistribution<>(std::log10(500.0f), std::log10(3000.0f)),
            Math::TUniformRealDistribution<>(1.0f, 3.0f),
            Math::TUniformRealDistribution<>(0.0f, 1.0f),
            Math::TUniformRealDistribution<>(3.0f, 4.0f),
            Math::TUniformRealDistribution<>(-1.0f, 0.0f),
            Math::TUniformRealDistribution<>(2.0f, 3.0f),
            Math::TUniformRealDistribution<>(0.5f, 4.5f),
            Math::TUniformRealDistribution<>(1e9f, 1e11f)
        },

        FeHGenerators_
        {
            std::make_unique<Math::TLogNormalDistribution<>>(-0.3f, 0.5f),
            std::make_unique<Math::TNormalDistribution<>>(-0.3f, 0.15f),
            std::make_unique<Math::TNormalDistribution<>>(-0.08f, 0.12f),
            std::make_unique<Math::TNormalDistribution<>>(0.05f, 0.16f)
        },

        SpinGenerators_
        {
            Math::TUniformRealDistribution<>(3.0f, 5.0f),
            Math::TUniformRealDistribution<>(0.001f, 0.998f)
        },

        AgeGenerator_(GenerationInfo.AgeLowerLimit, GenerationInfo.AgeUpperLimit),
        CommonGenerator_(0.0f, 1.0f),

        LogMassGenerator_(GenerationInfo.StellarTypeOption == EStellarTypeGenerationOption::kMergeStar
                          ? std::make_unique<Math::TUniformRealDistribution<>>(0.0f, 1.0f)
                          : std::make_unique<Math::TUniformRealDistribution<>>(std::log10(GenerationInfo.MassLowerLimit),
                                                                               std::log10(GenerationInfo.MassUpperLimit))),

        MassPdfs_(std::move(GenerationInfo.MassPdfs)),
        MassMaxPdfs_(GenerationInfo.MassMaxPdfs),
        AgeMaxPdf_(GenerationInfo.AgeMaxPdf),
        AgePdf_(std::move(GenerationInfo.AgePdf)),
        UniverseAge_(GenerationInfo.UniverseAge),
        AgeLowerLimit_(GenerationInfo.AgeLowerLimit),
        AgeUpperLimit_(GenerationInfo.AgeUpperLimit),
        FeHLowerLimit_(GenerationInfo.FeHLowerLimit),
        FeHUpperLimit_(GenerationInfo.FeHUpperLimit),
        MassLowerLimit_(GenerationInfo.MassLowerLimit),
        MassUpperLimit_(GenerationInfo.MassUpperLimit),
        CoilTemperatureLimit_(GenerationInfo.CoilTemperatureLimit),
        dEpdM_(GenerationInfo.dEpdM),

        AgeDistribution_(GenerationInfo.AgeDistribution),
        FeHDistribution_(GenerationInfo.FeHDistribution),
        MassDistribution_(GenerationInfo.MassDistribution),
        StellarTypeOption_(GenerationInfo.StellarTypeOption),
        MultiplicityOption_(GenerationInfo.MultiplicityOption)
    {
        InitializeMistData();
        InitializePdfs();
    }

    FStellarGenerator::FStellarGenerator(const FStellarGenerator& Other)
        : RandomEngine_(Other.RandomEngine_)
        , MagneticGenerators_(Other.MagneticGenerators_)
        , SpinGenerators_(Other.SpinGenerators_)
        , AgeGenerator_(Other.AgeGenerator_)
        , CommonGenerator_(Other.CommonGenerator_)
        , MassPdfs_(Other.MassPdfs_)
        , MassMaxPdfs_(Other.MassMaxPdfs_)
        , AgeMaxPdf_(Other.AgeMaxPdf_)
        , AgePdf_(Other.AgePdf_)
        , UniverseAge_(Other.UniverseAge_)
        , AgeLowerLimit_(Other.AgeLowerLimit_)
        , AgeUpperLimit_(Other.AgeUpperLimit_)
        , FeHLowerLimit_(Other.FeHLowerLimit_)
        , FeHUpperLimit_(Other.FeHUpperLimit_)
        , MassLowerLimit_(Other.MassLowerLimit_)
        , MassUpperLimit_(Other.MassUpperLimit_)
        , CoilTemperatureLimit_(Other.CoilTemperatureLimit_)
        , dEpdM_(Other.dEpdM_)
        , AgeDistribution_(Other.AgeDistribution_)
        , FeHDistribution_(Other.FeHDistribution_)
        , MassDistribution_(Other.MassDistribution_)
        , StellarTypeOption_(Other.StellarTypeOption_)
        , MultiplicityOption_(Other.MultiplicityOption_)
    {
        if (Other.LogMassGenerator_ != nullptr)
        {
            LogMassGenerator_ = std::make_unique<Math::TUniformRealDistribution<>>(
                std::log10(Other.MassLowerLimit_), std::log10(Other.MassUpperLimit_));
        }

        if (Other.FeHGenerators_[0] != nullptr)
        {
            auto* LogNormal = dynamic_cast<Math::TLogNormalDistribution<>*>(Other.FeHGenerators_[0].get());
            FeHGenerators_[0] = std::make_unique<Math::TLogNormalDistribution<>>(*LogNormal);
        }

        if (Other.FeHGenerators_[1] != nullptr)
        {
            auto* Normal = dynamic_cast<Math::TNormalDistribution<>*>(Other.FeHGenerators_[1].get());
            FeHGenerators_[1] = std::make_unique<Math::TNormalDistribution<>>(*Normal);
        }

        if (Other.FeHGenerators_[2] != nullptr)
        {
            auto* Normal = dynamic_cast<Math::TNormalDistribution<>*>(Other.FeHGenerators_[2].get());
            FeHGenerators_[2] = std::make_unique<Math::TNormalDistribution<>>(*Normal);
        }

        if (Other.FeHGenerators_[3] != nullptr)
        {
            auto* Normal = dynamic_cast<Math::TNormalDistribution<>*>(Other.FeHGenerators_[3].get());
            FeHGenerators_[3] = std::make_unique<Math::TNormalDistribution<>>(*Normal);
        }
    }

    FStellarGenerator::FStellarGenerator(FStellarGenerator&& Other) noexcept
        : RandomEngine_(std::move(Other.RandomEngine_))
        , MagneticGenerators_(std::move(Other.MagneticGenerators_))
        , FeHGenerators_(std::move(Other.FeHGenerators_))
        , SpinGenerators_(std::move(Other.SpinGenerators_))
        , AgeGenerator_(std::move(Other.AgeGenerator_))
        , CommonGenerator_(std::move(Other.CommonGenerator_))
        , LogMassGenerator_(std::move(Other.LogMassGenerator_))
        , MassPdfs_(std::move(Other.MassPdfs_))
        , MassMaxPdfs_(std::move(Other.MassMaxPdfs_))
        , AgeMaxPdf_(std::move(Other.AgeMaxPdf_))
        , AgePdf_(std::move(Other.AgePdf_))
        , UniverseAge_(std::exchange(Other.UniverseAge_, 0.0f))
        , AgeLowerLimit_(std::exchange(Other.AgeLowerLimit_, 0.0f))
        , AgeUpperLimit_(std::exchange(Other.AgeUpperLimit_, 0.0f))
        , FeHLowerLimit_(std::exchange(Other.FeHLowerLimit_, 0.0f))
        , FeHUpperLimit_(std::exchange(Other.FeHUpperLimit_, 0.0f))
        , MassLowerLimit_(std::exchange(Other.MassLowerLimit_, 0.0f))
        , MassUpperLimit_(std::exchange(Other.MassUpperLimit_, 0.0f))
        , CoilTemperatureLimit_(std::exchange(Other.CoilTemperatureLimit_, 0.0f))
        , dEpdM_(std::exchange(Other.dEpdM_, 0.0f))
        , AgeDistribution_(std::exchange(Other.AgeDistribution_, {}))
        , FeHDistribution_(std::exchange(Other.FeHDistribution_, {}))
        , MassDistribution_(std::exchange(Other.MassDistribution_, {}))
        , StellarTypeOption_(std::exchange(Other.StellarTypeOption_, {}))
        , MultiplicityOption_(std::exchange(Other.MultiplicityOption_, {}))
    {
    }

    FStellarGenerator& FStellarGenerator::operator=(const FStellarGenerator& Other)
    {
        if (this != &Other)
        {
            RandomEngine_         = Other.RandomEngine_;
            MagneticGenerators_   = Other.MagneticGenerators_;
            SpinGenerators_       = Other.SpinGenerators_;
            AgeGenerator_         = Other.AgeGenerator_;
            CommonGenerator_      = Other.CommonGenerator_;
            MassPdfs_             = Other.MassPdfs_;
            MassMaxPdfs_          = Other.MassMaxPdfs_;
            AgeMaxPdf_            = Other.AgeMaxPdf_;
            AgePdf_               = Other.AgePdf_;
            UniverseAge_          = Other.UniverseAge_;
            AgeLowerLimit_        = Other.AgeLowerLimit_;
            AgeUpperLimit_        = Other.AgeUpperLimit_;
            FeHLowerLimit_        = Other.FeHLowerLimit_;
            FeHUpperLimit_        = Other.FeHUpperLimit_;
            MassLowerLimit_       = Other.MassLowerLimit_;
            MassUpperLimit_       = Other.MassUpperLimit_;
            CoilTemperatureLimit_ = Other.CoilTemperatureLimit_;
            dEpdM_                = Other.dEpdM_;
            AgeDistribution_      = Other.AgeDistribution_;
            FeHDistribution_      = Other.FeHDistribution_;
            MassDistribution_     = Other.MassDistribution_;
            StellarTypeOption_    = Other.StellarTypeOption_;
            MultiplicityOption_   = Other.MultiplicityOption_;
            LogMassGenerator_     = Other.LogMassGenerator_
                                  ? std::make_unique<Math::TUniformRealDistribution<>>(std::log10(Other.MassLowerLimit_), std::log10(Other.MassUpperLimit_))
                                  : nullptr;

            if (Other.FeHGenerators_[0] != nullptr)
            {
                auto* LogNormal = dynamic_cast<Math::TLogNormalDistribution<>*>(Other.FeHGenerators_[0].get());
                FeHGenerators_[0] = std::make_unique<Math::TLogNormalDistribution<>>(*LogNormal);
            }

            if (Other.FeHGenerators_[1] != nullptr)
            {
                auto* Normal = dynamic_cast<Math::TNormalDistribution<>*>(Other.FeHGenerators_[1].get());
                FeHGenerators_[1] = std::make_unique<Math::TNormalDistribution<>>(*Normal);
            }

            if (Other.FeHGenerators_[2] != nullptr)
            {
                auto* Normal = dynamic_cast<Math::TNormalDistribution<>*>(Other.FeHGenerators_[2].get());
                FeHGenerators_[2] = std::make_unique<Math::TNormalDistribution<>>(*Normal);
            }

            if (Other.FeHGenerators_[3] != nullptr)
            {
                auto* Normal = dynamic_cast<Math::TNormalDistribution<>*>(Other.FeHGenerators_[3].get());
                FeHGenerators_[3] = std::make_unique<Math::TNormalDistribution<>>(*Normal);
            }
        }

        return *this;
    }

    FStellarGenerator& FStellarGenerator::operator=(FStellarGenerator&& Other) noexcept
    {
        if (this != &Other)
        {
            RandomEngine_         = std::move(Other.RandomEngine_);
            MagneticGenerators_   = std::move(Other.MagneticGenerators_);
            FeHGenerators_        = std::move(Other.FeHGenerators_);
            SpinGenerators_       = std::move(Other.SpinGenerators_);
            AgeGenerator_         = std::move(Other.AgeGenerator_);
            CommonGenerator_      = std::move(Other.CommonGenerator_);
            LogMassGenerator_     = std::move(Other.LogMassGenerator_);
            MassPdfs_             = std::move(Other.MassPdfs_);
            MassMaxPdfs_          = std::move(Other.MassMaxPdfs_);
            AgeMaxPdf_            = std::move(Other.AgeMaxPdf_);
            AgePdf_               = std::move(Other.AgePdf_);
            UniverseAge_          = std::exchange(Other.UniverseAge_, 0.0f);
            AgeLowerLimit_        = std::exchange(Other.AgeLowerLimit_, 0.0f);
            AgeUpperLimit_        = std::exchange(Other.AgeUpperLimit_, 0.0f);
            FeHLowerLimit_        = std::exchange(Other.FeHLowerLimit_, 0.0f);
            FeHUpperLimit_        = std::exchange(Other.FeHUpperLimit_, 0.0f);
            MassLowerLimit_       = std::exchange(Other.MassLowerLimit_, 0.0f);
            MassUpperLimit_       = std::exchange(Other.MassUpperLimit_, 0.0f);
            CoilTemperatureLimit_ = std::exchange(Other.CoilTemperatureLimit_, 0.0f);
            dEpdM_                = std::exchange(Other.dEpdM_, 0.0f);
            AgeDistribution_      = std::exchange(Other.AgeDistribution_, {});
            FeHDistribution_      = std::exchange(Other.FeHDistribution_, {});
            MassDistribution_     = std::exchange(Other.MassDistribution_, {});
            StellarTypeOption_    = std::exchange(Other.StellarTypeOption_, {});
            MultiplicityOption_   = std::exchange(Other.MultiplicityOption_, {});
        }

        return *this;
    }

    FStellarBasicProperties FStellarGenerator::GenerateBasicProperties(float Age, float FeH)
    {
        FStellarBasicProperties Properties{};
        Properties.StellarTypeOption = StellarTypeOption_;

        // 生成 3 个基本参数
        if (std::isnan(Age)) // 非有效数值，使用分布生成随机值
        {
            switch (AgeDistribution_)
            {
            case EGenerationDistribution::kFromPdf:
            {
                glm::vec2 MaxPdf = AgeMaxPdf_;
                if (!(AgeLowerLimit_ < UniverseAge_ - 1.38e10f + AgeMaxPdf_.x &&
                      AgeUpperLimit_ > UniverseAge_ - 1.38e10f + AgeMaxPdf_.x))
                {
                    if (AgeLowerLimit_ > UniverseAge_ - 1.38e10f + AgeMaxPdf_.x)
                    {
                        MaxPdf.y = AgePdf_(glm::vec3(), AgeLowerLimit_, UniverseAge_ / 1e9f);
                    }
                    else if (AgeUpperLimit_ < UniverseAge_ - 1.38e10f + AgeMaxPdf_.x)
                    {
                        MaxPdf.y = AgePdf_(glm::vec3(), AgeUpperLimit_, UniverseAge_ / 1e9f);
                    }
                }
                Properties.Age = GenerateAge(MaxPdf.y);
                break;
            }
            case EGenerationDistribution::kUniform:
            {
                Properties.Age = AgeLowerLimit_ + CommonGenerator_(RandomEngine_) * (AgeUpperLimit_ - AgeLowerLimit_);
                break;
            }
            case EGenerationDistribution::kUniformByExponent:
            {
                float Random      = CommonGenerator_(RandomEngine_);
                float LogAgeLower = std::log10(AgeLowerLimit_);
                float LogAgeUpper = std::log10(AgeUpperLimit_);
                Properties.Age    = std::pow(10.0f, LogAgeLower + Random * (LogAgeUpper - LogAgeLower));
                break;
            }
            default:
                break;
            }
        }
        else
        {
            Properties.Age = Age;
        }

        if (std::isnan(FeH)) // 非有效数值，使用分布生成随机值
        {
            Math::TDistribution<>* FeHGenerator = nullptr;

            float FeHLowerLimit = FeHLowerLimit_;
            float FeHUpperLimit = FeHUpperLimit_;

            // 不同的年龄使用不同的分布
            if (Properties.Age > UniverseAge_ - 1.38e10f + 8e9f)
            {
                FeHGenerator  = FeHGenerators_[0].get();
                FeHLowerLimit = -FeHUpperLimit_; // 对数分布，但是是反的
                FeHUpperLimit = -FeHLowerLimit_;
            }
            else if (Properties.Age > UniverseAge_ - 1.38e10f + 6e9f)
            {
                FeHGenerator = FeHGenerators_[1].get();
            }
            else if (Properties.Age > UniverseAge_ - 1.38e10f + 4e9f)
            {
                FeHGenerator = FeHGenerators_[2].get();
            }
            else
            {
                FeHGenerator = FeHGenerators_[3].get();
            }

            do {
                FeH = (*FeHGenerator)(RandomEngine_);
            } while (FeH > FeHUpperLimit || FeH < FeHLowerLimit);

            if (Properties.Age > UniverseAge_ - 1.38e10 + 8e9)
            {
                FeH *= -1.0f; // 把对数分布反过来
            }
        }

        Properties.FeH = FeH;

        if (MultiplicityOption_ != EMultiplicityGenerationOption::kBinarySecondStar)
        {
            Math::TBernoulliDistribution<> BinaryProbability(0.45 - 0.07 * std::pow(10, FeH));
            if (BinaryProbability(RandomEngine_))
            {
                Properties.MultiplicityOption = EMultiplicityGenerationOption::kBinaryFirstStar;
                Properties.bIsSingleStar      = false;
            }
        }
        else
        {
            Properties.MultiplicityOption = EMultiplicityGenerationOption::kBinarySecondStar;
            Properties.bIsSingleStar      = false;
        }

        if (MassLowerLimit_ == 0.0f && MassUpperLimit_ == 0.0f)
        {
            Properties.InitialMassSol = 0.0f;
        }
        else
        {
            switch (MassDistribution_)
            {
            case EGenerationDistribution::kFromPdf: {
                glm::vec2 MaxPdf{};
                float LogMassLower = std::log10(MassLowerLimit_);
                float LogMassUpper = std::log10(MassUpperLimit_);
                std::function<float(float)> LogMassPdf = nullptr;

                if (Properties.MultiplicityOption != EMultiplicityGenerationOption::kBinarySecondStar)
                {
                    if (Properties.MultiplicityOption == EMultiplicityGenerationOption::kBinaryFirstStar)
                    {
                        LogMassPdf = MassPdfs_[1];
                        MaxPdf     = MassMaxPdfs_[1];
                    }
                    else
                    {
                        LogMassPdf = MassPdfs_[0];
                        MaxPdf     = MassMaxPdfs_[0];
                    }
                }
                else
                {
                    LogMassPdf = MassPdfs_[1];
                    MaxPdf     = MassMaxPdfs_[1];
                }

                if (!(LogMassLower < MaxPdf.x && LogMassUpper > MaxPdf.x))
                {
                    // 调整最大值，防止接受率过低
                    if (LogMassLower > MaxPdf.x)
                    {
                        MaxPdf.y = LogMassPdf(LogMassLower);
                    }
                    else if (LogMassUpper < MaxPdf.x)
                    {
                        MaxPdf.y = LogMassPdf(LogMassUpper);
                    }
                }

                Properties.InitialMassSol = GenerateMass(MaxPdf.y, LogMassPdf);
                break;
            }
            case EGenerationDistribution::kUniform: {
                Properties.InitialMassSol = MassLowerLimit_ + CommonGenerator_(RandomEngine_) * (MassUpperLimit_ - MassLowerLimit_);
                break;
            }
            default:
                break;
            }
        }

        return Properties;
    }

    Astro::AStar FStellarGenerator::GenerateStar()
    {
        auto Properties = GenerateBasicProperties();
        return GenerateStar(Properties);
    }

    Astro::AStar FStellarGenerator::GenerateStar(FStellarBasicProperties& Properties)
    {
        if (Utils::Equal(Properties.InitialMassSol, -1.0f))
        {
            Properties = GenerateBasicProperties(Properties.Age, Properties.FeH);
        }

        Astro::AStar Star(Properties);
        FDataArray StarData;

        switch (Properties.StellarTypeOption)
        {
        case EStellarTypeGenerationOption::kRandom:
        {
            try
            {
                auto ExpectedResult = GetFullMistData(Properties, false, true);
                if (ExpectedResult.has_value())
                {
                    StarData = ExpectedResult.value();
                }
                else
                {
                    auto& DeathStar = ExpectedResult.error();
                    DeathStar.SetAge(Properties.Age);
                    DeathStar.SetFeH(Properties.FeH);
                    DeathStar.SetInitialMass(Properties.InitialMassSol * kSolarMass);
                    DeathStar.SetSingleton(Properties.bIsSingleStar);
                    ProcessDeathStar(EStellarTypeGenerationOption::kRandom, DeathStar);
                    if (DeathStar.GetEvolutionPhase() == Astro::AStar::EEvolutionPhase::kNull)
                    {
                        // 如果爆了，削一半质量
                        Properties.InitialMassSol /= 2;
                        DeathStar = GenerateStar(Properties);
                    }

                    return DeathStar;
                }
            }
            catch (const std::exception& e)
            {
                NpgsCoreError("Failed to generate star at Age={}, FeH={}, InMass={}: {}",
                              Properties.Age, Properties.FeH, Properties.InitialMassSol, e.what());
                return {};
            }

            break;
        }
        case EStellarTypeGenerationOption::kGiant:
        {
            Properties.Age = std::numeric_limits<float>::quiet_NaN(); // 使用 NaN，在计算年龄的时候根据寿命赋值一个濒死年龄
            try
            {
                StarData = GetFullMistData(Properties, false, true).value();
            }
            catch (const std::exception& e)
            {
                NpgsCoreError("Failed to generate giant star at Age={}, FeH={}, InMass={}: {}",
                              Properties.Age, Properties.FeH, Properties.InitialMassSol, e.what());
                return {};
            }

            break;
        }
        case EStellarTypeGenerationOption::kDeathStar:
        {
            ProcessDeathStar(EStellarTypeGenerationOption::kDeathStar, Star);
            Properties.Age            = static_cast<float>(Star.GetAge());
            Properties.FeH            = Star.GetFeH();
            Properties.InitialMassSol = Star.GetInitialMass() / kSolarMass;

            if (Star.GetEvolutionPhase() == Astro::AStar::EEvolutionPhase::kNull)
            {
                // 如果爆了，削一半质量
                Properties.InitialMassSol /= 2;
                Star = GenerateStar(Properties);
            }

            return Star;

            break;
        }
        case EStellarTypeGenerationOption::kMergeStar:
        {
            ProcessDeathStar(EStellarTypeGenerationOption::kMergeStar, Star);
            return Star;

            break;
        }
        default:
            break;
        }

        if (StarData.empty())
        {
            return {};
        }

        double Lifetime          = StarData[kLifetimeIndex_];
        double EvolutionProgress = StarData[kXIndex_];
        float  Age               = static_cast<float>(StarData[kStarAgeIndex_]);
        float  RadiusSol         = static_cast<float>(std::pow(10.0, StarData[kLogRIndex_]));
        float  MassSol           = static_cast<float>(StarData[kStarMassIndex_]);
        float  Teff              = static_cast<float>(std::pow(10.0, StarData[kLogTeffIndex_]));
        float  SurfaceZ          = static_cast<float>(std::pow(10.0, StarData[kLogSurfZIndex_]));
        float  SurfaceH1         = static_cast<float>(StarData[kSurfaceH1Index_]);
        float  SurfaceHe3        = static_cast<float>(StarData[kSurfaceHe3Index_]);
        float  CoreTemp          = static_cast<float>(std::pow(10.0, StarData[kLogCenterTIndex_]));
        float  CoreDensity       = static_cast<float>(std::pow(10.0, StarData[kLogCenterRhoIndex_]));
        float  MassLossRate      = static_cast<float>(StarData[kStarMdotIndex_]);

        float LuminositySol  = std::pow(RadiusSol, 2.0f) * std::pow((Teff / kSolarTeff), 4.0f);
        float EscapeVelocity = std::sqrt((2.0f * kGravityConstant * MassSol * kSolarMass) / (RadiusSol * kSolarRadius));

        float LifeProgress         = static_cast<float>(Age / Lifetime);
        float WindSpeedCoefficient = 3.0f - LifeProgress;
        float StellarWindSpeed     = WindSpeedCoefficient * EscapeVelocity;

        float SurfaceEnergeticNuclide = (SurfaceH1 * 0.00002f + SurfaceHe3);
        float SurfaceVolatiles = 1.0f - SurfaceZ - SurfaceEnergeticNuclide;

        float Theta = CommonGenerator_(RandomEngine_) * 2.0f * Math::kPi;
        float Phi   = CommonGenerator_(RandomEngine_) * Math::kPi;

        Astro::AStar::EEvolutionPhase EvolutionPhase = static_cast<Astro::AStar::EEvolutionPhase>(StarData[kPhaseIndex_]);

        Star.SetSingleton(Properties.bIsSingleStar);
        Star.SetAge(Age);
        Star.SetMass(MassSol * kSolarMass);
        Star.SetLifetime(Lifetime);
        Star.SetRadius(RadiusSol * kSolarRadius);
        Star.SetEscapeVelocity(EscapeVelocity);
        Star.SetLuminosity(LuminositySol * kSolarLuminosity);
        Star.SetTeff(Teff);
        Star.SetSurfaceH1(SurfaceH1);
        Star.SetSurfaceZ(SurfaceZ);
        Star.SetSurfaceEnergeticNuclide(SurfaceEnergeticNuclide);
        Star.SetSurfaceVolatiles(SurfaceVolatiles);
        Star.SetCoreTemp(CoreTemp);
        Star.SetCoreDensity(CoreDensity * 1000);
        Star.SetStellarWindSpeed(StellarWindSpeed);
        Star.SetStellarWindMassLossRate(-(MassLossRate * kSolarMass / kYearToSecond));
        Star.SetEvolutionProgress(EvolutionProgress);
        Star.SetEvolutionPhase(EvolutionPhase);
        Star.SetNormal(glm::vec2(Theta, Phi));

        CalculateSpectralType(static_cast<float>(StarData.back()), Star);
        GenerateMagnetic(Star);
        GenerateSpin(Star);

        double Mass          = Star.GetMass();
        double Luminosity    = Star.GetLuminosity();
        float  Radius        = Star.GetRadius();
        float  MagneticField = Star.GetMagneticField();

        float MinCoilMass = static_cast<float>(std::max(
            6.6156e14  * std::pow(MagneticField, 2.0f) * std::pow(Luminosity, 1.5) * std::pow(CoilTemperatureLimit_, -6.0f) * std::pow(dEpdM_, -1.0f),
            2.34865e29 * std::pow(MagneticField, 2.0f) * std::pow(Luminosity, 2.0) * std::pow(CoilTemperatureLimit_, -8.0f) * std::pow(Mass,   -1.0)
        ));

        Star.SetMinCoilMass(MinCoilMass);

        return Star;
    }

    template <typename CsvType>
    requires std::is_class_v<CsvType>
    TAssetHandle<CsvType> FStellarGenerator::LoadCsvAsset(const std::string& Filename, std::span<const std::string> Headers)
    {
        auto* AssetManager = EngineCoreServices->GetAssetManager();
        {
            std::shared_lock Lock(CacheMutex_);
            auto Asset = AssetManager->AcquireAsset<CsvType>(Filename);
            if (Asset)
            {
                return Asset;
            }
        }

        std::unique_lock Lock(CacheMutex_);
        AssetManager->AddAsset<CsvType>(Filename, CsvType(Filename, Headers));

        return AssetManager->AcquireAsset<CsvType>(Filename);
    }

    void FStellarGenerator::InitializeMistData()
    {
        if (bMistDataInitiated_)
        {
            return;
        }

        const std::array kPresetPrefix
        {
            GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-4.0"),
            GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-3.0"),
            GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-2.0"),
            GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-1.5"),
            GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-1.0"),
            GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=-0.5"),
            GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=+0.0"),
            GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]=+0.5"),
            GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/WhiteDwarfs/Thin"),
            GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/WhiteDwarfs/Thick")
        };

        std::vector<float> Masses;

        for (const auto& PrefixDirectory : kPresetPrefix)
        {
            for (const auto& Entry : std::filesystem::directory_iterator(PrefixDirectory))
            {
                std::string Filename = Entry.path().filename().string();

                float Mass = 0.0f;
                std::from_chars(Filename.data(), Filename.data() + Filename.find("Ms_track.csv"), Mass);

                Masses.push_back(Mass);

                if (PrefixDirectory.find("WhiteDwarfs") != std::string::npos)
                {
                    LoadCsvAsset<FWdMistData>(PrefixDirectory + "/" + Filename, kWdMistHeaders_);
                }
                else
                {
                    LoadCsvAsset<FMistData>(PrefixDirectory + "/" + Filename, kMistHeaders_);
                }
            }

            MassFilesCache_.emplace(PrefixDirectory, Masses);
            Masses.clear();
        }

        bMistDataInitiated_ = true;
    }

    void FStellarGenerator::InitializePdfs()
    {
        if (AgePdf_ == nullptr)
        {
            AgePdf_ = DefaultAgePdf;
            AgeMaxPdf_ = glm::vec2(8e9, 2.7f);
        }

        if (MassPdfs_[0] == nullptr)
        {
            MassPdfs_[0] = DefaultLogMassPdfSingleStar;
            MassMaxPdfs_[0] = glm::vec2(std::log10(0.1f), 0.158f);
        }

        if (MassPdfs_[1] == nullptr)
        {
            MassPdfs_[1] = DefaultLogMassPdfBinaryStar;
            MassMaxPdfs_[1] = glm::vec2(std::log10(0.22f), 0.086);
        }
    }

    float FStellarGenerator::GenerateAge(float MaxPdf)
    {
        float Age = 0.0f;
        float Probability = 0.0f;
        do {
            Age = AgeGenerator_(RandomEngine_);
            Probability = DefaultAgePdf(glm::vec3(), Age / 1e9f, UniverseAge_ / 1e9f);
        } while (CommonGenerator_(RandomEngine_) * MaxPdf > Probability);

        return Age;
    }

    float FStellarGenerator::GenerateMass(float MaxPdf, auto& LogMassPdf)
    {
        float LogMass = 0.0f;
        float Probability = 0.0f;

        float LogMassLower = std::log10(MassLowerLimit_);
        float LogMassUpper = std::log10(MassUpperLimit_);

        if (LogMassUpper >= std::log10(300.0f))
        {
            LogMassUpper = std::log10(299.9f);
        }

        do {
            LogMass = (*LogMassGenerator_)(RandomEngine_);
            Probability = LogMassPdf(LogMass);
        } while ((LogMass < LogMassLower || LogMass > LogMassUpper) || CommonGenerator_(RandomEngine_) * MaxPdf > Probability);

        return std::pow(10.0f, LogMass);
    }

    std::expected<FStellarGenerator::FDataArray, Astro::AStar>
    FStellarGenerator::GetFullMistData(const FStellarBasicProperties& Properties, bool bIsWhiteDwarf, bool bIsSingleWhiteDwarf)
    {
        float TargetAge     = Properties.Age;
        float TargetFeH     = Properties.FeH;
        float TargetMassSol = Properties.InitialMassSol;

        std::string PrefixDirectory;
        std::string MassString;
        std::stringstream MassStream;
        std::pair<std::string, std::string> Files;

        if (!bIsWhiteDwarf)
        {
            const std::array kPresetFeH{ -4.0f, -3.0f, -2.0f, -1.5f, -1.0f, -0.5f, 0.0f, 0.5f };

            float ClosestFeH = *std::ranges::min_element(kPresetFeH, [TargetFeH](float Lhs, float Rhs) -> bool
            {
                return std::abs(Lhs - TargetFeH) < std::abs(Rhs - TargetFeH);
            });

            TargetFeH = ClosestFeH;

            MassStream << std::fixed << std::setfill('0') << std::setw(6) << std::setprecision(2) << TargetMassSol;
            MassString = MassStream.str() + "0";

            std::stringstream FeHStream;
            FeHStream << std::fixed << std::setprecision(1) << TargetFeH;
            PrefixDirectory = FeHStream.str();
            if (TargetFeH >= 0.0f)
            {
                PrefixDirectory.insert(PrefixDirectory.begin(), '+');
            }
            PrefixDirectory.insert(0, GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]="));
        }
        else
        {
            if (bIsSingleWhiteDwarf)
            {
                PrefixDirectory = GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/WhiteDwarfs/Thin");
            }
            else
            {
                PrefixDirectory = GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/WhiteDwarfs/Thick");
            }
        }

        std::vector<float> Masses;
        {
            std::shared_lock Lock(CacheMutex_);
            Masses = MassFilesCache_[PrefixDirectory];
        }

        auto it = std::ranges::lower_bound(Masses, TargetMassSol);
        if (it == Masses.end())
        {
            if (!bIsWhiteDwarf)
            {
                throw std::out_of_range("Mass value out of range.");
            }
            else
            {
                it = std::prev(Masses.end(), 1);
            }
        }

        float LowerMass = 0.0f;
        float UpperMass = 0.0f;

        if (*it == TargetMassSol)
        {
            LowerMass = UpperMass = *it;
        }
        else
        {
            LowerMass = it == Masses.begin() ? *it : *(it - 1);
            UpperMass = *it;
        }

        float MassCoefficient = (TargetMassSol - LowerMass) / (UpperMass - LowerMass);

        MassString.clear();
        MassStream.str("");
        MassStream.clear();

        MassStream << std::fixed << std::setfill('0') << std::setw(6) << std::setprecision(2) << LowerMass;
        MassString = MassStream.str() + "0";
        std::string LowerMassFile = PrefixDirectory + "/" + MassString + "Ms_track.csv";

        MassString.clear();
        MassStream.str("");
        MassStream.clear();

        MassStream << std::fixed << std::setfill('0') << std::setw(6) << std::setprecision(2) << UpperMass;
        MassString = MassStream.str() + "0";
        std::string UpperMassFile = PrefixDirectory + "/" + MassString + "Ms_track.csv";

        Files = std::make_pair(LowerMassFile, UpperMassFile);

        auto ExpectedResult = InterpolateMistData(Files, TargetAge, TargetMassSol, MassCoefficient);
        if (ExpectedResult.has_value())
        {
            FDataArray& Result = ExpectedResult.value();
            Result.push_back(TargetFeH); // 加入插值使用的金属丰度，用于计算光谱类型
            return Result;
        }
        else
        {
            return ExpectedResult;
        }
    }

    std::expected<FStellarGenerator::FDataArray, Astro::AStar>
    FStellarGenerator::InterpolateMistData(const std::pair<std::string, std::string>& Files,
                                           double TargetAge, double TargetMassSol, double MassCoefficient)
    {
        FDataArray Result;

        if (Files.first.find("WhiteDwarfs") == std::string::npos)
        {
            if (Files.first != Files.second) [[likely]]
            {
                auto LowerData = LoadCsvAsset<FMistData>(Files.first,  kMistHeaders_);
                auto UpperData = LoadCsvAsset<FMistData>(Files.second, kMistHeaders_);

                auto LowerPhaseChanges = FindPhaseChanges(LowerData.Get());
                auto UpperPhaseChanges = FindPhaseChanges(UpperData.Get());

                if (std::isnan(TargetAge)) // 年龄为 NaN 在这里代表要生成濒死恒星
                {
                    double LowerLifetime = LowerPhaseChanges.back()[kStarAgeIndex_];
                    double UpperLifetime = UpperPhaseChanges.back()[kStarAgeIndex_];
                    double Lifetime = LowerLifetime + (UpperLifetime - LowerLifetime) * MassCoefficient;
                    TargetAge = Lifetime - 500000;
                }

                std::pair<std::vector<FDataArray>, std::vector<FDataArray>> PhaseChangePair
                {
                    LowerPhaseChanges,
                    UpperPhaseChanges
                };

                auto ExpectedResult = CalculateEvolutionProgress(PhaseChangePair, TargetAge, MassCoefficient);
                if (!ExpectedResult.has_value())
                {
                    return std::unexpected(ExpectedResult.error());
                }

                double EvolutionProgress = ExpectedResult.value();

                double LowerLifetime = PhaseChangePair.first.back()[kStarAgeIndex_];
                double UpperLifetime = PhaseChangePair.second.back()[kStarAgeIndex_];

                FDataArray LowerRows = InterpolateStarData(LowerData.Get(), EvolutionProgress);
                FDataArray UpperRows = InterpolateStarData(UpperData.Get(), EvolutionProgress);

                LowerRows.push_back(LowerLifetime);
                UpperRows.push_back(UpperLifetime);

                Result = InterpolateFinalData(std::make_pair(LowerRows, UpperRows), MassCoefficient, false);
            }
            else [[unlikely]]
            {
                auto StarData     = LoadCsvAsset<FMistData>(Files.first, kMistHeaders_);
                auto PhaseChanges = FindPhaseChanges(StarData.Get());

                if (std::isnan(TargetAge))
                {
                    double Lifetime = PhaseChanges.back()[kStarAgeIndex_];
                    TargetAge = Lifetime - 500000;
                }

                double EvolutionProgress = 0.0;
                double Lifetime = 0.0;
                if (TargetMassSol >= 0.1)
                {
                    std::pair<std::vector<FDataArray>, std::vector<FDataArray>> PhaseChangePair{ PhaseChanges, {} };
                    auto ExpectedResult = CalculateEvolutionProgress(PhaseChangePair, TargetAge, MassCoefficient);
                    if (!ExpectedResult.has_value())
                    {
                        return std::unexpected(ExpectedResult.error());
                    }

                    EvolutionProgress = ExpectedResult.value();
                    Lifetime          = PhaseChanges.back()[kStarAgeIndex_];
                    Result            = InterpolateStarData(StarData.Get(), EvolutionProgress);
                    Result.push_back(Lifetime);
                }
                else
                {
                    // 外推小质量恒星的数据
                    double OriginalLowerPhaseChangePoint = PhaseChanges[1][kStarAgeIndex_];
                    double OriginalUpperPhaseChangePoint = PhaseChanges[2][kStarAgeIndex_];
                    double LowerPhaseChangePoint = OriginalLowerPhaseChangePoint * std::pow(TargetMassSol / 0.1, -1.3);
                    double UpperPhaseChangePoint = OriginalUpperPhaseChangePoint * std::pow(TargetMassSol / 0.1, -1.3);
                    Lifetime = UpperPhaseChangePoint;
                    if (TargetAge < LowerPhaseChangePoint)
                    {
                        EvolutionProgress = TargetAge / LowerPhaseChangePoint - 1;
                    }
                    else if (LowerPhaseChangePoint <= TargetAge && TargetAge <= UpperPhaseChangePoint)
                    {
                        EvolutionProgress = (TargetAge - LowerPhaseChangePoint) / (UpperPhaseChangePoint - LowerPhaseChangePoint);
                    }
                    else if (TargetAge > UpperPhaseChangePoint)
                    {
                        return GenerateDeathStarPlaceholder(Lifetime);
                    }

                    Result = InterpolateStarData(StarData.Get(), EvolutionProgress);
                    Result.push_back(Lifetime);
                    ExpandMistData(TargetMassSol, Result);
                }
            }
        }
        else
        {
            if (Files.first != Files.second) [[likely]]
            {
                auto LowerData = LoadCsvAsset<FWdMistData>(Files.first,  kWdMistHeaders_);
                auto UpperData = LoadCsvAsset<FWdMistData>(Files.second, kWdMistHeaders_);

                FDataArray LowerRows = InterpolateStarData(LowerData.Get(), TargetAge);
                FDataArray UpperRows = InterpolateStarData(UpperData.Get(), TargetAge);

                Result = InterpolateFinalData(std::make_pair(LowerRows, UpperRows), MassCoefficient, true);
            }
            else [[unlikely]]
            {
                auto StarData = LoadCsvAsset<FWdMistData>(Files.first, kWdMistHeaders_);
                Result = InterpolateStarData(StarData.Get(), TargetAge);
            }
        }

        return Result;
    }

    std::vector<FStellarGenerator::FDataArray> FStellarGenerator::FindPhaseChanges(const FMistData* DataSheet)
    {
        std::vector<FDataArray> Result;
        {
            std::shared_lock Lock(CacheMutex_);
            auto it = PhaseChangesCache_.find(DataSheet);
            if (it != PhaseChangesCache_.end())
            {
                return PhaseChangesCache_[DataSheet];
            }
        }

        const auto* const CsvData = DataSheet->Data();
        int CurrentPhase = -2;
        for (const auto& Row : *CsvData)
        {
            if (Row[kPhaseIndex_] != CurrentPhase || Row[kXIndex_] == 10.0)
            {
                CurrentPhase = static_cast<int>(Row[kPhaseIndex_]);
                Result.push_back(Row);
            }
        }

        {
            std::unique_lock Lock(CacheMutex_);
            if (!PhaseChangesCache_.contains(DataSheet))
            {
                PhaseChangesCache_.emplace(DataSheet, Result);
            }
            else
            {
                Result = PhaseChangesCache_[DataSheet];
            }
        }

        return Result;
    }

    std::expected<double, Astro::AStar>
    FStellarGenerator::CalculateEvolutionProgress(std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& PhaseChanges,
                                                  double TargetAge, double MassCoefficient)
    {
        double Result = 0.0;
        double Phase  = 0.0;

        if (PhaseChanges.second.empty()) [[unlikely]]
        {
            auto TimePointResults = FindSurroundingTimePoints(PhaseChanges.first, TargetAge);
            Phase = TimePointResults.first;
            const auto& TimePoints = TimePointResults.second;
            if (TargetAge > TimePoints.second)
            {
                return GenerateDeathStarPlaceholder(TimePoints.second);
            }

            Result = (TargetAge - TimePoints.first) / (TimePoints.second - TimePoints.first) + Phase;
        }
        else [[likely]]
        {
            if (PhaseChanges.first.size() == PhaseChanges.second.size() &&
                (*std::prev(PhaseChanges.first.end(), 2))[kPhaseIndex_] == (*std::prev(PhaseChanges.second.end(), 2))[kPhaseIndex_])
            {
                auto ExpectedResult = FindSurroundingTimePoints(PhaseChanges, TargetAge, MassCoefficient);
                if (!ExpectedResult.has_value())
                {
                    return std::unexpected(ExpectedResult.error());
                }

                const auto& TimePointResults = ExpectedResult.value();

                Phase = TimePointResults.first;
                std::size_t Index = TimePointResults.second;

                if (Index + 1 != PhaseChanges.first.size())
                {
                    std::pair<double, double> LowerTimePoints
                    {
                        PhaseChanges.first[Index][kStarAgeIndex_],
                        PhaseChanges.first[Index + 1][kStarAgeIndex_]
                    };

                    std::pair<double, double> UpperTimePoints
                    {
                        PhaseChanges.second[Index][kStarAgeIndex_],
                        PhaseChanges.second[Index + 1][kStarAgeIndex_]
                    };

                    const auto& [LowerLowerTimePoint, LowerUpperTimePoint] = LowerTimePoints;
                    const auto& [UpperLowerTimePoint, UpperUpperTimePoint] = UpperTimePoints;

                    double LowerTimePoint = LowerLowerTimePoint + (UpperLowerTimePoint - LowerLowerTimePoint) * MassCoefficient;
                    double UpperTimePoint = LowerUpperTimePoint + (UpperUpperTimePoint - LowerUpperTimePoint) * MassCoefficient;

                    Result = (TargetAge - LowerTimePoint) / (UpperTimePoint - LowerTimePoint) + Phase;

                    if (Result > PhaseChanges.first.back()[kPhaseIndex_] + 1)
                    {
                        return 0.0;
                    }
                }
                else
                {
                    Result = 0.0;
                }
            }
            else
            {
                if (PhaseChanges.first.back()[kPhaseIndex_] == PhaseChanges.second.back()[kPhaseIndex_])
                {
                    double FirstDiscardTimePoint = 0.0;
                    double FirstCommonTimePoint  = (*std::prev(PhaseChanges.first.end(), 2))[kStarAgeIndex_];

                    std::size_t MinSize = std::min(PhaseChanges.first.size(), PhaseChanges.second.size());
                    for (std::size_t i = 0; i != MinSize - 1; ++i)
                    {
                        if (PhaseChanges.first[i][kPhaseIndex_] != PhaseChanges.second[i][kPhaseIndex_])
                        {
                            FirstDiscardTimePoint = PhaseChanges.first[i][kStarAgeIndex_];
                            break;
                        }
                    }

                    double DeltaTimePoint = FirstCommonTimePoint - FirstDiscardTimePoint;
                    (*std::prev(PhaseChanges.first.end(), 2))[kStarAgeIndex_] -= DeltaTimePoint;
                    PhaseChanges.first.back()[kStarAgeIndex_] -= DeltaTimePoint;
                }

                AlignArrays(PhaseChanges);

                auto ExpectedResult = CalculateEvolutionProgress(PhaseChanges, TargetAge, MassCoefficient);
                if (!ExpectedResult.has_value())
                {
                    return std::unexpected(ExpectedResult.error());
                }

                Result = ExpectedResult.value();
                double IntegerPart    = 0.0;
                double FractionalPart = std::modf(Result, &IntegerPart);
                if (PhaseChanges.second.back()[kPhaseIndex_] == 9 && FractionalPart > 0.99 && Result < 9.0 &&
                    IntegerPart >= (*std::prev(PhaseChanges.first.end(), 3))[kPhaseIndex_])
                {
                    Result = 9.0;
                }
            }
        }

        return Result;
    }

    std::pair<double, std::pair<double, double>>
    FStellarGenerator::FindSurroundingTimePoints(const std::vector<FDataArray>& PhaseChanges, double TargetAge)
    {
        std::vector<FDataArray>::const_iterator LowerTimePoint;
        std::vector<FDataArray>::const_iterator UpperTimePoint;

        if (PhaseChanges.size() != 2 || PhaseChanges.front()[kPhaseIndex_] != PhaseChanges.back()[kPhaseIndex_])
        {
            LowerTimePoint = std::lower_bound(PhaseChanges.begin(), PhaseChanges.end(), TargetAge,
            [](const FDataArray& Lhs, double Rhs) -> bool
            {
                return Lhs[0] < Rhs;
            });

            UpperTimePoint = std::upper_bound(PhaseChanges.begin(), PhaseChanges.end(), TargetAge,
            [](double Lhs, const FDataArray& Rhs) -> bool
            {
                return Lhs < Rhs[0];
            });

            if (LowerTimePoint == UpperTimePoint)
            {
                if (LowerTimePoint != PhaseChanges.begin())
                {
                    --LowerTimePoint;
                }
            }

            if (UpperTimePoint == PhaseChanges.end())
            {
                --LowerTimePoint;
                --UpperTimePoint;
            }
        }
        else
        {
            LowerTimePoint = PhaseChanges.begin();
            UpperTimePoint = std::prev(PhaseChanges.end(), 1);
        }

        // return std::make_pair(LowerTimePoint->back(), std::make_pair(LowerTimePoint->front(), UpperTimePoint->front()));
        return std::make_pair((*LowerTimePoint)[kXIndex_], std::make_pair((*LowerTimePoint)[kStarAgeIndex_], (*UpperTimePoint)[kStarAgeIndex_]));
    }

    std::expected<std::pair<double, std::size_t>, Astro::AStar>
    FStellarGenerator::FindSurroundingTimePoints(const std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& PhaseChanges,
                                                 double TargetAge, double MassCoefficient)
    {
        FDataArray LowerPhaseChangeTimePoints;
        FDataArray UpperPhaseChangeTimePoints;
        for (std::size_t i = 0; i != PhaseChanges.first.size(); ++i)
        {
            LowerPhaseChangeTimePoints.push_back(PhaseChanges.first[i][kStarAgeIndex_]);
            UpperPhaseChangeTimePoints.push_back(PhaseChanges.second[i][kStarAgeIndex_]);
        }

        FDataArray PhaseChangeTimePoints =
            InterpolateArray({ LowerPhaseChangeTimePoints, UpperPhaseChangeTimePoints }, MassCoefficient);

        if (TargetAge > PhaseChangeTimePoints.back())
        {
            double Lifetime = LowerPhaseChangeTimePoints.back() +
                (UpperPhaseChangeTimePoints.back() - LowerPhaseChangeTimePoints.back()) * MassCoefficient;
            return GenerateDeathStarPlaceholder(Lifetime);
        }

        std::vector<std::pair<double, double>> TimePointPairs;
        for (std::size_t i = 0; i != PhaseChanges.first.size(); ++i)
        {
            TimePointPairs.emplace_back(PhaseChanges.first[i][kPhaseIndex_], PhaseChangeTimePoints[i]);
        }

        std::pair<double, std::size_t> Result;
        for (std::size_t i = 0; i != TimePointPairs.size(); ++i)
        {
            if (TimePointPairs[i].second >= TargetAge)
            {
                Result.first = TimePointPairs[i == 0 ? 0 : i - 1].first;
                Result.second = i == 0 ? 0 : i - 1;
                break;
            }
        }

        return Result;
    }

    void FStellarGenerator::AlignArrays(std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& Arrays)
    {
        if (Arrays.first.back()[kPhaseIndex_] != 9 && Arrays.second.back()[kPhaseIndex_] != 9)
        {
            std::size_t MinSize = std::min(Arrays.first.size(), Arrays.second.size());
            Arrays.first.resize(MinSize);
            Arrays.second.resize(MinSize);
        }
        else if (Arrays.first.back()[kPhaseIndex_] != 9 && Arrays.second.back()[kPhaseIndex_] == 9)
        {
            if (Arrays.first.size() + 1 == Arrays.second.size())
            {
                Arrays.second.pop_back();
                Arrays.second.back()[kPhaseIndex_] = Arrays.first.back()[kPhaseIndex_];
                Arrays.second.back()[kXIndex_] = Arrays.first.back()[kXIndex_];
            }
            else
            {
                std::size_t MinSize = std::min(Arrays.first.size(), Arrays.second.size());
                Arrays.first.resize(MinSize - 1);
                Arrays.second.resize(MinSize - 1);
                Arrays.second.back()[kPhaseIndex_] = Arrays.first.back()[kPhaseIndex_];
                Arrays.second.back()[kXIndex_] = Arrays.first.back()[kXIndex_];
            }
        }
        else if (Arrays.first.back()[kPhaseIndex_] == 9 && Arrays.second.back()[kPhaseIndex_] == 9)
        {
            FDataArray LastArray1 = Arrays.first.back();
            FDataArray LastArray2 = Arrays.second.back();
            FDataArray SubLastArray1 = *std::prev(Arrays.first.end(), 2);
            FDataArray SubLastArray2 = *std::prev(Arrays.second.end(), 2);

            std::size_t MinSize = std::min(Arrays.first.size(), Arrays.second.size());

            Arrays.first.resize(MinSize - 2);
            Arrays.second.resize(MinSize - 2);
            Arrays.first.push_back(std::move(SubLastArray1));
            Arrays.first.push_back(std::move(LastArray1));
            Arrays.second.push_back(std::move(SubLastArray2));
            Arrays.second.push_back(std::move(LastArray2));
        }
        else
        {
            FDataArray LastArray1 = Arrays.first.back();
            FDataArray LastArray2 = Arrays.second.back();
            std::size_t MinSize = std::min(Arrays.first.size(), Arrays.second.size());
            Arrays.first.resize(MinSize - 1);
            Arrays.second.resize(MinSize - 1);
            Arrays.first.push_back(std::move(LastArray1));
            Arrays.second.push_back(std::move(LastArray2));
        }
    }

    FStellarGenerator::FDataArray FStellarGenerator::InterpolateHrDiagram(FStellarGenerator::FHrDiagram* Data, double BvColorIndex)
    {
        FDataArray Result;

        std::pair<FDataArray, FDataArray> SurroundingRows;
        try
        {
            SurroundingRows = Data->FindSurroundingValues("B-V", BvColorIndex);
        }
        catch (std::out_of_range& e)
        {
            NpgsCoreError("H-R Diagram interpolation capture exception: " + std::string(e.what()));
        }

        double Coefficient = (BvColorIndex - SurroundingRows.first[0]) / (SurroundingRows.second[0] - SurroundingRows.first[0]);

        auto& Array1 = SurroundingRows.first;
        auto& Array2 = SurroundingRows.second;

        while (!Array1.empty() && !Array2.empty() && (Array1.back() == -1 || Array2.back() == -1))
        {
            Array1.pop_back();
            Array2.pop_back();
        }

        Result = InterpolateArray(SurroundingRows, Coefficient);

        return Result;
    }

    FStellarGenerator::FDataArray
    FStellarGenerator::InterpolateStarData(FStellarGenerator::FMistData* Data, double EvolutionProgress)
    {
        return InterpolateStarData(Data, EvolutionProgress, "x", FStellarGenerator::kXIndex_, false);
    }

    FStellarGenerator::FDataArray FStellarGenerator::InterpolateStarData(FStellarGenerator::FWdMistData* Data, double TargetAge)
    {
        return InterpolateStarData(Data, TargetAge, "star_age", FStellarGenerator::kWdStarAgeIndex_, true);
    }

    FStellarGenerator::FDataArray
    FStellarGenerator::InterpolateStarData(auto* Data, double Target, const std::string& Header, int Index, bool bIsWhiteDwarf)
    {
        FDataArray Result;

        std::pair<FDataArray, FDataArray> SurroundingRows;
        try
        {
            SurroundingRows = Data->FindSurroundingValues(Header, Target);
        }
        catch (std::out_of_range& e)
        {
            if (!bIsWhiteDwarf)
            {
                NpgsCoreError("Stellar data interpolation capture exception: " + std::string(e.what()));
                NpgsCoreError("Header: {}, Target: {}", Header, Target);
            }
            else
            {
                SurroundingRows.first  = Data->Data()->back();
                SurroundingRows.second = Data->Data()->back();
            }
        }

        if (SurroundingRows.first != SurroundingRows.second)
        {
            if (!bIsWhiteDwarf)
            {
                int LowerPhase = static_cast<int>(SurroundingRows.first[Index]);
                int UpperPhase = static_cast<int>(SurroundingRows.second[Index]);
                if (LowerPhase != UpperPhase)
                {
                    SurroundingRows.second[Index] = LowerPhase + 1;
                }
            }

            double Coefficient = (Target - SurroundingRows.first[Index]) / (SurroundingRows.second[Index] - SurroundingRows.first[Index]);
            Result = InterpolateFinalData(SurroundingRows, Coefficient, bIsWhiteDwarf);
        }
        else
        {
            Result = SurroundingRows.first;
        }

        return Result;
    }

    FStellarGenerator::FDataArray
    FStellarGenerator::InterpolateArray(const std::pair<FDataArray, FDataArray>& DataArrays, double Coefficient)
    {
        if (DataArrays.first.size() != DataArrays.second.size())
        {
            throw std::runtime_error("Data arrays size mismatch.");
        }

        std::size_t Size = DataArrays.first.size();
        FDataArray Result(Size);
        for (std::size_t i = 0; i != Size; ++i)
        {
            Result[i] = DataArrays.first[i] + (DataArrays.second[i] - DataArrays.first[i]) * Coefficient;
        }

        return Result;
    }

    FStellarGenerator::FDataArray
    FStellarGenerator::InterpolateFinalData(const std::pair<FDataArray, FDataArray>& DataArrays, double Coefficient, bool bIsWhiteDwarf)
    {
        if (DataArrays.first.size() != DataArrays.second.size())
        {
            throw std::runtime_error("Data arrays size mismatch.");
        }

        FDataArray Result = InterpolateArray(DataArrays, Coefficient);

        if (!bIsWhiteDwarf)
        {
            Result[FStellarGenerator::kPhaseIndex_] = DataArrays.first[FStellarGenerator::kPhaseIndex_];
        }

        return Result;
    }

    void FStellarGenerator::CalculateSpectralType(float FeH, Astro::AStar& StarData)
    {
        float Teff           = StarData.GetTeff();
        auto  EvolutionPhase = StarData.GetEvolutionPhase();

        Astro::EStellarType  StellarType = StarData.GetStellarClass().GetStellarType();
        Astro::FSpectralType SpectralType;
        SpectralType.bIsAmStar = false;

        std::vector<std::pair<int, int>> SpectralSubclassMap;
        float InitialMassSol    = StarData.GetInitialMass() / kSolarMass;
        float Subclass          = 0.0f;
        float SurfaceH1         = StarData.GetSurfaceH1();
        float SurfaceZ          = StarData.GetSurfaceZ();
        float MinSurfaceH1      = Astro::AStar::kFeHSurfaceH1Map_.at(FeH) - 0.01f;
		float WNxhMassThreshold = CalculateWNxhMassThreshold(FeH);

        auto CalculateSpectralSubclass = [&](this auto&& Self, Astro::AStar::EEvolutionPhase BasePhase) -> void
        {
            std::uint32_t SpectralClass = BasePhase == Astro::AStar::EEvolutionPhase::kWolfRayet ? 11 : 0;

            if (BasePhase != Astro::AStar::EEvolutionPhase::kWolfRayet)
            {
                if (BasePhase == Astro::AStar::EEvolutionPhase::kMainSequence)
                {
                    // 如果表面氢质量分数低于 0.4 且还是主序星阶段，或初始质量超过 WNxh 门槛，转为 WR 星
                    // 该情况只有 O 型星会出现
                    if (SurfaceH1 < 0.4f || InitialMassSol > WNxhMassThreshold)
                    {
                        EvolutionPhase = Astro::AStar::EEvolutionPhase::kWolfRayet;
                        StarData.SetEvolutionPhase(EvolutionPhase);
                        Self(EvolutionPhase);
                        return;
                    }
                }

                const auto& InitialMap = Astro::AStar::kInitialCommonMap_;
                for (auto it = InitialMap.begin(); it != InitialMap.end() - 1; ++it)
                {
                    ++SpectralClass;
                    if (it->first >= Teff && (it + 1)->first < Teff)
                    {
                        SpectralSubclassMap = it->second;
                        break;
                    }
                }
            }
            else
            {
                if (SurfaceZ <= 0.05)
                {
                    SpectralSubclassMap      = Astro::AStar::kSpectralSubclassMap_WNxh_;
                    SpectralClass            = std::to_underlying(Astro::ESpectralClass::kSpectral_WN);
                    SpectralType.SpecialMark = std::to_underlying(Astro::ESpecialMark::kCode_h);
                }
                else if (SurfaceZ <= 0.1f)
                {
                    SpectralSubclassMap      = Astro::AStar::kSpectralSubclassMap_WN_;
                    SpectralClass            = std::to_underlying(Astro::ESpectralClass::kSpectral_WN);
                }
                else if (SurfaceZ <= 0.6f)
                {
                    if (InitialMassSol <= 140)
                    {
                        SpectralSubclassMap  = Astro::AStar::kSpectralSubclassMap_WC_;
                        SpectralClass        = std::to_underlying(Astro::ESpectralClass::kSpectral_WC);
                    }
                    else
                    {
                        SpectralSubclassMap  = Astro::AStar::kSpectralSubclassMap_WN_;
                        SpectralClass        = std::to_underlying(Astro::ESpectralClass::kSpectral_WN);
                    }
                }
                else
                {
                    SpectralSubclassMap      = Astro::AStar::kSpectralSubclassMap_WO_;
                    SpectralClass            = std::to_underlying(Astro::ESpectralClass::kSpectral_WO);
                }
            }

            SpectralType.HSpectralClass = static_cast<Astro::ESpectralClass>(SpectralClass);

            if (SpectralSubclassMap.empty())
            {
                NpgsCoreError("Failed to find match subclass map of Age={}, FeH={}, Mass={}, Teff={}",
                              StarData.GetAge(), StarData.GetFeH(), StarData.GetMass() / kSolarMass, StarData.GetTeff());
            }

            Subclass = static_cast<float>(SpectralSubclassMap.front().second);
            if (Teff < std::next(SpectralSubclassMap.begin(), 1)->first)
            {
                Subclass = static_cast<float>(std::next(SpectralSubclassMap.begin(), 1)->second);
                for (auto it = SpectralSubclassMap.begin() + 1; it != SpectralSubclassMap.end() - 1; ++it)
                {
                    if (it->first >= Teff && (it + 1)->first < Teff)
                    {
                        Subclass = static_cast<float>(it->second);
                        break;
                    }
                    ++Subclass;
                }
            }

            SpectralType.Subclass = Subclass;
        };

        if (EvolutionPhase != Astro::AStar::EEvolutionPhase::kWolfRayet)
        {
            switch (StellarType)
            {
            case Astro::EStellarType::kNormalStar:
            {
                if (Teff < 54000)
                {
                    CalculateSpectralSubclass(EvolutionPhase);

                    if (EvolutionPhase != Astro::AStar::EEvolutionPhase::kWolfRayet)
                    {
                        if (EvolutionPhase == Astro::AStar::EEvolutionPhase::kPrevMainSequence)
                        {
                            SpectralType.LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown;
                        }
                        else if (EvolutionPhase == Astro::AStar::EEvolutionPhase::kMainSequence)
                        {
                            if (SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_O &&
                                SurfaceH1 < MinSurfaceH1)
                            {
                                SpectralType.LuminosityClass = CalculateLuminosityClass(StarData);
                            }
                            else
                            {
                                SpectralType.LuminosityClass = Astro::ELuminosityClass::kLuminosity_V;
                            }
                        }
                        else
                        {
                            SpectralType.LuminosityClass = CalculateLuminosityClass(StarData);
                        }
                    }
                    else
                    {
                        SpectralType.LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown;
                    }
                }
                else
                {
                    // 前主序恒星温度达不到 O2 上限
                    if (InitialMassSol <= WNxhMassThreshold)
                    {
                        if (SurfaceH1 > MinSurfaceH1)
                        {
                            SpectralType.HSpectralClass  = Astro::ESpectralClass::kSpectral_O;
                            SpectralType.Subclass        = 2.0f;
                            SpectralType.LuminosityClass = Astro::ELuminosityClass::kLuminosity_V;
                        }
                        else if (SurfaceH1 > 0.5f)
                        {
                            SpectralType.HSpectralClass  = Astro::ESpectralClass::kSpectral_O;
                            SpectralType.Subclass        = 2.0f;
                            SpectralType.LuminosityClass = CalculateLuminosityClass(StarData);
                        }
                        else
                        {
                            CalculateSpectralSubclass(Astro::AStar::EEvolutionPhase::kWolfRayet);
                        }
                    }
                    else
                    {
                        CalculateSpectralSubclass(Astro::AStar::EEvolutionPhase::kWolfRayet);
                    }
                }

                break;
            }
            case Astro::EStellarType::kWhiteDwarf:
            {
                double MassSol = StarData.GetMass() / kSolarMass;

                if (Teff >= 12000)
                {
                    if (MassSol <= 0.5)
                    {
                        SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_DA;
                    }
                    else
                    {
                        if (Teff > 45000)
                        {
                            SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_DO;
                        }
                        else
                        {
                            SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_DB;
                        }
                    }
                }
                else
                {
                    SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_DC;
                }

                Subclass = 50400.0f / Teff;
                if (Subclass > 9.5f)
                {
                    Subclass = 9.5f;
                }

                SpectralType.Subclass = std::round(Subclass * 2.0f) / 2.0f;

                break;
            }
            case Astro::EStellarType::kNeutronStar:
            {
                SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_Q;
                break;
            }
            case Astro::EStellarType::kBlackHole:
            {
                SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_X;
                break;
            }
            case Astro::EStellarType::kDeathStarPlaceholder:
            {
                SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_Unknown;
                break;
            }
            default:
            {
                SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_Unknown;
                break;
            }
            }
        }
        else
        {
            CalculateSpectralSubclass(Astro::AStar::EEvolutionPhase::kWolfRayet);
            SpectralType.LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown;
        }

        Astro::FStellarClass StellarClass(StellarType, SpectralType);
        StarData.SetStellarClass(StellarClass);
    }

    Astro::ELuminosityClass FStellarGenerator::CalculateLuminosityClass(const Astro::AStar& StarData)
    {
        float MassLossRateSolPerYear = StarData.GetStellarWindMassLossRate() * kYearToSecond / kSolarMass;
        double MassSol = StarData.GetMass() / kSolarMass;
        Astro::ELuminosityClass LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown;

        double LuminositySol = StarData.GetLuminosity() / kSolarLuminosity;
        if (LuminositySol > 650000) // 光度高于 650000 Lsun
        {
            LuminosityClass = Astro::ELuminosityClass::kLuminosity_IaPlus;
        }

        if (MassLossRateSolPerYear > 1e-4f && MassSol >= 15) // 表面物质流失率大于 1e-4 Msun/yr 并且质量大于等于 15 Msun
        {
            LuminosityClass = Astro::ELuminosityClass::kLuminosity_0;
        }

        if (LuminosityClass != Astro::ELuminosityClass::kLuminosity_Unknown) // 如果判断为特超巨星，直接返回
        {
            return LuminosityClass;
        }

        std::string HrDiagramDataFilePath = GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/H-R Diagram/H-R Diagram.csv");
        auto        HrDiagramData         = LoadCsvAsset<FHrDiagram>(HrDiagramDataFilePath, kHrDiagramHeaders_);

        float Teff         = StarData.GetTeff();
        float BvColorIndex = 0.0f;
        if (std::log10(Teff) < 3.691f)
        {
            BvColorIndex = -3.684f * std::log10(Teff) + 14.551f;
        }
        else
        {
            BvColorIndex = 0.344f * std::pow(std::log10(Teff), 2.0f) - 3.402f * std::log10(Teff) + 8.037f;
        }

        if (BvColorIndex < -0.3f || BvColorIndex > 1.9727273f)
        {
            // 超过 HR 表的范围，使用光度判断
            if (LuminositySol > 100000)
            {
                return Astro::ELuminosityClass::kLuminosity_Ia;
            }
            else if (LuminositySol > 50000)
            {
                return Astro::ELuminosityClass::kLuminosity_Iab;
            }
            else if (LuminositySol > 10000)
            {
                return Astro::ELuminosityClass::kLuminosity_Ib;
            }
            else if (LuminositySol > 1000)
            {
                return Astro::ELuminosityClass::kLuminosity_II;
            }
            else if (LuminositySol > 100)
            {
                return Astro::ELuminosityClass::kLuminosity_III;
            }
            else if (LuminositySol > 10)
            {
                return Astro::ELuminosityClass::kLuminosity_IV;
            }
            else if (LuminositySol > 0.05)
            {
                return Astro::ELuminosityClass::kLuminosity_V;
            }
            else
            {
                return Astro::ELuminosityClass::kLuminosity_VI;
            }
        }

        FDataArray LuminosityData = InterpolateHrDiagram(HrDiagramData.Get(), BvColorIndex);
        if (LuminositySol > LuminosityData[1])
        {
            return Astro::ELuminosityClass::kLuminosity_Ia;
        }

        double ClosestValue = *std::min_element(LuminosityData.begin() + 1, LuminosityData.end(),
        [LuminositySol](double Lhs, double Rhs) -> bool
        {
            return std::abs(Lhs - LuminositySol) < std::abs(Rhs - LuminositySol);
        });

        while (LuminosityData.size() < 7)
        {
            LuminosityData.push_back(-1);
        }

        if (LuminositySol <= LuminosityData[1] && LuminositySol >= LuminosityData[2] &&
            (ClosestValue == LuminosityData[1] || ClosestValue == LuminosityData[2]))
        {
            LuminosityClass = Astro::ELuminosityClass::kLuminosity_Iab;
        }
        else
        {
            if (ClosestValue == LuminosityData[2])
            {
                LuminosityClass = Astro::ELuminosityClass::kLuminosity_Ib;
            }
            else if (ClosestValue == LuminosityData[3])
            {
                LuminosityClass = Astro::ELuminosityClass::kLuminosity_II;
            }
            else if (ClosestValue == LuminosityData[4])
            {
                LuminosityClass = Astro::ELuminosityClass::kLuminosity_III;
            }
            else if (ClosestValue == LuminosityData[5])
            {
                LuminosityClass = Astro::ELuminosityClass::kLuminosity_IV;
            }
            else if (ClosestValue == LuminosityData[6])
            {
                LuminosityClass = Astro::ELuminosityClass::kLuminosity_V;
            }
        }

        return LuminosityClass;
    }

    void FStellarGenerator::ProcessDeathStar(EStellarTypeGenerationOption DeathStarTypeOption, Astro::AStar& DeathStar)
    {
        double InputAge     = DeathStar.GetAge();
        float  InputFeH     = DeathStar.GetFeH();
        float  InputMassSol = DeathStar.GetInitialMass() / kSolarMass;

        Astro::AStar::EEvolutionPhase EvolutionPhase{};
        Astro::AStar::EStarFrom       DeathStarFrom{};
        Astro::EStellarType           DeathStarType{};
        Astro::FSpectralType          DeathStarClass;

        double DeathStarAge = InputAge - static_cast<float>(DeathStar.GetLifetime());
        float DeathStarMassSol = 0.0f;

        auto CalculateBlackHoleMass = [&, this]() -> float
        {
            FStellarBasicProperties Properties
            {
                .StellarTypeOption = EStellarTypeGenerationOption::kRandom,
                .Age               = static_cast<float>(DeathStar.GetLifetime() - 100),
                .FeH               = InputFeH,
                .InitialMassSol    = InputMassSol
            };

            auto GiantStar = GenerateStar(Properties);

            return static_cast<float>(GiantStar.GetMass() / kSolarMass * 0.8);
        };

        if (InputFeH <= -2.0f && InputMassSol >= 140 && InputMassSol < 250)
        {
            EvolutionPhase = Astro::AStar::EEvolutionPhase::kNull;
            DeathStarFrom  = Astro::AStar::EStarFrom::kPairInstabilitySupernova;
            DeathStarType  = Astro::EStellarType::kDeathStarPlaceholder;
            DeathStarClass =
            {
                .HSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                .bIsAmStar       = false,
                .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                .Subclass        = 0.0f,
                .AmSubclass      = 0.0f
            };
        }
        else if (InputFeH <= -2.0f && InputMassSol >= 250)
        {
            EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
            DeathStarFrom  = Astro::AStar::EStarFrom::kPhotondisintegration;
            DeathStarType  = Astro::EStellarType::kBlackHole;
            DeathStarClass =
            {
                .HSpectralClass  = Astro::ESpectralClass::kSpectral_X,
                .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                .bIsAmStar       = false,
                .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                .Subclass        = 0.0f,
                .AmSubclass      = 0.0f
            };
            DeathStarMassSol = CalculateBlackHoleMass();
        }
        else
        {
            if (InputMassSol > -0.75f && InputMassSol < 0.8f)
            {
                DeathStarMassSol = (0.9795f - 0.393f * InputMassSol) * InputMassSol;
            }
            else if (InputMassSol >= 0.8f && InputMassSol < 7.9f)
            {
                DeathStarMassSol =
                    -0.00012336f * std::pow(InputMassSol, 6.0f) + 0.003160f * std::pow(InputMassSol, 5.0f) -
                     0.02960f    * std::pow(InputMassSol, 4.0f) + 0.12350f  * std::pow(InputMassSol, 3.0f) -
                     0.21550f    * std::pow(InputMassSol, 2.0f) + 0.19022f  * InputMassSol + 0.46575f;
            }
            else if (InputMassSol >= 7.9f && InputMassSol < 10.0f)
            {
                DeathStarMassSol = 1.301f + 0.008095f * InputMassSol;
            }
            else if (InputMassSol >= 10.0f && InputMassSol < 21.0f)
            {
                DeathStarMassSol = 1.246f + 0.0136f * InputMassSol;
            }
            else if (InputMassSol >= 21.0f && InputMassSol < 23.3537f)
            {
                DeathStarMassSol = std::pow(10.0f, (1.334f - 0.009987f * InputMassSol));
            }
            else if (InputMassSol >= 23.3537f && InputMassSol < 33.75f)
            {
                DeathStarMassSol = 12.1f - 0.763f * InputMassSol + 0.0137f * std::pow(InputMassSol, 2.0f);
            }
            else
            {
                DeathStarMassSol = CalculateBlackHoleMass();
            }

            if (InputMassSol >= 0.075f && InputMassSol < 0.5f)
            {
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kHeliumWhiteDwarf;
                DeathStarFrom  = Astro::AStar::EStarFrom::kSlowColdingDown;
                DeathStarType  = Astro::EStellarType::kWhiteDwarf;
                DeathStarClass =
                {
                    .HSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                    .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                    .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                    .bIsAmStar       = false,
                    .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                    .Subclass        = 0.0f,
                    .AmSubclass      = 0.0f
                };
            }
            else if (InputMassSol >= 0.5f && InputMassSol < 8.0f)
            {
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kCarbonOxygenWhiteDwarf;
                DeathStarFrom  = Astro::AStar::EStarFrom::kEnvelopeDisperse;
                DeathStarType  = Astro::EStellarType::kWhiteDwarf;
                DeathStarClass =
                {
                    .HSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                    .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                    .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                    .bIsAmStar       = false,
                    .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                    .Subclass        = 0.0f,
                    .AmSubclass      = 0.0f
                };
            }
            else if (InputMassSol >= 8.0f && InputMassSol < 9.759f)
            {
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kOxygenNeonMagnWhiteDwarf;
                DeathStarFrom  = Astro::AStar::EStarFrom::kEnvelopeDisperse;
                DeathStarType  = Astro::EStellarType::kWhiteDwarf;
                DeathStarClass =
                {
                    .HSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                    .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                    .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                    .bIsAmStar       = false,
                    .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                    .Subclass        = 0.0f,
                    .AmSubclass      = 0.0f
                };
            }
            else if (InputMassSol >= 9.759f && InputMassSol < 10.0f)
            {
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kNeutronStar;
                DeathStarFrom  = Astro::AStar::EStarFrom::kElectronCaptureSupernova;
                DeathStarType  = Astro::EStellarType::kNeutronStar;
                DeathStarClass =
                {
                    .HSpectralClass  = Astro::ESpectralClass::kSpectral_Q,
                    .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                    .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                    .bIsAmStar       = false,
                    .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                    .Subclass        = 0.0f,
                    .AmSubclass      = 0.0f
                };
            }
            else if (InputMassSol >= 10.0f && InputMassSol < 21.0f)
            {
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kNeutronStar;
                DeathStarFrom  = Astro::AStar::EStarFrom::kIronCoreCollapseSupernova;
                DeathStarType  = Astro::EStellarType::kNeutronStar;
                DeathStarClass =
                {
                    .HSpectralClass  = Astro::ESpectralClass::kSpectral_Q,
                    .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                    .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                    .bIsAmStar       = false,
                    .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                    .Subclass        = 0.0f,
                    .AmSubclass      = 0.0f
                };
            }
            else if (InputMassSol >= 21.0f && InputMassSol < 23.3537f)
            {
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
                DeathStarFrom  = Astro::AStar::EStarFrom::kIronCoreCollapseSupernova;
                DeathStarType  = Astro::EStellarType::kBlackHole;
                DeathStarClass =
                {
                    .HSpectralClass  = Astro::ESpectralClass::kSpectral_X,
                    .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                    .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                    .bIsAmStar       = false,
                    .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                    .Subclass        = 0.0f,
                    .AmSubclass      = 0.0f
                };
            }
            else if (InputMassSol >= 23.3537f && InputMassSol < 33.75f)
            {
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kNeutronStar;
                DeathStarFrom  = Astro::AStar::EStarFrom::kIronCoreCollapseSupernova;
                DeathStarType  = Astro::EStellarType::kNeutronStar;
                DeathStarClass =
                {
                    .HSpectralClass  = Astro::ESpectralClass::kSpectral_Q,
                    .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                    .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                    .bIsAmStar       = false,
                    .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                    .Subclass        = 0.0f,
                    .AmSubclass      = 0.0f
                };
            }
            else
            {
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
                DeathStarFrom  = Astro::AStar::EStarFrom::kRelativisticJetHypernova;
                DeathStarType  = Astro::EStellarType::kBlackHole;
                DeathStarClass =
                {
                    .HSpectralClass  = Astro::ESpectralClass::kSpectral_X,
                    .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                    .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                    .bIsAmStar       = false,
                    .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                    .Subclass        = 0.0f,
                    .AmSubclass      = 0.0f
                };
            }
        }

        if (DeathStarTypeOption == EStellarTypeGenerationOption::kMergeStar ||
            DeathStarType == Astro::EStellarType::kNeutronStar)
        {
            float MergeStarProbability = 0.1f * static_cast<int>(DeathStar.IsSingleStar());
            MergeStarProbability *= static_cast<int>(DeathStarTypeOption != EStellarTypeGenerationOption::kDeathStar);
            Math::TBernoulliDistribution MergeProbability(MergeStarProbability);
            if (DeathStarTypeOption == EStellarTypeGenerationOption::kMergeStar || MergeProbability(RandomEngine_))
            {
                DeathStar.SetSingleton(true);
                DeathStarFrom = Astro::AStar::EStarFrom::kWhiteDwarfMerge;
                Math::TBernoulliDistribution BlackHoleProbability(0.114514);
                float MassSol = 0.0f;
                if (BlackHoleProbability(RandomEngine_))
                {
                    Math::TUniformRealDistribution<> MassDistribution(2.6f, 2.76f);
                    MassSol        = MassDistribution(RandomEngine_);
                    EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
                    DeathStarType  = Astro::EStellarType::kBlackHole;
                    DeathStarClass =
                    {
                        .HSpectralClass  = Astro::ESpectralClass::kSpectral_X,
                        .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                        .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                        .bIsAmStar       = false,
                        .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                        .Subclass        = 0.0f,
                        .AmSubclass      = 0.0f
                    };
                }
                else
                {
                    Math::TUniformRealDistribution<> MassDistribution(1.38f, 2.18072f);
                    MassSol        = MassDistribution(RandomEngine_);
                    EvolutionPhase = Astro::AStar::EEvolutionPhase::kNeutronStar;
                    DeathStarType  = Astro::EStellarType::kNeutronStar;
                    DeathStarClass =
                    {
                        .HSpectralClass  = Astro::ESpectralClass::kSpectral_Q,
                        .MSpectralClass  = Astro::ESpectralClass::kSpectral_Unknown,
                        .LuminosityClass = Astro::ELuminosityClass::kLuminosity_Unknown,
                        .bIsAmStar       = false,
                        .SpecialMark     = std::to_underlying(Astro::ESpecialMark::kCode_Null),
                        .Subclass        = 0.0f,
                        .AmSubclass      = 0.0f
                    };
                }

                DeathStarMassSol = MassSol;
            }
        }

        float StarAge                 = 0.0f;
        float LogR                    = 0.0f;
        float LogTeff                 = 0.0f;
        float LogCenterT              = 0.0f;
        float LogCenterRho            = 0.0f;
        float SurfaceZ                = 0.0f;
        float SurfaceEnergeticNuclide = 0.0f;
        float SurfaceVolatiles        = 0.0f;

        switch (DeathStarType)
        {
        case Astro::EStellarType::kWhiteDwarf:
        {
            FStellarBasicProperties WhiteDwarfBasicProperties =
            {
                .Age            = static_cast<float>(DeathStarAge),
                .FeH            = 0.0f,
                .InitialMassSol = DeathStarMassSol
            };

            FDataArray WhiteDwarfData = GetFullMistData(WhiteDwarfBasicProperties, true, true).value();

            StarAge      = static_cast<float>(WhiteDwarfData[kWdStarAgeIndex_]);
            LogR         = static_cast<float>(WhiteDwarfData[kWdLogRIndex_]);
            LogTeff      = static_cast<float>(WhiteDwarfData[kWdLogTeffIndex_]);
            LogCenterT   = static_cast<float>(WhiteDwarfData[kWdLogCenterTIndex_]);
            LogCenterRho = static_cast<float>(WhiteDwarfData[kWdLogCenterRhoIndex_]);

            if (DeathStarMassSol < 0.2f || DeathStarMassSol > 1.3f)
            {
                LogR         = std::log10(0.0323f - 0.021384f * DeathStarMassSol);
                LogCenterT   = std::numeric_limits<float>::min();
                LogCenterRho = std::numeric_limits<float>::min();
            }

            if (DeathStarAge > StarAge)
            {
                LogTeff = static_cast<float>(
                    std::log10(std::pow(10.0, LogTeff) * std::pow((20.0 * StarAge) / (DeathStarAge + 19.0 * StarAge), 7.0 / 4.0)));
                LogCenterT = std::numeric_limits<float>::min();
            }

            SurfaceZ                = 0.0f;
            SurfaceEnergeticNuclide = 0.0f;
            SurfaceVolatiles        = 1.0f;

            break;
        }
        case Astro::EStellarType::kNeutronStar:
        {
            if (DeathStarAge < 1e5f)
            {
                DeathStarAge += 1e5f;
            }

            float Radius = 0.0f;
            if (DeathStarMassSol <= 0.77711f)
            {
                Radius = -4.783f + 2.565f / DeathStarMassSol + 42.0f * DeathStarMassSol - 55.4f * std::pow(DeathStarMassSol, 2.0f) +
                    34.93f * std::pow(DeathStarMassSol, 3.0f) - 8.4f * std::pow(DeathStarMassSol, 4.0f);
            }
            else if (DeathStarMassSol <= 2.0181f)
            {
                Radius = 11.302f - 0.35184f * DeathStarMassSol;
            }
            else
            {
                Radius = -31951.1f + 63121.8f * DeathStarMassSol - 46717.8f * std::pow(DeathStarMassSol, 2.0f) +
                    15358.4f * std::pow(DeathStarMassSol, 3.0f) - 1892.365f * std::pow(DeathStarMassSol, 4.0f);
            }

            LogR    = std::log10(Radius * 1000 / kSolarRadius);
            LogTeff = static_cast<float>(std::log10(1.5e8 * std::pow((DeathStarAge - 1e5) + 22000, -0.5)));

            SurfaceZ                = std::numeric_limits<float>::quiet_NaN();
            SurfaceEnergeticNuclide = std::numeric_limits<float>::quiet_NaN();
            SurfaceVolatiles        = std::numeric_limits<float>::quiet_NaN();

            break;
        }
        case Astro::EStellarType::kBlackHole:
        {
            LogR                    = std::numeric_limits<float>::quiet_NaN();
            LogTeff                 = std::numeric_limits<float>::quiet_NaN();
            LogCenterT              = std::numeric_limits<float>::quiet_NaN();
            LogCenterRho            = std::numeric_limits<float>::quiet_NaN();
            SurfaceZ                = std::numeric_limits<float>::quiet_NaN();
            SurfaceEnergeticNuclide = std::numeric_limits<float>::quiet_NaN();
            SurfaceVolatiles        = std::numeric_limits<float>::quiet_NaN();

            break;
        }
        default:
            break;
        }

        double EvolutionProgress = static_cast<double>(EvolutionPhase);
        double Age               = DeathStarAge;
        float  MassSol           = DeathStarMassSol;
        float  RadiusSol         = std::pow(10.0f, LogR);
        float  Teff              = std::pow(10.0f, LogTeff);
        float  CoreTemp          = std::pow(10.0f, LogCenterT);
        float  CoreDensity       = std::pow(10.0f, LogCenterRho);

        float LuminositySol  = std::pow(RadiusSol, 2.0f) * std::pow((Teff / kSolarTeff), 4.0f);
        float EscapeVelocity = std::sqrt((2.0f * kGravityConstant * MassSol * kSolarMass) / (RadiusSol * kSolarRadius));

        float Theta = CommonGenerator_(RandomEngine_) * 2.0f * Math::kPi;
        float Phi   = CommonGenerator_(RandomEngine_) * Math::kPi;

        DeathStar.SetAge(Age);
        DeathStar.SetMass(MassSol * kSolarMass);
        DeathStar.SetLifetime(-DeathStar.GetLifetime());
        DeathStar.SetEvolutionProgress(EvolutionProgress);
        DeathStar.SetRadius(RadiusSol * kSolarRadius);
        DeathStar.SetEscapeVelocity(EscapeVelocity);
        DeathStar.SetLuminosity(LuminositySol * kSolarLuminosity);
        DeathStar.SetTeff(Teff);
        DeathStar.SetSurfaceZ(SurfaceZ);
        DeathStar.SetSurfaceEnergeticNuclide(SurfaceEnergeticNuclide);
        DeathStar.SetSurfaceVolatiles(SurfaceVolatiles);
        DeathStar.SetCoreTemp(CoreTemp);
        DeathStar.SetCoreDensity(CoreDensity * 1000);
        DeathStar.SetEvolutionPhase(EvolutionPhase);
        DeathStar.SetNormal(glm::vec2(Theta, Phi));
        DeathStar.SetStarFrom(DeathStarFrom);
        DeathStar.SetStellarClass(Astro::FStellarClass(DeathStarType, DeathStarClass));

        CalculateSpectralType(0.0, DeathStar);
        GenerateMagnetic(DeathStar);
        GenerateSpin(DeathStar);
    }

    void FStellarGenerator::GenerateMagnetic(Astro::AStar& StarData)
    {
        Math::TDistribution<>* MagneticGenerator = nullptr;

        Astro::EStellarType StellarType = StarData.GetStellarClass().GetStellarType();
        float MassSol = static_cast<float>(StarData.GetMass() / kSolarMass);
        Astro::AStar::EEvolutionPhase EvolutionPhase = StarData.GetEvolutionPhase();

        float MagneticField = 0.0f;

        switch (StellarType)
        {
        case Astro::EStellarType::kNormalStar:
        {
            if (MassSol >= 0.075f && MassSol < 0.33f)
            {
                MagneticGenerator = &MagneticGenerators_[0];
            }
            else if (MassSol >= 0.33f && MassSol < 0.6f)
            {
                MagneticGenerator = &MagneticGenerators_[1];
            }
            else if (MassSol >= 0.6f && MassSol < 1.5f)
            {
                MagneticGenerator = &MagneticGenerators_[2];
            }
            else if (MassSol >= 1.5f && MassSol < 20.0f)
            {
                auto SpectralType = StarData.GetStellarClass().Data();
                if (EvolutionPhase == Astro::AStar::EEvolutionPhase::kMainSequence &&
                    (SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_A ||
                     SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_B))
                {
                    Math::TBernoulliDistribution ProbabilityGenerator(0.15); //  p 星的概率
                    if (ProbabilityGenerator(RandomEngine_))
                    {
                        MagneticGenerator = &MagneticGenerators_[3];
                        SpectralType.SpecialMark |= std::to_underlying(Astro::ESpecialMark::kCode_p);
                        StarData.SetStellarClass(Astro::FStellarClass(Astro::EStellarType::kNormalStar, SpectralType));
                    }
                    else
                    {
                        MagneticGenerator = &MagneticGenerators_[4];
                    }
                }
                else
                {
                    MagneticGenerator = &MagneticGenerators_[4];
                }
            }
            else
            {
                MagneticGenerator = &MagneticGenerators_[5];
            }

            MagneticField = std::pow(10.0f, (*MagneticGenerator)(RandomEngine_)) / 10000;

            break;
        }
        case Astro::EStellarType::kWhiteDwarf:
        {
            MagneticGenerator = &MagneticGenerators_[6];
            MagneticField = std::pow(10.0f, (*MagneticGenerator)(RandomEngine_));
            break;
        }
        case Astro::EStellarType::kNeutronStar:
        {
            MagneticGenerator = &MagneticGenerators_[7];
            MagneticField = (*MagneticGenerator)(RandomEngine_) /
                static_cast<float>(std::pow((0.034f * StarData.GetAge() / 1e4f), 1.17f) + 0.84f);
            break;
        }
        case Astro::EStellarType::kBlackHole:
        {
            MagneticField = 0.0f;
            break;
        }
        case Astro::EStellarType::kDeathStarPlaceholder:
        {
            break;
        }
        default:
            break;
        }

        StarData.SetMagneticField(MagneticField);
    }

    void FStellarGenerator::GenerateSpin(Astro::AStar& StarData)
    {
        Astro::EStellarType StellarType = StarData.GetStellarClass().GetStellarType();
        float StarAge   = static_cast<float>(StarData.GetAge());
        float MassSol   = static_cast<float>(StarData.GetMass() / kSolarMass);
        float RadiusSol = StarData.GetRadius() / kSolarRadius;
        float Spin      = 0.0f;

        Math::TDistribution<float>* SpinGenerator = nullptr;

        switch (StellarType)
        {
        case Astro::EStellarType::kNormalStar:
        {
            float Base = 1.0f + CommonGenerator_(RandomEngine_);
            if (StarData.GetStellarClass().Data().SpecialMark & std::to_underlying(Astro::ESpecialMark::kCode_p))
            {
                Base *= 10;
            }

            float LogMass = std::log10(MassSol);
            float Term1   = 0.0f;
            float Term2   = 0.0f;
            float Term3   = std::pow(2.0f, std::sqrt(Base * (StarAge + 1e6f) * 1e-9f));

            if (MassSol <= 1.4f)
            {
                Term1 = std::pow(10.0f, 30.893f - 25.34303f * std::exp(LogMass) + 21.7577f * LogMass +
                                 7.34205f * std::pow(LogMass, 2.0f) + 0.12951f * std::pow(LogMass, 3.0f));
                Term2 = std::pow(RadiusSol / std::pow(MassSol, 0.9f), 2.5f);
            }
            else
            {
                Term1 = std::pow(10.0f, 28.0784f - 22.15753f * std::exp(LogMass) + 12.55134f * LogMass +
                                 30.9045f * std::pow(LogMass, 2.0f) - 10.1479f * std::pow(LogMass, 3.0f) +
                                 4.6894f  * std::pow(LogMass, 4.0f));
                Term2 = std::pow(RadiusSol / (1.1062f * std::pow(MassSol, 0.6f)), 2.5f);
            }

            Spin = Term1 * Term2 * Term3;

            break;
        }
        case Astro::EStellarType::kWhiteDwarf:
        {
            SpinGenerator = &SpinGenerators_[0];
            Spin = std::pow(10.0f, (*SpinGenerator)(RandomEngine_));
            break;
        }
        case Astro::EStellarType::kNeutronStar:
        {
            Spin = (StarAge * 3 * 1e-9f) + 1e-3f;
            break;
        }
        case Astro::EStellarType::kBlackHole: // 此处表示无量纲自旋参数，而非自转时间
        {
            SpinGenerator = &SpinGenerators_[1];
            Spin = (*SpinGenerator)(RandomEngine_);
            break;
        }
        default:
            break;
        }

        if (StellarType != Astro::EStellarType::kBlackHole)
        {
            float Oblateness = 4.0f * std::pow(Math::kPi, 2.0f) * std::pow(StarData.GetRadius(), 3.0f);
            Oblateness /= (std::pow(Spin, 2.0f) * kGravityConstant * static_cast<float>(StarData.GetMass()));
            StarData.SetOblateness(Oblateness);
        }

        StarData.SetSpin(Spin);
    }

    void FStellarGenerator::ExpandMistData(double TargetMassSol, FDataArray& StarData)
    {
        double RadiusSol     = std::pow(10.0, StarData[kLogRIndex_]);
        double Teff          = std::pow(10.0, StarData[kLogTeffIndex_]);
        double LuminositySol = std::pow(RadiusSol, 2.0) * std::pow((Teff / kSolarTeff), 4.0);

        double& StarMass = StarData[kStarMassIndex_];
        double& StarMdot = StarData[kStarMdotIndex_];
        double& LogR     = StarData[kLogRIndex_];
        double& LogTeff  = StarData[kLogTeffIndex_];

        double LogL = std::log10(LuminositySol);

        StarMass = TargetMassSol * (StarMass / 0.1);
        StarMdot = TargetMassSol * (StarMdot / 0.1);

        RadiusSol     = std::pow(10.0, LogR) * std::pow(TargetMassSol / 0.1, 2.3);
        LuminositySol = std::pow(10.0, LogL) * std::pow(TargetMassSol / 0.1, 2.3);

        Teff    = kSolarTeff * std::pow((LuminositySol / std::pow(RadiusSol, 2.0)), 0.25);
        LogTeff = std::log10(Teff);
        LogR    = std::log10(RadiusSol);
    }

    std::unordered_map<std::string, std::vector<float>> FStellarGenerator::MassFilesCache_;
    std::unordered_map<const FStellarGenerator::FMistData*, std::vector<FStellarGenerator::FDataArray>> FStellarGenerator::PhaseChangesCache_;
    std::shared_mutex FStellarGenerator::CacheMutex_;
    bool FStellarGenerator::bMistDataInitiated_ = false;
} // namespace Npgs
