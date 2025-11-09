#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <shared_mutex>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/Core/Math/NumericConstants.hpp"
#include "Engine/Core/Runtime/AssetLoaders/CommaSeparatedValues.hpp"
#include "Engine/Core/Types/Entries/Astro/Star.hpp"
#include "Engine/Core/Types/Properties/StellarClass.hpp"
#include "Engine/Utils/Random.hpp"

namespace Npgs
{
    struct FStellarBasicProperties;
    struct FStellarGenerationInfo;
    class FStellarGenerator
    {
    public:
        using FMistData   = TCommaSeparatedValues<double, 12>;
        using FWdMistData = TCommaSeparatedValues<double, 5>;
        using FHrDiagram  = TCommaSeparatedValues<double, 7>;
        using FDataArray  = std::vector<double>;

        enum class EGenerationDistribution : std::uint8_t
        {
            kFromPdf,
            kUniform,
            kUniformByExponent
        };

        enum class EStellarTypeGenerationOption : std::uint8_t
        {
            kRandom,
            kGiant,
            kDeathStar,
            kMergeStar,
        };

        enum class EMultiplicityGenerationOption : std::uint8_t
        {
            kSingleStar,
            kBinaryFirstStar,
            kBinarySecondStar
        };

    public:
        FStellarGenerator(const FStellarGenerationInfo& GenerationInfo);
        FStellarGenerator(const FStellarGenerator& Other);
        FStellarGenerator(FStellarGenerator&& Other) noexcept;
        ~FStellarGenerator() = default;

        FStellarGenerator& operator=(const FStellarGenerator& Other);
        FStellarGenerator& operator=(FStellarGenerator&& Other) noexcept;

        FStellarBasicProperties GenerateBasicProperties(float Age = std::numeric_limits<float>::quiet_NaN(),
                                                        float FeH = std::numeric_limits<float>::quiet_NaN());

        Astro::AStar GenerateStar();
        Astro::AStar GenerateStar(FStellarBasicProperties& Properties);

        FStellarGenerator& SetLogMassSuggestDistribution(std::unique_ptr<Utils::TDistribution<>>&& Distribution);
        FStellarGenerator& SetUniverseAge(float Age);
        FStellarGenerator& SetAgeLowerLimit(float Limit);
        FStellarGenerator& SetAgeUpperLimit(float Limit);
        FStellarGenerator& SetFeHLowerLimit(float Limit);
        FStellarGenerator& SetFeHUpperLimit(float Limit);
        FStellarGenerator& SetMassLowerLimit(float Limit);
        FStellarGenerator& SetMassUpperLimit(float Limit);
        FStellarGenerator& SetCoilTempLimit(float Limit);
        FStellarGenerator& SetdEpdM(float dEpdM);
        FStellarGenerator& SetAgePdf(std::function<float(glm::vec3, float, float)> AgePdf);
        FStellarGenerator& SetAgeMaxPdf(glm::vec2 MaxPdf);
        FStellarGenerator& SetMassPdfs(std::array<std::function<float(float)>, 2> MassPdfs);
        FStellarGenerator& SetMassMaxPdfs(std::array<glm::vec2, 2> MaxPdfs);
        FStellarGenerator& SetAgeDistribution(EGenerationDistribution Distribution);
        FStellarGenerator& SetFeHDistribution(EGenerationDistribution Distribution);
        FStellarGenerator& SetMassDistribution(EGenerationDistribution Distribution);
        FStellarGenerator& SetStellarTypeGenerationOption(EStellarTypeGenerationOption Option);

    private:
        template <typename CsvType>
        requires std::is_class_v<CsvType>
        CsvType* LoadCsvAsset(const std::string& Filename, std::span<const std::string> Headers);

        void InitializeMistData();
        void InitializePdfs();
        float GenerateAge(float MaxPdf);
        float GenerateMass(float MaxPdf, auto& LogMassPdf);
        
        std::expected<FDataArray, Astro::AStar>
        GetFullMistData(const FStellarBasicProperties& Properties, bool bIsWhiteDwarf, bool bIsSingleWhiteDwarf);
        
        std::expected<FDataArray, Astro::AStar>
        InterpolateMistData(const std::pair<std::string, std::string>& Files, double TargetAge, double TargetMassSol, double MassCoefficient);
        
        std::vector<FDataArray> FindPhaseChanges(const FMistData* DataSheet);

        std::expected<double, Astro::AStar>
        CalculateEvolutionProgress(std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& PhaseChanges, double TargetAge, double MassCoefficient);

        std::pair<double, std::pair<double, double>>
        FindSurroundingTimePoints(const std::vector<FDataArray>& PhaseChanges, double TargetAge);

        std::expected<std::pair<double, std::size_t>, Astro::AStar>
        FindSurroundingTimePoints(const std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& PhaseChanges, double TargetAge, double MassCoefficient);

        void AlignArrays(std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& Arrays);
        FDataArray InterpolateHrDiagram(FHrDiagram* Data, double BvColorIndex);
        FDataArray InterpolateStarData(FMistData* Data, double EvolutionProgress);
        FDataArray InterpolateStarData(FWdMistData* Data, double TargetAge);
        FDataArray InterpolateStarData(auto* Data, double Target, const std::string& Header, int Index, bool bIsWhiteDwarf);
        FDataArray InterpolateArray(const std::pair<FDataArray, FDataArray>& DataArrays, double Coefficient);
        FDataArray InterpolateFinalData(const std::pair<FDataArray, FDataArray>& DataArrays, double Coefficient, bool bIsWhiteDwarf);

        void CalculateSpectralType(float FeH, Astro::AStar& StarData);
        Astro::FStellarClass::ELuminosityClass CalculateLuminosityClass(const Astro::AStar& StarData);
        void ProcessDeathStar(EStellarTypeGenerationOption DeathStarTypeOption, Astro::AStar& DeathStar);
        void GenerateMagnetic(Astro::AStar& StarData);
        void GenerateSpin(Astro::AStar& StarData);
        void ExpandMistData(double TargetMassSol, FDataArray& StarData);

    public:
        static constexpr int kStarAgeIndex_        = 0;
        static constexpr int kStarMassIndex_       = 1;
        static constexpr int kStarMdotIndex_       = 2;
        static constexpr int kLogTeffIndex_        = 3;
        static constexpr int kLogRIndex_           = 4;
        static constexpr int kLogSurfZIndex_       = 5;
        static constexpr int kSurfaceH1Index_      = 6;
        static constexpr int kSurfaceHe3Index_     = 7;
        static constexpr int kLogCenterTIndex_     = 8;
        static constexpr int kLogCenterRhoIndex_   = 9;
        static constexpr int kPhaseIndex_          = 10;
        static constexpr int kXIndex_              = 11;
        static constexpr int kLifetimeIndex_       = 12;

        static constexpr int kWdStarAgeIndex_      = 0;
        static constexpr int kWdLogRIndex_         = 1;
        static constexpr int kWdLogTeffIndex_      = 2;
        static constexpr int kWdLogCenterTIndex_   = 3;
        static constexpr int kWdLogCenterRhoIndex_ = 4;

    private:
        std::mt19937                                           RandomEngine_;
        std::array<Utils::TUniformRealDistribution<>,       8> MagneticGenerators_;
        std::array<std::unique_ptr<Utils::TDistribution<>>, 4> FeHGenerators_;
        std::array<Utils::TUniformRealDistribution<>,       2> SpinGenerators_;
        Utils::TUniformRealDistribution<>                      AgeGenerator_;
        Utils::TUniformRealDistribution<>                      CommonGenerator_;
        std::unique_ptr<Utils::TDistribution<>>                LogMassGenerator_;

        std::array<std::function<float(float)>, 2>    MassPdfs_;
        std::array<glm::vec2, 2>                      MassMaxPdfs_;
        std::function<float(glm::vec3, float, float)> AgePdf_;
        glm::vec2                                     AgeMaxPdf_;

        float UniverseAge_;
        float AgeLowerLimit_;
        float AgeUpperLimit_;
        float FeHLowerLimit_;
        float FeHUpperLimit_;
        float MassLowerLimit_;
        float MassUpperLimit_;
        float CoilTemperatureLimit_;
        float dEpdM_;

        EGenerationDistribution       AgeDistribution_;
        EGenerationDistribution       FeHDistribution_;
        EGenerationDistribution       MassDistribution_;
        EStellarTypeGenerationOption  StellarTypeOption_;
        EMultiplicityGenerationOption MultiplicityOption_;

        static const inline std::array<std::string, 12> kMistHeaders_
        {
            "star_age",
            "star_mass",
            "star_mdot",
            "log_Teff",
            "log_R",
            "log_surf_z",
            "surface_h1",
            "surface_he3",
            "log_center_T",
            "log_center_Rho",
            "phase",
            "x"
        };

        static const inline std::array<std::string, 5> kWdMistHeaders_
        {
            "star_age",
            "log_R",
            "log_Teff",
            "log_center_T",
            "log_center_Rho"
        };

        static const inline std::array<std::string, 7> kHrDiagramHeaders_
        {
            "B-V", "Ia", "Ib", "II", "III", "IV", "V"
        };

        static std::unordered_map<std::string, std::vector<float>>           MassFilesCache_;
        static std::unordered_map<const FMistData*, std::vector<FDataArray>> PhaseChangesCache_;
        static std::shared_mutex                                             CacheMutex_;
        static bool                                                          bMistDataInitiated_;
    };

    struct FStellarBasicProperties
    {
        // 用于保存生成选项，类的生成选项仅影响该属性。生成的恒星完整信息也将根据该属性决定。该选项用于防止多线程生成恒星时属性和生成器胡乱匹配
        FStellarGenerator::EStellarTypeGenerationOption  StellarTypeOption{ FStellarGenerator::EStellarTypeGenerationOption::kRandom };
        FStellarGenerator::EMultiplicityGenerationOption MultiplicityOption{ FStellarGenerator::EMultiplicityGenerationOption::kSingleStar };

        float Age{};
        float FeH{};
        float InitialMassSol{};
        bool  bIsSingleStar{ true };

        explicit operator Astro::AStar() const;
    };

    struct FStellarGenerationInfo
    {
        const std::seed_seq* SeedSequence{ nullptr };
        FStellarGenerator::EStellarTypeGenerationOption  StellarTypeOption{ FStellarGenerator::EStellarTypeGenerationOption::kRandom };
        FStellarGenerator::EMultiplicityGenerationOption MultiplicityOption{ FStellarGenerator::EMultiplicityGenerationOption::kSingleStar };
        float UniverseAge{ 1.38e10f };
        float MassLowerLimit{ 0.1f };
        float MassUpperLimit{ 300.0f };
        FStellarGenerator::EGenerationDistribution MassDistribution{ FStellarGenerator::EGenerationDistribution::kFromPdf };
        float AgeLowerLimit{ 0.0f };
        float AgeUpperLimit{ 1.26e10f };
        FStellarGenerator::EGenerationDistribution AgeDistribution{ FStellarGenerator::EGenerationDistribution::kFromPdf };
        float FeHLowerLimit{ -4.0f };
        float FeHUpperLimit{ 0.5f };
        FStellarGenerator::EGenerationDistribution FeHDistribution{ FStellarGenerator::EGenerationDistribution::kFromPdf };
        float CoilTemperatureLimit{ 1514.114f };
        float dEpdM{ 2e6f };
        const std::function<float(glm::vec3, float, float)>& AgePdf{ nullptr };
        glm::vec2 AgeMaxPdf{ glm::vec2() };
        const std::array<std::function<float(float)>, 2>& MassPdfs{ nullptr, nullptr };
        std::array<glm::vec2, 2> MassMaxPdfs{ glm::vec2(), glm::vec2() };
    };
} // namespace Npgs

#include "StellarGenerator.inl"
