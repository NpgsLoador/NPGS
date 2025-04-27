#include "Camera.h"

#include <cmath>
#include <algorithm>
#include "Engine/Core/Base/Assert.h"
#include "Engine/Utils/Utils.h"

namespace Npgs
{
    FCamera::FCamera(glm::vec3 Position, float Sensitivity, float Speed, float Zoom, float InertiaDecay, float SmoothCoefficient)
        : Position_(Position)
        , Sensitivity_(Sensitivity)
        , Speed_(Speed)
        , Zoom_(Zoom)
        , InertiaDecay_(InertiaDecay)
        , SmoothCoefficient_(SmoothCoefficient)
    {
        UpdateVectors();
    }

    void FCamera::ProcessKeyboard(EMovement Direction, double DeltaTime)
    {
        float Velocity = static_cast<float>(Speed_ * DeltaTime);

        switch (Direction)
        {
        case EMovement::kForward:
            Position_ += Front_ * Velocity;
            break;
        case EMovement::kBack:
            Position_ -= Front_ * Velocity;
            break;
        case EMovement::kLeft:
            Position_ -= Right_ * Velocity;
            break;
        case EMovement::kRight:
            Position_ += Right_ * Velocity;
            break;
        case EMovement::kUp:
            Position_ += Up_ * Velocity;
            break;
        case EMovement::kDown:
            Position_ -= Up_ * Velocity;
            break;
        case EMovement::kRollLeft:
            ProcessRotation(0.0f, 0.0f, -10.0f * 2.5f * static_cast<float>(DeltaTime));
            break;
        case EMovement::kRollRight:
            ProcessRotation(0.0f, 0.0f,  10.0f * 2.5f * static_cast<float>(DeltaTime));
            break;
        }

        UpdateVectors();
    }

    void FCamera::ProcessEvent(double DeltaTime)
    {
        if (!bCameraAligned_)
        {
            ProcessAlign(DeltaTime);
        }

        if (Util::Equal(TargetOffset_.x, 0.0f) && Util::Equal(TargetOffset_.y, 0.0f))
        {
            return;
        }

        if (Mode_ == EMode::kFree)
        {
            ProcessOrient();
            TargetOffset_ = glm::vec2();
        }
        else if (Mode_ == EMode::kArcBall)
        {
            ProcessOrbital(DeltaTime);
        }
        else
        {
            ProcessOrbital(DeltaTime);
        }
    }

    void FCamera::SetVector(EVector Vector, glm::vec3 NewVector)
    {
        switch (Vector)
        {
        case EVector::kPosition:
            Position_ = NewVector;
            break;
        case EVector::kFront:
            Front_ = NewVector;
            break;
        case EVector::kUp:
            Up_ = NewVector;
            break;
        case EVector::kRight:
            Right_ = NewVector;
            break;
        default:
            NpgsAssert(false, "Invalid vector type");
        }
    }

#pragma warning(push)
#pragma warning(disable: 4715)
    glm::vec3 FCamera::GetVector(EVector Vector) const
    {
        switch (Vector)
        {
        case EVector::kPosition:
            return Position_;
        case EVector::kFront:
            return Front_;
        case EVector::kUp:
            return Up_;
        case EVector::kRight:
            return Right_;
        default:
            NpgsAssert(false, "Invalid vector type");
        }
    }
#pragma warning(pop)

    void FCamera::ProcessAlign(double DeltaTime)
    {
        glm::vec3 DesiredDirection = glm::normalize(OrbitTarget_ - Position_);

        float FrontDotTarget = glm::dot(Front_, DesiredDirection);
        bool bNeedAlignFront = (FrontDotTarget < 0.9999f);

        bool bNeedAlignUp = false;
        if (Mode_ == EMode::kAxisOrbital)
        {
            float UpDotAxis = glm::dot(Up_, OrbitAxis_);
            bNeedAlignUp = (UpDotAxis < 0.9999f);
        }

        if (!bNeedAlignFront && !bNeedAlignUp)
        {
            bCameraAligned_ = true;
            return;
        }

        glm::quat TargetOrient;

        if (Mode_ != EMode::kAxisOrbital)
        {
            glm::vec3 Direction = DesiredDirection;
            glm::vec3 Right     = glm::normalize(glm::cross(Direction, Up_));

            glm::mat3x3 RotationMatrix(Right, Up_, -Direction);
            TargetOrient = glm::conjugate(glm::quat_cast(RotationMatrix));
        }
        else
        {
            glm::vec3 Direction = DesiredDirection;
            glm::vec3 Right     = glm::normalize(glm::cross(Direction, OrbitAxis_));

            if (glm::length(Right) < 0.001f)
            {
                Right = glm::normalize(glm::cross(Direction, glm::abs(Direction.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f)));
            }

            glm::vec3 Up = glm::normalize(glm::cross(Right, Direction));

            glm::mat3x3 RotationMatrix(Right, Up, -Direction);
            TargetOrient = glm::conjugate(glm::quat_cast(RotationMatrix));
        }

        float RotationSpeed  = 3.0f;
        float RotationAmount = std::min(1.0f, static_cast<float>(DeltaTime) * RotationSpeed);

        Orientation_ = glm::normalize(glm::slerp(Orientation_, TargetOrient, RotationAmount));
        UpdateVectors();
    }

    void FCamera::ProcessOrient()
    {
        float HorizontalAngle = static_cast<float>(Sensitivity_ *  TargetOffset_.x);
        float VerticalAngle   = static_cast<float>(Sensitivity_ * -TargetOffset_.y);
        ProcessRotation(HorizontalAngle, VerticalAngle, 0.0f);
    }

    void FCamera::ProcessRotation(float Yaw, float Pitch, float Roll)
    {
        glm::quat QuatYaw   = glm::angleAxis(glm::radians(Yaw),   glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat QuatPitch = glm::angleAxis(glm::radians(Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat QuatRoll  = glm::angleAxis(glm::radians(Roll),  glm::vec3(0.0f, 0.0f, 1.0f));

        Orientation_ = glm::normalize(QuatYaw * QuatPitch * QuatRoll * Orientation_);

        UpdateVectors();
    }

    void FCamera::ProcessOrbital(double DeltaTime)
    {
        if (Mode_ == EMode::kFree)
        {
            return;
        }

        if (std::abs(TargetOffset_.x) > VelocityThreshold_ * 1000.0f || std::abs(TargetOffset_.y) > VelocityThreshold_ * 1000.0f)
        {
            float SmoothedX = Sensitivity_ * 10.0f * SmoothCoefficient_ * TargetOffset_.x + (1.0f - SmoothCoefficient_) * PrevOffset_.x;
            float SmoothedY = Sensitivity_ * 10.0f * SmoothCoefficient_ * TargetOffset_.y + (1.0f - SmoothCoefficient_) * PrevOffset_.y;

            OrbitalVelocity_ = glm::vec2(SmoothedX, SmoothedY);
            PrevOffset_      = glm::vec2(SmoothedX, SmoothedY);

            TargetOffset_ *= (1.0f - SmoothCoefficient_);
        }
        else
        {
            float DecaySpeed        = 50.0f;
            float TimeAdjustedDecay = std::pow(InertiaDecay_, static_cast<float>(DeltaTime) * DecaySpeed);
            OrbitalVelocity_ *= TimeAdjustedDecay;

            if (glm::length(OrbitalVelocity_) < VelocityThreshold_)
            {
                OrbitalVelocity_ = glm::vec2(0.0f);
                return;
            }

            PrevOffset_ = OrbitalVelocity_;
        }

        glm::vec3 OrbitAxis         = Mode_ == EMode::kArcBall ? Up_ : OrbitAxis_;
        glm::vec3 PrevRight         = Right_;
        glm::vec3 DirectionToCamera = Position_ - OrbitTarget_;

        float HorizontalAngle = static_cast<float>(Sensitivity_ * -OrbitalVelocity_.x);
        float VerticalAngle   = static_cast<float>(Sensitivity_ *  OrbitalVelocity_.y);

        glm::quat HorizontalRotation = glm::angleAxis(glm::radians(HorizontalAngle), OrbitAxis);
        glm::quat VerticalRotation   = glm::angleAxis(glm::radians(VerticalAngle), Right_);

        DirectionToCamera = HorizontalRotation * VerticalRotation * DirectionToCamera;
        Position_         = OrbitTarget_ + DirectionToCamera;

        glm::vec3 Direction = glm::normalize(OrbitTarget_ - Position_);
        glm::vec3 Right     = glm::normalize(glm::cross(Direction, OrbitAxis));

        Right = glm::dot(Right, PrevRight) < 0.0f ? -Right : Right;

        glm::vec3 Up = glm::normalize(glm::cross(Right, Direction));
        glm::mat3x3 RotationMatrix(Right, Up, -Direction);
        glm::quat TargetOrient = glm::normalize(glm::conjugate(glm::quat_cast(RotationMatrix)));

        Orientation_ = TargetOrient;

        UpdateVectors();
    }

    void FCamera::UpdateVectors()
    {
        Orientation_ = glm::normalize(Orientation_);
        glm::quat ConjugateOrient = glm::conjugate(Orientation_);

        Front_ = glm::normalize(ConjugateOrient * glm::vec3(0.0f, 0.0f, -1.0f));
        Right_ = glm::normalize(ConjugateOrient * glm::vec3(1.0f, 0.0f,  0.0f));
        Up_    = glm::normalize(ConjugateOrient * glm::vec3(0.0f, 1.0f,  0.0f));
    }
} // namespace Npgs
