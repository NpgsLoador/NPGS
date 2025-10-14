#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    NPGS_INLINE FStellarGenerator::FBasicProperties::operator Astro::AStar() const
    {
        Astro::AStar Star;
        Star.SetAge(Age);
        Star.SetFeH(FeH);
        Star.SetInitialMass(InitialMassSol * kSolarMass);
        Star.SetSingleton(bIsSingleStar);

        return Star;
    }

    NPGS_INLINE FStellarGenerator&
    FStellarGenerator::SetLogMassSuggestDistribution(std::unique_ptr<Util::TDistribution<>>&& Distribution)
    {
        LogMassGenerator_ = std::move(Distribution);
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetUniverseAge(float Age)
    {
        UniverseAge_ = Age;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetAgeLowerLimit(float Limit)
    {
        AgeLowerLimit_ = Limit;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetAgeUpperLimit(float Limit)
    {
        AgeUpperLimit_ = Limit;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetFeHLowerLimit(float Limit)
    {
        FeHLowerLimit_ = Limit;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetFeHUpperLimit(float Limit)
    {
        FeHUpperLimit_ = Limit;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetMassLowerLimit(float Limit)
    {
        MassLowerLimit_ = Limit;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetMassUpperLimit(float Limit)
    {
        MassUpperLimit_ = Limit;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetCoilTempLimit(float Limit)
    {
        CoilTemperatureLimit_ = Limit;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetdEpdM(float dEpdM)
    {
        dEpdM_ = dEpdM;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetAgePdf(std::function<float(glm::vec3, float, float)> AgePdf)
    {
        AgePdf_ = std::move(AgePdf);
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetAgeMaxPdf(glm::vec2 MaxPdf)
    {
        AgeMaxPdf_ = MaxPdf;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetMassPdfs(std::array<std::function<float(float)>, 2> MassPdfs)
    {
        MassPdfs_ = std::move(MassPdfs);
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetMassMaxPdfs(std::array<glm::vec2, 2> MaxPdfs)
    {
        MassMaxPdfs_ = MaxPdfs;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetAgeDistribution(EGenerationDistribution Distribution)
    {
        AgeDistribution_ = Distribution;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetFeHDistribution(EGenerationDistribution Distribution)
    {
        FeHDistribution_ = Distribution;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetMassDistribution(EGenerationDistribution Distribution)
    {
        MassDistribution_ = Distribution;
        return *this;
    }

    NPGS_INLINE FStellarGenerator& FStellarGenerator::SetStellarTypeGenerationOption(EStellarTypeGenerationOption Option)
    {
        StellarTypeOption_ = Option;
        return *this;
    }
} // namespace Npgs
