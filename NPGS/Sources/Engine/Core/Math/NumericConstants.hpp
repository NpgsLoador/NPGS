#pragma once

#include <cstdint>
#include <numbers>

namespace Npgs::Math
{
    // Math constants
    // --------------
    constexpr float kPi    = std::numbers::pi_v<float>;
    constexpr float kEuler = std::numbers::e_v<float>;
} // namespace Npgs:;Math

namespace Npgs
{
    // Astronomy constants
    // -------------------
    constexpr float kSolarMass                = 1.988416e30f;
    constexpr float kSolarLuminosity          = 3.828e26f;
    constexpr int   kSolarRadius              = 695700000;
    constexpr int   kSolarFeH                 = 0;
    constexpr int   kSolarTeff                = 5772;
    constexpr int   kSolarCoreTemp            = 15700000;
    constexpr float kSolarAbsoluteMagnitude   = 4.83f;
    constexpr float kSolarSurfaceGravityLogG  = 4.4371249f; // log g in cgs

    constexpr int   kSolarConstantOfEarth     = 1361;

    constexpr float kJupiterMass              = 1.8982e27f;
    constexpr int   kJupiterRadius            = 69911000;

    constexpr float kEarthMass                = 5.972168e24f;
    constexpr int   kEarthRadius              = 6371000;

    constexpr float kMoonMass                 = 7.346e22f;
    constexpr int   kMoonRadius               = 1737400;

    // Physical constants
    // ------------------
    constexpr int   kSpeedOfLight             = 299792458;
    constexpr float kGravityConstant          = 6.6743e-11f;
    constexpr float kStefanBoltzmann          = 5.6703744e-8f;
    constexpr float kVacuumPermeability       = 1.2566371e-6f;
    constexpr float kElementaryCharge         = 1.6021766e-19f;

    // Convert constants
    // -----------------
    constexpr float kSolarMassToEarth         = static_cast<float>(kSolarMass   / kEarthMass);
    constexpr float kSolarRadiusToEarth       = static_cast<float>(kSolarRadius / kEarthRadius);
    constexpr float kEarthMassToSolar         = static_cast<float>(kEarthMass   / kSolarMass);
    constexpr float kEarthRadiusToSolar       = static_cast<float>(kEarthRadius / kSolarRadius);

    constexpr int           kYearToSecond     = 31536000;
    constexpr int           kDayToSecond      = 86400;
    constexpr int           kPascalToAtm      = 101325;
    constexpr std::uint64_t kAuToMeter        = 149597870700;
    constexpr std::uint64_t kLightYearToMeter = 9460730472580800;
} // namespace Npgs
