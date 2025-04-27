#include "Engine/Core/Base/Base.h"

namespace Npgs::Util
{
    NPGS_INLINE std::shared_ptr<spdlog::logger>& FLogger::GetCoreLogger()
    {
        return kCoreLogger_;
    }

    NPGS_INLINE std::shared_ptr<spdlog::logger>& FLogger::GetClientLogger()
    {
        return kClientLogger_;
    }
} // namespace Npgs::Util