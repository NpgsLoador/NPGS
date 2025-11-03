#include "Engine/Core/Base/Base.hpp"

namespace Npgs::Util
{
    NPGS_INLINE std::shared_ptr<spdlog::logger>& FLogger::GetCoreLogger()
    {
        return CoreLogger_;
    }

    NPGS_INLINE std::shared_ptr<spdlog::logger>& FLogger::GetClientLogger()
    {
        return ClientLogger_;
    }
} // namespace Npgs::Util