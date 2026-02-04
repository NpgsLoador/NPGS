#include "stdafx.h"
#include "StellarClass.hpp"

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>

#include "Engine/Core/Base/Assert.hpp"

namespace Npgs::Astro
{
    // FStellarClass implementations
    // -----------------------------
    FStellarClass::FStellarClass(EStellarType StellarType, FSpectralType SpectralType)
        : SpectralType_(SpectralType)
        , StellarType_(StellarType)
    {
    }

    std::string FStellarClass::ToString() const
    {
        if (SpectralType_.SpectralClass == ESpectralClass::kSpectral_Unknown)
        {
            return "Unknown";
        }

        std::string SpectralTypeStr = SpectralClassToString(SpectralType_.SpectralClass, SpectralType_.Subclass);

        SpectralTypeStr += LuminosityClassToString(SpectralType_.LuminosityClass);
        SpectralTypeStr += SpecialMarkToString(static_cast<ESpecialMark>(SpectralType_.SpecialMark));

        return SpectralTypeStr;
    }

    FStellarClass FStellarClass::Parse(const std::string& StellarClassStr)
    {
        NpgsAssert(!StellarClassStr.empty(), "StellarClassStr is empty.");

        EStellarType StellarType = EStellarType::kNormalStar;

        ESpectralClass      SpectralClass   = ESpectralClass::kSpectral_Unknown;
        ELuminosityClass    LuminosityClass = ELuminosityClass::kLuminosity_Unknown;
        FSpecialMarkDigital SpecialMark     = std::to_underlying(ESpecialMark::kCode_Null);
        float               Subclass        = 0.0f;

        EParseState State = EParseState::kBegin;
        std::size_t Index = 0;

        while (State != EParseState::kEnd)
        {
            char Char     = 0;
            char NextChar = Index + 1 >= StellarClassStr.size() ? '\0' : StellarClassStr[Index + 1];
            if (Index == StellarClassStr.size())
            {
                Char = '\0';
            }
            else
            {
                Char = StellarClassStr[Index];
            }

            switch (State)
            {
            case EParseState::kBegin:
                State = ParseStellarType(Char, StellarType, SpectralClass, Index);
                break;
            case EParseState::kSpectralClass:
                State = ParseSpectralClass(Char, SpectralClass, Index);
                break;
            case EParseState::kWolfRayetStar:
                State = ParseWolfRayetStar(Char, SpectralClass, Index);
                break;
            case EParseState::kWhiteDwarf:
                State = ParseWhiteDwarf(Char, SpectralClass, Index);
                break;
            case EParseState::kWhiteDwarfEx:
                State = ParseWhiteDwarfEx(Char, StellarClassStr[Index - 1], SpectralClass, Index);
                break;
            case EParseState::kSubdwarfPerfix:
                switch (Char)
                {
                case 'd':
                    LuminosityClass = ELuminosityClass::kLuminosity_VI;
                    State = EParseState::kSpectralClass;
                    ++Index;
                    break;
                default:
                    State = EParseState::kEnd;
                    break;
                }

                break;
            case EParseState::kSubclass:
                if (std::isdigit(Char))
                {
                    Subclass = static_cast<float>(Char - '0');
                    State    = EParseState::kSubclassDecimal;
                    ++Index;
                }
                else
                {
                    State = EParseState::kSpecialMark;
                }

                break;
            case EParseState::kSubclassDecimal:
                if (Char == '.')
                {
                    State = EParseState::kSubclassDecimalFinal;
                    ++Index;
                }
                else
                {
                    State = EParseState::kSpecialMark;
                }

                break;
            case EParseState::kSubclassDecimalFinal:
                if (std::isdigit(Char))
                {
                    Subclass += 0.1f * (Char - '0');
                }

                State = EParseState::kSpecialMark;
                ++Index;
                break;
            case EParseState::kSpecialMark:
                State = ParseSpecialMark(Char, NextChar, SpecialMark, Index);
                break;
            case EParseState::kLuminosityClass:
                State = ParseLuminosityClass(Char, LuminosityClass, Index);
                break;
            case EParseState::kLuminosityClassI:
                State = ParseLuminosityClassI(Char, LuminosityClass, Index);
                break;
            case EParseState::kLuminosityClassIa:
                State = ParseLuminosityClassIa(Char, LuminosityClass);
                break;
            case EParseState::kLuminosityClassII:
                State = ParseLuminosityClassII(Char, LuminosityClass);
                break;
            case EParseState::kLuminosityClassV:
                State = ParseLuminosityClassV(Char, LuminosityClass);
                break;
            }
        }

        FSpectralType SpectralType
        {
            .SpectralClass   = SpectralClass,
            .LuminosityClass = LuminosityClass,
            .SpecialMark     = SpecialMark,
            .Subclass        = Subclass
        };

        return FStellarClass(StellarType, SpectralType);
    }

    // Processor functions implementations
    // -----------------------------------
    FStellarClass::EParseState FStellarClass::ParseStellarType(char Char, EStellarType& StellarType,
                                                               ESpectralClass& HSpectralClass, std::size_t& Index)
    {
        switch (Char)
        {
        case 'X':
            StellarType = EStellarType::kBlackHole;
            return EParseState::kEnd;
        case 'Q':
            StellarType = EStellarType::kNeutronStar;
            return EParseState::kEnd;
        case 'D':
            StellarType    = EStellarType::kWhiteDwarf;
            HSpectralClass = ESpectralClass::kSpectral_D;
            ++Index;
            return EParseState::kWhiteDwarf;
        case 's': // sd 前缀
            StellarType = EStellarType::kNormalStar;
            ++Index;
            return EParseState::kSubdwarfPerfix;
        case '?':
            return EParseState::kEnd;
        default:
            StellarType = EStellarType::kNormalStar;
            return EParseState::kSpectralClass;
        }
    }

    FStellarClass::EParseState FStellarClass::ParseSpectralClass(char Char, ESpectralClass& SpectralClass, std::size_t& Index)
    {
        switch (Char)
        {
        case 'W':
            ++Index;
            return EParseState::kWolfRayetStar;
        case 'O':
            SpectralClass = ESpectralClass::kSpectral_O;
            ++Index;
            return EParseState::kSubclass;
        case 'B':
            SpectralClass = ESpectralClass::kSpectral_B;
            ++Index;
            return EParseState::kSubclass;
        case 'A':
            SpectralClass = ESpectralClass::kSpectral_A;
            ++Index;
            return EParseState::kSubclass;
        case 'F':
            SpectralClass = ESpectralClass::kSpectral_F;
            ++Index;
            return EParseState::kSubclass;
        case 'G':
            SpectralClass = ESpectralClass::kSpectral_G;
            ++Index;
            return EParseState::kSubclass;
        case 'K':
            SpectralClass = ESpectralClass::kSpectral_K;
            ++Index;
            return EParseState::kSubclass;
        case 'M':
            SpectralClass = ESpectralClass::kSpectral_M;
            ++Index;
            return EParseState::kSubclass;
        case 'R':
            SpectralClass = ESpectralClass::kSpectral_R;
            ++Index;
            return EParseState::kSubclass;
        case 'N':
            SpectralClass = ESpectralClass::kSpectral_N;
            ++Index;
            return EParseState::kSubclass;
        case 'C':
            SpectralClass = ESpectralClass::kSpectral_C;
            ++Index;
            return EParseState::kSubclass;
        case 'S':
            SpectralClass = ESpectralClass::kSpectral_S;
            ++Index;
            return EParseState::kSubclass;
        case 'L':
            SpectralClass = ESpectralClass::kSpectral_L;
            ++Index;
            return EParseState::kSubclass;
        case 'T':
            SpectralClass = ESpectralClass::kSpectral_T;
            ++Index;
            return EParseState::kSubclass;
        case 'Y':
            SpectralClass = ESpectralClass::kSpectral_Y;
            ++Index;
            return EParseState::kSubclass;
        default:
            return EParseState::kEnd;
        }
    }

    FStellarClass::EParseState FStellarClass::ParseWolfRayetStar(char Char, ESpectralClass& SpectralClass, std::size_t& Index)
    {
        switch (Char)
        {
        case 'C':
            SpectralClass = ESpectralClass::kSpectral_WC;
            ++Index;
            return EParseState::kSubclass;
        case 'N':
            SpectralClass = ESpectralClass::kSpectral_WN;
            ++Index;
            return EParseState::kSubclass;
        case 'O':
            SpectralClass = ESpectralClass::kSpectral_WO;
            ++Index;
            return EParseState::kSubclass;
        default:
            return EParseState::kEnd;
        }
    }

    FStellarClass::EParseState FStellarClass::ParseWhiteDwarf(char Char, ESpectralClass& SpectralClass, std::size_t& Index)
    {
        ++Index;

        switch (Char)
        {
        case 'A':
            SpectralClass = ESpectralClass::kSpectral_DA;
            return EParseState::kWhiteDwarfEx;
        case 'B':
            SpectralClass = ESpectralClass::kSpectral_DB;
            return EParseState::kWhiteDwarfEx;
        case 'C':
            SpectralClass = ESpectralClass::kSpectral_DC;
            return EParseState::kWhiteDwarfEx;
        case 'O':
            SpectralClass = ESpectralClass::kSpectral_DO;
            return EParseState::kWhiteDwarfEx;
        case 'Q':
            SpectralClass = ESpectralClass::kSpectral_DQ;
            return EParseState::kWhiteDwarfEx;
        case 'X':
            SpectralClass = ESpectralClass::kSpectral_DX;
            return EParseState::kWhiteDwarfEx;
        case 'Z':
            SpectralClass = ESpectralClass::kSpectral_DZ;
            return EParseState::kWhiteDwarfEx;
        default:
            SpectralClass = ESpectralClass::kSpectral_D;
            return EParseState::kSubclass;
        }
    }

    FStellarClass::EParseState FStellarClass::ParseWhiteDwarfEx(char Char, char PrevChar, ESpectralClass& SpectralClass, std::size_t& Index)
    {
        if (Char == PrevChar)
        {
            NpgsAssert(false, "Invalid white dwarf extended type.");
        }

        switch (Char)
        {
        case 'A':
            ++Index;
            break;
        case 'B':
            ++Index;
            break;
        case 'C':
            ++Index;
            break;
        case 'O':
            ++Index;
            break;
        case 'Q':
            ++Index;
            break;
        case 'X':
            ++Index;
            break;
        case 'Z':
            ++Index;
            break;
        default:
            break;
        }

        return EParseState::kSubclass;
    }

    FStellarClass::EParseState FStellarClass::ParseLuminosityClass(char Char, ELuminosityClass& LuminosityClass, std::size_t& Index)
    {
        switch (Char)
        {
        case '0':
            if (LuminosityClass == ELuminosityClass::kLuminosity_Unknown)
            {
                LuminosityClass  = ELuminosityClass::kLuminosity_0;
                return EParseState::kSpecialMark;
            }
            else
            {
                return EParseState::kEnd;
            }
        case 'I':
            ++Index;
            return EParseState::kLuminosityClassI;
        case 'V':
            ++Index;
            return EParseState::kLuminosityClassV;
        case ' ':
            ++Index;
            return EParseState::kLuminosityClass;
        default:
            return EParseState::kSpecialMark;
        }
    }

    FStellarClass::EParseState FStellarClass::ParseLuminosityClassI(char Char, ELuminosityClass& LuminosityClass, std::size_t& Index)
    {
        switch (Char)
        {
        case 'a':
            ++Index;
            return EParseState::kLuminosityClassIa;
        case 'b':
            LuminosityClass = ELuminosityClass::kLuminosity_Ib;
            ++Index;
            return EParseState::kSpecialMark;
        case 'I':
            ++Index;
            return EParseState::kLuminosityClassII;
        case 'V':
            LuminosityClass = ELuminosityClass::kLuminosity_IV;
            ++Index;
            return EParseState::kSpecialMark;
        default:
            LuminosityClass = ELuminosityClass::kLuminosity_I;
            return EParseState::kSpecialMark;
        }
    }

    FStellarClass::EParseState FStellarClass::ParseLuminosityClassIa(char Char, ELuminosityClass& LuminosityClass)
    {
        switch (Char)
        {
        case '+':
            LuminosityClass = ELuminosityClass::kLuminosity_IaPlus;
            return EParseState::kSpecialMark;
        case 'b':
            LuminosityClass = ELuminosityClass::kLuminosity_Iab;
            return EParseState::kSpecialMark;
        default:
            LuminosityClass = ELuminosityClass::kLuminosity_Ia;
            return EParseState::kSpecialMark;
        }
    }

    FStellarClass::EParseState FStellarClass::ParseLuminosityClassII(char Char, ELuminosityClass& LuminosityClass)
    {
        switch (Char)
        {
        case 'I':
            LuminosityClass = ELuminosityClass::kLuminosity_III;
            return EParseState::kSpecialMark;
        default:
            LuminosityClass = ELuminosityClass::kLuminosity_II;
            return EParseState::kSpecialMark;
        }
    }

    FStellarClass::EParseState FStellarClass::ParseLuminosityClassV(char Char, ELuminosityClass& LuminosityClass)
    {
        switch (Char)
        {
        case 'I':
            LuminosityClass = ELuminosityClass::kLuminosity_VI;
            return EParseState::kSpecialMark;
        default:
            LuminosityClass = ELuminosityClass::kLuminosity_V;
            return EParseState::kSpecialMark;
        }
    }

    FStellarClass::EParseState FStellarClass::ParseSpecialMark(char Char, char NextChar, FSpecialMarkDigital& SpecialMark, std::size_t& Index)
    {
        switch (Char)
        {
        case 'f':
            SpecialMark |= static_cast<std::uint32_t>(ESpecialMark::kCode_f);
            break;
        case 'h':
            SpecialMark |= static_cast<std::uint32_t>(ESpecialMark::kCode_h);
            break;
        case 'm':
            SpecialMark |= static_cast<std::uint32_t>(ESpecialMark::kCode_m);
            break;
        case 'p':
            SpecialMark |= static_cast<std::uint32_t>(ESpecialMark::kCode_p);
            break;
        case 'e':
            SpecialMark |= static_cast<std::uint32_t>(ESpecialMark::kCode_e);
            break;
        case 'z':
            SpecialMark |= static_cast<std::uint32_t>(ESpecialMark::kCode_z);
            break;
        case '+':
            ++Index;
            return EParseState::kSpecialMark;
        case ' ':
            ++Index;
            return EParseState::kSpecialMark;
        case '\0':
            return EParseState::kEnd;
        default:
            return EParseState::kLuminosityClass;
        }

        ++Index;
        return (std::isalpha(NextChar) && std::islower(NextChar)) ? EParseState::kSpecialMark : EParseState::kEnd;
    }

    std::string FStellarClass::SpectralClassToString(ESpectralClass SpectralClass, float Subclass)
    {
        std::ostringstream Stream;

        if (Subclass == std::floor(Subclass))
        {
            Stream << std::fixed << std::setprecision(0) << Subclass;
        }
        else
        {
            Subclass = std::round(Subclass * 10.0f) / 10.0f;
            Stream << std::fixed << std::setprecision(1) << Subclass;
        }

        switch (SpectralClass)
        {
        case ESpectralClass::kSpectral_O:
            return "O" + Stream.str();
        case ESpectralClass::kSpectral_B:
            return "B" + Stream.str();
        case ESpectralClass::kSpectral_A:
            return "A" + Stream.str();
        case ESpectralClass::kSpectral_F:
            return "F" + Stream.str();
        case ESpectralClass::kSpectral_G:
            return "G" + Stream.str();
        case ESpectralClass::kSpectral_K:
            return "K" + Stream.str();
        case ESpectralClass::kSpectral_M:
            return "M" + Stream.str();
        case ESpectralClass::kSpectral_R:
            return "R" + Stream.str();
        case ESpectralClass::kSpectral_N:
            return "N" + Stream.str();
        case ESpectralClass::kSpectral_C:
            return "C" + Stream.str();
        case ESpectralClass::kSpectral_S:
            return "S" + Stream.str();
        case ESpectralClass::kSpectral_WO:
            return "WO" + Stream.str();
        case ESpectralClass::kSpectral_WN:
            return "WN" + Stream.str();
        case ESpectralClass::kSpectral_WC:
            return "WC" + Stream.str();
        case ESpectralClass::kSpectral_L:
            return "L" + Stream.str();
        case ESpectralClass::kSpectral_T:
            return "T" + Stream.str();
        case ESpectralClass::kSpectral_Y:
            return "Y" + Stream.str();
        case ESpectralClass::kSpectral_D:
            return "D" + Stream.str();
        case ESpectralClass::kSpectral_DA:
            return "DA" + Stream.str();
        case ESpectralClass::kSpectral_DB:
            return "DB" + Stream.str();
        case ESpectralClass::kSpectral_DC:
            return "DC" + Stream.str();
        case ESpectralClass::kSpectral_DO:
            return "DO" + Stream.str();
        case ESpectralClass::kSpectral_DQ:
            return "DQ" + Stream.str();
        case ESpectralClass::kSpectral_DX:
            return "DX" + Stream.str();
        case ESpectralClass::kSpectral_DZ:
            return "DZ" + Stream.str();
        case ESpectralClass::kSpectral_Q:
            return "Q";
        case ESpectralClass::kSpectral_X:
            return "X";
        default:
            return "Unknown";
        }
    }

    std::string FStellarClass::LuminosityClassToString(ELuminosityClass LuminosityClass)
    {
        switch (LuminosityClass)
        {
        case ELuminosityClass::kLuminosity_0:
            return "0";
        case ELuminosityClass::kLuminosity_IaPlus:
            return "Ia+";
        case ELuminosityClass::kLuminosity_Ia:
            return "Ia";
        case ELuminosityClass::kLuminosity_Ib:
            return "Ib";
        case ELuminosityClass::kLuminosity_Iab:
            return "Iab";
        case ELuminosityClass::kLuminosity_I:
            return "I";
        case ELuminosityClass::kLuminosity_II:
            return "II";
        case ELuminosityClass::kLuminosity_III:
            return "III";
        case ELuminosityClass::kLuminosity_IV:
            return "IV";
        case ELuminosityClass::kLuminosity_V:
            return "V";
        case ELuminosityClass::kLuminosity_VI:
            return "VI";
        default:
            return "";
        }
    }

    std::string FStellarClass::SpecialMarkToString(ESpecialMark SpecialMark)
    {
        std::string Result;

        if (std::to_underlying(SpecialMark) == 0)
        {
            return Result;
        }

        auto HasMark = [SpecialMark](ESpecialMark MarkCode) -> bool
        {
            return std::to_underlying(SpecialMark) & std::to_underlying(MarkCode);
        };

        if (HasMark(ESpecialMark::kCode_f))
            Result += "f";
        if (HasMark(ESpecialMark::kCode_h))
            Result += "h";
        if (HasMark(ESpecialMark::kCode_m))
            Result += "m";
        if (HasMark(ESpecialMark::kCode_p))
            Result += "p";
        if (HasMark(ESpecialMark::kCode_e))
            Result += "e";
        if (HasMark(ESpecialMark::kCode_z))
            Result += "z";

        return Result;
    }
} // namespace Npgs::Astro
