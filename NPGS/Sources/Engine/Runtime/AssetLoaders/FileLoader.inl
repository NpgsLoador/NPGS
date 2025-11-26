#include <cstdint>
#include <stdexcept>
#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
    template <typename Ty>
    std::vector<Ty> Npgs::FFileLoader::StripData()
    {
        auto DataSpan = GetDataAs<Ty>();
        std::vector<Ty> Data(DataSpan.begin(), DataSpan.end());
        Unload();
        return Data;
    }

    template <typename Ty>
    requires std::is_standard_layout_v<Ty>&& std::is_trivial_v<Ty>
    inline std::span<const Ty> FFileLoader::GetDataAs() const
    {
        if (reinterpret_cast<std::uintptr_t>(Data_) % alignof(Ty) != 0)
        {
            throw std::runtime_error("Data is not properly aligned for this type.");
        }

        if (Size_ % sizeof(Ty) != 0)
        {
            throw std::runtime_error("File size is not a multiple of the requested type size.");
        }

        return std::span<const Ty>(reinterpret_cast<const Ty*>(Data_), Size_ / sizeof(Ty));
    }

    NPGS_INLINE bool Npgs::FFileLoader::Empty() const
    {
        return Data_ == nullptr || Size_ == 0;
    }

    NPGS_INLINE std::size_t Npgs::FFileLoader::Size() const
    {
        return Size_;
    }
} // namespace Npgs
