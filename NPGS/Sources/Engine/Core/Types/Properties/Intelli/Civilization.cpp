#include "stdafx.h"
#include "Civilization.h"

namespace Npgs::Intelli
{
    FStandard::FStandard(const FLifeProperties& LifeProperties, const FCivilizationProperties& CivilizationProperties)
        : LifeProperties_(LifeProperties), CivilizationProperties_(CivilizationProperties)
    {
    }

    const float FStandard::kNull_                       = 0.0f;
    const float FStandard::kInitialGeneralIntelligence_ = 1.0f;
    const float FStandard::kUrgesellschaft_             = 2.0f;
    const float FStandard::kEarlyIndustrielle_          = 3.0f;
    const float FStandard::kSteamAge_                   = 4.0f;
    const float FStandard::kElectricAge_                = 5.0f;
    const float FStandard::kAtomicAge_                  = 6.0f;
    const float FStandard::kDigitalAge_                 = 7.0f;
    const float FStandard::kEarlyAsiAge_                = 8.0f;
} // namespace Npgs::Intelli
