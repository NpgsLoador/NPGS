#pragma once

#include <cstdint>
#include <memory>

#include <boost/multiprecision/cpp_int.hpp>
#include "Engine/Core/Types/Entries/Astro/CelestialObject.hpp"
#include "Engine/Core/Types/Properties/Intelli/Civilization.hpp"

namespace Npgs::Astro
{
    struct FComplexMass
    {
        boost::multiprecision::uint128_t Z;
        boost::multiprecision::uint128_t Volatiles;
        boost::multiprecision::uint128_t EnergeticNuclide;
    };

    class APlanet : public FCelestialBody
    {
    public:
        enum class EPlanetType : std::uint8_t
        {
            kRocky                            = 0,
            kTerra                            = 1,
            kIcePlanet                        = 2,
            kChthonian                        = 3,
            kOceanic                          = 4,
            kSubIceGiant                      = 5,
            kIceGiant                         = 6,
            kGasGiant                         = 7,
            kHotSubIceGiant                   = 8,
            kHotIceGiant                      = 9,
            kHotGasGiant                      = 10,
            kRockyAsteroidCluster             = 11,
            kRockyIceAsteroidCluster          = 12,
            kArtificalOrbitalStructureCluster = 13
        };

        struct FExtendedProperties
        {
            FComplexMass                        AtmosphereMass;              // 大气层质量，单位 kg
            FComplexMass                        CoreMass;                    // 核心质量，单位 kg
            FComplexMass                        OceanMass;                   // 海洋质量，单位 kg
            boost::multiprecision::uint128_t    CrustMineralMass;            // 地壳矿脉质量，单位 kg
            std::unique_ptr<Intelli::FStandard> CivilizationData;            // 文明数据
            float                               BalanceTemperature{};        // 平衡温度，单位 K
            EPlanetType                         Type{ EPlanetType::kRocky }; // 行星类型
            bool                                bIsMigrated{ false };        // 是否为迁移行星
        };

    public:
        using Base = FCelestialBody;
        using Base::Base;

        APlanet()                   = default;
        APlanet(const FCelestialBody::FBasicProperties& BasicProperties, FExtendedProperties&& ExtraProperties);
        APlanet(const APlanet& Other);
        APlanet(APlanet&&) noexcept = default;
        ~APlanet()                  = default;

        APlanet& operator=(const APlanet& Other);
        APlanet& operator=(APlanet&&) noexcept = default;

        const FExtendedProperties& GetExtendedProperties() const;
        APlanet& SetExtendedProperties(FExtendedProperties&& ExtraProperties);

        // Setters
        // Setters for ExtendedProperties
        // ------------------------------
        APlanet& SetAtmosphereMass(FComplexMass AtmosphereMass);
        APlanet& SetCoreMass(FComplexMass CoreMass);
        APlanet& SetOceanMass(FComplexMass OceanMass);
        APlanet& SetCrustMineralMass(float CrustMineralMass);
        APlanet& SetCrustMineralMass(boost::multiprecision::uint128_t CrustMineralMass);
        APlanet& SetCivilizationData(std::unique_ptr<Intelli::FStandard>&& CivilizationData);
        APlanet& SetBalanceTemperature(float BalanceTemperature);
        APlanet& SetMigration(bool bIsMigrated);
        APlanet& SetPlanetType(EPlanetType Type);

        // Setters for every mass property
        // -------------------------------
        APlanet& SetAtmosphereMassZ(float AtmosphereMassZ);
        APlanet& SetAtmosphereMassZ(boost::multiprecision::uint128_t AtmosphereMassZ);
        APlanet& SetAtmosphereMassVolatiles(float AtmosphereMassVolatiles);
        APlanet& SetAtmosphereMassVolatiles(boost::multiprecision::uint128_t AtmosphereMassVolatiles);
        APlanet& SetAtmosphereMassEnergeticNuclide(float AtmosphereMassEnergeticNuclide);
        APlanet& SetAtmosphereMassEnergeticNuclide(boost::multiprecision::uint128_t AtmosphereMassEnergeticNuclide);
        APlanet& SetCoreMassZ(float CoreMassZ);
        APlanet& SetCoreMassZ(boost::multiprecision::uint128_t CoreMassZ);
        APlanet& SetCoreMassVolatiles(float CoreMassVolatiles);
        APlanet& SetCoreMassVolatiles(boost::multiprecision::uint128_t CoreMassVolatiles);
        APlanet& SetCoreMassEnergeticNuclide(float CoreMassEnergeticNuclide);
        APlanet& SetCoreMassEnergeticNuclide(boost::multiprecision::uint128_t CoreMassEnergeticNuclide);
        APlanet& SetOceanMassZ(float OceanMassZ);
        APlanet& SetOceanMassZ(boost::multiprecision::uint128_t OceanMassZ);
        APlanet& SetOceanMassVolatiles(float OceanMassVolatiles);
        APlanet& SetOceanMassVolatiles(boost::multiprecision::uint128_t OceanMassVolatiles);
        APlanet& SetOceanMassEnergeticNuclide(float OceanMassEnergeticNuclide);
        APlanet& SetOceanMassEnergeticNuclide(boost::multiprecision::uint128_t OceanMassEnergeticNuclide);

        // Getters
        // Getters for ExtendedProperties
        // ------------------------------
        const FComplexMass&                     GetAtmosphereMassStruct() const;
        const boost::multiprecision::uint128_t  GetAtmosphereMass() const;
        const boost::multiprecision::uint128_t& GetAtmosphereMassZ() const;
        const boost::multiprecision::uint128_t& GetAtmosphereMassVolatiles() const;
        const boost::multiprecision::uint128_t& GetAtmosphereMassEnergeticNuclide() const;
        const FComplexMass&                     GetCoreMassStruct() const;
        const boost::multiprecision::uint128_t  GetCoreMass() const;
        const boost::multiprecision::uint128_t& GetCoreMassZ() const;
        const boost::multiprecision::uint128_t& GetCoreMassVolatiles() const;
        const boost::multiprecision::uint128_t& GetCoreMassEnergeticNuclide() const;
        const FComplexMass&                     GetOceanMassStruct() const;
        const boost::multiprecision::uint128_t  GetOceanMass() const;
        const boost::multiprecision::uint128_t& GetOceanMassZ() const;
        const boost::multiprecision::uint128_t& GetOceanMassVolatiles() const;
        const boost::multiprecision::uint128_t& GetOceanMassEnergeticNuclide() const;
        const boost::multiprecision::uint128_t  GetMass() const;
        const boost::multiprecision::uint128_t& GetCrustMineralMass() const;
        float                                   GetBalanceTemperature() const;
        bool                                    IsMigrated() const;
        EPlanetType                             GetPlanetType() const;

        template <typename DigitalType>
        DigitalType GetAtmosphereMassDigital() const;

        template <typename DigitalType>
        DigitalType GetAtmosphereMassZDigital() const;

        template <typename DigitalType>
        DigitalType GetAtmosphereMassVolatilesDigital() const;

        template <typename DigitalType>
        DigitalType GetAtmosphereMassEnergeticNuclideDigital() const;

        template <typename DigitalType>
        DigitalType GetCoreMassDigital() const;

        template <typename DigitalType>
        DigitalType GetCoreMassZDigital() const;

        template <typename DigitalType>
        DigitalType GetCoreMassVolatilesDigital() const;

        template <typename DigitalType>
        DigitalType GetCoreMassEnergeticNuclideDigital() const;

        template <typename DigitalType>
        DigitalType GetOceanMassDigital() const;

        template <typename DigitalType>
        DigitalType GetOceanMassZDigital() const;

        template <typename DigitalType>
        DigitalType GetOceanMassVolatilesDigital() const;

        template <typename DigitalType>
        DigitalType GetOceanMassEnergeticNuclideDigital() const;

        template <typename DigitalType>
        DigitalType GetMassDigital() const;

        template <typename DigitalType>
        DigitalType GetCrustMineralMassDigital() const;

        Intelli::FStandard& CivilizationData();

    private:
        FExtendedProperties ExtraProperties_{};
    };

    class AAsteroidCluster : public IAstroObject
    {
    public:
        enum class EAsteroidType : std::uint8_t
        {
            kRocky                     = 0,
            kRockyIce                  = 1,
            kArtificalOrbitalStructure = 2
        };

        struct FBasicProperties
        {
            FComplexMass  Mass;
            EAsteroidType Type{ EAsteroidType::kRocky };
        };

    public:
        AAsteroidCluster() = default;
        AAsteroidCluster(const FBasicProperties& Properties);
        ~AAsteroidCluster() = default;

        // Setters
        // Setters for BasicProperties
        // ---------------------------
        AAsteroidCluster& SetMass(const FComplexMass& Mass);

        // Setters for every mass property
        // -------------------------------
        AAsteroidCluster& SetMassZ(float MassZ);
        AAsteroidCluster& SetMassZ(boost::multiprecision::uint128_t MassZ);
        AAsteroidCluster& SetMassVolatiles(float MassVolatiles);
        AAsteroidCluster& SetMassVolatiles(boost::multiprecision::uint128_t MassVolatiles);
        AAsteroidCluster& SetMassEnergeticNuclide(float MassEnergeticNuclide);
        AAsteroidCluster& SetMassEnergeticNuclide(boost::multiprecision::uint128_t MassEnergeticNuclide);
        AAsteroidCluster& SetAsteroidType(EAsteroidType Type);

        // Getters
        // Getters for BasicProperties
        // ---------------------------
        const boost::multiprecision::uint128_t  GetMass() const;
        const boost::multiprecision::uint128_t& GetMassZ() const;
        const boost::multiprecision::uint128_t& GetMassVolatiles() const;
        const boost::multiprecision::uint128_t& GetMassEnergeticNuclide() const;
        EAsteroidType                           GetAsteroidType() const;

        template <typename DigitalType>
        DigitalType GetMassDigital() const;

        template <typename DigitalType>
        DigitalType GetMassZDigital() const;

        template <typename DigitalType>
        DigitalType GetMassVolatilesDigital() const;

        template <typename DigitalType>
        DigitalType GetMassEnergeticNuclideDigital() const;

    private:
        FBasicProperties Properties_{};
    };
} // namespace Npgs::Astro

#include "Planet.inl"
