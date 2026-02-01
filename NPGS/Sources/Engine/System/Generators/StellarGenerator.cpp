#include "stdafx.h"
#include "StellarGenerator.hpp"

#include <cmath>
#include <algorithm>
#include <charconv>
#include <exception>
#include <filesystem>
#include <format>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#define NPGS_ENABLE_ENUM_BIT_OPERATOR
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
        struct FDeathStarPayload
        {
            double Lifetime{};
        };

        void GenerateDeathStarPlaceholder(double Lifetime)
        {
            FDeathStarPayload DeathStar;
            DeathStar.Lifetime = Lifetime;

            throw DeathStar; // not error
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
    const std::array<std::string, 12> FStellarGenerator::kMistHeaders_
    {
        "star_age", "star_mass", "star_mdot", "log_Teff", "log_R", "log_surf_z",
        "surface_h1", "surface_he3", "log_center_T", "log_center_Rho", "phase", "x"
    };

    const std::array<std::string, 5> FStellarGenerator::kWdMistHeaders_
    {
        "star_age", "log_R", "log_Teff", "log_center_T", "log_center_Rho"
    };

    FStellarGenerator::FStellarGenerator(const FStellarGenerationInfo& GenerationInfo)
        :
        RandomEngine_(*GenerationInfo.SeedSequence),
        MagneticGenerators_
        {
            Math::TUniformRealDistribution<>(std::log10(500.0f),  std::log10(3000.0f)),  // 500-3000G, 极低质量 M 型星
            Math::TUniformRealDistribution<>(std::log10(10.0f),   std::log10(1000.0f)),  // 10-1000G, 晚期 K/M 型星
            Math::TUniformRealDistribution<>(std::log10(1.0f),    std::log10(10.0f)),    // 1-10G, 太阳类恒星
            Math::TUniformRealDistribution<>(std::log10(300.0f),  std::log10(10000.0f)), // 300-10000G, 磁场特殊星（Ap/Bp/Op）
            Math::TUniformRealDistribution<>(std::log10(0.1f),    std::log10(1.0f)),     // 0.1-1G, 普通大质量恒星（A/B/O）
            Math::TUniformRealDistribution<>(std::log10(1e4f),    std::log10(3e4f)),     // 10kG-30kG, 超级 Op（原神怎么你了）
            Math::TUniformRealDistribution<>(std::log10(1e3f),    std::log10(1e5f)),     // 白矮星，不包括磁白矮星
            Math::TUniformRealDistribution<>(1e9f, 1e12f),                               // 中子星，不包括磁星。并且是线性分布
            Math::TUniformRealDistribution<>(std::log10(1e6f),    std::log10(1e9f)),     // 磁白矮星，1e6-1e9G
            Math::TUniformRealDistribution<>(std::log10(1e13f),   std::log10(1e15f))     // 磁星，1e9-1e11T
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
            std::make_unique<Math::TNormalDistribution<>>(0.6f, 0.2f),   // mean=205km/s, sigma=190km/s
            std::make_unique<Math::TGammaDistribution<>>(4.82f, 0.037f), // alpha=4.82, beta=1/25
            std::make_unique<Math::TUniformRealDistribution<>>(3.0f, 5.0f),
            std::make_unique<Math::TUniformRealDistribution<>>(5e-3f, 1.0f),
            std::make_unique<Math::TUniformRealDistribution<>>(0.001f, 0.998f)
        },

        XpStarProbabilityGenerators_
        {
            Math::TBernoulliDistribution<>(0.15), // 常规 Ap/Bp/Op
            Math::TBernoulliDistribution<>(0.07)  // MeMiS 0.07 Op
        },

        AgeGenerator_(GenerationInfo.AgeLowerLimit, GenerationInfo.AgeUpperLimit),
        CommonGenerator_(0.0f, 1.0f),
        FastMassiveStarProbabilityGenerator_(0.25),

        LogMassGenerator_(GenerationInfo.StellarTypeOption == EStellarTypeGenerationOption::kMergeStar
                          ? std::make_unique<Math::TUniformRealDistribution<>>(0.0f, 1.0f)
                          : std::make_unique<Math::TUniformRealDistribution<>>(
                              std::log10(GenerationInfo.MassLowerLimit), std::log10(GenerationInfo.MassUpperLimit))),

        MassPdfs_(GenerationInfo.MassPdfs),
        MassMaxPdfs_(GenerationInfo.MassMaxPdfs),
        AgeMaxPdf_(GenerationInfo.AgeMaxPdf),
        AgePdf_(GenerationInfo.AgePdf),
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
        , XpStarProbabilityGenerators_(Other.XpStarProbabilityGenerators_)
        , AgeGenerator_(Other.AgeGenerator_)
        , CommonGenerator_(Other.CommonGenerator_)
        , FastMassiveStarProbabilityGenerator_(Other.FastMassiveStarProbabilityGenerator_)
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
            LogMassGenerator_ = Other.LogMassGenerator_->Clone();
        }

        for (std::size_t i = 0; i != Other.SpinGenerators_.size(); ++i)
        {
            const auto& OtherGenerator = Other.SpinGenerators_[i];
            if (OtherGenerator != nullptr)
            {
                SpinGenerators_[i] = OtherGenerator->Clone();
            }
        }

        for (std::size_t i = 0; i != Other.FeHGenerators_.size(); ++i)
        {
            const auto& OtherGenerator = Other.FeHGenerators_[i];
            if (OtherGenerator != nullptr)
            {
                FeHGenerators_[i] = OtherGenerator->Clone();
            }
        }
    }

    FStellarGenerator::FStellarGenerator(FStellarGenerator&& Other) noexcept
        : RandomEngine_(std::move(Other.RandomEngine_))
        , MagneticGenerators_(std::move(Other.MagneticGenerators_))
        , FeHGenerators_(std::move(Other.FeHGenerators_))
        , SpinGenerators_(std::move(Other.SpinGenerators_))
        , XpStarProbabilityGenerators_(std::move(Other.XpStarProbabilityGenerators_))
        , AgeGenerator_(std::move(Other.AgeGenerator_))
        , CommonGenerator_(std::move(Other.CommonGenerator_))
        , FastMassiveStarProbabilityGenerator_(std::move(Other.FastMassiveStarProbabilityGenerator_))
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
            RandomEngine_                        = Other.RandomEngine_;
            MagneticGenerators_                  = Other.MagneticGenerators_;
            XpStarProbabilityGenerators_         = Other.XpStarProbabilityGenerators_;
            AgeGenerator_                        = Other.AgeGenerator_;
            CommonGenerator_                     = Other.CommonGenerator_;
            FastMassiveStarProbabilityGenerator_ = Other.FastMassiveStarProbabilityGenerator_;
            MassPdfs_                            = Other.MassPdfs_;
            MassMaxPdfs_                         = Other.MassMaxPdfs_;
            AgeMaxPdf_                           = Other.AgeMaxPdf_;
            AgePdf_                              = Other.AgePdf_;
            UniverseAge_                         = Other.UniverseAge_;
            AgeLowerLimit_                       = Other.AgeLowerLimit_;
            AgeUpperLimit_                       = Other.AgeUpperLimit_;
            FeHLowerLimit_                       = Other.FeHLowerLimit_;
            FeHUpperLimit_                       = Other.FeHUpperLimit_;
            MassLowerLimit_                      = Other.MassLowerLimit_;
            MassUpperLimit_                      = Other.MassUpperLimit_;
            CoilTemperatureLimit_                = Other.CoilTemperatureLimit_;
            dEpdM_                               = Other.dEpdM_;
            AgeDistribution_                     = Other.AgeDistribution_;
            FeHDistribution_                     = Other.FeHDistribution_;
            MassDistribution_                    = Other.MassDistribution_;
            StellarTypeOption_                   = Other.StellarTypeOption_;
            MultiplicityOption_                  = Other.MultiplicityOption_;

            LogMassGenerator_ = Other.LogMassGenerator_->Clone();

            for (std::size_t i = 0; i != Other.SpinGenerators_.size(); ++i)
            {
                const auto& OtherGenerator = Other.SpinGenerators_[i];
                if (OtherGenerator != nullptr)
                {
                    SpinGenerators_[i] = OtherGenerator->Clone();
                }
            }

            for (std::size_t i = 0; i != Other.FeHGenerators_.size(); ++i)
            {
                const auto& OtherGenerator = Other.FeHGenerators_[i];
                if (OtherGenerator != nullptr)
                {
                    FeHGenerators_[i] = OtherGenerator->Clone();
                }
            }
        }

        return *this;
    }

    FStellarGenerator& FStellarGenerator::operator=(FStellarGenerator&& Other) noexcept
    {
        if (this != &Other)
        {
            RandomEngine_                        = std::move(Other.RandomEngine_);
            MagneticGenerators_                  = std::move(Other.MagneticGenerators_);
            FeHGenerators_                       = std::move(Other.FeHGenerators_);
            SpinGenerators_                      = std::move(Other.SpinGenerators_);
            XpStarProbabilityGenerators_         = std::move(Other.XpStarProbabilityGenerators_);
            AgeGenerator_                        = std::move(Other.AgeGenerator_);
            CommonGenerator_                     = std::move(Other.CommonGenerator_);
            FastMassiveStarProbabilityGenerator_ = std::move(Other.FastMassiveStarProbabilityGenerator_);
            LogMassGenerator_                    = std::move(Other.LogMassGenerator_);
            MassPdfs_                            = std::move(Other.MassPdfs_);
            MassMaxPdfs_                         = std::move(Other.MassMaxPdfs_);
            AgeMaxPdf_                           = std::move(Other.AgeMaxPdf_);
            AgePdf_                              = std::move(Other.AgePdf_);
            UniverseAge_                         = std::exchange(Other.UniverseAge_, 0.0f);
            AgeLowerLimit_                       = std::exchange(Other.AgeLowerLimit_, 0.0f);
            AgeUpperLimit_                       = std::exchange(Other.AgeUpperLimit_, 0.0f);
            FeHLowerLimit_                       = std::exchange(Other.FeHLowerLimit_, 0.0f);
            FeHUpperLimit_                       = std::exchange(Other.FeHUpperLimit_, 0.0f);
            MassLowerLimit_                      = std::exchange(Other.MassLowerLimit_, 0.0f);
            MassUpperLimit_                      = std::exchange(Other.MassUpperLimit_, 0.0f);
            CoilTemperatureLimit_                = std::exchange(Other.CoilTemperatureLimit_, 0.0f);
            dEpdM_                               = std::exchange(Other.dEpdM_, 0.0f);
            AgeDistribution_                     = std::exchange(Other.AgeDistribution_, {});
            FeHDistribution_                     = std::exchange(Other.FeHDistribution_, {});
            MassDistribution_                    = std::exchange(Other.MassDistribution_, {});
            StellarTypeOption_                   = std::exchange(Other.StellarTypeOption_, {});
            MultiplicityOption_                  = std::exchange(Other.MultiplicityOption_, {});
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
            case EGenerationDistribution::kUniform:
                Properties.Age = AgeLowerLimit_ + CommonGenerator_(RandomEngine_) * (AgeUpperLimit_ - AgeLowerLimit_);
                break;
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
            case EGenerationDistribution::kFromPdf:
            {
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
            case EGenerationDistribution::kUniform:
                Properties.InitialMassSol = MassLowerLimit_ + CommonGenerator_(RandomEngine_) * (MassUpperLimit_ - MassLowerLimit_);
                break;
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

        return GenerateStarInternal(Properties, nullptr);
    }

    template <typename CsvType>
    requires std::is_class_v<CsvType>
    TAssetHandle<CsvType> FStellarGenerator::LoadCsvAsset(const std::string& Filename, std::span<const std::string> Headers) const
    {
        auto* AssetManager = EngineCoreServices->GetAssetManager();
        auto  Asset = AssetManager->AcquireAsset<CsvType>(Filename);
        if (Asset)
        {
            return Asset;
        }

        AssetManager->AddAsset<CsvType>(Filename, CsvType(Filename, Headers));
        return AssetManager->AcquireAsset<CsvType>(Filename);
    }

    void FStellarGenerator::InitializeMistData() const
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

    Astro::AStar FStellarGenerator::GenerateStarInternal(const FStellarBasicProperties& Properties, Astro::AStar* ZamsStarData)
    {
        std::optional<Astro::AStar> LocalZamsStar;
        Astro::AStar* ZamsStar = nullptr;
        if (ZamsStarData == nullptr)
        {
            auto ZamsData = CalculateZamsStarData(Properties.InitialMassSol, Properties.FeH);
            LocalZamsStar.emplace(MakeNormalStar(ZamsData, Properties));
            CalculateSpectralType(*LocalZamsStar);
            GenerateZamsStarMagnetic(*LocalZamsStar);
            GenerateZamsStarInitialSpin(*LocalZamsStar);
            ZamsStar = &LocalZamsStar.value();
        }
        else
        {
            ZamsStar = ZamsStarData;
        }

        FDataArray StarData;

        switch (Properties.StellarTypeOption)
        {
        case EStellarTypeGenerationOption::kRandom:
            try
            {
                StarData = GetFullMistData(Properties, false, true);
            }
            catch (FDeathStarPayload DeathStarPayload)
            {
                ZamsStar->SetLifetime(DeathStarPayload.Lifetime);
                auto DeathStar = GenerateDeathStar(EStellarTypeGenerationOption::kRandom, Properties, *ZamsStar);
                if (DeathStar.GetEvolutionPhase() == Astro::AStar::EEvolutionPhase::kNull)
                {
                    // 如果爆了，削一半质量
                    auto NewProperties = Properties;
                    NewProperties.InitialMassSol /= 2;
                    DeathStar = GenerateStarInternal(NewProperties, nullptr);
                }

                return DeathStar;
            }
            catch (const std::exception& e)
            {
                NpgsCoreError("Failed to generate star at Age={}, FeH={}, InMass={}: {}",
                              Properties.Age, Properties.FeH, Properties.InitialMassSol, e.what());
                return {};
            }

            break;
        case EStellarTypeGenerationOption::kGiant:
        {
            auto NewProperties = Properties;
            NewProperties.Age = std::numeric_limits<float>::quiet_NaN(); // 使用 NaN，在计算年龄的时候根据寿命赋值一个濒死年龄
            try
            {
                StarData = GetFullMistData(NewProperties, false, true);
            }
            catch (const std::exception& e)
            {
                NpgsCoreError("Failed to generate giant star at Age={}, FeH={}, InMass={}: {}",
                              NewProperties.Age, NewProperties.FeH, NewProperties.InitialMassSol, e.what());
                return {};
            }

            break;
        }
        case EStellarTypeGenerationOption::kDeathStar:
        {
            auto DeathStar = GenerateDeathStar(EStellarTypeGenerationOption::kDeathStar, Properties, *ZamsStar);
            if (DeathStar.GetEvolutionPhase() == Astro::AStar::EEvolutionPhase::kNull)
            {
                FStellarBasicProperties NewProperties
                {
                    .Age            = static_cast<float>(DeathStar.GetAge()),
                    .FeH            = DeathStar.GetFeH(),
                    .InitialMassSol = DeathStar.GetInitialMass() / kSolarMass / 2.0f
                };

                // 如果爆了，削一半质量
                DeathStar = GenerateStarInternal(NewProperties, nullptr);
            }

            return DeathStar;
        }
        case EStellarTypeGenerationOption::kMergeStar:
            return GenerateDeathStar(EStellarTypeGenerationOption::kMergeStar, Properties, *ZamsStar);
        default:
            break;
        }

        if (StarData.empty())
        {
            return {};
        }

        auto Star = MakeNormalStar(StarData, Properties);
        CalculateSpectralType(Star);
        CalculateMinCoilMass(Star);
        ApplyFromZams(Star, *ZamsStar);

        return Star;
    }

    void FStellarGenerator::ApplyFromZams(Astro::AStar& StarData, const Astro::AStar& ZamsStarData)
    {
        StarData.SetMagneticField(ZamsStarData.GetMagneticField());
        CalculateCurrentSpinAndOblateness(StarData, ZamsStarData);
    }

    FStellarGenerator::FDataArray
    FStellarGenerator::GetFullMistData(const FStellarBasicProperties& Properties, bool bIsWhiteDwarf, bool bIsSingleWhiteDwarf) const
    {
        float TargetAge     = Properties.Age;
        float TargetFeH     = Properties.FeH;
        float TargetMassSol = Properties.InitialMassSol;

        auto DataTables = GetDataTables(TargetMassSol, TargetFeH, bIsWhiteDwarf, bIsSingleWhiteDwarf);
        auto Files      = std::make_pair(DataTables.LowerMassFilename, DataTables.UpperMassFilename);

        float MassCoefficient = (TargetMassSol - DataTables.LowerMass) / (DataTables.UpperMass - DataTables.LowerMass);
        return InterpolateMistData(Files, TargetAge, TargetMassSol, MassCoefficient);
    }

    FStellarGenerator::FDataFileTables
    FStellarGenerator::GetDataTables(float MassSol, float FeH, bool bIsWhiteDwarf, bool bIsSingleWhiteDwarf) const
    {
        std::string PrefixDirectory;

        if (!bIsWhiteDwarf)
        {
            const std::array kPresetFeH{ -4.0f, -3.0f, -2.0f, -1.5f, -1.0f, -0.5f, 0.0f, 0.5f };

            float ClosestFeH = GetClosestFeH(FeH);
            PrefixDirectory = std::format("{:.1f}", ClosestFeH);
            if (ClosestFeH >= 0.0f)
            {
                PrefixDirectory.insert(PrefixDirectory.begin(), '+');
            }

            PrefixDirectory.insert(0, GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/[Fe_H]="));
        }
        else
        {
            PrefixDirectory = bIsSingleWhiteDwarf
                ? GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/WhiteDwarfs/Thin")
                : GetAssetFullPath(EAssetType::kDataTable, "StellarParameters/MIST/WhiteDwarfs/Thick");
        }

        std::vector<float> Masses;
        {
            std::shared_lock Lock(FileCacheMutex_);
            Masses = MassFilesCache_[PrefixDirectory];
        }

        auto it = std::ranges::lower_bound(Masses, MassSol);
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

        if (*it == MassSol)
        {
            LowerMass = UpperMass = *it;
        }
        else
        {
            LowerMass = it == Masses.begin() ? *it : *(it - 1);
            UpperMass = *it;
        }

        std::string LowerMassFile = PrefixDirectory + "/" + std::format("{:06.2f}0", LowerMass) + "Ms_track.csv";
        std::string UpperMassFile = PrefixDirectory + "/" + std::format("{:06.2f}0", UpperMass) + "Ms_track.csv";

        return {
            .LowerMassFilename = std::move(LowerMassFile),
            .UpperMassFilename = std::move(UpperMassFile),
            .LowerMass         = LowerMass,
            .UpperMass         = UpperMass
        };
    }

    float FStellarGenerator::GetClosestFeH(float FeH) const
    {
        static constexpr std::array kPresetFeH{ -4.0f, -3.0f, -2.0f, -1.5f, -1.0f, -0.5f, 0.0f, 0.5f };

        float ClosestFeH = *std::ranges::min_element(kPresetFeH, [FeH](float Lhs, float Rhs) -> bool
        {
            return std::abs(Lhs - FeH) < std::abs(Rhs - FeH);
        });

        return ClosestFeH;
    }

    FStellarGenerator::FDataArray
    FStellarGenerator::InterpolateMistData(const std::pair<std::string, std::string>& Files,
                                           double TargetAge, double TargetMassSol, double MassCoefficient) const
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

                double EvolutionProgress = CalculateEvolutionProgress(PhaseChangePair, TargetAge, MassCoefficient);

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
                    EvolutionProgress = CalculateEvolutionProgress(PhaseChangePair, TargetAge, MassCoefficient);

                    Lifetime = PhaseChanges.back()[kStarAgeIndex_];
                    Result   = InterpolateStarData(StarData.Get(), EvolutionProgress);
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
                        GenerateDeathStarPlaceholder(Lifetime);
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

    std::vector<FStellarGenerator::FDataArray> FStellarGenerator::FindPhaseChanges(const FMistData* DataSheet) const
    {
        {
            std::shared_lock Lock(PhaseChangeCacheMutex_);
            if (PhaseChangesCache_.contains(DataSheet))
            {
                return PhaseChangesCache_[DataSheet];
            }
        }

        const auto* const CsvData = DataSheet->Data();
        int CurrentPhase = -2;
        std::vector<FDataArray> Result;
        for (const auto& Row : *CsvData)
        {
            if (Row[kPhaseIndex_] != CurrentPhase || Row[kXIndex_] == 10.0)
            {
                CurrentPhase = static_cast<int>(Row[kPhaseIndex_]);
                Result.push_back(Row);
            }
        }

        {
            std::unique_lock Lock(PhaseChangeCacheMutex_);
            if (!PhaseChangesCache_.contains(DataSheet))
            {
                PhaseChangesCache_.emplace(DataSheet, Result);
            }
        }

        return Result;
    }

    double FStellarGenerator::CalculateEvolutionProgress(std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& PhaseChanges,
                                                         double TargetAge, double MassCoefficient) const
    {
        double Result = 0.0;
        double Phase  = 0.0;

        if (PhaseChanges.second.empty()) [[unlikely]]
        {
            auto [PhaseResult, LowerTimePoint, UpperTimePoint] = FindSurroundingTimePoints(PhaseChanges.first, TargetAge);
            Phase = PhaseResult;
            if (TargetAge > UpperTimePoint)
            {
                GenerateDeathStarPlaceholder(UpperTimePoint);
            }

            Result = (TargetAge - LowerTimePoint) / (UpperTimePoint - LowerTimePoint) + Phase;
        }
        else [[likely]]
        {
            if (PhaseChanges.first.size() == PhaseChanges.second.size() &&
                (*std::prev(PhaseChanges.first.end(), 2))[kPhaseIndex_] == (*std::prev(PhaseChanges.second.end(), 2))[kPhaseIndex_])
            {
                auto TimePointResults = FindSurroundingTimePoints(PhaseChanges, TargetAge, MassCoefficient);

                Phase = TimePointResults.Phase;
                std::size_t Index = TimePointResults.TableIndex;

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
                Result = CalculateEvolutionProgress(PhaseChanges, TargetAge, MassCoefficient);

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

    FStellarGenerator::FSurroundingTimePoints
    FStellarGenerator::FindSurroundingTimePoints(const std::vector<FDataArray>& PhaseChanges, double TargetAge) const
    {
        std::vector<FDataArray>::const_iterator LowerTimePoint;
        std::vector<FDataArray>::const_iterator UpperTimePoint;

        if (PhaseChanges.size() != 2 || PhaseChanges.front()[kPhaseIndex_] != PhaseChanges.back()[kPhaseIndex_])
        {
            LowerTimePoint = std::ranges::lower_bound(PhaseChanges, TargetAge, {}, [](const FDataArray& Array) -> double
            {
                return Array[0];
            });

            UpperTimePoint = std::ranges::upper_bound(PhaseChanges, TargetAge, {}, [](const FDataArray& Array) -> double
            {
                return Array[0];
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

        return {
            .Phase          = (*LowerTimePoint)[kXIndex_],
            .LowerTimePoint = (*LowerTimePoint)[kStarAgeIndex_],
            .UpperTimePoint = (*UpperTimePoint)[kStarAgeIndex_]
        };
    }

    FStellarGenerator::FExactChangedTimePoints
    FStellarGenerator::FindSurroundingTimePoints(const std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& PhaseChanges,
                                                 double TargetAge, double MassCoefficient) const
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
            double Lifetime = PhaseChangeTimePoints.back();
            GenerateDeathStarPlaceholder(Lifetime);
        }

        std::vector<std::pair<double, double>> TimePointPairs; // [Phase, ChangedTimePointWithThisPhase]
        for (std::size_t i = 0; i != PhaseChanges.first.size(); ++i)
        {
            TimePointPairs.emplace_back(PhaseChanges.first[i][kPhaseIndex_], PhaseChangeTimePoints[i]);
        }

        FExactChangedTimePoints Result;
        for (std::size_t i = 0; i != TimePointPairs.size(); ++i)
        {
            if (TimePointPairs[i].second >= TargetAge)
            {
                std::size_t Index = i == 0 ? 0 : i - 1;
                Result.Phase      = TimePointPairs[Index].first;
                Result.TableIndex = Index;
                break;
            }
        }

        return Result;
    }

    void FStellarGenerator::AlignArrays(std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& Arrays) const
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

    FStellarGenerator::FDataArray
    FStellarGenerator::InterpolateStarData(FStellarGenerator::FMistData* Data, double EvolutionProgress) const
    {
        return InterpolateStarData(Data, EvolutionProgress, "x", FStellarGenerator::kXIndex_, false);
    }

    FStellarGenerator::FDataArray FStellarGenerator::InterpolateStarData(FStellarGenerator::FWdMistData* Data, double TargetAge) const
    {
        return InterpolateStarData(Data, TargetAge, "star_age", FStellarGenerator::kWdStarAgeIndex_, true);
    }

    FStellarGenerator::FDataArray
    FStellarGenerator::InterpolateStarData(auto* Data, double Target, const std::string& Header, int Index, bool bIsWhiteDwarf) const
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
    FStellarGenerator::InterpolateArray(const std::pair<FDataArray, FDataArray>& DataArrays, double Coefficient) const
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
    FStellarGenerator::InterpolateFinalData(const std::pair<FDataArray, FDataArray>& DataArrays, double Coefficient, bool bIsWhiteDwarf) const
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

    Astro::AStar FStellarGenerator::MakeNormalStar(const FDataArray& StarData, const FStellarBasicProperties& Properties)
    {
        Astro::AStar Star(Properties);

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
        auto   EvolutionPhase    = static_cast<Astro::AStar::EEvolutionPhase>(StarData[kPhaseIndex_]);

        float LuminositySol  = std::pow(RadiusSol, 2.0f) * std::pow((Teff / kSolarTeff), 4.0f);
        float EscapeVelocity = std::sqrt((2.0f * kGravityConstant * MassSol * kSolarMass) / (RadiusSol * kSolarRadius));

        float LifeProgress         = static_cast<float>(Age / Lifetime);
        float WindSpeedCoefficient = 3.0f - LifeProgress;
        float StellarWindSpeed     = WindSpeedCoefficient * EscapeVelocity;

        float SurfaceEnergeticNuclide = (SurfaceH1 * 0.00002f + SurfaceHe3);
        float SurfaceVolatiles = 1.0f - SurfaceZ - SurfaceEnergeticNuclide;

        float Theta = CommonGenerator_(RandomEngine_) * 2.0f * Math::kPi;
        float Phi   = CommonGenerator_(RandomEngine_) * Math::kPi;

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

        return Star;
    }

    Astro::AStar FStellarGenerator::GenerateDeathStar(EStellarTypeGenerationOption DeathStarTypeOption,
                                                      const FStellarBasicProperties& Properties, Astro::AStar& ZamsStarData)
    {
        float InputAge     = Properties.Age;
        float InputFeH     = Properties.FeH;
        float InputMassSol = static_cast<float>(ZamsStarData.GetInitialMass() / kSolarMass);
        float DeathStarAge = InputAge - static_cast<float>(ZamsStarData.GetLifetime());

        if (DeathStarTypeOption == EStellarTypeGenerationOption::kMergeStar)
        {
            auto BasicProperties = GenerateMergeStar(Properties);
            auto DeathStar       = CalculateAndMakeDeathStar(ZamsStarData, BasicProperties, DeathStarAge);
            GenerateCompatStarMagnetic(DeathStar);
            GenerateCompatStarSpin(DeathStar);
            return DeathStar;
        }

        FStellarBasicProperties DyingStarProperties
        {
            .StellarTypeOption = EStellarTypeGenerationOption::kRandom,
            .Age               = static_cast<float>(ZamsStarData.GetLifetime()) - (InputMassSol > 10.0f ? 100.0f : 1e4f), // 避免浮点数精度问题
            .FeH               = InputFeH,
            .InitialMassSol    = InputMassSol
        };

        auto  DyingStar                   = GenerateStarInternal(DyingStarProperties, &ZamsStarData);
        float CoreSpecificAngularMomentum = CalculateCoreSpecificAngularMomentum(DyingStar);
        auto  RemainsBasicProperties      = DetermineRemainsProperties(DyingStar, InputMassSol, InputFeH, CoreSpecificAngularMomentum);

        auto DeathStar = CalculateAndMakeDeathStar(DyingStar, RemainsBasicProperties, DeathStarAge);

        if (DeathStar.GetStellarClass().GetStellarType() != Astro::EStellarType::kBlackHole)
        {
            CalculateExtremeProperties(DeathStar, ZamsStarData, CoreSpecificAngularMomentum);
            if (DeathStar.GetMagneticField() >= 1e9f)
            {
                DeathStar.SetEvolutionPhase(Astro::AStar::EEvolutionPhase::kMagnetar);
            }
        }

        return DeathStar;
    }

    FStellarGenerator::FDataArray FStellarGenerator::FindZamsData(const FMistData* DataSheet) const
    {
        {
            std::shared_lock Lock(ZamsDataMutex_);
            if (ZamsDataCache_.contains(DataSheet))
            {
                return ZamsDataCache_[DataSheet];
            }
        }

        const auto* const CsvData = DataSheet->Data();
        float MinLogRadius = std::numeric_limits<float>::max();
        FDataArray Result;
        for (const auto& Row : *CsvData)
        {
            if (Row[kPhaseIndex_] < 0)
            {
                continue;
            }

            if (Row[kLogRIndex_] < MinLogRadius)
            {
                MinLogRadius = static_cast<float>(Row[kLogRIndex_]);
                Result = Row;
                continue;
            }

            if (Row[kLogRIndex_] > MinLogRadius || Row[kXIndex_] > 0.2)
            {
                break;
            }
        }

        {
            std::unique_lock Lock(ZamsDataMutex_);
            if (!ZamsDataCache_.contains(DataSheet))
            {
                ZamsDataCache_.emplace(DataSheet, Result);
            }
        }

        return Result;
    }

    FStellarGenerator::FDataArray FStellarGenerator::CalculateZamsStarData(float InitialMassSol, float FeH) const
    {
        auto DataTables = GetDataTables(InitialMassSol, FeH, false, false);
        FDataArray ZamsData;

        if (DataTables.LowerMassFilename != DataTables.UpperMassFilename) [[likely]]
        {
            auto LowerData     = LoadCsvAsset<FMistData>(DataTables.LowerMassFilename, kMistHeaders_);
            auto UpperData     = LoadCsvAsset<FMistData>(DataTables.UpperMassFilename, kMistHeaders_);
            auto LowerZamsData = FindZamsData(LowerData.Get());
            auto UpperZamsData = FindZamsData(UpperData.Get());
            auto LowerLifetime = LowerData->Data()->back()[kStarAgeIndex_];
            auto UpperLifetime = UpperData->Data()->back()[kStarAgeIndex_];

            LowerZamsData.push_back(LowerLifetime);
            UpperZamsData.push_back(UpperLifetime);

            float MassCoefficient = (InitialMassSol - DataTables.LowerMass) / (DataTables.UpperMass - DataTables.LowerMass);
            ZamsData = InterpolateArray({ LowerZamsData, UpperZamsData }, MassCoefficient);
        }
        else [[unlikely]]
        {
            auto Data = LoadCsvAsset<FMistData>(DataTables.LowerMassFilename, kMistHeaders_);
            auto Lifetime = Data->Data()->back()[kStarAgeIndex_];
            ZamsData = FindZamsData(Data.Get());
            ZamsData.push_back(Lifetime);
        }

        return ZamsData;
    }

    void FStellarGenerator::CalculateSpectralType(Astro::AStar& StarData) const
    {
        float Teff           = StarData.GetTeff();
        float FeH            = StarData.GetFeH();
        auto  EvolutionPhase = StarData.GetEvolutionPhase();

        Astro::EStellarType  StellarType = StarData.GetStellarClass().GetStellarType();
        Astro::FSpectralType SpectralType;
        SpectralType.bIsAmStar = false;

        std::vector<std::pair<int, int>> SpectralSubclassMap;
        float InitialMassSol = StarData.GetInitialMass() / kSolarMass;
        float Subclass       = 0.0f;
        float SurfaceH1      = StarData.GetSurfaceH1();
        float SurfaceZ       = StarData.GetSurfaceZ();
        float MinSurfaceH1   = Astro::AStar::kFeHSurfaceH1Map_.at(GetClosestFeH(FeH)) - 0.01f;

        auto CalculateMassThreshold = [](float FeH, float BaseMass, float Exponent, float MinMassSol) -> float
        {
            float FeHRatio  = std::pow(10.0f, FeH);
            float Threshold = BaseMass * std::pow(FeHRatio, Exponent);

            return std::clamp(Threshold, MinMassSol, 300.0f);
        };

        float WNxhLowerMassThreshold = CalculateMassThreshold(FeH, 60.0f,  -0.31f, 45.0f);

        // 使用温度匹配亚型，因为计算谱线需要外挂数据库且非常麻烦
        auto CalculateSpectralSubclass = [&](this auto&& Self, Astro::AStar::EEvolutionPhase BasePhase) -> void
        {
            std::uint32_t SpectralClass = BasePhase == Astro::AStar::EEvolutionPhase::kWolfRayet ? 11 : 0;

            if (BasePhase != Astro::AStar::EEvolutionPhase::kWolfRayet)
            {
                if (BasePhase == Astro::AStar::EEvolutionPhase::kMainSequence)
                {
                    // 如果表面氢质量分数低于 0.4 且还是主序星阶段，或初始质量超过 WNxh 门槛，转为 WR 星
                    // 该情况只有 O 型星会出现
                    if (SurfaceH1 < 0.4f || InitialMassSol > WNxhLowerMassThreshold)
                    {
                        EvolutionPhase = Astro::AStar::EEvolutionPhase::kWolfRayet;
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

                float MassLossRateSolPerYear = StarData.GetStellarWindMassLossRate() * kYearToSecond / kSolarMass;
                if (SpectralClass == std::to_underlying(Astro::ESpectralClass::kSpectral_O) &&
                    (SurfaceH1 < 0.6f || MassLossRateSolPerYear > 1e-6))
                {
                    SpectralType.MarkSpecial(Astro::ESpecialMark::kCode_f);
                }
            }
            else
            {
                if (SurfaceZ <= 0.05f && SurfaceH1 >= 0.05f)
                {
                    SpectralSubclassMap = Astro::AStar::kSpectralSubclassMap_WNxh_;
                    SpectralClass = std::to_underlying(Astro::ESpectralClass::kSpectral_WN);
                    SpectralType.MarkSpecial(Astro::ESpecialMark::kCode_h);
                }
                else if (SurfaceZ <= 0.1f)
                {
                    SpectralSubclassMap = Astro::AStar::kSpectralSubclassMap_WN_;
                    SpectralClass = std::to_underlying(Astro::ESpectralClass::kSpectral_WN);
                }
                else if (SurfaceZ <= 0.6f)
                {
                    SpectralSubclassMap = Astro::AStar::kSpectralSubclassMap_WC_;
                    SpectralClass = std::to_underlying(Astro::ESpectralClass::kSpectral_WC);
                }
                else
                {
                    SpectralSubclassMap = Astro::AStar::kSpectralSubclassMap_WO_;
                    SpectralClass = std::to_underlying(Astro::ESpectralClass::kSpectral_WO);
                }
            }

            SpectralType.HSpectralClass = static_cast<Astro::ESpectralClass>(SpectralClass);

            if (SpectralSubclassMap.empty())
            {
                NpgsCoreError("Failed to find match subclass map of Age={}, FeH={}, Mass={}, Teff={}",
                              StarData.GetAge(), StarData.GetFeH(), StarData.GetMass() / kSolarMass, StarData.GetTeff());
            }

            Subclass = static_cast<float>(SpectralSubclassMap.front().second);
            float MinGap = std::numeric_limits<float>::max();
            for (const auto& [StandardTeff, StandardSubclass] : SpectralSubclassMap)
            {
                float Gap = std::abs(Teff - StandardTeff);
                if (Gap < MinGap)
                {
                    MinGap   = Gap;
                    Subclass = static_cast<float>(StandardSubclass);
                }
            }

            SpectralType.Subclass = Subclass;
        };

        if (EvolutionPhase != Astro::AStar::EEvolutionPhase::kWolfRayet)
        {
            switch (StellarType)
            {
            case Astro::EStellarType::kNormalStar:
                if (Teff < 54000)
                {
                    CalculateSpectralSubclass(EvolutionPhase);

                    if (EvolutionPhase != Astro::AStar::EEvolutionPhase::kWolfRayet &&
                        EvolutionPhase != Astro::AStar::EEvolutionPhase::kPrevMainSequence)
                    {
                        if (EvolutionPhase == Astro::AStar::EEvolutionPhase::kMainSequence)
                        {
                            if ((SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_O ||
                                 SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_B) &&
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
                    if (InitialMassSol <= WNxhLowerMassThreshold)
                    {
                        if (SpectralType.SpecialMarked(Astro::ESpecialMark::kCode_f))
                        {
                            SpectralType.HSpectralClass  = Astro::ESpectralClass::kSpectral_O;
                            SpectralType.Subclass        = 2.0f;
                            SpectralType.LuminosityClass = Astro::ELuminosityClass::kLuminosity_I;
                        }
                        else if (SurfaceH1 > MinSurfaceH1)
                        {
                            SpectralType.HSpectralClass  = Astro::ESpectralClass::kSpectral_O;
                            SpectralType.Subclass        = 2.0f;
                            SpectralType.LuminosityClass = Astro::ELuminosityClass::kLuminosity_V;
                        }
                        else if (SurfaceH1 > 0.4f)
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
            case Astro::EStellarType::kWhiteDwarf:
                if (Teff >= 12000)
                {
                    if (StarData.GetMass() / kSolarMass <= 0.5)
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
            case Astro::EStellarType::kNeutronStar:
                SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_Q;
                break;
            case Astro::EStellarType::kBlackHole:
                SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_X;
                break;
            case Astro::EStellarType::kDeathStarPlaceholder:
                SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_Unknown;
                break;
            default:
                SpectralType.HSpectralClass = Astro::ESpectralClass::kSpectral_Unknown;
                break;
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

    Astro::ELuminosityClass FStellarGenerator::CalculateLuminosityClass(const Astro::AStar& StarData) const
    {
        auto CalculateSurfaceGravity = [](float MassSol, float LuminositySol, float Teff) -> float
        {
            float LogMass       = std::log10(MassSol);
            float LogLuminosity = std::log10(LuminositySol);
            float LogTeff       = std::log10(Teff / kSolarTeff) * 4.0f;

            return kSolarSurfaceGravityLogG + LogMass + LogTeff - LogLuminosity;
        };

        float MassSol       = static_cast<float>(StarData.GetMass() / kSolarMass);
        float LuminositySol = static_cast<float>(StarData.GetLuminosity() / kSolarLuminosity);
        float Teff          = StarData.GetTeff();
        float LogG          = CalculateSurfaceGravity(MassSol, LuminositySol, Teff);

        auto BaseClass = CalculateLuminosityClassFromSurfaceGravity(Teff, LogG);

        if (Teff >= 43900 && BaseClass == Astro::ELuminosityClass::kLuminosity_IV)
        {
            BaseClass = Astro::ELuminosityClass::kLuminosity_III;
        }
        else if (Teff >= 40450)
        {
            if (BaseClass < Astro::ELuminosityClass::kLuminosity_I) // merge Ia/Iab/Ib
            {
                BaseClass = Astro::ELuminosityClass::kLuminosity_I;
            }
            else if (BaseClass == Astro::ELuminosityClass::kLuminosity_II)
            {
                BaseClass = Astro::ELuminosityClass::kLuminosity_I;
            }
        }

        if (BaseClass == Astro::ELuminosityClass::kLuminosity_Ia)
        {
            auto EvolutionPhase = StarData.GetEvolutionPhase();
            if (EvolutionPhase == Astro::AStar::EEvolutionPhase::kEarlyAgb ||
                EvolutionPhase == Astro::AStar::EEvolutionPhase::kThermalPulseAgb ||
                EvolutionPhase == Astro::AStar::EEvolutionPhase::kPostAgb)
            {
                BaseClass = Astro::ELuminosityClass::kLuminosity_II;
            }
            else
            {
                auto HypergiantClass = CalculateHypergiant(StarData);
                if (HypergiantClass != Astro::ELuminosityClass::kLuminosity_Unknown)
                {
                    BaseClass = HypergiantClass;
                }
            }
        }

        return BaseClass;
    }

    Astro::ELuminosityClass FStellarGenerator::CalculateLuminosityClassFromSurfaceGravity(float Teff, float LogG) const
    {
        struct LuminosityStandard
        {
            Astro::ELuminosityClass Class{};
            float LogG{};
        };

        struct SpectralTypeCalibration
        {
            int MinTeff{};
            std::array<LuminosityStandard, 8> Standards;
        };

        auto MakeData = [](int Teff, float V, float IV, float III, float II, float Ib, float Iab, float Ia)
            -> SpectralTypeCalibration
        {
            return {
                .MinTeff   = Teff,
                .Standards =
                {{
                    { Astro::ELuminosityClass::kLuminosity_V,      V   },
                    { Astro::ELuminosityClass::kLuminosity_IV,     IV  },
                    { Astro::ELuminosityClass::kLuminosity_III,    III },
                    { Astro::ELuminosityClass::kLuminosity_II,     II  },
                    { Astro::ELuminosityClass::kLuminosity_Ib,     Ib  },
                    { Astro::ELuminosityClass::kLuminosity_Iab,    Iab },
                    { Astro::ELuminosityClass::kLuminosity_Ia,     Ia  }
                }}
            };
        };

        // A new calibration of stellar parameters of Galactic O stars
        // Fundamental stellar parameters derived from the evolutionary tracks
        static const std::vector<SpectralTypeCalibration> kCalibrationTable
        {
            //       Teff   V      IV     III    II     Ib     Iab    Ia
            // -------------------------------------------------------------
            // O2-O4
            MakeData(41400, 3.95f, 3.90f, 3.85f, 3.80f, 3.75f, 3.72f, 3.70f),
            // O-Type Early (Table VII: O5)
            MakeData(39500, 3.90f, 3.86f, 3.82f, 3.76f, 3.74f, 3.69f, 3.65f),
            // O-Type Late (Table VII: O9)
            MakeData(31400, 3.95f, 3.82f, 3.74f, 3.58f, 3.50f, 3.44f, 3.31f),
            // B-Type Early (Table VII: B0)
            MakeData(26000, 4.00f, 3.88f, 3.74f, 3.39f, 3.27f, 3.19f, 3.05f),
            // B-Type Middle (Table VII: B5)
            MakeData(14500, 4.10f, 3.98f, 3.81f, 2.90f, 2.52f, 2.40f, 2.22f),
            // B-Type Late / A-Type Early (Table VII: B9/A0)
            MakeData(9500,  4.07f, 3.91f, 3.75f, 2.85f, 2.23f, 2.01f, 1.81f),
            // A-Type Middle (Table VII: A5)
            MakeData(7910,  4.22f, 4.06f, 3.86f, 2.81f, 2.14f, 1.74f, 1.53f),
            // F-Type Early (Table VII: F0)
            MakeData(7020,  4.28f, 4.05f, 3.83f, 2.67f, 2.00f, 1.51f, 1.25f),
            // F-Type Late (Table VII: F8)
            MakeData(6050,  4.35f, 3.89f, 3.50f, 2.38f, 1.71f, 1.06f, 0.83f),
            // G-Type Early (Table VII: G0)
            MakeData(5860,  4.39f, 3.84f, 3.20f, 2.29f, 1.62f, 0.95f, 0.72f),
            // G-Type Late (Table VII: G8)
            MakeData(5380,  4.55f, 3.64f, 2.95f, 1.84f, 1.30f, 0.60f, 0.30f),
            // K-Type Early (Table VII: K0)
            MakeData(5170,  4.57f, 3.57f, 2.89f, 1.74f, 1.20f, 0.54f, 0.25f),
            // K-Type Late (Table VII: K5)
            MakeData(4300,  4.57f, 3.50f, 1.93f, 1.20f, 0.77f, 0.35f, 0.10f),
            // M-Type (Table VII: M0 - M5)
            MakeData(0,     4.61f, 3.00f, 1.63f, 1.01f, 0.61f, 0.30f, 0.00f)
        };

        const auto* Current = &kCalibrationTable.back();
        for (const auto& Row : kCalibrationTable)
        {
            if (Teff >= Row.MinTeff)
            {
                Current = &Row;
                break;
            }
        }

        Astro::ELuminosityClass BestClass = Astro::ELuminosityClass::kLuminosity_Unknown;
        float MinGap = std::numeric_limits<float>::max();

        for (const auto& Standard : Current->Standards)
        {
            float Gap = std::abs(LogG - Standard.LogG);
            if (Gap < MinGap)
            {
                MinGap    = Gap;
                BestClass = Standard.Class;
            }
        }

        return BestClass;
    }

    Astro::ELuminosityClass FStellarGenerator::CalculateHypergiant(const Astro::AStar& StarData) const
    {
        float Luminosity     = static_cast<float>(StarData.GetLuminosity());
        float InitialMassSol = StarData.GetInitialMass() / kSolarMass;
        if (InitialMassSol < 20.0f)
        {
            return Astro::ELuminosityClass::kLuminosity_Unknown;
        }

        auto [EddingtonLimit, HdLimit] = CalculatePhysicalLuminosityLimit(StarData);
        float MinLimit = std::min(EddingtonLimit, HdLimit);
        float ProximityRatio = Luminosity / MinLimit;

        if (ProximityRatio >= 1.0f)
            return Astro::ELuminosityClass::kLuminosity_0;
        if (ProximityRatio >= 0.6f)
            return Astro::ELuminosityClass::kLuminosity_IaPlus;

        float MassLossRateSolPerYear = StarData.GetStellarWindMassLossRate() * kYearToSecond / kSolarMass;
        if (MassLossRateSolPerYear >= 1e-4f)
        {
            return Astro::ELuminosityClass::kLuminosity_0;
        }

        return Astro::ELuminosityClass::kLuminosity_Unknown;
    }

    FStellarGenerator::FPhysicalLuminosityLimit
    FStellarGenerator::CalculatePhysicalLuminosityLimit(const Astro::AStar& StarData) const
    {
        float MassSol = static_cast<float>(StarData.GetMass() / kSolarMass);

        // 1. Eddington Limit
        float EddingtonFactor   = StarData.GetEvolutionPhase() == Astro::AStar::EEvolutionPhase::kWolfRayet
                                ? 65000.0f : 32000.0f;
        float EddingtonLimitSol = EddingtonFactor * MassSol;

        // 2. Humphreys-Davidson Limit
        static constexpr float kKinkLogTeff             = 3.8f;
        static constexpr float kPlateauLogLuminositySol = 5.8f;

        float LogTeff = std::log10(StarData.GetTeff());
        float LogHdLimitSol = 0.0f;
        if (LogTeff <= kKinkLogTeff)
        {
            LogHdLimitSol = kPlateauLogLuminositySol;
        }
        else
        {
            static constexpr float kSlope = 1.0f; // Image from Wikipedia
            LogHdLimitSol = kPlateauLogLuminositySol + kSlope * (LogTeff - kKinkLogTeff);
        }
        float HdLimitSol = std::pow(10.0f, LogHdLimitSol);
    
        return {
            .EddingtonLimit = EddingtonLimitSol * kSolarLuminosity,
            .HdLimit        = HdLimitSol        * kSolarLuminosity
        };
    }

    FStellarGenerator::FRemainsBasicProperties
    FStellarGenerator::DetermineRemainsProperties(const Astro::AStar& DyingStarData, float InitialMassSol, float FeH, float CoreSpecificAngularMomentum)
    {
        Astro::AStar::EEvolutionPhase EvolutionPhase{};
        Astro::AStar::EStarFrom       DeathStarFrom{};
        Astro::EStellarType           DeathStarType{};
        Astro::FSpectralType          DeathStarClass;
        float                         DeathStarMassSol{};

        if (InitialMassSol < 10.0f)
        {
            DeathStarMassSol = CalculateRemainsMassSol(InitialMassSol);

            if (InitialMassSol >= 0.075f && InitialMassSol < 0.5f)
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
            else if (InitialMassSol >= 0.5f && InitialMassSol < 8.0f)
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
            else if (InitialMassSol >= 8.0f && InitialMassSol < 9.759f)
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
            else if (InitialMassSol >= 9.759f && InitialMassSol < 10.0f)
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
        }
        else
        {
            float HeliumCoreMassSol = CalculateHeliumCoreMassSol(DyingStarData);
            float DyingStarMassSol  = static_cast<float>(DyingStarData.GetMass() / kSolarMass);

            if (HeliumCoreMassSol >= 64.0f && HeliumCoreMassSol < 133.0f)
            {
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kNull;
                DeathStarFrom  = Astro::AStar::EStarFrom::kPairInstabilityHypernova;
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

                DeathStarMassSol = 0.0f;
            }
            else if (HeliumCoreMassSol >= 133.0f)
            {
                EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
                DeathStarFrom  = Astro::AStar::EStarFrom::kPhotodisintegration;
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

                DeathStarMassSol = DyingStarMassSol;
            }
            else
            {
                if (InitialMassSol < 33.75f)
                {
                    DeathStarMassSol = CalculateRemainsMassSol(InitialMassSol);
                }
                else
                {
                    DeathStarMassSol = DyingStarMassSol;
                }

                if (HeliumCoreMassSol >= 1.543f && HeliumCoreMassSol < 5.07f) // 10-21 Msun
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
                else if (HeliumCoreMassSol >= 5.07f && HeliumCoreMassSol < 6.011f) // 21-23.3537 Msun 臭弹谷
                {
                    EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
                    DeathStarFrom  = Astro::AStar::EStarFrom::kFailedSupernova;
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
                else if (HeliumCoreMassSol >= 6.011f && HeliumCoreMassSol < 11.005f) // 23.3537-33.75 Msun
                {
                    if (CheckEnvelopeStripSuccessful(FeH)) // 爆炸岛
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
                    else // 哑炮
                    {
                        EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
                        DeathStarFrom  = Astro::AStar::EStarFrom::kFailedSupernova;
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
                else
                {
                    EvolutionPhase = Astro::AStar::EEvolutionPhase::kStellarBlackHole;
                    
                    DeathStarFrom  = CoreSpecificAngularMomentum > 3e12f
                                   ? Astro::AStar::EStarFrom::kRelativisticJetHypernova
                                   : Astro::AStar::EStarFrom::kFailedSupernova;
                    
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
        }

        return {
            .EvolutionPhase    = EvolutionPhase,
            .DeathStarFrom     = DeathStarFrom,
            .DeathStarType     = DeathStarType,
            .DeathStarClass    = DeathStarClass,
            .DeathStarMassSol  = DeathStarMassSol
        };
    }

    FStellarGenerator::FRemainsBasicProperties FStellarGenerator::GenerateMergeStar(const FStellarBasicProperties& Properties)
    {
        Astro::AStar::EEvolutionPhase EvolutionPhase{};
        Astro::AStar::EStarFrom       DeathStarFrom{};
        Astro::EStellarType           DeathStarType{};
        Astro::FSpectralType          DeathStarClass;
        float                         DeathStarMassSol{};

        float MergeStarProbability = 0.1f * static_cast<int>(Properties.bIsSingleStar);
        Math::TBernoulliDistribution MergeProbability(MergeStarProbability);
        if (MergeProbability(RandomEngine_))
        {
            DeathStarFrom = Astro::AStar::EStarFrom::kWhiteDwarfMerge;
            Math::TBernoulliDistribution BlackHoleProbability(0.114514);
            float MassSol = 0.0f;
            if (BlackHoleProbability(RandomEngine_))
            {
                Math::TUniformRealDistribution<> MassDistribution(2.6f, 3.2f);
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
                Math::TUniformRealDistribution<> MassDistribution(1.2f, 1.3f);
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

        return {
            .EvolutionPhase   = EvolutionPhase,
            .DeathStarFrom    = DeathStarFrom,
            .DeathStarType    = DeathStarType,
            .DeathStarClass   = DeathStarClass,
            .DeathStarMassSol = DeathStarMassSol
        };
    }

    Astro::AStar FStellarGenerator::CalculateAndMakeDeathStar(const Astro::AStar& DyingStarData,
                                                              const FRemainsBasicProperties& Properties, float DeathStarAge)
    {
        auto [EvolutionPhase, DeathStarFrom, DeathStarType, DeathStarClass, DeathStarMassSol] = Properties;

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
                .Age            = DeathStarAge,
                .FeH            = 0.0f,
                .InitialMassSol = DeathStarMassSol
            };

            auto WhiteDwarfData = GetFullMistData(WhiteDwarfBasicProperties, true, true);

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
            LogR                    = std::numeric_limits<float>::quiet_NaN();
            LogTeff                 = std::numeric_limits<float>::quiet_NaN();
            LogCenterT              = std::numeric_limits<float>::quiet_NaN();
            LogCenterRho            = std::numeric_limits<float>::quiet_NaN();
            SurfaceZ                = std::numeric_limits<float>::quiet_NaN();
            SurfaceEnergeticNuclide = std::numeric_limits<float>::quiet_NaN();
            SurfaceVolatiles        = std::numeric_limits<float>::quiet_NaN();

            break;
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

        Astro::AStar DeathStar;

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

        CalculateSpectralType(DeathStar);

        return DeathStar;
    }

    float FStellarGenerator::CalculateRemainsMassSol(float InitialMassSol) const
    {
        float RemainsMassSol = 0.0f;
        if (InitialMassSol > -0.75f && InitialMassSol < 0.8f)
        {
            RemainsMassSol = (0.9795f - 0.393f * InitialMassSol) * InitialMassSol;
        }
        else if (InitialMassSol >= 0.8f && InitialMassSol < 7.9f)
        {
            RemainsMassSol =
                -0.00012336f * std::pow(InitialMassSol, 6.0f) + 0.00316f * std::pow(InitialMassSol, 5.0f) -
                 0.0296f     * std::pow(InitialMassSol, 4.0f) + 0.1235f  * std::pow(InitialMassSol, 3.0f) -
                 0.2155f     * std::pow(InitialMassSol, 2.0f) + 0.19022f * InitialMassSol + 0.46575f;
        }
        else if (InitialMassSol >= 7.9f && InitialMassSol < 10.0f)
        {
            RemainsMassSol = 1.301f + 0.008095f * InitialMassSol;
        }
        else if (InitialMassSol >= 10.0f && InitialMassSol < 21.0f)
        {
            RemainsMassSol = 1.246f + 0.0136f * InitialMassSol;
        }
        else if (InitialMassSol >= 21.0f && InitialMassSol < 23.3537f)
        {
            RemainsMassSol = std::pow(10.0f, (1.334f - 0.009987f * InitialMassSol));
        }
        else if (InitialMassSol >= 23.3537f && InitialMassSol < 33.75f)
        {
            RemainsMassSol = 12.1f - 0.763f * InitialMassSol + 0.0137f * std::pow(InitialMassSol, 2.0f);
        }
        else
        {
            RemainsMassSol = std::numeric_limits<float>::quiet_NaN(); // it should be error
        }

        return RemainsMassSol;
    }

    bool FStellarGenerator::CheckEnvelopeStripSuccessful(float FeH)
    {
        if (FeH >= 0.0f)
        {
            return true;
        }
        else if (FeH <= -1.0f)
        {
            return false;
        }

        Math::TBernoulliDistribution<> Explodability(std::abs(FeH));
        return Explodability(RandomEngine_);
    }

    void FStellarGenerator::CalculateExtremeProperties(Astro::AStar& DeathStarData, const Astro::AStar& ZamsStarData,
                                                       float CoreSpecificAngularMomentum)
    {
        float InitialSpin     = 0.0f;
        float DeathStarMass   = static_cast<float>(DeathStarData.GetMass());
        float DeathStarRadius = static_cast<float>(DeathStarData.GetRadius());
        auto  DeathStarType   = DeathStarData.GetStellarClass().GetStellarType();

        if (DeathStarType == Astro::EStellarType::kNeutronStar)
        {
            InitialSpin = (2.0f * Math::kPi * std::pow(DeathStarRadius, 2.0f) * 0.35f) / CoreSpecificAngularMomentum;
        }
        else
        {
            InitialSpin = (2.0f * Math::kPi * std::pow(DeathStarRadius, 2.0f) * 0.2f)  / CoreSpecificAngularMomentum;
        }

        float DynamoField = std::numeric_limits<float>::min();
        if (InitialSpin <= 3e-3f && DeathStarType == Astro::EStellarType::kNeutronStar)
        {
            DynamoField = CalculateDynamoField(InitialSpin);
        }

        // 对于化石磁场，其磁感应强度通常需要数百亿年才能消散
        // 普通磁场变化太几把麻烦不算了
        // 残骸继承的磁通量，大致等于 ZAMS 时期的总磁通量
        float ZamsMagneticField    = ZamsStarData.GetMagneticField();
        float ZamsRadius           = static_cast<float>(ZamsStarData.GetRadius());
        float Efficiency           = 0.05f + CommonGenerator_(RandomEngine_) * 0.05f; // 剩余的 90%-95% 基本都丢了
        // 在偶极场模型下核心增强的磁场强度差不多和 R_core/R_zams 抵消了，数量级是对的
        float CompressedMagnetic   = ZamsMagneticField * std::pow(ZamsRadius / DeathStarRadius, 2.0f) * Efficiency;
        float InitialMagneticField = std::max(CompressedMagnetic, DynamoField);

        if (DeathStarType == Astro::EStellarType::kWhiteDwarf)
        {
            DeathStarData.SetMagneticField(InitialMagneticField);
            DeathStarData.SetSpin(InitialSpin);
            return; // 白矮星的性质稳定太多
        }

        CalculateNeutronStarDecayedProperties(DeathStarData, InitialMagneticField, InitialSpin);
    }

    float FStellarGenerator::CalculateDynamoField(float Spin)
    {
        // Neutron Star Dynamos and the Origins of Pulsar Magnetism
        static constexpr float kMaxMagneticField = 3e11f;
        static constexpr float kOptimal          = 1e-3f; // 直接拉满
        static constexpr float kThreshold        = 3e-3f;

        float Efficiency = (kThreshold - Spin) / (kThreshold - kOptimal);
        Efficiency = std::clamp(Efficiency, 0.0f, 1.0f);
        float SaturationLevel = kMaxMagneticField * std::sqrt(Efficiency);

        float GeneratorEfficiency = 0.3f + CommonGenerator_(RandomEngine_) * 0.7f; // 30%-100%
        return SaturationLevel * GeneratorEfficiency;
    }

    void FStellarGenerator::CalculateNeutronStarDecayedProperties(Astro::AStar& DeathStarData, float InitialMagneticField, float InitialSpin) const
    {
        // 磁感应强度
        // 霍尔漂移：B(t) = B_0 / (1 + t / Tau_H0)
        // 欧姆耗散：B(t) = B_0 * exp(-(t - t_break) / Tau_ohm)
        static constexpr float kMagneticBreakPoint = 1e9f; // 高于此值霍尔漂移，低于此值欧姆耗散

        // Tau_H0 = Mu_0 * n_e * e * L^2 / B_0
        // n_e 取平均值 1e41m^-3，L 取 500m
        double TauHall = kVacuumPermeability * 1e41 * kElementaryCharge * std::pow(500.0, 2.0) / InitialMagneticField;
        // t_break = Tau_H0 * (B_0 / kMagneticBreakPoint - 1)
        double BreakTime = TauHall * (InitialMagneticField / kMagneticBreakPoint - 1.0f);

        // Tau_ohm = Mu_0 * Sigma * L^2
        double Teff   = DeathStarData.GetTeff();
        double Tint   = 1.29e8 * std::pow(Teff / 1e6, 1.85);
        double Sigma  = Tint > 1e8 ? 1e14 * (1e8 / Tint) : 1e14;
        double TauOhm = kVacuumPermeability * Sigma * std::pow(500.0, 2.0);

        double DeathStarAgeSecond = DeathStarData.GetAge() * kYearToSecond;
        double MagneticField      = 0.0;
        if (InitialMagneticField >= kMagneticBreakPoint)
        {
            if (DeathStarAgeSecond < BreakTime)
            {
                MagneticField = InitialMagneticField / (1.0 + DeathStarAgeSecond / TauHall);
            }
            else
            {
                double MagneticAtBreak = InitialMagneticField / (1.0 + BreakTime / TauHall);
                MagneticField = MagneticAtBreak * std::exp(-(DeathStarAgeSecond - BreakTime) / TauOhm);
            }
        }
        else
        {
            MagneticField = InitialMagneticField * std::exp(-DeathStarAgeSecond / TauOhm);
        }

        DeathStarData.SetMagneticField(static_cast<float>(MagneticField));

        // 自转周期
        // I = (0.237 +/- 0.008) M * R^2 * [1 + 4.2 * x + 0.03 * x^2], x = M/Msun * km/R
        double DeathStarMass   = DeathStarData.GetMass();
        double DeathStarRadius = DeathStarData.GetRadius();
        double Xx              = (DeathStarMass / kSolarMass) * (1000.0 / DeathStarRadius);
        double Inertia         = 0.237 * DeathStarMass * std::pow(DeathStarRadius, 2.0) * (1.0 + 4.2 * Xx + 0.03 * std::pow(Xx, 2.0));

        // Pdot = (8 * Pi^2 * R^6) / (3 * Mu_0 * c^3 * I) * (B^2 / P)
        double Kx = (8.0 * std::pow(Math::kPi, 2.0) * std::pow(DeathStarRadius, 6.0)) /
                    (3.0 * kVacuumPermeability * std::pow(kSpeedOfLight, 3.0) * Inertia);

        double IntegralResult = 0.0; // B_0^2
        if (InitialMagneticField > kMagneticBreakPoint)
        {
            double HalLEndTime = std::min(BreakTime, DeathStarAgeSecond);
            // 积分 B0^2 / (1 + t/Tau_H0)^2 dt = B0^2 * Tau_H0 * (1 - 1/(1 + t/Tau_H0))
            IntegralResult += std::pow(InitialMagneticField, 2.0) * TauHall * (1.0 - 1.0 / (1.0 + HalLEndTime / TauHall));
        }

        if (DeathStarAgeSecond > BreakTime)
        {
            double MagneticAtBreak = (InitialMagneticField > kMagneticBreakPoint)
                ? InitialMagneticField / (1.0 + BreakTime / TauHall) : InitialMagneticField;
            double OhmicDurationBegin = (InitialMagneticField > kMagneticBreakPoint)
                ? (DeathStarAgeSecond - BreakTime) : DeathStarAgeSecond;

            // 积分 B_break^2 * exp(-2t / Tau_ohm) dt = (Tau_ohm * B_break^2 / 2) * (1 - exp(-2t / Tau_ohm))
            IntegralResult += (TauOhm * std::pow(MagneticAtBreak, 2.0) / 2.0) *
                              (1.0 - std::exp(-2.0 * OhmicDurationBegin / TauOhm));
        }

        // P = sqrt(P0^2 + 2 * k * integral(B^2 dt))
        double Spin = std::sqrt(std::pow(InitialSpin, 2.0) + 2.0 * Kx * IntegralResult);
        DeathStarData.SetSpin(static_cast<float>(Spin));
    }

    float FStellarGenerator::CalculateCoreSpecificAngularMomentum(const Astro::AStar& DyingStarData) const
    {
        float Radius = DyingStarData.GetRadius();
        float Spin   = DyingStarData.GetSpin();
        float SurfaceSpecificAngularMomentum = (2.0f * Math::kPi * std::pow(Radius, 2.0f)) / Spin;

        // Presupernova Evolution of Differentially Rotating Massive Stars Including Magnetic Fields
        static constexpr float kFmax = 0.1f;
        static constexpr float kFmin = 1e-5f;
        static constexpr float kTmid = 7500.0f;
        static constexpr float kKx   = 20.0f;
        float Teff = DyingStarData.GetTeff();

        float Exponent = -kKx * (std::log10(Teff) - std::log10(kTmid));
        float CouplingFactor = kFmin + (kFmax - kFmin) / (1 + std::exp(Exponent));

        return SurfaceSpecificAngularMomentum * CouplingFactor;
    }

    float FStellarGenerator::CalculateHeliumCoreMassSol(const Astro::AStar& DyingStarData) const
    {
        float InitialMassSol    = static_cast<float>(DyingStarData.GetInitialMass() / kSolarMass);
        float HeliumCoreMassSol = 0.0f;

        // The Evolution of Massive Helium Stars Including Mass Loss
        float TheoreticalHeCoreMassSol = InitialMassSol <= 30.0f
            ? 0.0385f * std::pow(InitialMassSol, 1.603f)
            : 0.5f * InitialMassSol - 5.87f;

        float DyingStarMassSol = static_cast<float>(DyingStarData.GetMass() / kSolarMass);
        if (DyingStarData.GetEvolutionPhase() == Astro::AStar::EEvolutionPhase::kWolfRayet ||
            DyingStarMassSol < TheoreticalHeCoreMassSol)
        {
            HeliumCoreMassSol = DyingStarMassSol;
        }
        else
        {
            HeliumCoreMassSol = TheoreticalHeCoreMassSol;
        }

        return HeliumCoreMassSol;
    }

    void FStellarGenerator::GenerateZamsStarMagnetic(Astro::AStar& StarData)
    {
        Math::TDistribution<>* MagneticGenerator = nullptr;
        auto  ZamsSpectralType = StarData.GetStellarClass().Data();
        float MagneticField    = 0.0f;
        float InitialMassSol   = static_cast<float>(StarData.GetInitialMass() / kSolarMass);

        if (InitialMassSol >= 0.075f && InitialMassSol < 0.33f)
        {
            MagneticGenerator = &MagneticGenerators_[kLowMassMStarIndex_];
        }
        else if (InitialMassSol >= 0.33f && InitialMassSol < 0.6f)
        {
            MagneticGenerator = &MagneticGenerators_[kLateKMStarIndex_];
        }
        else if (InitialMassSol >= 0.6f && InitialMassSol < 1.5f)
        {
            MagneticGenerator = &MagneticGenerators_[kSolarLikeStarIndex_];
        }
        else if (InitialMassSol >= 1.5f && InitialMassSol < 20.0f)
        {
            if ((ZamsSpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_A  ||
                 ZamsSpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_B) &&
                XpStarProbabilityGenerators_[kXpStarIndex_](RandomEngine_))
            {
                MagneticGenerator = &MagneticGenerators_[kCommonXpStarIndex_];
                StarData.ModifyStellarClass(Astro::ESpecialMark::kCode_p, true);
            }
            else
            {
                MagneticGenerator = &MagneticGenerators_[kNormalMassiveStarIndex_];
            }
        }
        else
        {
            if ((ZamsSpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_O   ||
                 ZamsSpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_WN) &&
                XpStarProbabilityGenerators_[kOpStarIndex_](RandomEngine_))
            {
                Math::TBernoulliDistribution MonsterProbability(0.0015);
                if (MonsterProbability(RandomEngine_))
                {
                    MagneticGenerator = &MagneticGenerators_[kExtremeOpStarIndex_];
                }
                else
                {
                    MagneticGenerator = &MagneticGenerators_[kCommonXpStarIndex_];
                }

                StarData.ModifyStellarClass(Astro::ESpecialMark::kCode_p, true);
            }
            else
            {
                MagneticGenerator = &MagneticGenerators_[kNormalMassiveStarIndex_];
            }
        }

        MagneticField = std::pow(10.0f, (*MagneticGenerator)(RandomEngine_)) / 10000;
        StarData.SetMagneticField(MagneticField);
    }

    void FStellarGenerator::GenerateCompatStarMagnetic(Astro::AStar& StarData)
    {
        Math::TDistribution<>* MagneticGenerator = nullptr;
        float MagneticField = 0.0f;
        auto  StellarType   = StarData.GetStellarClass().GetStellarType();

        switch (StellarType)
        {
        case Astro::EStellarType::kWhiteDwarf:
            MagneticGenerator = &MagneticGenerators_[kWhiteDwarfIndex_];
            MagneticField     = std::pow(10.0f, (*MagneticGenerator)(RandomEngine_));
            break;
        case Astro::EStellarType::kNeutronStar:
        {
            MagneticGenerator = &MagneticGenerators_[kNeutronStarIndex_];

            float StarAge = static_cast<float>(StarData.GetAge());
            MagneticField = (*MagneticGenerator)(RandomEngine_) / (std::pow((0.034f * StarAge / 1e4f), 1.17f) + 0.84f);

            break;
        }
        case Astro::EStellarType::kBlackHole:
            MagneticField = 0.0f;
            break;
        default:
            break;
        }

        StarData.SetMagneticField(MagneticField);
    }

    void FStellarGenerator::GenerateZamsStarInitialSpin(Astro::AStar& StarData)
    {
        float Radius         = StarData.GetRadius();
        float EffectiveMass  = CalculateEffectiveMass(StarData);
        float CriticalSpin   = 2.0f * Math::kPi * std::sqrt(std::pow(Radius, 3.0f) / (kGravityConstant * EffectiveMass));
        StarData.SetCriticalSpin(CriticalSpin);

        float InitialMassSol = static_cast<float>(StarData.GetInitialMass() / kSolarMass);
        float FeH            = StarData.GetFeH();
        float LogMass        = std::log10(InitialMassSol);

        float InitialSpin    = 0.0f;

        if (InitialMassSol <= 1.4f)
        {
            InitialSpin = std::pow(10.0f, 30.893f - 25.34303f * std::exp(LogMass) + 21.7577f * LogMass +
                                   7.34205f * std::pow(LogMass, 2.0f) + 0.12951f * std::pow(LogMass, 3.0f));
        }
        else
        {
            if (InitialMassSol <= 8.0f)
            {
                InitialSpin = std::pow(10.0f, 28.0784f - 22.15753f * std::exp(LogMass) + 12.55134f * LogMass +
                                       30.9045f * std::pow(LogMass, 2.0f) - 10.1479f * std::pow(LogMass, 3.0f) +
                                       4.6894f  * std::pow(LogMass, 4.0f));
            }
            else
            {
                // The VLT-FLAMES Tarantula Survey. XII. Rotational velocities of the single O-type stars
                // Grids of stellar models with rotation
                // 双峰，高速峰符合正态分布，低速峰符合伽马分布
                float Ratio = 0.0f;
                if (FastMassiveStarProbabilityGenerator_(RandomEngine_))
                {
                    Ratio = SpinGenerators_[kFastMassiveStarIndex_]->Generate(RandomEngine_);
                    Ratio = std::clamp(Ratio, 0.4f, 0.98f);
                }
                else
                {
                    Ratio = SpinGenerators_[kSlowMassiveStarIndex_]->Generate(RandomEngine_);
                    Ratio = std::clamp(Ratio, 0.01f, 0.4f);
                }

                InitialSpin = CriticalSpin / Ratio;
            }
        }

        StarData.SetSpin(InitialSpin);
    }

    float FStellarGenerator::CalculateEffectiveMass(const Astro::AStar& StarData) const
    {
        auto [EddingtonLimit, _] = CalculatePhysicalLuminosityLimit(StarData);

        float Luminosity = static_cast<float>(StarData.GetLuminosity());
        float Gamma = Luminosity / EddingtonLimit;
        if (Gamma >= 0.95f)
        {
            Gamma = 0.95f;
        }

        float EffectiveMass = static_cast<float>(StarData.GetMass() * (1.0f - Gamma));
        return EffectiveMass;
    }

    void FStellarGenerator::CalculateCurrentSpinAndOblateness(Astro::AStar& StarData, const Astro::AStar& ZamsStarData)
    {
        float ZamsRadiusSol = ZamsStarData.GetRadius() / kSolarRadius;
        float InitialSpin   = ZamsStarData.GetSpin();
        bool  bIsFastSpin   = ZamsStarData.GetSpin() / ZamsStarData.GetCriticalSpin() >= 0.4f;
        CalculateNormalStarDecayedSpin(StarData, ZamsRadiusSol, InitialSpin, bIsFastSpin);

        float EffectiveMass = CalculateEffectiveMass(StarData);
        CalculateNormalStarOblateness(StarData, EffectiveMass, StarData.GetSpin());
    }

    void FStellarGenerator::CalculateNormalStarDecayedSpin(Astro::AStar& StarData, float ZamsRadiusSol, float InitialSpin, bool bIsFastSpin)
    {
        auto  EvolutionPhase = StarData.GetEvolutionPhase();
        float InitialMassSol = static_cast<float>(StarData.GetInitialMass() / kSolarMass);
        float FeH            = StarData.GetFeH();
        float RadiusSol      = StarData.GetRadius() / kSolarRadius;

        float AngularMomentumMultipler = 0.0f;
        float BrakingFactor            = 0.0f;

        if (InitialMassSol <= 1.4f)
        {
            AngularMomentumMultipler = std::pow(RadiusSol / ZamsRadiusSol, 2.5f);

            float Base = 1.0f + CommonGenerator_(RandomEngine_);
            if (StarData.GetStellarClass().Data().SpecialMarked(Astro::ESpecialMark::kCode_p))
            {
                Base *= 10;
            }

            float StarAge = static_cast<float>(StarData.GetAge());
            BrakingFactor = std::pow(2.0f, std::sqrt(Base * (StarAge + 1e6f) * 1e-9f));
        }
        else
        {
            AngularMomentumMultipler = std::pow(RadiusSol / ZamsRadiusSol, 2.0f);

            if (EvolutionPhase != Astro::AStar::EEvolutionPhase::kWolfRayet)
            {
                float AgeSecond    = static_cast<float>(StarData.GetAge() * kYearToSecond);
                float Magnetic     = StarData.GetMagneticField();
                float MassLossRate = StarData.GetStellarWindMassLossRate();
                float WindSpeed    = StarData.GetStellarWindSpeed();
                float Mass         = static_cast<float>(StarData.GetMass());
                float Radius       = RadiusSol * kSolarRadius;

                float Numerator    = 2.0f * Math::kPi * std::pow(Magnetic, 2.0f) * std::pow(Radius, 2.0f);
                float Denominator  = kVacuumPermeability * MassLossRate * WindSpeed;
                float EtaStar      = Numerator / Denominator;
                float TauMass      = Mass / MassLossRate;

                if (Magnetic > 0.01f && EtaStar > 1.0f)
                {
                    // ud-Doula - Dynamical simulations of magnetically channelled line-driven stellar
                    float SqrtEta  = std::sqrt(EtaStar);
                    float TauSpin  = 0.2f * TauMass / SqrtEta;
                    float Exponent = AgeSecond / TauSpin;
                    BrakingFactor  = std::exp(Exponent);
                }
                else
                {
                    // THE EVOLUTION OF ROTATING STARS. 159, Figure 3
                    // Langer (1998): J ~ M^y (y 约为 2.0 对于 O 星风)
                    float MassRatio = InitialMassSol / static_cast<float>(StarData.GetMass() / kSolarMass);
                    BrakingFactor   = std::pow(MassRatio, 2.0f);

                    if (BrakingFactor > 20.0f)
                    {
                        BrakingFactor = 20.0f;
                    }
                }
            }
            else
            {
                // WR 要么走 ud-Doula，要么直接继承比率，不走星风兜底，除非半径更大，否则也不碰角动量
                AngularMomentumMultipler = std::max(AngularMomentumMultipler, 1.0f);
                if (bIsFastSpin)
                {
                    // The Galactic WC stars. 0.2-0.4
                    BrakingFactor = 1.0f / (0.2f + CommonGenerator_(RandomEngine_) * 0.2f);
                }
                else
                {
                    // The evolution of rotating stars. 178, 0.05-0.1
                    BrakingFactor = 1.0f / (0.05f + CommonGenerator_(RandomEngine_) * 0.05f);
                }
            }
        }

        float Spin = InitialSpin * AngularMomentumMultipler * BrakingFactor;
        StarData.SetSpin(Spin);
    }

    void FStellarGenerator::CalculateNormalStarOblateness(Astro::AStar& StarData, float EffectiveMass, float Spin)
    {
        // Modified Roche Approximation
        float Alpha = 4.0f * std::pow(Math::kPi, 2.0f) * std::pow(StarData.GetRadius(), 3.0f);
        Alpha /= (std::pow(Spin, 2.0f) * kGravityConstant * EffectiveMass);

        float RocheFactor = Alpha / (2.0f + Alpha);
        float Oblateness  = RocheFactor * 0.95f;
        Oblateness = std::min(Oblateness, 0.33f);

        StarData.SetOblateness(Oblateness);

        auto SpectralType = StarData.GetStellarClass().Data();
        if (SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_O ||
            SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_B)
        {
            float XeProbatility = std::clamp((Alpha - 0.5f) / 0.4f, 0.0f, 1.0f);
            Math::TBernoulliDistribution XeStarProbability(XeProbatility);
            if (XeStarProbability(RandomEngine_))
            {
                StarData.ModifyStellarClass(Astro::ESpecialMark::kCode_e, true);
            }
        }
    }

    void FStellarGenerator::GenerateCompatStarSpin(Astro::AStar& StarData)
    {
        float StarAge = static_cast<float>(StarData.GetAge());
        float Spin    = 0.0f;

        Astro::EStellarType StellarType = StarData.GetStellarClass().GetStellarType();

        switch (StellarType)
        {
        case Astro::EStellarType::kWhiteDwarf:
            Spin = std::pow(10.0f, SpinGenerators_[kWhiteDwarfSpinIndex_]->Generate(RandomEngine_));
            break;
        case Astro::EStellarType::kNeutronStar:
            Spin = StarAge * 3e-9f + SpinGenerators_[kNeutronStarSpinIndex_]->Generate(RandomEngine_);
            break;
        case Astro::EStellarType::kBlackHole: // 此处表示无量纲自旋参数，而非自转时间
            Spin = SpinGenerators_[kBlackHoleSpinIndex_]->Generate(RandomEngine_);
            break;
        default:
            break;
        }

        StarData.SetSpin(Spin);
    }

    void FStellarGenerator::ExpandMistData(double TargetMassSol, FDataArray& StarData) const
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

    void FStellarGenerator::CalculateMinCoilMass(Astro::AStar& StarData) const
    {
        double Mass          = StarData.GetMass();
        double Luminosity    = StarData.GetLuminosity();
        float  Radius        = StarData.GetRadius();
        float  MagneticField = StarData.GetMagneticField();

        float MinCoilMass = static_cast<float>(std::max(
            6.6156e14  * std::pow(MagneticField, 2.0f) * std::pow(Luminosity, 1.5) * std::pow(CoilTemperatureLimit_, -6.0f) * std::pow(dEpdM_, -1.0f),
            2.34865e29 * std::pow(MagneticField, 2.0f) * std::pow(Luminosity, 2.0) * std::pow(CoilTemperatureLimit_, -8.0f) * std::pow(Mass,   -1.0)
        ));

        StarData.SetMinCoilMass(MinCoilMass);
    }
} // namespace Npgs
