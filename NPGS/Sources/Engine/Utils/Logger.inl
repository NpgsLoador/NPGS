#include "Logger.h"

#include "Engine/Core/Base/Base.h"

namespace Npgs::Util
{
    NPGS_INLINE std::shared_ptr<spdlog::logger>& FLogger::GetCoreLogger()
    {
        return _kCoreLogger;
    }

    NPGS_INLINE std::shared_ptr<spdlog::logger>& FLogger::GetClientLogger()
    {
        return _kClientLogger;
    }
} // namespace Npgs::Util