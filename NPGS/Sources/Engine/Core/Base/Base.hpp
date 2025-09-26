#pragma once

// NPGS_API NPGS_INLINE
// --------------------
#ifdef _RELEASE
#define RELEASE_FORCE_INLINE
#endif // _RELEASE

// #define MSVC_ATTRIBUTE_FORCE_INLINE

#ifdef _WIN64
#   ifdef _MSVC_LANG
#       ifdef RELEASE_FORCE_INLINE
#           define NPGS_INLINE __forceinline
#       else
#           ifdef MSVC_ATTRIBUTE_FORCE_INLINE
#               define NPGS_INLINE [[msvc::forceinline]] inline
#           else
#               define NPGS_INLINE inline
#           endif // MSVC_ATTRIBUTE_FORCE_INLINE
#       endif // RELEASE_FORCE_INLINE
#   else
#       error NPGS can only build on Visual Studio with MSVC
#   endif // _MSVC_LANG
#else
#   error NPGS only support 64-bit Windows
#endif // _WIN64

// Bit operator function
// ---------------------
#define Bit(x) (1ULL << x)
