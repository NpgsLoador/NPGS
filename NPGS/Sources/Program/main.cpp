#include "stdafx.h"
#include "Npgs.hpp"
#include "Application.hpp"

using namespace Npgs;
using namespace Npgs::Utils;

void Declare();

int main1()
{
    FLogger::Initialize();
    FEngineServices::GetInstance()->InitializeCoreServices();

    FApplication App({ 1280, 960 }, "Learn glNext FPS:", false, false, true);
    App.ExecuteMainRender();

    // FEngineServices::GetInstance()->ShutdownCoreServices();
    
    return 0;
}

#pragma warning(disable : 4251)

#define NPGS_ENABLE_CONSOLE_LOGGER
#include "Universe.hpp"

int main() {
    using namespace Npgs;
    using namespace Npgs::Astro;
    using namespace Npgs::Utils;

    FLogger::Initialize();
    FEngineServices::GetInstance()->InitializeCoreServices();

    int Option = 0;
    std::cin >> Option;
    switch (Option)
    {
    case 1:
    {
        std::println("Enter the system count:");
        std::size_t StarCount = 0;
        std::cin >> StarCount;

        std::println("Enter the seed:");
        unsigned Seed = 0;
        std::cin >> Seed;

        try
        {
            FUniverse Space(Seed, StarCount, StarCount / 2, StarCount / 2);
            Space.FillUniverse();
            Space.CountStars();
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
        }

        break;
    }
    case 2:
    {
        FStellarGenerationInfo TestGeneratorInfo;
        TestGeneratorInfo.SeedSequence = new std::seed_seq({ 42 });

        FStellarGenerator TestGenerator(TestGeneratorInfo);
        FStellarBasicProperties Properties;
        Properties.Age = 1e7f;
        Properties.FeH = 0.0f;
        Properties.InitialMassSol = 20.0f;

        auto Star = TestGenerator.GenerateStar(Properties);

        std::println("{:>6} {:>6} {:>8} {:>8} {:7} {:>5} {:>13} {:>8} {:>8} {:>11} {:>8} {:>9} {:>5} {:>15} {:>9} {:>8}",
                     "InMass", "Mass", "Radius", "Age", "Class", "FeH", "Lum", "Teff", "CoreTemp", "CoreDensity", "Mdot", "WindSpeed", "Phase", "Magnetic", "Lifetime", "Oblateness");
        std::println(
            "{:6.2f} {:6.2f} {:8.2f} {:8.2E} {:7} {:5.2f} {:13.4f} {:8.1f} {:8.2E} {:11.2E} {:8.2E} {:9} {:5} {:15.5f} {:9.2E} {:8.2f}",
            Star.GetInitialMass() / kSolarMass,
            Star.GetMass() / kSolarMass,
            Star.GetRadius() / kSolarRadius,
            Star.GetAge(),
            Star.GetStellarClass().ToString(),
            Star.GetFeH(),
            Star.GetLuminosity() / kSolarLuminosity,
            // kSolarAbsoluteMagnitude - 2.5 * std::log10(Star.GetLuminosity() / kSolarLuminosity),
            Star.GetTeff(),
            Star.GetCoreTemp(),
            Star.GetCoreDensity(),
            Star.GetStellarWindMassLossRate() * kYearToSecond / kSolarMass,
            static_cast<int>(std::round(Star.GetStellarWindSpeed())),
            static_cast<int>(Star.GetEvolutionPhase()),
            // Star.GetSurfaceZ(),
            // Star.GetSurfaceEnergeticNuclide(),
            // Star.GetSurfaceVolatiles(),
            Star.GetMagneticField(),
            Star.GetLifetime(),
            Star.GetOblateness()
        );

        break;
    }
    }

    return 0;
}
