#include "Engine/Core/Base/Base.hpp"

namespace Npgs::Astro
{
    NPGS_INLINE FCelestialBody& FCelestialBody::SetBasicProperties(const FBasicProperties& Properties)
    {
        Properties_ = Properties;
        return *this;
    }

    NPGS_INLINE const FCelestialBody::FBasicProperties& FCelestialBody::GetBasicProperties() const
    {
        return Properties_;
    }

    NPGS_INLINE FCelestialBody& FCelestialBody::SetName(const std::string& Name)
    {
        Properties_.Name = Name;
        return *this;
    }

    NPGS_INLINE FCelestialBody& FCelestialBody::SetNormal(glm::vec2 Normal)
    {
        Properties_.Normal = Normal;
        return *this;
    }

    NPGS_INLINE FCelestialBody& FCelestialBody::SetAge(double Age)
    {
        Properties_.Age = Age;
        return *this;
    }

    NPGS_INLINE FCelestialBody& FCelestialBody::SetRadius(float Radius)
    {
        Properties_.Radius = Radius;
        return *this;
    }

    NPGS_INLINE FCelestialBody& FCelestialBody::SetSpin(float Spin)
    {
        Properties_.Spin = Spin;
        return *this;
    }

    NPGS_INLINE FCelestialBody& FCelestialBody::SetOblateness(float Oblateness)
    {
        Properties_.Oblateness = Oblateness;
        return *this;
    }

    NPGS_INLINE FCelestialBody& FCelestialBody::SetEscapeVelocity(float EscapeVelocity)
    {
        Properties_.EscapeVelocity = EscapeVelocity;
        return *this;
    }

    NPGS_INLINE FCelestialBody& FCelestialBody::SetMagneticField(float MagneticField)
    {
        Properties_.MagneticField = MagneticField;
        return *this;
    }

    NPGS_INLINE const std::string& FCelestialBody::GetName() const
    {
        return Properties_.Name;
    }

    NPGS_INLINE glm::vec2 FCelestialBody::GetNormal() const
    {
        return Properties_.Normal;
    }

    NPGS_INLINE double FCelestialBody::GetAge() const
    {
        return Properties_.Age;
    }

    NPGS_INLINE float FCelestialBody::GetRadius() const
    {
        return Properties_.Radius;
    }

    NPGS_INLINE float FCelestialBody::GetSpin() const
    {
        return Properties_.Spin;
    }

    NPGS_INLINE float FCelestialBody::GetOblateness() const
    {
        return Properties_.Oblateness;
    }

    NPGS_INLINE float FCelestialBody::GetEscapeVelocity() const
    {
        return Properties_.EscapeVelocity;
    }

    NPGS_INLINE float FCelestialBody::GetMagneticField() const
    {
        return Properties_.MagneticField;
    }
} // namespace Npgs::Astro
