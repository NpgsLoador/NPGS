#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/Core/Types/Entries/Astro/CelestialObject.h"
#include "Engine/Core/Types/Entries/Astro/Planet.h"
#include "Engine/Core/Types/Entries/Astro/Star.h"
#include "Engine/Core/Types/Entries/NpgsObject.h"
#include "Engine/Core/Types/Properties/Intelli/Artifact.h"

namespace Npgs::Astro
{
    struct FBaryCenter : public INpgsObject
    {
        std::string Name;           // 质心名字
        glm::vec3   Position{};     // 位置，使用 3 个 float 分量的向量存储
        glm::vec2   Normal{};       // 法向量，(theta, phi)
        std::size_t DistanceRank{}; // 距离 (0, 0, 0) 的排名

        FBaryCenter() = default;
        FBaryCenter(glm::vec3 Position, glm::vec2 Normal, std::size_t DistanceRank, const std::string& Name);
    };

    class FOrbit
    {
    public:
        using FObjectPointer = std::variant<FBaryCenter*, AStar*, APlanet*, AAsteroidCluster*, Intelli::AArtifact*>;

        enum class EObjectType : std::uint8_t
        {
            kBaryCenter,
            kStar,
            kPlanet,
            kAsteroidCluster,
            kArtifactCluster
        };

        struct FKeplerElements
        {
            float SemiMajorAxis{};            // 半长轴，单位 AU
            float Eccentricity{};             // 离心率
            float Inclination{};              // 轨道倾角，单位 rad
            float LongitudeOfAscendingNode{}; // 升交点经度，单位 rad
            float ArgumentOfPeriapsis{};      // 近心点幅角，单位 rad
            float TrueAnomaly{};              // 真近点角，单位 rad
        };

        class FOrbitalObject // 轨道上存储的天体信息
        {
        public:
            FOrbitalObject();
            FOrbitalObject(INpgsObject* Object, EObjectType Type);
            ~FOrbitalObject() = default;

            template <typename ObjectType>
            requires std::is_class_v<ObjectType>
            FOrbitalObject& SetObject(ObjectType* Object);

            EObjectType GetObjectType() const;

            template <typename ObjectType>
            requires std::is_class_v<ObjectType>
            ObjectType* GetObject() const;

        private:
            FObjectPointer Object_; // 指向天体数组中该天体的指针
            EObjectType    Type_;   // 天体类型，用于区分 union
        };

        class FOrbitalDetails // 轨道信息
        {
        public:
            FOrbitalDetails();
            FOrbitalDetails(INpgsObject* Object, EObjectType Type, FOrbit* HostOrbit, float InitialTrueAnomaly = 0.0f);
            ~FOrbitalDetails() = default;

            FOrbitalDetails& SetHostOrbit(FOrbit* HostOrbit);
            FOrbitalDetails& SetOrbitalObject(INpgsObject* Object, EObjectType Type);
            FOrbitalDetails& SetInitialTrueAnomaly(float InitialTrueAnomaly);

            FOrbit* GetHostOrbit();
            FOrbitalObject& GetOrbitalObject();
            float GetInitialTrueAnomaly() const;

            std::vector<FOrbit*>& DirectOrbitsData();

        private:
            std::vector<FOrbit*> DirectOrbits_;       // 直接下级轨道
            FOrbitalObject       Object_;             // 天体信息
            FOrbit*              HostOrbit_;          // 所在轨道
            float                InitialTrueAnomaly_; // 初始真近点角，单位 rad
        };

    public:
        FOrbit() = default;
        ~FOrbit() = default;

        FOrbit& SetSemiMajorAxis(float SemiMajorAxis);
        FOrbit& SetEccentricity(float Eccentricity);
        FOrbit& SetInclination(float Inclination);
        FOrbit& SetLongitudeOfAscendingNode(float LongitudeOfAscendingNode);
        FOrbit& SetArgumentOfPeriapsis(float ArgumentOfPeriapsis);
        FOrbit& SetTrueAnomaly(float TrueAnomaly);
        FOrbit& SetParent(INpgsObject* Object, EObjectType Type);
        FOrbit& SetNormal(glm::vec2 Normal);
        FOrbit& SetPeriod(float Period);

        float GetSemiMajorAxis() const;
        float GetEccentricity() const;
        float GetInclination() const;
        float GetLongitudeOfAscendingNode() const;
        float GetArgumentOfPeriapsis() const;
        float GetTrueAnomaly() const;
        const FOrbitalObject& GetParent() const;
        glm::vec2 GetNormal() const;
        float GetPeriod() const;

        std::vector<FOrbitalDetails>& ObjectsData();

    private:
        std::vector<FOrbitalDetails> Objects_;  // 每条轨道信息，上有存储的天体
        FKeplerElements              OrbitElements_;
        FOrbitalObject               Parent_;   // 上级天体
        glm::vec2                    Normal_{}; // 轨道法向量 (theta, phi)
        float                        Period_{}; // 轨道周期，单位 s
    };

    class FStellarSystem : public INpgsObject
    {
    public:
        using Base = INpgsObject;
        using Base::Base;

        FStellarSystem()                          = default;
        FStellarSystem(const FBaryCenter& SystemBary);
        FStellarSystem(const FStellarSystem& Other);
        FStellarSystem(FStellarSystem&&) noexcept = default;
        ~FStellarSystem()                         = default;

        FStellarSystem& operator=(const FStellarSystem& Other);
        FStellarSystem& operator=(FStellarSystem&&) noexcept = default;

        FStellarSystem& SetBaryPosition(glm::vec3 Poisition);
        FStellarSystem& SetBaryNormal(glm::vec2 Normal);
        FStellarSystem& SetBaryDistanceRank(std::size_t DistanceRank);
        FStellarSystem& SetBaryName(const std::string& Name);

        glm::vec3 GetBaryPosition() const;
        glm::vec2 GetBaryNormal() const;
        std::size_t GetBaryDistanceRank() const;
        const std::string& GetBaryName() const;

        FBaryCenter* GetBaryCenter();
        std::vector<std::unique_ptr<Astro::AStar>>& StarsData();
        std::vector<std::unique_ptr<Astro::APlanet>>& PlanetsData();
        std::vector<std::unique_ptr<Astro::AAsteroidCluster>>& AsteroidClustersData();
        std::vector<std::unique_ptr<FOrbit>>& OrbitsData();

    private:
        FBaryCenter                                           SystemBary_;
        std::vector<std::unique_ptr<Astro::AStar>>            Stars_;
        std::vector<std::unique_ptr<Astro::APlanet>>          Planets_;
        std::vector<std::unique_ptr<Astro::AAsteroidCluster>> AsteroidClusters_;
        std::vector<std::unique_ptr<FOrbit>>                  Orbits_;
    };
}

#include "StellarSystem.inl"
