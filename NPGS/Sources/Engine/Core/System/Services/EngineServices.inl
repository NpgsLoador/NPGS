#include "EngineServices.h"

namespace Npgs
{
    NPGS_INLINE FCoreServices* Npgs::FEngineServices::GetCoreServices() const
    {
        return _CoreServices.get();
    }

    NPGS_INLINE FResourceServices* Npgs::FEngineServices::GetResourceServices() const
    {
        return _ResourceServices.get();
    }
} // namespace Npgs
