/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* The vector tiers a software filter can build, shared by the filters
 * that have vector code:
 *
 * SOFTFILTER_HAVE_SSE2  where the whole build has SSE2 (all x86-64, and
 *                       32-bit x86 built with it); <emmintrin.h> is in.
 * SOFTFILTER_HAVE_AVX2  where AVX2 functions can be built next to it, to
 *                       be picked at create from the SIMD mask
 *                       (SOFTFILTER_SIMD_AVX2): with the target attribute
 *                       on GCC 5+ and Clang 4+, as is on MSVC 2017 15.3+,
 *                       and only where the baseline has AVX2 under
 *                       PlayStation clang and clang-cl, which declare
 *                       only the intrinsics of ISAs the whole build
 *                       enables. Each AVX2 function is marked
 *                       SOFTFILTER_TARGET_AVX2 and ends with
 *                       SOFTFILTER_ZEROUPPER(); <immintrin.h> is in.
 * SOFTFILTER_HAVE_NEON  on ARMv8 and ARM builds with NEON;
 *                       <arm_neon.h> is in.
 *
 * SOFTFILTER_NO_SIMD leaves them all out, for builds and tests of the
 * plain code. SOFTFILTER_FORCE_INLINE is for the per-pixel rule a vector
 * path falls back to. */

#ifndef SOFTFILTER_SIMD_H__
#define SOFTFILTER_SIMD_H__

#include <retro_inline.h>

/* For a filter's per-pixel rule where the vector path calls it: as a
 * call, the loop around it can cost detailed frames several percent. */
#if defined(_MSC_VER)
#define SOFTFILTER_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define SOFTFILTER_FORCE_INLINE INLINE __attribute__((always_inline))
#else
#define SOFTFILTER_FORCE_INLINE INLINE
#endif

#if !defined(SOFTFILTER_NO_SIMD) && (defined(__SSE2__) || defined(_M_X64) \
   || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define SOFTFILTER_HAVE_SSE2 1
#include <emmintrin.h>

#if defined(_MSC_VER) && !defined(__clang__)
#if _MSC_VER >= 1911
#define SOFTFILTER_HAVE_AVX2 1
#define SOFTFILTER_TARGET_AVX2
#endif
#elif defined(__clang__) && (defined(__SCE__) || defined(_MSC_VER))
#ifdef __AVX2__
#define SOFTFILTER_HAVE_AVX2 1
#define SOFTFILTER_TARGET_AVX2
#endif
#elif (defined(__clang__) && __clang_major__ >= 4) \
   || (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 5)
#define SOFTFILTER_HAVE_AVX2 1
#define SOFTFILTER_TARGET_AVX2 __attribute__((target("avx2")))
#endif

#ifdef SOFTFILTER_HAVE_AVX2
#include <immintrin.h>
#if defined(_MSC_VER) && !defined(__clang__)
#define SOFTFILTER_ZEROUPPER() _mm256_zeroupper()
#else
#define SOFTFILTER_ZEROUPPER()
#endif
#endif

#elif !defined(SOFTFILTER_NO_SIMD) && (defined(__ARM_NEON) \
   || defined(__ARM_NEON__) || defined(_M_ARM64))
#define SOFTFILTER_HAVE_NEON 1
#include <arm_neon.h>
#endif

#endif
