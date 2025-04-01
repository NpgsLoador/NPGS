#include "Planet.h"

#include <utility>

namespace Npgs::Astro
{
    APlanet::APlanet(const FCelestialBody::FBasicProperties& BasicProperties, FExtendedProperties&& ExtraProperties)
        : Base(BasicProperties), _ExtraProperties(std::move(ExtraProperties))
    {
    }

    APlanet::APlanet(const APlanet& Other)
        : Base(Other)
    {
        _ExtraProperties =
        {
            .AtmosphereMass     = Other._ExtraProperties.AtmosphereMass,
            .CoreMass           = Other._ExtraProperties.CoreMass,
            .OceanMass          = Other._ExtraProperties.OceanMass,
            .CrustMineralMass   = Other._ExtraProperties.CrustMineralMass,
            .CivilizationData   = Other._ExtraProperties.CivilizationData
                                ? std::make_unique<Intelli::FStandard>(*Other._ExtraProperties.CivilizationData)
                                : nullptr,
            .BalanceTemperature = Other._ExtraProperties.BalanceTemperature,
            .Type               = Other._ExtraProperties.Type,
            .bIsMigrated        = Other._ExtraProperties.bIsMigrated
        };
    }

    APlanet& APlanet::operator=(const APlanet& Other)
    {
        if (this != &Other)
        {
            Base::operator=(Other);

            _ExtraProperties =
            {
                .AtmosphereMass     = Other._ExtraProperties.AtmosphereMass,
                .CoreMass           = Other._ExtraProperties.CoreMass,
                .OceanMass          = Other._ExtraProperties.OceanMass,
                .CrustMineralMass   = Other._ExtraProperties.CrustMineralMass,
                .CivilizationData   = Other._ExtraProperties.CivilizationData
                                    ? std::make_unique<Intelli::FStandard>(*Other._ExtraProperties.CivilizationData)
                                    : nullptr,
                .BalanceTemperature = Other._ExtraProperties.BalanceTemperature,
                .Type               = Other._ExtraProperties.Type,
                .bIsMigrated        = Other._ExtraProperties.bIsMigrated
            };
        }

        return *this;
    }

    AAsteroidCluster::AAsteroidCluster(const FBasicProperties& Properties)
        : _Properties(Properties)
    {
    }
} // namespace Npgs::Astro
