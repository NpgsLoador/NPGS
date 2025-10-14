#include <utility>
#include "Engine/Core/Base/Base.hpp"

namespace Npgs::Astro
{
    NPGS_INLINE const APlanet::FExtendedProperties& APlanet::GetExtendedProperties() const
    {
        return ExtraProperties_;
    }

    NPGS_INLINE APlanet& APlanet::SetExtendedProperties(FExtendedProperties&& ExtraProperties)
    {
        ExtraProperties_ = std::move(ExtraProperties);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetAtmosphereMass(FComplexMass AtmosphereMass)
    {
        ExtraProperties_.AtmosphereMass = std::move(AtmosphereMass);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetCoreMass(FComplexMass CoreMass)
    {
        ExtraProperties_.CoreMass = std::move(CoreMass);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetOceanMass(FComplexMass OceanMass)
    {
        ExtraProperties_.OceanMass = std::move(OceanMass);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetCrustMineralMass(float CrustMineralMass)
    {
        ExtraProperties_.CrustMineralMass = boost::multiprecision::uint128_t(CrustMineralMass);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetCrustMineralMass(boost::multiprecision::uint128_t CrustMineralMass)
    {
        ExtraProperties_.CrustMineralMass = std::move(CrustMineralMass);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetCivilizationData(std::unique_ptr<Intelli::FStandard>&& CivilizationData)
    {
        ExtraProperties_.CivilizationData = std::move(CivilizationData);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetBalanceTemperature(float BalanceTemperature)
    {
        ExtraProperties_.BalanceTemperature = BalanceTemperature;
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetMigration(bool bIsMigrated)
    {
        ExtraProperties_.bIsMigrated = bIsMigrated;
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetPlanetType(EPlanetType Type)
    {
        ExtraProperties_.Type = Type;
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetAtmosphereMassZ(float AtmosphereMassZ)
    {
        ExtraProperties_.AtmosphereMass.Z = boost::multiprecision::uint128_t(AtmosphereMassZ);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetAtmosphereMassZ(boost::multiprecision::uint128_t AtmosphereMassZ)
    {
        ExtraProperties_.AtmosphereMass.Z = std::move(AtmosphereMassZ);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetAtmosphereMassVolatiles(float AtmosphereMassVolatiles)
    {
        ExtraProperties_.AtmosphereMass.Volatiles = boost::multiprecision::uint128_t(AtmosphereMassVolatiles);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetAtmosphereMassVolatiles(boost::multiprecision::uint128_t AtmosphereMassVolatiles)
    {
        ExtraProperties_.AtmosphereMass.Volatiles = std::move(AtmosphereMassVolatiles);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetAtmosphereMassEnergeticNuclide(float AtmosphereMassEnergeticNuclide)
    {
        ExtraProperties_.AtmosphereMass.EnergeticNuclide = boost::multiprecision::uint128_t(AtmosphereMassEnergeticNuclide);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetAtmosphereMassEnergeticNuclide(boost::multiprecision::uint128_t AtmosphereMassEnergeticNuclide)
    {
        ExtraProperties_.AtmosphereMass.EnergeticNuclide = std::move(AtmosphereMassEnergeticNuclide);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetCoreMassZ(float CoreMassZ)
    {
        ExtraProperties_.CoreMass.Z = boost::multiprecision::uint128_t(CoreMassZ);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetCoreMassZ(boost::multiprecision::uint128_t CoreMassZ)
    {
        ExtraProperties_.CoreMass.Z = std::move(CoreMassZ);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetCoreMassVolatiles(float CoreMassVolatiles)
    {
        ExtraProperties_.CoreMass.Volatiles = boost::multiprecision::uint128_t(CoreMassVolatiles);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetCoreMassVolatiles(boost::multiprecision::uint128_t CoreMassVolatiles)
    {
        ExtraProperties_.CoreMass.Volatiles = std::move(CoreMassVolatiles);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetCoreMassEnergeticNuclide(float CoreMassEnergeticNuclide)
    {
        ExtraProperties_.CoreMass.EnergeticNuclide = boost::multiprecision::uint128_t(CoreMassEnergeticNuclide);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetCoreMassEnergeticNuclide(boost::multiprecision::uint128_t CoreMassEnergeticNuclide)
    {
        ExtraProperties_.CoreMass.EnergeticNuclide = std::move(CoreMassEnergeticNuclide);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetOceanMassZ(float OceanMassZ)
    {
        ExtraProperties_.OceanMass.Z = boost::multiprecision::uint128_t(OceanMassZ);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetOceanMassZ(boost::multiprecision::uint128_t OceanMassZ)
    {
        ExtraProperties_.OceanMass.Z = std::move(OceanMassZ);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetOceanMassVolatiles(float OceanMassVolatiles)
    {
        ExtraProperties_.OceanMass.Volatiles = boost::multiprecision::uint128_t(OceanMassVolatiles);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetOceanMassVolatiles(boost::multiprecision::uint128_t OceanMassVolatiles)
    {
        ExtraProperties_.OceanMass.Volatiles = std::move(OceanMassVolatiles);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetOceanMassEnergeticNuclide(float OceanMassEnergeticNuclide)
    {
        ExtraProperties_.OceanMass.EnergeticNuclide = boost::multiprecision::uint128_t(OceanMassEnergeticNuclide);
        return *this;
    }

    NPGS_INLINE APlanet& APlanet::SetOceanMassEnergeticNuclide(boost::multiprecision::uint128_t OceanMassEnergeticNuclide)
    {
        ExtraProperties_.OceanMass.EnergeticNuclide = std::move(OceanMassEnergeticNuclide);
        return *this;
    }

    NPGS_INLINE const FComplexMass& APlanet::GetAtmosphereMassStruct() const
    {
        return ExtraProperties_.AtmosphereMass;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t APlanet::GetAtmosphereMass() const
    {
        return GetAtmosphereMassZ() + GetAtmosphereMassVolatiles() + GetAtmosphereMassEnergeticNuclide();
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetAtmosphereMassZ() const
    {
        return ExtraProperties_.AtmosphereMass.Z;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetAtmosphereMassVolatiles() const
    {
        return ExtraProperties_.AtmosphereMass.Volatiles;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetAtmosphereMassEnergeticNuclide() const
    {
        return ExtraProperties_.AtmosphereMass.EnergeticNuclide;
    }

    NPGS_INLINE const FComplexMass& APlanet::GetCoreMassStruct() const
    {
        return ExtraProperties_.CoreMass;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t APlanet::GetCoreMass() const
    {
        return GetCoreMassZ() + GetCoreMassVolatiles() + GetCoreMassEnergeticNuclide();
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetCoreMassZ() const
    {
        return ExtraProperties_.CoreMass.Z;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetCoreMassVolatiles() const
    {
        return ExtraProperties_.CoreMass.Volatiles;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetCoreMassEnergeticNuclide() const
    {
        return ExtraProperties_.CoreMass.EnergeticNuclide;
    }

    NPGS_INLINE const FComplexMass& APlanet::GetOceanMassStruct() const
    {
        return ExtraProperties_.OceanMass;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t APlanet::GetOceanMass() const
    {
        return GetOceanMassZ() + GetOceanMassVolatiles() + GetOceanMassEnergeticNuclide();
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetOceanMassZ() const
    {
        return ExtraProperties_.OceanMass.Z;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetOceanMassVolatiles() const
    {
        return ExtraProperties_.OceanMass.Volatiles;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetOceanMassEnergeticNuclide() const
    {
        return ExtraProperties_.OceanMass.EnergeticNuclide;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t APlanet::GetMass() const
    {
        return GetAtmosphereMass() + GetOceanMass() + GetCoreMass() + GetCrustMineralMass();
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& APlanet::GetCrustMineralMass() const
    {
        return ExtraProperties_.CrustMineralMass;
    }

    NPGS_INLINE float APlanet::GetBalanceTemperature() const
    {
        return ExtraProperties_.BalanceTemperature;
    }

    NPGS_INLINE bool APlanet::IsMigrated() const
    {
        return ExtraProperties_.bIsMigrated;
    }

    NPGS_INLINE APlanet::EPlanetType APlanet::GetPlanetType() const
    {
        return ExtraProperties_.Type;
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetAtmosphereMassDigital() const
    {
        return GetAtmosphereMass().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetAtmosphereMassZDigital() const
    {
        return GetAtmosphereMassZ().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetAtmosphereMassVolatilesDigital() const
    {
        return GetAtmosphereMassVolatiles().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetAtmosphereMassEnergeticNuclideDigital() const
    {
        return GetAtmosphereMassEnergeticNuclide().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetCoreMassDigital() const
    {
        return GetCoreMass().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetCoreMassZDigital() const
    {
        return GetCoreMassZ().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetCoreMassVolatilesDigital() const
    {
        return GetCoreMassVolatiles().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetCoreMassEnergeticNuclideDigital() const
    {
        return GetCoreMassEnergeticNuclide().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetOceanMassDigital() const
    {
        return GetOceanMass().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetOceanMassZDigital() const
    {
        return GetOceanMassZ().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetOceanMassVolatilesDigital() const
    {
        return GetOceanMassVolatiles().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetOceanMassEnergeticNuclideDigital() const
    {
        return GetOceanMassEnergeticNuclide().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetMassDigital() const
    {
        return GetMass().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType APlanet::GetCrustMineralMassDigital() const
    {
        return GetCrustMineralMass().convert_to<DigitalType>();
    }

    NPGS_INLINE Intelli::FStandard& APlanet::CivilizationData()
    {
        return *ExtraProperties_.CivilizationData;
    }

    NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMass(const FComplexMass& Mass)
    {
        Properties_.Mass = Mass;
        return *this;
    }

    NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassZ(float MassZ)
    {
        Properties_.Mass.Z = boost::multiprecision::uint128_t(MassZ);
        return *this;
    }

    NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassZ(boost::multiprecision::uint128_t MassZ)
    {
        Properties_.Mass.Z = std::move(MassZ);
        return *this;
    }

    NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassVolatiles(float MassVolatiles)
    {
        Properties_.Mass.Volatiles = boost::multiprecision::uint128_t(MassVolatiles);
        return *this;
    }

    NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassVolatiles(boost::multiprecision::uint128_t MassVolatiles)
    {
        Properties_.Mass.Volatiles = std::move(MassVolatiles);
        return *this;
    }

    NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassEnergeticNuclide(float MassEnergeticNuclide)
    {
        Properties_.Mass.EnergeticNuclide = boost::multiprecision::uint128_t(MassEnergeticNuclide);
        return *this;
    }

    NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetMassEnergeticNuclide(boost::multiprecision::uint128_t MassEnergeticNuclide)
    {
        Properties_.Mass.EnergeticNuclide = std::move(MassEnergeticNuclide);
        return *this;
    }

    NPGS_INLINE AAsteroidCluster& AAsteroidCluster::SetAsteroidType(EAsteroidType Type)
    {
        Properties_.Type = Type;
        return *this;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t AAsteroidCluster::GetMass() const
    {
        return GetMassZ() + GetMassVolatiles() + GetMassEnergeticNuclide();
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& AAsteroidCluster::GetMassZ() const
    {
        return Properties_.Mass.Z;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& AAsteroidCluster::GetMassVolatiles() const
    {
        return Properties_.Mass.Volatiles;
    }

    NPGS_INLINE const boost::multiprecision::uint128_t& AAsteroidCluster::GetMassEnergeticNuclide() const
    {
        return Properties_.Mass.EnergeticNuclide;
    }

    NPGS_INLINE AAsteroidCluster::EAsteroidType AAsteroidCluster::GetAsteroidType() const
    {
        return Properties_.Type;
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType AAsteroidCluster::GetMassDigital() const
    {
        return GetMass().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType AAsteroidCluster::GetMassZDigital() const
    {
        return GetMassZ().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType AAsteroidCluster::GetMassVolatilesDigital() const
    {
        return GetMassVolatiles().convert_to<DigitalType>();
    }

    template <typename DigitalType>
    NPGS_INLINE DigitalType AAsteroidCluster::GetMassEnergeticNuclideDigital() const
    {
        return GetMassEnergeticNuclide().convert_to<DigitalType>();
    }
} // namespace Npgs::Astro
