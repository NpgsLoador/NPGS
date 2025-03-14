#pragma once

#include <cstddef>
#include <array>
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

#include "Engine/Core/Base/Base.h"
#include "Engine/Core/Runtime/AssetLoaders/CommaSeparatedValues.hpp"
#include "Engine/Core/Types/Entries/Astro/Star.h"
#include "Engine/Core/Types/Properties/StellarClass.h"
#include "Engine/Utils/Random.hpp"

_NPGS_BEGIN
_SYSTEM_BEGIN
_GENERATOR_BEGIN

class FStellarGenerator
{
public:
    using FMistData   = Runtime::Asset::TCommaSeparatedValues<double, 12>;
    using FWdMistData = Runtime::Asset::TCommaSeparatedValues<double, 5>;
    using FHrDiagram  = Runtime::Asset::TCommaSeparatedValues<double, 7>;
    using FDataArray  = std::vector<double>;

    enum class EGenerationDistribution
    {
        kFromPdf,
        kUniform,
        kUniformByExponent
    };

    enum class EStellarTypeGenerationOption
    {
        kRandom,
        kGiant,
        kDeathStar,
        kMergeStar,
    };

    enum class EMultiplicityGenerationOption
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
            Star.SetInitialMass(InitialMassSol);
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
    FDataArray GetFullMistData(const FBasicProperties& Properties, bool bIsWhiteDwarf, bool bIsSingleWhiteDwarf);
    FDataArray InterpolateMistData(const std::pair<std::string, std::string>& Files, double TargetAge, double TargetMass, double MassCoefficient);
    std::vector<FDataArray> FindPhaseChanges(const FMistData* DataSheet);

    double CalculateEvolutionProgress(std::pair<std::vector<FDataArray>, std::vector<FDataArray>>& PhaseChanges,
                                      double TargetAge, double MassCoefficient);

    std::pair<double, std::pair<double, double>>
        FindSurroundingTimePoints(const std::vector<FDataArray>& PhaseChanges, double TargetAge);

    std::pair<double, std::size_t>
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
    void ExpandMistData(double TargetMass, FDataArray& StarData);

public:
    static const int _kStarAgeIndex;
    static const int _kStarMassIndex;
    static const int _kStarMdotIndex;
    static const int _kLogTeffIndex;
    static const int _kLogRIndex;
    static const int _kLogSurfZIndex;
    static const int _kSurfaceH1Index;
    static const int _kSurfaceHe3Index;
    static const int _kLogCenterTIndex;
    static const int _kLogCenterRhoIndex;
    static const int _kPhaseIndex;
    static const int _kXIndex;
    static const int _kLifetimeIndex;

    static const int _kWdStarAgeIndex;
    static const int _kWdLogRIndex;
    static const int _kWdLogTeffIndex;
    static const int _kWdLogCenterTIndex;
    static const int _kWdLogCenterRhoIndex;

private:
    std::mt19937                                          _RandomEngine;
    std::array<Util::TUniformRealDistribution<>,       8> _MagneticGenerators;
    std::array<std::unique_ptr<Util::TDistribution<>>, 4> _FeHGenerators;
    std::array<Util::TUniformRealDistribution<>,       2> _SpinGenerators;
    Util::TUniformRealDistribution<>                      _AgeGenerator;
    Util::TUniformRealDistribution<>                      _CommonGenerator;
    std::unique_ptr<Util::TDistribution<>>                _LogMassGenerator;

    std::array<std::function<float(float)>, 2>    _MassPdfs;
    std::array<glm::vec2, 2>                      _MassMaxPdfs;
    std::function<float(glm::vec3, float, float)> _AgePdf;
    glm::vec2                                     _AgeMaxPdf;

    float _UniverseAge;
    float _AgeLowerLimit;
    float _AgeUpperLimit;
    float _FeHLowerLimit;
    float _FeHUpperLimit;
    float _MassLowerLimit;
    float _MassUpperLimit;
    float _CoilTemperatureLimit;
    float _dEpdM;

    EGenerationDistribution       _AgeDistribution;
    EGenerationDistribution       _FeHDistribution;
    EGenerationDistribution       _MassDistribution;
    EStellarTypeGenerationOption  _StellarTypeOption;
    EMultiplicityGenerationOption _MultiplicityOption;

    static const std::vector<std::string>                                _kMistHeaders;
    static const std::vector<std::string>                                _kWdMistHeaders;
    static const std::vector<std::string>                                _kHrDiagramHeaders;
    static std::unordered_map<std::string, std::vector<float>>           _kMassFilesCache;
    static std::unordered_map<const FMistData*, std::vector<FDataArray>> _kPhaseChangesCache;
    static std::shared_mutex                                             _kCacheMutex;
    static bool                                                          _kbMistDataInitiated;
};

_GENERATOR_END
_SYSTEM_END
_NPGS_END

#include "StellarGenerator.inl"
