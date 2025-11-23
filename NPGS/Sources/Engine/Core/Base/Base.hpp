#pragma once

// NPGS_API NPGS_INLINE
// --------------------
#ifdef _RELEASE
#define RELEASE_FORCE_INLINE
#endif // _RELEASE

// #define MSVC_ATTRIBUTE_FORCE_INLINE

#ifdef _WIN64
#   if defined(TRY_LLVM_CLANG_CL) || defined(_MSVC_LANG) && defined(_MSC_VER)
#       ifdef RELEASE_FORCE_INLINE
#           ifdef MSVC_ATTRIBUTE_FORCE_INLINE
#               define NPGS_INLINE [[msvc::forceinline]] inline
#           else
#               define NPGS_INLINE __forceinline
#           endif // MSVC_ATTRIBUTE_FORCE_INLINE
#       else
#               define NPGS_INLINE inline
#       endif // RELEASE_FORCE_INLINE
#   elif defined(__clang__) && !defined(TRY_LLVM_CLANG_CL)
#       error NPGS project officially supports MSVC toolchain only.
        // Building with LLVM (clang-cl) is not recommended and unsupported for the following reasons:
        // 0. This project does not exhibit significant CPU performance bottlenecks.
        //    The core logic and performance critical points of this project lie in shader performance,
        //    memory access patterns, algorithmic structures, rather than purely CPU-intensive computations.
        //    In our testing and analysis, the final programs compiled with MSVC and LLVM showed no
        //    perceptible performance differences.Therefore, switching toolchains for negligible or
        //    non-existent performance gains is not worth the effort.
        // 
        // 1. Incompatibility with vcpkg AutoLink: The LLVM linker (lld) does not support vcpkg's AutoLinking mechanism.
        //    If you proceed, you must manually link ALL library dependencies.
        // 
        // 2. IntelliSense Issues : To avoid severe IntelliSense degradation with clang-cl,
        //    you must explicitly #include the corresponding.hpp file within each.inl file for template implementations.
        // 
        // If you understand these challenges and wish to proceed at your own risk,
        // you must first resolve the above issues and then define TRY_LLVM_CLANG_CL.
        // Or you may can try reinstall all vcpkg packages to fix ABI compatibility issues in lld (I don't know what cause it).
        // If you successfully build using LLVM, please consider contributing a Pull Request or Discussion on GitHub. :)
#   endif // _MSVC_LANG
#else
#   error NPGS only support 64-bit Windows
#endif // _WIN64

// Bit operator function
// ---------------------
#define Bit(x) (1ULL << x)

#ifndef _RELEASE
#define NPGS_ENABLE_CONSOLE_LOGGER
#else
#define GLM_FORCE_INLINE
#define NPGS_ENABLE_FILE_LOGGER
#endif // _RELEASE
