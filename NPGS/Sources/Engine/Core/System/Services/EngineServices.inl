#include "Engine/Core/Base/Base.h"

namespace Npgs
{
    NPGS_INLINE FCoreServices* Npgs::FEngineServices::GetCoreServices() const
    {
        return CoreServices_.get();
    }

    NPGS_INLINE FResourceServices* Npgs::FEngineServices::GetResourceServices() const
    {
        return ResourceServices_.get();
    }
} // namespace Npgs
