#include "Star.h"

namespace Npgs::Astro
{
    AStar::AStar(const FCelestialBody::FBasicProperties& BasicProperties, const FExtendedProperties& ExtraProperties)
        : FCelestialBody(BasicProperties), ExtraProperties_(ExtraProperties)
    {
    }

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_O_
    {
        { 54000, 2 },
        { 44900, 3 },
        { 42900, 4 },
        { 41400, 5 },
        { 39500, 6 },
        { 38500, 7 },
        { 35100, 8 },
        { 34500, 9 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_B_
    {
        { 33400, 0 },
        { 26000, 1 },
        { 20600, 2 },
        { 17200, 3 },
        { 16400, 4 },
        { 15700, 5 },
        { 14500, 6 },
        { 14000, 7 },
        { 12300, 8 },
        { 10910, 9 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_A_
    {
        { 9900,  0 },
        { 9700,  1 },
        { 9450,  2 },
        { 8590,  3 },
        { 8300,  4 },
        { 8100,  5 },
        { 7910,  6 },
        { 7840,  7 },
        { 7700,  8 },
        { 7590,  9 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_F_
    {
        { 7200,  0 },
        { 7020,  1 },
        { 6900,  2 },
        { 6750,  3 },
        { 6670,  4 },
        { 6550,  5 },
        { 6520,  6 },
        { 6300,  7 },
        { 6260,  8 },
        { 6220,  9 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_G_
    {
        { 6100,  0 },
        { 5860,  1 },
        { 5770,  2 },
        { 5720,  3 },
        { 5680,  4 },
        { 5660,  5 },
        { 5600,  6 },
        { 5550,  7 },
        { 5480,  8 },
        { 5380,  9 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_K_
    {
        { 5260,  0 },
        { 5170,  1 },
        { 5100,  2 },
        { 4830,  3 },
        { 4600,  4 },
        { 4440,  5 },
        { 4300,  6 },
        { 4100,  7 },
        { 3990,  8 },
        { 3930,  9 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_M_
    {
        { 3850,  0 },
        { 3660,  1 },
        { 3560,  2 },
        { 3430,  3 },
        { 3210,  4 },
        { 3060,  5 },
        { 2810,  6 },
        { 2680,  7 },
        { 2570,  8 },
        { 2380,  9 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_L_
    {
        { 2270,  0 },
        { 2160,  1 },
        { 2060,  2 },
        { 1920,  3 },
        { 1870,  4 },
        { 1710,  5 },
        { 1550,  6 },
        { 1530,  7 },
        { 1420,  8 },
        { 1370,  9 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_T_
    {
        { 1255,  0 },
        { 1240,  1 },
        { 1220,  2 },
        { 1200,  3 },
        { 1180,  4 },
        { 1160,  5 },
        { 950,   6 },
        { 825,   7 },
        { 680,   8 },
        { 560,   9 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_Y_
    {
        { 450,   0 },
        { 360,   1 },
        { 320,   2 },
        { 250,   4 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_WN_
    {
        { 141000, 2 },
        { 85000,  3 },
        { 70000,  4 },
        { 60000,  5 },
        { 56000,  6 },
        { 50000,  7 },
        { 45000,  8 },
        { 40000,  9 },
        { 25000,  10 },
        { 20000,  11 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_WC_
    {
        { 117000, 4 },
        { 83000,  5 },
        { 78000,  6 },
        { 71000,  7 },
        { 60000,  8 },
        { 44000,  9 },
        { 40000,  10 },
        { 30000,  11 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_WO_
    {
        { 220000, 1 },
        { 200000, 2 },
        { 180000, 3 },
        { 150000, 4 }
    };

    const std::vector<std::pair<int, int>> AStar::kSpectralSubclassMap_WNxh_
    {
        { 50000, 5 },
        { 45000, 6 },
        { 43000, 7 },
        { 40000, 8 },
        { 35000, 9 }
    };

    const std::vector<std::pair<int, std::vector<std::pair<int, int>>>> AStar::kInitialCommonMap_
    {
        { 54000,  AStar::kSpectralSubclassMap_O_ },
        { 33400,  AStar::kSpectralSubclassMap_B_ },
        { 9900,   AStar::kSpectralSubclassMap_A_ },
        { 7200,   AStar::kSpectralSubclassMap_F_ },
        { 6100,   AStar::kSpectralSubclassMap_G_ },
        { 5260,   AStar::kSpectralSubclassMap_K_ },
        { 3850,   AStar::kSpectralSubclassMap_M_ },
        { 2270,   AStar::kSpectralSubclassMap_L_ },
        { 1255,   AStar::kSpectralSubclassMap_T_ },
        { 450,    AStar::kSpectralSubclassMap_Y_ },
        { 0, {} }
    };

    const std::vector<std::pair<int, std::vector<std::pair<int, int>>>> AStar::kInitialWolfRayetMap_
    {
        { 220000, AStar::kSpectralSubclassMap_WO_ },
        { 141000, AStar::kSpectralSubclassMap_WN_ },
        { 117000, AStar::kSpectralSubclassMap_WC_ },
        { 0, {} }
    };

    const std::unordered_map<AStar::EEvolutionPhase, Astro::FStellarClass::ELuminosityClass> AStar::kLuminosityMap_
    {
        { AStar::EEvolutionPhase::kMainSequence,     Astro::FStellarClass::ELuminosityClass::kLuminosity_V   },
        { AStar::EEvolutionPhase::kRedGiant,         Astro::FStellarClass::ELuminosityClass::kLuminosity_III },
        { AStar::EEvolutionPhase::kCoreHeBurn,       Astro::FStellarClass::ELuminosityClass::kLuminosity_IV  },
        { AStar::EEvolutionPhase::kEarlyAgb,         Astro::FStellarClass::ELuminosityClass::kLuminosity_II  },
        { AStar::EEvolutionPhase::kThermalPulseAgb,  Astro::FStellarClass::ELuminosityClass::kLuminosity_I   },
        { AStar::EEvolutionPhase::kPostAgb,          Astro::FStellarClass::ELuminosityClass::kLuminosity_I   }
    };

    const std::unordered_map<float, float> AStar::kFeHSurfaceH1Map_
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
