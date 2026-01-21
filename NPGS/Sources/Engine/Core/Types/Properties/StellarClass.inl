#include "StellarClass.hpp"
#include <utility>

namespace Npgs::Astro
{
    NPGS_INLINE EStellarType FStellarClass::GetStellarType() const
    {
        return StellarType_;
    }

    NPGS_INLINE void FSpectralType::MarkSpecial(ESpecialMark Mark)
    {
        SpecialMark |= std::to_underlying(Mark);
    }

    NPGS_INLINE void FSpectralType::UnmarkSpecial(ESpecialMark Mark)
    {
        SpecialMark &= ~std::to_underlying(Mark);
    }

    NPGS_INLINE bool Astro::FSpectralType::SpecialMarked(ESpecialMark Mark) const
    {
        return (SpecialMark & std::to_underlying(Mark)) != 0;
    }
} // namespace Npgs::Astro
