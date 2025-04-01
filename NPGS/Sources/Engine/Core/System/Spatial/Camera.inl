#include "Camera.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE void FCamera::AlignCamera()
    {
        _bCameraAligned = false;
    }

    NPGS_INLINE void FCamera::ProcessMouseScroll(double OffsetY)
    {
        float SpeedFactor = 0.1f * _Speed;
        _Speed += static_cast<float>(OffsetY * SpeedFactor);
        _Speed = std::max(0.0f, _Speed);
    }

    NPGS_INLINE void FCamera::SetOrientation(glm::quat Orientation)
    {
        _Orientation = Orientation;
    }

    NPGS_INLINE glm::quat FCamera::GetOrientation() const
    {
        return _Orientation;
    }

    NPGS_INLINE glm::mat4x4 FCamera::GetViewMatrix() const
    {
        return glm::lookAt(_Position, _Position + _Front, _Up);
    }

    NPGS_INLINE glm::mat4x4 FCamera::GetProjectionMatrix(float WindowAspect, float Near) const
    {
        glm::mat4x4 Matrix = glm::infinitePerspective(glm::radians(_Zoom), WindowAspect, Near);
        Matrix[1][1] *= -1;
        return Matrix;
    }

    NPGS_INLINE void FCamera::SetOrbitTarget(glm::vec3 Target)
    {
        _OrbitTarget = Target;
        _OrbitRadius = glm::length(Target - _Position);

        UpdateVectors();
    }

    NPGS_INLINE void FCamera::SetOrbitAxis(glm::vec3 Axis)
    {
        _OrbitAxis = Axis;
    }

    NPGS_INLINE void FCamera::SetTargetOffset(glm::vec2 Offset)
    {
        _TargetOffset = Offset;
    }

    NPGS_INLINE void FCamera::SetZoom(float Zoom)
    {
        _Zoom = Zoom;
    }

    NPGS_INLINE void FCamera::SetMode(EMode Mode)
    {
        _Mode = Mode;
    }

    NPGS_INLINE float FCamera::GetZoom() const
    {
        return _Zoom;
    }

    NPGS_INLINE FCamera::EMode FCamera::GetMode() const
    {
        return _Mode;
    }
} // namespace Npgs
