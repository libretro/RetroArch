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
   && (defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 9))
/* Baseline does not, but the toolchain can build the instructions into
 * one function without raising the ISA for the translation unit, so
 * compile them in and choose at runtime.  This is the common case: of
 * everything in tree only Makefile.libnx passes +crc explicitly, and
 * arm64 macOS gets it from the compiler default, which leaves Android,
 * Linux ARM, iOS and tvOS on the table path despite every one of those
 * CPUs from ARMv8.1 onwards having the instructions. */
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
#    define CRC32_HAVE_PCLMUL_PATH 1
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

static const uint32_t crc32_ogg_table[256] = {
   0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
   0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
   0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61,
   0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
   0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9,
   0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75,
   0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011,
   0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
   0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039,
   0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
   0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81,
   0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
   0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49,
   0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
   0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1,
   0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
   0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE,
   0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072,
   0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16,
   0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
   0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE,
   0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02,
   0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066,
   0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
   0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E,
   0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692,
   0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6,
   0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
   0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E,
   0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2,
   0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686,
   0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
   0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637,
   0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
   0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F,
   0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
   0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47,
   0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B,
   0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF,
   0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
   0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7,
   0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B,
   0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F,
   0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
   0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7,
   0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B,
   0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F,
   0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
   0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640,
   0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
   0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8,
   0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
   0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30,
   0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC,
   0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088,
   0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
   0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0,
   0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C,
   0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18,
   0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
   0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0,
   0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C,
   0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668,
   0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4
};

uint32_t encoding_crc32_ogg(uint32_t crc, const uint8_t *data, size_t len)
{
   size_t i;

   for (i = 0; i < len; i++)
      crc = (crc << 8) ^ crc32_ogg_table[(uint8_t)(crc >> 24) ^ data[i]];

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
