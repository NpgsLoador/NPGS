#include "stdafx.h"
#include "Star.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace Npgs::Astro
{
    namespace
    {
        using FSubclassResult = std::pair<const AStar::FSubclassMap*, AStar::FSubclassMap::const_iterator>;

        FSubclassResult GetCommonSubclassMap(Astro::ESpectralClass SpectralClass, float Subclass)
        {
            const AStar::FSubclassMap* SubclassMap = nullptr;

            switch (SpectralClass)
            {
            case ESpectralClass::kSpectral_O:
                SubclassMap = &AStar::kSpectralSubclassMap_O_;
                break;
            case ESpectralClass::kSpectral_B:
                SubclassMap = &AStar::kSpectralSubclassMap_B_;
                break;
            case ESpectralClass::kSpectral_A:
                SubclassMap = &AStar::kSpectralSubclassMap_A_;
                break;
            case ESpectralClass::kSpectral_F:
                SubclassMap = &AStar::kSpectralSubclassMap_F_;
                break;
            case ESpectralClass::kSpectral_G:
                SubclassMap = &AStar::kSpectralSubclassMap_G_;
                break;
            case ESpectralClass::kSpectral_K:
                SubclassMap = &AStar::kSpectralSubclassMap_K_;
                break;
            case ESpectralClass::kSpectral_M:
                SubclassMap = &AStar::kSpectralSubclassMap_M_;
                break;
            case ESpectralClass::kSpectral_L:
                SubclassMap = &AStar::kSpectralSubclassMap_L_;
                break;
            case ESpectralClass::kSpectral_T:
                SubclassMap = &AStar::kSpectralSubclassMap_T_;
                break;
            case ESpectralClass::kSpectral_Y:
                SubclassMap = &AStar::kSpectralSubclassMap_Y_;
                break;
            default:
                throw std::logic_error("Required subclass map does not exist.");
            }

            if (SubclassMap == nullptr || SubclassMap->empty())
            {
                throw std::logic_error("Required subclass is empty.");
            }

            auto it = std::ranges::find_if(*SubclassMap, [Subclass](float EntrySubclass) -> bool
            {
                return EntrySubclass == Subclass;
            }, &std::pair<int, float>::second);

            if (it == SubclassMap->end())
            {
                throw std::logic_error("Required subclass does not exist.");
            }

            return { SubclassMap, it };
        }
    }

    AStar::AStar(const FCelestialBody::FBasicProperties& BasicProperties, const FExtendedProperties& ExtraProperties)
        : FCelestialBody(BasicProperties)
        , ExtraProperties_(ExtraProperties)
    {
    }

    int AStar::GetCommonSubclassStandardTeff(Astro::ESpectralClass SpectralClass, float Subclass)
    {
        auto [_, SubclassIt] = GetCommonSubclassMap(SpectralClass, Subclass);
        return SubclassIt->first;
    }

    int AStar::GetCommonSubclassUpperTeff(Astro::ESpectralClass SpectralClass, float Subclass)
    {
        auto [SubclassMap, SubclassIt] = GetCommonSubclassMap(SpectralClass, Subclass);

        int CurrentTeff  = SubclassIt->first;
        int PreviousTeff = 0;

        if (SubclassIt != SubclassMap->begin())
        {
            PreviousTeff = std::prev(SubclassIt)->first;
        }
        else
        {
            const FSubclassMap* PreviousMap = nullptr;

            switch (SpectralClass)
            {
            case ESpectralClass::kSpectral_B:
                PreviousMap = &kSpectralSubclassMap_O_;
                break;
            case ESpectralClass::kSpectral_A:
                PreviousMap = &kSpectralSubclassMap_B_;
                break;
            case ESpectralClass::kSpectral_F:
                PreviousMap = &kSpectralSubclassMap_A_;
                break;
            case ESpectralClass::kSpectral_G:
                PreviousMap = &kSpectralSubclassMap_F_;
                break;
            case ESpectralClass::kSpectral_K:
                PreviousMap = &kSpectralSubclassMap_G_;
                break;
            case ESpectralClass::kSpectral_M:
                PreviousMap = &kSpectralSubclassMap_K_;
                break;
            case ESpectralClass::kSpectral_L:
                PreviousMap = &kSpectralSubclassMap_M_;
                break;
            case ESpectralClass::kSpectral_T:
                PreviousMap = &kSpectralSubclassMap_L_;
                break;
            case ESpectralClass::kSpectral_Y:
                PreviousMap = &kSpectralSubclassMap_T_;
                break;
            default:
                return CurrentTeff;
            }

            if (PreviousMap != nullptr && !PreviousMap->empty())
            {
                PreviousTeff = PreviousMap->back().first;
            }
            else
            {
                return CurrentTeff;
            }
        }

        return (CurrentTeff + PreviousTeff) / 2;
    }

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_O_
    {
        { 54000, 2.0f },
        { 44900, 3.0f },
        { 43900, 3.5f },
        { 42900, 4.0f },
        { 42150, 4.5f },
        { 41400, 5.0f },
        { 40500, 5.5f },
        { 39500, 6.0f },
        { 38300, 6.5f },
        { 38500, 7.0f },
        { 36100, 7.5f },
        { 35100, 8.0f },
        { 34300, 8.5f },
        { 33300, 9.0f },
        { 32600, 9.2f },
        { 31900, 9.5f },
        { 31650, 9.7f }
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_B_
    {
        { 31400, 0.0f },
        { 29000, 0.5f },
        { 26000, 1.0f },
        { 24500, 1.5f },
        { 20600, 2.0f },
        { 18500, 2.5f },
        { 17000, 3.0f },
        { 16400, 4.0f },
        { 15700, 5.0f },
        { 14500, 6.0f },
        { 14000, 7.0f },
        { 12300, 8.0f },
        { 10700, 9.0f },
        { 10400, 9.5f }
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_A_
    {
        { 9700,  0.0f },
        { 9300,  1.0f },
        { 8800,  2.0f },
        { 8600,  3.0f },
        { 8250,  4.0f },
        { 8100,  5.0f },
        { 7910,  6.0f },
        { 7760,  7.0f },
        { 7590,  8.0f },
        { 7400,  9.0f }
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_F_
    {
        { 7220,  0.0f },
        { 7020,  1.0f },
        { 6820,  2.0f },
        { 6750,  3.0f },
        { 6670,  4.0f },
        { 6550,  5.0f },
        { 6350,  6.0f },
        { 6280,  7.0f },
        { 6180,  8.0f },
        { 6050,  9.0f },
        { 5990,  9.5f },
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_G_
    {
        { 5930,  0.0f },
        { 5860,  1.0f },
        { 5770,  2.0f },
        { 5720,  3.0f },
        { 5680,  4.0f },
        { 5660,  5.0f },
        { 5600,  6.0f },
        { 5550,  7.0f },
        { 5480,  8.0f },
        { 5380,  9.0f }
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_K_
    {
        { 5270,  0.0f },
        { 5170,  1.0f },
        { 5100,  2.0f },
        { 4830,  3.0f },
        { 4600,  4.0f },
        { 4440,  5.0f },
        { 4300,  6.0f },
        { 4100,  7.0f },
        { 3990,  8.0f },
        { 3930,  9.0f }
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_M_
    {
        { 3850,  0.0f },
        { 3770,  0.5f },
        { 3660,  1.0f },
        { 3620,  1.5f },
        { 3560,  2.0f },
        { 3470,  2.5f },
        { 3430,  3.0f },
        { 3270,  3.5f },
        { 3210,  4.0f },
        { 3110,  4.5f },
        { 3060,  5.0f },
        { 2930,  5.5f },
        { 2810,  6.0f },
        { 2740,  6.5f },
        { 2680,  7.0f },
        { 2630,  7.5f },
        { 2570,  8.0f },
        { 2420,  8.5f },
        { 2380,  9.0f },
        { 2350,  9.5f }
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_L_
    {
        { 2270,  0.0f },
        { 2160,  1.0f },
        { 2060,  2.0f },
        { 1920,  3.0f },
        { 1870,  4.0f },
        { 1710,  5.0f },
        { 1550,  6.0f },
        { 1530,  7.0f },
        { 1420,  8.0f },
        { 1370,  9.0f }
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_T_
    {
        { 1255,  0.0f },
        { 1240,  1.0f },
        { 1220,  2.0f },
        { 1200,  3.0f },
        { 1180,  4.0f },
        { 1170,  4.5f },
        { 1160,  5.0f },
        { 1040,  5.5f },
        { 950,   6.0f },
        { 825,   7.0f },
        { 750,   7.5f },
        { 680,   8.0f },
        { 600,   8.5f },
        { 560,   9.0f },
        { 510,   9.5f },
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_Y_
    {
        { 450,   0.0f },
        { 400,   0.5f },
        { 360,   1.0f },
        { 325,   1.5f },
        { 320,   2.0f },
        { 250,   4.0f }
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_WN_
    {
        { 141000, 2.0f },
        { 85000,  3.0f },
        { 70000,  4.0f },
        { 60000,  5.0f },
        { 56000,  6.0f },
        { 50000,  7.0f },
        { 45000,  8.0f },
        { 40000,  9.0f },
        { 25000,  10.0f },
        { 20000,  11.0f }
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_WC_
    {
        { 117000, 4.0f },
        { 83000,  5.0f },
        { 78000,  6.0f },
        { 71000,  7.0f },
        { 60000,  8.0f },
        { 44000,  9.0f },
        { 40000,  10.0f },
        { 30000,  11.0f }
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_WO_
    {
        { 220000, 1.0f },
        { 200000, 2.0f },
        { 180000, 3.0f },
        { 150000, 4.0f }
    };

    const AStar::FSubclassMap AStar::kSpectralSubclassMap_WNxh_
    {
        { 50000, 5.0f },
        { 45000, 6.0f },
        { 43000, 7.0f },
        { 40000, 8.0f },
        { 35000, 9.0f }
    };

    const std::vector<std::pair<int, const AStar::FSubclassMap&>> AStar::kInitialCommonMap_
    {
        { 54000,                                                                                             AStar::kSpectralSubclassMap_O_},
        { (AStar::kSpectralSubclassMap_O_.back().first + AStar::kSpectralSubclassMap_B_.front().first) / 2,  AStar::kSpectralSubclassMap_B_},
        { (AStar::kSpectralSubclassMap_B_.back().first + AStar::kSpectralSubclassMap_A_.front().first) / 2,  AStar::kSpectralSubclassMap_A_ },
        { (AStar::kSpectralSubclassMap_A_.back().first + AStar::kSpectralSubclassMap_F_.front().first) / 2,  AStar::kSpectralSubclassMap_F_ },
        { (AStar::kSpectralSubclassMap_F_.back().first + AStar::kSpectralSubclassMap_G_.front().first) / 2,  AStar::kSpectralSubclassMap_G_ },
        { (AStar::kSpectralSubclassMap_G_.back().first + AStar::kSpectralSubclassMap_K_.front().first) / 2,  AStar::kSpectralSubclassMap_K_ },
        { (AStar::kSpectralSubclassMap_K_.back().first + AStar::kSpectralSubclassMap_M_.front().first) / 2,  AStar::kSpectralSubclassMap_M_ },
        { (AStar::kSpectralSubclassMap_M_.back().first + AStar::kSpectralSubclassMap_L_.front().first) / 2,  AStar::kSpectralSubclassMap_L_ },
        { (AStar::kSpectralSubclassMap_L_.back().first + AStar::kSpectralSubclassMap_T_.front().first) / 2,  AStar::kSpectralSubclassMap_T_ },
        { (AStar::kSpectralSubclassMap_T_.back().first + AStar::kSpectralSubclassMap_Y_.front().first) / 2,  AStar::kSpectralSubclassMap_Y_ },
        { 0, {} }
    };

    const ankerl::unordered_dense::map<float, float> AStar::kFeHSurfaceH1Map_
    {
        { -4.0f, 0.75098f },
        { -3.0f, 0.75095f },
        { -2.0f, 0.75063f },
        { -1.5f, 0.74986f },
        { -1.0f, 0.74743f },
        { -0.5f, 0.73973f },
        {  0.0f, 0.7154f  },
        {  0.5f, 0.63846f }
    };
} // namespace Npgs::Astro
