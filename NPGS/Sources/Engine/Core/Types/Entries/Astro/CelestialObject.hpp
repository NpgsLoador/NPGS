#pragma once

#include <string>
#include <glm/glm.hpp>
#include "Engine/Core/Types/Entries/NpgsObject.hpp"

namespace Npgs::Astro
{
    class IAstroObject : public INpgsObject
    {
    public:
        IAstroObject()                    = default;
		IAstroObject(const IAstroObject&) = default;
		IAstroObject(IAstroObject&&)      = default;
        virtual ~IAstroObject()           = default;

		IAstroObject& operator=(const IAstroObject&) = default;
		IAstroObject& operator=(IAstroObject&&)      = default;
    };

    class FCelestialBody : public IAstroObject
    {
    public:
        struct FBasicProperties
        {
            std::string Name;        // 名字
            glm::vec2   Normal{};    // 法向量，球坐标表示，(theta, phi)

            double Age{};            // 年龄，单位年
            float  Radius{};         // 半径，单位 m
            float  Spin{};           // 对于普通天体表示自转周期，单位 s；对于黑洞表示无量纲自旋参数
            float  Oblateness{};     // 扁率
            float  EscapeVelocity{}; // 逃逸速度，单位 m/s
            float  MagneticField{};  // 磁场强度，单位 T
        };

    public:
        using Base = IAstroObject;
        using Base::Base;

        FCelestialBody()                          = default;
        FCelestialBody(const FBasicProperties& Properties);
        FCelestialBody(const FCelestialBody&)     = default;
        FCelestialBody(FCelestialBody&&) noexcept = default;
        virtual ~FCelestialBody() override        = default;

        FCelestialBody& operator=(const FCelestialBody&)     = default;
        FCelestialBody& operator=(FCelestialBody&&) noexcept = default;

        const FBasicProperties& GetBasicProperties() const;
        FCelestialBody&         SetBasicProperties(const FBasicProperties& Properties);

        const std::string& GetName() const;
        FCelestialBody&    SetName(std::string Name);
        glm::vec2          GetNormal() const;
        FCelestialBody&    SetNormal(glm::vec2 Normal);
        double             GetAge() const;
        FCelestialBody&    SetAge(double Age);
        float              GetRadius() const;
        FCelestialBody&    SetRadius(float Radius);
        float              GetSpin() const;
        FCelestialBody&    SetSpin(float Spin);
        float              GetOblateness() const;
        FCelestialBody&    SetOblateness(float Oblateness);
        float              GetEscapeVelocity() const;
        FCelestialBody&    SetEscapeVelocity(float EscapeVelocity);
        float              GetMagneticField() const;
        FCelestialBody&    SetMagneticField(float MagneticField);


    private:
        FBasicProperties Properties_{};
    };
} // namespace Npgs::Astro

#include "CelestialObject.inl"
