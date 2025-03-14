#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Engine/Core/Base/Base.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_SPATIAL_BEGIN

class FCamera
{
public:
    enum class EMode
    {
        kFree, kArcBall, kAxisOrbital
    };

    enum class EMovement
    {
        kForward, kBack, kLeft, kRight, kUp, kDown, kRollLeft, kRollRight
    };

    enum class EVector
    {
        kPosition, kFront, kUp, kRight
    };

public:
    FCamera() = delete;
    FCamera(const glm::vec3& Position = glm::vec3(0.0f), float Sensitivity = 0.05f, float Speed = 2.5f,
            float Zoom = 60.0f, float InertiaDecay = 0.5, float SmoothCoefficient = 0.2);

    ~FCamera() = default;

    void AlignCamera();
    void ProcessKeyboard(EMovement Direction, double DeltaTime);
    void ProcessMouseScroll(double OffsetY);
    void ProcessEvent(double DeltaTime);

    void SetOrientation(const glm::quat& Orientation);
    void SetVector(EVector Vector, const glm::vec3& NewVector);
    void SetOrbitTarget(const glm::vec3& Target);
    void SetOrbitAxis(const glm::vec3& Axis);
    void SetTargetOffset(const glm::vec2& Offset);
    void SetZoom(float Zoom);
    void SetMode(EMode Mode);

    const glm::quat& GetOrientation() const;
    const glm::vec3& GetVector(EVector Vector) const;
    glm::mat4x4 GetViewMatrix() const;
    glm::mat4x4 GetProjectionMatrix(float WindowAspect, float Near) const;
    float GetZoom() const;
    EMode GetMode() const;

private:
    void ProcessAlign(double DeltaTime);
    void ProcessOrient();
    void ProcessRotation(float Yaw, float Pitch, float Roll);
    void ProcessOrbital(double DeltaTime);
    void UpdateVectors();

private:
    glm::quat _Orientation{ 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 _Position;
    glm::vec3 _Front;
    glm::vec3 _Up;
    glm::vec3 _Right;
    glm::vec3 _WorldUp;

    glm::vec3 _OrbitTarget{};
    glm::vec3 _OrbitAxis{};
    glm::vec2 _PrevOffset{};
    glm::vec2 _TargetOffset{};
    glm::vec2 _OrbitalVelocity{};

    EMode _Mode{ EMode::kFree };

    float _InertiaDecay{ 0.5f };
    float _VelocityThreshold{ 0.001f };

    float _Sensitivity;
    float _Speed;
    float _Zoom;
    float _SmoothCoefficient;
    float _OrbitRadius{};

    bool _bCameraAligned{ false };
};

_SPATIAL_END
_SYSTEM_END
_NPGS_END

#include "Camera.inl"
