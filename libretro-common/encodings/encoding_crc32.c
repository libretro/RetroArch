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
 *   1. Carry-less multiply folding (x86 PCLMULQDQ, chosen at runtime),
 *      512 bits wide where AVX-512 has VPCLMULQDQ.
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
   /* VPCLMULQDQ on ZMM: GCC 8, clang 6 (Apple's 11), Visual Studio 2019;
    * clang for PlayStation and clang-cl declare it only where the whole
    * build has it. x86-64 only, as XXH3's AVX-512 is. */
#  if defined(CRC32_HAVE_PCLMUL_PATH) && (defined(__x86_64__) || defined(_M_X64))
#    if defined(__clang__) && (defined(__SCE__) || defined(_MSC_VER))
#      if defined(__VPCLMULQDQ__) && defined(__AVX512BW__) \
         && defined(__AVX512VL__)
#        define CRC32_HAVE_VPCLMUL_PATH 1
#        define CRC32_TARGET_VPCLMUL
#      endif
#    elif defined(_MSC_VER)
#      if _MSC_VER >= 1920
#        define CRC32_HAVE_VPCLMUL_PATH 1
#        define CRC32_TARGET_VPCLMUL
#      endif
#    elif defined(__clang__)
#      if (defined(__apple_build_version__) && __clang_major__ >= 11) \
         || (!defined(__apple_build_version__) && __clang_major__ >= 6)
#        define CRC32_HAVE_VPCLMUL_PATH 1
#      endif
#    elif defined(__GNUC__) && __GNUC__ >= 8
#      define CRC32_HAVE_VPCLMUL_PATH 1
#    endif
#    if defined(CRC32_HAVE_VPCLMUL_PATH) && !defined(CRC32_TARGET_VPCLMUL)
#      define CRC32_TARGET_VPCLMUL __attribute__((target( \
         "avx512f,avx512bw,avx512vl,vpclmulqdq,pclmul")))
#    endif
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

/* The 128-bit tails stay out of the AVX-512 functions: inlined there,
 * GCC may give their PCLMULQDQ an XMM16-31 register, an EVEX form an
 * assembler older than binutils 2.30 cannot encode */
#if defined(CRC32_HAVE_VPCLMUL_PATH) && defined(__GNUC__) \
 && !defined(__clang__)
#define CRC32_TAIL_NOINLINE __attribute__((noinline))
#else
#define CRC32_TAIL_NOINLINE
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

CRC32_TARGET_PCLMUL CRC32_TAIL_NOINLINE
static uint32_t crc32_pclmul_tail(__m128i x0, const uint8_t *data, size_t len);

CRC32_TARGET_PCLMUL
static uint32_t crc32_pclmul(uint32_t crc, const uint8_t *data, size_t len)
{
   const __m128i k1k2   = _mm_set_epi32((int)CRC32_K2_HI, (int)CRC32_K2_LO,
                                        (int)CRC32_K1_HI, (int)CRC32_K1_LO);
   const __m128i k3k4   = _mm_set_epi32((int)CRC32_K4_HI, (int)CRC32_K4_LO,
                                        (int)CRC32_K3_HI, (int)CRC32_K3_LO);
   __m128i x0, x1, x2, x3;

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
   return crc32_pclmul_tail(x0, data, len);
}

/* One 128-bit accumulator and what is left, under 64 octets more than
 * its 16: folded on 16 at a time, then reduced to the remainder */
CRC32_TARGET_PCLMUL CRC32_TAIL_NOINLINE
static uint32_t crc32_pclmul_tail(__m128i x0, const uint8_t *data, size_t len)
{
   const __m128i k3k4   = _mm_set_epi32((int)CRC32_K4_HI, (int)CRC32_K4_LO,
                                        (int)CRC32_K3_HI, (int)CRC32_K3_LO);
   const __m128i k5     = _mm_set_epi32(0, 0,
                                        (int)CRC32_K5_HI, (int)CRC32_K5_LO);
   const __m128i mupp   = _mm_set_epi32((int)CRC32_PP_HI, (int)CRC32_PP_LO,
                                        (int)CRC32_MU_HI, (int)CRC32_MU_LO);
   const __m128i mask32 = _mm_set_epi32(0, 0, 0, -1);
   __m128i t;
   uint32_t crc;

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

#if defined(CRC32_HAVE_VPCLMUL_PATH)
#include <immintrin.h>
#include <retro_atomic.h>
#if defined(_MSC_VER)
#include <intrin.h>
#else
/* Unity builds bring <cpuid.h> in more than once, and the one in
 * clang 12 and older has no include guard */
#ifndef RETRO_CPUID_H_INCLUDED
#define RETRO_CPUID_H_INCLUDED
#include <cpuid.h>
#endif
#endif

/* AVX-512 with VPCLMULQDQ: four 128-bit lanes to a register, four
 * registers, 256 octets a turn; then one register 64 at a time, its
 * lanes folded into the one the 128-bit tail goes on with */

/* The K() above of each lane: x^(4*512 + 32) and x^(4*512 - 32), and so
 * on, as {high dword, low dword} pairs, the - 32 one first */
#define CRC32_K2048 0x00000001, 0x322D1430, 0x00000001, 0x1542778A
#define CRC32_K512  0x00000001, 0xC6E41596, 0x00000001, 0x54442BD4
#define CRC32_K384  0x00000001, 0x74359406, 0x00000000, 0x3DB1ECDC
#define CRC32_K256  0x00000001, 0x5A546366, 0x00000000, 0xF1DA05AA
#define CRC32_K128  0x00000000, 0xCCAA009E, 0x00000001, 0x751997D0

#define CRC32_LANE(k) _mm512_broadcast_i32x4(_mm_set_epi32(k))

#if defined(__GNUC__) && !defined(__clang__)
/* GCC hands VPCLMULQDQ to the system assembler, and binutils before
 * 2.30 cannot encode it on ZMM. So the instruction is spelled out in
 * EVEX bytes by a gas macro that reads the register numbers off the
 * names the compiler chose: same instruction, same registers, any gas
 * that has AVX-512F. Defined once per assembly file, on first use. */
#define CRC32_VPCLMUL_GAS \
   ".ifndef .Lcrc32_vpclmul_gas\n\t" \
   ".Lcrc32_vpclmul_gas = 1\n\t" \
   ".macro crc32_zmm_num sym, reg\n\t" \
   ".irp i,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15," \
   "16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31\n\t" \
   ".ifc \\reg,%%zmm\\i\n\t\\sym = \\i\n\t.endif\n\t" \
   ".ifc \\reg,zmm\\i\n\t\\sym = \\i\n\t.endif\n\t" \
   ".endr\n\t" \
   ".endm\n\t" \
   /* vpclmulqdq $imm, src2, src1, dst: EVEX.512.66.0F3A.W0 44 /r ib */ \
   ".macro crc32_vpclmulqdq imm, src2, src1, dst\n\t" \
   "crc32_zmm_num .Lcrc32_vd, \\dst\n\t" \
   "crc32_zmm_num .Lcrc32_v1, \\src1\n\t" \
   "crc32_zmm_num .Lcrc32_v2, \\src2\n\t" \
   ".byte 0x62\n\t" \
   ".byte ((~.Lcrc32_vd & 8) << 4) | ((~.Lcrc32_v2 & 16) << 2)" \
   " | ((~.Lcrc32_v2 & 8) << 2) | (~.Lcrc32_vd & 16) | 3\n\t" \
   ".byte ((~.Lcrc32_v1 & 15) << 3) | 5\n\t" \
   ".byte 0x40 | ((~.Lcrc32_v1 & 16) >> 1)\n\t" \
   ".byte 0x44\n\t" \
   ".byte 0xC0 | ((.Lcrc32_vd & 7) << 3) | (.Lcrc32_v2 & 7)\n\t" \
   ".byte \\imm\n\t" \
   ".endm\n\t" \
   ".endif\n\t"

CRC32_TARGET_VPCLMUL __attribute__((always_inline))
static INLINE __m512i crc32_fold64(__m512i x, __m512i k, __m512i d)
{
   __m512i lo, hi;
   __asm__(CRC32_VPCLMUL_GAS
         "crc32_vpclmulqdq 0x11, %[k], %[x], %[hi]\n\t"
         "crc32_vpclmulqdq 0x00, %[k], %[x], %[lo]"
         : [hi] "=&v" (hi), [lo] "=v" (lo)
         : [x] "v" (x), [k] "v" (k));
   return _mm512_ternarylogic_epi64(lo, hi, d, 0x96);
}
#define CRC32_FOLD64(x, k, d) crc32_fold64((x), (k), (d))
#else
#define CRC32_FOLD64(x, k, d) _mm512_ternarylogic_epi64( \
      _mm512_clmulepi64_epi128((x), (k), 0x00), \
      _mm512_clmulepi64_epi128((x), (k), 0x11), (d), 0x96)
#endif

CRC32_TARGET_VPCLMUL
static uint32_t crc32_vpclmul(uint32_t crc, const uint8_t *data, size_t len)
{
   const __m512i k2048 = CRC32_LANE(CRC32_K2048);
   const __m512i k512  = CRC32_LANE(CRC32_K512);
   /* lane 0 on past three, 1 past two, 2 past one, 3 as it is */
   const __m512i kl    = _mm512_inserti32x4(_mm512_inserti32x4(
         _mm512_inserti32x4(_mm512_setzero_si512(),
            _mm_set_epi32(CRC32_K384), 0), _mm_set_epi32(CRC32_K256), 1),
         _mm_set_epi32(CRC32_K128), 2);
   __m512i z0, z1, z2, z3;
   __m256i y;

   z0 = _mm512_xor_si512(_mm512_loadu_si512((const void*)data),
         _mm512_castsi128_si512(_mm_cvtsi32_si128((int)crc)));
   z1 = _mm512_loadu_si512((const void*)(data +  64));
   z2 = _mm512_loadu_si512((const void*)(data + 128));
   z3 = _mm512_loadu_si512((const void*)(data + 192));
   data += 256;
   len  -= 256;
   while (len >= 256)
   {
      z0 = CRC32_FOLD64(z0, k2048, _mm512_loadu_si512((const void*)data));
      z1 = CRC32_FOLD64(z1, k2048,
            _mm512_loadu_si512((const void*)(data +  64)));
      z2 = CRC32_FOLD64(z2, k2048,
            _mm512_loadu_si512((const void*)(data + 128)));
      z3 = CRC32_FOLD64(z3, k2048,
            _mm512_loadu_si512((const void*)(data + 192)));
      data += 256;
      len  -= 256;
   }
   z1 = CRC32_FOLD64(z0, k512, z1);
   z2 = CRC32_FOLD64(z1, k512, z2);
   z0 = CRC32_FOLD64(z2, k512, z3);
   while (len >= 64)
   {
      z0 = CRC32_FOLD64(z0, k512, _mm512_loadu_si512((const void*)data));
      data += 64;
      len  -= 64;
   }
   z0 = _mm512_mask_mov_epi64(CRC32_FOLD64(z0, kl, _mm512_setzero_si512()),
         0xC0, z0);
   y  = _mm256_xor_si256(_mm512_castsi512_si256(z0),
         _mm512_extracti64x4_epi64(z0, 1));
   return crc32_pclmul_tail(_mm_xor_si128(_mm256_castsi256_si128(y),
         _mm256_extracti128_si256(y, 1)), data, len);
}
#undef CRC32_LANE

/* AVX-512, which says the OS keeps ZMM state, and leaf 7's VPCLMULQDQ
 * (ECX bit 10): asked once */
static retro_atomic_int_t crc32_vp_state; /* 0 not yet, 1 no, 2 yes */

static int crc32_have_vpclmul(void)
{
   int st = retro_atomic_load_acquire_int(&crc32_vp_state);
   if (!st)
   {
      uint32_t ecx = 0;
      if (cpu_features_get() & RETRO_SIMD_AVX512)
      {
#if defined(_MSC_VER)
         int r[4];
         __cpuidex(r, 0, 0);
         if (r[0] >= 7)
         {
            __cpuidex(r, 7, 0);
            ecx = (uint32_t)r[2];
         }
#else
         unsigned a, b, c, d;
         if (__get_cpuid_max(0, NULL) >= 7)
         {
            __cpuid_count(7, 0, a, b, c, d);
            ecx = c;
         }
#endif
      }
      st = (ecx & (1u << 10)) ? 2 : 1;
      retro_atomic_store_release_int(&crc32_vp_state, st);
   }
   return st == 2;
}
#endif


/* Detection lives in cpu_features_get(), which probes once behind an
 * acquire/release pair and is the frontend's single answer to "what
 * does this CPU have".  PCLMULQDQ needs no OSXSAVE/XCR0 dance: it is
 * an XMM instruction, so CR4.OSFXSR is the only OS requirement and
 * that is universal.  Only AVX-width state needs the xgetbv check. */
static int crc32_have_pclmul(void)
{
   return (cpu_features_get() & RETRO_SIMD_PCLMUL) != 0;
}

/* clang for PlayStation and clang-cl: where the build has the ISAs */
#if !(defined(__clang__) && (defined(_MSC_VER) || defined(__SCE__))) \
   || (defined(__SSSE3__) && defined(__PCLMUL__))
/* ------------------------------------------------------------------ */
/* MSB-first CRCs by carry-less folding: Ogg's CRC-32, CRC-16/CCITT   */
/* ------------------------------------------------------------------ */

/* Neither is reflected, so the octets are the polynomial as they come:
 * each 16 of them, byte-swapped, is 128 coefficients with the first
 * octet's top bit the highest. The register's seed is the same as that
 * many bits added to the front of the message. Folding keeps one
 * accumulator A with A ~ every octet so far (mod P), placed as their
 * last 16: A * x^D = A_hi * x^(D+64) + A_lo * x^D, each part times
 * x^n mod P, which for a 16- or 32-bit P leaves the product inside 128
 * bits. A itself and what is left after it are then the table path's,
 * from a seed of 0, which needs no reduction of its own.
 *
 * The constants are x^n mod P, for 0x104C11DB7 and 0x11021 */

static uint32_t crc32_ogg_table(uint32_t crc, const uint8_t *data,
      size_t len);
static uint32_t crc16_ccitt_table(uint32_t crc, const uint8_t *data,
      size_t len);

typedef uint32_t (*crc_msb_table_t)(uint32_t crc, const uint8_t *data,
      size_t len);

typedef struct crc_msb
{
   uint32_t        k576, k512;   /* four accumulators on */
   uint32_t        k192, k128;   /* one */
   uint32_t        k2112, k2048; /* four 512-bit ones on */
   uint32_t        k448, k384, k320, k256; /* lanes into one */
   unsigned        width;
   crc_msb_table_t table;
} crc_msb_t;

static const crc_msb_t crc_msb_ogg = {
   0x8833794CU, 0xE6228B11U, 0xC5B9CD4CU, 0xE8A45605U,
   0xCBCF3BCBU, 0x88FE2237U,
   0x64BF7A9BU, 0x8C3828A8U, 0x569700E5U, 0x75BE46B7U,
   32, crc32_ogg_table
};

static const crc_msb_t crc_msb_c16 = {
   0x8832U, 0x13FCU, 0x650BU, 0xAEFCU,
   0xF17EU, 0xFD50U,
   0x2535U, 0xCDE2U, 0x26AAU, 0x8E29U,
   16, crc16_ccitt_table
};

#if defined(_MSC_VER)
#include <tmmintrin.h>
#define CRC32_TARGET_MSB
#else
#include <tmmintrin.h>
#define CRC32_TARGET_MSB __attribute__((target("pclmul,ssse3")))
#endif

#define CRC_MSB_K(hi, lo) _mm_set_epi32(0, (int)(hi), 0, (int)(lo))
#define CRC_MSB_REV _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, \
      7, 6, 5, 4, 3, 2, 1, 0)
#define CRC_MSB_LOAD(p) _mm_shuffle_epi8( \
      _mm_loadu_si128((const __m128i*)(const void*)(p)), rev)

CRC32_TARGET_MSB
static INLINE __m128i crc_msb_fold(__m128i x, __m128i k, __m128i d)
{
   return _mm_xor_si128(_mm_xor_si128(_mm_clmulepi64_si128(x, k, 0x11),
         _mm_clmulepi64_si128(x, k, 0x00)), d);
}

/* One accumulator, the octets after it 16 at a time, then it and the
 * rest through the table */
CRC32_TARGET_MSB CRC32_TAIL_NOINLINE
static uint32_t crc_msb_tail(const crc_msb_t *m, __m128i x0,
      const uint8_t *data, size_t len)
{
   const __m128i rev  = CRC_MSB_REV;
   const __m128i k128 = CRC_MSB_K(m->k192, m->k128);
   uint8_t       a[16];
   while (len >= 16)
   {
      x0    = crc_msb_fold(x0, k128, CRC_MSB_LOAD(data));
      data += 16;
      len  -= 16;
   }
   _mm_storeu_si128((__m128i*)(void*)a, _mm_shuffle_epi8(x0, rev));
   return m->table(m->table(0, a, 16), data, len);
}

/* At least 64 octets, from the register @crc */
CRC32_TARGET_MSB
static uint32_t crc_msb_pclmul(const crc_msb_t *m, uint32_t crc,
      const uint8_t *data, size_t len)
{
   const __m128i rev  = CRC_MSB_REV;
   const __m128i k512 = CRC_MSB_K(m->k576, m->k512);
   const __m128i k128 = CRC_MSB_K(m->k192, m->k128);
   __m128i x0, x1, x2, x3;

   x0 = _mm_xor_si128(CRC_MSB_LOAD(data),
         _mm_set_epi32((int)(crc << (32 - m->width)), 0, 0, 0));
   x1 = CRC_MSB_LOAD(data + 16);
   x2 = CRC_MSB_LOAD(data + 32);
   x3 = CRC_MSB_LOAD(data + 48);
   data += 64;
   len  -= 64;
   while (len >= 64)
   {
      x0    = crc_msb_fold(x0, k512, CRC_MSB_LOAD(data));
      x1    = crc_msb_fold(x1, k512, CRC_MSB_LOAD(data + 16));
      x2    = crc_msb_fold(x2, k512, CRC_MSB_LOAD(data + 32));
      x3    = crc_msb_fold(x3, k512, CRC_MSB_LOAD(data + 48));
      data += 64;
      len  -= 64;
   }
   x1 = crc_msb_fold(x0, k128, x1);
   x2 = crc_msb_fold(x1, k128, x2);
   x0 = crc_msb_fold(x2, k128, x3);
   return crc_msb_tail(m, x0, data, len);
}

#if defined(CRC32_HAVE_VPCLMUL_PATH)
#define CRC_MSB_K4(hi, lo) _mm512_broadcast_i32x4(CRC_MSB_K(hi, lo))
#define CRC_MSB_LOAD64(p) _mm512_shuffle_epi8( \
      _mm512_loadu_si512((const void*)(p)), rev4)

/* At least 256 octets: four 512-bit accumulators, then one, its lanes
 * into the 128-bit tail */
CRC32_TARGET_VPCLMUL
static uint32_t crc_msb_vpclmul(const crc_msb_t *m, uint32_t crc,
      const uint8_t *data, size_t len)
{
   const __m512i rev4  = _mm512_broadcast_i32x4(CRC_MSB_REV);
   const __m512i k2048 = CRC_MSB_K4(m->k2112, m->k2048);
   const __m512i k512  = CRC_MSB_K4(m->k576, m->k512);
   const __m512i kl    = _mm512_inserti32x4(_mm512_inserti32x4(
         _mm512_inserti32x4(_mm512_setzero_si512(),
            CRC_MSB_K(m->k448, m->k384), 0), CRC_MSB_K(m->k320, m->k256), 1),
         CRC_MSB_K(m->k192, m->k128), 2);
   __m512i z0, z1, z2, z3;
   __m256i y;

   z0 = _mm512_xor_si512(CRC_MSB_LOAD64(data), _mm512_castsi128_si512(
         _mm_set_epi32((int)(crc << (32 - m->width)), 0, 0, 0)));
   z1 = CRC_MSB_LOAD64(data +  64);
   z2 = CRC_MSB_LOAD64(data + 128);
   z3 = CRC_MSB_LOAD64(data + 192);
   data += 256;
   len  -= 256;
   while (len >= 256)
   {
      z0    = CRC32_FOLD64(z0, k2048, CRC_MSB_LOAD64(data));
      z1    = CRC32_FOLD64(z1, k2048, CRC_MSB_LOAD64(data +  64));
      z2    = CRC32_FOLD64(z2, k2048, CRC_MSB_LOAD64(data + 128));
      z3    = CRC32_FOLD64(z3, k2048, CRC_MSB_LOAD64(data + 192));
      data += 256;
      len  -= 256;
   }
   z1 = CRC32_FOLD64(z0, k512, z1);
   z2 = CRC32_FOLD64(z1, k512, z2);
   z0 = CRC32_FOLD64(z2, k512, z3);
   while (len >= 64)
   {
      z0    = CRC32_FOLD64(z0, k512, CRC_MSB_LOAD64(data));
      data += 64;
      len  -= 64;
   }
   z0 = _mm512_mask_mov_epi64(CRC32_FOLD64(z0, kl, _mm512_setzero_si512()),
         0xC0, z0);
   y  = _mm256_xor_si256(_mm512_castsi512_si256(z0),
         _mm512_extracti64x4_epi64(z0, 1));
   return crc_msb_tail(m, _mm_xor_si128(_mm256_castsi256_si128(y),
         _mm256_extracti128_si256(y, 1)), data, len);
}
#undef CRC_MSB_LOAD64
#undef CRC32_FOLD64
#undef CRC_MSB_K4
#endif

/* The fold, where the CPU has it and @len is worth it; else the table */
static uint32_t crc_msb(const crc_msb_t *m, uint32_t crc,
      const uint8_t *data, size_t len)
{
#if defined(CRC32_HAVE_VPCLMUL_PATH)
   if (len >= 256 && crc32_have_vpclmul())
      return crc_msb_vpclmul(m, crc, data, len);
#endif
   if (     len >= 64
         && (cpu_features_get() & (RETRO_SIMD_PCLMUL | RETRO_SIMD_SSSE3))
            == (RETRO_SIMD_PCLMUL | RETRO_SIMD_SSSE3))
      return crc_msb_pclmul(m, crc, data, len);
   return m->table(crc, data, len);
}
#undef CRC_MSB_LOAD
#undef CRC_MSB_REV
#undef CRC_MSB_K
#define CRC32_HAVE_MSB_PATH 1

#endif

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

/* Three chains of CRC32X, each a lane of 1 KiB, keep the instruction's
 * latency of two or three cycles filled where one chain waits on it.
 * The lanes are put back together as one moved on past a lane's zeros
 * is a linear map of the register, a nibble at a time:
 *    crc = on(on(c0) ^ c1) ^ c2
 * crc32_arm_on[j][v] is v << 4j moved on 1024 zero octets. */
#define CRC32_ARM_LANE 1024

static const uint32_t crc32_arm_on[8][16] = {
   {
      0x00000000U, 0xF891F16FU, 0x2A52E49FU, 0xD2C315F0U, 0x54A5C93EU,
      0xAC343851U, 0x7EF72DA1U, 0x8666DCCEU, 0xA94B927CU, 0x51DA6313U,
      0x831976E3U, 0x7B88878CU, 0xFDEE5B42U, 0x057FAA2DU, 0xD7BCBFDDU,
      0x2F2D4EB2U
   },
   {
      0x00000000U, 0x89E622B9U, 0xC8BD4333U, 0x415B618AU, 0x4A0B8027U,
      0xC3EDA29EU, 0x82B6C314U, 0x0B50E1ADU, 0x9417004EU, 0x1DF122F7U,
      0x5CAA437DU, 0xD54C61C4U, 0xDE1C8069U, 0x57FAA2D0U, 0x16A1C35AU,
      0x9F47E1E3U
   },
   {
      0x00000000U, 0xF35F06DDU, 0x3DCF0BFBU, 0xCE900D26U, 0x7B9E17F6U,
      0x88C1112BU, 0x46511C0DU, 0xB50E1AD0U, 0xF73C2FECU, 0x04632931U,
      0xCAF32417U, 0x39AC22CAU, 0x8CA2381AU, 0x7FFD3EC7U, 0xB16D33E1U,
      0x4232353CU
   },
   {
      0x00000000U, 0x35095999U, 0x6A12B332U, 0x5F1BEAABU, 0xD4256664U,
      0xE12C3FFDU, 0xBE37D556U, 0x8B3E8CCFU, 0x733BCA89U, 0x46329310U,
      0x192979BBU, 0x2C202022U, 0xA71EACEDU, 0x9217F574U, 0xCD0C1FDFU,
      0xF8054646U
   },
   {
      0x00000000U, 0xE6779512U, 0x179E2C65U, 0xF1E9B977U, 0x2F3C58CAU,
      0xC94BCDD8U, 0x38A274AFU, 0xDED5E1BDU, 0x5E78B194U, 0xB80F2486U,
      0x49E69DF1U, 0xAF9108E3U, 0x7144E95EU, 0x97337C4CU, 0x66DAC53BU,
      0x80AD5029U
   },
   {
      0x00000000U, 0xBCF16328U, 0xA293C011U, 0x1E62A339U, 0x9E568663U,
      0x22A7E54BU, 0x3CC54672U, 0x8034255AU, 0xE7DC0A87U, 0x5B2D69AFU,
      0x454FCA96U, 0xF9BEA9BEU, 0x798A8CE4U, 0xC57BEFCCU, 0xDB194CF5U,
      0x67E82FDDU
   },
   {
      0x00000000U, 0x14C9134FU, 0x2992269EU, 0x3D5B35D1U, 0x53244D3CU,
      0x47ED5E73U, 0x7AB66BA2U, 0x6E7F78EDU, 0xA6489A78U, 0xB2818937U,
      0x8FDABCE6U, 0x9B13AFA9U, 0xF56CD744U, 0xE1A5C40BU, 0xDCFEF1DAU,
      0xC837E295U
   },
   {
      0x00000000U, 0x97E032B1U, 0xF4B16323U, 0x63515192U, 0x3213C007U,
      0xA5F3F2B6U, 0xC6A2A324U, 0x51429195U, 0x6427800EU, 0xF3C7B2BFU,
      0x9096E32DU, 0x0776D19CU, 0x56344009U, 0xC1D472B8U, 0xA285232AU,
      0x3565119BU
   }
};

static INLINE uint32_t crc32_arm_lane_on(uint32_t c)
{
   return crc32_arm_on[0][ c        & 15] ^ crc32_arm_on[1][(c >>  4) & 15]
        ^ crc32_arm_on[2][(c >>  8) & 15] ^ crc32_arm_on[3][(c >> 12) & 15]
        ^ crc32_arm_on[4][(c >> 16) & 15] ^ crc32_arm_on[5][(c >> 20) & 15]
        ^ crc32_arm_on[6][(c >> 24) & 15] ^ crc32_arm_on[7][ c >> 28      ];
}

CRC32_TARGET_ARM
static uint32_t crc32_arm(uint32_t crc, const uint8_t *data, size_t len)
{
   while (((uintptr_t)data & 7) && len > 0)
   {
      crc = __crc32b(crc, *data);
      data++;
      len--;
   }
   while (len >= 3 * CRC32_ARM_LANE)
   {
      uint32_t c1 = 0, c2 = 0;
      size_t   i;
      for (i = 0; i < CRC32_ARM_LANE; i += 8)
      {
         uint64_t v0, v1, v2;
         memcpy(&v0, data + i, sizeof(v0));
         memcpy(&v1, data + CRC32_ARM_LANE + i, sizeof(v1));
         memcpy(&v2, data + 2 * CRC32_ARM_LANE + i, sizeof(v2));
         crc = __crc32d(crc, v0);
         c1  = __crc32d(c1, v1);
         c2  = __crc32d(c2, v2);
      }
      crc   = crc32_arm_lane_on(crc32_arm_lane_on(crc) ^ c1) ^ c2;
      data += 3 * CRC32_ARM_LANE;
      len  -= 3 * CRC32_ARM_LANE;
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

static uint32_t crc16_ccitt_table(uint32_t seed, const uint8_t *data,
      size_t len)
{
   uint16_t crc = (uint16_t)seed;
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

static uint32_t crc32_ogg_table(uint32_t crc, const uint8_t *data,
      size_t len)
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

uint16_t encoding_crc16_ccitt(uint16_t crc, const uint8_t *data, size_t len)
{
#if defined(CRC32_HAVE_MSB_PATH)
   return (uint16_t)crc_msb(&crc_msb_c16, crc, data, len);
#else
   return (uint16_t)crc16_ccitt_table(crc, data, len);
#endif
}

uint32_t encoding_crc32_ogg(uint32_t crc, const uint8_t *data, size_t len)
{
#if defined(CRC32_HAVE_MSB_PATH)
   return crc_msb(&crc_msb_ogg, crc, data, len);
#else
   return crc32_ogg_table(crc, data, len);
#endif
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
#if defined(CRC32_HAVE_VPCLMUL_PATH)
   if (len >= 256 && crc32_have_vpclmul())
      crc = crc32_vpclmul(crc, data, len);
   else
#endif
   if (len >= 64 && crc32_have_pclmul())
      crc = crc32_pclmul(crc, data, len);
   else
      crc = crc32_slice_by_8(crc, data, len);
#else
   crc = crc32_slice_by_8(crc, data, len);
#endif

   return ~crc;
}
