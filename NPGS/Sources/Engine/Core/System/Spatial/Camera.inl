#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>
#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    NPGS_INLINE void FCamera::AlignCamera()
    {
        bCameraAligned_ = false;
    }

    NPGS_INLINE void FCamera::ProcessMouseScroll(double OffsetY)
    {
        float SpeedFactor = 0.1f * Speed_;
        Speed_ += static_cast<float>(OffsetY * SpeedFactor);
        Speed_ = std::max(0.0f, Speed_);
    }

    NPGS_INLINE void FCamera::SetOrientation(glm::quat Orientation)
    {
        Orientation_ = Orientation;
    }

    NPGS_INLINE glm::quat FCamera::GetOrientation() const
    {
        return Orientation_;
    }

    NPGS_INLINE glm::mat4x4 FCamera::GetViewMatrix() const
    {
        return glm::lookAt(Position_, Position_ + Front_, Up_);
    }

    NPGS_INLINE glm::mat4x4 FCamera::GetProjectionMatrix(float WindowAspect, float Near) const
    {
        glm::mat4x4 Matrix = glm::infinitePerspective(glm::radians(Zoom_), WindowAspect, Near);
        Matrix[1][1] *= -1;
        return Matrix;
    }

    NPGS_INLINE void FCamera::SetOrbitTarget(glm::vec3 Target)
    {
        OrbitTarget_ = Target;
        OrbitRadius_ = glm::length(Target - Position_);

        UpdateVectors();
    }

    NPGS_INLINE void FCamera::SetOrbitAxis(glm::vec3 Axis)
    {
        OrbitAxis_ = Axis;
    }

    NPGS_INLINE void FCamera::SetTargetOffset(glm::vec2 Offset)
    {
        TargetOffset_ = Offset;
    }

    NPGS_INLINE void FCamera::SetZoom(float Zoom)
    {
        Zoom_ = Zoom;
    }

    NPGS_INLINE void FCamera::SetMode(EMode Mode)
    {
        Mode_ = Mode;
    }

    NPGS_INLINE float FCamera::GetZoom() const
    {
        return Zoom_;
    }

    NPGS_INLINE FCamera::EMode FCamera::GetMode() const
    {
        return Mode_;
    }
} // namespace Npgs
