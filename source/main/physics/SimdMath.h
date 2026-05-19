#pragma once

// ============================================================================
// SIMD / SSE Feature Detection
// ============================================================================

// Detect SSE support at compile time
#if defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1) || defined(_M_X64) || defined(_M_AMD64)
    #define ROR_HAS_SSE 1
#else
    #define ROR_HAS_SSE 0
#endif

// Detect SSE2 support (virtually all x64 CPUs have this)
#if defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64) || defined(_M_AMD64)
    #define ROR_HAS_SSE2 1
#else
    #define ROR_HAS_SSE2 0
#endif

// Include appropriate headers
#if ROR_HAS_SSE
    #include <xmmintrin.h>  // SSE
#endif

#if ROR_HAS_SSE2
    #include <emmintrin.h>  // SSE2
#endif

// Platform-specific architecture detection
#if defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__) || defined(__amd64__)
    #define ROR_ARCH_X64 1
    #define ROR_ARCH_X86 0
#elif defined(_M_IX86) || defined(__i386__)
    #define ROR_ARCH_X64 0
    #define ROR_ARCH_X86 1
#else
    #define ROR_ARCH_X64 0
    #define ROR_ARCH_X86 0
#endif

// ARM detection (for future portability)
#if defined(__ARM_NEON) || defined(__aarch64__)
    #define ROR_HAS_NEON 1
#else
    #define ROR_HAS_NEON 0
#endif

// ============================================================================
// SIMD math functions
// ============================================================================

#ifdef ROR_HAS_SSE
    // Fast SIMD version for single value
    // Precision Comparison:
    //  • fast_invSqrt: ~0.175% max error (includes 1 Newton-Raphson iteration)
    //  • _mm_rsqrt_ss: ~0.15% max error (acceptable for physics simulation)
    inline float simd_invSqrt(float v)
    {
        __m128 val = _mm_set_ss(v);
        __m128 result = _mm_rsqrt_ss(val);
    
        // Optional: One Newton-Raphson iteration for higher precision (~0.0015% error)
        // result = result * (1.5f - 0.5f * v * result * result);
    
        return _mm_cvtss_f32(result);
    }
#else
    #include "ApproxMath.h"
    #define simd_invSqrt fast_invSqrt
#endif
