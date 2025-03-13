#include "Camera.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

_NPGS_BEGIN
_SYSTEM_BEGIN
_SPATIAL_BEGIN

NPGS_INLINE void FCamera::ProcessMouseScroll(double OffsetY)
{
    float SpeedFactor = 0.1f * _Speed;
    _Speed += static_cast<float>(OffsetY * SpeedFactor);
    _Speed = std::max(0.0f, _Speed);
}

NPGS_INLINE void FCamera::SetOrientation(const glm::quat& Orientation)
{
    _Orientation = Orientation;
}

NPGS_INLINE const glm::quat& FCamera::GetOrientation() const
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
    return Matrix;
}

NPGS_INLINE void FCamera::SetOrbitAxis(const glm::vec3& Axis)
{
    _OrbitAxis = Axis;
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

_SPATIAL_END
_SYSTEM_END
_NPGS_END
