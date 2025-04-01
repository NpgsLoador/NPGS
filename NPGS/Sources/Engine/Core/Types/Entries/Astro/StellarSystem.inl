#include "StellarSystem.h"

#include "Engine/Core/Base/Assert.h"
#include "Engine/Core/Base/Base.h"

namespace Npgs::Astro
{
    template <typename ObjectType>
    requires std::is_class_v<ObjectType>
    NPGS_INLINE FOrbit::FOrbitalObject& FOrbit::FOrbitalObject::SetObject(ObjectType* Object)
    {
        static_assert(std::is_same_v<ObjectType, FBaryCenter> ||
                      std::is_same_v<ObjectType, AStar> ||
                      std::is_same_v<ObjectType, APlanet> ||
                      std::is_same_v<ObjectType, AAsteroidCluster> ||
                      std::is_same_v<ObjectType, Intelli::AArtifact>,
                      "Invalid object type for SetObject");

        _Object = Object;

        if constexpr (std::is_same_v<ObjectType, FBaryCenter>)
            _Type = EObjectType::kBaryCenter;
        else if constexpr (std::is_same_v<ObjectType, AStar>)
            _Type = EObjectType::kStar;
        else if constexpr (std::is_same_v<ObjectType, APlanet>)
            _Type = EObjectType::kPlanet;
        else if constexpr (std::is_same_v<ObjectType, AAsteroidCluster>)
            _Type = EObjectType::kAsteroidCluster;
        else if constexpr (std::is_same_v<ObjectType, Intelli::AArtifact>)
            _Type = EObjectType::kArtifactCluster;

        return *this;
    }

    NPGS_INLINE FOrbit::EObjectType FOrbit::FOrbitalObject::GetObjectType() const
    {
        return _Type;
    }

    template <typename ObjectType>
    requires std::is_class_v<ObjectType>
    NPGS_INLINE ObjectType* FOrbit::FOrbitalObject::GetObject() const
    {
        static_assert(std::is_same_v<ObjectType, FBaryCenter> ||
                      std::is_same_v<ObjectType, AStar> ||
                      std::is_same_v<ObjectType, APlanet> ||
                      std::is_same_v<ObjectType, AAsteroidCluster> ||
                      std::is_same_v<ObjectType, Intelli::AArtifact>,
                      "Invalid object type for SetObject");

        return std::get<ObjectType*>(_Object);
    }

    NPGS_INLINE FOrbit::FOrbitalDetails& FOrbit::FOrbitalDetails::SetHostOrbit(FOrbit* HostFOrbit)
    {
        _HostOrbit = HostFOrbit;
        return *this;
    }

    NPGS_INLINE FOrbit::FOrbitalDetails& FOrbit::FOrbitalDetails::SetInitialTrueAnomaly(float InitialTrueAnomaly)
    {
        _InitialTrueAnomaly = InitialTrueAnomaly;
        return *this;
    }

    NPGS_INLINE FOrbit* FOrbit::FOrbitalDetails::GetHostOrbit()
    {
        return _HostOrbit;
    }

    NPGS_INLINE FOrbit::FOrbitalObject& FOrbit::FOrbitalDetails::GetOrbitalObject()
    {
        return _Object;
    }

    NPGS_INLINE float FOrbit::FOrbitalDetails::GetInitialTrueAnomaly() const
    {
        return _InitialTrueAnomaly;
    }

    NPGS_INLINE std::vector<FOrbit*>& FOrbit::FOrbitalDetails::DirectOrbitsData()
    {
        return _DirectOrbits;
    }

    NPGS_INLINE FOrbit& FOrbit::SetSemiMajorAxis(float SemiMajorAxis)
    {
        _OrbitElements.SemiMajorAxis = SemiMajorAxis;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetEccentricity(float Eccentricity)
    {
        _OrbitElements.Eccentricity = Eccentricity;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetInclination(float Inclination)
    {
        _OrbitElements.Inclination = Inclination;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetLongitudeOfAscendingNode(float LongitudeOfAscendingNode)
    {
        _OrbitElements.LongitudeOfAscendingNode = LongitudeOfAscendingNode;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetArgumentOfPeriapsis(float ArgumentOfPeriapsis)
    {
        _OrbitElements.ArgumentOfPeriapsis = ArgumentOfPeriapsis;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetTrueAnomaly(float TrueAnomaly)
    {
        _OrbitElements.TrueAnomaly = TrueAnomaly;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetNormal(glm::vec2 Normal)
    {
        _Normal = Normal;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetPeriod(float Period)
    {
        _Period = Period;
        return *this;
    }

    NPGS_INLINE float FOrbit::GetSemiMajorAxis() const
    {
        return _OrbitElements.SemiMajorAxis;
    }

    NPGS_INLINE float FOrbit::GetEccentricity() const
    {
        return _OrbitElements.Eccentricity;
    }

    NPGS_INLINE float FOrbit::GetInclination() const
    {
        return _OrbitElements.Inclination;
    }

    NPGS_INLINE float FOrbit::GetLongitudeOfAscendingNode() const
    {
        return _OrbitElements.LongitudeOfAscendingNode;
    }

    NPGS_INLINE float FOrbit::GetArgumentOfPeriapsis() const
    {
        return _OrbitElements.ArgumentOfPeriapsis;
    }

    NPGS_INLINE float FOrbit::GetTrueAnomaly() const
    {
        return _OrbitElements.TrueAnomaly;
    }

    NPGS_INLINE const FOrbit::FOrbitalObject& FOrbit::GetParent() const
    {
        return _Parent;
    }

    NPGS_INLINE glm::vec2 FOrbit::GetNormal() const
    {
        return _Normal;
    }

    NPGS_INLINE float FOrbit::GetPeriod() const
    {
        return _Period;
    }

    NPGS_INLINE std::vector<FOrbit::FOrbitalDetails>& FOrbit::ObjectsData()
    {
        return _Objects;
    }

    NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryPosition(glm::vec3 Position)
    {
        _SystemBary.Position = Position;
        return *this;
    }

    NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryNormal(glm::vec2 Normal)
    {
        _SystemBary.Normal = Normal;
        return *this;
    }

    NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryDistanceRank(std::size_t DistanceRank)
    {
        _SystemBary.DistanceRank = DistanceRank;
        return *this;
    }

    NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryName(const std::string& Name)
    {
        _SystemBary.Name = Name;
        return *this;
    }

    NPGS_INLINE glm::vec3 FStellarSystem::GetBaryPosition() const
    {
        return _SystemBary.Position;
    }

    NPGS_INLINE glm::vec2 FStellarSystem::GetBaryNormal() const
    {
        return _SystemBary.Normal;
    }

    NPGS_INLINE std::size_t FStellarSystem::GetBaryDistanceRank() const
    {
        return _SystemBary.DistanceRank;
    }

    NPGS_INLINE const std::string& FStellarSystem::GetBaryName() const
    {
        return _SystemBary.Name;
    }

    NPGS_INLINE FBaryCenter* FStellarSystem::GetBaryCenter()
    {
        return &_SystemBary;
    }

    NPGS_INLINE std::vector<std::unique_ptr<Astro::AStar>>& FStellarSystem::StarsData()
    {
        return _Stars;
    }

    NPGS_INLINE std::vector<std::unique_ptr<Astro::APlanet>>& FStellarSystem::PlanetsData()
    {
        return _Planets;
    }

    NPGS_INLINE std::vector<std::unique_ptr<Astro::AAsteroidCluster>>& FStellarSystem::AsteroidClustersData()
    {
        return _AsteroidClusters;
    }

    NPGS_INLINE std::vector<std::unique_ptr<FOrbit>>& FStellarSystem::OrbitsData()
    {
        return _Orbits;
    }
} // namespace Npgs::Astro
