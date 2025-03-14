#include "Camera.h"

#include <cmath>
#include <algorithm>
#include "Engine/Core/Base/Assert.h"
#include "Engine/Utils/Utils.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_SPATIAL_BEGIN

FCamera::FCamera(const glm::vec3& Position, float Sensitivity, float Speed, float Zoom, float InertiaDecay, float SmoothCoefficient)
    :
    _Position(Position),
    _Sensitivity(Sensitivity),
    _Speed(Speed),
    _Zoom(Zoom),
    _InertiaDecay(InertiaDecay),
    _SmoothCoefficient(SmoothCoefficient)
{
    UpdateVectors();
}

void FCamera::ProcessKeyboard(EMovement Direction, double DeltaTime)
{
    float Velocity = static_cast<float>(_Speed * DeltaTime);

    switch (Direction)
    {
    case EMovement::kForward:
        _Position += _Front * Velocity;
        break;
    case EMovement::kBack:
        _Position -= _Front * Velocity;
        break;
    case EMovement::kLeft:
        _Position -= _Right * Velocity;
        break;
    case EMovement::kRight:
        _Position += _Right * Velocity;
        break;
    case EMovement::kUp:
        _Position += _Up * Velocity;
        break;
    case EMovement::kDown:
        _Position -= _Up * Velocity;
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
    if (!_bCameraAligned)
    {
        ProcessAlign(DeltaTime);
    }

    if (Util::Equal(_TargetOffset.x, 0.0f) && Util::Equal(_TargetOffset.y, 0.0f))
    {
        return;
    }

    if (_Mode == EMode::kFree)
    {
        ProcessOrient();
        _TargetOffset = glm::vec2();
    }
    else if (_Mode == EMode::kArcBall)
    {
        ProcessOrbital(DeltaTime);
    }
    else
    {
        ProcessOrbital(DeltaTime);
    }
}

void FCamera::SetVector(EVector Vector, const glm::vec3& NewVector)
{
    switch (Vector)
    {
    case EVector::kPosition:
        _Position = NewVector;
        break;
    case EVector::kFront:
        _Front = NewVector;
        break;
    case EVector::kUp:
        _Up = NewVector;
        break;
    case EVector::kRight:
        _Right = NewVector;
        break;
    default:
        NpgsAssert(false, "Invalid vector type");
    }
}

#pragma warning(push)
#pragma warning(disable: 4715)
const glm::vec3& FCamera::GetVector(EVector Vector) const
{
    switch (Vector)
    {
    case EVector::kPosition:
        return _Position;
    case EVector::kFront:
        return _Front;
    case EVector::kUp:
        return _Up;
    case EVector::kRight:
        return _Right;
    default:
        NpgsAssert(false, "Invalid vector type");
    }
}
#pragma warning(pop)

void FCamera::ProcessAlign(double DeltaTime)
{
    glm::vec3 DesiredDirection = glm::normalize(_OrbitTarget - _Position);

    float FrontDotTarget = glm::dot(_Front, DesiredDirection);
    bool bNeedAlignFront = (FrontDotTarget < 0.9999f);

    bool bNeedAlignUp = false;
    if (_Mode == EMode::kAxisOrbital)
    {
        float UpDotAxis = glm::dot(_Up, _OrbitAxis);
        bNeedAlignUp = (UpDotAxis < 0.9999f);
    }

    if (!bNeedAlignFront && !bNeedAlignUp)
    {
        _bCameraAligned = true;
        return;
    }

    glm::quat TargetOrient;

    if (_Mode != EMode::kAxisOrbital)
    {
        glm::vec3 Direction = DesiredDirection;
        glm::vec3 Right     = glm::normalize(glm::cross(Direction, _Up));

        glm::mat3x3 RotationMatrix(Right, _Up, -Direction);
        TargetOrient = glm::conjugate(glm::quat_cast(RotationMatrix));
    }
    else
    {
        glm::vec3 Direction = DesiredDirection;
        glm::vec3 Right     = glm::normalize(glm::cross(Direction, _OrbitAxis));

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

    _Orientation = glm::normalize(glm::slerp(_Orientation, TargetOrient, RotationAmount));
    UpdateVectors();
}

void FCamera::ProcessOrient()
{
    float HorizontalAngle = static_cast<float>(_Sensitivity *  _TargetOffset.x);
    float VerticalAngle   = static_cast<float>(_Sensitivity * -_TargetOffset.y);
    ProcessRotation(HorizontalAngle, VerticalAngle, 0.0f);
}

void FCamera::ProcessRotation(float Yaw, float Pitch, float Roll)
{
    glm::quat QuatYaw   = glm::angleAxis(glm::radians(Yaw),   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat QuatPitch = glm::angleAxis(glm::radians(Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat QuatRoll  = glm::angleAxis(glm::radians(Roll),  glm::vec3(0.0f, 0.0f, 1.0f));

    _Orientation = glm::normalize(QuatYaw * QuatPitch * QuatRoll * _Orientation);

    UpdateVectors();
}

void FCamera::ProcessOrbital(double DeltaTime)
{
    if (_Mode == EMode::kFree)
    {
        return;
    }

    if (std::abs(_TargetOffset.x) > _VelocityThreshold * 1000.0f || std::abs(_TargetOffset.y) > _VelocityThreshold * 1000.0f)
    {
        float SmoothedX = _Sensitivity * 10.0f * _SmoothCoefficient * _TargetOffset.x + (1.0f - _SmoothCoefficient) * _PrevOffset.x;
        float SmoothedY = _Sensitivity * 10.0f * _SmoothCoefficient * _TargetOffset.y + (1.0f - _SmoothCoefficient) * _PrevOffset.y;

        _OrbitalVelocity = glm::vec2(SmoothedX, SmoothedY);
        _PrevOffset      = glm::vec2(SmoothedX, SmoothedY);

        _TargetOffset *= (1.0f - _SmoothCoefficient);
    }
    else
    {
        float DecaySpeed        = 50.0f;
        float TimeAdjustedDecay = std::pow(_InertiaDecay, static_cast<float>(DeltaTime) * DecaySpeed);
        _OrbitalVelocity *= TimeAdjustedDecay;

        if (glm::length(_OrbitalVelocity) < _VelocityThreshold)
        {
            _OrbitalVelocity = glm::vec2(0.0f);
            return;
        }

        _PrevOffset = _OrbitalVelocity;
    }

    glm::vec3 OrbitAxis         = _Mode == EMode::kArcBall ? _Up : _OrbitAxis;
    glm::vec3 PrevRight         = _Right;
    glm::vec3 DirectionToCamera = _Position - _OrbitTarget;

    float HorizontalAngle = static_cast<float>(_Sensitivity * -_OrbitalVelocity.x);
    float VerticalAngle   = static_cast<float>(_Sensitivity *  _OrbitalVelocity.y);

    glm::quat HorizontalRotation = glm::angleAxis(glm::radians(HorizontalAngle), OrbitAxis);
    glm::quat VerticalRotation   = glm::angleAxis(glm::radians(VerticalAngle), _Right);

    DirectionToCamera = HorizontalRotation * VerticalRotation * DirectionToCamera;
    _Position         = _OrbitTarget + DirectionToCamera;

    glm::vec3 Direction = glm::normalize(_OrbitTarget - _Position);
    glm::vec3 Right     = glm::normalize(glm::cross(Direction, OrbitAxis));

    Right = glm::dot(Right, PrevRight) < 0.0f ? -Right : Right;

    glm::vec3 Up = glm::normalize(glm::cross(Right, Direction));
    glm::mat3x3 RotationMatrix(Right, Up, -Direction);
    glm::quat TargetOrient = glm::normalize(glm::conjugate(glm::quat_cast(RotationMatrix)));
    glm::quat OrientOffset = glm::normalize(glm::inverse(TargetOrient) * _Orientation);

    _Orientation = glm::normalize(TargetOrient * glm::conjugate(OrientOffset));

    UpdateVectors();
}

void FCamera::UpdateVectors()
{
    _Orientation = glm::normalize(_Orientation);
    glm::quat ConjugateOrient = glm::conjugate(_Orientation);

    _Front = glm::normalize(ConjugateOrient * glm::vec3(0.0f, 0.0f, -1.0f));
    _Right = glm::normalize(ConjugateOrient * glm::vec3(1.0f, 0.0f,  0.0f));
    _Up    = glm::normalize(ConjugateOrient * glm::vec3(0.0f, 1.0f,  0.0f));
}

_SPATIAL_END
_SYSTEM_END
_NPGS_END
