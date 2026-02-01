#include "stdafx.h"
#include "Universe.hpp"

#include <cstdlib>
#include <algorithm>
#include <array>
#include <format>
#include <iomanip>
#include <limits>
#include <print>
#include <ranges>
#include <sstream>
#include <string>
#include <utility>

#include "Engine/Core/Base/Base.hpp"
#include "Engine/Core/Math/NumericConstants.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/System/Generators/OrbitalGenerator.hpp"
#include "Engine/System/Services/EngineServices.hpp"

namespace Npgs
{
    FUniverse::FUniverse(std::uint32_t Seed, std::size_t StarCount, std::size_t ExtraGiantCount, std::size_t ExtraMassiveStarCount,
                         std::size_t ExtraNeutronStarCount, std::size_t ExtraBlackHoleCount, std::size_t ExtraMergeStarCount,
                         float UniverseAge)
        : RandomEngine_(Seed)
        , SeedGenerator_(0ull, std::numeric_limits<std::uint32_t>::max())
        , CommonGenerator_(0.0f, 1.0f)
        , ThreadPool_(EngineCoreServices->GetThreadPool())
        , StarCount_(StarCount)
        , ExtraGiantCount_(ExtraGiantCount)
        , ExtraMassiveStarCount_(ExtraMassiveStarCount)
        , ExtraNeutronStarCount_(ExtraNeutronStarCount)
        , ExtraBlackHoleCount_(ExtraBlackHoleCount)
        , ExtraMergeStarCount_(ExtraMergeStarCount)
        , UniverseAge_(UniverseAge)
    {
        std::vector<std::uint32_t> Seeds(32);
        for (int i = 0; i != 32; ++i)
        {
            Seeds[i] = SeedGenerator_(RandomEngine_);
        }

        std::ranges::shuffle(Seeds, RandomEngine_);
        std::seed_seq SeedSequence(Seeds.begin(), Seeds.end());
        RandomEngine_.seed(SeedSequence);
    }

    void FUniverse::FillUniverse()
    {
        int MaxThread = ThreadPool_->GetMaxThreadCount();

        GenerateStars(MaxThread);
        FillStellarSystem(MaxThread);

        // ThreadPool_->Terminate();
    }

    void FUniverse::ReplaceStar(std::size_t DistanceRank, const Astro::AStar& StarData)
    {
        for (auto& System : OrbitalSystems_)
        {
            if (DistanceRank == System.GetBaryDistanceRank())
            {
                auto& Stars = System.StarsData();
                if (Stars.size() > 1)
                {
                    return; // TODO: 处理双星
                }

                Stars.clear();
                Stars.push_back(std::make_unique<Astro::AStar>(StarData));
            }
        }
    }

    // 通用统计结构体，替代原本分散的定义
    template <typename Ty>
    struct StatEntry
    {
        Ty Value{};
        const Astro::AStar* Star = nullptr;
    };

    void FUniverse::CountStars()
    {
        constexpr int kTypeOIndex = 0;
        constexpr int kTypeBIndex = 1;
        constexpr int kTypeAIndex = 2;
        constexpr int kTypeFIndex = 3;
        constexpr int kTypeGIndex = 4;
        constexpr int kTypeKIndex = 5;
        constexpr int kTypeMIndex = 6;

        std::array<std::size_t, 7> MainSequences{};
        std::array<std::size_t, 7> Subgiants{};
        std::array<std::size_t, 7> Giants{};
        std::array<std::size_t, 7> BrightGiants{};
        std::array<std::size_t, 7> Supergiants{};
        std::array<std::size_t, 7> Hypergiants{};

        std::size_t WolfRayet    = 0;
        std::size_t WhiteDwarfs  = 0;
        std::size_t NeutronStars = 0;
        std::size_t BlackHoles   = 0;
        std::size_t TotalStars   = 1;
        std::size_t TotalBinarys = 0;
        std::size_t TotalSingles = 0;

        using MostLuminous   = StatEntry<double>;
        using MostMassive    = StatEntry<double>;
        using Largest        = StatEntry<float>;
        using Hottest        = StatEntry<float>;
        using Oldest         = StatEntry<double>;
        using MostOblateness = StatEntry<float>;
        using MostMagnetic   = StatEntry<float>;
        using MostMdot       = StatEntry<double>;

        auto FormatTitle = []() -> std::string
        {
            return std::format("{:>6} {:>6} {:>8} {:>8} {:10} {:>5} {:>13} {:>8} {:>8} {:>11} {:>8} {:>9} {:>5} {:>15} {:>9} {:>8}",
                               "InMass", "Mass", "Radius", "Age", "Class", "FeH", "Lum", "Teff", "CoreTemp", "CoreDensity", "Mdot", "WindSpeed", "Phase", "Magnetic", "Lifetime", "Oblateness");
        };

        auto FormatInfo = [](const Astro::AStar* Star) -> std::string
        {
            if (Star == nullptr)
            {
                return "No star generated.";
            }

            return std::format(
                "{:6.2f} {:6.2f} {:8.2f} {:8.2E} {:10} {:5.2f} {:13.4f} {:8.1f} {:8.2E} {:11.2E} {:8.2E} {:9} {:5} {:15.5f} {:9.2E} {:8.2f}",
                Star->GetInitialMass() / kSolarMass,
                Star->GetMass() / kSolarMass,
                Star->GetRadius() / kSolarRadius,
                Star->GetAge(),
                Star->GetStellarClass().ToString(),
                Star->GetFeH(),
                Star->GetLuminosity() / kSolarLuminosity,
                // kSolarAbsoluteMagnitude - 2.5 * std::log10(Star->GetLuminosity() / kSolarLuminosity),
                Star->GetTeff(),
                Star->GetCoreTemp(),
                Star->GetCoreDensity(),
                Star->GetStellarWindMassLossRate() * kYearToSecond / kSolarMass,
                static_cast<int>(std::round(Star->GetStellarWindSpeed())),
                static_cast<int>(Star->GetEvolutionPhase()),
                // Star->GetSurfaceZ(),
                // Star->GetSurfaceEnergeticNuclide(),
                // Star->GetSurfaceVolatiles(),
                Star->GetMagneticField(),
                Star->GetLifetime(),
                Star->GetOblateness()
            );
        };

        auto CountClass = [](const Astro::FSpectralType& SpectralType, std::array<std::size_t, 7>& Type)
        {
            switch (SpectralType.HSpectralClass)
            {
            case Astro::ESpectralClass::kSpectral_O:
                ++Type[kTypeOIndex];
                break;
            case Astro::ESpectralClass::kSpectral_B:
                ++Type[kTypeBIndex];
                break;
            case Astro::ESpectralClass::kSpectral_A:
                ++Type[kTypeAIndex];
                break;
            case Astro::ESpectralClass::kSpectral_F:
                ++Type[kTypeFIndex];
                break;
            case Astro::ESpectralClass::kSpectral_G:
                ++Type[kTypeGIndex];
                break;
            case Astro::ESpectralClass::kSpectral_K:
                ++Type[kTypeKIndex];
                break;
            case Astro::ESpectralClass::kSpectral_M:
                ++Type[kTypeMIndex];
                break;
            }
        };

        auto CountMostLuminous = [](const std::unique_ptr<Astro::AStar>& Star, MostLuminous& StartStat)
        {
            double Value = Star->GetLuminosity() / kSolarLuminosity;
            if (StartStat.Value < Value)
            {
                StartStat.Value = Value;
                StartStat.Star = Star.get();
            }
        };

        auto CountMostMassive = [](const std::unique_ptr<Astro::AStar>& Star, MostMassive& StartStat)
        {
            double Value = Star->GetMass() / kSolarMass;
            if (StartStat.Value < Value)
            {
                StartStat.Value = Value;
                StartStat.Star = Star.get();
            }
        };

        auto CountLargest = [](const std::unique_ptr<Astro::AStar>& Star, Largest& StartStat)
        {
            float Value = Star->GetRadius() / kSolarRadius;
            if (StartStat.Value < Value)
            {
                StartStat.Value = Value;
                StartStat.Star = Star.get();
            }
        };

        auto CountHottest = [](const std::unique_ptr<Astro::AStar>& Star, Hottest& StartStat)
        {
            float Value = Star->GetTeff();
            if (StartStat.Value < Value)
            {
                StartStat.Value = Value;
                StartStat.Star = Star.get();
            }
        };

        auto CountOldest = [](const std::unique_ptr<Astro::AStar>& Star, Oldest& StartStat)
        {
            double Value = Star->GetAge();
            if (StartStat.Value < Value)
            {
                StartStat.Value = Value;
                StartStat.Star = Star.get();
            }
        };

        auto CountMostOblateness = [](const std::unique_ptr<Astro::AStar>& Star, MostOblateness& StartStat)
        {
            float Value = Star->GetOblateness();
            if (StartStat.Value < Value)
            {
                StartStat.Value = Value;
                StartStat.Star = Star.get();
            }
        };

        auto CountMostMagnetic = [](const std::unique_ptr<Astro::AStar>& Star, MostMagnetic& StartStat)
        {
            float Value = Star->GetMagneticField() * 10000;
            if (StartStat.Value < Value)
            {
                StartStat.Value = Value;
                StartStat.Star = Star.get();
            }
        };

        auto CountMostMdot = [](const std::unique_ptr<Astro::AStar>& Star, MostMdot& StartStat)
        {
            double Value = Star->GetStellarWindMassLossRate() * kYearToSecond / kSolarMass;
            if (StartStat.Value < Value)
            {
                StartStat.Value = Value;
                StartStat.Star = Star.get();
            }
        };

        // 声明统计变量
#define DECLARE_STATS(Type, Prefix)     \
            Type Prefix##MainSequence;  \
            Type Prefix##Subgiant;      \
            Type Prefix##Giant;         \
            Type Prefix##BrightGiant;   \
            Type Prefix##Supergiant;    \
            Type Prefix##Hypergiant;    \
            Type Prefix##WolfRayet;     \
            Type Prefix##OMainSequence; \
            Type Prefix##OGiant;        \
            Type Prefix##OfGiant;       \
            Type Prefix##OSupergiant;   \
            Type Prefix##OfSupergiant;

        DECLARE_STATS(MostLuminous, MostLuminous)
        DECLARE_STATS(MostMassive, MostMassive)
        DECLARE_STATS(Largest, Largest)
        DECLARE_STATS(Hottest, Hottest)
        DECLARE_STATS(Oldest, Oldest)
        DECLARE_STATS(MostOblateness, MostOblateness)
        DECLARE_STATS(MostMagnetic, MostMagnetic)
        DECLARE_STATS(MostMdot, MostMdot)

#undef DECLARE_STATS

        std::println("Star statistics results:");
        std::println("{}", FormatTitle());
        std::println("");

        for (auto& System : OrbitalSystems_)
        {
            for (auto& Star : System.StarsData())
            {
                ++TotalStars;

                if (Star->IsSingleStar())
                {
                    ++TotalSingles;
                }
                else
                {
                    ++TotalBinarys;
                }

                const auto& Class = Star->GetStellarClass();
                Astro::EStellarType StellarType = Class.GetStellarType();
                if (StellarType != Astro::EStellarType::kNormalStar)
                {
                    switch (StellarType)
                    {
                    case Astro::EStellarType::kBlackHole:
                        ++BlackHoles;
                        break;
                    case Astro::EStellarType::kNeutronStar:
                        ++NeutronStars;
                        break;
                    case Astro::EStellarType::kWhiteDwarf:
                        ++WhiteDwarfs;
                        break;
                    default:
                        break;
                    }

                    continue;
                }

                Astro::FSpectralType SpectralType = Class.Data();

                // 定义统计更新宏，简化调用
#define UPDATE_ALL_STATS(Suffix)                                       \
                    CountMostLuminous(Star, MostLuminous##Suffix);     \
                    CountMostMassive(Star, MostMassive##Suffix);       \
                    CountLargest(Star, Largest##Suffix);               \
                    CountHottest(Star, Hottest##Suffix);               \
                    CountOldest(Star, Oldest##Suffix);                 \
                    CountMostOblateness(Star, MostOblateness##Suffix); \
                    CountMostMagnetic(Star, MostMagnetic##Suffix);     \
                    CountMostMdot(Star, MostMdot##Suffix);

                // O Type Stars Special Handling (Note: These are only reached if not caught by above checks)
                if (SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_O)
                {
                    bool bIsF = SpectralType.SpecialMarked(Astro::ESpecialMark::kCode_f);

                    if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_V && !bIsF)
                    {
                        CountClass(SpectralType, MainSequences);
                        UPDATE_ALL_STATS(OMainSequence)
                    }
                    else if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_III && !bIsF)
                    {
                        CountClass(SpectralType, Giants);
                        UPDATE_ALL_STATS(OGiant)
                    }
                    else if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_I && !bIsF)
                    {
                        CountClass(SpectralType, Supergiants);
                        UPDATE_ALL_STATS(OSupergiant)
                    }
                    else if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_III && bIsF)
                    {
                        CountClass(SpectralType, Giants);
                        UPDATE_ALL_STATS(OfGiant)
                    }
                    else if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_I && bIsF)
                    {
                        CountClass(SpectralType, Hypergiants); // Assuming mapping based on original logic logic
                        UPDATE_ALL_STATS(OfSupergiant)
                    }
                }

                if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_Unknown)
                {
                    if (SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_WC ||
                        SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_WN ||
                        SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_WO)
                    {
                        ++WolfRayet;
                        UPDATE_ALL_STATS(WolfRayet)
                        continue;
                    }
                }

                if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_0 ||
                    SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_IaPlus)
                {
                    CountClass(SpectralType, Hypergiants);
                    UPDATE_ALL_STATS(Hypergiant)
                    continue;
                }

                if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_Ia ||
                    SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_Iab ||
                    SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_Ib)
                {
                    CountClass(SpectralType, Supergiants);
                    UPDATE_ALL_STATS(Supergiant)
                    continue;
                }

                if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_II)
                {
                    CountClass(SpectralType, BrightGiants);
                    UPDATE_ALL_STATS(BrightGiant)
                    continue;
                }

                if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_III)
                {
                    CountClass(SpectralType, Giants);
                    UPDATE_ALL_STATS(Giant)
                    continue;
                }

                if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_IV)
                {
                    CountClass(SpectralType, Subgiants);
                    UPDATE_ALL_STATS(Subgiant)
                    continue;
                }

                bool Fuck;

                if (SpectralType.LuminosityClass == Astro::ELuminosityClass::kLuminosity_V)
                {
                    if (SpectralType.HSpectralClass == Astro::ESpectralClass::kSpectral_O)
                        Fuck = true;
                    CountClass(SpectralType, MainSequences);
                    UPDATE_ALL_STATS(MainSequence)
                    continue;
                }

#undef UPDATE_ALL_STATS
            }
        }

        // 打印结果辅助函数
        auto PrintStatsInfo = [&](std::string_view Title, std::string_view ValueName, const auto& Items)
        {
            for (const auto& [Category, Item] : Items)
            {
                std::println("{} {} star: {}: {}", Title, Category, ValueName, Item->Value);
                std::println("{}", FormatInfo(Item->Star));
            }
            std::println("");
        };

        // 宏定义：构建统计项列表
#define MAKE_STAT_LIST(Prefix)                                 \
            std::vector<std::pair<std::string_view, const decltype(Prefix##MainSequence)*>> \
            {                                                  \
                { "main sequence",   &Prefix##MainSequence },  \
                { "Wolf-Rayet",      &Prefix##WolfRayet },     \
                { "subgiant",        &Prefix##Subgiant },      \
                { "giant",           &Prefix##Giant },         \
                { "bright giant",    &Prefix##BrightGiant },   \
                { "supergiant",      &Prefix##Supergiant },    \
                { "hypergiant",      &Prefix##Hypergiant },    \
                { "O main sequence", &Prefix##OMainSequence }, \
                { "O giant",         &Prefix##OGiant },        \
                { "Of giant",        &Prefix##OfGiant },       \
                { "O supergiant",    &Prefix##OSupergiant },   \
                { "Of supergiant",   &Prefix##OfSupergiant }   \
            }

        // 使用辅助函数替换海量 println
        PrintStatsInfo("Most luminous", "luminosity", MAKE_STAT_LIST(MostLuminous));
        PrintStatsInfo("Most massive", "mass", MAKE_STAT_LIST(MostMassive));
        PrintStatsInfo("Largest", "radius", MAKE_STAT_LIST(Largest));
        PrintStatsInfo("Hottest", "Teff", MAKE_STAT_LIST(Hottest));
        PrintStatsInfo("Oldest", "Age", MAKE_STAT_LIST(Oldest));
        PrintStatsInfo("Most oblateness", "Oblateness", MAKE_STAT_LIST(MostOblateness));
        PrintStatsInfo("Most magnetic", "MagneticField", MAKE_STAT_LIST(MostMagnetic));
        PrintStatsInfo("Most Mdot", "Mdot", MAKE_STAT_LIST(MostMdot));

#undef MAKE_STAT_LIST

        std::size_t TotalMainSequence = 0;
        for (std::size_t Count : MainSequences)
        {
            TotalMainSequence += Count;
        }

        std::println("");
        std::println("Total main sequence: {}", TotalMainSequence);
        std::println("Total main sequence rate: {}", TotalMainSequence / static_cast<double>(TotalStars));
        std::println("Total O type star rate: {}", static_cast<double>(MainSequences[kTypeOIndex]) / static_cast<double>(TotalMainSequence));
        std::println("Total B type star rate: {}", static_cast<double>(MainSequences[kTypeBIndex]) / static_cast<double>(TotalMainSequence));
        std::println("Total A type star rate: {}", static_cast<double>(MainSequences[kTypeAIndex]) / static_cast<double>(TotalMainSequence));
        std::println("Total F type star rate: {}", static_cast<double>(MainSequences[kTypeFIndex]) / static_cast<double>(TotalMainSequence));
        std::println("Total G type star rate: {}", static_cast<double>(MainSequences[kTypeGIndex]) / static_cast<double>(TotalMainSequence));
        std::println("Total K type star rate: {}", static_cast<double>(MainSequences[kTypeKIndex]) / static_cast<double>(TotalMainSequence));
        std::println("Total M type star rate: {}", static_cast<double>(MainSequences[kTypeMIndex]) / static_cast<double>(TotalMainSequence));
        std::println("Total Wolf-Rayet / O main star rate: {}", static_cast<double>(WolfRayet) / static_cast<double>(MainSequences[kTypeOIndex]));

        std::println("O type main sequence: {}\nB type main sequence: {}\nA type main sequence: {}\nF type main sequence: {}\nG type main sequence: {}\nK type main sequence: {}\nM type main sequence: {}",
                     MainSequences[kTypeOIndex], MainSequences[kTypeBIndex], MainSequences[kTypeAIndex], MainSequences[kTypeFIndex], MainSequences[kTypeGIndex], MainSequences[kTypeKIndex], MainSequences[kTypeMIndex]);
        std::println("O type subgiants: {}\nB type subgiants: {}\nA type subgiants: {}\nF type subgiants: {}\nG type subgiants: {}\nK type subgiants: {}\nM type subgiants: {}",
                     Subgiants[kTypeOIndex], Subgiants[kTypeBIndex], Subgiants[kTypeAIndex], Subgiants[kTypeFIndex], Subgiants[kTypeGIndex], Subgiants[kTypeKIndex], Subgiants[kTypeMIndex]);
        std::println("O type giants: {}\nB type giants: {}\nA type giants: {}\nF type giants: {}\nG type giants: {}\nK type giants: {}\nM type giants: {}",
                     Giants[kTypeOIndex], Giants[kTypeBIndex], Giants[kTypeAIndex], Giants[kTypeFIndex], Giants[kTypeGIndex], Giants[kTypeKIndex], Giants[kTypeMIndex]);
        std::println("O type bright giants: {}\nB type bright giants: {}\nA type bright giants: {}\nF type bright giants: {}\nG type bright giants: {}\nK type bright giants: {}\nM type bright giants: {}",
                     BrightGiants[kTypeOIndex], BrightGiants[kTypeBIndex], BrightGiants[kTypeAIndex], BrightGiants[kTypeAIndex], BrightGiants[kTypeFIndex], BrightGiants[kTypeGIndex], BrightGiants[kTypeMIndex]);
        std::println("O type supergiants: {}\nB type supergiants: {}\nA type supergiants: {}\nF type supergiants: {}\nG type supergiants: {}\nK type supergiants: {}\nM type supergiants: {}",
                     Supergiants[kTypeOIndex], Supergiants[kTypeBIndex], Supergiants[kTypeAIndex], Supergiants[kTypeFIndex], Supergiants[kTypeGIndex], Supergiants[kTypeKIndex], Supergiants[kTypeMIndex]);
        std::println("O type hypergiants: {}\nB type hypergiants: {}\nA type hypergiants: {}\nF type hypergiants: {}\nG type hypergiants: {}\nK type hypergiants: {}\nM type hypergiants: {}",
                     Hypergiants[kTypeOIndex], Hypergiants[kTypeBIndex], Hypergiants[kTypeAIndex], Hypergiants[kTypeFIndex], Hypergiants[kTypeGIndex], Hypergiants[kTypeKIndex], Hypergiants[kTypeMIndex]);
        std::println("Wolf-Rayet stars: {}", WolfRayet);
        std::println("White dwarfs: {}\nNeutron stars: {}\nBlack holes: {}", WhiteDwarfs, NeutronStars, BlackHoles);
        std::println("");
        std::println("Number of single stars: {}", TotalSingles);
        std::println("Number of binary stars: {}", TotalBinarys);
        std::println("");
    }

    void FUniverse::GenerateStars(int MaxThread)
    {
        NpgsCoreInfo("Initializating and generating basic properties...");
        std::vector<FStellarGenerator> Generators;
        std::vector<FStellarBasicProperties> BasicProperties;

        auto CreateGenerators =
        [&, this](EStellarTypeGenerationOption StellarTypeOption = EStellarTypeGenerationOption::kRandom,
                  float MassLowerLimit = 0.1f, float MassUpperLimit = 300.0f,
                  EGenerationDistribution MassDistribution = EGenerationDistribution::kFromPdf,
                  float AgeLowerLimit = 0.0f, float AgeUpperLimit = 1.26e10f,
                  EGenerationDistribution AgeDistribution = EGenerationDistribution::kFromPdf,
                  float FeHLowerLimit = -4.0f, float FeHUpperLimit = 0.5f,
                  EGenerationDistribution FeHDistribution = EGenerationDistribution::kFromPdf) -> void
        {
            for (int i = 0; i != MaxThread; ++i)
            {
                std::vector<std::uint32_t> Seeds(32);
                for (int i = 0; i != 32; ++i)
                {
                    Seeds[i] = SeedGenerator_(RandomEngine_);
                }

                std::ranges::shuffle(Seeds, RandomEngine_);
                std::seed_seq SeedSequence(Seeds.begin(), Seeds.end());

                FStellarGenerationInfo GenerationInfo
                {
                    .SeedSequence       = &SeedSequence,
                    .StellarTypeOption  = StellarTypeOption,
                    .MultiplicityOption = EMultiplicityGenerationOption::kSingleStar,
                    .UniverseAge        = UniverseAge_,
                    .MassLowerLimit     = MassLowerLimit,
                    .MassUpperLimit     = MassUpperLimit,
                    .MassDistribution   = MassDistribution,
                    .AgeLowerLimit      = AgeLowerLimit,
                    .AgeUpperLimit      = AgeUpperLimit,
                    .AgeDistribution    = AgeDistribution,
                    .FeHLowerLimit      = FeHLowerLimit,
                    .FeHUpperLimit      = FeHUpperLimit,
                    .FeHDistribution    = FeHDistribution
                };

                Generators.emplace_back(GenerationInfo);
            }
        };

        // 生成基础属性
        auto GenerateBasicProperties = [&](std::size_t NumStars) -> void
        {
            for (std::size_t i = 0; i != NumStars; ++i)
            {
                std::size_t ThreadId = i % Generators.size();
                BasicProperties.push_back(Generators[ThreadId].GenerateBasicProperties());
            }
        };

        // 特殊星基础参数设置
        if (ExtraGiantCount_ != 0)
        {
            Generators.clear();
            CreateGenerators(EStellarTypeGenerationOption::kGiant, 1.0f, 35.0f);
            GenerateBasicProperties(ExtraGiantCount_);
        }

        if (ExtraMassiveStarCount_ != 0)
        {
            Generators.clear();
            CreateGenerators(EStellarTypeGenerationOption::kRandom,
                             20.0f, 300.0f, EGenerationDistribution::kUniform,
                             0.0f,  3.5e6f, EGenerationDistribution::kUniform);
            GenerateBasicProperties(ExtraMassiveStarCount_);
        }

        if (ExtraNeutronStarCount_ != 0)
        {
            Generators.clear();
            CreateGenerators(EStellarTypeGenerationOption::kDeathStar,
                             10.0f, 20.0f, EGenerationDistribution::kUniform,
                             1e7f,   1e8f, EGenerationDistribution::kUniformByExponent);
            GenerateBasicProperties(ExtraNeutronStarCount_);
        }

        if (ExtraBlackHoleCount_ != 0)
        {
            Generators.clear();
            CreateGenerators(EStellarTypeGenerationOption::kRandom,
                             35.0f,  300.0f, EGenerationDistribution::kUniform,
                             1e7f, 1.26e10f, EGenerationDistribution::kFromPdf, -2.0, 0.5);
            GenerateBasicProperties(ExtraBlackHoleCount_);
        }

        if (ExtraMergeStarCount_ != 0)
        {
            Generators.clear();
            CreateGenerators(EStellarTypeGenerationOption::kMergeStar,
                             0.0f, 0.0f, EGenerationDistribution::kUniform,
                             1e6f, 1e8f, EGenerationDistribution::kUniformByExponent);
            GenerateBasicProperties(ExtraMergeStarCount_);
        }

        std::size_t CommonStarsCount =
            StarCount_ - ExtraGiantCount_ - ExtraMassiveStarCount_ - ExtraNeutronStarCount_ - ExtraBlackHoleCount_ - ExtraMergeStarCount_;

        Generators.clear();
        CreateGenerators(EStellarTypeGenerationOption::kRandom, 0.075f);
        GenerateBasicProperties(CommonStarsCount);

        NpgsCoreInfo("Interpolating stellar data as {} threads...", MaxThread);

        std::vector<Astro::AStar> Stars = InterpolateStars(MaxThread, Generators, BasicProperties);

        NpgsCoreInfo("Generating stellar slots...");
        GenerateSlots(0.1f, StarCount_, 0.004f);

        NpgsCoreInfo("Linking positions in octree to stellar systems...");
        OrbitalSystems_.reserve(StarCount_);
        std::ranges::shuffle(Stars, RandomEngine_);
        std::vector<glm::vec3> Slots;
        OctreeLinkToStellarSystems(Stars, Slots);

        NpgsCoreInfo("Generating binary stars...");
        GenerateBinaryStars(MaxThread);

        NpgsCoreInfo("Sorting...");
        std::ranges::sort(Slots, [](glm::vec3 Point1, glm::vec3 Point2)
        {
            return glm::length(Point1) < glm::length(Point2);
        });

        NpgsCoreInfo("Assigning name...");
        std::string Name;
        std::ostringstream Stream;
        for (auto& System : OrbitalSystems_)
        {
            glm::vec3 Position = System.GetBaryPosition();
            auto it = std::ranges::lower_bound(Slots, Position, [](glm::vec3 Point1, glm::vec3 Point2) -> bool
            {
                return glm::length(Point1) < glm::length(Point2);
            });
            std::ptrdiff_t Offset = it - Slots.begin();
            Stream << std::setfill('0') << std::setw(8) << std::to_string(Offset);
            Name = "SYSTEM-" + Stream.str();
            System.SetBaryName(Name).SetBaryDistanceRank(Offset);

            auto& Stars = System.StarsData();
            if (Stars.size() > 1)
            {
                std::ranges::sort(Stars, [](const std::unique_ptr<Astro::AStar>& Star1, std::unique_ptr<Astro::AStar>& Star2) -> bool
                {
                    return Star1->GetMass() > Star2->GetMass();
                });

                char Rank = 'A';
                for (auto& Star : Stars)
                {
                    Star->SetName("STAR-" + Stream.str() + " " + Rank);
                    ++Rank;
                }
            }
            else
            {
                Stars.front()->SetName("STAR-" + Stream.str());
            }

            Stream.str("");
            Stream.clear();
        }

        NpgsCoreInfo("Reset home stellar system...");
        FNodeType* HomeNode = Octree_->Find(glm::vec3(0.0f), [](const FNodeType& Node) -> bool
        {
            if (Node.IsLeafNode())
            {
                auto& Points = Node.GetPoints();
                return Node.GetLink([](void*) -> bool { return true; }) != nullptr &&
                    std::ranges::find(Points, glm::vec3(0.0f)) != Points.end();
            }
            else
            {
                return false;
            }
        });

        // TODO: Fix when home star not at home grid
        auto* HomeSystem = HomeNode->GetLink([](Astro::FOrbitalSystem* System) -> bool
        {
            return System->GetBaryPosition() == glm::vec3(0.0f);
        });
        HomeNode->RemoveStorage();
        HomeNode->AddPoint(glm::vec3(0.0f));
        HomeSystem->SetBaryNormal(glm::vec2(0.0f));

        for (auto& Star : HomeSystem->StarsData())
        {
            Star->SetNormal(glm::vec3(0.0f));
        }

        NpgsCoreInfo("Stellar generation completed.");
    }

    void FUniverse::FillStellarSystem(int MaxThread)
    {
        NpgsCoreInfo("Generating planets...");

        std::vector<FOrbitalGenerator> Generators;

        for (int i = 0; i != MaxThread; ++i)
        {
            std::vector<std::uint32_t> Seeds(32);
            for (int i = 0; i != 32; ++i)
            {
                Seeds[i] = SeedGenerator_(RandomEngine_);
            }

            std::ranges::shuffle(Seeds, RandomEngine_);
            std::seed_seq SeedSequence(Seeds.begin(), Seeds.end());

            FOrbitalGenerationInfo GenerationInfo;
            GenerationInfo.SeedSequence = &SeedSequence;
            Generators.emplace_back(GenerationInfo);
        }
    }

    std::vector<Astro::AStar>
    FUniverse::InterpolateStars(int MaxThread, std::vector<FStellarGenerator>& Generators,
                                std::vector<FStellarBasicProperties>& BasicProperties)
    {
        std::vector<std::vector<FStellarBasicProperties>> PropertyLists(MaxThread);
        std::vector<std::promise<std::vector<Astro::AStar>>> Promises(MaxThread);
        std::vector<std::future<std::vector<Astro::AStar>>> ChunkFutures;

        MakeChunks(MaxThread, BasicProperties, PropertyLists, Promises, ChunkFutures);

        for (int i = 0; i != MaxThread; ++i)
        {
            ThreadPool_->Submit([&, i]() -> void
            {
                std::vector<Astro::AStar> Stars;
                for (auto& Properties : PropertyLists[i])
                {
                    Stars.push_back(Generators[i].GenerateStar(Properties));
                }
                Promises[i].set_value(std::move(Stars));
            });
        }

        BasicProperties.clear();

        std::vector<Astro::AStar> Stars;
        for (auto& Future : ChunkFutures)
        {
            auto Chunk = Future.get();
            Stars.append_range(Chunk | std::views::as_rvalue);
        }

        return Stars;
    }

    void FUniverse::GenerateSlots(float MinDistance, std::size_t SampleCount, float Density)
    {
        float Radius     = std::pow((3.0f * SampleCount / (4 * Math::kPi * Density)), (1.0f / 3.0f));
        float LeafSize   = std::pow((1.0f / Density), (1.0f / 3.0f));
        int   Exponent   = static_cast<int>(std::ceil(std::log2(Radius / LeafSize)));
        float LeafRadius = LeafSize * 0.5f;
        float RootRadius = LeafSize * static_cast<float>(std::pow(2, Exponent));

        NpgsCoreInfo("Initializing octree...");
        Octree_ = std::make_unique<TOctree<Astro::FOrbitalSystem>>(glm::vec3(0.0), RootRadius);
        NpgsCoreInfo("Building empty octree...");
        Octree_->BuildEmptyTree(LeafRadius); // 快速构建一个空树，每个叶子节点作为一个格子，用于生成恒星

        // 遍历八叉树，将距离原点大于半径的叶子节点标记为无效，保证恒星只会在范围内生成
        NpgsCoreInfo("Traversing octree to generate slots...");
        Octree_->Traverse([Radius](FNodeType& Node) -> void
        {
            if (Node.IsLeafNode() && glm::length(Node.GetCenter()) > Radius)
            {
                Node.SetValidation(false);
            }
        });

        NpgsCoreInfo("Get valid leafs");
        std::size_t ValidLeafCount = Octree_->GetCapacity();
        std::vector<FNodeType*> LeafNodes;

        auto CollectLeafNodes = [&LeafNodes](FNodeType& Node) -> void
        {
            if (Node.IsLeafNode())
            {
                LeafNodes.push_back(&Node);
            }
        };

        // 使用栅格采样，八叉树的每个叶子节点作为一个格子，在这个格子中生成一个恒星
        NpgsCoreInfo("Sampling slots...");
        while (ValidLeafCount != SampleCount)
        {
            LeafNodes.clear();
            Octree_->Traverse(CollectLeafNodes);
            std::ranges::shuffle(LeafNodes, RandomEngine_); // 打乱叶子节点，保证随机性

            // 删除或收回叶子节点，直到格子数量等于目标数量
            if (ValidLeafCount < SampleCount)
            {
                for (auto* Node : LeafNodes)
                {
                    glm::vec3 Center = Node->GetCenter();
                    float Distance = glm::length(Center);
                    if (!Node->IsValid() && Distance >= Radius && Distance <= Radius + LeafRadius)
                    {
                        Node->SetValidation(true);
                        if (++ValidLeafCount == SampleCount)
                        {
                            break;
                        }
                    }
                }
            }
            else
            {
                for (auto* Node : LeafNodes)
                {
                    glm::vec3 Center = Node->GetCenter();
                    float Distance = glm::length(Center);
                    if (Node->IsValid() && Distance >= Radius - LeafRadius && Distance <= Radius)
                    {
                        Node->SetValidation(false);
                        if (--ValidLeafCount == SampleCount)
                        {
                            break;
                        }
                    }
                }
            }
        }

        Math::TUniformRealDistribution Offset(-LeafRadius, LeafRadius - MinDistance); // 用于随机生成恒星位置相对于叶子节点中心点的偏移量
        // 遍历八叉树，为每个有效的叶子节点生成一个恒星
        Octree_->Traverse([&Offset, this](FNodeType& Node) -> void
        {
            if (Node.IsLeafNode() && Node.IsValid())
            {
                glm::vec3 Center(Node.GetCenter());
                glm::vec3 StellarSlot(Center.x + Offset(RandomEngine_),
                                      Center.y + Offset(RandomEngine_),
                                      Center.z + Offset(RandomEngine_));
                Node.AddPoint(StellarSlot);
            }
        });

        // 为了保证恒星系统的唯一性，将原点附近所在的叶子节点作为存储初始恒星系统的结点
        // 寻找包含了 (LeafRadius, LeafRadius, LeafRadius) 的叶子节点，将这个格子存储的位置修改为原点
        FNodeType* HomeNode = Octree_->Find(glm::vec3(LeafRadius), [](const FNodeType& Node) -> bool
        {
            return Node.IsLeafNode();
        });

        // 把最靠近原点的格子存储旧的位置点删除，加入初始恒星系统
        HomeNode->RemoveStorage();
        HomeNode->AddPoint(glm::vec3(0.0f));
    }

    void FUniverse::OctreeLinkToStellarSystems(std::vector<Astro::AStar>& Stars, std::vector<glm::vec3>& Slots)
    {
        std::size_t Index = 0;

        Octree_->Traverse([&](FNodeType& Node) -> void
        {
            if (Node.IsLeafNode() && Node.IsValid())
            {
                for (const auto& Point : Node.GetPoints())
                {
                    Astro::FBaryCenter NewBary(Point, glm::vec2(0.0f), 0, "");
                    Astro::FOrbitalSystem NewSystem(NewBary);
                    NewSystem.StarsData().push_back(std::make_unique<Astro::AStar>(Stars.back()));
                    NewSystem.SetBaryNormal(NewSystem.StarsData().front()->GetNormal());
                    Stars.pop_back();

                    OrbitalSystems_.push_back(std::move(NewSystem));

                    Node.AddLink(&OrbitalSystems_[Index]);
                    Slots.push_back(Point);
                    ++Index;
                }
            }
        });
    }

    void FUniverse::GenerateBinaryStars(int MaxThread)
    {
        std::vector<FStellarGenerator> Generators;
        for (int i = 0; i != MaxThread; ++i)
        {
            std::vector<std::uint32_t> Seeds(32);
            for (int j = 0; j != 32; ++j)
            {
                Seeds[j] = SeedGenerator_(RandomEngine_);
            }

            std::ranges::shuffle(Seeds, RandomEngine_);
            std::seed_seq SeedSequence(Seeds.begin(), Seeds.end());

            FStellarGenerationInfo GenerationInfo
            {
                .SeedSequence       = &SeedSequence,
                .StellarTypeOption  = EStellarTypeGenerationOption::kRandom,
                .MultiplicityOption = EMultiplicityGenerationOption::kBinarySecondStar
            };

            Generators.emplace_back(GenerationInfo);
        }

        std::vector<Astro::FOrbitalSystem*> BinarySystems;
        for (auto& System : OrbitalSystems_)
        {
            const auto& Star = System.StarsData().front();
            if (!Star->IsSingleStar())
            {
                BinarySystems.push_back(&System);
            }
        }

        std::vector<FStellarBasicProperties> BasicProperties;
        for (std::size_t i = 0; i != BinarySystems.size(); ++i)
        {
            std::size_t ThreadId    = i % MaxThread;
            auto& SelectedGenerator = Generators[ThreadId];

            const auto& Star = BinarySystems[i]->StarsData().front();
            float FirstStarInitialMassSol = Star->GetInitialMass() / kSolarMass;
            float MassLowerLimit = std::max(0.075f, 0.1f * FirstStarInitialMassSol);
            float MassUpperLimit = std::min(10 * FirstStarInitialMassSol, 300.0f);

            SelectedGenerator.SetMassLowerLimit(MassLowerLimit);
            SelectedGenerator.SetMassUpperLimit(MassUpperLimit);
            SelectedGenerator.SetLogMassSuggestDistribution(
                std::make_unique<Math::TNormalDistribution<>>(std::log10(FirstStarInitialMassSol), 0.25f));

            double Age = Star->GetAge();
            float  FeH = Star->GetFeH();

            if (std::to_underlying(Star->GetEvolutionPhase()) > 10)
            {
                Age -= Star->GetLifetime();
            }

            BasicProperties.push_back(SelectedGenerator.GenerateBasicProperties(static_cast<float>(Age), FeH));
        }

        std::vector<Astro::AStar> Stars = InterpolateStars(MaxThread, Generators, BasicProperties);

        for (std::size_t i = 0; i != BinarySystems.size(); ++i)
        {
            BinarySystems[i]->StarsData().push_back(std::make_unique<Astro::AStar>(Stars[i]));
        }
    }
} // namespace Npgs
