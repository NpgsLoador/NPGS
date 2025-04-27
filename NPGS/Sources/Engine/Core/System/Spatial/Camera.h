#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Npgs
{
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
        FCamera(glm::vec3 Position = glm::vec3(0.0f), float Sensitivity = 0.05f, float Speed = 2.5f,
                float Zoom = 60.0f, float InertiaDecay = 0.5, float SmoothCoefficient = 0.2);

        ~FCamera() = default;

        void AlignCamera();
        void ProcessKeyboard(EMovement Direction, double DeltaTime);
        void ProcessMouseScroll(double OffsetY);
        void ProcessEvent(double DeltaTime);

        void SetOrientation(glm::quat Orientation);
        void SetVector(EVector Vector, glm::vec3 NewVector);
        void SetOrbitTarget(glm::vec3 Target);
        void SetOrbitAxis(glm::vec3 Axis);
        void SetTargetOffset(glm::vec2 Offset);
        void SetZoom(float Zoom);
        void SetMode(EMode Mode);

        glm::quat GetOrientation() const;
        glm::vec3 GetVector(EVector Vector) const;
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
        EMode Mode_{ EMode::kFree };

        // Camera status parameters
        glm::quat Orientation_{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 Position_;
        glm::vec3 Front_;
        glm::vec3 Up_;
        glm::vec3 Right_;
        glm::vec3 WorldUp_;

        glm::vec3 OrbitTarget_{};
        glm::vec3 OrbitAxis_{};
        glm::vec2 PrevOffset_{};
        glm::vec2 TargetOffset_{};
        glm::vec2 OrbitalVelocity_{};

        float InertiaDecay_{ 0.5f };
        float VelocityThreshold_{ 0.001f };

        float Sensitivity_;
        float Speed_;
        float Zoom_;
        float SmoothCoefficient_;
        float OrbitRadius_{};

        bool bCameraAligned_{ false };
    };
} // namespace Npgs

#include "Camera.inl"
