#include "stdafx.h"
#include "Planet.hpp"

#include <utility>

namespace Npgs::Astro
{
    APlanet::APlanet(const FCelestialBody::FBasicProperties& BasicProperties, FExtendedProperties&& ExtraProperties)
        : Base(BasicProperties), ExtraProperties_(std::move(ExtraProperties))
    {
    }

    APlanet::APlanet(const APlanet& Other)
        : Base(Other)
    {
        ExtraProperties_ =
        {
            .AtmosphereMass     = Other.ExtraProperties_.AtmosphereMass,
            .CoreMass           = Other.ExtraProperties_.CoreMass,
            .OceanMass          = Other.ExtraProperties_.OceanMass,
            .CrustMineralMass   = Other.ExtraProperties_.CrustMineralMass,
            .CivilizationData   = Other.ExtraProperties_.CivilizationData
                                ? std::make_unique<Intelli::FStandard>(*Other.ExtraProperties_.CivilizationData)
                                : nullptr,
            .BalanceTemperature = Other.ExtraProperties_.BalanceTemperature,
            .Type               = Other.ExtraProperties_.Type,
            .bIsMigrated        = Other.ExtraProperties_.bIsMigrated
        };
    }

    APlanet& APlanet::operator=(const APlanet& Other)
    {
        if (this != &Other)
        {
            Base::operator=(Other);

            ExtraProperties_ =
            {
                .AtmosphereMass     = Other.ExtraProperties_.AtmosphereMass,
                .CoreMass           = Other.ExtraProperties_.CoreMass,
                .OceanMass          = Other.ExtraProperties_.OceanMass,
                .CrustMineralMass   = Other.ExtraProperties_.CrustMineralMass,
                .CivilizationData   = Other.ExtraProperties_.CivilizationData
                                    ? std::make_unique<Intelli::FStandard>(*Other.ExtraProperties_.CivilizationData)
                                    : nullptr,
                .BalanceTemperature = Other.ExtraProperties_.BalanceTemperature,
                .Type               = Other.ExtraProperties_.Type,
                .bIsMigrated        = Other.ExtraProperties_.bIsMigrated
            };
        }

        return *this;
    }

    AAsteroidCluster::AAsteroidCluster(const FBasicProperties& Properties)
        : Properties_(Properties)
    {
    }
} // namespace Npgs::Astro
