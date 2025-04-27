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
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/Core/Math/NumericConstants.h"
#include "Engine/Core/Runtime/AssetLoaders/CommaSeparatedValues.hpp"
#include "Engine/Core/Types/Entries/Astro/Star.h"
#include "Engine/Core/Types/Properties/StellarClass.h"
#include "Engine/Utils/Random.hpp"

namespace Npgs
{
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

        struct FBasicProperties
        {
            // 用于保存生成选项，类的生成选项仅影响该属性。生成的恒星完整信息也将根据该属性决定。该选项用于防止多线程生成恒星时属性和生成器胡乱匹配
            EStellarTypeGenerationOption  StellarTypeOption{ EStellarTypeGenerationOption::kRandom };
            EMultiplicityGenerationOption MultiplicityOption{ EMultiplicityGenerationOption::kSingleStar };

            float Age{};
            float FeH{};
            float InitialMassSol{};
            bool  bIsSingleStar{ true };

            explicit operator Astro::AStar() const
            {
                Astro::AStar Star;
                Star.SetAge(Age);
                Star.SetFeH(FeH);
                Star.SetInitialMass(InitialMassSol * kSolarMass);
                Star.SetSingleton(bIsSingleStar);

                return Star;
            }
        };

        struct FGenerationInfo
        {
            const std::seed_seq* SeedSequence{ nullptr };
            EStellarTypeGenerationOption  StellarTypeOption{ EStellarTypeGenerationOption::kRandom };
            EMultiplicityGenerationOption MultiplicityOption{ EMultiplicityGenerationOption::kSingleStar };
            float UniverseAge{ 1.38e10f };
            float MassLowerLimit{ 0.1f };
            float MassUpperLimit{ 300.0f };
            EGenerationDistribution MassDistribution{ EGenerationDistribution::kFromPdf };
            float AgeLowerLimit{ 0.0f };
            float AgeUpperLimit{ 1.26e10f };
            EGenerationDistribution AgeDistribution{ EGenerationDistribution::kFromPdf };
            float FeHLowerLimit{ -4.0f };
            float FeHUpperLimit{ 0.5f };
            EGenerationDistribution FeHDistribution{ EGenerationDistribution::kFromPdf };
            float CoilTemperatureLimit{ 1514.114f };
            float dEpdM{ 2e6f };
            const std::function<float(glm::vec3, float, float)>& AgePdf{ nullptr };
            glm::vec2 AgeMaxPdf{ glm::vec2() };
            const std::array<std::function<float(float)>, 2>& MassPdfs{ nullptr, nullptr };
            std::array<glm::vec2, 2> MassMaxPdfs{ glm::vec2(), glm::vec2() };
        };

    public:
        FStellarGenerator() = delete;
        FStellarGenerator(const FGenerationInfo& GenerationInfo);
        FStellarGenerator(const FStellarGenerator& Other);
        FStellarGenerator(FStellarGenerator&& Other) noexcept;
        ~FStellarGenerator() = default;

        FStellarGenerator& operator=(const FStellarGenerator& Other);
        FStellarGenerator& operator=(FStellarGenerator&& Other) noexcept;

        FBasicProperties GenerateBasicProperties(float Age = std::numeric_limits<float>::quiet_NaN(),
                                                 float FeH = std::numeric_limits<float>::quiet_NaN());

        Astro::AStar GenerateStar();
        Astro::AStar GenerateStar(FBasicProperties& Properties);

        FStellarGenerator& SetLogMassSuggestDistribution(std::unique_ptr<Util::TDistribution<>>&& Distribution);
        FStellarGenerator& SetUniverseAge(float Age);
        FStellarGenerator& SetAgeLowerLimit(float Limit);
        FStellarGenerator& SetAgeUpperLimit(float Limit);
        FStellarGenerator& SetFeHLowerLimit(float Limit);
        FStellarGenerator& SetFeHUpperLimit(float Limit);
        FStellarGenerator& SetMassLowerLimit(float Limit);
        FStellarGenerator& SetMassUpperLimit(float Limit);
        FStellarGenerator& SetCoilTempLimit(float Limit);
        FStellarGenerator& SetdEpdM(float dEpdM);
        FStellarGenerator& SetAgePdf(const std::function<float(glm::vec3, float, float)>& AgePdf);
        FStellarGenerator& SetAgeMaxPdf(glm::vec2 MaxPdf);
        FStellarGenerator& SetMassPdfs(const std::array<std::function<float(float)>, 2>& MassPdfs);
        FStellarGenerator& SetMassMaxPdfs(std::array<glm::vec2, 2> MaxPdfs);
        FStellarGenerator& SetAgeDistribution(EGenerationDistribution Distribution);
        FStellarGenerator& SetFeHDistribution(EGenerationDistribution Distribution);
        FStellarGenerator& SetMassDistribution(EGenerationDistribution Distribution);
        FStellarGenerator& SetStellarTypeGenerationOption(EStellarTypeGenerationOption Option);

    private:
        template <typename CsvType>
        requires std::is_class_v<CsvType>
        CsvType* LoadCsvAsset(const std::string& Filename, const std::vector<std::string>& Headers);

        void InitializeMistData();
        void InitializePdfs();
        float GenerateAge(float MaxPdf);
        float GenerateMass(float MaxPdf, auto& LogMassPdf);
        
        std::expected<FDataArray, Astro::AStar>
        GetFullMistData(const FBasicProperties& Properties, bool bIsWhiteDwarf, bool bIsSingleWhiteDwarf);
        
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
        static const int kStarAgeIndex_;
        static const int kStarMassIndex_;
        static const int kStarMdotIndex_;
        static const int kLogTeffIndex_;
        static const int kLogRIndex_;
        static const int kLogSurfZIndex_;
        static const int kSurfaceH1Index_;
        static const int kSurfaceHe3Index_;
        static const int kLogCenterTIndex_;
        static const int kLogCenterRhoIndex_;
        static const int kPhaseIndex_;
        static const int kXIndex_;
        static const int kLifetimeIndex_;

        static const int kWdStarAgeIndex_;
        static const int kWdLogRIndex_;
        static const int kWdLogTeffIndex_;
        static const int kWdLogCenterTIndex_;
        static const int kWdLogCenterRhoIndex_;

    private:
        std::mt19937                                          RandomEngine_;
        std::array<Util::TUniformRealDistribution<>,       8> MagneticGenerators_;
        std::array<std::unique_ptr<Util::TDistribution<>>, 4> FeHGenerators_;
        std::array<Util::TUniformRealDistribution<>,       2> SpinGenerators_;
        Util::TUniformRealDistribution<>                      AgeGenerator_;
        Util::TUniformRealDistribution<>                      CommonGenerator_;
        std::unique_ptr<Util::TDistribution<>>                LogMassGenerator_;

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

        static const std::vector<std::string>                                kMistHeaders_;
        static const std::vector<std::string>                                kWdMistHeaders_;
        static const std::vector<std::string>                                kHrDiagramHeaders_;
        static std::unordered_map<std::string, std::vector<float>>           kMassFilesCache_;
        static std::unordered_map<const FMistData*, std::vector<FDataArray>> kPhaseChangesCache_;
        static std::shared_mutex                                             kCacheMutex_;
        static bool                                                          kbMistDataInitiated_;
    };
} // namespace Npgs

#include "StellarGenerator.inl"
