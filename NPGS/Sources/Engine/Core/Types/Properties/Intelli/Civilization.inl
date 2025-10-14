#include <utility>
#include "Engine/Core/Base/Base.hpp"

namespace Npgs::Intelli
{
    NPGS_INLINE FStandard& FStandard::SetOrganismBiomass(float OrganismBiomass)
    {
        LifeProperties_.OrganismBiomass = boost::multiprecision::uint128_t(OrganismBiomass);
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetOrganismBiomass(const boost::multiprecision::uint128_t& OrganismBiomass)
    {
        LifeProperties_.OrganismBiomass = OrganismBiomass;
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetOrganismUsedPower(float OrganismUsedPower)
    {
        LifeProperties_.OrganismUsedPower = OrganismUsedPower;
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetLifePhase(ELifePhase Phase)
    {
        LifeProperties_.Phase = Phase;
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetAtrificalStructureMass(float AtrificalStructureMass)
    {
        CivilizationProperties_.AtrificalStructureMass = boost::multiprecision::uint128_t(AtrificalStructureMass);
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetAtrificalStructureMass(boost::multiprecision::uint128_t AtrificalStructureMass)
    {
        CivilizationProperties_.AtrificalStructureMass = std::move(AtrificalStructureMass);
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetCitizenBiomass(float CitizenBiomass)
    {
        CivilizationProperties_.CitizenBiomass = boost::multiprecision::uint128_t(CitizenBiomass);
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetCitizenBiomass(boost::multiprecision::uint128_t CitizenBiomass)
    {
        CivilizationProperties_.CitizenBiomass = std::move(CitizenBiomass);
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetUseableEnergeticNuclide(float UseableEnergeticNuclide)
    {
        CivilizationProperties_.UseableEnergeticNuclide = boost::multiprecision::uint128_t(UseableEnergeticNuclide);
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetUseableEnergeticNuclide(boost::multiprecision::uint128_t UseableEnergeticNuclide)
    {
        CivilizationProperties_.UseableEnergeticNuclide = std::move(UseableEnergeticNuclide);
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetOrbitAssetsMass(float OrbitAssetsMass)
    {
        CivilizationProperties_.OrbitAssetsMass = boost::multiprecision::uint128_t(OrbitAssetsMass);
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetOrbitAssetsMass(boost::multiprecision::uint128_t OrbitAssetsMass)
    {
        CivilizationProperties_.OrbitAssetsMass = std::move(OrbitAssetsMass);
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetGeneralintelligenceCount(std::uint64_t GeneralintelligenceCount)
    {
        CivilizationProperties_.GeneralintelligenceCount = GeneralintelligenceCount;
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetGeneralIntelligenceAverageSynapseActivationCount(float GeneralIntelligenceAverageSynapseActivationCount)
    {
        CivilizationProperties_.GeneralIntelligenceAverageSynapseActivationCount = GeneralIntelligenceAverageSynapseActivationCount;
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetGeneralIntelligenceSynapseCount(float GeneralIntelligenceSynapseCount)
    {
        CivilizationProperties_.GeneralIntelligenceSynapseCount = GeneralIntelligenceSynapseCount;
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetGeneralIntelligenceAverageLifetime(float GeneralIntelligenceAverageLifetime)
    {
        CivilizationProperties_.GeneralIntelligenceAverageLifetime = GeneralIntelligenceAverageLifetime;
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetCitizenUsedPower(float CitizenUsedPower)
    {
        CivilizationProperties_.CitizenUsedPower = CitizenUsedPower;
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetCivilizationProgress(float CivilizationProgress)
    {
        CivilizationProperties_.CivilizationProgress = CivilizationProgress;
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetStoragedHistoryDataSize(float StoragedHistoryDataSize)
    {
        CivilizationProperties_.StoragedHistoryDataSize = StoragedHistoryDataSize;
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetTeamworkCoefficient(float TeamworkCoefficient)
    {
        CivilizationProperties_.TeamworkCoefficient = TeamworkCoefficient;
        return *this;
    }

    NPGS_INLINE FStandard& FStandard::SetIsIndependentIndividual(bool bIsIndependentIndividual)
    {
        CivilizationProperties_.bIsIndependentIndividual = bIsIndependentIndividual;
        return *this;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& FStandard::GetOrganismBiomass() const
    {
        return LifeProperties_.OrganismBiomass;
    }

    NPGS_INLINE float FStandard::GetOrganismUsedPower() const
    {
        return LifeProperties_.OrganismUsedPower;
    }

    NPGS_INLINE FStandard::ELifePhase FStandard::GetLifePhase() const
    {
        return LifeProperties_.Phase;
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType FStandard::GetOrganismBiomassDigital() const
    {
        return LifeProperties_.OrganismBiomass.convert_to<DigitalType>();
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& FStandard::GetAtrificalStructureMass() const
    {
        return CivilizationProperties_.AtrificalStructureMass;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& FStandard::GetCitizenBiomass() const
    {
        return CivilizationProperties_.CitizenBiomass;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& FStandard::GetUseableEnergeticNuclide() const
    {
        return CivilizationProperties_.UseableEnergeticNuclide;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& FStandard::GetOrbitAssetsMass() const
    {
        return CivilizationProperties_.OrbitAssetsMass;
    }

    NPGS_INLINE std::uint64_t FStandard::GetGeneralintelligenceCount() const
    {
        return CivilizationProperties_.GeneralintelligenceCount;
    }

    NPGS_INLINE float FStandard::GetGeneralIntelligenceAverageSynapseActivationCount() const
    {
        return CivilizationProperties_.GeneralIntelligenceAverageSynapseActivationCount;
    }

    NPGS_INLINE float FStandard::GetGeneralIntelligenceSynapseCount() const
    {
        return CivilizationProperties_.GeneralIntelligenceSynapseCount;
    }

    NPGS_INLINE float FStandard::GetGeneralIntelligenceAverageLifetime() const
    {
        return CivilizationProperties_.GeneralIntelligenceAverageLifetime;
    }

    NPGS_INLINE float FStandard::GetCitizenUsedPower() const
    {
        return CivilizationProperties_.CitizenUsedPower;
    }

    NPGS_INLINE float FStandard::GetCivilizationProgress() const
    {
        return CivilizationProperties_.CivilizationProgress;
    }

    NPGS_INLINE float FStandard::GetStoragedHistoryDataSize() const
    {
        return CivilizationProperties_.StoragedHistoryDataSize;
    }

    NPGS_INLINE float FStandard::GetTeamworkCoefficient() const
    {
        return CivilizationProperties_.TeamworkCoefficient;
    }

    NPGS_INLINE bool FStandard::IsIndependentIndividual() const
    {
        return CivilizationProperties_.bIsIndependentIndividual;
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType FStandard::GetAtrificalStructureMassDigital() const
    {
        return CivilizationProperties_.AtrificalStructureMass.convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType FStandard::GetCitizenBiomassDigital() const
    {
        return CivilizationProperties_.CitizenBiomass.convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType FStandard::GetUseableEnergeticNuclideDigital() const
    {
        return CivilizationProperties_.UseableEnergeticNuclide.convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType FStandard::GetOrbitAssetsMassDigital() const
    {
        return CivilizationProperties_.OrbitAssetsMass.convert_to<DigitalType>();
    }
} // namespace Npgs::Intelli
