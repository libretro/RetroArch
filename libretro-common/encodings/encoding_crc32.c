/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_crc32.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <encodings/crc32.h>
#include <retro_endianness.h>
#include <retro_inline.h>

#include "encoding_crc32_tables.h"

/*
 * CRC-32, polynomial 0xEDB88320 (bit-reflected 0x04C11DB7).
 * Bit-exact with zlib crc32(), gzip and PNG.
 *
 * Three implementations, in descending order of speed:
 *
 *   1. Carry-less multiply folding (x86 PCLMULQDQ, chosen at runtime).
 *   2. ARMv8 CRC32 instructions (chosen at compile time).
 *   3. Slicing-by-8 over a const table (every other target, both endians).
 *
 * The byte-at-a-time loop survives only as the tail handler for the
 * table path; no target uses it as a whole-buffer implementation.
 */

#if defined(__ARM_FEATURE_CRC32)
/* Baseline already has the instructions, so use them unconditionally. */
#define CRC32_HAVE_ARM_PATH 1
#elif (defined(__aarch64__) || defined(_M_ARM64)) \
   && ((defined(__clang__) && __clang_major__ >= 16) \
      || (!defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 9))
/* Baseline does not, but the toolchain can build the instructions into
 * one function without raising the ISA for the translation unit, so
 * compile them in and choose at runtime.  This is the common case: of
 * everything in tree only Makefile.libnx passes +crc explicitly and
 * arm64 macOS gets it from the compiler default, so without this
 * branch Android, Linux ARM, iOS and tvOS would take the table path
 * despite having the instructions.  Whether they get it is a property
 * of the compiler, not the target: NDK r27c (clang 18) compiles this
 * branch in for aarch64-linux-android, an older clang fails the
 * predicate below and falls back.
 *
 * The version predicates are about the ACLE header, not codegen:
 * target("crc") lets the compiler emit the instructions, but __crc32b()
 * and friends still have to be declared.  GCC's aarch64 arm_acle.h has
 * declared them behind '#pragma GCC target ("+nothing+crc")' since GCC 8.
 * Clang gated them on __ARM_FEATURE_CRC32 alone through 15.x, which a
 * plain armv8-a baseline does not set, so older clang fails on
 * -Werror=implicit-function-declaration; 16.x added '|| __ARM_64BIT_STATE'.
 * Apple numbers __clang_major__ its own way, so this also drops Xcode 15
 * to the table path, which is the safe direction. */
#define CRC32_HAVE_ARM_PATH     1
#define CRC32_ARM_NEEDS_TARGET  1
#define CRC32_ARM_DISPATCH      1
#elif defined(__x86_64__) || defined(__i386__) \
   || defined(_M_X64)     || defined(_M_IX86)
/* Being on x86 is necessary but not sufficient: the toolchain also has
 * to be able to build the intrinsics.  Prefer the feature tests, and
 * never fall back on a bare __GNUC__ version comparison -- Clang
 * reports itself as GCC 4.2 forever, so a ">= 4.4" predicate rejects
 * every Clang build, which is every Apple, Android NDK, Emscripten and
 * PS4 toolchain.  The version fallback below carries !__clang__ for
 * that reason and exists only for GCC old enough to lack
 * __has_attribute. */
#  if defined(_MSC_VER)
     /* wmmintrin.h arrived in Visual Studio 2008 SP1, which is not
      * distinguishable from RTM by _MSC_VER, so require VS2010. */
#    if _MSC_VER >= 1600
#      define CRC32_HAVE_PCLMUL_PATH 1
#    endif
#  elif defined(__has_attribute) && defined(__has_include)
#    if __has_attribute(target) && __has_include(<wmmintrin.h>)
#      define CRC32_HAVE_PCLMUL_PATH 1
#    endif
#  elif defined(__GNUC__) && !defined(__clang__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
     /* GCC 4.4 introduced both wmmintrin.h and x86 target attributes. */
#    define CRC32_HAVE_PCLMUL_PATH 1
#  endif
#endif

/* ------------------------------------------------------------------ */
/* Portable slicing-by-8                                              */
/* ------------------------------------------------------------------ */

#if !defined(CRC32_HAVE_ARM_PATH) || defined(CRC32_ARM_DISPATCH)

/* Load a 32-bit little-endian word without assuming pointer alignment
 * and without punning through a uint32_t lvalue. A constant-size memcpy
 * lowers to a single load on every compiler we support. */
static INLINE uint32_t crc32_load_le32(const uint8_t *p)
{
   uint32_t v;
   memcpy(&v, p, sizeof(v));
#ifdef MSB_FIRST
   v = SWAP32(v);
#endif
   return v;
}

static uint32_t crc32_slice_by_8(uint32_t crc, const uint8_t *data, size_t len)
{
   while (len >= 8)
   {
      uint32_t lo = crc32_load_le32(data)     ^ crc;
      uint32_t hi = crc32_load_le32(data + 4);

      crc = crc32_slice8[7][ lo        & 0xFF]
          ^ crc32_slice8[6][(lo >>  8) & 0xFF]
          ^ crc32_slice8[5][(lo >> 16) & 0xFF]
          ^ crc32_slice8[4][(lo >> 24) & 0xFF]
          ^ crc32_slice8[3][ hi        & 0xFF]
          ^ crc32_slice8[2][(hi >>  8) & 0xFF]
          ^ crc32_slice8[1][(hi >> 16) & 0xFF]
          ^ crc32_slice8[0][(hi >> 24) & 0xFF];

      data += 8;
      len  -= 8;
   }

   /* Decrement inside the body rather than in the loop test: the
    * test-and-decrement form wraps len to SIZE_MAX on the final
    * iteration. That is defined behaviour, but it trips
    * -fsanitize=unsigned-integer-overflow and there is no reason to
    * make callers filter it out. */
   while (len > 0)
   {
      crc = crc32_slice8[0][(crc ^ (*data++)) & 0xFF] ^ (crc >> 8);
      len--;
   }

   return crc;
}
#endif /* !CRC32_HAVE_ARM_PATH || CRC32_ARM_DISPATCH */

/* ------------------------------------------------------------------ */
/* x86 carry-less multiply folding                                    */
/* ------------------------------------------------------------------ */

#if defined(CRC32_HAVE_PCLMUL_PATH)

#include <features/features_cpu.h>

#if defined(_MSC_VER)
#include <wmmintrin.h>
#define CRC32_TARGET_PCLMUL
#else
#include <emmintrin.h>
#include <wmmintrin.h>
#define CRC32_TARGET_PCLMUL __attribute__((target("pclmul")))
#endif

/* Folding constants, all of the form
 *    K(n) = bit_reflect_32(x^n mod P(x)) << 1
 * with P(x) = 0x104C11DB7:
 *
 *    k1 = K(4*128 + 32)  advance four 128-bit accumulators
 *    k2 = K(4*128 - 32)
 *    k3 = K(  128 + 32)  advance one 128-bit accumulator
 *    k4 = K(  128 - 32)
 *    k5 = K(64)          reduce 128 -> 64
 *    mu = bit_reflect_33(floor(x^64 / P(x)))   Barrett quotient
 *    P' = bit_reflect_33(P(x))
 */
/* Built with _mm_set_epi32 rather than _mm_set_epi64x: the latter is
 * not available in 32-bit MSVC. Each pair is {high dword, low dword}. */
#define CRC32_K1_HI 0x00000001
#define CRC32_K1_LO 0x54442BD4
#define CRC32_K2_HI 0x00000001
#define CRC32_K2_LO 0xC6E41596
#define CRC32_K3_HI 0x00000001
#define CRC32_K3_LO 0x751997D0
#define CRC32_K4_HI 0x00000000
#define CRC32_K4_LO 0xCCAA009E
#define CRC32_K5_HI 0x00000001
#define CRC32_K5_LO 0x63CD6124
#define CRC32_MU_HI 0x00000001
#define CRC32_MU_LO 0xF7011641
#define CRC32_PP_HI 0x00000001
#define CRC32_PP_LO 0xDB710641

CRC32_TARGET_PCLMUL
static INLINE __m128i crc32_fold16(__m128i x, __m128i k)
{
   return _mm_xor_si128(_mm_clmulepi64_si128(x, k, 0x00),
                        _mm_clmulepi64_si128(x, k, 0x11));
}

CRC32_TARGET_PCLMUL
static uint32_t crc32_pclmul(uint32_t crc, const uint8_t *data, size_t len)
{
   const __m128i k1k2   = _mm_set_epi32((int)CRC32_K2_HI, (int)CRC32_K2_LO,
                                        (int)CRC32_K1_HI, (int)CRC32_K1_LO);
   const __m128i k3k4   = _mm_set_epi32((int)CRC32_K4_HI, (int)CRC32_K4_LO,
                                        (int)CRC32_K3_HI, (int)CRC32_K3_LO);
   const __m128i k5     = _mm_set_epi32(0, 0,
                                        (int)CRC32_K5_HI, (int)CRC32_K5_LO);
   const __m128i mupp   = _mm_set_epi32((int)CRC32_PP_HI, (int)CRC32_PP_LO,
                                        (int)CRC32_MU_HI, (int)CRC32_MU_LO);
   const __m128i mask32 = _mm_set_epi32(0, 0, 0, -1);
   __m128i x0, x1, x2, x3, t;

   /* Below four accumulators' worth, the reduction tail dominates. */
   if (len < 64)
      return crc32_slice_by_8(crc, data, len);

   x0 = _mm_loadu_si128((const __m128i *)(data +  0));
   x1 = _mm_loadu_si128((const __m128i *)(data + 16));
   x2 = _mm_loadu_si128((const __m128i *)(data + 32));
   x3 = _mm_loadu_si128((const __m128i *)(data + 48));
   x0 = _mm_xor_si128(x0, _mm_cvtsi32_si128((int)crc));
   data += 64;
   len  -= 64;

   /* Four independent chains, 64 bytes per iteration. */
   while (len >= 64)
   {
      x0 = _mm_xor_si128(crc32_fold16(x0, k1k2),
            _mm_loadu_si128((const __m128i *)(data +  0)));
      x1 = _mm_xor_si128(crc32_fold16(x1, k1k2),
            _mm_loadu_si128((const __m128i *)(data + 16)));
      x2 = _mm_xor_si128(crc32_fold16(x2, k1k2),
            _mm_loadu_si128((const __m128i *)(data + 32)));
      x3 = _mm_xor_si128(crc32_fold16(x3, k1k2),
            _mm_loadu_si128((const __m128i *)(data + 48)));
      data += 64;
      len  -= 64;
   }

   /* Collapse the four chains into one. */
   x1 = _mm_xor_si128(crc32_fold16(x0, k3k4), x1);
   x2 = _mm_xor_si128(crc32_fold16(x1, k3k4), x2);
   x0 = _mm_xor_si128(crc32_fold16(x2, k3k4), x3);

   while (len >= 16)
   {
      x0 = _mm_xor_si128(crc32_fold16(x0, k3k4),
            _mm_loadu_si128((const __m128i *)data));
      data += 16;
      len  -= 16;
   }

   /* 128 -> 64 */
   t  = _mm_clmulepi64_si128(x0, k3k4, 0x10);
   x0 = _mm_xor_si128(_mm_srli_si128(x0, 8), t);

   /* 64 -> 32 */
   t  = _mm_clmulepi64_si128(_mm_and_si128(x0, mask32), k5, 0x00);
   x0 = _mm_xor_si128(_mm_srli_si128(x0, 4), t);

   /* Barrett reduction to the final 32-bit remainder. */
   t  = _mm_clmulepi64_si128(_mm_and_si128(x0, mask32), mupp, 0x00);
   t  = _mm_clmulepi64_si128(_mm_and_si128(t,  mask32), mupp, 0x10);
   x0 = _mm_xor_si128(x0, t);

   crc = (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(x0, 4));

   if (len)
      crc = crc32_slice_by_8(crc, data, len);
   return crc;
}

/* Detection lives in cpu_features_get(), which probes once behind an
 * acquire/release pair and is the frontend's single answer to "what
 * does this CPU have".  PCLMULQDQ needs no OSXSAVE/XCR0 dance: it is
 * an XMM instruction, so CR4.OSFXSR is the only OS requirement and
 * that is universal.  Only AVX-width state needs the xgetbv check. */
static int crc32_have_pclmul(void)
{
   return (cpu_features_get() & RETRO_SIMD_PCLMUL) != 0;
}

#endif /* x86 */

/* ------------------------------------------------------------------ */
/* ARMv8 CRC32 instructions                                           */
/* ------------------------------------------------------------------ */

#if defined(CRC32_HAVE_ARM_PATH)

/* arm64_neon.h is an MSVC header. Selecting it on _M_ARM64 alone breaks
 * every Clang that defines that macro for Windows-on-ARM compatibility,
 * MSYS2's CLANGARM64 among them, which has arm_acle.h and no
 * arm64_neon.h. Key on the compiler rather than the target macro. */
#if defined(_MSC_VER) && !defined(__clang__)
#include <arm64_neon.h>
#else
#include <arm_acle.h>
#endif

#if defined(CRC32_ARM_NEEDS_TARGET)
#include <features/features_cpu.h>
#if defined(__clang__)
#define CRC32_TARGET_ARM __attribute__((target("crc")))
#else
#define CRC32_TARGET_ARM __attribute__((target("arch=armv8-a+crc")))
#endif
#else
#define CRC32_TARGET_ARM
#endif

CRC32_TARGET_ARM
static uint32_t crc32_arm(uint32_t crc, const uint8_t *data, size_t len)
{
   while (((uintptr_t)data & 7) && len > 0)
   {
      crc = __crc32b(crc, *data);
      data++;
      len--;
   }
   while (len >= 8)
   {
      uint64_t v;
      memcpy(&v, data, sizeof(v));
      crc = __crc32d(crc, v);
      data += 8;
      len  -= 8;
   }
   while (len > 0)
   {
      crc = __crc32b(crc, *data++);
      len--;
   }
   return crc;
}
#endif /* __ARM_FEATURE_CRC32 */

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ *
 * Ogg page CRC                                                         *
 * ------------------------------------------------------------------ *
 *
 * A different checksum that shares the CRC-32 name: the same generator
 * polynomial written the other way round (0x04C11DB7, MSB-first rather
 * than reflected), seeded with zero and with no final complement. Ogg
 * specifies it that way, so it is not interchangeable with
 * encoding_crc32() above and the two must not be confused.
 *
 * The table is generated rather than built at first use. The three
 * callers that used to carry their own each guarded a lazily-filled
 * table with a non-atomic flag - the pattern removed from this file by
 * 19278 - and one of them called its initialiser on every entry with a
 * comment saying that avoided the race, which it does not: concurrent
 * writers storing identical values are still a data race. A const
 * table has no initialiser to race on.
 */

/* ------------------------------------------------------------------ *
 * CRC-16/CCITT-FALSE                                                   *
 * ------------------------------------------------------------------ *
 *
 * Polynomial 0x1021 MSB-first, no final xor. The seed is the caller's:
 * this variant is conventionally started at 0xFFFF, which is what the
 * CHD readers pass, but nothing here assumes it.
 *
 * Sixteen bits rather than thirty-two, and a different polynomial from
 * either function above, so it shares no table with them - only the
 * slicing arrangement, which is a property of the technique rather
 * than of any particular CRC.
 */

uint16_t encoding_crc16_ccitt(uint16_t crc, const uint8_t *data, size_t len)
{
   while (len >= 8)
   {
      crc = (uint16_t)(
              crc16_ccitt_slice8[7][(uint8_t)(data[0] ^ (crc >> 8))]
            ^ crc16_ccitt_slice8[6][(uint8_t)(data[1] ^  crc      )]
            ^ crc16_ccitt_slice8[5][data[2]]
            ^ crc16_ccitt_slice8[4][data[3]]
            ^ crc16_ccitt_slice8[3][data[4]]
            ^ crc16_ccitt_slice8[2][data[5]]
            ^ crc16_ccitt_slice8[1][data[6]]
            ^ crc16_ccitt_slice8[0][data[7]]);

      data += 8;
      len  -= 8;
   }

   /* Decremented inside the body, as in the two above, so the loop
    * test cannot wrap len to SIZE_MAX on its last pass. */
   while (len > 0)
   {
      crc = (uint16_t)((crc << 8)
            ^ crc16_ccitt_slice8[0][(uint8_t)((crc >> 8) ^ (*data++))]);
      len--;
   }

   return crc;
}

uint32_t encoding_crc32_ogg(uint32_t crc, const uint8_t *data, size_t len)
{
   /* Slicing-by-8, as for the reflected variant above, but assembling
    * each word from bytes rather than loading it. The reflected path
    * can load little-endian and let the table order absorb it; this
    * one consumes bytes most-significant first, so a load would need
    * a byte swap on one endianness or the other. Byte assembly costs
    * a little of the gain and keeps a single path for both. */
   while (len >= 8)
   {
      crc ^= ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
           | ((uint32_t)data[2] <<  8) |  (uint32_t)data[3];

      crc  = crc32_ogg_slice8[7][(crc >> 24) & 0xFF]
           ^ crc32_ogg_slice8[6][(crc >> 16) & 0xFF]
           ^ crc32_ogg_slice8[5][(crc >>  8) & 0xFF]
           ^ crc32_ogg_slice8[4][ crc        & 0xFF]
           ^ crc32_ogg_slice8[3][data[4]]
           ^ crc32_ogg_slice8[2][data[5]]
           ^ crc32_ogg_slice8[1][data[6]]
           ^ crc32_ogg_slice8[0][data[7]];

      data += 8;
      len  -= 8;
   }

   /* Decremented inside the body for the same reason as the reflected
    * tail: the test-and-decrement form wraps len to SIZE_MAX on the
    * last iteration and trips -fsanitize=unsigned-integer-overflow. */
   while (len > 0)
   {
      crc = (crc << 8) ^ crc32_ogg_slice8[0][(uint8_t)(crc >> 24) ^ (*data++)];
      len--;
   }

   return crc;
}

uint32_t encoding_crc32(uint32_t crc, const uint8_t *data, size_t len)
{
   crc = ~crc;

#if defined(CRC32_ARM_DISPATCH)
   if (cpu_features_get() & RETRO_SIMD_CRC32)
      crc = crc32_arm(crc, data, len);
   else
      crc = crc32_slice_by_8(crc, data, len);
#elif defined(CRC32_HAVE_ARM_PATH)
   crc = crc32_arm(crc, data, len);
#elif defined(CRC32_HAVE_PCLMUL_PATH)
   if (len >= 64 && crc32_have_pclmul())
      crc = crc32_pclmul(crc, data, len);
   else
      crc = crc32_slice_by_8(crc, data, len);
#else
   crc = crc32_slice_by_8(crc, data, len);
#endif

   return ~crc;
}
