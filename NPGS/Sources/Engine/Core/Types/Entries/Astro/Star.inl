#include "Engine/Core/Base/Base.hpp"

namespace Npgs::Astro
{
    NPGS_INLINE const AStar::FExtendedProperties& AStar::GetExtendedProperties() const
    {
        return ExtraProperties_;
    }

    NPGS_INLINE AStar& AStar::SetExtendedProperties(FExtendedProperties ExtraProperties)
    {
        ExtraProperties_ = std::move(ExtraProperties);
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetMass(double Mass)
    {
        ExtraProperties_.Mass = Mass;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetLuminosity(double Luminosity)
    {
        ExtraProperties_.Luminosity = Luminosity;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetLifetime(double Lifetime)
    {
        ExtraProperties_.Lifetime = Lifetime;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetEvolutionProgress(double EvolutionProgress)
    {
        ExtraProperties_.EvolutionProgress = EvolutionProgress;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetFeH(float FeH)
    {
        ExtraProperties_.FeH = FeH;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetInitialMass(float InitialMass)
    {
        ExtraProperties_.InitialMass = InitialMass;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetSurfaceH1(float SurfaceH1)
    {
        ExtraProperties_.SurfaceH1 = SurfaceH1;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetSurfaceZ(float SurfaceZ)
    {
        ExtraProperties_.SurfaceZ = SurfaceZ;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetSurfaceEnergeticNuclide(float SurfaceEnergeticNuclide)
    {
        ExtraProperties_.SurfaceEnergeticNuclide = SurfaceEnergeticNuclide;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetSurfaceVolatiles(float SurfaceVolatiles)
    {
        ExtraProperties_.SurfaceVolatiles = SurfaceVolatiles;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetTeff(float Teff)
    {
        ExtraProperties_.Teff = Teff;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetCoreTemp(float CoreTemp)
    {
        ExtraProperties_.CoreTemp = CoreTemp;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetCoreDensity(float CoreDensity)
    {
        ExtraProperties_.CoreDensity = CoreDensity;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetStellarWindSpeed(float StellarWindSpeed)
    {
        ExtraProperties_.StellarWindSpeed = StellarWindSpeed;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetStellarWindMassLossRate(float StellarWindMassLossRate)
    {
        ExtraProperties_.StellarWindMassLossRate = StellarWindMassLossRate;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetMinCoilMass(float MinCoilMass)
    {
        ExtraProperties_.MinCoilMass = MinCoilMass;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetSingleton(bool bIsSingleStar)
    {
        ExtraProperties_.bIsSingleStar = bIsSingleStar;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetHasPlanets(bool bHasPlanets)
    {
        ExtraProperties_.bHasPlanets = bHasPlanets;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetStarFrom(EStarFrom From)
    {
        ExtraProperties_.From = From;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetEvolutionPhase(EEvolutionPhase Phase)
    {
        ExtraProperties_.Phase = Phase;
        return *this;
    }

    NPGS_INLINE AStar& AStar::SetStellarClass(Astro::FStellarClass Class)
    {
        ExtraProperties_.Class = std::move(Class);
        return *this;
    }

    NPGS_INLINE double AStar::GetMass() const
    {
        return ExtraProperties_.Mass;
    }

    NPGS_INLINE double AStar::GetLuminosity() const
    {
        return ExtraProperties_.Luminosity;
    }

    NPGS_INLINE double AStar::GetLifetime() const
    {
        return ExtraProperties_.Lifetime;
    }

    NPGS_INLINE double AStar::GetEvolutionProgress() const
    {
        return ExtraProperties_.EvolutionProgress;
    }

    NPGS_INLINE float AStar::GetFeH() const
    {
        return ExtraProperties_.FeH;
    }

    NPGS_INLINE float AStar::GetInitialMass() const
    {
        return ExtraProperties_.InitialMass;
    }

    NPGS_INLINE float AStar::GetSurfaceH1() const
    {
        return ExtraProperties_.SurfaceH1;
    }

    NPGS_INLINE float AStar::GetSurfaceZ() const
    {
        return ExtraProperties_.SurfaceZ;
    }

    NPGS_INLINE float AStar::GetSurfaceEnergeticNuclide() const
    {
        return ExtraProperties_.SurfaceEnergeticNuclide;
    }

    NPGS_INLINE float AStar::GetSurfaceVolatiles() const
    {
        return ExtraProperties_.SurfaceVolatiles;
    }

    NPGS_INLINE float AStar::GetTeff() const
    {
        return ExtraProperties_.Teff;
    }

    NPGS_INLINE float AStar::GetCoreTemp() const
    {
        return ExtraProperties_.CoreTemp;
    }

    NPGS_INLINE float AStar::GetCoreDensity() const
    {
        return ExtraProperties_.CoreDensity;
    }

    NPGS_INLINE float AStar::GetStellarWindSpeed() const
    {
        return ExtraProperties_.StellarWindSpeed;
    }

    NPGS_INLINE float AStar::GetStellarWindMassLossRate() const
    {
        return ExtraProperties_.StellarWindMassLossRate;
    }

    NPGS_INLINE float AStar::GetMinCoilMass() const
    {
        return ExtraProperties_.MinCoilMass;
    }

    NPGS_INLINE bool AStar::IsSingleStar() const
    {
        return ExtraProperties_.bIsSingleStar;
    }

    NPGS_INLINE bool AStar::HasPlanets() const
    {
        return ExtraProperties_.bHasPlanets;
    }

    NPGS_INLINE AStar::EStarFrom AStar::GetStarFrom() const
    {
        return ExtraProperties_.From;
    }

    NPGS_INLINE AStar::EEvolutionPhase AStar::GetEvolutionPhase() const
    {
        return ExtraProperties_.Phase;
    }

    NPGS_INLINE const Astro::FStellarClass& AStar::GetStellarClass() const
    {
        return ExtraProperties_.Class;
    }
} // namespace Npgs::Astro
