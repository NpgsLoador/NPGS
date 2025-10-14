#include <utility>
#include "Engine/Core/Base/Base.hpp"

namespace Npgs::Astro
{
    template <typename ObjectType>
    requires std::is_class_v<ObjectType>
    FOrbit::FOrbitalObject& FOrbit::FOrbitalObject::SetObject(ObjectType* Object)
    {
        static_assert(std::is_same_v<ObjectType, FBaryCenter> ||
                      std::is_same_v<ObjectType, AStar> ||
                      std::is_same_v<ObjectType, APlanet> ||
                      std::is_same_v<ObjectType, AAsteroidCluster> ||
                      std::is_same_v<ObjectType, Intelli::AArtifact>,
                      "Invalid object type for SetObject");

        Object_ = Object;

        if constexpr (std::is_same_v<ObjectType, FBaryCenter>)
            Type_ = EObjectType::kBaryCenter;
        else if constexpr (std::is_same_v<ObjectType, AStar>)
            Type_ = EObjectType::kStar;
        else if constexpr (std::is_same_v<ObjectType, APlanet>)
            Type_ = EObjectType::kPlanet;
        else if constexpr (std::is_same_v<ObjectType, AAsteroidCluster>)
            Type_ = EObjectType::kAsteroidCluster;
        else if constexpr (std::is_same_v<ObjectType, Intelli::AArtifact>)
            Type_ = EObjectType::kArtifactCluster;

        return *this;
    }

    NPGS_INLINE FOrbit::EObjectType FOrbit::FOrbitalObject::GetObjectType() const
    {
        return Type_;
    }

    template <typename ObjectType>
    requires std::is_class_v<ObjectType>
    ObjectType* FOrbit::FOrbitalObject::GetObject() const
    {
        static_assert(std::is_same_v<ObjectType, FBaryCenter> ||
                      std::is_same_v<ObjectType, AStar> ||
                      std::is_same_v<ObjectType, APlanet> ||
                      std::is_same_v<ObjectType, AAsteroidCluster> ||
                      std::is_same_v<ObjectType, Intelli::AArtifact>,
                      "Invalid object type for SetObject");

        return std::get<ObjectType*>(Object_);
    }

    NPGS_INLINE FOrbit::FOrbitalDetails& FOrbit::FOrbitalDetails::SetHostOrbit(FOrbit* HostFOrbit)
    {
        HostOrbit_ = HostFOrbit;
        return *this;
    }

    NPGS_INLINE FOrbit::FOrbitalDetails& FOrbit::FOrbitalDetails::SetInitialTrueAnomaly(float InitialTrueAnomaly)
    {
        InitialTrueAnomaly_ = InitialTrueAnomaly;
        return *this;
    }

    NPGS_INLINE FOrbit* FOrbit::FOrbitalDetails::GetHostOrbit()
    {
        return HostOrbit_;
    }

    NPGS_INLINE FOrbit::FOrbitalObject& FOrbit::FOrbitalDetails::GetOrbitalObject()
    {
        return Object_;
    }

    NPGS_INLINE float FOrbit::FOrbitalDetails::GetInitialTrueAnomaly() const
    {
        return InitialTrueAnomaly_;
    }

    NPGS_INLINE std::vector<FOrbit*>& FOrbit::FOrbitalDetails::DirectOrbitsData()
    {
        return DirectOrbits_;
    }

    NPGS_INLINE FOrbit& FOrbit::SetSemiMajorAxis(float SemiMajorAxis)
    {
        OrbitElements_.SemiMajorAxis = SemiMajorAxis;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetEccentricity(float Eccentricity)
    {
        OrbitElements_.Eccentricity = Eccentricity;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetInclination(float Inclination)
    {
        OrbitElements_.Inclination = Inclination;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetLongitudeOfAscendingNode(float LongitudeOfAscendingNode)
    {
        OrbitElements_.LongitudeOfAscendingNode = LongitudeOfAscendingNode;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetArgumentOfPeriapsis(float ArgumentOfPeriapsis)
    {
        OrbitElements_.ArgumentOfPeriapsis = ArgumentOfPeriapsis;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetTrueAnomaly(float TrueAnomaly)
    {
        OrbitElements_.TrueAnomaly = TrueAnomaly;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetNormal(glm::vec2 Normal)
    {
        Normal_ = Normal;
        return *this;
    }

    NPGS_INLINE FOrbit& FOrbit::SetPeriod(float Period)
    {
        Period_ = Period;
        return *this;
    }

    NPGS_INLINE float FOrbit::GetSemiMajorAxis() const
    {
        return OrbitElements_.SemiMajorAxis;
    }

    NPGS_INLINE float FOrbit::GetEccentricity() const
    {
        return OrbitElements_.Eccentricity;
    }

    NPGS_INLINE float FOrbit::GetInclination() const
    {
        return OrbitElements_.Inclination;
    }

    NPGS_INLINE float FOrbit::GetLongitudeOfAscendingNode() const
    {
        return OrbitElements_.LongitudeOfAscendingNode;
    }

    NPGS_INLINE float FOrbit::GetArgumentOfPeriapsis() const
    {
        return OrbitElements_.ArgumentOfPeriapsis;
    }

    NPGS_INLINE float FOrbit::GetTrueAnomaly() const
    {
        return OrbitElements_.TrueAnomaly;
    }

    NPGS_INLINE const FOrbit::FOrbitalObject& FOrbit::GetParent() const
    {
        return Parent_;
    }

    NPGS_INLINE glm::vec2 FOrbit::GetNormal() const
    {
        return Normal_;
    }

    NPGS_INLINE float FOrbit::GetPeriod() const
    {
        return Period_;
    }

    NPGS_INLINE std::vector<FOrbit::FOrbitalDetails>& FOrbit::ObjectsData()
    {
        return Objects_;
    }

    NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryPosition(glm::vec3 Position)
    {
        SystemBary_.Position = Position;
        return *this;
    }

    NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryNormal(glm::vec2 Normal)
    {
        SystemBary_.Normal = Normal;
        return *this;
    }

    NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryDistanceRank(std::size_t DistanceRank)
    {
        SystemBary_.DistanceRank = DistanceRank;
        return *this;
    }

    NPGS_INLINE FStellarSystem& FStellarSystem::SetBaryName(std::string Name)
    {
        SystemBary_.Name = std::move(Name);
        return *this;
    }

    NPGS_INLINE glm::vec3 FStellarSystem::GetBaryPosition() const
    {
        return SystemBary_.Position;
    }

    NPGS_INLINE glm::vec2 FStellarSystem::GetBaryNormal() const
    {
        return SystemBary_.Normal;
    }

    NPGS_INLINE std::size_t FStellarSystem::GetBaryDistanceRank() const
    {
        return SystemBary_.DistanceRank;
    }

    NPGS_INLINE const std::string& FStellarSystem::GetBaryName() const
    {
        return SystemBary_.Name;
    }

    NPGS_INLINE FBaryCenter* FStellarSystem::GetBaryCenter()
    {
        return &SystemBary_;
    }

    NPGS_INLINE std::vector<std::unique_ptr<Astro::AStar>>& FStellarSystem::StarsData()
    {
        return Stars_;
    }

    NPGS_INLINE std::vector<std::unique_ptr<Astro::APlanet>>& FStellarSystem::PlanetsData()
    {
        return Planets_;
    }

    NPGS_INLINE std::vector<std::unique_ptr<Astro::AAsteroidCluster>>& FStellarSystem::AsteroidClustersData()
    {
        return AsteroidClusters_;
    }

    NPGS_INLINE std::vector<std::unique_ptr<FOrbit>>& FStellarSystem::OrbitsData()
    {
        return Orbits_;
    }
} // namespace Npgs::Astro
