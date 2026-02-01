#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <shared_mutex>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <glm/glm.hpp>

#include "Engine/Core/Math/NumericConstants.hpp"
#include "Engine/Core/Math/Random.hpp"
#include "Engine/Core/Types/Entries/Astro/Star.hpp"
#include "Engine/Core/Types/Properties/StellarClass.hpp"
#include "Engine/Runtime/AssetLoaders/CommaSeparatedValues.hpp"
#include "Engine/Runtime/Managers/AssetManager.hpp"

namespace Npgs
{
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

    struct FStellarBasicProperties
    {
        // 用于保存生成选项，类的生成选项仅影响该属性。生成的恒星完整信息也将根据该属性决定。该选项用于防止多线程生成恒星时属性和生成器胡乱匹配
        EStellarTypeGenerationOption  StellarTypeOption{ EStellarTypeGenerationOption::kRandom };
        EMultiplicityGenerationOption MultiplicityOption{ EMultiplicityGenerationOption::kSingleStar };

        float Age{};
        float FeH{};
        float InitialMassSol{};
        bool  bIsSingleStar{ true };

        explicit operator Astro::AStar() const;
    };

    struct FStellarGenerationInfo
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
        std::function<float(glm::vec3, float, float)> AgePdf{ nullptr };
        glm::vec2 AgeMaxPdf{ glm::vec2() };
        std::array<std::function<float(float)>, 2> MassPdfs{ nullptr, nullptr };
        std::array<glm::vec2, 2> MassMaxPdfs{ glm::vec2(), glm::vec2() };
    };

    class FStellarGenerator
    {
    public:
        using FMistData   = TCommaSeparatedValues<double, 12>;
        using FWdMistData = TCommaSeparatedValues<double, 5>;
        using FDataArray  = std::vector<double>;

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

        FStellarGenerator& SetLogMassSuggestDistribution(std::unique_ptr<Math::TDistribution<>>&& Distribution);
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

    public:
        static constexpr int kStarAgeIndex_           = 0;
        static constexpr int kStarMassIndex_          = 1;
        static constexpr int kStarMdotIndex_          = 2;
        static constexpr int kLogTeffIndex_           = 3;
        static constexpr int kLogRIndex_              = 4;
        static constexpr int kLogSurfZIndex_          = 5;
        static constexpr int kSurfaceH1Index_         = 6;
        static constexpr int kSurfaceHe3Index_        = 7;
        static constexpr int kLogCenterTIndex_        = 8;
        static constexpr int kLogCenterRhoIndex_      = 9;
        static constexpr int kPhaseIndex_             = 10;
        static constexpr int kXIndex_                 = 11;
        static constexpr int kLifetimeIndex_          = 12;

        static constexpr int kWdStarAgeIndex_         = 0;
        static constexpr int kWdLogRIndex_            = 1;
        static constexpr int kWdLogTeffIndex_         = 2;
        static constexpr int kWdLogCenterTIndex_      = 3;
        static constexpr int kWdLogCenterRhoIndex_    = 4;

        static constexpr int kLowMassMStarIndex_      = 0;
        static constexpr int kLateKMStarIndex_        = 1;
        static constexpr int kSolarLikeStarIndex_     = 2;
        static constexpr int kCommonXpStarIndex_      = 3;
        static constexpr int kNormalMassiveStarIndex_ = 4;
        static constexpr int kExtremeOpStarIndex_     = 5;
        static constexpr int kWhiteDwarfIndex_        = 6;
        static constexpr int kNeutronStarIndex_       = 7;

        static constexpr int kXpStarIndex_            = 0;
        static constexpr int kOpStarIndex_            = 1;

        static constexpr int kFastMassiveStarIndex_   = 0;
        static constexpr int kSlowMassiveStarIndex_   = 1;
        static constexpr int kWhiteDwarfSpinIndex_    = 2;
        static constexpr int kNeutronStarSpinIndex_   = 3;
        static constexpr int kBlackHoleSpinIndex_     = 4;

    private:
        struct FDataFileTables
        {
            std::string LowerMassFilename;
            std::string UpperMassFilename;
            float LowerMass{};
            float UpperMass{};
        };

        struct FSurroundingTimePoints
        {
            double Phase{};
            double LowerTimePoint{};
            double UpperTimePoint{};
        };

        struct FExactChangedTimePoints
        {
            double      Phase{};
            std::size_t TableIndex{};
        };

        struct FPhysicalLuminosityLimit
        {
            float EddingtonLimit{};
            float HdLimit{};
        };

        struct FRemainsBasicProperties
        {
            Astro::AStar::EEvolutionPhase EvolutionPhase{};
            Astro::AStar::EStarFrom       DeathStarFrom{};
            Astro::EStellarType           DeathStarType{};
            Astro::FSpectralType          DeathStarClass;
            float                         DeathStarMassSol{};
        };

    private:
        template <typename CsvType>
        requires std::is_class_v<CsvType>
        TAssetHandle<CsvType> LoadCsvAsset(const std::string& Filename, std::span<const std::string> Headers) const;

        void InitializeMistData() const;
        void InitializePdfs();
        float GenerateAge(float MaxPdf);
        float GenerateMass(float MaxPdf, auto& LogMassPdf);
        Astro::AStar GenerateStarInternal(const FStellarBasicProperties& Properties, Astro::AStar* ZamsStarData);
        void ApplyFromZamsStar(Astro::AStar& StarData, const Astro::AStar& ZamsStarData);
        FDataArray GetFullMistData(const FStellarBasicProperties& Properties, bool bIsWhiteDwarf, bool bIsSingleWhiteDwarf) const;
        FDataFileTables GetDataTables(float MassSol, float FeH, bool bIsWhiteDwarf, bool bIsSingleWhiteDwarf) const;
        float GetClosestFeH(float FeH) const;

        FDataArray InterpolateMistData(const std::pair<std::string, std::string>& Files,
                                       double TargetAge, double TargetMassSol, double MassCoefficient) const;

        std::vector<FDataArray> FindPhaseChanges(const FMistData* DataSheet) const;

        double CalculateEvolutionProgress(std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& PhaseChanges,
                                          double TargetAge, double MassCoefficient) const;

        FSurroundingTimePoints FindSurroundingTimePoints(const std::vector<FDataArray>& PhaseChanges, double TargetAge) const;

        FExactChangedTimePoints FindSurroundingTimePoints(const std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& PhaseChanges,
                                                          double TargetAge, double MassCoefficient) const;

        void AlignArrays(std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& Arrays) const;
        FDataArray InterpolateStarData(FMistData* Data, double EvolutionProgress) const;
        FDataArray InterpolateStarData(FWdMistData* Data, double TargetAge) const;
        FDataArray InterpolateStarData(auto* Data, double Target, const std::string& Header, int Index, bool bIsWhiteDwarf) const;
        FDataArray InterpolateArray(const std::pair<FDataArray, FDataArray>& DataArrays, double Coefficient) const;
        FDataArray InterpolateFinalData(const std::pair<FDataArray, FDataArray>& DataArrays, double Coefficient, bool bIsWhiteDwarf) const;
        Astro::AStar MakeNormalStar(const FDataArray& StarData, const FStellarBasicProperties& Properties);

        Astro::AStar GenerateDeathStar(EStellarTypeGenerationOption DeathStarTypeOption,
                                       const FStellarBasicProperties& Properties, Astro::AStar& ZamsStarData);

        FDataArray FindZamsData(const FMistData* DataSheet) const;
        FDataArray CalculateZamsStarData(float InitialMassSol, float FeH) const;
        void CalculateSpectralType(Astro::AStar& StarData) const;
        Astro::ELuminosityClass CalculateLuminosityClass(const Astro::AStar& StarData) const;
        Astro::ELuminosityClass CalculateLuminosityClassFromSurfaceGravity(float Teff, float LogG) const;
        Astro::ELuminosityClass CalculateHypergiant(const Astro::AStar& StarData) const;
        FPhysicalLuminosityLimit CalculatePhysicalLuminosityLimit(const Astro::AStar& StarData) const;

        FRemainsBasicProperties DetermineRemainsProperties(const Astro::AStar& DyingStarData, float InitialMassSol,
                                                           float FeH, float CoreSpecificAngularMomentum);

        FRemainsBasicProperties GenerateMergeStar(const FStellarBasicProperties& Properties);
        Astro::AStar CalculateAndMakeDeathStar(const Astro::AStar& DyingStarData, const FRemainsBasicProperties& Properties, float DeathStarAge);

        float CalculateRemainsMassSol(float InitialMassSol) const;
        bool CheckEnvelopeStripSuccessful(float FeH);
        void CalculateExtremeProperties(Astro::AStar& DeathStarData, const Astro::AStar& ZamsStarData, float CoreSpecificAngularMomentum);
        float CalculateDynamoField(float Spin);
        void CalculateNeutronStarDecayedProperties(Astro::AStar& DeathStarData, float InitialMagneticField, float InitialSpin) const;
        float CalculateCoreSpecificAngularMomentum(const Astro::AStar& DyingStarData) const;
        float CalculateHeliumCoreMassSol(const Astro::AStar& DyingStarData) const;
        void GenerateZamsStarMagnetic(Astro::AStar& StarData);
        void GenerateCompatStarMagnetic(Astro::AStar& StarData);
        void GenerateZamsStarInitialSpin(Astro::AStar& StarData);
        float CalculateEffectiveMass(const Astro::AStar& StarData) const;
        void CalculateCurrentSpinAndOblateness(Astro::AStar& StarData, const Astro::AStar& ZamsStarData);
        void CalculateNormalStarDecayedSpin(Astro::AStar& StarData, float ZamsRadiusSol, float InitialSpin, bool bIsFastSpin);
        void CalculateNormalStarOblateness(Astro::AStar& StarData, float EffectiveMass, float Spin);
        void GenerateCompatStarSpin(Astro::AStar& StarData);
        void ExpandMistData(double TargetMassSol, FDataArray& StarData) const;
        void CalculateMinCoilMass(Astro::AStar& StarData) const;

    private:
        std::mt19937                                          RandomEngine_;
        std::array<Math::TUniformRealDistribution<>,       8> MagneticGenerators_;
        std::array<std::unique_ptr<Math::TDistribution<>>, 4> FeHGenerators_;
        std::array<std::unique_ptr<Math::TDistribution<>>, 5> SpinGenerators_;
        std::array<Math::TBernoulliDistribution<>,         2> XpStarProbabilityGenerators_;
        Math::TUniformRealDistribution<>                      AgeGenerator_;
        Math::TUniformRealDistribution<>                      CommonGenerator_;
        Math::TBernoulliDistribution<>                        FastMassiveStarProbabilityGenerator_;
        std::unique_ptr<Math::TDistribution<>>                LogMassGenerator_;

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

        static const std::array<std::string, 12> kMistHeaders_;
        static const std::array<std::string, 5>  kWdMistHeaders_;

        static inline ankerl::unordered_dense::map<std::string, std::vector<float>>           MassFilesCache_;
        static inline ankerl::unordered_dense::map<const FMistData*, std::vector<FDataArray>> PhaseChangesCache_;
        static inline ankerl::unordered_dense::map<const FMistData*, FDataArray>              ZamsDataCache_;
        static inline std::shared_mutex FileCacheMutex_;
        static inline std::shared_mutex PhaseChangeCacheMutex_;
        static inline std::shared_mutex ZamsDataMutex_;
        static inline bool              bMistDataInitiated_{ false };
    };
} // namespace Npgs

#include "StellarGenerator.inl"
