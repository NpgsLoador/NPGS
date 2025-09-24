#include "stdafx.h"
#include "StellarSystem.h"

namespace Npgs::Astro
{
    FBaryCenter::FBaryCenter(glm::vec3 Position, glm::vec2 Normal, std::size_t DistanceRank, const std::string& Name)
        : Name(Name), Position(Position), Normal(Normal), DistanceRank(DistanceRank)
    {
    }

    FOrbit::FOrbitalObject::FOrbitalObject()
        : Object_{}, Type_(EObjectType::kBaryCenter)
    {
    }

    FOrbit::FOrbitalObject::FOrbitalObject(INpgsObject* Object, EObjectType Type)
        : Type_(Type)
    {
        switch (Type)
        {
        case EObjectType::kBaryCenter:
            Object_ = static_cast<FBaryCenter*>(Object);
            break;
        case EObjectType::kStar:
            Object_ = static_cast<AStar*>(Object);
            break;
        case EObjectType::kPlanet:
            Object_ = static_cast<APlanet*>(Object);
            break;
        case EObjectType::kAsteroidCluster:
            Object_ = static_cast<AAsteroidCluster*>(Object);
            break;
        case EObjectType::kArtifactCluster:
            Object_ = static_cast<Intelli::AArtifact*>(Object);
            break;
        }
    }

    FOrbit::FOrbitalDetails::FOrbitalDetails()
        : Object_{}, HostOrbit_(nullptr), InitialTrueAnomaly_(0.0f)
    {
    }

    FOrbit::FOrbitalDetails::FOrbitalDetails(INpgsObject* Object, EObjectType Type, FOrbit* HostOrbit, float InitialTrueAnomaly)
        : Object_(FOrbitalObject(Object, Type)), HostOrbit_(HostOrbit), InitialTrueAnomaly_(InitialTrueAnomaly)
    {
    }

    FOrbit::FOrbitalDetails& FOrbit::FOrbitalDetails::SetOrbitalObject(INpgsObject* Object, EObjectType Type)
    {
        switch (Type)
        {
        case EObjectType::kBaryCenter:
            Object_.SetObject(static_cast<FBaryCenter*>(Object));
            break;
        case EObjectType::kStar:
            Object_.SetObject(static_cast<AStar*>(Object));
            break;
        case EObjectType::kPlanet:
            Object_.SetObject(static_cast<APlanet*>(Object));
            break;
        case EObjectType::kAsteroidCluster:
            Object_.SetObject(static_cast<AAsteroidCluster*>(Object));
            break;
        case EObjectType::kArtifactCluster:
            Object_.SetObject(static_cast<Intelli::AArtifact*>(Object));
            break;
        default:
            break;
        }

        return *this;
    }

    FOrbit& FOrbit::SetParent(INpgsObject* Object, EObjectType Type)
    {
        switch (Type)
        {
        case EObjectType::kBaryCenter:
            Parent_.SetObject(static_cast<FBaryCenter*>(Object));
            break;
        case EObjectType::kStar:
            Parent_.SetObject(static_cast<AStar*>(Object));
            break;
        case EObjectType::kPlanet:
            Parent_.SetObject(static_cast<APlanet*>(Object));
            break;
        case EObjectType::kAsteroidCluster:
            Parent_.SetObject(static_cast<AAsteroidCluster*>(Object));
            break;
        case EObjectType::kArtifactCluster:
            Parent_.SetObject(static_cast<Intelli::AArtifact*>(Object));
            break;
        default:
            break;
        }

        return *this;
    }

    FStellarSystem::FStellarSystem(const FBaryCenter& SystemBary)
        : SystemBary_(SystemBary)
    {
    }

    FStellarSystem::FStellarSystem(const FStellarSystem& Other)
        : SystemBary_(Other.SystemBary_)
    {
        for (const auto& Star : Other.Stars_)
        {
            Stars_.push_back(std::make_unique<AStar>(*Star));
        }
        for (const auto& Planet : Other.Planets_)
        {
            Planets_.push_back(std::make_unique<APlanet>(*Planet));
        }
        for (const auto& AsteroidCluster : Other.AsteroidClusters_)
        {
            AsteroidClusters_.push_back(std::make_unique<AAsteroidCluster>(*AsteroidCluster));
        }
        for (const auto& Orbit : Other.Orbits_)
        {
            Orbits_.push_back(std::make_unique<FOrbit>(*Orbit));
        }
    }

    FStellarSystem& FStellarSystem::operator=(const FStellarSystem& Other)
    {
        if (this != &Other)
        {
            SystemBary_ = Other.SystemBary_;

            Stars_.clear();
            for (const auto& Star : Other.Stars_)
            {
                Stars_.push_back(std::make_unique<AStar>(*Star));
            }

            Planets_.clear();
            for (const auto& Planet : Other.Planets_)
            {
                Planets_.push_back(std::make_unique<APlanet>(*Planet));
            }

            AsteroidClusters_.clear();
            for (const auto& AsteroidCluster : Other.AsteroidClusters_)
            {
                AsteroidClusters_.push_back(std::make_unique<AAsteroidCluster>(*AsteroidCluster));
            }

            Orbits_.clear();
            for (const auto& Orbit : Other.Orbits_)
            {
                Orbits_.push_back(std::make_unique<FOrbit>(*Orbit));
            }
        }

        return *this;
    }
} // namespace Npgs::Astro
