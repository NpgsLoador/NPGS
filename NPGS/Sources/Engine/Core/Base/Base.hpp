#pragma once

// NPGS_API NPGS_INLINE
// --------------------
#ifdef _RELEASE
#define RELEASE_FORCE_INLINE
#endif // _RELEASE

// #define MSVC_ATTRIBUTE_FORCE_INLINE

#ifdef _WIN64
#   ifdef _MSC_VER
#       ifdef RELEASE_FORCE_INLINE
#           ifdef MSVC_ATTRIBUTE_FORCE_INLINE
#               define NPGS_INLINE [[msvc::forceinline]] inline
#           else
#               define NPGS_INLINE __forceinline
#           endif // MSVC_ATTRIBUTE_FORCE_INLINE
#       else
#               define NPGS_INLINE inline
#       endif // RELEASE_FORCE_INLINE
#   else
#       error NPGS can only build on Visual Studio with MSVC.
#       error If you want to build on LLVM (clang-cl), please add all libs manually 
#       error then remove this #error because lld cannot use vcpkg Auto-Link.
#       error Besides, you need to add all #include "Header.hpp" in .inl file to avoid IntelliSense doom.
#       error IntelliSense doom only happen when use clang-cl without header including in .inl file.
#       error No compiling error.
#   endif // _MSC_VER
#else
#   error NPGS only support 64-bit Windows
#endif // _WIN64

// Bit operator function
// ---------------------
#define Bit(x) (1ULL << x)
