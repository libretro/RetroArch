/* FLAC audio decoder. Choice of public domain or MIT-0. See license statements
 * at the end of this file. rflac - v0.12.42 - 2023-11-02  David Reid -
 * mackron@gmail.com  GitHub: https://github.com/mackron/dr_libs
 *
 * rflac is RetroArch's fork of dr_flac, trimmed and extended for this
 * codebase. What it implements:
 *
 *  - Native (raw) FLAC streams as specified by RFC 9639: all bit depths
 *    from 4 to 32 bits per sample, including the 32-bit extension with
 *    its 33-bit stereo difference channels; mono through 8 channels; all
 *    subframe types (CONSTANT, VERBATIM, FIXED, LPC) and both residual
 *    coding methods (RICE and RICE2), including escaped partitions.
 *  - Frame CRC-8 and CRC-16 verification, always on.
 *  - Seeking, via the seektable when present, an integer binary search
 *    otherwise, with brute force as the last resort.
 *  - Metadata reporting through an optional callback (STREAMINFO,
 *    seektable, Vorbis comments, cuesheet, pictures, application blocks)
 *    and skipping of leading ID3v2 tags.
 *  - Reading from memory or through user callbacks, with s16 and f32
 *    output.
 *
 * What it does not implement:
 *
 *  - Ogg-encapsulated FLAC. Only native fLaC streams are accepted.
 *  - Encoding of any kind.
 *  - Negative LPC coefficient shifts (never emitted by known encoders;
 *    such streams are rejected).
 */

#include <retro_inline.h>
#include <retro_endianness.h>
#include <formats/rflac.h>
#include "rflac_internal.h"

static uint64_t rflac_read_pcm_frames_s16(rflac* pFlac, uint64_t framesToRead, int16_t* pBufferOut);
static uint64_t rflac_read_pcm_frames_f32(rflac* pFlac, uint64_t framesToRead, float* pBufferOut);
#include <features/features_cpu.h>

/* Disable some annoying warnings. */
#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
   #pragma GCC diagnostic push
   #if __GNUC__ >= 7
   #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
   #endif
#endif

#ifdef __linux__
   #ifndef _BSD_SOURCE
      #define _BSD_SOURCE
   #endif
   #ifndef _DEFAULT_SOURCE
      #define _DEFAULT_SOURCE
   #endif
   #ifndef __USE_BSD
      #define __USE_BSD
   #endif
   #include <endian.h>
#endif

#include <stdlib.h>
#include <string.h>

/* Intrinsics Support  There's a bug in GCC 4.2.x which results in an incorrect
 * compilation error when using _mm_slli_epi32() where it complains with
 * "error: shift must be an immediate"  Unfortuantely rflac depends on this for
 * a few things so we're just going to disable SSE on GCC 4.2 and below.
 */
#if !defined(RFLAC_NO_SIMD)
   #if defined(RFLAC_X64) || defined(RFLAC_X86)
      #if defined(_MSC_VER) && !defined(__clang__)
         /* MSVC. */
         #if _MSC_VER >= 1400 && !defined(RFLAC_NO_SSE2)    /* 2005 */
            #define RFLAC_SUPPORT_SSE2
         #endif
      #elif defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)))
         /* Assume GNUC-style. */
         #if defined(__SSE2__) && !defined(RFLAC_NO_SSE2)
            #define RFLAC_SUPPORT_SSE2
         #endif
      #endif

      /* If at this point we still haven't determined compiler support for the
       * intrinsics just fall back to __has_include. */
      #if !defined(__GNUC__) && !defined(__clang__) && defined(__has_include)
         #if !defined(RFLAC_SUPPORT_SSE2) && !defined(RFLAC_NO_SSE2) && __has_include(<emmintrin.h>)
            #define RFLAC_SUPPORT_SSE2
         #endif
      #endif

      #if defined(RFLAC_SUPPORT_SSE2)
         #include <emmintrin.h>
      #endif
   #endif

   #if defined(RFLAC_ARM)
      #if !defined(RFLAC_NO_NEON) && (defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64))
         #define RFLAC_SUPPORT_NEON
         #include <arm_neon.h>
      #endif
   #endif
#endif

/* Compile-time CPU feature support. */
/* MSVC and clang-cl need <intrin.h> for the __lzcnt intrinsics used by the
 * LZCNT clz fast path (rflac__clz_lzcnt) and for _BitScanReverse. Runtime
 * feature detection itself is handled through libretro-common's shared
 * features_cpu interface (cpu_features_get) rather than an in-tree cpuid. */
#if defined(_MSC_VER) && (defined(RFLAC_X86) || defined(RFLAC_X64))
   #include <intrin.h>
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1500 && (defined(RFLAC_X86) || defined(RFLAC_X64)) && !defined(__clang__)
   #define RFLAC_HAS_LZCNT_INTRINSIC
#elif (defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)))
   #define RFLAC_HAS_LZCNT_INTRINSIC
#elif defined(__clang__)
   #if defined(__has_builtin)
      #if __has_builtin(__builtin_clzll) || __has_builtin(__builtin_clzl)
         #define RFLAC_HAS_LZCNT_INTRINSIC
      #endif
   #endif
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1400 && !defined(__clang__)
   #define RFLAC_HAS_BYTESWAP16_INTRINSIC
   #define RFLAC_HAS_BYTESWAP32_INTRINSIC
   #define RFLAC_HAS_BYTESWAP64_INTRINSIC
#elif defined(__clang__)
   #if defined(__has_builtin)
      #if __has_builtin(__builtin_bswap16)
         #define RFLAC_HAS_BYTESWAP16_INTRINSIC
      #endif
      #if __has_builtin(__builtin_bswap32)
         #define RFLAC_HAS_BYTESWAP32_INTRINSIC
      #endif
      #if __has_builtin(__builtin_bswap64)
         #define RFLAC_HAS_BYTESWAP64_INTRINSIC
      #endif
   #endif
#elif defined(__GNUC__)
   #if ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
      #define RFLAC_HAS_BYTESWAP32_INTRINSIC
      #define RFLAC_HAS_BYTESWAP64_INTRINSIC
   #endif
   #if ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
      #define RFLAC_HAS_BYTESWAP16_INTRINSIC
   #endif
#elif defined(__WATCOMC__) && defined(__386__)
   #define RFLAC_HAS_BYTESWAP16_INTRINSIC
   #define RFLAC_HAS_BYTESWAP32_INTRINSIC
   #define RFLAC_HAS_BYTESWAP64_INTRINSIC
   extern __inline uint16_t _watcom_bswap16(uint16_t);
   extern __inline uint32_t _watcom_bswap32(uint32_t);
   extern __inline uint64_t _watcom_bswap64(uint64_t);
#pragma aux _watcom_bswap16 = \
   "xchg al, ah" \
   parm  [ax]    \
   value [ax]    \
   modify nomemory;
#pragma aux _watcom_bswap32 = \
   "bswap eax" \
   parm  [eax] \
   value [eax] \
   modify nomemory;
#pragma aux _watcom_bswap64 = \
   "bswap eax"     \
   "bswap edx"     \
   "xchg eax,edx"  \
   parm [eax edx]  \
   value [eax edx] \
   modify nomemory;
#endif

/* 64 for AVX-512 in the future. */
#define RFLAC_MAX_SIMD_VECTOR_SIZE                     64

/* Result Codes */
#define RFLAC_SUCCESS                                   0
/* A generic error. */
#define RFLAC_ERROR                                    -1
#define RFLAC_AT_END                                   -53

#define RFLAC_CRC_MISMATCH                             -100
/* End Result Codes */


#define RFLAC_SUBFRAME_CONSTANT                        0
#define RFLAC_SUBFRAME_VERBATIM                        1
#define RFLAC_SUBFRAME_FIXED                           8
#define RFLAC_SUBFRAME_LPC                             32
#define RFLAC_SUBFRAME_RESERVED                        255

#define RFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE  0
#define RFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2 1

#define RFLAC_CHANNEL_ASSIGNMENT_INDEPENDENT           0
#define RFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE             8
#define RFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE            9
#define RFLAC_CHANNEL_ASSIGNMENT_MID_SIDE              10

#define RFLAC_SEEKPOINT_SIZE_IN_BYTES                  18
#define RFLAC_CUESHEET_TRACK_SIZE_IN_BYTES             36
#define RFLAC_CUESHEET_TRACK_INDEX_SIZE_IN_BYTES       12

#define RFLAC_ALIGN(x, a)                              ((((x) + (a) - 1) / (a)) * (a))

/* CPU caps. */
#if defined(__has_feature)
   #if __has_feature(thread_sanitizer)
      #define RFLAC_NO_THREAD_SANITIZE __attribute__((no_sanitize("thread")))
   #else
      #define RFLAC_NO_THREAD_SANITIZE
   #endif
#else
   #define RFLAC_NO_THREAD_SANITIZE
#endif

#if defined(RFLAC_HAS_LZCNT_INTRINSIC)
static uint32_t rflac__gIsLZCNTSupported = 0;
#endif

#if defined(RFLAC_X86) || defined(RFLAC_X64)
static uint32_t rflac__gIsSSE2Supported  = 0;

static INLINE uint32_t rflac_has_sse2(void)
{
#if defined(RFLAC_SUPPORT_SSE2)
   #if (defined(RFLAC_X64) || defined(RFLAC_X86)) && !defined(RFLAC_NO_SSE2)
      #if defined(RFLAC_X64)
         return 1;    /* 64-bit targets always support SSE2. */
      #elif (defined(_M_IX86_FP) && _M_IX86_FP == 2) || defined(__SSE2__)
         /* If the compiler is allowed to freely generate SSE2 code we can
          * assume support. */
         return 1;
      #else
         return (cpu_features_get() & RETRO_SIMD_SSE2) != 0;
      #endif
   #else
      return 0;       /* SSE2 is only supported on x86 and x64 architectures. */
   #endif
#else
   return 0;           /* No compiler support. */
#endif
}



/* I've had a bug report that Clang's ThreadSanitizer presents a warning in this
 * function. Having reviewed this, this does actually make sense. However, since
 * CPU caps should never differ for a running process, I don't think the trade
 * off of complicating internal API's by passing around CPU caps versus just
 * disabling the warnings is worthwhile. I'm therefore just going to disable
 * these warnings. This is disabled via the RFLAC_NO_THREAD_SANITIZE attribute.
 */
RFLAC_NO_THREAD_SANITIZE static void rflac__init_cpu_caps(void)
{
   static uint32_t isCPUCapsInitialized = 0;

   if (!isCPUCapsInitialized) {
      /* LZCNT (ABM) via the shared features_cpu capability mask.  On
       * platforms where it is not reported (e.g. macOS x86, which detects
       * features via sysctl rather than cpuid) this stays unset and
       * rflac__clz uses the software fallback. */
#if defined(RFLAC_HAS_LZCNT_INTRINSIC)
      rflac__gIsLZCNTSupported =
          (cpu_features_get() & RETRO_SIMD_LZCNT) != 0;
#endif

      /* SSE2 */
      rflac__gIsSSE2Supported = rflac_has_sse2();

      /* Initialized. */
      isCPUCapsInitialized = 1;
   }
}
#else
static uint32_t rflac__gIsNEONSupported  = 0;

static INLINE uint32_t rflac__has_neon(void)
{
#if defined(RFLAC_SUPPORT_NEON)
   #if defined(RFLAC_ARM) && !defined(RFLAC_NO_NEON)
      #if (defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64))
         /* If the compiler is allowed to freely generate NEON code we can
          * assume support. */
         return 1;
      #else
         /* TODO: Runtime check. */
         return 0;
      #endif
   #else
      return 0;       /* NEON is only supported on ARM architectures. */
   #endif
#else
   return 0;           /* No compiler support. */
#endif
}

RFLAC_NO_THREAD_SANITIZE static void rflac__init_cpu_caps(void)
{
   rflac__gIsNEONSupported = rflac__has_neon();
#if defined(RFLAC_HAS_LZCNT_INTRINSIC) && defined(RFLAC_ARM) && (defined(__ARM_ARCH) && __ARM_ARCH >= 5)
   rflac__gIsLZCNTSupported = 1;
#endif
}
#endif

static INLINE uint16_t rflac__swap_endian_uint16(uint16_t n)
{
#ifdef RFLAC_HAS_BYTESWAP16_INTRINSIC
#if defined(_MSC_VER) && !defined(__clang__)
   return _byteswap_ushort(n);
#elif defined(__GNUC__) || defined(__clang__)
   return __builtin_bswap16(n);
#elif defined(__WATCOMC__) && defined(__386__)
   return _watcom_bswap16(n);
#else
#error "This compiler does not support the byte swap intrinsic."
#endif
#else
   return (   (n & 0xFF00) >> 8)
      | ((n & 0x00FF) << 8);
#endif
}

static INLINE uint32_t rflac__swap_endian_uint32(uint32_t n)
{
#ifdef RFLAC_HAS_BYTESWAP32_INTRINSIC
   #if defined(_MSC_VER) && !defined(__clang__)
      return _byteswap_ulong(n);
   #elif defined(__GNUC__) || defined(__clang__)
      /* 64-bit inline assembly has not been tested, so disabling for now. */
      #if defined(RFLAC_ARM) && (defined(__ARM_ARCH) && __ARM_ARCH >= 6) && !defined(__ARM_ARCH_6M__) && !defined(RFLAC_64BIT)
         /* Inline assembly optimized implementation for ARM. In my testing, GCC
          * does not generate optimized code with __builtin_bswap32(). */
         uint32_t r;
         __asm__ __volatile__ (
         #if defined(RFLAC_64BIT)
            /* This is untested. If someone in the community could test this,
             * that would be appreciated! */
            "rev %w[out], %w[in]" : [out]"=r"(r) : [in]"r"(n)
         #else
            "rev %[out], %[in]" : [out]"=r"(r) : [in]"r"(n)
         #endif
         );
         return r;
      #else
         return __builtin_bswap32(n);
      #endif
   #elif defined(__WATCOMC__) && defined(__386__)
      return _watcom_bswap32(n);
   #else
      #error "This compiler does not support the byte swap intrinsic."
   #endif
#else
   return ((n & 0xFF000000) >> 24) |
        ((n & 0x00FF0000) >>  8) |
        ((n & 0x0000FF00) <<  8) |
        ((n & 0x000000FF) << 24);
#endif
}

static INLINE uint64_t rflac__swap_endian_uint64(uint64_t n)
{
#ifdef RFLAC_HAS_BYTESWAP64_INTRINSIC
   #if defined(_MSC_VER) && !defined(__clang__)
      return _byteswap_uint64(n);
   #elif defined(__GNUC__) || defined(__clang__)
      return __builtin_bswap64(n);
   #elif defined(__WATCOMC__) && defined(__386__)
      return _watcom_bswap64(n);
   #else
      #error "This compiler does not support the byte swap intrinsic."
   #endif
#else
   /* Weird "<< 32" bitshift is required for C89 because it doesn't support
    * 64-bit constants. Should be optimized out by a good compiler. */
   return ((n & ((uint64_t)0xFF000000 << 32)) >> 56) |
        ((n & ((uint64_t)0x00FF0000 << 32)) >> 40) |
        ((n & ((uint64_t)0x0000FF00 << 32)) >> 24) |
        ((n & ((uint64_t)0x000000FF << 32)) >>  8) |
        ((n & ((uint64_t)0xFF000000      )) <<  8) |
        ((n & ((uint64_t)0x00FF0000      )) << 24) |
        ((n & ((uint64_t)0x0000FF00      )) << 40) |
        ((n & ((uint64_t)0x000000FF      )) << 56);
#endif
}

static INLINE uint32_t rflac__be2host_32_ptr_unaligned(const void* pData)
{
   const uint8_t* pNum = (uint8_t*)pData;
   return *(pNum) << 24 | *(pNum+1) << 16 | *(pNum+2) << 8 | *(pNum+3);
}

static INLINE uint32_t rflac__le2host_32_ptr_unaligned(const void* pData)
{
   const uint8_t* pNum = (uint8_t*)pData;
   return *pNum | *(pNum+1) << 8 |  *(pNum+2) << 16 | *(pNum+3) << 24;
}


static INLINE uint32_t rflac__unsynchsafe_32(uint32_t n)
{
   uint32_t result = 0;
   result |= (n & 0x7F000000) >> 3;
   result |= (n & 0x007F0000) >> 2;
   result |= (n & 0x00007F00) >> 1;
   result |= (n & 0x0000007F) >> 0;

   return result;
}

/* The CRC code below is based on this document: http://zlib.net/crc_v3.txt */
static uint8_t rflac__crc8_table[] = {
   0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
   0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
   0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
   0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
   0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
   0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
   0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
   0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
   0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
   0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
   0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
   0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
   0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
   0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
   0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
   0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

static uint16_t rflac__crc16_table[] = {
   0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
   0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
   0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
   0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
   0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
   0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
   0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
   0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
   0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
   0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
   0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
   0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
   0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
   0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
   0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
   0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
   0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
   0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
   0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
   0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
   0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
   0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
   0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
   0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
   0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
   0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
   0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
   0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
   0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
   0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
   0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
   0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
};

static INLINE uint8_t rflac_crc8_byte(uint8_t crc, uint8_t data)
{
   return rflac__crc8_table[crc ^ data];
}

static INLINE uint8_t rflac_crc8(uint8_t crc, uint32_t data, uint32_t count)
{
   uint32_t wholeBytes;
   uint32_t leftoverBits;
   uint64_t leftoverDataMask;

   static uint64_t leftoverDataMaskTable[8] = {
      0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F
   };

   wholeBytes = count >> 3;
   leftoverBits = count - (wholeBytes*8);
   leftoverDataMask = leftoverDataMaskTable[leftoverBits];

   switch (wholeBytes) {
      case 4: crc = rflac_crc8_byte(crc, (uint8_t)((data & (0xFF000000UL << leftoverBits)) >> (24 + leftoverBits)));
      case 3: crc = rflac_crc8_byte(crc, (uint8_t)((data & (0x00FF0000UL << leftoverBits)) >> (16 + leftoverBits)));
      case 2: crc = rflac_crc8_byte(crc, (uint8_t)((data & (0x0000FF00UL << leftoverBits)) >> ( 8 + leftoverBits)));
      case 1: crc = rflac_crc8_byte(crc, (uint8_t)((data & (0x000000FFUL << leftoverBits)) >> ( 0 + leftoverBits)));
      case 0: if (leftoverBits > 0) crc = (uint8_t)((crc << leftoverBits) ^ rflac__crc8_table[(crc >> (8 - leftoverBits)) ^ (data & leftoverDataMask)]);
   }
   return crc;
}

static INLINE uint16_t rflac_crc16_byte(uint16_t crc, uint8_t data)
{
   return (crc << 8) ^ rflac__crc16_table[(uint8_t)(crc >> 8) ^ data];
}

/* Slice tables for whole-cache-line CRC-16 accumulation.
 * rflac__crc16_slices[k][v] is the CRC-16 of byte v followed by k zero
 * bytes, so a full line can be folded with one independent table lookup
 * per byte instead of a serial byte-by-byte dependency chain.
 * Generated once at open time from the canonical byte table; the
 * initialization is idempotent, so the benign race on first concurrent
 * use is harmless (same reasoning as the CPU caps above). */
static uint16_t rflac__crc16_slices[8][256];
static uint32_t rflac__crc16_slices_initialized = 0;

RFLAC_NO_THREAD_SANITIZE static void rflac__crc16_init_slices(void)
{
   int v, k;
   if (rflac__crc16_slices_initialized)
      return;
   for (v = 0; v < 256; v++)
   {
      uint16_t crc = rflac__crc16_table[v];
      rflac__crc16_slices[0][v] = crc;
      for (k = 1; k < 8; k++)
      {
         crc = (uint16_t)((crc << 8) ^ rflac__crc16_table[(uint8_t)(crc >> 8)]);
         rflac__crc16_slices[k][v] = crc;
      }
   }
   rflac__crc16_slices_initialized = 1;
}

static INLINE uint16_t rflac_crc16_cache(uint16_t crc, size_t data)
{
   /* The 16-bit running CRC folds into the first two message bytes;
    * every byte then takes one independent lookup. */
#ifdef RFLAC_64BIT
   return (uint16_t)(rflac__crc16_slices[7][(uint8_t)((data >> 56) ^ (crc >> 8))]
                   ^ rflac__crc16_slices[6][(uint8_t)((data >> 48) ^  crc      )]
                   ^ rflac__crc16_slices[5][(uint8_t) (data >> 40)]
                   ^ rflac__crc16_slices[4][(uint8_t) (data >> 32)]
                   ^ rflac__crc16_slices[3][(uint8_t) (data >> 24)]
                   ^ rflac__crc16_slices[2][(uint8_t) (data >> 16)]
                   ^ rflac__crc16_slices[1][(uint8_t) (data >>  8)]
                   ^ rflac__crc16_slices[0][(uint8_t) (data      )]);
#else
   return (uint16_t)(rflac__crc16_slices[3][(uint8_t)((data >> 24) ^ (crc >> 8))]
                   ^ rflac__crc16_slices[2][(uint8_t)((data >> 16) ^  crc      )]
                   ^ rflac__crc16_slices[1][(uint8_t) (data >>  8)]
                   ^ rflac__crc16_slices[0][(uint8_t) (data      )]);
#endif
}

static INLINE uint16_t rflac_crc16_bytes(uint16_t crc, size_t data,
      uint32_t byteCount)
{
   switch (byteCount)
   {
#ifdef RFLAC_64BIT
   case 8: crc = rflac_crc16_byte(crc, (uint8_t)((data >> 56) & 0xFF));
   case 7: crc = rflac_crc16_byte(crc, (uint8_t)((data >> 48) & 0xFF));
   case 6: crc = rflac_crc16_byte(crc, (uint8_t)((data >> 40) & 0xFF));
   case 5: crc = rflac_crc16_byte(crc, (uint8_t)((data >> 32) & 0xFF));
#endif
   case 4: crc = rflac_crc16_byte(crc, (uint8_t)((data >> 24) & 0xFF));
   case 3: crc = rflac_crc16_byte(crc, (uint8_t)((data >> 16) & 0xFF));
   case 2: crc = rflac_crc16_byte(crc, (uint8_t)((data >>  8) & 0xFF));
   case 1: crc = rflac_crc16_byte(crc, (uint8_t)((data >>  0) & 0xFF));
   }

   return crc;
}

#if defined(MSB_FIRST)
#define rflac__be2host__cache_line(x) (x)
#elif defined(RFLAC_64BIT)
#define rflac__be2host__cache_line rflac__swap_endian_uint64
#else
#define rflac__be2host__cache_line rflac__swap_endian_uint32
#endif

/* BIT READING ATTEMPT #2  This uses a 32- or 64-bit bit-shifted cache - as bits
 * are read, the cache is shifted such that the first valid bit is sitting on
 * the most significant bit. It uses the notion of an L1 and L2 cache (borrowed
 * from CPU architecture), where the L1 cache is a 32- or 64-bit unsigned
 * integer (depending on whether or not a 32- or 64-bit build is being compiled)
 * and the L2 is an array of "cache lines", with each cache line being the same
 * size as the L1. The L2 is a buffer of about 4KB and is where data from
 * onRead() is read into.
 */
#define RFLAC_CACHE_L1_SIZE_BYTES(bs)                      (sizeof((bs)->cache))
#define RFLAC_CACHE_L1_SIZE_BITS(bs)                       (sizeof((bs)->cache)*8)
#define RFLAC_CACHE_L1_BITS_REMAINING(bs)                  (RFLAC_CACHE_L1_SIZE_BITS(bs) - (bs)->consumedBits)
#define RFLAC_CACHE_L1_SELECTION_MASK(_bitCount)           (~((~(size_t)0) >> (_bitCount)))
#define RFLAC_CACHE_L1_SELECTION_SHIFT(bs, _bitCount)      (RFLAC_CACHE_L1_SIZE_BITS(bs) - (_bitCount))
#define RFLAC_CACHE_L1_SELECT(bs, _bitCount)               (((bs)->cache) & RFLAC_CACHE_L1_SELECTION_MASK(_bitCount))
#define RFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, _bitCount)     (RFLAC_CACHE_L1_SELECT((bs), (_bitCount)) >>  RFLAC_CACHE_L1_SELECTION_SHIFT((bs), (_bitCount)))
#define RFLAC_CACHE_L2_SIZE_BYTES(bs)                      (sizeof((bs)->cacheL2))
#define RFLAC_CACHE_L2_LINE_COUNT(bs)                      (RFLAC_CACHE_L2_SIZE_BYTES(bs) / sizeof((bs)->cacheL2[0]))


static INLINE void rflac__update_crc16(rflac_bs* bs)
{
   if (bs->crc16CacheIgnoredBytes == 0)
      bs->crc16 = rflac_crc16_cache(bs->crc16, bs->crc16Cache);
   else {
      bs->crc16 = rflac_crc16_bytes(bs->crc16, bs->crc16Cache, RFLAC_CACHE_L1_SIZE_BYTES(bs) - bs->crc16CacheIgnoredBytes);
      bs->crc16CacheIgnoredBytes = 0;
   }
}

static INLINE uint16_t rflac__flush_crc16(rflac_bs* bs)
{
   /* The bits that were read from the L1 cache need to be accumulated. The
    * number of bytes needing to be accumulated is determined by the number of
    * bits that have been consumed.
    */
   if (RFLAC_CACHE_L1_BITS_REMAINING(bs) == 0)
      rflac__update_crc16(bs);
   else {
      /* We only accumulate the consumed bits. */
      bs->crc16 = rflac_crc16_bytes(bs->crc16, bs->crc16Cache >> RFLAC_CACHE_L1_BITS_REMAINING(bs), (bs->consumedBits >> 3) - bs->crc16CacheIgnoredBytes);

      /* The bits that we just accumulated should never be accumulated again. We
       * need to keep track of how many bytes were accumulated so we can handle
       * that later.
       */
      bs->crc16CacheIgnoredBytes = bs->consumedBits >> 3;
   }

   return bs->crc16;
}

static INLINE uint32_t rflac__reload_l1_cache_from_l2(rflac_bs* bs)
{
   size_t bytesRead;
   size_t alignedL1LineCount;

   /* Fast path. Try loading straight from L2. */
   if (bs->nextL2Line < RFLAC_CACHE_L2_LINE_COUNT(bs)) {
      bs->cache = bs->cacheL2[bs->nextL2Line++];
      return 1;
   }

   /* If we get here it means we've run out of data in the L2 cache. We'll need
    * to fetch more from the client, if there's any left.
    */
   if (bs->unalignedByteCount > 0)
      /* If we have any unaligned bytes it means there's no more aligned bytes
       * left in the client. */
      return 0;

   bytesRead = bs->onRead(bs->pUserData, bs->cacheL2, RFLAC_CACHE_L2_SIZE_BYTES(bs));

   bs->nextL2Line = 0;
   if (bytesRead == RFLAC_CACHE_L2_SIZE_BYTES(bs))
   {
      bs->cache = bs->cacheL2[bs->nextL2Line++];
      return 1;
   }


   /* If we get here it means we were unable to retrieve enough data to fill the
    * entire L2 cache. It probably means we've just reached the end of the file.
    * We need to move the valid data down to the end of the buffer and adjust
    * the index of the next line accordingly. Also keep in mind that the L2
    * cache must be aligned to the size of the L1 so we'll need to seek
    * backwards by any misaligned bytes.
    */
   alignedL1LineCount = bytesRead / RFLAC_CACHE_L1_SIZE_BYTES(bs);

   /* We need to keep track of any unaligned bytes for later use. */
   bs->unalignedByteCount = bytesRead - (alignedL1LineCount * RFLAC_CACHE_L1_SIZE_BYTES(bs));
   if (bs->unalignedByteCount > 0)
      bs->unalignedCache = bs->cacheL2[alignedL1LineCount];

   if (alignedL1LineCount > 0)
   {
      size_t offset = RFLAC_CACHE_L2_LINE_COUNT(bs) - alignedL1LineCount;
      size_t i;
      for (i = alignedL1LineCount; i > 0; --i)
         bs->cacheL2[i-1 + offset] = bs->cacheL2[i-1];

      bs->nextL2Line = (uint32_t)offset;
      bs->cache = bs->cacheL2[bs->nextL2Line++];
      return 1;
   }

   /* If we get into this branch it means we weren't able to load any L1-aligned
    * data. */
   bs->nextL2Line = RFLAC_CACHE_L2_LINE_COUNT(bs);
   return 0;
}

static uint32_t rflac__reload_cache(rflac_bs* bs)
{
   size_t bytesRead;

   rflac__update_crc16(bs);

   /* Fast path: try just moving the next value from the L2 to the L1 cache. */
   if (rflac__reload_l1_cache_from_l2(bs))
   {
      bs->cache = rflac__be2host__cache_line(bs->cache);
      bs->consumedBits = 0;
      bs->crc16Cache = bs->cache;
      return 1;
   }

   /* Slow path. */

   /* If we get here it means we have failed to load the L1 cache from the L2.
    * Likely we've just reached the end of the stream and the last few bytes did
    * not meet the alignment requirements for the L2 cache. In this case we need
    * to fall back to a slower path and read the data from the unaligned cache.
    */
   bytesRead = bs->unalignedByteCount;
   if (bytesRead == 0)
   {
      /* <-- The stream has been exhausted, so marked the bits as consumed. */
      bs->consumedBits = RFLAC_CACHE_L1_SIZE_BITS(bs);
      return 0;
   }

   bs->consumedBits = (uint32_t)(RFLAC_CACHE_L1_SIZE_BYTES(bs) - bytesRead) * 8;

   bs->cache = rflac__be2host__cache_line(bs->unalignedCache);
   /* <-- Make sure the consumed bits are always set to zero. Other parts of the
    * library depend on this property. */
   bs->cache &= RFLAC_CACHE_L1_SELECTION_MASK(RFLAC_CACHE_L1_BITS_REMAINING(bs));
   /* <-- At this point the unaligned bytes have been moved into the cache and
    * we thus have no more unaligned bytes. */
   bs->unalignedByteCount = 0;

   bs->crc16Cache = bs->cache >> bs->consumedBits;
   bs->crc16CacheIgnoredBytes = bs->consumedBits >> 3;
   return 1;
}

static void rflac__reset_cache(rflac_bs* bs)
{
   /* <-- This clears the L2 cache. */
   bs->nextL2Line   = RFLAC_CACHE_L2_LINE_COUNT(bs);
   /* <-- This clears the L1 cache. */
   bs->consumedBits = RFLAC_CACHE_L1_SIZE_BITS(bs);
   bs->cache = 0;
   /* <-- This clears the trailing unaligned bytes. */
   bs->unalignedByteCount = 0;
   bs->unalignedCache = 0;

   bs->crc16Cache = 0;
   bs->crc16CacheIgnoredBytes = 0;
}


static INLINE uint32_t rflac__read_uint32(rflac_bs* bs, unsigned int bitCount,
      uint32_t* pResultOut)
{
   if (bs->consumedBits == RFLAC_CACHE_L1_SIZE_BITS(bs))
   {
      if (!rflac__reload_cache(bs))
         return 0;
   }

   if (bitCount <= RFLAC_CACHE_L1_BITS_REMAINING(bs)) {
      /* If we want to load all 32-bits from a 32-bit cache we need to do it
       * slightly differently because we can't do a 32-bit shift on a 32-bit
       * integer. This will never be the case on 64-bit caches, so we can have a
       * slightly more optimal solution for this.
       */
#ifdef RFLAC_64BIT
      *pResultOut = (uint32_t)RFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCount);
      bs->consumedBits += bitCount;
      bs->cache <<= bitCount;
#else
      if (bitCount < RFLAC_CACHE_L1_SIZE_BITS(bs)) {
         *pResultOut = (uint32_t)RFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCount);
         bs->consumedBits += bitCount;
         bs->cache <<= bitCount;
      } else {
         /* Cannot shift by 32-bits, so need to do it differently. */
         *pResultOut = (uint32_t)bs->cache;
         bs->consumedBits = RFLAC_CACHE_L1_SIZE_BITS(bs);
         bs->cache = 0;
      }
#endif

      return 1;
   } else {
      /* It straddles the cached data. It will never cover more than the next
       * chunk. We just read the number in two parts and combine them. */
      uint32_t bitCountHi = RFLAC_CACHE_L1_BITS_REMAINING(bs);
      uint32_t bitCountLo = bitCount - bitCountHi;
      uint32_t resultHi = (uint32_t)RFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCountHi);

      if (!rflac__reload_cache(bs))
         return 0;
      if (bitCountLo > RFLAC_CACHE_L1_BITS_REMAINING(bs))
         /* This happens when we get to end of stream */
         return 0;

      *pResultOut = (resultHi << bitCountLo) | (uint32_t)RFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, bitCountLo);
      bs->consumedBits += bitCountLo;
      bs->cache <<= bitCountLo;
      return 1;
   }
}

static uint32_t rflac__read_int32(rflac_bs* bs, unsigned int bitCount,
      int32_t* pResult)
{
   uint32_t result;

   if (!rflac__read_uint32(bs, bitCount, &result))
      return 0;

   /* Do not attempt to shift by 32 as it's undefined. */
   if (bitCount < 32)
   {
      uint32_t signbit = ((result >> (bitCount-1)) & 0x01);
      result |= (~signbit + 1) << bitCount;
   }

   *pResult = (int32_t)result;
   return 1;
}

/* Composes a 64-bit value out of two 32-bit reads, so it needs no native
 * 64-bit support and must stay available on ILP32 targets: the RFC 9639
 * 32-bit sample path (rflac__read_int64w) calls it unconditionally. */
static uint32_t rflac__read_uint64(rflac_bs* bs, unsigned int bitCount,
      uint64_t* pResultOut)
{
   uint32_t resultHi;
   uint32_t resultLo;

   if (!rflac__read_uint32(bs, bitCount - 32, &resultHi))
      return 0;

   if (!rflac__read_uint32(bs, 32, &resultLo))
      return 0;

   *pResultOut = (((uint64_t)resultHi) << 32) | ((uint64_t)resultLo);
   return 1;
}

static uint32_t rflac__read_uint16(rflac_bs* bs, unsigned int bitCount,
      uint16_t* pResult)
{
   uint32_t result;

   if (!rflac__read_uint32(bs, bitCount, &result))
      return 0;

   *pResult = (uint16_t)result;
   return 1;
}

static uint32_t rflac__read_uint8(rflac_bs* bs, unsigned int bitCount,
      uint8_t* pResult)
{
   uint32_t result;

   if (!rflac__read_uint32(bs, bitCount, &result))
      return 0;

   *pResult = (uint8_t)result;
   return 1;
}

static uint32_t rflac__read_int8(rflac_bs* bs, unsigned int bitCount,
      int8_t* pResult)
{
   int32_t result;

   if (!rflac__read_int32(bs, bitCount, &result))
      return 0;

   *pResult = (int8_t)result;
   return 1;
}


static uint32_t rflac__seek_bits(rflac_bs* bs, size_t bitsToSeek)
{
  if (bitsToSeek <= RFLAC_CACHE_L1_BITS_REMAINING(bs))
  {
    bs->consumedBits += (uint32_t)bitsToSeek;
    bs->cache <<= bitsToSeek;
    return 1;
  }

  /* It straddles the cached data. This function isn't called too frequently so
   * I'm favouring simplicity here. */
  bitsToSeek       -= RFLAC_CACHE_L1_BITS_REMAINING(bs);
  bs->consumedBits += RFLAC_CACHE_L1_BITS_REMAINING(bs);
  bs->cache         = 0;

  /* Simple case. Seek in groups of the same number as bits that fit within a
   * cache line. */
#ifdef RFLAC_64BIT
  while (bitsToSeek >= RFLAC_CACHE_L1_SIZE_BITS(bs)) {
    uint64_t bin;
    if (!rflac__read_uint64(bs, RFLAC_CACHE_L1_SIZE_BITS(bs), &bin))
       return 0;
    bitsToSeek -= RFLAC_CACHE_L1_SIZE_BITS(bs);
  }
#else
  while (bitsToSeek >= RFLAC_CACHE_L1_SIZE_BITS(bs)) {
    uint32_t bin;
    if (!rflac__read_uint32(bs, RFLAC_CACHE_L1_SIZE_BITS(bs), &bin))
       return 0;
    bitsToSeek -= RFLAC_CACHE_L1_SIZE_BITS(bs);
  }
#endif

  /* Whole leftover bytes. */
  while (bitsToSeek >= 8) {
    uint8_t bin;
    if (!rflac__read_uint8(bs, 8, &bin))
       return 0;
    bitsToSeek -= 8;
  }

  /* Leftover bits. */
  if (bitsToSeek > 0) {
    uint8_t bin;
    if (!rflac__read_uint8(bs, (uint32_t)bitsToSeek, &bin))
       return 0;
    bitsToSeek = 0; /* <-- Necessary for the assert below. */
  }

  return 1;
}


/* This function moves the bit streamer to the first bit after the sync code
 * (bit 15 of the of the frame header). It will also update the CRC-16. */
static uint32_t rflac__find_and_seek_to_next_sync_code(rflac_bs* bs)
{
   /* The sync code is always aligned to 8 bits. This is convenient for us
    * because it means we can do byte-aligned movements. The first thing to do
    * is align to the next byte.
    */
   if (rflac__seek_bits(bs, RFLAC_CACHE_L1_BITS_REMAINING(bs) & 7))
   {
     for (;;)
     {
        uint8_t hi;

        bs->crc16 = 0;
        bs->crc16CacheIgnoredBytes = bs->consumedBits >> 3;

        if (!rflac__read_uint8(bs, 8, &hi))
          return 0;

        if (hi == 0xFF)
        {
          uint8_t lo;
          if (!rflac__read_uint8(bs, 6, &lo))
            return 0;

          if (lo == 0x3E)
            return 1;

          if (!rflac__seek_bits(bs, RFLAC_CACHE_L1_BITS_REMAINING(bs) & 7))
            return 0;
        }
     }
   }

   return 0;
}

#if defined(RFLAC_HAS_LZCNT_INTRINSIC)
#define RFLAC_IMPLEMENT_CLZ_LZCNT
#endif
#if  defined(_MSC_VER) && _MSC_VER >= 1400 && (defined(RFLAC_X64) || defined(RFLAC_X86)) && !defined(__clang__)
#define RFLAC_IMPLEMENT_CLZ_MSVC
#endif
#if  defined(__WATCOMC__) && defined(__386__)
#define RFLAC_IMPLEMENT_CLZ_WATCOM
#endif
static INLINE uint32_t rflac__clz_software(size_t x)
{
   uint32_t n;
   static uint32_t clz_table_4[] = {
      0,
      4,
      3, 3,
      2, 2, 2, 2,
      1, 1, 1, 1, 1, 1, 1, 1
   };

   if (x == 0)
      return sizeof(x)*8;

   n = clz_table_4[x >> (sizeof(x)*8 - 4)];
   if (n == 0) {
#ifdef RFLAC_64BIT
      if ((x & ((uint64_t)0xFFFFFFFF << 32)) == 0) { n  = 32; x <<= 32; }
      if ((x & ((uint64_t)0xFFFF0000 << 32)) == 0) { n += 16; x <<= 16; }
      if ((x & ((uint64_t)0xFF000000 << 32)) == 0) { n += 8;  x <<= 8;  }
      if ((x & ((uint64_t)0xF0000000 << 32)) == 0) { n += 4;  x <<= 4;  }
#else
      if ((x & 0xFFFF0000) == 0) { n  = 16; x <<= 16; }
      if ((x & 0xFF000000) == 0) { n += 8;  x <<= 8;  }
      if ((x & 0xF0000000) == 0) { n += 4;  x <<= 4;  }
#endif
      n += clz_table_4[x >> (sizeof(x)*8 - 4)];
   }

   return n - 1;
}

#ifdef RFLAC_IMPLEMENT_CLZ_LZCNT
static INLINE uint32_t rflac__is_lzcnt_supported(void)
{
   /* Fast compile time check for ARM. */
#if defined(RFLAC_HAS_LZCNT_INTRINSIC) && defined(RFLAC_ARM) && (defined(__ARM_ARCH) && __ARM_ARCH >= 5)
   return 1;
#elif defined(__MRC__)
   return 1;
#else
   /* If the compiler itself does not support the intrinsic then we'll need to
    * return false. */
#ifdef RFLAC_HAS_LZCNT_INTRINSIC
   return rflac__gIsLZCNTSupported;
#else
   return 0;
#endif
#endif
}

static INLINE uint32_t rflac__clz_lzcnt(size_t x)
{
   /* It's critical for competitive decoding performance that this function be
    * highly optimal. With MSVC we can use the __lzcnt64() and __lzcnt()
    * intrinsics to achieve good performance, however on GCC and Clang it's a
    * little bit more annoying. The __builtin_clzl() and __builtin_clzll()
    * intrinsics leave it undefined as to the return value when `x` is 0. We
    * need this to be well defined as returning 32 or 64, depending on whether
    * or not it's a 32- or 64-bit build. To work around this we would need to
    * add a conditional to check for the x = 0 case, but this creates
    * unnecessary inefficiency. To work around this problem I have written some
    * inline assembly to emit the LZCNT (x86) or CLZ (ARM) instruction directly
    * which removes the need to include the conditional. This has worked well in
    * the past, but for some reason Clang's MSVC compatible driver, clang-cl,
    * does not seem to be handling this in the same way as the normal Clang
    * driver. It seems that `clang-cl` is just outputting the wrong results
    * sometimes, maybe due to some register getting clobbered?  I'm not sure if
    * this is a bug with rflac's inlined assembly (most likely), a bug in
    * `clang-cl` or just a misunderstanding on my part with inline assembly
    * rules for `clang-cl`. If somebody can identify an error in rflac's inlined
    * assembly I'm happy to get that fixed.  Fortunately there is an easy
    * workaround for this. Clang implements MSVC-specific intrinsics for
    * compatibility. It also defines _MSC_VER for extra compatibility. We can
    * therefore just check for _MSC_VER and use the MSVC intrinsic which,
    * fortunately for us, Clang supports. It would still be nice to know how to
    * fix the inlined assembly for correctness sake, however.
    */

/* && !defined(__clang__)*/    /* <-- Intentionally wanting Clang to use the
 * MSVC __lzcnt64/__lzcnt intrinsics due to above ^. */
#if defined(_MSC_VER)
   #ifdef RFLAC_64BIT
      return (uint32_t)__lzcnt64(x);
   #else
      return (uint32_t)__lzcnt(x);
   #endif
#else
   #if defined(__GNUC__) || defined(__clang__)
      #if defined(RFLAC_X64)
         {
            uint64_t r;
            __asm__ __volatile__ (
               "lzcnt{ %1, %0| %0, %1}" : "=r"(r) : "r"(x) : "cc"
            );

            return (uint32_t)r;
         }
      #elif defined(RFLAC_X86)
         {
            uint32_t r;
            __asm__ __volatile__ (
               "lzcnt{l %1, %0| %0, %1}" : "=r"(r) : "r"(x) : "cc"
            );

            return r;
         }
      /* I haven't tested 64-bit inline assembly, so only enabling this for the
       * 32-bit build for now. */
      #elif defined(RFLAC_ARM) && (defined(__ARM_ARCH) && __ARM_ARCH >= 5) && !defined(__ARM_ARCH_6M__) && !defined(RFLAC_64BIT)
         {
            unsigned int r;
            __asm__ __volatile__ (
            #if defined(RFLAC_64BIT)
               /* This is untested. If someone in the community could test this,
                * that would be appreciated! */
               "clz %w[out], %w[in]" : [out]"=r"(r) : [in]"r"(x)
            #else
               "clz %[out], %[in]" : [out]"=r"(r) : [in]"r"(x)
            #endif
            );

            return r;
         }
      #else
         if (x == 0)
            return sizeof(x)*8;
         #ifdef RFLAC_64BIT
            return (uint32_t)__builtin_clzll((uint64_t)x);
         #else
            return (uint32_t)__builtin_clzl((uint32_t)x);
         #endif
      #endif
   #else
      /* Unsupported compiler. */
      #error "This compiler does not support the lzcnt intrinsic."
   #endif
#endif
}
#endif

#ifdef RFLAC_IMPLEMENT_CLZ_MSVC
#include <intrin.h> /* For BitScanReverse(). */

static INLINE uint32_t rflac__clz_msvc(size_t x)
{
   uint32_t n;
   if (x == 0)
      return sizeof(x)*8;
#ifdef RFLAC_64BIT
   _BitScanReverse64((unsigned long*)&n, x);
#else
   _BitScanReverse((unsigned long*)&n, x);
#endif
   return sizeof(x)*8 - n - 1;
}
#endif

#ifdef RFLAC_IMPLEMENT_CLZ_WATCOM
static __inline uint32_t rflac__clz_watcom (uint32_t);
#ifdef RFLAC_IMPLEMENT_CLZ_WATCOM_LZCNT
/* Use the LZCNT instruction (only available on some processors since the
 * 2010s). */
#pragma aux rflac__clz_watcom_lzcnt = \
   "db 0F3h, 0Fh, 0BDh, 0C0h" /* lzcnt eax, eax */ \
   parm [eax] \
   value [eax] \
   modify nomemory;
#else
/* Use the 386+-compatible implementation. */
#pragma aux rflac__clz_watcom = \
   "bsr eax, eax" \
   "xor eax, 31" \
   parm [eax] nomemory \
   value [eax] \
   modify exact [eax] nomemory;
#endif
#endif

static INLINE uint32_t rflac__clz(size_t x)
{
#ifdef RFLAC_IMPLEMENT_CLZ_LZCNT
   if (rflac__is_lzcnt_supported())
      return rflac__clz_lzcnt(x);
   else
#endif
   {
#ifdef RFLAC_IMPLEMENT_CLZ_MSVC
      return rflac__clz_msvc(x);
#elif defined(RFLAC_IMPLEMENT_CLZ_WATCOM_LZCNT)
      return rflac__clz_watcom_lzcnt(x);
#elif defined(RFLAC_IMPLEMENT_CLZ_WATCOM)
      return (x == 0) ? sizeof(x)*8 : rflac__clz_watcom(x);
#elif defined(__MRC__)
      return __cntlzw(x);
#else
      return rflac__clz_software(x);
#endif
   }
}


static INLINE uint32_t rflac__seek_past_next_set_bit(rflac_bs* bs,
      unsigned int* pOffsetOut)
{
   uint32_t zeroCounter = 0;
   uint32_t setBitOffsetPlus1;

   while (bs->cache == 0)
   {
      zeroCounter += (uint32_t)RFLAC_CACHE_L1_BITS_REMAINING(bs);
      if (!rflac__reload_cache(bs))
         return 0;
   }

   if (bs->cache == 1)
   {
      /* Not catching this would lead to undefined behaviour: a shift of a
       * 32-bit number by 32 or more is undefined */
      *pOffsetOut = zeroCounter + (uint32_t)RFLAC_CACHE_L1_BITS_REMAINING(bs) - 1;
      if (!rflac__reload_cache(bs))
         return 0;

      return 1;
   }

   setBitOffsetPlus1 = rflac__clz(bs->cache);
   setBitOffsetPlus1 += 1;

   /* This happens when we get to end of stream */
   if (setBitOffsetPlus1 > RFLAC_CACHE_L1_BITS_REMAINING(bs))
      return 0;

   bs->consumedBits += setBitOffsetPlus1;
   bs->cache <<= setBitOffsetPlus1;

   *pOffsetOut = zeroCounter + setBitOffsetPlus1 - 1;
   return 1;
}


static int32_t rflac__read_utf8_coded_number(rflac_bs* bs, uint64_t* pNumberOut,
      uint8_t* pCRCOut)
{
   int i;
   int byteCount;
   uint64_t result;
   uint8_t utf8[7] = {0};
   uint8_t crc = *pCRCOut;

   if (!rflac__read_uint8(bs, 8, utf8)) {
      *pNumberOut = 0;
      return RFLAC_AT_END;
   }
   crc = rflac_crc8(crc, utf8[0], 8);

   if ((utf8[0] & 0x80) == 0) {
      *pNumberOut = utf8[0];
      *pCRCOut = crc;
      return RFLAC_SUCCESS;
   }

   if ((utf8[0] & 0xE0) == 0xC0)
      byteCount = 2;
   else if ((utf8[0] & 0xF0) == 0xE0)
      byteCount = 3;
   else if ((utf8[0] & 0xF8) == 0xF0)
      byteCount = 4;
   else if ((utf8[0] & 0xFC) == 0xF8)
      byteCount = 5;
   else if ((utf8[0] & 0xFE) == 0xFC)
      byteCount = 6;
   else if ((utf8[0] & 0xFF) == 0xFE)
      byteCount = 7;
   else
   {
      *pNumberOut = 0;
      return RFLAC_CRC_MISMATCH;     /* Bad UTF-8 encoding. */
   }

   result = (uint64_t)(utf8[0] & (0xFF >> (byteCount + 1)));
   for (i = 1; i < byteCount; ++i)
   {
      if (!rflac__read_uint8(bs, 8, utf8 + i))
      {
         *pNumberOut = 0;
         return RFLAC_AT_END;
      }
      crc = rflac_crc8(crc, utf8[i], 8);

      result = (result << 6) | (utf8[i] & 0x3F);
   }

   *pNumberOut = result;
   *pCRCOut = crc;
   return RFLAC_SUCCESS;
}


static INLINE uint32_t rflac__ilog2_u32(uint32_t x)
{
   uint32_t result = 0;
   while (x > 0)
   {
      result += 1;
      x >>= 1;
   }
   return result;
}

static INLINE uint32_t rflac__use_64_bit_prediction(uint32_t bitsPerSample,
      uint32_t order, uint32_t precision)
{
   /* https://web.archive.org/web/20220205005724/https://github.com/ietf-wg-
    * cellar/flac-specification/blob/37a49aa48ba4ba12e8757badfc59c0df35435fec/rf
    * c_backmatter.md */
   return bitsPerSample + precision + rflac__ilog2_u32(order) > 32;
}


/* The next two functions are responsible for calculating the prediction.  When
 * the bits per sample is >16 we need to use 64-bit integer arithmetic because
 * otherwise we'll run out of precision. It's safe to assume this will be slower
 * on 32-bit platforms so we use a more optimal solution when the bits per
 * sample is <=16.
 */
#if defined(__clang__)
__attribute__((no_sanitize("signed-integer-overflow")))
#endif
/* Force-inlined hot leaves: these carry INLINE hints upstream, but
 * their bodies exceed gcc's -O2 inlining heuristics, so each residual
 * sample paid a function call plus a bit-cache state round-trip
 * through the decoder struct.  Forcing the inline lets the compiler
 * keep the cache in registers across the unrolled residual loop. */
#if defined(__GNUC__) || defined(__clang__)
#define RFLAC_HOT_INLINE __attribute__((always_inline)) static INLINE
#elif defined(_MSC_VER)
#define RFLAC_HOT_INLINE static __forceinline
#else
#define RFLAC_HOT_INLINE static INLINE
#endif

RFLAC_HOT_INLINE int32_t rflac__calculate_prediction_32(uint32_t order,
      int32_t shift, const int32_t* coefficients, int32_t* pDecodedSamples)
{
   /* Unsigned, because this deliberately wraps.
    *
    * A valid stream cannot overflow here - the format bounds the
    * coefficient precision and the sample width so that the sum fits -
    * which is why this fast path exists at all.  A corrupt one is not
    * bounded by anything, and signed overflow is undefined rather than
    * merely wrong, so the accumulation is done unsigned, where the
    * wrap is defined, and reinterpreted at the end.  Same bits either
    * way on every two's-complement target; the difference is that this
    * one is not undefined behaviour.  The 64-bit variant below casts
    * an operand before multiplying and so never had the problem. */
   uint32_t prediction = 0;

   /* 32-bit version. */

   /* VC++ optimizes this to a single jmp. I've not yet verified this for other
    * compilers. */
   switch (order)
   {
   case 32: prediction += (uint32_t)coefficients[31] * (uint32_t)pDecodedSamples[-32];
   case 31: prediction += (uint32_t)coefficients[30] * (uint32_t)pDecodedSamples[-31];
   case 30: prediction += (uint32_t)coefficients[29] * (uint32_t)pDecodedSamples[-30];
   case 29: prediction += (uint32_t)coefficients[28] * (uint32_t)pDecodedSamples[-29];
   case 28: prediction += (uint32_t)coefficients[27] * (uint32_t)pDecodedSamples[-28];
   case 27: prediction += (uint32_t)coefficients[26] * (uint32_t)pDecodedSamples[-27];
   case 26: prediction += (uint32_t)coefficients[25] * (uint32_t)pDecodedSamples[-26];
   case 25: prediction += (uint32_t)coefficients[24] * (uint32_t)pDecodedSamples[-25];
   case 24: prediction += (uint32_t)coefficients[23] * (uint32_t)pDecodedSamples[-24];
   case 23: prediction += (uint32_t)coefficients[22] * (uint32_t)pDecodedSamples[-23];
   case 22: prediction += (uint32_t)coefficients[21] * (uint32_t)pDecodedSamples[-22];
   case 21: prediction += (uint32_t)coefficients[20] * (uint32_t)pDecodedSamples[-21];
   case 20: prediction += (uint32_t)coefficients[19] * (uint32_t)pDecodedSamples[-20];
   case 19: prediction += (uint32_t)coefficients[18] * (uint32_t)pDecodedSamples[-19];
   case 18: prediction += (uint32_t)coefficients[17] * (uint32_t)pDecodedSamples[-18];
   case 17: prediction += (uint32_t)coefficients[16] * (uint32_t)pDecodedSamples[-17];
   case 16: prediction += (uint32_t)coefficients[15] * (uint32_t)pDecodedSamples[-16];
   case 15: prediction += (uint32_t)coefficients[14] * (uint32_t)pDecodedSamples[-15];
   case 14: prediction += (uint32_t)coefficients[13] * (uint32_t)pDecodedSamples[-14];
   case 13: prediction += (uint32_t)coefficients[12] * (uint32_t)pDecodedSamples[-13];
   case 12: prediction += (uint32_t)coefficients[11] * (uint32_t)pDecodedSamples[-12];
   case 11: prediction += (uint32_t)coefficients[10] * (uint32_t)pDecodedSamples[-11];
   case 10: prediction += (uint32_t)coefficients[9] * (uint32_t)pDecodedSamples[-10];
   case  9: prediction += (uint32_t)coefficients[8] * (uint32_t)pDecodedSamples[-9];
   case  8: prediction += (uint32_t)coefficients[7] * (uint32_t)pDecodedSamples[-8];
   case  7: prediction += (uint32_t)coefficients[6] * (uint32_t)pDecodedSamples[-7];
   case  6: prediction += (uint32_t)coefficients[5] * (uint32_t)pDecodedSamples[-6];
   case  5: prediction += (uint32_t)coefficients[4] * (uint32_t)pDecodedSamples[-5];
   case  4: prediction += (uint32_t)coefficients[3] * (uint32_t)pDecodedSamples[-4];
   case  3: prediction += (uint32_t)coefficients[2] * (uint32_t)pDecodedSamples[-3];
   case  2: prediction += (uint32_t)coefficients[1] * (uint32_t)pDecodedSamples[-2];
   case  1: prediction += (uint32_t)coefficients[0] * (uint32_t)pDecodedSamples[-1];
   }

   return (int32_t)prediction >> shift;
}

RFLAC_HOT_INLINE int32_t rflac__calculate_prediction_64(uint32_t order,
      int32_t shift, const int32_t* coefficients, int32_t* pDecodedSamples)
{
   int64_t prediction;

   /* 64-bit version. */

   /* This method is faster on the 32-bit build when compiling with VC++. See
    * note below. */
#ifndef RFLAC_64BIT
   if (order == 8)
   {
      prediction  = coefficients[0] * (int64_t)pDecodedSamples[-1];
      prediction += coefficients[1] * (int64_t)pDecodedSamples[-2];
      prediction += coefficients[2] * (int64_t)pDecodedSamples[-3];
      prediction += coefficients[3] * (int64_t)pDecodedSamples[-4];
      prediction += coefficients[4] * (int64_t)pDecodedSamples[-5];
      prediction += coefficients[5] * (int64_t)pDecodedSamples[-6];
      prediction += coefficients[6] * (int64_t)pDecodedSamples[-7];
      prediction += coefficients[7] * (int64_t)pDecodedSamples[-8];
   }
   else if (order == 7)
   {
      prediction  = coefficients[0] * (int64_t)pDecodedSamples[-1];
      prediction += coefficients[1] * (int64_t)pDecodedSamples[-2];
      prediction += coefficients[2] * (int64_t)pDecodedSamples[-3];
      prediction += coefficients[3] * (int64_t)pDecodedSamples[-4];
      prediction += coefficients[4] * (int64_t)pDecodedSamples[-5];
      prediction += coefficients[5] * (int64_t)pDecodedSamples[-6];
      prediction += coefficients[6] * (int64_t)pDecodedSamples[-7];
   }
   else if (order == 3)
   {
      prediction  = coefficients[0] * (int64_t)pDecodedSamples[-1];
      prediction += coefficients[1] * (int64_t)pDecodedSamples[-2];
      prediction += coefficients[2] * (int64_t)pDecodedSamples[-3];
   }
   else if (order == 6)
   {
      prediction  = coefficients[0] * (int64_t)pDecodedSamples[-1];
      prediction += coefficients[1] * (int64_t)pDecodedSamples[-2];
      prediction += coefficients[2] * (int64_t)pDecodedSamples[-3];
      prediction += coefficients[3] * (int64_t)pDecodedSamples[-4];
      prediction += coefficients[4] * (int64_t)pDecodedSamples[-5];
      prediction += coefficients[5] * (int64_t)pDecodedSamples[-6];
   }
   else if (order == 5)
   {
      prediction  = coefficients[0] * (int64_t)pDecodedSamples[-1];
      prediction += coefficients[1] * (int64_t)pDecodedSamples[-2];
      prediction += coefficients[2] * (int64_t)pDecodedSamples[-3];
      prediction += coefficients[3] * (int64_t)pDecodedSamples[-4];
      prediction += coefficients[4] * (int64_t)pDecodedSamples[-5];
   }
   else if (order == 4)
   {
      prediction  = coefficients[0] * (int64_t)pDecodedSamples[-1];
      prediction += coefficients[1] * (int64_t)pDecodedSamples[-2];
      prediction += coefficients[2] * (int64_t)pDecodedSamples[-3];
      prediction += coefficients[3] * (int64_t)pDecodedSamples[-4];
   }
   else if (order == 12)
   {
      prediction  = coefficients[0]  * (int64_t)pDecodedSamples[-1];
      prediction += coefficients[1]  * (int64_t)pDecodedSamples[-2];
      prediction += coefficients[2]  * (int64_t)pDecodedSamples[-3];
      prediction += coefficients[3]  * (int64_t)pDecodedSamples[-4];
      prediction += coefficients[4]  * (int64_t)pDecodedSamples[-5];
      prediction += coefficients[5]  * (int64_t)pDecodedSamples[-6];
      prediction += coefficients[6]  * (int64_t)pDecodedSamples[-7];
      prediction += coefficients[7]  * (int64_t)pDecodedSamples[-8];
      prediction += coefficients[8]  * (int64_t)pDecodedSamples[-9];
      prediction += coefficients[9]  * (int64_t)pDecodedSamples[-10];
      prediction += coefficients[10] * (int64_t)pDecodedSamples[-11];
      prediction += coefficients[11] * (int64_t)pDecodedSamples[-12];
   }
   else if (order == 2)
   {
      prediction  = coefficients[0] * (int64_t)pDecodedSamples[-1];
      prediction += coefficients[1] * (int64_t)pDecodedSamples[-2];
   }
   else if (order == 1)
   {
      prediction = coefficients[0] * (int64_t)pDecodedSamples[-1];
   }
   else if (order == 10)
   {
      prediction  = coefficients[0]  * (int64_t)pDecodedSamples[-1];
      prediction += coefficients[1]  * (int64_t)pDecodedSamples[-2];
      prediction += coefficients[2]  * (int64_t)pDecodedSamples[-3];
      prediction += coefficients[3]  * (int64_t)pDecodedSamples[-4];
      prediction += coefficients[4]  * (int64_t)pDecodedSamples[-5];
      prediction += coefficients[5]  * (int64_t)pDecodedSamples[-6];
      prediction += coefficients[6]  * (int64_t)pDecodedSamples[-7];
      prediction += coefficients[7]  * (int64_t)pDecodedSamples[-8];
      prediction += coefficients[8]  * (int64_t)pDecodedSamples[-9];
      prediction += coefficients[9]  * (int64_t)pDecodedSamples[-10];
   }
   else if (order == 9)
   {
      prediction  = coefficients[0]  * (int64_t)pDecodedSamples[-1];
      prediction += coefficients[1]  * (int64_t)pDecodedSamples[-2];
      prediction += coefficients[2]  * (int64_t)pDecodedSamples[-3];
      prediction += coefficients[3]  * (int64_t)pDecodedSamples[-4];
      prediction += coefficients[4]  * (int64_t)pDecodedSamples[-5];
      prediction += coefficients[5]  * (int64_t)pDecodedSamples[-6];
      prediction += coefficients[6]  * (int64_t)pDecodedSamples[-7];
      prediction += coefficients[7]  * (int64_t)pDecodedSamples[-8];
      prediction += coefficients[8]  * (int64_t)pDecodedSamples[-9];
   }
   else if (order == 11)
   {
      prediction  = coefficients[0]  * (int64_t)pDecodedSamples[-1];
      prediction += coefficients[1]  * (int64_t)pDecodedSamples[-2];
      prediction += coefficients[2]  * (int64_t)pDecodedSamples[-3];
      prediction += coefficients[3]  * (int64_t)pDecodedSamples[-4];
      prediction += coefficients[4]  * (int64_t)pDecodedSamples[-5];
      prediction += coefficients[5]  * (int64_t)pDecodedSamples[-6];
      prediction += coefficients[6]  * (int64_t)pDecodedSamples[-7];
      prediction += coefficients[7]  * (int64_t)pDecodedSamples[-8];
      prediction += coefficients[8]  * (int64_t)pDecodedSamples[-9];
      prediction += coefficients[9]  * (int64_t)pDecodedSamples[-10];
      prediction += coefficients[10] * (int64_t)pDecodedSamples[-11];
   }
   else
   {
      int j;

      prediction = 0;
      for (j = 0; j < (int)order; ++j)
         prediction += coefficients[j] * (int64_t)pDecodedSamples[-j-1];
   }
#endif

   /* VC++ optimizes this to a single jmp instruction, but only the 64-bit
    * build. The 32-bit build generates less efficient code for some reason. The
    * ugly version above is faster so we'll just switch between the two
    * depending on the target platform.
    */
#ifdef RFLAC_64BIT
   prediction = 0;
   switch (order)
   {
   case 32: prediction += coefficients[31] * (int64_t)pDecodedSamples[-32];
   case 31: prediction += coefficients[30] * (int64_t)pDecodedSamples[-31];
   case 30: prediction += coefficients[29] * (int64_t)pDecodedSamples[-30];
   case 29: prediction += coefficients[28] * (int64_t)pDecodedSamples[-29];
   case 28: prediction += coefficients[27] * (int64_t)pDecodedSamples[-28];
   case 27: prediction += coefficients[26] * (int64_t)pDecodedSamples[-27];
   case 26: prediction += coefficients[25] * (int64_t)pDecodedSamples[-26];
   case 25: prediction += coefficients[24] * (int64_t)pDecodedSamples[-25];
   case 24: prediction += coefficients[23] * (int64_t)pDecodedSamples[-24];
   case 23: prediction += coefficients[22] * (int64_t)pDecodedSamples[-23];
   case 22: prediction += coefficients[21] * (int64_t)pDecodedSamples[-22];
   case 21: prediction += coefficients[20] * (int64_t)pDecodedSamples[-21];
   case 20: prediction += coefficients[19] * (int64_t)pDecodedSamples[-20];
   case 19: prediction += coefficients[18] * (int64_t)pDecodedSamples[-19];
   case 18: prediction += coefficients[17] * (int64_t)pDecodedSamples[-18];
   case 17: prediction += coefficients[16] * (int64_t)pDecodedSamples[-17];
   case 16: prediction += coefficients[15] * (int64_t)pDecodedSamples[-16];
   case 15: prediction += coefficients[14] * (int64_t)pDecodedSamples[-15];
   case 14: prediction += coefficients[13] * (int64_t)pDecodedSamples[-14];
   case 13: prediction += coefficients[12] * (int64_t)pDecodedSamples[-13];
   case 12: prediction += coefficients[11] * (int64_t)pDecodedSamples[-12];
   case 11: prediction += coefficients[10] * (int64_t)pDecodedSamples[-11];
   case 10: prediction += coefficients[ 9] * (int64_t)pDecodedSamples[-10];
   case  9: prediction += coefficients[ 8] * (int64_t)pDecodedSamples[- 9];
   case  8: prediction += coefficients[ 7] * (int64_t)pDecodedSamples[- 8];
   case  7: prediction += coefficients[ 6] * (int64_t)pDecodedSamples[- 7];
   case  6: prediction += coefficients[ 5] * (int64_t)pDecodedSamples[- 6];
   case  5: prediction += coefficients[ 4] * (int64_t)pDecodedSamples[- 5];
   case  4: prediction += coefficients[ 3] * (int64_t)pDecodedSamples[- 4];
   case  3: prediction += coefficients[ 2] * (int64_t)pDecodedSamples[- 3];
   case  2: prediction += coefficients[ 1] * (int64_t)pDecodedSamples[- 2];
   case  1: prediction += coefficients[ 0] * (int64_t)pDecodedSamples[- 1];
   }
#endif

   return (int32_t)(prediction >> shift);
}

RFLAC_HOT_INLINE uint32_t rflac__read_rice_parts_x1(rflac_bs* bs,
      uint8_t riceParam, uint32_t* pZeroCounterOut, uint32_t* pRiceParamPartOut)
{
   uint32_t  riceParamPlus1 = riceParam + 1;
   uint32_t  riceParamPlus1Shift = RFLAC_CACHE_L1_SELECTION_SHIFT(bs, riceParamPlus1);
   uint32_t  riceParamPlus1MaxConsumedBits = RFLAC_CACHE_L1_SIZE_BITS(bs) - riceParamPlus1;

   /* The idea here is to use local variables for the cache in an attempt to
    * encourage the compiler to store them in registers. I have no idea how this
    * will work in practice...
    */
   size_t bs_cache = bs->cache;
   uint32_t  bs_consumedBits = bs->consumedBits;

   /* The first thing to do is find the first unset bit. Most likely a bit will
    * be set in the current cache line. */
   uint32_t  lzcount = rflac__clz(bs_cache);
   if (lzcount < sizeof(bs_cache)*8) {
      pZeroCounterOut[0] = lzcount;

      /* It is most likely that the riceParam part (which comes after the zero
       * counter) is also on this cache line. When extracting this, we include
       * the set bit from the unary coded part because it simplifies cache
       * management. This bit will be handled outside of this function at a
       * higher level.
       */
   extract_rice_param_part:
      bs_cache       <<= lzcount;
      bs_consumedBits += lzcount;

      if (bs_consumedBits <= riceParamPlus1MaxConsumedBits) {
         /* Getting here means the rice parameter part is wholly contained
          * within the current cache line. */
         pRiceParamPartOut[0] = (uint32_t)(bs_cache >> riceParamPlus1Shift);
         bs_cache       <<= riceParamPlus1;
         bs_consumedBits += riceParamPlus1;
      } else {
         uint32_t riceParamPartHi;
         uint32_t riceParamPartLo;
         uint32_t riceParamPartLoBitCount;

         /* Getting here means the rice parameter part straddles the cache line.
          * We need to read from the tail of the current cache line, reload the
          * cache, and then combine it with the head of the next cache line.
          */

         /* Grab the high part of the rice parameter part. */
         riceParamPartHi = (uint32_t)(bs_cache >> riceParamPlus1Shift);

         /* Before reloading the cache we need to grab the size in bits of the
          * low part. */
         riceParamPartLoBitCount = bs_consumedBits - riceParamPlus1MaxConsumedBits;

         /* Now reload the cache. */
         if (bs->nextL2Line < RFLAC_CACHE_L2_LINE_COUNT(bs)) {
            rflac__update_crc16(bs);
            bs_cache = rflac__be2host__cache_line(bs->cacheL2[bs->nextL2Line++]);
            bs_consumedBits = riceParamPartLoBitCount;
            bs->crc16Cache = bs_cache;
         } else {
            /* Slow path. We need to fetch more data from the client. */
            if (!rflac__reload_cache(bs))
               return 0;
            if (riceParamPartLoBitCount > RFLAC_CACHE_L1_BITS_REMAINING(bs)) {
               /* This happens when we get to end of stream */
               return 0;
            }

            bs_cache = bs->cache;
            bs_consumedBits = bs->consumedBits + riceParamPartLoBitCount;
         }

         /* We should now have enough information to construct the rice
          * parameter part. */
         riceParamPartLo = (uint32_t)(bs_cache >> (RFLAC_CACHE_L1_SELECTION_SHIFT(bs, riceParamPartLoBitCount)));
         pRiceParamPartOut[0] = riceParamPartHi | riceParamPartLo;

         bs_cache <<= riceParamPartLoBitCount;
      }
   }
   else
   {
      /* Getting here means there are no bits set on the cache line. This is a
       * less optimal case because we just wasted a call to rflac__clz() and we
       * need to reload the cache.
       */
      uint32_t zeroCounter = (uint32_t)(RFLAC_CACHE_L1_SIZE_BITS(bs) - bs_consumedBits);
      for (;;)
      {
         if (bs->nextL2Line < RFLAC_CACHE_L2_LINE_COUNT(bs)) {
            rflac__update_crc16(bs);
            bs_cache = rflac__be2host__cache_line(bs->cacheL2[bs->nextL2Line++]);
            bs_consumedBits = 0;
            bs->crc16Cache = bs_cache;
         } else {
            /* Slow path. We need to fetch more data from the client. */
            if (!rflac__reload_cache(bs))
               return 0;

            bs_cache = bs->cache;
            bs_consumedBits = bs->consumedBits;
         }

         lzcount = rflac__clz(bs_cache);
         zeroCounter += lzcount;

         if (lzcount < sizeof(bs_cache)*8)
            break;
      }

      pZeroCounterOut[0] = zeroCounter;
      goto extract_rice_param_part;
   }

   /* Make sure the cache is restored at the end of it all. */
   bs->cache = bs_cache;
   bs->consumedBits = bs_consumedBits;

   return 1;
}

static INLINE uint32_t rflac__seek_rice_parts(rflac_bs* bs, uint8_t riceParam)
{
   uint32_t  riceParamPlus1 = riceParam + 1;
   uint32_t  riceParamPlus1MaxConsumedBits = RFLAC_CACHE_L1_SIZE_BITS(bs) - riceParamPlus1;

   /* The idea here is to use local variables for the cache in an attempt to
    * encourage the compiler to store them in registers. I have no idea how this
    * will work in practice...
    */
   size_t bs_cache = bs->cache;
   uint32_t  bs_consumedBits = bs->consumedBits;

   /* The first thing to do is find the first unset bit. Most likely a bit will
    * be set in the current cache line. */
   uint32_t  lzcount = rflac__clz(bs_cache);
   if (lzcount < sizeof(bs_cache)*8) {
      /* It is most likely that the riceParam part (which comes after the zero
       * counter) is also on this cache line. When extracting this, we include
       * the set bit from the unary coded part because it simplifies cache
       * management. This bit will be handled outside of this function at a
       * higher level.
       */
   extract_rice_param_part:
      bs_cache       <<= lzcount;
      bs_consumedBits += lzcount;

      if (bs_consumedBits <= riceParamPlus1MaxConsumedBits) {
         /* Getting here means the rice parameter part is wholly contained
          * within the current cache line. */
         bs_cache       <<= riceParamPlus1;
         bs_consumedBits += riceParamPlus1;
      } else {
         /* Getting here means the rice parameter part straddles the cache line.
          * We need to read from the tail of the current cache line, reload the
          * cache, and then combine it with the head of the next cache line.
          */

         /* Before reloading the cache we need to grab the size in bits of the
          * low part. */
         uint32_t riceParamPartLoBitCount = bs_consumedBits - riceParamPlus1MaxConsumedBits;

         /* Now reload the cache. */
         if (bs->nextL2Line < RFLAC_CACHE_L2_LINE_COUNT(bs)) {
            rflac__update_crc16(bs);
            bs_cache = rflac__be2host__cache_line(bs->cacheL2[bs->nextL2Line++]);
            bs_consumedBits = riceParamPartLoBitCount;
            bs->crc16Cache = bs_cache;
         } else {
            /* Slow path. We need to fetch more data from the client. */
            if (!rflac__reload_cache(bs))
               return 0;

            if (riceParamPartLoBitCount > RFLAC_CACHE_L1_BITS_REMAINING(bs)) {
               /* This happens when we get to end of stream */
               return 0;
            }

            bs_cache = bs->cache;
            bs_consumedBits = bs->consumedBits + riceParamPartLoBitCount;
         }

         bs_cache <<= riceParamPartLoBitCount;
      }
   }
   else
   {
      /* Getting here means there are no bits set on the cache line. This is a
       * less optimal case because we just wasted a call to rflac__clz() and we
       * need to reload the cache.
       */
      for (;;)
      {
         if (bs->nextL2Line < RFLAC_CACHE_L2_LINE_COUNT(bs))
         {
            rflac__update_crc16(bs);
            bs_cache = rflac__be2host__cache_line(bs->cacheL2[bs->nextL2Line++]);
            bs_consumedBits = 0;
            bs->crc16Cache = bs_cache;
         } else {
            /* Slow path. We need to fetch more data from the client. */
            if (!rflac__reload_cache(bs))
               return 0;

            bs_cache = bs->cache;
            bs_consumedBits = bs->consumedBits;
         }

         lzcount = rflac__clz(bs_cache);
         if (lzcount < sizeof(bs_cache)*8)
            break;
      }

      goto extract_rice_param_part;
   }

   /* Make sure the cache is restored at the end of it all. */
   bs->cache = bs_cache;
   bs->consumedBits = bs_consumedBits;

   return 1;
}


static uint32_t rflac__decode_samples_with_residual__rice__scalar_zeroorder(
      rflac_bs* bs, uint32_t count, uint8_t riceParam, int32_t* pSamplesOut)
{
   uint32_t t[2] = {0x00000000, 0xFFFFFFFF};
   uint32_t zeroCountPart0;
   uint32_t riceParamPart0;
   uint32_t riceParamMask;
   uint32_t i;
   riceParamMask  = (uint32_t)~((~0UL) << riceParam);

   i = 0;
   while (i < count) {
      /* Rice extraction. */
      if (!rflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart0, &riceParamPart0))
         return 0;

      /* Rice reconstruction. */
      riceParamPart0 &= riceParamMask;
      riceParamPart0 |= (zeroCountPart0 << riceParam);
      riceParamPart0  = (riceParamPart0 >> 1) ^ t[riceParamPart0 & 0x01];

      pSamplesOut[i] = riceParamPart0;

      i += 1;
   }

   return 1;
}

static uint32_t rflac__decode_samples_with_residual__rice__scalar(rflac_bs* bs,
      uint32_t bitsPerSample, uint32_t count, uint8_t riceParam,
      uint32_t lpcOrder, int32_t lpcShift, uint32_t lpcPrecision,
      const int32_t* coefficients, int32_t* pSamplesOut)
{
   uint32_t t[2] = {0x00000000, 0xFFFFFFFF};
   uint32_t zeroCountPart0 = 0;
   uint32_t zeroCountPart1 = 0;
   uint32_t zeroCountPart2 = 0;
   uint32_t zeroCountPart3 = 0;
   uint32_t riceParamPart0 = 0;
   uint32_t riceParamPart1 = 0;
   uint32_t riceParamPart2 = 0;
   uint32_t riceParamPart3 = 0;
   uint32_t riceParamMask;
   const int32_t* pSamplesOutEnd;
   uint32_t i;

   if (lpcOrder == 0)
      return rflac__decode_samples_with_residual__rice__scalar_zeroorder(bs, count, riceParam, pSamplesOut);

   riceParamMask  = (uint32_t)~((~0UL) << riceParam);
   pSamplesOutEnd = pSamplesOut + (count & ~3);

   if (rflac__use_64_bit_prediction(bitsPerSample, lpcOrder, lpcPrecision)) {
      while (pSamplesOut < pSamplesOutEnd) {
         /* Rice extraction. It's faster to do this one at a time against local
          * variables than it is to use the x4 version against an array. Not
          * sure why, but perhaps it's making more efficient use of registers?
          */
         if (!rflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart0, &riceParamPart0) ||
            !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart1, &riceParamPart1) ||
            !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart2, &riceParamPart2) ||
            !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart3, &riceParamPart3)) {
            return 0;
         }

         riceParamPart0 &= riceParamMask;
         riceParamPart1 &= riceParamMask;
         riceParamPart2 &= riceParamMask;
         riceParamPart3 &= riceParamMask;

         riceParamPart0 |= (zeroCountPart0 << riceParam);
         riceParamPart1 |= (zeroCountPart1 << riceParam);
         riceParamPart2 |= (zeroCountPart2 << riceParam);
         riceParamPart3 |= (zeroCountPart3 << riceParam);

         riceParamPart0  = (riceParamPart0 >> 1) ^ t[riceParamPart0 & 0x01];
         riceParamPart1  = (riceParamPart1 >> 1) ^ t[riceParamPart1 & 0x01];
         riceParamPart2  = (riceParamPart2 >> 1) ^ t[riceParamPart2 & 0x01];
         riceParamPart3  = (riceParamPart3 >> 1) ^ t[riceParamPart3 & 0x01];

         pSamplesOut[0] = riceParamPart0 + rflac__calculate_prediction_64(lpcOrder, lpcShift, coefficients, pSamplesOut + 0);
         pSamplesOut[1] = riceParamPart1 + rflac__calculate_prediction_64(lpcOrder, lpcShift, coefficients, pSamplesOut + 1);
         pSamplesOut[2] = riceParamPart2 + rflac__calculate_prediction_64(lpcOrder, lpcShift, coefficients, pSamplesOut + 2);
         pSamplesOut[3] = riceParamPart3 + rflac__calculate_prediction_64(lpcOrder, lpcShift, coefficients, pSamplesOut + 3);

         pSamplesOut += 4;
      }
   } else {
      while (pSamplesOut < pSamplesOutEnd) {
         if (!rflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart0, &riceParamPart0) ||
            !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart1, &riceParamPart1) ||
            !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart2, &riceParamPart2) ||
            !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart3, &riceParamPart3)) {
            return 0;
         }

         riceParamPart0 &= riceParamMask;
         riceParamPart1 &= riceParamMask;
         riceParamPart2 &= riceParamMask;
         riceParamPart3 &= riceParamMask;

         riceParamPart0 |= (zeroCountPart0 << riceParam);
         riceParamPart1 |= (zeroCountPart1 << riceParam);
         riceParamPart2 |= (zeroCountPart2 << riceParam);
         riceParamPart3 |= (zeroCountPart3 << riceParam);

         riceParamPart0  = (riceParamPart0 >> 1) ^ t[riceParamPart0 & 0x01];
         riceParamPart1  = (riceParamPart1 >> 1) ^ t[riceParamPart1 & 0x01];
         riceParamPart2  = (riceParamPart2 >> 1) ^ t[riceParamPart2 & 0x01];
         riceParamPart3  = (riceParamPart3 >> 1) ^ t[riceParamPart3 & 0x01];

         pSamplesOut[0] = riceParamPart0 + rflac__calculate_prediction_32(lpcOrder, lpcShift, coefficients, pSamplesOut + 0);
         pSamplesOut[1] = riceParamPart1 + rflac__calculate_prediction_32(lpcOrder, lpcShift, coefficients, pSamplesOut + 1);
         pSamplesOut[2] = riceParamPart2 + rflac__calculate_prediction_32(lpcOrder, lpcShift, coefficients, pSamplesOut + 2);
         pSamplesOut[3] = riceParamPart3 + rflac__calculate_prediction_32(lpcOrder, lpcShift, coefficients, pSamplesOut + 3);

         pSamplesOut += 4;
      }
   }

   i = (count & ~3);
   while (i < count) {
      /* Rice extraction. */
      if (!rflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart0, &riceParamPart0))
         return 0;

      /* Rice reconstruction. */
      riceParamPart0 &= riceParamMask;
      riceParamPart0 |= (zeroCountPart0 << riceParam);
      riceParamPart0  = (riceParamPart0 >> 1) ^ t[riceParamPart0 & 0x01];

      /* Sample reconstruction. */
      if (rflac__use_64_bit_prediction(bitsPerSample, lpcOrder, lpcPrecision))
         pSamplesOut[0] = riceParamPart0 + rflac__calculate_prediction_64(lpcOrder, lpcShift, coefficients, pSamplesOut + 0);
      else
         pSamplesOut[0] = riceParamPart0 + rflac__calculate_prediction_32(lpcOrder, lpcShift, coefficients, pSamplesOut + 0);

      i += 1;
      pSamplesOut += 1;
   }

   return 1;
}

#if defined(RFLAC_SUPPORT_SSE2)
static INLINE __m128i rflac__mm_packs_interleaved_epi32(__m128i a, __m128i b)
{
   __m128i r;

   /* Pack. */
   r = _mm_packs_epi32(a, b);

   /* a3a2 a1a0 b3b2 b1b0 -> a3a2 b3b2 a1a0 b1b0 */
   r = _mm_shuffle_epi32(r, _MM_SHUFFLE(3, 1, 2, 0));

   /* a3a2 b3b2 a1a0 b1b0 -> a3b3 a2b2 a1b1 a0b0 */
   r = _mm_shufflehi_epi16(r, _MM_SHUFFLE(3, 1, 2, 0));
   r = _mm_shufflelo_epi16(r, _MM_SHUFFLE(3, 1, 2, 0));

   return r;
}
#endif

#if defined(RFLAC_SUPPORT_NEON)
static uint32_t rflac__decode_samples_with_residual__rice__neon_32(rflac_bs* bs,
      uint32_t count, uint8_t riceParam, uint32_t order, int32_t shift,
      const int32_t* coefficients, int32_t* pSamplesOut)
{
   int i;
   uint32_t riceParamMask;
   int32_t* pDecodedSamples    = pSamplesOut;
   int32_t* pDecodedSamplesEnd = pSamplesOut + (count & ~3);
   uint32_t zeroCountParts[4];
   uint32_t riceParamParts[4];
   int32x4_t coefficients128_0;
   int32x4_t coefficients128_4;
   int32x4_t coefficients128_8;
   int32x4_t samples128_0;
   int32x4_t samples128_4;
   int32x4_t samples128_8;
   uint32x4_t riceParamMask128;
   int32x4_t riceParam128;
   int32x2_t shift64;
   uint32x4_t one128;

   const uint32_t t[2] = {0x00000000, 0xFFFFFFFF};

   riceParamMask    = (uint32_t)~((~0UL) << riceParam);
   riceParamMask128 = vdupq_n_u32(riceParamMask);
   riceParam128     = vdupq_n_s32(riceParam);
   /* Negate the shift because we'll be doing a variable shift using
    * vshlq_s32(). */
   shift64          = vdup_n_s32(-shift);
   one128           = vdupq_n_u32(1);

   /* Pre-loading the coefficients and prior samples is annoying because we need
    * to ensure we don't try reading more than what's available in the input
    * buffers. It would be conenient to use a fall-through switch to do this,
    * but this results in strict aliasing warnings with GCC. To work around this
    * I'm just doing something hacky. This feels a bit convoluted so I think
    * there's opportunity for this to be simplified.
    */
   {
      int runningOrder = order;
      int32_t tempC[4] = {0, 0, 0, 0};
      int32_t tempS[4] = {0, 0, 0, 0};

      /* 0 - 3. */
      if (runningOrder >= 4) {
         coefficients128_0 = vld1q_s32(coefficients + 0);
         samples128_0      = vld1q_s32(pSamplesOut  - 4);
         runningOrder -= 4;
      } else {
         switch (runningOrder) {
            /* fallthrough */
            case 3: tempC[2] = coefficients[2]; tempS[1] = pSamplesOut[-3];
            /* fallthrough */
            case 2: tempC[1] = coefficients[1]; tempS[2] = pSamplesOut[-2];
            /* fallthrough */
            case 1: tempC[0] = coefficients[0]; tempS[3] = pSamplesOut[-1];
         }

         coefficients128_0 = vld1q_s32(tempC);
         samples128_0      = vld1q_s32(tempS);
         runningOrder = 0;
      }

      /* 4 - 7 */
      if (runningOrder >= 4) {
         coefficients128_4 = vld1q_s32(coefficients + 4);
         samples128_4      = vld1q_s32(pSamplesOut  - 8);
         runningOrder -= 4;
      } else {
         switch (runningOrder) {
            /* fallthrough */
            case 3: tempC[2] = coefficients[6]; tempS[1] = pSamplesOut[-7];
            /* fallthrough */
            case 2: tempC[1] = coefficients[5]; tempS[2] = pSamplesOut[-6];
            /* fallthrough */
            case 1: tempC[0] = coefficients[4]; tempS[3] = pSamplesOut[-5];
         }

         coefficients128_4 = vld1q_s32(tempC);
         samples128_4      = vld1q_s32(tempS);
         runningOrder = 0;
      }

      /* 8 - 11 */
      if (runningOrder == 4) {
         coefficients128_8 = vld1q_s32(coefficients + 8);
         samples128_8      = vld1q_s32(pSamplesOut  - 12);
         runningOrder -= 4;
      } else {
         switch (runningOrder) {
            /* fallthrough */
            case 3: tempC[2] = coefficients[10]; tempS[1] = pSamplesOut[-11];
            /* fallthrough */
            case 2: tempC[1] = coefficients[ 9]; tempS[2] = pSamplesOut[-10];
            /* fallthrough */
            case 1: tempC[0] = coefficients[ 8]; tempS[3] = pSamplesOut[- 9];
         }

         coefficients128_8 = vld1q_s32(tempC);
         samples128_8      = vld1q_s32(tempS);
         runningOrder = 0;
      }

      /* Coefficients need to be shuffled for our streaming algorithm below to
       * work. Samples are already in the correct order from the loading routine
       * above. */
      coefficients128_0 = vrev64q_s32(vcombine_s32(vget_high_s32(coefficients128_0), vget_low_s32(coefficients128_0)));
      coefficients128_4 = vrev64q_s32(vcombine_s32(vget_high_s32(coefficients128_4), vget_low_s32(coefficients128_4)));
      coefficients128_8 = vrev64q_s32(vcombine_s32(vget_high_s32(coefficients128_8), vget_low_s32(coefficients128_8)));
   }

   /* For this version we are doing one sample at a time. */
   while (pDecodedSamples < pDecodedSamplesEnd) {
      int32x4_t prediction128;
      int32x2_t prediction64;
      uint32x4_t zeroCountPart128;
      uint32x4_t riceParamPart128;

      if (!rflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[0], &riceParamParts[0]) ||
         !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[1], &riceParamParts[1]) ||
         !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[2], &riceParamParts[2]) ||
         !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[3], &riceParamParts[3])) {
         return 0;
      }

      zeroCountPart128 = vld1q_u32(zeroCountParts);
      riceParamPart128 = vld1q_u32(riceParamParts);

      riceParamPart128 = vandq_u32(riceParamPart128, riceParamMask128);
      riceParamPart128 = vorrq_u32(riceParamPart128, vshlq_u32(zeroCountPart128, riceParam128));
      riceParamPart128 = veorq_u32(vshrq_n_u32(riceParamPart128, 1), vaddq_u32(veorq_u32(vandq_u32(riceParamPart128, one128), vdupq_n_u32(0xFFFFFFFF)), one128));

      if (order <= 4) {
         for (i = 0; i < 4; i += 1) {
            prediction128 = vmulq_s32(coefficients128_0, samples128_0);

            /* Horizontal add and shift. */
            /* horizontal add; the sum must end up in position 0 */
            prediction64 = vadd_s32(vget_high_s32(prediction128), vget_low_s32(prediction128));
            prediction64 = vpadd_s32(prediction64, prediction64);
            prediction64 = vshl_s32(prediction64, shift64);
            prediction64 = vadd_s32(prediction64, vget_low_s32(vreinterpretq_s32_u32(riceParamPart128)));

            samples128_0 = vextq_s32(samples128_0, vcombine_s32(prediction64, vdup_n_s32(0)), 1);
            riceParamPart128 = vextq_u32(riceParamPart128, vdupq_n_u32(0), 1);
         }
      } else if (order <= 8) {
         for (i = 0; i < 4; i += 1) {
            prediction128 =                vmulq_s32(coefficients128_4, samples128_4);
            prediction128 = vmlaq_s32(prediction128, coefficients128_0, samples128_0);

            /* Horizontal add and shift. */
            /* horizontal add; the sum must end up in position 0 */
            prediction64 = vadd_s32(vget_high_s32(prediction128), vget_low_s32(prediction128));
            prediction64 = vpadd_s32(prediction64, prediction64);
            prediction64 = vshl_s32(prediction64, shift64);
            prediction64 = vadd_s32(prediction64, vget_low_s32(vreinterpretq_s32_u32(riceParamPart128)));

            samples128_4 = vextq_s32(samples128_4, samples128_0, 1);
            samples128_0 = vextq_s32(samples128_0, vcombine_s32(prediction64, vdup_n_s32(0)), 1);
            riceParamPart128 = vextq_u32(riceParamPart128, vdupq_n_u32(0), 1);
         }
      } else {
         for (i = 0; i < 4; i += 1) {
            prediction128 =                vmulq_s32(coefficients128_8, samples128_8);
            prediction128 = vmlaq_s32(prediction128, coefficients128_4, samples128_4);
            prediction128 = vmlaq_s32(prediction128, coefficients128_0, samples128_0);

            /* Horizontal add and shift. */
            /* horizontal add; the sum must end up in position 0 */
            prediction64 = vadd_s32(vget_high_s32(prediction128), vget_low_s32(prediction128));
            prediction64 = vpadd_s32(prediction64, prediction64);
            prediction64 = vshl_s32(prediction64, shift64);
            prediction64 = vadd_s32(prediction64, vget_low_s32(vreinterpretq_s32_u32(riceParamPart128)));

            samples128_8 = vextq_s32(samples128_8, samples128_4, 1);
            samples128_4 = vextq_s32(samples128_4, samples128_0, 1);
            samples128_0 = vextq_s32(samples128_0, vcombine_s32(prediction64, vdup_n_s32(0)), 1);
            riceParamPart128 = vextq_u32(riceParamPart128, vdupq_n_u32(0), 1);
         }
      }

      /* We store samples in groups of 4. */
      vst1q_s32(pDecodedSamples, samples128_0);
      pDecodedSamples += 4;
   }

   /* Make sure we process the last few samples. */
   i = (count & ~3);
   while (i < (int)count) {
      /* Rice extraction. */
      if (!rflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[0], &riceParamParts[0]))
         return 0;

      /* Rice reconstruction. */
      riceParamParts[0] &= riceParamMask;
      riceParamParts[0] |= (zeroCountParts[0] << riceParam);
      riceParamParts[0]  = (riceParamParts[0] >> 1) ^ t[riceParamParts[0] & 0x01];

      /* Sample reconstruction. */
      pDecodedSamples[0] = riceParamParts[0] + rflac__calculate_prediction_32(order, shift, coefficients, pDecodedSamples);

      i += 1;
      pDecodedSamples += 1;
   }

   return 1;
}

static uint32_t rflac__decode_samples_with_residual__rice__neon_64(rflac_bs* bs,
      uint32_t count, uint8_t riceParam, uint32_t order, int32_t shift,
      const int32_t* coefficients, int32_t* pSamplesOut)
{
   int i;
   uint32_t riceParamMask;
   int32_t* pDecodedSamples    = pSamplesOut;
   int32_t* pDecodedSamplesEnd = pSamplesOut + (count & ~3);
   uint32_t zeroCountParts[4];
   uint32_t riceParamParts[4];
   int32x4_t coefficients128_0;
   int32x4_t coefficients128_4;
   int32x4_t coefficients128_8;
   int32x4_t samples128_0;
   int32x4_t samples128_4;
   int32x4_t samples128_8;
   uint32x4_t riceParamMask128;
   int32x4_t riceParam128;
   int64x1_t shift64;
   uint32x4_t one128;
   int64x2_t prediction128 = { 0 };
   uint32x4_t zeroCountPart128;
   uint32x4_t riceParamPart128;

   const uint32_t t[2] = {0x00000000, 0xFFFFFFFF};

   riceParamMask    = (uint32_t)~((~0UL) << riceParam);
   riceParamMask128 = vdupq_n_u32(riceParamMask);
   riceParam128     = vdupq_n_s32(riceParam);
   /* Negate the shift because we'll be doing a variable shift using
    * vshlq_s32(). */
   shift64          = vdup_n_s64(-shift);
   one128           = vdupq_n_u32(1);

   /* Pre-loading the coefficients and prior samples is annoying because we need
    * to ensure we don't try reading more than what's available in the input
    * buffers. It would be convenient to use a fall-through switch to do this,
    * but this results in strict aliasing warnings with GCC. To work around this
    * I'm just doing something hacky. This feels a bit convoluted so I think
    * there's opportunity for this to be simplified.
    */
   {
      int runningOrder = order;
      int32_t tempC[4] = {0, 0, 0, 0};
      int32_t tempS[4] = {0, 0, 0, 0};

      /* 0 - 3. */
      if (runningOrder >= 4) {
         coefficients128_0 = vld1q_s32(coefficients + 0);
         samples128_0      = vld1q_s32(pSamplesOut  - 4);
         runningOrder -= 4;
      } else {
         switch (runningOrder) {
            /* fallthrough */
            case 3: tempC[2] = coefficients[2]; tempS[1] = pSamplesOut[-3];
            /* fallthrough */
            case 2: tempC[1] = coefficients[1]; tempS[2] = pSamplesOut[-2];
            /* fallthrough */
            case 1: tempC[0] = coefficients[0]; tempS[3] = pSamplesOut[-1];
         }

         coefficients128_0 = vld1q_s32(tempC);
         samples128_0      = vld1q_s32(tempS);
         runningOrder = 0;
      }

      /* 4 - 7 */
      if (runningOrder >= 4) {
         coefficients128_4 = vld1q_s32(coefficients + 4);
         samples128_4      = vld1q_s32(pSamplesOut  - 8);
         runningOrder -= 4;
      } else {
         switch (runningOrder) {
            /* fallthrough */
            case 3: tempC[2] = coefficients[6]; tempS[1] = pSamplesOut[-7];
            /* fallthrough */
            case 2: tempC[1] = coefficients[5]; tempS[2] = pSamplesOut[-6];
            /* fallthrough */
            case 1: tempC[0] = coefficients[4]; tempS[3] = pSamplesOut[-5];
         }

         coefficients128_4 = vld1q_s32(tempC);
         samples128_4      = vld1q_s32(tempS);
         runningOrder = 0;
      }

      /* 8 - 11 */
      if (runningOrder == 4) {
         coefficients128_8 = vld1q_s32(coefficients + 8);
         samples128_8      = vld1q_s32(pSamplesOut  - 12);
         runningOrder -= 4;
      } else {
         switch (runningOrder) {
            /* fallthrough */
            case 3: tempC[2] = coefficients[10]; tempS[1] = pSamplesOut[-11];
            /* fallthrough */
            case 2: tempC[1] = coefficients[ 9]; tempS[2] = pSamplesOut[-10];
            /* fallthrough */
            case 1: tempC[0] = coefficients[ 8]; tempS[3] = pSamplesOut[- 9];
         }

         coefficients128_8 = vld1q_s32(tempC);
         samples128_8      = vld1q_s32(tempS);
         runningOrder = 0;
      }

      /* Coefficients need to be shuffled for our streaming algorithm below to
       * work. Samples are already in the correct order from the loading routine
       * above. */
      coefficients128_0 = vrev64q_s32(vcombine_s32(vget_high_s32(coefficients128_0), vget_low_s32(coefficients128_0)));
      coefficients128_4 = vrev64q_s32(vcombine_s32(vget_high_s32(coefficients128_4), vget_low_s32(coefficients128_4)));
      coefficients128_8 = vrev64q_s32(vcombine_s32(vget_high_s32(coefficients128_8), vget_low_s32(coefficients128_8)));
   }

   /* For this version we are doing one sample at a time. */
   while (pDecodedSamples < pDecodedSamplesEnd) {
      if (!rflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[0], &riceParamParts[0]) ||
         !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[1], &riceParamParts[1]) ||
         !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[2], &riceParamParts[2]) ||
         !rflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[3], &riceParamParts[3])) {
         return 0;
      }

      zeroCountPart128 = vld1q_u32(zeroCountParts);
      riceParamPart128 = vld1q_u32(riceParamParts);

      riceParamPart128 = vandq_u32(riceParamPart128, riceParamMask128);
      riceParamPart128 = vorrq_u32(riceParamPart128, vshlq_u32(zeroCountPart128, riceParam128));
      riceParamPart128 = veorq_u32(vshrq_n_u32(riceParamPart128, 1), vaddq_u32(veorq_u32(vandq_u32(riceParamPart128, one128), vdupq_n_u32(0xFFFFFFFF)), one128));

      for (i = 0; i < 4; i += 1) {
         int64x1_t prediction64;

         /* Reset to 0. */
         prediction128 = veorq_s64(prediction128, prediction128);
         switch (order)
         {
         case 12:
         case 11: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_low_s32(coefficients128_8), vget_low_s32(samples128_8)));
         case 10:
         case  9: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_high_s32(coefficients128_8), vget_high_s32(samples128_8)));
         case  8:
         case  7: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_low_s32(coefficients128_4), vget_low_s32(samples128_4)));
         case  6:
         case  5: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_high_s32(coefficients128_4), vget_high_s32(samples128_4)));
         case  4:
         case  3: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_low_s32(coefficients128_0), vget_low_s32(samples128_0)));
         case  2:
         case  1: prediction128 = vaddq_s64(prediction128, vmull_s32(vget_high_s32(coefficients128_0), vget_high_s32(samples128_0)));
         }

         /* Horizontal add and shift. */
         prediction64 = vadd_s64(vget_high_s64(prediction128), vget_low_s64(prediction128));
         prediction64 = vshl_s64(prediction64, shift64);
         prediction64 = vadd_s64(prediction64, vdup_n_s64(vgetq_lane_u32(riceParamPart128, 0)));

         /* Our value should be sitting in prediction64[0]. We need to combine
          * this with our SSE samples. */
         samples128_8 = vextq_s32(samples128_8, samples128_4, 1);
         samples128_4 = vextq_s32(samples128_4, samples128_0, 1);
         samples128_0 = vextq_s32(samples128_0, vcombine_s32(vreinterpret_s32_s64(prediction64), vdup_n_s32(0)), 1);

         /* Slide our rice parameter down so that the value in position 0
          * contains the next one to process. */
         riceParamPart128 = vextq_u32(riceParamPart128, vdupq_n_u32(0), 1);
      }

      /* We store samples in groups of 4. */
      vst1q_s32(pDecodedSamples, samples128_0);
      pDecodedSamples += 4;
   }

   /* Make sure we process the last few samples. */
   i = (count & ~3);
   while (i < (int)count) {
      /* Rice extraction. */
      if (!rflac__read_rice_parts_x1(bs, riceParam, &zeroCountParts[0], &riceParamParts[0]))
         return 0;

      /* Rice reconstruction. */
      riceParamParts[0] &= riceParamMask;
      riceParamParts[0] |= (zeroCountParts[0] << riceParam);
      riceParamParts[0]  = (riceParamParts[0] >> 1) ^ t[riceParamParts[0] & 0x01];

      /* Sample reconstruction. */
      pDecodedSamples[0] = riceParamParts[0] + rflac__calculate_prediction_64(order, shift, coefficients, pDecodedSamples);

      i += 1;
      pDecodedSamples += 1;
   }

   return 1;
}

static uint32_t rflac__decode_samples_with_residual__rice__neon(rflac_bs* bs,
      uint32_t bitsPerSample, uint32_t count, uint8_t riceParam,
      uint32_t lpcOrder, int32_t lpcShift, uint32_t lpcPrecision,
      const int32_t* coefficients, int32_t* pSamplesOut)
{
   /* In my testing the order is rarely > 12, so in this case I'm going to
    * simplify the NEON implementation by only handling order <= 12. */
   if (lpcOrder > 0 && lpcOrder <= 12) {
      if (rflac__use_64_bit_prediction(bitsPerSample, lpcOrder, lpcPrecision))
         return rflac__decode_samples_with_residual__rice__neon_64(bs, count, riceParam, lpcOrder, lpcShift, coefficients, pSamplesOut);
      else
         return rflac__decode_samples_with_residual__rice__neon_32(bs, count, riceParam, lpcOrder, lpcShift, coefficients, pSamplesOut);
   } else {
      return rflac__decode_samples_with_residual__rice__scalar(bs, bitsPerSample, count, riceParam, lpcOrder, lpcShift, lpcPrecision, coefficients, pSamplesOut);
   }
}
#endif

static uint32_t rflac__decode_samples_with_residual__rice(rflac_bs* bs,
      uint32_t bitsPerSample, uint32_t count, uint8_t riceParam,
      uint32_t lpcOrder, int32_t lpcShift, uint32_t lpcPrecision,
      const int32_t* coefficients, int32_t* pSamplesOut)
{
#if defined(RFLAC_SUPPORT_NEON)
   if (rflac__gIsNEONSupported)
      return rflac__decode_samples_with_residual__rice__neon(bs, bitsPerSample, count, riceParam, lpcOrder, lpcShift, lpcPrecision, coefficients, pSamplesOut);
   else
#endif
   {
      /* Scalar fallback. */
      return rflac__decode_samples_with_residual__rice__scalar(bs, bitsPerSample, count, riceParam, lpcOrder, lpcShift, lpcPrecision, coefficients, pSamplesOut);
   }
}

#if defined(__clang__)
__attribute__((no_sanitize("signed-integer-overflow")))
#endif
static uint32_t rflac__decode_samples_with_residual__unencoded(rflac_bs* bs,
      uint32_t bitsPerSample, uint32_t count, uint8_t unencodedBitsPerSample,
      uint32_t lpcOrder, int32_t lpcShift, uint32_t lpcPrecision,
      const int32_t* coefficients, int32_t* pSamplesOut)
{
   uint32_t i;

   for (i = 0; i < count; ++i) {
      if (unencodedBitsPerSample > 0) {
         if (!rflac__read_int32(bs, unencodedBitsPerSample, pSamplesOut + i))
            return 0;
      } else {
         pSamplesOut[i] = 0;
      }

      if (rflac__use_64_bit_prediction(bitsPerSample, lpcOrder, lpcPrecision))
         pSamplesOut[i] += rflac__calculate_prediction_64(lpcOrder, lpcShift, coefficients, pSamplesOut + i);
      else
         pSamplesOut[i] += rflac__calculate_prediction_32(lpcOrder, lpcShift, coefficients, pSamplesOut + i);
   }

   return 1;
}


/* Reads and decodes the residual for the sub-frame the decoder is currently
 * sitting on. This function should be called when the decoder is sitting at the
 * very start of the RESIDUAL block. The first <order> residuals will be
 * ignored. The <blockSize> and <order> parameters are used to determine how
 * many residual values need to be decoded.
 */
static uint32_t rflac__decode_samples_with_residual(rflac_bs* bs,
      uint32_t bitsPerSample, uint32_t blockSize, uint32_t lpcOrder,
      int32_t lpcShift, uint32_t lpcPrecision, const int32_t* coefficients,
      int32_t* pDecodedSamples)
{
   uint8_t residualMethod;
   uint8_t partitionOrder;
   uint32_t samplesInPartition;
   uint32_t partitionsRemaining;

   if (!rflac__read_uint8(bs, 2, &residualMethod))
      return 0;

   if (residualMethod != RFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE && residualMethod != RFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2)
      return 0;    /* Unknown or unsupported residual coding method. */

   /* Ignore the first <order> values. */
   pDecodedSamples += lpcOrder;

   if (!rflac__read_uint8(bs, 4, &partitionOrder))
      return 0;

   /* From the FLAC spec: The Rice partition order in a Rice-coded residual
    * section must be less than or equal to 8.
    */
   if (partitionOrder > 8)
      return 0;

   /* Validation check. */
   if ((blockSize / (1 << partitionOrder)) < lpcOrder)
      return 0;

   samplesInPartition  = (blockSize / (1 << partitionOrder)) - lpcOrder;
   partitionsRemaining = (1 << partitionOrder);
   for (;;)
   {
      uint8_t riceParam = 0;
      if (residualMethod == RFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE) {
         if (!rflac__read_uint8(bs, 4, &riceParam))
            return 0;
         if (riceParam == 15)
            riceParam = 0xFF;
      } else if (residualMethod == RFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
         if (!rflac__read_uint8(bs, 5, &riceParam))
            return 0;
         if (riceParam == 31)
            riceParam = 0xFF;
      }

      if (riceParam != 0xFF) {
         if (!rflac__decode_samples_with_residual__rice(bs, bitsPerSample, samplesInPartition, riceParam, lpcOrder, lpcShift, lpcPrecision, coefficients, pDecodedSamples))
            return 0;
      } else {
         uint8_t unencodedBitsPerSample = 0;
         if (!rflac__read_uint8(bs, 5, &unencodedBitsPerSample))
            return 0;

         if (!rflac__decode_samples_with_residual__unencoded(bs, bitsPerSample, samplesInPartition, unencodedBitsPerSample, lpcOrder, lpcShift, lpcPrecision, coefficients, pDecodedSamples))
            return 0;
      }

      pDecodedSamples += samplesInPartition;

      if (partitionsRemaining == 1)
         break;

      partitionsRemaining -= 1;

      if (partitionOrder != 0)
         samplesInPartition = blockSize / (1 << partitionOrder);
   }

   return 1;
}


static uint32_t rflac__decode_samples__constant(rflac_bs* bs,
      uint32_t blockSize, uint32_t subframeBitsPerSample,
      int32_t* pDecodedSamples)
{
   uint32_t i;

   /* Only a single sample needs to be decoded here. */
   int32_t sample;
   if (!rflac__read_int32(bs, subframeBitsPerSample, &sample))
      return 0;

   /* We don't really need to expand this, but it does simplify the process of
    * reading samples. If this becomes a performance issue (unlikely) we'll want
    * to look at a more efficient way.
    */
   for (i = 0; i < blockSize; ++i)
      pDecodedSamples[i] = sample;

   return 1;
}

static uint32_t rflac__decode_samples__verbatim(rflac_bs* bs,
      uint32_t blockSize, uint32_t subframeBitsPerSample,
      int32_t* pDecodedSamples)
{
   uint32_t i;

   for (i = 0; i < blockSize; ++i) {
      int32_t sample;
      if (!rflac__read_int32(bs, subframeBitsPerSample, &sample))
         return 0;

      pDecodedSamples[i] = sample;
   }

   return 1;
}

static uint32_t rflac__decode_samples__fixed(rflac_bs* bs, uint32_t blockSize,
      uint32_t subframeBitsPerSample, uint8_t lpcOrder,
      int32_t* pDecodedSamples)
{
   uint32_t i;

   static int32_t lpcCoefficientsTable[5][4] = {
      {0,  0, 0,  0},
      {1,  0, 0,  0},
      {2, -1, 0,  0},
      {3, -3, 1,  0},
      {4, -6, 4, -1}
   };

   /* Warm up samples and coefficients. */
   for (i = 0; i < lpcOrder; ++i) {
      int32_t sample;
      if (!rflac__read_int32(bs, subframeBitsPerSample, &sample))
         return 0;

      pDecodedSamples[i] = sample;
   }

   if (!rflac__decode_samples_with_residual(bs, subframeBitsPerSample, blockSize, lpcOrder, 0, 4, lpcCoefficientsTable[lpcOrder], pDecodedSamples))
      return 0;

   return 1;
}

static uint32_t rflac__decode_samples__lpc(rflac_bs* bs, uint32_t blockSize,
      uint32_t bitsPerSample, uint8_t lpcOrder, int32_t* pDecodedSamples)
{
   uint8_t i;
   uint8_t lpcPrecision;
   int8_t lpcShift;
   int32_t coefficients[32];

   /* Warm up samples. */
   for (i = 0; i < lpcOrder; ++i) {
      int32_t sample;
      if (!rflac__read_int32(bs, bitsPerSample, &sample))
         return 0;

      pDecodedSamples[i] = sample;
   }

   if (!rflac__read_uint8(bs, 4, &lpcPrecision))
      return 0;
   if (lpcPrecision == 15) {
      return 0;    /* Invalid. */
   }
   lpcPrecision += 1;

   if (!rflac__read_int8(bs, 5, &lpcShift))
      return 0;

   /* From the FLAC specification:  Quantized linear predictor coefficient shift
    * needed in bits (NOTE: this number is signed two's-complement)  Emphasis on
    * the "signed two's-complement". In practice there does not seem to be any
    * encoders nor decoders supporting negative shifts. For now rflac is not
    * going to support negative shifts as I don't have any reference files.
    * However, when a reference file comes through I will consider adding
    * support.
    */
   if (lpcShift < 0)
      return 0;
   memset(coefficients, 0, sizeof(coefficients));
   for (i = 0; i < lpcOrder; ++i) {
      if (!rflac__read_int32(bs, lpcPrecision, coefficients + i))
         return 0;
   }

   if (!rflac__decode_samples_with_residual(bs, bitsPerSample, blockSize, lpcOrder, lpcShift, lpcPrecision, coefficients, pDecodedSamples))
      return 0;
   return 1;
}


static uint32_t rflac__read_next_flac_frame_header(rflac_bs* bs,
      uint8_t streaminfoBitsPerSample, rflac_frame_header* header)
{
   const uint32_t sampleRateTable[12]  = {0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};
   /* -1 = reserved; code 0b111 = 32 bits since RFC 9639. */
   const uint8_t bitsPerSampleTable[8] = {0, 8, 12, (uint8_t)-1, 16, 20, 24, 32};

   /* Keep looping until we find a valid sync code. */
   for (;;)
   {
      uint8_t crc8 = 0xCE; /* 0xCE = rflac_crc8(0, 0x3FFE, 14); */
      uint8_t reserved = 0;
      uint8_t blockingStrategy = 0;
      uint8_t blockSize = 0;
      uint8_t sampleRate = 0;
      uint8_t channelAssignment = 0;
      uint8_t bitsPerSample = 0;
      uint32_t isVariableBlockSize;

      if (!rflac__find_and_seek_to_next_sync_code(bs))
         return 0;

      if (!rflac__read_uint8(bs, 1, &reserved))
         return 0;
      if (reserved == 1)
         continue;
      crc8 = rflac_crc8(crc8, reserved, 1);

      if (!rflac__read_uint8(bs, 1, &blockingStrategy))
         return 0;
      crc8 = rflac_crc8(crc8, blockingStrategy, 1);

      if (!rflac__read_uint8(bs, 4, &blockSize))
         return 0;
      if (blockSize == 0)
         continue;
      crc8 = rflac_crc8(crc8, blockSize, 4);

      if (!rflac__read_uint8(bs, 4, &sampleRate))
         return 0;
      crc8 = rflac_crc8(crc8, sampleRate, 4);

      if (!rflac__read_uint8(bs, 4, &channelAssignment))
         return 0;
      if (channelAssignment > 10)
         continue;
      crc8 = rflac_crc8(crc8, channelAssignment, 4);

      if (!rflac__read_uint8(bs, 3, &bitsPerSample))
         return 0;
      if (bitsPerSample == 3)  /* the only remaining reserved code; 7 = 32-bit since RFC 9639 */
         continue;
      crc8 = rflac_crc8(crc8, bitsPerSample, 3);


      if (!rflac__read_uint8(bs, 1, &reserved))
         return 0;
      if (reserved == 1)
         continue;
      crc8 = rflac_crc8(crc8, reserved, 1);


      isVariableBlockSize = blockingStrategy == 1;
      if (isVariableBlockSize) {
         uint64_t pcmFrameNumber;
         int32_t result = rflac__read_utf8_coded_number(bs, &pcmFrameNumber, &crc8);
         if (result != RFLAC_SUCCESS) {
            if (result == RFLAC_AT_END)
               return 0;
            else
               continue;
         }
         header->flacFrameNumber  = 0;
         header->pcmFrameNumber = pcmFrameNumber;
      } else {
         uint64_t flacFrameNumber = 0;
         int32_t result = rflac__read_utf8_coded_number(bs, &flacFrameNumber, &crc8);
         if (result != RFLAC_SUCCESS) {
            if (result == RFLAC_AT_END)
               return 0;
            else
               continue;
         }
         /* <-- Safe cast. */
         header->flacFrameNumber  = (uint32_t)flacFrameNumber;
         header->pcmFrameNumber = 0;
      }

      if (blockSize == 1)
         header->blockSizeInPCMFrames = 192;
      else if (blockSize <= 5)
         header->blockSizeInPCMFrames = 576 * (1 << (blockSize - 2));
      else if (blockSize == 6)
      {
         if (!rflac__read_uint16(bs, 8, &header->blockSizeInPCMFrames))
            return 0;
         crc8 = rflac_crc8(crc8, header->blockSizeInPCMFrames, 8);
         header->blockSizeInPCMFrames += 1;
      }
      else if (blockSize == 7)
      {
         if (!rflac__read_uint16(bs, 16, &header->blockSizeInPCMFrames))
            return 0;
         crc8 = rflac_crc8(crc8, header->blockSizeInPCMFrames, 16);
         if (header->blockSizeInPCMFrames == 0xFFFF) {
            /* Frame is too big. This is the size of the frame minus 1. The
             * STREAMINFO block defines the max block size which is 16-bits.
             * Adding one will make it 17 bits and therefore too big. */
            return 0;
         }
         header->blockSizeInPCMFrames += 1;
      }
      else
         header->blockSizeInPCMFrames = 256 * (1 << (blockSize - 8));


      if (sampleRate <= 11)
         header->sampleRate = sampleRateTable[sampleRate];
      else if (sampleRate == 12) {
         if (!rflac__read_uint32(bs, 8, &header->sampleRate))
            return 0;
         crc8 = rflac_crc8(crc8, header->sampleRate, 8);
         header->sampleRate *= 1000;
      } else if (sampleRate == 13) {
         if (!rflac__read_uint32(bs, 16, &header->sampleRate))
            return 0;
         crc8 = rflac_crc8(crc8, header->sampleRate, 16);
      } else if (sampleRate == 14) {
         if (!rflac__read_uint32(bs, 16, &header->sampleRate))
            return 0;
         crc8 = rflac_crc8(crc8, header->sampleRate, 16);
         header->sampleRate *= 10;
      } else {
         continue;  /* Invalid. Assume an invalid block. */
      }


      header->channelAssignment = channelAssignment;

      header->bitsPerSample = bitsPerSampleTable[bitsPerSample];
      if (header->bitsPerSample == 0)
         header->bitsPerSample = streaminfoBitsPerSample;

      /* If this subframe has a different bitsPerSample then streaminfo or the
       * first frame, reject it */
      if (header->bitsPerSample != streaminfoBitsPerSample)
         return 0;

      if (!rflac__read_uint8(bs, 8, &header->crc8))
         return 0;

      if (header->crc8 != crc8)
         /* CRC mismatch. Loop back to the top and find the next sync code. */
         continue;
      return 1;
   }
}

static uint32_t rflac__read_subframe_header(rflac_bs* bs,
      rflac_subframe* pSubframe)
{
   uint8_t header;
   int type;

   if (!rflac__read_uint8(bs, 8, &header))
      return 0;

   /* First bit should always be 0. */
   if ((header & 0x80) != 0)
      return 0;

   type = (header & 0x7E) >> 1;
   if (type == 0)
      pSubframe->subframeType = RFLAC_SUBFRAME_CONSTANT;
   else if (type == 1)
      pSubframe->subframeType = RFLAC_SUBFRAME_VERBATIM;
   else
   {
      if ((type & 0x20) != 0)
      {
         pSubframe->subframeType = RFLAC_SUBFRAME_LPC;
         pSubframe->lpcOrder = (uint8_t)(type & 0x1F) + 1;
      } else if ((type & 0x08) != 0) {
         pSubframe->subframeType = RFLAC_SUBFRAME_FIXED;
         pSubframe->lpcOrder = (uint8_t)(type & 0x07);
         if (pSubframe->lpcOrder > 4) {
            pSubframe->subframeType = RFLAC_SUBFRAME_RESERVED;
            pSubframe->lpcOrder = 0;
         }
      } else
         pSubframe->subframeType = RFLAC_SUBFRAME_RESERVED;
   }

   if (pSubframe->subframeType == RFLAC_SUBFRAME_RESERVED)
      return 0;

   /* Wasted bits per sample. */
   pSubframe->wastedBitsPerSample = 0;
   if ((header & 0x01) == 1) {
      unsigned int wastedBitsPerSample;
      if (!rflac__seek_past_next_set_bit(bs, &wastedBitsPerSample))
         return 0;
      pSubframe->wastedBitsPerSample = (uint8_t)wastedBitsPerSample + 1;
   }

   return 1;
}


/* -------------------------------------------------------------------------
 * Wide (33-bit) side-channel decoding, for the RFC 9639 32-bit extension.
 *
 * Stereo decorrelation stores the difference channel at bitsPerSample + 1
 * bits, so 32-bit streams need 33-bit side-channel samples. Residuals are
 * still guaranteed to fit in 32 bits (RFC 9639 restricts them so decoders
 * can keep 32-bit residual processing), so only warmup/constant/verbatim
 * sample reads, the prediction accumulator and the sample storage widen to
 * 64-bit. This path is scalar only; 32-bit content already bypasses the
 * SIMD reconstruction (which is gated to <= 24 bits).
 * ---------------------------------------------------------------------- */

static uint32_t rflac__read_int64w(rflac_bs* bs, unsigned int bitCount,
      int64_t* pResult)
{
   uint64_t result;

   if (bitCount > 32)
   {
      if (!rflac__read_uint64(bs, bitCount, &result))
         return 0;
   }
   else
   {
      uint32_t result32;
      if (!rflac__read_uint32(bs, bitCount, &result32))
         return 0;
      result = result32;
   }

   /* Sign extend. bitCount is never 64 here (at most 33). */
   *pResult = (int64_t)(result << (64 - bitCount)) >> (64 - bitCount);
   return 1;
}

static uint32_t rflac__decode_samples_with_residual__rice__wide(rflac_bs* bs,
      uint32_t count, uint8_t riceParam, uint32_t order, int32_t shift,
      const int32_t* coefficients, int64_t* pSamplesOut)
{
   uint32_t t[2] = {0x00000000, 0xFFFFFFFF};
   uint32_t zeroCountPart0;
   uint32_t riceParamPart0;
   uint32_t riceParamMask;
   uint32_t i;
   riceParamMask = (uint32_t)~((~0UL) << riceParam);

   for (i = 0; i < count; ++i)
   {
      uint32_t j;
      int64_t prediction = 0;

      if (!rflac__read_rice_parts_x1(bs, riceParam, &zeroCountPart0, &riceParamPart0))
         return 0;

      riceParamPart0 &= riceParamMask;
      riceParamPart0 |= (zeroCountPart0 << riceParam);
      riceParamPart0  = (riceParamPart0 >> 1) ^ t[riceParamPart0 & 0x01];

      for (j = 0; j < order; ++j)
         prediction += (int64_t)coefficients[j] * pSamplesOut[(int)i - (int)j - 1];

      pSamplesOut[i] = (int64_t)(int32_t)riceParamPart0 + (prediction >> shift);
   }

   return 1;
}

static uint32_t rflac__decode_samples_with_residual__unencoded__wide(
      rflac_bs* bs, uint32_t count, uint8_t unencodedBitsPerSample,
      uint32_t order, int32_t shift, const int32_t* coefficients,
      int64_t* pSamplesOut)
{
   uint32_t i;

   for (i = 0; i < count; ++i)
   {
      uint32_t j;
      int64_t prediction = 0;
      int32_t residual = 0;

      if (unencodedBitsPerSample > 0)
      {
         if (!rflac__read_int32(bs, unencodedBitsPerSample, &residual))
            return 0;
      }

      for (j = 0; j < order; ++j)
         prediction += (int64_t)coefficients[j] * pSamplesOut[(int)i - (int)j - 1];

      pSamplesOut[i] = (int64_t)residual + (prediction >> shift);
   }

   return 1;
}

static uint32_t rflac__decode_samples_with_residual__wide(rflac_bs* bs,
      uint32_t blockSize, uint32_t lpcOrder, int32_t lpcShift,
      const int32_t* coefficients, int64_t* pDecodedSamples)
{
   uint8_t residualMethod;
   uint8_t partitionOrder;
   uint32_t samplesInPartition;
   uint32_t partitionsRemaining;

   if (!rflac__read_uint8(bs, 2, &residualMethod))
      return 0;

   if (residualMethod != RFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE && residualMethod != RFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2)
      return 0;

   pDecodedSamples += lpcOrder;

   if (!rflac__read_uint8(bs, 4, &partitionOrder))
      return 0;

   if (partitionOrder > 8)
      return 0;

   if ((blockSize / (1 << partitionOrder)) < lpcOrder)
      return 0;

   samplesInPartition  = (blockSize / (1 << partitionOrder)) - lpcOrder;
   partitionsRemaining = (1 << partitionOrder);
   for (;;)
   {
      uint8_t riceParam = 0;
      if (residualMethod == RFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE) {
         if (!rflac__read_uint8(bs, 4, &riceParam))
            return 0;
         if (riceParam == 15)
            riceParam = 0xFF;
      } else if (residualMethod == RFLAC_RESIDUAL_CODING_METHOD_PARTITIONED_RICE2) {
         if (!rflac__read_uint8(bs, 5, &riceParam))
            return 0;
         if (riceParam == 31)
            riceParam = 0xFF;
      }

      if (riceParam != 0xFF) {
         if (!rflac__decode_samples_with_residual__rice__wide(bs, samplesInPartition, riceParam, lpcOrder, lpcShift, coefficients, pDecodedSamples))
            return 0;
      } else {
         uint8_t unencodedBitsPerSample = 0;
         if (!rflac__read_uint8(bs, 5, &unencodedBitsPerSample))
            return 0;

         if (!rflac__decode_samples_with_residual__unencoded__wide(bs, samplesInPartition, unencodedBitsPerSample, lpcOrder, lpcShift, coefficients, pDecodedSamples))
            return 0;
      }

      pDecodedSamples += samplesInPartition;

      if (partitionsRemaining == 1)
         break;

      partitionsRemaining -= 1;

      if (partitionOrder != 0)
         samplesInPartition = blockSize / (1 << partitionOrder);
   }

   return 1;
}

static uint32_t rflac__decode_samples__constant__wide(rflac_bs* bs,
      uint32_t blockSize, uint32_t subframeBitsPerSample,
      int64_t* pDecodedSamples)
{
   uint32_t i;
   int64_t sample;
   if (!rflac__read_int64w(bs, subframeBitsPerSample, &sample))
      return 0;

   for (i = 0; i < blockSize; ++i)
      pDecodedSamples[i] = sample;

   return 1;
}

static uint32_t rflac__decode_samples__verbatim__wide(rflac_bs* bs,
      uint32_t blockSize, uint32_t subframeBitsPerSample,
      int64_t* pDecodedSamples)
{
   uint32_t i;

   for (i = 0; i < blockSize; ++i) {
      if (!rflac__read_int64w(bs, subframeBitsPerSample, pDecodedSamples + i))
         return 0;
   }

   return 1;
}

static uint32_t rflac__decode_samples__fixed__wide(rflac_bs* bs,
      uint32_t blockSize, uint32_t subframeBitsPerSample, uint8_t lpcOrder,
      int64_t* pDecodedSamples)
{
   uint32_t i;

   static int32_t lpcCoefficientsTable[5][4] = {
      {0,  0, 0,  0},
      {1,  0, 0,  0},
      {2, -1, 0,  0},
      {3, -3, 1,  0},
      {4, -6, 4, -1}
   };

   for (i = 0; i < lpcOrder; ++i) {
      if (!rflac__read_int64w(bs, subframeBitsPerSample, pDecodedSamples + i))
         return 0;
   }

   if (!rflac__decode_samples_with_residual__wide(bs, blockSize, lpcOrder, 0, lpcCoefficientsTable[lpcOrder], pDecodedSamples))
      return 0;

   return 1;
}

static uint32_t rflac__decode_samples__lpc__wide(rflac_bs* bs,
      uint32_t blockSize, uint32_t bitsPerSample, uint8_t lpcOrder,
      int64_t* pDecodedSamples)
{
   uint8_t i;
   uint8_t lpcPrecision;
   int8_t lpcShift;
   int32_t coefficients[32];

   for (i = 0; i < lpcOrder; ++i) {
      if (!rflac__read_int64w(bs, bitsPerSample, pDecodedSamples + i))
         return 0;
   }

   if (!rflac__read_uint8(bs, 4, &lpcPrecision))
      return 0;
   if (lpcPrecision == 15)
      return 0;
   lpcPrecision += 1;

   if (!rflac__read_int8(bs, 5, &lpcShift))
      return 0;
   if (lpcShift < 0)
      return 0;

   memset(coefficients, 0, sizeof(coefficients));
   for (i = 0; i < lpcOrder; ++i) {
      if (!rflac__read_int32(bs, lpcPrecision, coefficients + i))
         return 0;
   }

   if (!rflac__decode_samples_with_residual__wide(bs, blockSize, lpcOrder, lpcShift, coefficients, pDecodedSamples))
      return 0;
   return 1;
}

static uint32_t rflac__decode_subframe_wide(rflac_bs* bs, rflac_frame* frame,
      int subframeIndex, uint32_t subframeBitsPerSample,
      int64_t* pDecodedSamplesOut)
{
   rflac_subframe* pSubframe = frame->subframes + subframeIndex;
   if (!rflac__read_subframe_header(bs, pSubframe))
      return 0;

   if (pSubframe->wastedBitsPerSample >= subframeBitsPerSample)
      return 0;
   subframeBitsPerSample -= pSubframe->wastedBitsPerSample;

   pSubframe->pSamplesS32 = NULL;  /* Samples live in the wide plane. */

   switch (pSubframe->subframeType)
   {
      case RFLAC_SUBFRAME_CONSTANT:
         return rflac__decode_samples__constant__wide(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pDecodedSamplesOut);
      case RFLAC_SUBFRAME_VERBATIM:
         return rflac__decode_samples__verbatim__wide(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pDecodedSamplesOut);
      case RFLAC_SUBFRAME_FIXED:
         return rflac__decode_samples__fixed__wide(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pSubframe->lpcOrder, pDecodedSamplesOut);
      case RFLAC_SUBFRAME_LPC:
         return rflac__decode_samples__lpc__wide(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pSubframe->lpcOrder, pDecodedSamplesOut);
      default:
         return 0;
   }
}

static uint32_t rflac__decode_subframe(rflac_bs* bs, rflac_frame* frame,
      int subframeIndex, int32_t* pDecodedSamplesOut)
{
   uint32_t subframeBitsPerSample;
   rflac_subframe* pSubframe = frame->subframes + subframeIndex;
   if (!rflac__read_subframe_header(bs, pSubframe))
      return 0;

   /* Side channels require an extra bit per sample. Took a while to figure that
    * one out... */
   subframeBitsPerSample = frame->header.bitsPerSample;
   if ((frame->header.channelAssignment == RFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE || frame->header.channelAssignment == RFLAC_CHANNEL_ASSIGNMENT_MID_SIDE) && subframeIndex == 1)
      subframeBitsPerSample += 1;
   else if (frame->header.channelAssignment == RFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE && subframeIndex == 0)
      subframeBitsPerSample += 1;

   /* 33-bit side channels take the wide path (rflac__decode_subframe_wide);
    * anything reaching this narrow path wider than 32 is invalid. */
   if (subframeBitsPerSample > 32)
      return 0;

   /* Need to handle wasted bits per sample. */
   if (pSubframe->wastedBitsPerSample >= subframeBitsPerSample)
      return 0;
   subframeBitsPerSample -= pSubframe->wastedBitsPerSample;

   pSubframe->pSamplesS32 = pDecodedSamplesOut;

   switch (pSubframe->subframeType)
   {
      case RFLAC_SUBFRAME_CONSTANT:
      {
         rflac__decode_samples__constant(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pSubframe->pSamplesS32);
      } break;

      case RFLAC_SUBFRAME_VERBATIM:
      {
         rflac__decode_samples__verbatim(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pSubframe->pSamplesS32);
      } break;

      case RFLAC_SUBFRAME_FIXED:
      {
         rflac__decode_samples__fixed(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pSubframe->lpcOrder, pSubframe->pSamplesS32);
      } break;

      case RFLAC_SUBFRAME_LPC:
      {
         rflac__decode_samples__lpc(bs, frame->header.blockSizeInPCMFrames, subframeBitsPerSample, pSubframe->lpcOrder, pSubframe->pSamplesS32);
      } break;

      default: return 0;
   }

   return 1;
}


static INLINE uint8_t rflac__get_channel_count_from_channel_assignment(
      int8_t channelAssignment)
{
   uint8_t lookup[] = {1, 2, 3, 4, 5, 6, 7, 8, 2, 2, 2};
   return lookup[channelAssignment];
}

static int32_t rflac__decode_flac_frame_unchecked(rflac* pFlac)
{
   int channelCount;
   int i;
   uint8_t paddingSizeInBits;
   uint16_t desiredCRC16;
   uint16_t actualCRC16;

   /* This function should be called while the stream is sitting on the first
    * byte after the frame header. */
   memset(pFlac->currentFLACFrame.subframes, 0, sizeof(pFlac->currentFLACFrame.subframes));

   /* The frame block size must never be larger than the maximum block size
    * defined by the FLAC stream. */
   if (pFlac->currentFLACFrame.header.blockSizeInPCMFrames > pFlac->maxBlockSizeInPCMFrames)
      return RFLAC_ERROR;

   /* The number of channels in the frame must match the channel count from the
    * STREAMINFO block. */
   channelCount = rflac__get_channel_count_from_channel_assignment(pFlac->currentFLACFrame.header.channelAssignment);
   if (channelCount != (int)pFlac->channels)
      return RFLAC_ERROR;

   pFlac->wideChannelIndex = 0xFF;
   for (i = 0; i < channelCount; ++i)
   {
      /* 32-bit streams store the stereo difference channel at 33 bits,
       * which needs the wide (int64) decode path. */
      int isWide = 0;
      if (pFlac->currentFLACFrame.header.bitsPerSample == 32)
      {
         if ((pFlac->currentFLACFrame.header.channelAssignment == RFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE || pFlac->currentFLACFrame.header.channelAssignment == RFLAC_CHANNEL_ASSIGNMENT_MID_SIDE) && i == 1)
            isWide = 1;
         else if (pFlac->currentFLACFrame.header.channelAssignment == RFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE && i == 0)
            isWide = 1;
      }

      if (isWide)
      {
         if (!rflac__decode_subframe_wide(&pFlac->bs, &pFlac->currentFLACFrame, i, 33, pFlac->pWideSamples))
            return RFLAC_ERROR;
         pFlac->wideChannelIndex = (uint8_t)i;
      }
      else if (!rflac__decode_subframe(&pFlac->bs, &pFlac->currentFLACFrame, i, pFlac->pDecodedSamples + (pFlac->currentFLACFrame.header.blockSizeInPCMFrames * i)))
         return RFLAC_ERROR;
   }

   paddingSizeInBits = (uint8_t)(RFLAC_CACHE_L1_BITS_REMAINING(&pFlac->bs) & 7);
   if (paddingSizeInBits > 0)
   {
      uint8_t padding = 0;
      if (!rflac__read_uint8(&pFlac->bs, paddingSizeInBits, &padding))
         return RFLAC_AT_END;
   }

   actualCRC16 = rflac__flush_crc16(&pFlac->bs);
   if (!rflac__read_uint16(&pFlac->bs, 16, &desiredCRC16))
      return RFLAC_AT_END;

   if (actualCRC16 != desiredCRC16)
      return RFLAC_CRC_MISMATCH;    /* CRC mismatch. */

   pFlac->currentFLACFrame.pcmFramesRemaining = pFlac->currentFLACFrame.header.blockSizeInPCMFrames;

   return RFLAC_SUCCESS;
}

/* A frame that did not decode has no samples, and must not look as
 * though it has.
 *
 * Decoding clobbers the subframe sample pointers on its way through -
 * the wide path sets them NULL by design, the narrow one leaves them
 * as they were if it rejects a subframe header - but the count of
 * frames still to hand out is only written at the end, on success.
 * A failure therefore left the previous frame's count against this
 * frame's cleared pointers.
 *
 * The reader alone would survive that, since it takes the count as
 * what is left of the frame it last decoded.  Seeking does not: the
 * fast paths in rflac_seek_to_pcm_frame move within the current frame
 * by adjusting that count, and the backward one derives it from the
 * block size, so a count of zero was restored to something positive
 * and the next read went through a NULL pointer.  Reached by a
 * corrupt stream - a frame header claiming 32 bits inside a 16-bit
 * one sends a subframe down the wide path, which fails - and then a
 * seek backwards.
 *
 * Clearing the block size as well as the count is what makes the fast
 * paths decline: both compute from it, so both fall through to a real
 * seek, which re-reads a frame header and decodes properly. */
static int32_t rflac__decode_flac_frame(rflac* pFlac)
{
   int32_t result = rflac__decode_flac_frame_unchecked(pFlac);
   if (result != RFLAC_SUCCESS)
   {
      pFlac->currentFLACFrame.pcmFramesRemaining          = 0;
      pFlac->currentFLACFrame.header.blockSizeInPCMFrames = 0;
   }
   return result;
}

static uint32_t rflac__read_and_decode_next_flac_frame(rflac* pFlac)
{
   for (;;)
   {
      int32_t result;

      if (!rflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header))
         return 0;

      result = rflac__decode_flac_frame(pFlac);
      if (result != RFLAC_SUCCESS)
      {
         if (result == RFLAC_CRC_MISMATCH)
            continue;   /* CRC mismatch. Skip to the next frame. */
         return 0;
      }

      return 1;
   }
}


static uint64_t rflac__seek_forward_by_pcm_frames(rflac* pFlac,
      uint64_t pcmFramesToSeek)
{
   uint64_t pcmFramesRead = 0;
   while (pcmFramesToSeek > 0) {
      if (pFlac->currentFLACFrame.pcmFramesRemaining == 0) {
         if (!rflac__read_and_decode_next_flac_frame(pFlac)) {
            /* Couldn't read the next frame, so just break from the loop and
             * return. */
            break;
         }
      } else {
         if (pFlac->currentFLACFrame.pcmFramesRemaining > pcmFramesToSeek) {
            pcmFramesRead   += pcmFramesToSeek;
            /* <-- Safe cast. Will always be < currentFrame.pcmFramesRemaining <
             * 65536. */
            pFlac->currentFLACFrame.pcmFramesRemaining -= (uint32_t)pcmFramesToSeek;
            pcmFramesToSeek  = 0;
         } else {
            pcmFramesRead   += pFlac->currentFLACFrame.pcmFramesRemaining;
            pcmFramesToSeek -= pFlac->currentFLACFrame.pcmFramesRemaining;
            pFlac->currentFLACFrame.pcmFramesRemaining = 0;
         }
      }
   }

   pFlac->currentPCMFrame += pcmFramesRead;
   return pcmFramesRead;
}



typedef struct
{
   rflac_read_proc onRead;
   rflac_seek_proc onSeek;
   rflac_meta_proc onMeta;
   void* pUserData;
   void* pUserDataMD;
   uint32_t sampleRate;
   uint8_t  channels;
   uint8_t  bitsPerSample;
   uint64_t totalPCMFrameCount;
   uint16_t maxBlockSizeInPCMFrames;
   uint64_t runningFilePos;
   uint32_t hasMetadataBlocks;
   /* <-- A bit streamer is required for loading data during initialization. */
   rflac_bs bs;

} rflac_init_info;

static INLINE uint32_t rflac__read_and_decode_block_header(
      rflac_read_proc onRead, void* pUserData, uint8_t* isLastBlock,
      uint8_t* blockType, uint32_t* blockSize)
{
   uint32_t blockHeader;

   *blockSize = 0;
   if (onRead(pUserData, &blockHeader, 4) != 4)
      return 0;

#ifndef MSB_FIRST
   blockHeader  = rflac__swap_endian_uint32(blockHeader);
#endif
   *isLastBlock = (uint8_t)((blockHeader & 0x80000000UL) >> 31);
   *blockType   = (uint8_t)((blockHeader & 0x7F000000UL) >> 24);
   *blockSize   = (blockHeader & 0x00FFFFFFUL);
   return 1;
}

static uint32_t rflac__read_streaminfo(rflac_read_proc onRead, void* pUserData,
      rflac_streaminfo* pStreamInfo)
{
   uint32_t blockSizes;
   uint64_t frameSizes = 0;
   uint64_t importantProps;
   uint8_t md5[16];

   /* min/max block size. */
   if (onRead(pUserData, &blockSizes, 4) != 4)
      return 0;

   /* min/max frame size. */
   if (onRead(pUserData, &frameSizes, 6) != 6)
      return 0;

   /* Sample rate, channels, bits per sample and total sample count. */
   if (onRead(pUserData, &importantProps, 8) != 8)
      return 0;

   /* MD5 */
   if (onRead(pUserData, md5, sizeof(md5)) != sizeof(md5))
      return 0;

#ifndef MSB_FIRST
   blockSizes     = rflac__swap_endian_uint32(blockSizes);
   frameSizes     = rflac__swap_endian_uint64(frameSizes);
   importantProps = rflac__swap_endian_uint64(importantProps);
#endif

   pStreamInfo->minBlockSizeInPCMFrames = (uint16_t)((blockSizes & 0xFFFF0000) >> 16);
   pStreamInfo->maxBlockSizeInPCMFrames = (uint16_t) (blockSizes & 0x0000FFFF);
   pStreamInfo->minFrameSizeInPCMFrames = (uint32_t)((frameSizes     &  (((uint64_t)0x00FFFFFF << 16) << 24)) >> 40);
   pStreamInfo->maxFrameSizeInPCMFrames = (uint32_t)((frameSizes     &  (((uint64_t)0x00FFFFFF << 16) <<  0)) >> 16);
   pStreamInfo->sampleRate              = (uint32_t)((importantProps &  (((uint64_t)0x000FFFFF << 16) << 28)) >> 44);
   pStreamInfo->channels                = (uint8_t )((importantProps &  (((uint64_t)0x0000000E << 16) << 24)) >> 41) + 1;
   pStreamInfo->bitsPerSample           = (uint8_t )((importantProps &  (((uint64_t)0x0000001F << 16) << 20)) >> 36) + 1;
   pStreamInfo->totalPCMFrameCount      =                ((importantProps & ((((uint64_t)0x0000000F << 16) << 16) | 0xFFFFFFFF)));
   memcpy(pStreamInfo->md5, md5, sizeof(md5));

   return 1;
}

static uint32_t rflac__read_and_decode_metadata(rflac_read_proc onRead,
      rflac_seek_proc onSeek, rflac_meta_proc onMeta, void* pUserData,
      void* pUserDataMD, uint64_t* pFirstFramePos, uint64_t* pSeektablePos,
      uint32_t* pSeekpointCount)
{
   /* We want to keep track of the byte position in the stream of the seektable.
    * At the time of calling this function we know that we'll be sitting on byte
    * 42.
    */
   uint64_t runningFilePos = 42;
   uint64_t seektablePos   = 0;
   uint32_t seektableSize  = 0;

   for (;;)
   {
      rflac_metadata metadata;
      uint8_t isLastBlock = 0;
      uint8_t blockType = 0;
      uint32_t blockSize;
      if (rflac__read_and_decode_block_header(onRead, pUserData, &isLastBlock, &blockType, &blockSize) == 0)
         return 0;
      runningFilePos += 4;

      metadata.type = blockType;
      metadata.pRawData = NULL;
      metadata.rawDataSize = 0;

      switch (blockType)
      {
         case RFLAC_METADATA_BLOCK_TYPE_APPLICATION:
         {
            if (blockSize < 4)
               return 0;

            if (onMeta)
            {
               void* pRawData = malloc(blockSize);
               if (pRawData == NULL)
                  return 0;

               if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                  free(pRawData);
                  return 0;
               }

               metadata.pRawData = pRawData;
               metadata.rawDataSize = blockSize;
               metadata.data.application.id       = *(uint32_t*)pRawData;
#ifndef MSB_FIRST
               metadata.data.application.id       = rflac__swap_endian_uint32(metadata.data.application.id);
#endif
               metadata.data.application.pData    = (const void*)((uint8_t*)pRawData + sizeof(uint32_t));
               metadata.data.application.dataSize = blockSize - sizeof(uint32_t);
               onMeta(pUserDataMD, &metadata);

               free(pRawData);
            }
         } break;

         case RFLAC_METADATA_BLOCK_TYPE_SEEKTABLE:
         {
            seektablePos  = runningFilePos;
            seektableSize = blockSize;

            if (onMeta)
            {
               uint32_t iSeekpoint;
               uint32_t seekpointCount = blockSize/RFLAC_SEEKPOINT_SIZE_IN_BYTES;
               void *pRawData = malloc(seekpointCount * sizeof(rflac_seekpoint));
               if (pRawData == NULL)
                  return 0;

               /* Read seekpoint by seekpoint and do some processing. */
               for (iSeekpoint = 0; iSeekpoint < seekpointCount; ++iSeekpoint) {
                  rflac_seekpoint* pSeekpoint = (rflac_seekpoint*)pRawData + iSeekpoint;

                  if (onRead(pUserData, pSeekpoint, RFLAC_SEEKPOINT_SIZE_IN_BYTES) != RFLAC_SEEKPOINT_SIZE_IN_BYTES)
                  {
                     free(pRawData);
                     return 0;
                  }

                  /* Endian swap. */
#ifndef MSB_FIRST
                  pSeekpoint->firstPCMFrame   = rflac__swap_endian_uint64(pSeekpoint->firstPCMFrame);
                  pSeekpoint->flacFrameOffset = rflac__swap_endian_uint64(pSeekpoint->flacFrameOffset);
                  pSeekpoint->pcmFrameCount   = rflac__swap_endian_uint16(pSeekpoint->pcmFrameCount);
#endif
               }

               metadata.pRawData = pRawData;
               metadata.rawDataSize = blockSize;
               metadata.data.seektable.seekpointCount = seekpointCount;
               metadata.data.seektable.pSeekpoints = (const rflac_seekpoint*)pRawData;

               onMeta(pUserDataMD, &metadata);

               free(pRawData);
            }
         } break;

         case RFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT:
         {
            if (blockSize < 8)
               return 0;

            if (onMeta)
            {
               const char* pRunningData;
               const char* pRunningDataEnd;
               uint32_t i;
               void *pRawData = malloc(blockSize);
               if (pRawData == NULL)
                  return 0;

               if (onRead(pUserData, pRawData, blockSize) != blockSize)
               {
                  free(pRawData);
                  return 0;
               }

               metadata.pRawData = pRawData;
               metadata.rawDataSize = blockSize;

               pRunningData    = (const char*)pRawData;
               pRunningDataEnd = (const char*)pRawData + blockSize;

               metadata.data.vorbis_comment.vendorLength = rflac__le2host_32_ptr_unaligned(pRunningData); pRunningData += 4;

               /* Need space for the rest of the block */
               /* Note the order of operations to avoid overflow to a valid
                * value */
               if ((pRunningDataEnd - pRunningData) - 4 < (int64_t)metadata.data.vorbis_comment.vendorLength)
               {
                  free(pRawData);
                  return 0;
               }
               metadata.data.vorbis_comment.vendor       = pRunningData;                                            pRunningData += metadata.data.vorbis_comment.vendorLength;
               metadata.data.vorbis_comment.commentCount = rflac__le2host_32_ptr_unaligned(pRunningData); pRunningData += 4;

               /* Need space for 'commentCount' comments after the block, which
                * at minimum is a uint32_t per comment */
               /* Note the order of operations to avoid overflow to a valid
                * value */
               if ((pRunningDataEnd - pRunningData) / sizeof(uint32_t) < metadata.data.vorbis_comment.commentCount)
               {
                  free(pRawData);
                  return 0;
               }
               metadata.data.vorbis_comment.pComments    = pRunningData;

               /* Check that the comments section is valid before passing it to
                * the callback */
               for (i = 0; i < metadata.data.vorbis_comment.commentCount; ++i) {
                  uint32_t commentLength;

                  if (pRunningDataEnd - pRunningData < 4) {
                     free(pRawData);
                     return 0;
                  }

                  commentLength = rflac__le2host_32_ptr_unaligned(pRunningData); pRunningData += 4;
                  /* Note the order of operations to avoid overflow to a valid
                   * value */
                  if (pRunningDataEnd - pRunningData < (int64_t)commentLength) {
                     free(pRawData);
                     return 0;
                  }
                  pRunningData += commentLength;
               }

               onMeta(pUserDataMD, &metadata);

               free(pRawData);
            }
         } break;

         case RFLAC_METADATA_BLOCK_TYPE_CUESHEET:
         {
            if (blockSize < 396)
               return 0;

            if (onMeta)
            {
               const char* pRunningData;
               const char* pRunningDataEnd;
               size_t bufferSize;
               uint8_t iTrack;
               uint8_t iIndex;
               void *pTrackData;

               /* This needs to be loaded in two passes. The first pass is used
                * to calculate the size of the memory allocation we need for
                * storing the necessary data. The second pass will fill that
                * buffer with usable data.
                */
               void *pRawData = malloc(blockSize);
               if (pRawData == NULL)
                  return 0;

               if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                  free(pRawData);
                  return 0;
               }

               metadata.pRawData = pRawData;
               metadata.rawDataSize = blockSize;

               pRunningData    = (const char*)pRawData;
               pRunningDataEnd = (const char*)pRawData + blockSize;

               memcpy(metadata.data.cuesheet.catalog, pRunningData, 128);                              pRunningData += 128;
               metadata.data.cuesheet.leadInSampleCount = *(const uint64_t*)pRunningData;
#ifndef MSB_FIRST
               metadata.data.cuesheet.leadInSampleCount = rflac__swap_endian_uint64(metadata.data.cuesheet.leadInSampleCount);
#endif
               pRunningData += 8;
               metadata.data.cuesheet.isCD              = (pRunningData[0] & 0x80) != 0;                           pRunningData += 259;
               metadata.data.cuesheet.trackCount        = pRunningData[0];                                         pRunningData += 1;
               /* Will be filled later. */
               metadata.data.cuesheet.pTrackData        = NULL;

               /* Pass 1: calculate the buffer size for the track data. */
               {
                  /* Will be restored at the end in preparation for the second
                   * pass. */
                  const char* pRunningDataSaved = pRunningData;

                  bufferSize = metadata.data.cuesheet.trackCount * RFLAC_CUESHEET_TRACK_SIZE_IN_BYTES;

                  for (iTrack = 0; iTrack < metadata.data.cuesheet.trackCount; ++iTrack) {
                     uint8_t indexCount;
                     uint32_t indexPointSize;

                     if (pRunningDataEnd - pRunningData < RFLAC_CUESHEET_TRACK_SIZE_IN_BYTES) {
                        free(pRawData);
                        return 0;
                     }

                     /* Skip to the index point count */
                     pRunningData += 35;

                     indexCount = pRunningData[0];
                     pRunningData += 1;

                     bufferSize += indexCount * sizeof(rflac_cuesheet_track_index);

                     /* Quick validation check. */
                     indexPointSize = indexCount * RFLAC_CUESHEET_TRACK_INDEX_SIZE_IN_BYTES;
                     if (pRunningDataEnd - pRunningData < (int64_t)indexPointSize) {
                        free(pRawData);
                        return 0;
                     }

                     pRunningData += indexPointSize;
                  }

                  pRunningData = pRunningDataSaved;
               }

               /* Pass 2: Allocate a buffer and fill the data. Validation was
                * done in the step above so can be skipped. */
               {
                  char* pRunningTrackData;

                  pTrackData = malloc(bufferSize);
                  if (pTrackData == NULL)
                  {
                     free(pRawData);
                     return 0;
                  }

                  pRunningTrackData = (char*)pTrackData;

                  for (iTrack = 0; iTrack < metadata.data.cuesheet.trackCount; ++iTrack) {
                     uint8_t indexCount;

                     memcpy(pRunningTrackData, pRunningData, RFLAC_CUESHEET_TRACK_SIZE_IN_BYTES);
                     /* Skip forward, but not beyond the last byte in the
                      * CUESHEET_TRACK block which is the index count. */
                     pRunningData      += RFLAC_CUESHEET_TRACK_SIZE_IN_BYTES-1;
                     pRunningTrackData += RFLAC_CUESHEET_TRACK_SIZE_IN_BYTES-1;

                     /* Grab the index count for the next part. */
                     indexCount = pRunningData[0];
                     pRunningData      += 1;
                     pRunningTrackData += 1;

                     /* Extract each track index. */
                     for (iIndex = 0; iIndex < indexCount; ++iIndex) {
                        rflac_cuesheet_track_index* pTrackIndex = (rflac_cuesheet_track_index*)pRunningTrackData;

                        memcpy(pRunningTrackData, pRunningData, RFLAC_CUESHEET_TRACK_INDEX_SIZE_IN_BYTES);
                        pRunningData      += RFLAC_CUESHEET_TRACK_INDEX_SIZE_IN_BYTES;
                        pRunningTrackData += sizeof(rflac_cuesheet_track_index);

#ifndef MSB_FIRST
                        pTrackIndex->offset = rflac__swap_endian_uint64(pTrackIndex->offset);
#endif
                     }
                  }

                  metadata.data.cuesheet.pTrackData = pTrackData;
               }

               /* The original data is no longer needed. */
               free(pRawData);
               pRawData = NULL;

               onMeta(pUserDataMD, &metadata);

               free(pTrackData);
               pTrackData = NULL;
            }
         } break;

         case RFLAC_METADATA_BLOCK_TYPE_PICTURE:
         {
            if (blockSize < 32)
               return 0;

            if (onMeta) {
               const char* pRunningData;
               const char* pRunningDataEnd;
               void *pRawData = malloc(blockSize);
               if (pRawData == NULL)
                  return 0;

               if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                  free(pRawData);
                  return 0;
               }

               metadata.pRawData = pRawData;
               metadata.rawDataSize = blockSize;

               pRunningData    = (const char*)pRawData;
               pRunningDataEnd = (const char*)pRawData + blockSize;

               metadata.data.picture.type       = rflac__be2host_32_ptr_unaligned(pRunningData); pRunningData += 4;
               metadata.data.picture.mimeLength = rflac__be2host_32_ptr_unaligned(pRunningData); pRunningData += 4;

               /* Need space for the rest of the block */
               /* Note the order of operations to avoid overflow to a valid
                * value */
               if ((pRunningDataEnd - pRunningData) - 24 < (int64_t)metadata.data.picture.mimeLength) {
                  free(pRawData);
                  return 0;
               }
               metadata.data.picture.mime              = pRunningData;                                   pRunningData += metadata.data.picture.mimeLength;
               metadata.data.picture.descriptionLength = rflac__be2host_32_ptr_unaligned(pRunningData); pRunningData += 4;

               /* Need space for the rest of the block */
               /* Note the order of operations to avoid overflow to a valid
                * value */
               if ((pRunningDataEnd - pRunningData) - 20 < (int64_t)metadata.data.picture.descriptionLength) {
                  free(pRawData);
                  return 0;
               }
               metadata.data.picture.description     = pRunningData;                                   pRunningData += metadata.data.picture.descriptionLength;
               metadata.data.picture.width           = rflac__be2host_32_ptr_unaligned(pRunningData); pRunningData += 4;
               metadata.data.picture.height          = rflac__be2host_32_ptr_unaligned(pRunningData); pRunningData += 4;
               metadata.data.picture.colorDepth      = rflac__be2host_32_ptr_unaligned(pRunningData); pRunningData += 4;
               metadata.data.picture.indexColorCount = rflac__be2host_32_ptr_unaligned(pRunningData); pRunningData += 4;
               metadata.data.picture.pictureDataSize = rflac__be2host_32_ptr_unaligned(pRunningData); pRunningData += 4;
               metadata.data.picture.pPictureData    = (const uint8_t*)pRunningData;

               /* Need space for the picture after the block */
               /* Note the order of operations to avoid overflow to a valid
                * value */
               if (pRunningDataEnd - pRunningData < (int64_t)metadata.data.picture.pictureDataSize) {
                  free(pRawData);
                  return 0;
               }

               onMeta(pUserDataMD, &metadata);

               free(pRawData);
            }
         } break;

         case RFLAC_METADATA_BLOCK_TYPE_PADDING:
         {
            if (onMeta) {

               /* Padding doesn't have anything meaningful in it, so just skip
                * over it, but make sure the caller is aware of it by firing the
                * callback. */
               if (!onSeek(pUserData, blockSize, rflac_seek_origin_current)) {
                  /* An error occurred while seeking. Attempt to recover by
                   * treating this as the last block which will in turn
                   * terminate the loop. */
                  isLastBlock = 1;
               } else {
                  onMeta(pUserDataMD, &metadata);
               }
            }
         } break;

         case RFLAC_METADATA_BLOCK_TYPE_INVALID:
         {
            /* Invalid chunk. Just skip over this one. */
            if (onMeta) {
               if (!onSeek(pUserData, blockSize, rflac_seek_origin_current)) {
                  /* An error occurred while seeking. Attempt to recover by
                   * treating this as the last block which will in turn
                   * terminate the loop. */
                  isLastBlock = 1;
               }
            }
         } break;

         default:
         {
            /* It's an unknown chunk, but not necessarily invalid. There's a
             * chance more metadata blocks might be defined later on, so we can
             * at the very least report the chunk to the application and let it
             * look at the raw data.
             */
            if (onMeta) {
               void* pRawData = malloc(blockSize);
               if (pRawData == NULL)
                  return 0;

               if (onRead(pUserData, pRawData, blockSize) != blockSize) {
                  free(pRawData);
                  return 0;
               }

               metadata.pRawData = pRawData;
               metadata.rawDataSize = blockSize;
               onMeta(pUserDataMD, &metadata);

               free(pRawData);
            }
         } break;
      }

      /* If we're not handling metadata, just skip over the block. If we are, it
       * will have been handled earlier in the switch statement above. */
      if (onMeta == NULL && blockSize > 0) {
         if (!onSeek(pUserData, blockSize, rflac_seek_origin_current))
            isLastBlock = 1;
      }

      runningFilePos += blockSize;
      if (isLastBlock)
         break;
   }

   *pSeektablePos   = seektablePos;
   *pSeekpointCount = seektableSize / RFLAC_SEEKPOINT_SIZE_IN_BYTES;
   *pFirstFramePos  = runningFilePos;

   return 1;
}

static uint32_t rflac__init_private__native(rflac_init_info* pInit,
      rflac_read_proc onRead, rflac_seek_proc onSeek, rflac_meta_proc onMeta,
      void* pUserData, void* pUserDataMD)
{
   /* Pre Condition: The bit stream should be sitting just past the 4-byte id
    * header. */

   uint8_t isLastBlock;
   uint8_t blockType;
   uint32_t blockSize;
   rflac_streaminfo streaminfo;

   (void)onSeek;

   /* The first metadata block should be the STREAMINFO block. */
   if (!rflac__read_and_decode_block_header(onRead, pUserData, &isLastBlock, &blockType, &blockSize))
      return 0;

   /* The first block must be the STREAMINFO block. (The old 'relaxed'
    * mode that tolerated its absence was only reachable through the
    * removed dr_flac container-selection API.) */
   if (blockType != RFLAC_METADATA_BLOCK_TYPE_STREAMINFO || blockSize != 34)
      return 0;

   if (!rflac__read_streaminfo(onRead, pUserData, &streaminfo))
      return 0;

   pInit->sampleRate              = streaminfo.sampleRate;
   pInit->channels                = streaminfo.channels;
   pInit->bitsPerSample           = streaminfo.bitsPerSample;
   pInit->totalPCMFrameCount      = streaminfo.totalPCMFrameCount;
   /* Don't care about the min block size - only the max (used for determining
    * the size of the memory allocation). */
   pInit->maxBlockSizeInPCMFrames = streaminfo.maxBlockSizeInPCMFrames;
   pInit->hasMetadataBlocks       = !isLastBlock;

   if (onMeta) {
      rflac_metadata metadata;
      metadata.type = RFLAC_METADATA_BLOCK_TYPE_STREAMINFO;
      metadata.pRawData = NULL;
      metadata.rawDataSize = 0;
      metadata.data.streaminfo = streaminfo;
      onMeta(pUserDataMD, &metadata);
   }

   return 1;
}


static uint32_t rflac__init_private(rflac_init_info* pInit,
      rflac_read_proc onRead, rflac_seek_proc onSeek, rflac_meta_proc onMeta,
      void* pUserData, void* pUserDataMD)
{
   uint8_t id[4];

   if (pInit == NULL || onRead == NULL || onSeek == NULL)
      return 0;

   memset(pInit, 0, sizeof(*pInit));
   pInit->onRead       = onRead;
   pInit->onSeek       = onSeek;
   pInit->onMeta       = onMeta;
   pInit->pUserData    = pUserData;
   pInit->pUserDataMD  = pUserDataMD;

   pInit->bs.onRead    = onRead;
   pInit->bs.onSeek    = onSeek;
   pInit->bs.pUserData = pUserData;
   rflac__reset_cache(&pInit->bs);


   /* Skip over any ID3 tags. */
   for (;;)
   {
      if (onRead(pUserData, id, 4) != 4)
         return 0;    /* Ran out of data. */
      pInit->runningFilePos += 4;

      if (id[0] == 'I' && id[1] == 'D' && id[2] == '3') {
         uint8_t header[6];
         uint8_t flags;
         uint32_t headerSize;

         if (onRead(pUserData, header, 6) != 6)
            return 0;    /* Ran out of data. */
         pInit->runningFilePos += 6;

         flags = header[1];

         memcpy(&headerSize, header+2, 4);
#ifndef MSB_FIRST
         headerSize = rflac__swap_endian_uint32(headerSize);
#endif
         headerSize = rflac__unsynchsafe_32(headerSize);
         if (flags & 0x10)
            headerSize += 10;

         if (!onSeek(pUserData, headerSize, rflac_seek_origin_current))
            return 0;    /* Failed to seek past the tag. */
         pInit->runningFilePos += headerSize;
      }
      else
         break;
   }

   if (id[0] == 'f' && id[1] == 'L' && id[2] == 'a' && id[3] == 'C')
      return rflac__init_private__native(pInit, onRead, onSeek, onMeta, pUserData, pUserDataMD);

   /* Unsupported container. */
   return 0;
}

static void rflac__init_from_info(rflac* pFlac, const rflac_init_info* pInit)
{
   memset(pFlac, 0, sizeof(*pFlac));
   pFlac->bs                      = pInit->bs;
   pFlac->onMeta                  = pInit->onMeta;
   pFlac->pUserDataMD             = pInit->pUserDataMD;
   pFlac->maxBlockSizeInPCMFrames = pInit->maxBlockSizeInPCMFrames;
   pFlac->sampleRate              = pInit->sampleRate;
   pFlac->channels                = (uint8_t)pInit->channels;
   pFlac->bitsPerSample           = (uint8_t)pInit->bitsPerSample;
   pFlac->totalPCMFrameCount      = pInit->totalPCMFrameCount;
}

static rflac* rflac_open_with_metadata_private(rflac_read_proc onRead,
      rflac_seek_proc onSeek, rflac_meta_proc onMeta, void* pUserData,
      void* pUserDataMD)
{
   rflac_init_info init;
   uint32_t allocationSize;
   uint32_t wholeSIMDVectorCountPerChannel;
   uint32_t decodedSamplesAllocationSize;
   uint32_t wideSamplesAllocationSize;
   uint64_t firstFramePos;
   uint64_t seektablePos;
   uint32_t seekpointCount;
   rflac* pFlac;

   /* CPU support first. */
   rflac__init_cpu_caps();
   rflac__crc16_init_slices();

   if (!rflac__init_private(&init, onRead, onSeek, onMeta, pUserData, pUserDataMD))
      return NULL;

   /*
   The size of the allocation for the rflac object needs to be large enough to fit the following:
    1) The main members of the rflac structure
    2) A block of memory large enough to store the decoded samples of the largest frame in the stream

   The complicated part of the allocation is making sure there's enough room the decoded samples, taking into consideration
   the different SIMD instruction sets.
   */
   allocationSize = sizeof(rflac);

   /* The allocation size for decoded frames depends on the number of 32-bit
    * integers that fit inside the largest SIMD vector we are supporting.
    */
   if ((init.maxBlockSizeInPCMFrames % (RFLAC_MAX_SIMD_VECTOR_SIZE / sizeof(int32_t))) == 0)
      wholeSIMDVectorCountPerChannel = (init.maxBlockSizeInPCMFrames / (RFLAC_MAX_SIMD_VECTOR_SIZE / sizeof(int32_t)));
   else
      wholeSIMDVectorCountPerChannel = (init.maxBlockSizeInPCMFrames / (RFLAC_MAX_SIMD_VECTOR_SIZE / sizeof(int32_t))) + 1;

   decodedSamplesAllocationSize = wholeSIMDVectorCountPerChannel * RFLAC_MAX_SIMD_VECTOR_SIZE * init.channels;

   /* 32-bit stereo streams need a 64-bit plane for the 33-bit stereo
    * difference channel. */
   wideSamplesAllocationSize = 0;
   if (init.bitsPerSample == 32 && init.channels == 2)
      wideSamplesAllocationSize = init.maxBlockSizeInPCMFrames * (uint32_t)sizeof(int64_t) + 8;

   allocationSize += decodedSamplesAllocationSize;
   allocationSize += wideSamplesAllocationSize;
   /* Allocate extra bytes to ensure we have enough for alignment. */
   allocationSize += RFLAC_MAX_SIMD_VECTOR_SIZE;


   /* This part is a bit awkward. We need to load the seektable so that it can
    * be referenced in-memory, but I want the rflac object to consist of only a
    * single heap allocation. To this, the size of the seek table needs to be
    * known, which we determine when reading and decoding the metadata.
    */
   firstFramePos  = 42;   /* <-- We know we are at byte 42 at this point. */
   seektablePos   = 0;
   seekpointCount = 0;
   if (init.hasMetadataBlocks) {
      rflac_read_proc onReadOverride = onRead;
      rflac_seek_proc onSeekOverride = onSeek;
      void* pUserDataOverride = pUserData;


      if (!rflac__read_and_decode_metadata(onReadOverride, onSeekOverride, onMeta, pUserDataOverride, pUserDataMD, &firstFramePos, &seektablePos, &seekpointCount))
         return NULL;

      allocationSize += seekpointCount * sizeof(rflac_seekpoint);
   }


   pFlac = (rflac*)malloc(allocationSize);
   if (pFlac == NULL)
      return NULL;

   rflac__init_from_info(pFlac, &init);
   pFlac->pDecodedSamples = (int32_t*)RFLAC_ALIGN((size_t)pFlac->pExtraData, RFLAC_MAX_SIMD_VECTOR_SIZE);
   pFlac->pWideSamples     = NULL;
   pFlac->wideChannelIndex = 0xFF;
   if (wideSamplesAllocationSize != 0)
      pFlac->pWideSamples = (int64_t*)RFLAC_ALIGN((size_t)((uint8_t*)pFlac->pDecodedSamples + decodedSamplesAllocationSize), 8);


   pFlac->firstFLACFramePosInBytes = firstFramePos;

   /* NOTE: Seektables are not currently compatible with Ogg encapsulation (Ogg
    * has its own accelerated seeking system). I may change this later, so I'm
    * leaving this here for now. */
   {
      /* If we have a seektable we need to load it now, making sure we move back
       * to where we were previously. */
      if (seektablePos != 0) {
         pFlac->seekpointCount = seekpointCount;
         pFlac->pSeekpoints = (rflac_seekpoint*)((uint8_t*)pFlac->pDecodedSamples + decodedSamplesAllocationSize + wideSamplesAllocationSize);

         /* Seek to the seektable, then just read directly into our seektable
          * buffer. */
         if (pFlac->bs.onSeek(pFlac->bs.pUserData, (int)seektablePos, rflac_seek_origin_start)) {
            uint32_t iSeekpoint;

            for (iSeekpoint = 0; iSeekpoint < seekpointCount; iSeekpoint += 1) {
               if (pFlac->bs.onRead(pFlac->bs.pUserData, pFlac->pSeekpoints + iSeekpoint, RFLAC_SEEKPOINT_SIZE_IN_BYTES) == RFLAC_SEEKPOINT_SIZE_IN_BYTES) {
                  /* Endian swap. */
#ifndef MSB_FIRST
                  pFlac->pSeekpoints[iSeekpoint].firstPCMFrame   = rflac__swap_endian_uint64(pFlac->pSeekpoints[iSeekpoint].firstPCMFrame);
                  pFlac->pSeekpoints[iSeekpoint].flacFrameOffset = rflac__swap_endian_uint64(pFlac->pSeekpoints[iSeekpoint].flacFrameOffset);
#endif
#ifndef MSB_FIRST
                  pFlac->pSeekpoints[iSeekpoint].pcmFrameCount   = rflac__swap_endian_uint16(pFlac->pSeekpoints[iSeekpoint].pcmFrameCount);
#endif
               } else {
                  /* Failed to read the seektable. Pretend we don't have one. */
                  pFlac->pSeekpoints = NULL;
                  pFlac->seekpointCount = 0;
                  break;
               }
            }

            /* We need to seek back to where we were. If this fails it's a
             * critical error. */
            if (!pFlac->bs.onSeek(pFlac->bs.pUserData, (int)pFlac->firstFLACFramePosInBytes, rflac_seek_origin_start)) {
               free(pFlac);
               return NULL;
            }
         } else {
            /* Failed to seek to the seektable. Ominous sign, but for now we can
             * just pretend we don't have one. */
            pFlac->pSeekpoints = NULL;
            pFlac->seekpointCount = 0;
         }
      }
   }

   return pFlac;
}

static INLINE void rflac_read_pcm_frames_s16__decode_left_side__scalar(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

   for (i = 0; i < frameCount4; ++i)
   {
      uint32_t left0 = pInputSamples0U32[i*4+0] << shift0;
      uint32_t left1 = pInputSamples0U32[i*4+1] << shift0;
      uint32_t left2 = pInputSamples0U32[i*4+2] << shift0;
      uint32_t left3 = pInputSamples0U32[i*4+3] << shift0;

      uint32_t side0 = pInputSamples1U32[i*4+0] << shift1;
      uint32_t side1 = pInputSamples1U32[i*4+1] << shift1;
      uint32_t side2 = pInputSamples1U32[i*4+2] << shift1;
      uint32_t side3 = pInputSamples1U32[i*4+3] << shift1;

      uint32_t right0 = left0 - side0;
      uint32_t right1 = left1 - side1;
      uint32_t right2 = left2 - side2;
      uint32_t right3 = left3 - side3;

      left0  >>= 16;
      left1  >>= 16;
      left2  >>= 16;
      left3  >>= 16;

      right0 >>= 16;
      right1 >>= 16;
      right2 >>= 16;
      right3 >>= 16;

      pOutputSamples[i*8+0] = (int16_t)left0;
      pOutputSamples[i*8+1] = (int16_t)right0;
      pOutputSamples[i*8+2] = (int16_t)left1;
      pOutputSamples[i*8+3] = (int16_t)right1;
      pOutputSamples[i*8+4] = (int16_t)left2;
      pOutputSamples[i*8+5] = (int16_t)right2;
      pOutputSamples[i*8+6] = (int16_t)left3;
      pOutputSamples[i*8+7] = (int16_t)right3;
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i)
   {
      uint32_t left  = pInputSamples0U32[i] << shift0;
      uint32_t side  = pInputSamples1U32[i] << shift1;
      uint32_t right = left - side;

      left  >>= 16;
      right >>= 16;

      pOutputSamples[i*2+0] = (int16_t)left;
      pOutputSamples[i*2+1] = (int16_t)right;
   }
}

#if defined(RFLAC_SUPPORT_SSE2)
static INLINE void rflac_read_pcm_frames_s16__decode_left_side__sse2(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

   for (i = 0; i < frameCount4; ++i) {
      __m128i left  = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples0 + i), shift0);
      __m128i side  = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples1 + i), shift1);
      __m128i right = _mm_sub_epi32(left, side);

      left  = _mm_srai_epi32(left,  16);
      right = _mm_srai_epi32(right, 16);

      _mm_storeu_si128((__m128i*)(pOutputSamples + i*8), rflac__mm_packs_interleaved_epi32(left, right));
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      uint32_t left  = pInputSamples0U32[i] << shift0;
      uint32_t side  = pInputSamples1U32[i] << shift1;
      uint32_t right = left - side;

      left  >>= 16;
      right >>= 16;

      pOutputSamples[i*2+0] = (int16_t)left;
      pOutputSamples[i*2+1] = (int16_t)right;
   }
}
#endif

#if defined(RFLAC_SUPPORT_NEON)
static INLINE void rflac_read_pcm_frames_s16__decode_left_side__neon(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
   int32x4_t shift0_4 = vdupq_n_s32(shift0);
   int32x4_t shift1_4 = vdupq_n_s32(shift1);

   for (i = 0; i < frameCount4; ++i)
   {
      uint32x4_t left  = vshlq_u32(vld1q_u32(pInputSamples0U32 + i*4), shift0_4);
      uint32x4_t side  = vshlq_u32(vld1q_u32(pInputSamples1U32 + i*4), shift1_4);
      uint32x4_t right = vsubq_u32(left, side);

      left  = vshrq_n_u32(left,  16);
      right = vshrq_n_u32(right, 16);

      {
         /* interleaving store; replaces vzip + contiguous store */
         uint16x4x2_t v;
         v.val[0] = vmovn_u32(left);
         v.val[1] = vmovn_u32(right);
         vst2_u16((uint16_t*)pOutputSamples + i*8, v);
      }
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i)
   {
      uint32_t left  = pInputSamples0U32[i] << shift0;
      uint32_t side  = pInputSamples1U32[i] << shift1;
      uint32_t right = left - side;

      left  >>= 16;
      right >>= 16;

      pOutputSamples[i*2+0] = (int16_t)left;
      pOutputSamples[i*2+1] = (int16_t)right;
   }
}
#endif

static INLINE void rflac_read_pcm_frames_s16__decode_left_side(rflac* pFlac,
      uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
#if defined(RFLAC_SUPPORT_SSE2)
   if (rflac__gIsSSE2Supported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_s16__decode_left_side__sse2(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#elif defined(RFLAC_SUPPORT_NEON)
   if (rflac__gIsNEONSupported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_s16__decode_left_side__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#endif
   {
      /* Scalar fallback. */
      rflac_read_pcm_frames_s16__decode_left_side__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   }
}

static INLINE void rflac_read_pcm_frames_s16__decode_right_side__scalar(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

   for (i = 0; i < frameCount4; ++i)
   {
      uint32_t side0  = pInputSamples0U32[i*4+0] << shift0;
      uint32_t side1  = pInputSamples0U32[i*4+1] << shift0;
      uint32_t side2  = pInputSamples0U32[i*4+2] << shift0;
      uint32_t side3  = pInputSamples0U32[i*4+3] << shift0;

      uint32_t right0 = pInputSamples1U32[i*4+0] << shift1;
      uint32_t right1 = pInputSamples1U32[i*4+1] << shift1;
      uint32_t right2 = pInputSamples1U32[i*4+2] << shift1;
      uint32_t right3 = pInputSamples1U32[i*4+3] << shift1;

      uint32_t left0 = right0 + side0;
      uint32_t left1 = right1 + side1;
      uint32_t left2 = right2 + side2;
      uint32_t left3 = right3 + side3;

      left0  >>= 16;
      left1  >>= 16;
      left2  >>= 16;
      left3  >>= 16;

      right0 >>= 16;
      right1 >>= 16;
      right2 >>= 16;
      right3 >>= 16;

      pOutputSamples[i*8+0] = (int16_t)left0;
      pOutputSamples[i*8+1] = (int16_t)right0;
      pOutputSamples[i*8+2] = (int16_t)left1;
      pOutputSamples[i*8+3] = (int16_t)right1;
      pOutputSamples[i*8+4] = (int16_t)left2;
      pOutputSamples[i*8+5] = (int16_t)right2;
      pOutputSamples[i*8+6] = (int16_t)left3;
      pOutputSamples[i*8+7] = (int16_t)right3;
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i)
   {
      uint32_t side  = pInputSamples0U32[i] << shift0;
      uint32_t right = pInputSamples1U32[i] << shift1;
      uint32_t left  = right + side;

      left  >>= 16;
      right >>= 16;

      pOutputSamples[i*2+0] = (int16_t)left;
      pOutputSamples[i*2+1] = (int16_t)right;
   }
}

#if defined(RFLAC_SUPPORT_SSE2)
static INLINE void rflac_read_pcm_frames_s16__decode_right_side__sse2(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

   for (i = 0; i < frameCount4; ++i)
   {
      __m128i side  = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples0 + i), shift0);
      __m128i right = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples1 + i), shift1);
      __m128i left  = _mm_add_epi32(right, side);

      left  = _mm_srai_epi32(left,  16);
      right = _mm_srai_epi32(right, 16);

      _mm_storeu_si128((__m128i*)(pOutputSamples + i*8), rflac__mm_packs_interleaved_epi32(left, right));
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      uint32_t side  = pInputSamples0U32[i] << shift0;
      uint32_t right = pInputSamples1U32[i] << shift1;
      uint32_t left  = right + side;

      left  >>= 16;
      right >>= 16;

      pOutputSamples[i*2+0] = (int16_t)left;
      pOutputSamples[i*2+1] = (int16_t)right;
   }
}
#endif

#if defined(RFLAC_SUPPORT_NEON)
static INLINE void rflac_read_pcm_frames_s16__decode_right_side__neon(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
   int32x4_t shift0_4 = vdupq_n_s32(shift0);
   int32x4_t shift1_4 = vdupq_n_s32(shift1);

   for (i = 0; i < frameCount4; ++i)
   {
      uint32x4_t side  = vshlq_u32(vld1q_u32(pInputSamples0U32 + i*4), shift0_4);
      uint32x4_t right = vshlq_u32(vld1q_u32(pInputSamples1U32 + i*4), shift1_4);
      uint32x4_t left  = vaddq_u32(right, side);

      left  = vshrq_n_u32(left,  16);
      right = vshrq_n_u32(right, 16);

      {
         /* interleaving store; replaces vzip + contiguous store */
         uint16x4x2_t v;
         v.val[0] = vmovn_u32(left);
         v.val[1] = vmovn_u32(right);
         vst2_u16((uint16_t*)pOutputSamples + i*8, v);
      }
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      uint32_t side  = pInputSamples0U32[i] << shift0;
      uint32_t right = pInputSamples1U32[i] << shift1;
      uint32_t left  = right + side;

      left  >>= 16;
      right >>= 16;

      pOutputSamples[i*2+0] = (int16_t)left;
      pOutputSamples[i*2+1] = (int16_t)right;
   }
}
#endif

static INLINE void rflac_read_pcm_frames_s16__decode_right_side(rflac* pFlac,
      uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
#if defined(RFLAC_SUPPORT_SSE2)
   if (rflac__gIsSSE2Supported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_s16__decode_right_side__sse2(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#elif defined(RFLAC_SUPPORT_NEON)
   if (rflac__gIsNEONSupported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_s16__decode_right_side__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#endif
   {
      /* Scalar fallback. */
      rflac_read_pcm_frames_s16__decode_right_side__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   }
}

static INLINE void rflac_read_pcm_frames_s16__decode_mid_side__scalar(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift = unusedBitsPerSample;

   if (shift > 0)
   {
      shift -= 1;
      for (i = 0; i < frameCount4; ++i)
      {
         uint32_t temp0L;
         uint32_t temp1L;
         uint32_t temp2L;
         uint32_t temp3L;
         uint32_t temp0R;
         uint32_t temp1R;
         uint32_t temp2R;
         uint32_t temp3R;

         uint32_t mid0  = pInputSamples0U32[i*4+0] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid1  = pInputSamples0U32[i*4+1] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid2  = pInputSamples0U32[i*4+2] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid3  = pInputSamples0U32[i*4+3] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;

         uint32_t side0 = pInputSamples1U32[i*4+0] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side1 = pInputSamples1U32[i*4+1] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side2 = pInputSamples1U32[i*4+2] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side3 = pInputSamples1U32[i*4+3] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid0 = (mid0 << 1) | (side0 & 0x01);
         mid1 = (mid1 << 1) | (side1 & 0x01);
         mid2 = (mid2 << 1) | (side2 & 0x01);
         mid3 = (mid3 << 1) | (side3 & 0x01);

         temp0L = (mid0 + side0) << shift;
         temp1L = (mid1 + side1) << shift;
         temp2L = (mid2 + side2) << shift;
         temp3L = (mid3 + side3) << shift;

         temp0R = (mid0 - side0) << shift;
         temp1R = (mid1 - side1) << shift;
         temp2R = (mid2 - side2) << shift;
         temp3R = (mid3 - side3) << shift;

         temp0L >>= 16;
         temp1L >>= 16;
         temp2L >>= 16;
         temp3L >>= 16;

         temp0R >>= 16;
         temp1R >>= 16;
         temp2R >>= 16;
         temp3R >>= 16;

         pOutputSamples[i*8+0] = (int16_t)temp0L;
         pOutputSamples[i*8+1] = (int16_t)temp0R;
         pOutputSamples[i*8+2] = (int16_t)temp1L;
         pOutputSamples[i*8+3] = (int16_t)temp1R;
         pOutputSamples[i*8+4] = (int16_t)temp2L;
         pOutputSamples[i*8+5] = (int16_t)temp2R;
         pOutputSamples[i*8+6] = (int16_t)temp3L;
         pOutputSamples[i*8+7] = (int16_t)temp3R;
      }
   }
   else
   {
      for (i = 0; i < frameCount4; ++i)
      {
         uint32_t temp0L;
         uint32_t temp1L;
         uint32_t temp2L;
         uint32_t temp3L;
         uint32_t temp0R;
         uint32_t temp1R;
         uint32_t temp2R;
         uint32_t temp3R;

         uint32_t mid0  = pInputSamples0U32[i*4+0] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid1  = pInputSamples0U32[i*4+1] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid2  = pInputSamples0U32[i*4+2] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid3  = pInputSamples0U32[i*4+3] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;

         uint32_t side0 = pInputSamples1U32[i*4+0] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side1 = pInputSamples1U32[i*4+1] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side2 = pInputSamples1U32[i*4+2] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side3 = pInputSamples1U32[i*4+3] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid0 = (mid0 << 1) | (side0 & 0x01);
         mid1 = (mid1 << 1) | (side1 & 0x01);
         mid2 = (mid2 << 1) | (side2 & 0x01);
         mid3 = (mid3 << 1) | (side3 & 0x01);

         temp0L = ((int32_t)(mid0 + side0) >> 1);
         temp1L = ((int32_t)(mid1 + side1) >> 1);
         temp2L = ((int32_t)(mid2 + side2) >> 1);
         temp3L = ((int32_t)(mid3 + side3) >> 1);

         temp0R = ((int32_t)(mid0 - side0) >> 1);
         temp1R = ((int32_t)(mid1 - side1) >> 1);
         temp2R = ((int32_t)(mid2 - side2) >> 1);
         temp3R = ((int32_t)(mid3 - side3) >> 1);

         temp0L >>= 16;
         temp1L >>= 16;
         temp2L >>= 16;
         temp3L >>= 16;

         temp0R >>= 16;
         temp1R >>= 16;
         temp2R >>= 16;
         temp3R >>= 16;

         pOutputSamples[i*8+0] = (int16_t)temp0L;
         pOutputSamples[i*8+1] = (int16_t)temp0R;
         pOutputSamples[i*8+2] = (int16_t)temp1L;
         pOutputSamples[i*8+3] = (int16_t)temp1R;
         pOutputSamples[i*8+4] = (int16_t)temp2L;
         pOutputSamples[i*8+5] = (int16_t)temp2R;
         pOutputSamples[i*8+6] = (int16_t)temp3L;
         pOutputSamples[i*8+7] = (int16_t)temp3R;
      }
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i)
   {
      uint32_t mid  = pInputSamples0U32[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
      uint32_t side = pInputSamples1U32[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

      mid = (mid << 1) | (side & 0x01);

      pOutputSamples[i*2+0] = (int16_t)(((uint32_t)((int32_t)(mid + side) >> 1) << unusedBitsPerSample) >> 16);
      pOutputSamples[i*2+1] = (int16_t)(((uint32_t)((int32_t)(mid - side) >> 1) << unusedBitsPerSample) >> 16);
   }
}

#if defined(RFLAC_SUPPORT_SSE2)
static INLINE void rflac_read_pcm_frames_s16__decode_mid_side__sse2(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift = unusedBitsPerSample;

   if (shift == 0)
   {
      for (i = 0; i < frameCount4; ++i)
      {
         __m128i left;
         __m128i right;
         __m128i mid   = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples0 + i), pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample);
         __m128i side  = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples1 + i), pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample);

         mid   = _mm_or_si128(_mm_slli_epi32(mid, 1), _mm_and_si128(side, _mm_set1_epi32(0x01)));

         left  = _mm_srai_epi32(_mm_add_epi32(mid, side), 1);
         right = _mm_srai_epi32(_mm_sub_epi32(mid, side), 1);

         left  = _mm_srai_epi32(left,  16);
         right = _mm_srai_epi32(right, 16);

         _mm_storeu_si128((__m128i*)(pOutputSamples + i*8), rflac__mm_packs_interleaved_epi32(left, right));
      }

      for (i = (frameCount4 << 2); i < frameCount; ++i) {
         uint32_t mid  = pInputSamples0U32[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t side = pInputSamples1U32[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid = (mid << 1) | (side & 0x01);

         pOutputSamples[i*2+0] = (int16_t)(((int32_t)(mid + side) >> 1) >> 16);
         pOutputSamples[i*2+1] = (int16_t)(((int32_t)(mid - side) >> 1) >> 16);
      }
   }
   else
   {
      shift -= 1;
      for (i = 0; i < frameCount4; ++i)
      {
         __m128i left;
         __m128i right;
         __m128i mid   = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples0 + i), pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample);
         __m128i side  = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples1 + i), pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample);

         mid   = _mm_or_si128(_mm_slli_epi32(mid, 1), _mm_and_si128(side, _mm_set1_epi32(0x01)));

         left  = _mm_slli_epi32(_mm_add_epi32(mid, side), shift);
         right = _mm_slli_epi32(_mm_sub_epi32(mid, side), shift);

         left  = _mm_srai_epi32(left,  16);
         right = _mm_srai_epi32(right, 16);

         _mm_storeu_si128((__m128i*)(pOutputSamples + i*8), rflac__mm_packs_interleaved_epi32(left, right));
      }

      for (i = (frameCount4 << 2); i < frameCount; ++i)
      {
         uint32_t mid  = pInputSamples0U32[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t side = pInputSamples1U32[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid = (mid << 1) | (side & 0x01);

         pOutputSamples[i*2+0] = (int16_t)(((mid + side) << shift) >> 16);
         pOutputSamples[i*2+1] = (int16_t)(((mid - side) << shift) >> 16);
      }
   }
}
#endif

#if defined(RFLAC_SUPPORT_NEON)
static INLINE void rflac_read_pcm_frames_s16__decode_mid_side__neon(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift = unusedBitsPerSample;
   /* wbps = Wasted Bits Per Sample */
   int32x4_t wbpsShift0_4 = vdupq_n_s32(pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample);
   int32x4_t wbpsShift1_4 = vdupq_n_s32(pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample);

   if (shift == 0)
   {
      for (i = 0; i < frameCount4; ++i)
      {
         int32x4_t left;
         int32x4_t right;
         uint32x4_t mid   = vshlq_u32(vld1q_u32(pInputSamples0U32 + i*4), wbpsShift0_4);
         uint32x4_t side  = vshlq_u32(vld1q_u32(pInputSamples1U32 + i*4), wbpsShift1_4);

         mid   = vorrq_u32(vshlq_n_u32(mid, 1), vandq_u32(side, vdupq_n_u32(1)));

         left  = vshrq_n_s32(vreinterpretq_s32_u32(vaddq_u32(mid, side)), 1);
         right = vshrq_n_s32(vreinterpretq_s32_u32(vsubq_u32(mid, side)), 1);

         left  = vshrq_n_s32(left,  16);
         right = vshrq_n_s32(right, 16);

         {
            /* interleaving store; replaces vzip + contiguous store */
            int16x4x2_t v;
            v.val[0] = vmovn_s32(left);
            v.val[1] = vmovn_s32(right);
            vst2_s16(pOutputSamples + i*8, v);
         }
      }

      for (i = (frameCount4 << 2); i < frameCount; ++i)
      {
         uint32_t mid  = pInputSamples0U32[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t side = pInputSamples1U32[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid = (mid << 1) | (side & 0x01);

         pOutputSamples[i*2+0] = (int16_t)(((int32_t)(mid + side) >> 1) >> 16);
         pOutputSamples[i*2+1] = (int16_t)(((int32_t)(mid - side) >> 1) >> 16);
      }
   }
   else
   {
      int32x4_t shift4;

      shift -= 1;
      shift4 = vdupq_n_s32(shift);

      for (i = 0; i < frameCount4; ++i)
      {
         int32x4_t left;
         int32x4_t right;

         uint32x4_t mid   = vshlq_u32(vld1q_u32(pInputSamples0U32 + i*4), wbpsShift0_4);
         uint32x4_t side  = vshlq_u32(vld1q_u32(pInputSamples1U32 + i*4), wbpsShift1_4);

         mid   = vorrq_u32(vshlq_n_u32(mid, 1), vandq_u32(side, vdupq_n_u32(1)));

         left  = vreinterpretq_s32_u32(vshlq_u32(vaddq_u32(mid, side), shift4));
         right = vreinterpretq_s32_u32(vshlq_u32(vsubq_u32(mid, side), shift4));

         left  = vshrq_n_s32(left,  16);
         right = vshrq_n_s32(right, 16);

         {
            /* interleaving store; replaces vzip + contiguous store */
            int16x4x2_t v;
            v.val[0] = vmovn_s32(left);
            v.val[1] = vmovn_s32(right);
            vst2_s16(pOutputSamples + i*8, v);
         }
      }

      for (i = (frameCount4 << 2); i < frameCount; ++i)
      {
         uint32_t mid  = pInputSamples0U32[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t side = pInputSamples1U32[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid = (mid << 1) | (side & 0x01);

         pOutputSamples[i*2+0] = (int16_t)(((mid + side) << shift) >> 16);
         pOutputSamples[i*2+1] = (int16_t)(((mid - side) << shift) >> 16);
      }
   }
}
#endif

static INLINE void rflac_read_pcm_frames_s16__decode_mid_side(rflac* pFlac,
      uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
#if defined(RFLAC_SUPPORT_SSE2)
   if (rflac__gIsSSE2Supported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_s16__decode_mid_side__sse2(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#elif defined(RFLAC_SUPPORT_NEON)
   if (rflac__gIsNEONSupported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_s16__decode_mid_side__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#endif
   {
      /* Scalar fallback. */
      rflac_read_pcm_frames_s16__decode_mid_side__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   }
}

static INLINE void rflac_read_pcm_frames_s16__decode_independent_stereo__scalar(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

   for (i = 0; i < frameCount4; ++i) {
      uint32_t tempL0 = pInputSamples0U32[i*4+0] << shift0;
      uint32_t tempL1 = pInputSamples0U32[i*4+1] << shift0;
      uint32_t tempL2 = pInputSamples0U32[i*4+2] << shift0;
      uint32_t tempL3 = pInputSamples0U32[i*4+3] << shift0;

      uint32_t tempR0 = pInputSamples1U32[i*4+0] << shift1;
      uint32_t tempR1 = pInputSamples1U32[i*4+1] << shift1;
      uint32_t tempR2 = pInputSamples1U32[i*4+2] << shift1;
      uint32_t tempR3 = pInputSamples1U32[i*4+3] << shift1;

      tempL0 >>= 16;
      tempL1 >>= 16;
      tempL2 >>= 16;
      tempL3 >>= 16;

      tempR0 >>= 16;
      tempR1 >>= 16;
      tempR2 >>= 16;
      tempR3 >>= 16;

      pOutputSamples[i*8+0] = (int16_t)tempL0;
      pOutputSamples[i*8+1] = (int16_t)tempR0;
      pOutputSamples[i*8+2] = (int16_t)tempL1;
      pOutputSamples[i*8+3] = (int16_t)tempR1;
      pOutputSamples[i*8+4] = (int16_t)tempL2;
      pOutputSamples[i*8+5] = (int16_t)tempR2;
      pOutputSamples[i*8+6] = (int16_t)tempL3;
      pOutputSamples[i*8+7] = (int16_t)tempR3;
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      pOutputSamples[i*2+0] = (int16_t)((pInputSamples0U32[i] << shift0) >> 16);
      pOutputSamples[i*2+1] = (int16_t)((pInputSamples1U32[i] << shift1) >> 16);
   }
}

#if defined(RFLAC_SUPPORT_SSE2)
static INLINE void rflac_read_pcm_frames_s16__decode_independent_stereo__sse2(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

   for (i = 0; i < frameCount4; ++i) {
      __m128i left  = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples0 + i), shift0);
      __m128i right = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples1 + i), shift1);

      left  = _mm_srai_epi32(left,  16);
      right = _mm_srai_epi32(right, 16);

      /* At this point we have results. We can now pack and interleave these
       * into a single __m128i object and then store the in the output buffer. */
      _mm_storeu_si128((__m128i*)(pOutputSamples + i*8), rflac__mm_packs_interleaved_epi32(left, right));
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      pOutputSamples[i*2+0] = (int16_t)((pInputSamples0U32[i] << shift0) >> 16);
      pOutputSamples[i*2+1] = (int16_t)((pInputSamples1U32[i] << shift1) >> 16);
   }
}
#endif

#if defined(RFLAC_SUPPORT_NEON)
static INLINE void rflac_read_pcm_frames_s16__decode_independent_stereo__neon(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0    = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1    = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
   int32x4_t shift0_4 = vdupq_n_s32(shift0);
   int32x4_t shift1_4 = vdupq_n_s32(shift1);

   for (i = 0; i < frameCount4; ++i) {
      int32x4_t left;
      int32x4_t right;

      left  = vreinterpretq_s32_u32(vshlq_u32(vld1q_u32(pInputSamples0U32 + i*4), shift0_4));
      right = vreinterpretq_s32_u32(vshlq_u32(vld1q_u32(pInputSamples1U32 + i*4), shift1_4));

      left  = vshrq_n_s32(left,  16);
      right = vshrq_n_s32(right, 16);

      {
         /* interleaving store; replaces vzip + contiguous store */
         int16x4x2_t v;
         v.val[0] = vmovn_s32(left);
         v.val[1] = vmovn_s32(right);
         vst2_s16(pOutputSamples + i*8, v);
      }
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i)
   {
      pOutputSamples[i*2+0] = (int16_t)((pInputSamples0U32[i] << shift0) >> 16);
      pOutputSamples[i*2+1] = (int16_t)((pInputSamples1U32[i] << shift1) >> 16);
   }
}
#endif

static INLINE void rflac_read_pcm_frames_s16__decode_independent_stereo(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      int16_t* pOutputSamples)
{
#if defined(RFLAC_SUPPORT_SSE2)
   if (rflac__gIsSSE2Supported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_s16__decode_independent_stereo__sse2(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#elif defined(RFLAC_SUPPORT_NEON)
   if (rflac__gIsNEONSupported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_s16__decode_independent_stereo__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#endif
   {
      /* Scalar fallback. */
      rflac_read_pcm_frames_s16__decode_independent_stereo__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   }
}


/* Wide (33-bit side channel) reconstruction for 32-bit streams. Scalar
 * only; unusedBitsPerSample is always 0 here (bitsPerSample == 32), so
 * only the wasted-bit shifts apply and samples work in natural units. */

static void rflac_read_pcm_frames_s16__decode_left_side__wide(rflac* pFlac,
      uint64_t frameCount, const int32_t* pInLeft, const int64_t* pInSide,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint32_t shiftL = pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shiftS = pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

   for (i = 0; i < frameCount; ++i)
   {
      int64_t left  = (int64_t)pInLeft[i] << shiftL;
      int64_t side  = pInSide[i] << shiftS;
      int64_t right = left - side;

      pOutputSamples[i*2+0] = (int16_t)(left  >> 16);
      pOutputSamples[i*2+1] = (int16_t)(right >> 16);
   }
}

static void rflac_read_pcm_frames_s16__decode_right_side__wide(rflac* pFlac,
      uint64_t frameCount, const int64_t* pInSide, const int32_t* pInRight,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint32_t shiftS = pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shiftR = pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

   for (i = 0; i < frameCount; ++i)
   {
      int64_t side  = pInSide[i] << shiftS;
      int64_t right = (int64_t)pInRight[i] << shiftR;
      int64_t left  = right + side;

      pOutputSamples[i*2+0] = (int16_t)(left  >> 16);
      pOutputSamples[i*2+1] = (int16_t)(right >> 16);
   }
}

static void rflac_read_pcm_frames_s16__decode_mid_side__wide(rflac* pFlac,
      uint64_t frameCount, const int32_t* pInMid, const int64_t* pInSide,
      int16_t* pOutputSamples)
{
   uint64_t i;
   uint32_t shiftM = pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shiftS = pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

   for (i = 0; i < frameCount; ++i)
   {
      int64_t mid  = (int64_t)pInMid[i] << shiftM;
      int64_t side = pInSide[i] << shiftS;
      int64_t left;
      int64_t right;

      mid   = (mid << 1) | (side & 0x01);
      left  = (mid + side) >> 1;
      right = (mid - side) >> 1;

      pOutputSamples[i*2+0] = (int16_t)(left  >> 16);
      pOutputSamples[i*2+1] = (int16_t)(right >> 16);
   }
}

static void rflac_read_pcm_frames_f32__decode_left_side__wide(rflac* pFlac,
      uint64_t frameCount, const int32_t* pInLeft, const int64_t* pInSide,
      float* pOutputSamples)
{
   uint64_t i;
   uint32_t shiftL = pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shiftS = pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
   float factor = 1 / 2147483648.0;

   for (i = 0; i < frameCount; ++i)
   {
      int64_t left  = (int64_t)pInLeft[i] << shiftL;
      int64_t side  = pInSide[i] << shiftS;
      int64_t right = left - side;

      pOutputSamples[i*2+0] = (int32_t)left  * factor;
      pOutputSamples[i*2+1] = (int32_t)right * factor;
   }
}

static void rflac_read_pcm_frames_f32__decode_right_side__wide(rflac* pFlac,
      uint64_t frameCount, const int64_t* pInSide, const int32_t* pInRight,
      float* pOutputSamples)
{
   uint64_t i;
   uint32_t shiftS = pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shiftR = pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
   float factor = 1 / 2147483648.0;

   for (i = 0; i < frameCount; ++i)
   {
      int64_t side  = pInSide[i] << shiftS;
      int64_t right = (int64_t)pInRight[i] << shiftR;
      int64_t left  = right + side;

      pOutputSamples[i*2+0] = (int32_t)left  * factor;
      pOutputSamples[i*2+1] = (int32_t)right * factor;
   }
}

static void rflac_read_pcm_frames_f32__decode_mid_side__wide(rflac* pFlac,
      uint64_t frameCount, const int32_t* pInMid, const int64_t* pInSide,
      float* pOutputSamples)
{
   uint64_t i;
   uint32_t shiftM = pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shiftS = pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
   float factor = 1 / 2147483648.0;

   for (i = 0; i < frameCount; ++i)
   {
      int64_t mid  = (int64_t)pInMid[i] << shiftM;
      int64_t side = pInSide[i] << shiftS;
      int64_t left;
      int64_t right;

      mid   = (mid << 1) | (side & 0x01);
      left  = (mid + side) >> 1;
      right = (mid - side) >> 1;

      pOutputSamples[i*2+0] = (int32_t)left  * factor;
      pOutputSamples[i*2+1] = (int32_t)right * factor;
   }
}

static uint64_t rflac_read_pcm_frames_s16(rflac* pFlac, uint64_t framesToRead,
      int16_t* pBufferOut)
{
   uint64_t framesRead;
   uint32_t unusedBitsPerSample;

   if (pFlac == NULL || framesToRead == 0)
      return 0;

   if (pBufferOut == NULL)
      return rflac__seek_forward_by_pcm_frames(pFlac, framesToRead);

   unusedBitsPerSample = 32 - pFlac->bitsPerSample;

   framesRead = 0;
   while (framesToRead > 0)
   {
      /* If we've run out of samples in this frame, go to the next. */
      if (pFlac->currentFLACFrame.pcmFramesRemaining == 0)
      {
         if (!rflac__read_and_decode_next_flac_frame(pFlac))
            /* Couldn't read the next frame, so just break from the loop and
             * return. */
            break;
      }
      else
      {
         unsigned int channelCount = rflac__get_channel_count_from_channel_assignment(pFlac->currentFLACFrame.header.channelAssignment);
         uint64_t iFirstPCMFrame = pFlac->currentFLACFrame.header.blockSizeInPCMFrames - pFlac->currentFLACFrame.pcmFramesRemaining;
         uint64_t frameCountThisIteration = framesToRead;

         if (frameCountThisIteration > pFlac->currentFLACFrame.pcmFramesRemaining)
            frameCountThisIteration = pFlac->currentFLACFrame.pcmFramesRemaining;

         if (channelCount == 2) {
            const int32_t* pDecodedSamples0 = pFlac->currentFLACFrame.subframes[0].pSamplesS32 + iFirstPCMFrame;
            const int32_t* pDecodedSamples1 = pFlac->currentFLACFrame.subframes[1].pSamplesS32 + iFirstPCMFrame;

            switch (pFlac->currentFLACFrame.header.channelAssignment)
            {
               case RFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE:
                  {
                     if (pFlac->wideChannelIndex == 1)
                        rflac_read_pcm_frames_s16__decode_left_side__wide(pFlac, frameCountThisIteration, pDecodedSamples0, pFlac->pWideSamples + iFirstPCMFrame, pBufferOut);
                     else
                        rflac_read_pcm_frames_s16__decode_left_side(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
                  } break;

               case RFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE:
                  {
                     if (pFlac->wideChannelIndex == 0)
                        rflac_read_pcm_frames_s16__decode_right_side__wide(pFlac, frameCountThisIteration, pFlac->pWideSamples + iFirstPCMFrame, pDecodedSamples1, pBufferOut);
                     else
                        rflac_read_pcm_frames_s16__decode_right_side(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
                  } break;

               case RFLAC_CHANNEL_ASSIGNMENT_MID_SIDE:
                  {
                     if (pFlac->wideChannelIndex == 1)
                        rflac_read_pcm_frames_s16__decode_mid_side__wide(pFlac, frameCountThisIteration, pDecodedSamples0, pFlac->pWideSamples + iFirstPCMFrame, pBufferOut);
                     else
                        rflac_read_pcm_frames_s16__decode_mid_side(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
                  } break;

               case RFLAC_CHANNEL_ASSIGNMENT_INDEPENDENT:
               default:
                  {
                     rflac_read_pcm_frames_s16__decode_independent_stereo(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
                  } break;
            }
         } else {
            /* Generic interleaving. */
            uint64_t i;
            for (i = 0; i < frameCountThisIteration; ++i) {
               unsigned int j;
               for (j = 0; j < channelCount; ++j) {
                  int32_t sampleS32 = (int32_t)((uint32_t)(pFlac->currentFLACFrame.subframes[j].pSamplesS32[iFirstPCMFrame + i]) << (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[j].wastedBitsPerSample));
                  pBufferOut[(i*channelCount)+j] = (int16_t)(sampleS32 >> 16);
               }
            }
         }

         framesRead                += frameCountThisIteration;
         pBufferOut                += frameCountThisIteration * channelCount;
         framesToRead              -= frameCountThisIteration;
         pFlac->currentPCMFrame    += frameCountThisIteration;
         pFlac->currentFLACFrame.pcmFramesRemaining -= (uint32_t)frameCountThisIteration;
      }
   }

   return framesRead;
}

static INLINE void rflac_read_pcm_frames_f32__decode_left_side__scalar(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

   float factor = 1 / 2147483648.0;

   for (i = 0; i < frameCount4; ++i) {
      uint32_t left0 = pInputSamples0U32[i*4+0] << shift0;
      uint32_t left1 = pInputSamples0U32[i*4+1] << shift0;
      uint32_t left2 = pInputSamples0U32[i*4+2] << shift0;
      uint32_t left3 = pInputSamples0U32[i*4+3] << shift0;

      uint32_t side0 = pInputSamples1U32[i*4+0] << shift1;
      uint32_t side1 = pInputSamples1U32[i*4+1] << shift1;
      uint32_t side2 = pInputSamples1U32[i*4+2] << shift1;
      uint32_t side3 = pInputSamples1U32[i*4+3] << shift1;

      uint32_t right0 = left0 - side0;
      uint32_t right1 = left1 - side1;
      uint32_t right2 = left2 - side2;
      uint32_t right3 = left3 - side3;

      pOutputSamples[i*8+0] = (int32_t)left0  * factor;
      pOutputSamples[i*8+1] = (int32_t)right0 * factor;
      pOutputSamples[i*8+2] = (int32_t)left1  * factor;
      pOutputSamples[i*8+3] = (int32_t)right1 * factor;
      pOutputSamples[i*8+4] = (int32_t)left2  * factor;
      pOutputSamples[i*8+5] = (int32_t)right2 * factor;
      pOutputSamples[i*8+6] = (int32_t)left3  * factor;
      pOutputSamples[i*8+7] = (int32_t)right3 * factor;
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      uint32_t left  = pInputSamples0U32[i] << shift0;
      uint32_t side  = pInputSamples1U32[i] << shift1;
      uint32_t right = left - side;

      pOutputSamples[i*2+0] = (int32_t)left  * factor;
      pOutputSamples[i*2+1] = (int32_t)right * factor;
   }
}

#if defined(RFLAC_SUPPORT_SSE2)
static INLINE void rflac_read_pcm_frames_f32__decode_left_side__sse2(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample) - 8;
   uint32_t shift1 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample) - 8;
   __m128 factor = _mm_set1_ps(1.0f / 8388608.0f);

   for (i = 0; i < frameCount4; ++i)
   {
      __m128i left  = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples0 + i), shift0);
      __m128i side  = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples1 + i), shift1);
      __m128i right = _mm_sub_epi32(left, side);
      __m128 leftf  = _mm_mul_ps(_mm_cvtepi32_ps(left),  factor);
      __m128 rightf = _mm_mul_ps(_mm_cvtepi32_ps(right), factor);

      _mm_storeu_ps(pOutputSamples + i*8 + 0, _mm_unpacklo_ps(leftf, rightf));
      _mm_storeu_ps(pOutputSamples + i*8 + 4, _mm_unpackhi_ps(leftf, rightf));
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i)
   {
      uint32_t left  = pInputSamples0U32[i] << shift0;
      uint32_t side  = pInputSamples1U32[i] << shift1;
      uint32_t right = left - side;

      pOutputSamples[i*2+0] = (int32_t)left  / 8388608.0f;
      pOutputSamples[i*2+1] = (int32_t)right / 8388608.0f;
   }
}
#endif

#if defined(RFLAC_SUPPORT_NEON)
static INLINE void rflac_read_pcm_frames_f32__decode_left_side__neon(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample) - 8;
   uint32_t shift1 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample) - 8;
   float32x4_t factor4  = vdupq_n_f32(1.0f / 8388608.0f);
   int32x4_t shift0_4   = vdupq_n_s32(shift0);
   int32x4_t shift1_4   = vdupq_n_s32(shift1);

   for (i = 0; i < frameCount4; ++i)
   {
      uint32x4_t left    = vshlq_u32(vld1q_u32(pInputSamples0U32 + i*4), shift0_4);
      uint32x4_t side    = vshlq_u32(vld1q_u32(pInputSamples1U32 + i*4), shift1_4);
      uint32x4_t right   = vsubq_u32(left, side);
      float32x4_t leftf  = vmulq_f32(vcvtq_f32_s32(vreinterpretq_s32_u32(left)),  factor4);
      float32x4_t rightf = vmulq_f32(vcvtq_f32_s32(vreinterpretq_s32_u32(right)), factor4);

      {
         /* interleaving store; replaces vzip + contiguous store */
         float32x4x2_t v;
         v.val[0] = leftf;
         v.val[1] = rightf;
         vst2q_f32(pOutputSamples + i*8, v);
      }
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i)
   {
      uint32_t left  = pInputSamples0U32[i] << shift0;
      uint32_t side  = pInputSamples1U32[i] << shift1;
      uint32_t right = left - side;

      pOutputSamples[i*2+0] = (int32_t)left  / 8388608.0f;
      pOutputSamples[i*2+1] = (int32_t)right / 8388608.0f;
   }
}
#endif

static INLINE void rflac_read_pcm_frames_f32__decode_left_side(rflac* pFlac,
      uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
#if defined(RFLAC_SUPPORT_SSE2)
   if (rflac__gIsSSE2Supported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_f32__decode_left_side__sse2(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#elif defined(RFLAC_SUPPORT_NEON)
   if (rflac__gIsNEONSupported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_f32__decode_left_side__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#endif
   {
      /* Scalar fallback. */
      rflac_read_pcm_frames_f32__decode_left_side__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   }
}

static INLINE void rflac_read_pcm_frames_f32__decode_right_side__scalar(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
   float factor = 1 / 2147483648.0;

   for (i = 0; i < frameCount4; ++i) {
      uint32_t side0  = pInputSamples0U32[i*4+0] << shift0;
      uint32_t side1  = pInputSamples0U32[i*4+1] << shift0;
      uint32_t side2  = pInputSamples0U32[i*4+2] << shift0;
      uint32_t side3  = pInputSamples0U32[i*4+3] << shift0;

      uint32_t right0 = pInputSamples1U32[i*4+0] << shift1;
      uint32_t right1 = pInputSamples1U32[i*4+1] << shift1;
      uint32_t right2 = pInputSamples1U32[i*4+2] << shift1;
      uint32_t right3 = pInputSamples1U32[i*4+3] << shift1;

      uint32_t left0 = right0 + side0;
      uint32_t left1 = right1 + side1;
      uint32_t left2 = right2 + side2;
      uint32_t left3 = right3 + side3;

      pOutputSamples[i*8+0] = (int32_t)left0  * factor;
      pOutputSamples[i*8+1] = (int32_t)right0 * factor;
      pOutputSamples[i*8+2] = (int32_t)left1  * factor;
      pOutputSamples[i*8+3] = (int32_t)right1 * factor;
      pOutputSamples[i*8+4] = (int32_t)left2  * factor;
      pOutputSamples[i*8+5] = (int32_t)right2 * factor;
      pOutputSamples[i*8+6] = (int32_t)left3  * factor;
      pOutputSamples[i*8+7] = (int32_t)right3 * factor;
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i)
   {
      uint32_t side  = pInputSamples0U32[i] << shift0;
      uint32_t right = pInputSamples1U32[i] << shift1;
      uint32_t left  = right + side;

      pOutputSamples[i*2+0] = (int32_t)left  * factor;
      pOutputSamples[i*2+1] = (int32_t)right * factor;
   }
}

#if defined(RFLAC_SUPPORT_SSE2)
static INLINE void rflac_read_pcm_frames_f32__decode_right_side__sse2(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample) - 8;
   uint32_t shift1 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample) - 8;
   __m128 factor;

   factor = _mm_set1_ps(1.0f / 8388608.0f);

   for (i = 0; i < frameCount4; ++i)
   {
      __m128i side  = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples0 + i), shift0);
      __m128i right = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples1 + i), shift1);
      __m128i left  = _mm_add_epi32(right, side);
      __m128 leftf  = _mm_mul_ps(_mm_cvtepi32_ps(left),  factor);
      __m128 rightf = _mm_mul_ps(_mm_cvtepi32_ps(right), factor);

      _mm_storeu_ps(pOutputSamples + i*8 + 0, _mm_unpacklo_ps(leftf, rightf));
      _mm_storeu_ps(pOutputSamples + i*8 + 4, _mm_unpackhi_ps(leftf, rightf));
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      uint32_t side  = pInputSamples0U32[i] << shift0;
      uint32_t right = pInputSamples1U32[i] << shift1;
      uint32_t left  = right + side;

      pOutputSamples[i*2+0] = (int32_t)left  / 8388608.0f;
      pOutputSamples[i*2+1] = (int32_t)right / 8388608.0f;
   }
}
#endif

#if defined(RFLAC_SUPPORT_NEON)
static INLINE void rflac_read_pcm_frames_f32__decode_right_side__neon(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample) - 8;
   uint32_t shift1 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample) - 8;
   float32x4_t factor4 = vdupq_n_f32(1.0f / 8388608.0f);
   int32x4_t shift0_4  = vdupq_n_s32(shift0);
   int32x4_t shift1_4  = vdupq_n_s32(shift1);

   for (i = 0; i < frameCount4; ++i) {
      uint32x4_t side;
      uint32x4_t right;
      uint32x4_t left;
      float32x4_t leftf;
      float32x4_t rightf;

      side   = vshlq_u32(vld1q_u32(pInputSamples0U32 + i*4), shift0_4);
      right  = vshlq_u32(vld1q_u32(pInputSamples1U32 + i*4), shift1_4);
      left   = vaddq_u32(right, side);
      leftf  = vmulq_f32(vcvtq_f32_s32(vreinterpretq_s32_u32(left)),  factor4);
      rightf = vmulq_f32(vcvtq_f32_s32(vreinterpretq_s32_u32(right)), factor4);

      {
         /* interleaving store; replaces vzip + contiguous store */
         float32x4x2_t v;
         v.val[0] = leftf;
         v.val[1] = rightf;
         vst2q_f32(pOutputSamples + i*8, v);
      }
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      uint32_t side  = pInputSamples0U32[i] << shift0;
      uint32_t right = pInputSamples1U32[i] << shift1;
      uint32_t left  = right + side;

      pOutputSamples[i*2+0] = (int32_t)left  / 8388608.0f;
      pOutputSamples[i*2+1] = (int32_t)right / 8388608.0f;
   }
}
#endif

static INLINE void rflac_read_pcm_frames_f32__decode_right_side(rflac* pFlac,
      uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
#if defined(RFLAC_SUPPORT_SSE2)
   if (rflac__gIsSSE2Supported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_f32__decode_right_side__sse2(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#elif defined(RFLAC_SUPPORT_NEON)
   if (rflac__gIsNEONSupported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_f32__decode_right_side__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#endif
   {
      /* Scalar fallback. */
      rflac_read_pcm_frames_f32__decode_right_side__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   }
}

static INLINE void rflac_read_pcm_frames_f32__decode_mid_side__scalar(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift = unusedBitsPerSample;
   float factor = 1 / 2147483648.0;

   if (shift > 0) {
      shift -= 1;
      for (i = 0; i < frameCount4; ++i) {
         uint32_t temp0L;
         uint32_t temp1L;
         uint32_t temp2L;
         uint32_t temp3L;
         uint32_t temp0R;
         uint32_t temp1R;
         uint32_t temp2R;
         uint32_t temp3R;

         uint32_t mid0  = pInputSamples0U32[i*4+0] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid1  = pInputSamples0U32[i*4+1] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid2  = pInputSamples0U32[i*4+2] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid3  = pInputSamples0U32[i*4+3] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;

         uint32_t side0 = pInputSamples1U32[i*4+0] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side1 = pInputSamples1U32[i*4+1] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side2 = pInputSamples1U32[i*4+2] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side3 = pInputSamples1U32[i*4+3] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid0 = (mid0 << 1) | (side0 & 0x01);
         mid1 = (mid1 << 1) | (side1 & 0x01);
         mid2 = (mid2 << 1) | (side2 & 0x01);
         mid3 = (mid3 << 1) | (side3 & 0x01);

         temp0L = (mid0 + side0) << shift;
         temp1L = (mid1 + side1) << shift;
         temp2L = (mid2 + side2) << shift;
         temp3L = (mid3 + side3) << shift;

         temp0R = (mid0 - side0) << shift;
         temp1R = (mid1 - side1) << shift;
         temp2R = (mid2 - side2) << shift;
         temp3R = (mid3 - side3) << shift;

         pOutputSamples[i*8+0] = (int32_t)temp0L * factor;
         pOutputSamples[i*8+1] = (int32_t)temp0R * factor;
         pOutputSamples[i*8+2] = (int32_t)temp1L * factor;
         pOutputSamples[i*8+3] = (int32_t)temp1R * factor;
         pOutputSamples[i*8+4] = (int32_t)temp2L * factor;
         pOutputSamples[i*8+5] = (int32_t)temp2R * factor;
         pOutputSamples[i*8+6] = (int32_t)temp3L * factor;
         pOutputSamples[i*8+7] = (int32_t)temp3R * factor;
      }
   } else {
      for (i = 0; i < frameCount4; ++i) {
         uint32_t temp0L;
         uint32_t temp1L;
         uint32_t temp2L;
         uint32_t temp3L;
         uint32_t temp0R;
         uint32_t temp1R;
         uint32_t temp2R;
         uint32_t temp3R;

         uint32_t mid0  = pInputSamples0U32[i*4+0] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid1  = pInputSamples0U32[i*4+1] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid2  = pInputSamples0U32[i*4+2] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t mid3  = pInputSamples0U32[i*4+3] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;

         uint32_t side0 = pInputSamples1U32[i*4+0] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side1 = pInputSamples1U32[i*4+1] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side2 = pInputSamples1U32[i*4+2] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
         uint32_t side3 = pInputSamples1U32[i*4+3] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid0 = (mid0 << 1) | (side0 & 0x01);
         mid1 = (mid1 << 1) | (side1 & 0x01);
         mid2 = (mid2 << 1) | (side2 & 0x01);
         mid3 = (mid3 << 1) | (side3 & 0x01);

         temp0L = (uint32_t)((int32_t)(mid0 + side0) >> 1);
         temp1L = (uint32_t)((int32_t)(mid1 + side1) >> 1);
         temp2L = (uint32_t)((int32_t)(mid2 + side2) >> 1);
         temp3L = (uint32_t)((int32_t)(mid3 + side3) >> 1);

         temp0R = (uint32_t)((int32_t)(mid0 - side0) >> 1);
         temp1R = (uint32_t)((int32_t)(mid1 - side1) >> 1);
         temp2R = (uint32_t)((int32_t)(mid2 - side2) >> 1);
         temp3R = (uint32_t)((int32_t)(mid3 - side3) >> 1);

         pOutputSamples[i*8+0] = (int32_t)temp0L * factor;
         pOutputSamples[i*8+1] = (int32_t)temp0R * factor;
         pOutputSamples[i*8+2] = (int32_t)temp1L * factor;
         pOutputSamples[i*8+3] = (int32_t)temp1R * factor;
         pOutputSamples[i*8+4] = (int32_t)temp2L * factor;
         pOutputSamples[i*8+5] = (int32_t)temp2R * factor;
         pOutputSamples[i*8+6] = (int32_t)temp3L * factor;
         pOutputSamples[i*8+7] = (int32_t)temp3R * factor;
      }
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      uint32_t mid  = pInputSamples0U32[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
      uint32_t side = pInputSamples1U32[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

      mid = (mid << 1) | (side & 0x01);

      pOutputSamples[i*2+0] = (int32_t)((uint32_t)((int32_t)(mid + side) >> 1) << unusedBitsPerSample) * factor;
      pOutputSamples[i*2+1] = (int32_t)((uint32_t)((int32_t)(mid - side) >> 1) << unusedBitsPerSample) * factor;
   }
}

#if defined(RFLAC_SUPPORT_SSE2)
static INLINE void rflac_read_pcm_frames_f32__decode_mid_side__sse2(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift = unusedBitsPerSample - 8;
   float factor;
   __m128 factor128;

   factor = 1.0f / 8388608.0f;
   factor128 = _mm_set1_ps(factor);

   if (shift == 0) {
      for (i = 0; i < frameCount4; ++i) {
         __m128i mid;
         __m128i side;
         __m128i tempL;
         __m128i tempR;
         __m128  leftf;
         __m128  rightf;

         mid    = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples0 + i), pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample);
         side   = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples1 + i), pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample);

         mid    = _mm_or_si128(_mm_slli_epi32(mid, 1), _mm_and_si128(side, _mm_set1_epi32(0x01)));

         tempL  = _mm_srai_epi32(_mm_add_epi32(mid, side), 1);
         tempR  = _mm_srai_epi32(_mm_sub_epi32(mid, side), 1);

         leftf  = _mm_mul_ps(_mm_cvtepi32_ps(tempL), factor128);
         rightf = _mm_mul_ps(_mm_cvtepi32_ps(tempR), factor128);

         _mm_storeu_ps(pOutputSamples + i*8 + 0, _mm_unpacklo_ps(leftf, rightf));
         _mm_storeu_ps(pOutputSamples + i*8 + 4, _mm_unpackhi_ps(leftf, rightf));
      }

      for (i = (frameCount4 << 2); i < frameCount; ++i) {
         uint32_t mid  = pInputSamples0U32[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t side = pInputSamples1U32[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid = (mid << 1) | (side & 0x01);

         pOutputSamples[i*2+0] = ((int32_t)(mid + side) >> 1) * factor;
         pOutputSamples[i*2+1] = ((int32_t)(mid - side) >> 1) * factor;
      }
   } else {
      shift -= 1;
      for (i = 0; i < frameCount4; ++i) {
         __m128i mid;
         __m128i side;
         __m128i tempL;
         __m128i tempR;
         __m128 leftf;
         __m128 rightf;

         mid    = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples0 + i), pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample);
         side   = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples1 + i), pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample);

         mid    = _mm_or_si128(_mm_slli_epi32(mid, 1), _mm_and_si128(side, _mm_set1_epi32(0x01)));

         tempL  = _mm_slli_epi32(_mm_add_epi32(mid, side), shift);
         tempR  = _mm_slli_epi32(_mm_sub_epi32(mid, side), shift);

         leftf  = _mm_mul_ps(_mm_cvtepi32_ps(tempL), factor128);
         rightf = _mm_mul_ps(_mm_cvtepi32_ps(tempR), factor128);

         _mm_storeu_ps(pOutputSamples + i*8 + 0, _mm_unpacklo_ps(leftf, rightf));
         _mm_storeu_ps(pOutputSamples + i*8 + 4, _mm_unpackhi_ps(leftf, rightf));
      }

      for (i = (frameCount4 << 2); i < frameCount; ++i) {
         uint32_t mid  = pInputSamples0U32[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t side = pInputSamples1U32[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid = (mid << 1) | (side & 0x01);

         pOutputSamples[i*2+0] = (int32_t)((mid + side) << shift) * factor;
         pOutputSamples[i*2+1] = (int32_t)((mid - side) << shift) * factor;
      }
   }
}
#endif

#if defined(RFLAC_SUPPORT_NEON)
static INLINE void rflac_read_pcm_frames_f32__decode_mid_side__neon(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   int32x4_t shift4;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift = unusedBitsPerSample - 8;
   float factor        = 1.0f / 8388608.0f;
   float32x4_t factor4 = vdupq_n_f32(factor);
   int32x4_t wbps0_4   = vdupq_n_s32(pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample);
   int32x4_t wbps1_4   = vdupq_n_s32(pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample);

   if (shift == 0) {
      for (i = 0; i < frameCount4; ++i) {
         int32x4_t lefti;
         int32x4_t righti;
         float32x4_t leftf;
         float32x4_t rightf;

         uint32x4_t mid  = vshlq_u32(vld1q_u32(pInputSamples0U32 + i*4), wbps0_4);
         uint32x4_t side = vshlq_u32(vld1q_u32(pInputSamples1U32 + i*4), wbps1_4);

         mid    = vorrq_u32(vshlq_n_u32(mid, 1), vandq_u32(side, vdupq_n_u32(1)));

         lefti  = vshrq_n_s32(vreinterpretq_s32_u32(vaddq_u32(mid, side)), 1);
         righti = vshrq_n_s32(vreinterpretq_s32_u32(vsubq_u32(mid, side)), 1);

         leftf  = vmulq_f32(vcvtq_f32_s32(lefti),  factor4);
         rightf = vmulq_f32(vcvtq_f32_s32(righti), factor4);

         {
            /* interleaving store; replaces vzip + contiguous store */
            float32x4x2_t v;
            v.val[0] = leftf;
            v.val[1] = rightf;
            vst2q_f32(pOutputSamples + i*8, v);
         }
      }

      for (i = (frameCount4 << 2); i < frameCount; ++i) {
         uint32_t mid  = pInputSamples0U32[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t side = pInputSamples1U32[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid = (mid << 1) | (side & 0x01);

         pOutputSamples[i*2+0] = ((int32_t)(mid + side) >> 1) * factor;
         pOutputSamples[i*2+1] = ((int32_t)(mid - side) >> 1) * factor;
      }
   } else {
      shift -= 1;
      shift4 = vdupq_n_s32(shift);
      for (i = 0; i < frameCount4; ++i) {
         uint32x4_t mid;
         uint32x4_t side;
         int32x4_t lefti;
         int32x4_t righti;
         float32x4_t leftf;
         float32x4_t rightf;

         mid    = vshlq_u32(vld1q_u32(pInputSamples0U32 + i*4), wbps0_4);
         side   = vshlq_u32(vld1q_u32(pInputSamples1U32 + i*4), wbps1_4);

         mid    = vorrq_u32(vshlq_n_u32(mid, 1), vandq_u32(side, vdupq_n_u32(1)));

         lefti  = vreinterpretq_s32_u32(vshlq_u32(vaddq_u32(mid, side), shift4));
         righti = vreinterpretq_s32_u32(vshlq_u32(vsubq_u32(mid, side), shift4));

         leftf  = vmulq_f32(vcvtq_f32_s32(lefti),  factor4);
         rightf = vmulq_f32(vcvtq_f32_s32(righti), factor4);

         {
            /* interleaving store; replaces vzip + contiguous store */
            float32x4x2_t v;
            v.val[0] = leftf;
            v.val[1] = rightf;
            vst2q_f32(pOutputSamples + i*8, v);
         }
      }

      for (i = (frameCount4 << 2); i < frameCount; ++i) {
         uint32_t mid  = pInputSamples0U32[i] << pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
         uint32_t side = pInputSamples1U32[i] << pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;

         mid = (mid << 1) | (side & 0x01);

         pOutputSamples[i*2+0] = (int32_t)((mid + side) << shift) * factor;
         pOutputSamples[i*2+1] = (int32_t)((mid - side) << shift) * factor;
      }
   }
}
#endif

static INLINE void rflac_read_pcm_frames_f32__decode_mid_side(rflac* pFlac,
      uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
#if defined(RFLAC_SUPPORT_SSE2)
   if (rflac__gIsSSE2Supported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_f32__decode_mid_side__sse2(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#elif defined(RFLAC_SUPPORT_NEON)
   if (rflac__gIsNEONSupported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_f32__decode_mid_side__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#endif
   {
      /* Scalar fallback. */
      rflac_read_pcm_frames_f32__decode_mid_side__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   }
}

static INLINE void rflac_read_pcm_frames_f32__decode_independent_stereo__scalar(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample;
   uint32_t shift1 = unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample;
   float factor = 1 / 2147483648.0;

   for (i = 0; i < frameCount4; ++i) {
      uint32_t tempL0 = pInputSamples0U32[i*4+0] << shift0;
      uint32_t tempL1 = pInputSamples0U32[i*4+1] << shift0;
      uint32_t tempL2 = pInputSamples0U32[i*4+2] << shift0;
      uint32_t tempL3 = pInputSamples0U32[i*4+3] << shift0;

      uint32_t tempR0 = pInputSamples1U32[i*4+0] << shift1;
      uint32_t tempR1 = pInputSamples1U32[i*4+1] << shift1;
      uint32_t tempR2 = pInputSamples1U32[i*4+2] << shift1;
      uint32_t tempR3 = pInputSamples1U32[i*4+3] << shift1;

      pOutputSamples[i*8+0] = (int32_t)tempL0 * factor;
      pOutputSamples[i*8+1] = (int32_t)tempR0 * factor;
      pOutputSamples[i*8+2] = (int32_t)tempL1 * factor;
      pOutputSamples[i*8+3] = (int32_t)tempR1 * factor;
      pOutputSamples[i*8+4] = (int32_t)tempL2 * factor;
      pOutputSamples[i*8+5] = (int32_t)tempR2 * factor;
      pOutputSamples[i*8+6] = (int32_t)tempL3 * factor;
      pOutputSamples[i*8+7] = (int32_t)tempR3 * factor;
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      pOutputSamples[i*2+0] = (int32_t)(pInputSamples0U32[i] << shift0) * factor;
      pOutputSamples[i*2+1] = (int32_t)(pInputSamples1U32[i] << shift1) * factor;
   }
}

#if defined(RFLAC_SUPPORT_SSE2)
static INLINE void rflac_read_pcm_frames_f32__decode_independent_stereo__sse2(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample) - 8;
   uint32_t shift1 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample) - 8;

   float factor = 1.0f / 8388608.0f;
   __m128 factor128 = _mm_set1_ps(factor);

   for (i = 0; i < frameCount4; ++i) {
      __m128i lefti;
      __m128i righti;
      __m128 leftf;
      __m128 rightf;

      lefti  = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples0 + i), shift0);
      righti = _mm_slli_epi32(_mm_loadu_si128((const __m128i*)pInputSamples1 + i), shift1);

      leftf  = _mm_mul_ps(_mm_cvtepi32_ps(lefti),  factor128);
      rightf = _mm_mul_ps(_mm_cvtepi32_ps(righti), factor128);

      _mm_storeu_ps(pOutputSamples + i*8 + 0, _mm_unpacklo_ps(leftf, rightf));
      _mm_storeu_ps(pOutputSamples + i*8 + 4, _mm_unpackhi_ps(leftf, rightf));
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      pOutputSamples[i*2+0] = (int32_t)(pInputSamples0U32[i] << shift0) * factor;
      pOutputSamples[i*2+1] = (int32_t)(pInputSamples1U32[i] << shift1) * factor;
   }
}
#endif

#if defined(RFLAC_SUPPORT_NEON)
static INLINE void rflac_read_pcm_frames_f32__decode_independent_stereo__neon(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
   uint64_t i;
   uint64_t frameCount4 = frameCount >> 2;
   const uint32_t* pInputSamples0U32 = (const uint32_t*)pInputSamples0;
   const uint32_t* pInputSamples1U32 = (const uint32_t*)pInputSamples1;
   uint32_t shift0 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[0].wastedBitsPerSample) - 8;
   uint32_t shift1 = (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[1].wastedBitsPerSample) - 8;

   float factor = 1.0f / 8388608.0f;
   float32x4_t factor4 = vdupq_n_f32(factor);
   int32x4_t shift0_4  = vdupq_n_s32(shift0);
   int32x4_t shift1_4  = vdupq_n_s32(shift1);

   for (i = 0; i < frameCount4; ++i) {
      int32x4_t lefti;
      int32x4_t righti;
      float32x4_t leftf;
      float32x4_t rightf;

      lefti  = vreinterpretq_s32_u32(vshlq_u32(vld1q_u32(pInputSamples0U32 + i*4), shift0_4));
      righti = vreinterpretq_s32_u32(vshlq_u32(vld1q_u32(pInputSamples1U32 + i*4), shift1_4));

      leftf  = vmulq_f32(vcvtq_f32_s32(lefti),  factor4);
      rightf = vmulq_f32(vcvtq_f32_s32(righti), factor4);

      {
         /* interleaving store; replaces vzip + contiguous store */
         float32x4x2_t v;
         v.val[0] = leftf;
         v.val[1] = rightf;
         vst2q_f32(pOutputSamples + i*8, v);
      }
   }

   for (i = (frameCount4 << 2); i < frameCount; ++i) {
      pOutputSamples[i*2+0] = (int32_t)(pInputSamples0U32[i] << shift0) * factor;
      pOutputSamples[i*2+1] = (int32_t)(pInputSamples1U32[i] << shift1) * factor;
   }
}
#endif

static INLINE void rflac_read_pcm_frames_f32__decode_independent_stereo(
      rflac* pFlac, uint64_t frameCount, uint32_t unusedBitsPerSample,
      const int32_t* pInputSamples0, const int32_t* pInputSamples1,
      float* pOutputSamples)
{
#if defined(RFLAC_SUPPORT_SSE2)
   if (rflac__gIsSSE2Supported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_f32__decode_independent_stereo__sse2(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#elif defined(RFLAC_SUPPORT_NEON)
   if (rflac__gIsNEONSupported && pFlac->bitsPerSample <= 24)
      rflac_read_pcm_frames_f32__decode_independent_stereo__neon(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   else
#endif
   {
      /* Scalar fallback. */
      rflac_read_pcm_frames_f32__decode_independent_stereo__scalar(pFlac, frameCount, unusedBitsPerSample, pInputSamples0, pInputSamples1, pOutputSamples);
   }
}

static uint64_t rflac_read_pcm_frames_f32(rflac* pFlac, uint64_t framesToRead,
      float* pBufferOut)
{
   uint64_t framesRead;
   uint32_t unusedBitsPerSample;

   if (pFlac == NULL || framesToRead == 0)
      return 0;

   if (pBufferOut == NULL)
      return rflac__seek_forward_by_pcm_frames(pFlac, framesToRead);

   unusedBitsPerSample = 32 - pFlac->bitsPerSample;

   framesRead = 0;
   while (framesToRead > 0) {
      /* If we've run out of samples in this frame, go to the next. */
      if (pFlac->currentFLACFrame.pcmFramesRemaining == 0) {
         if (!rflac__read_and_decode_next_flac_frame(pFlac)) {
            /* Couldn't read the next frame, so just break from the loop and
             * return. */
            break;
         }
      } else {
         unsigned int channelCount = rflac__get_channel_count_from_channel_assignment(pFlac->currentFLACFrame.header.channelAssignment);
         uint64_t iFirstPCMFrame = pFlac->currentFLACFrame.header.blockSizeInPCMFrames - pFlac->currentFLACFrame.pcmFramesRemaining;
         uint64_t frameCountThisIteration = framesToRead;

         if (frameCountThisIteration > pFlac->currentFLACFrame.pcmFramesRemaining)
            frameCountThisIteration = pFlac->currentFLACFrame.pcmFramesRemaining;

         if (channelCount == 2) {
            const int32_t* pDecodedSamples0 = pFlac->currentFLACFrame.subframes[0].pSamplesS32 + iFirstPCMFrame;
            const int32_t* pDecodedSamples1 = pFlac->currentFLACFrame.subframes[1].pSamplesS32 + iFirstPCMFrame;

            switch (pFlac->currentFLACFrame.header.channelAssignment)
            {
               case RFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE:
               {
                  if (pFlac->wideChannelIndex == 1)
                     rflac_read_pcm_frames_f32__decode_left_side__wide(pFlac, frameCountThisIteration, pDecodedSamples0, pFlac->pWideSamples + iFirstPCMFrame, pBufferOut);
                  else
                     rflac_read_pcm_frames_f32__decode_left_side(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
               } break;

               case RFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE:
               {
                  if (pFlac->wideChannelIndex == 0)
                     rflac_read_pcm_frames_f32__decode_right_side__wide(pFlac, frameCountThisIteration, pFlac->pWideSamples + iFirstPCMFrame, pDecodedSamples1, pBufferOut);
                  else
                     rflac_read_pcm_frames_f32__decode_right_side(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
               } break;

               case RFLAC_CHANNEL_ASSIGNMENT_MID_SIDE:
               {
                  if (pFlac->wideChannelIndex == 1)
                     rflac_read_pcm_frames_f32__decode_mid_side__wide(pFlac, frameCountThisIteration, pDecodedSamples0, pFlac->pWideSamples + iFirstPCMFrame, pBufferOut);
                  else
                     rflac_read_pcm_frames_f32__decode_mid_side(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
               } break;

               case RFLAC_CHANNEL_ASSIGNMENT_INDEPENDENT:
               default:
               {
                  rflac_read_pcm_frames_f32__decode_independent_stereo(pFlac, frameCountThisIteration, unusedBitsPerSample, pDecodedSamples0, pDecodedSamples1, pBufferOut);
               } break;
            }
         } else {
            /* Generic interleaving. */
            uint64_t i;
            for (i = 0; i < frameCountThisIteration; ++i) {
               unsigned int j;
               for (j = 0; j < channelCount; ++j) {
                  int32_t sampleS32 = (int32_t)((uint32_t)(pFlac->currentFLACFrame.subframes[j].pSamplesS32[iFirstPCMFrame + i]) << (unusedBitsPerSample + pFlac->currentFLACFrame.subframes[j].wastedBitsPerSample));
                  pBufferOut[(i*channelCount)+j] = (float)(sampleS32 / 2147483648.0);
               }
            }
         }

         framesRead                += frameCountThisIteration;
         pBufferOut                += frameCountThisIteration * channelCount;
         framesToRead              -= frameCountThisIteration;
         pFlac->currentPCMFrame    += frameCountThisIteration;
         pFlac->currentFLACFrame.pcmFramesRemaining -= (unsigned int)frameCountThisIteration;
      }
   }

   return framesRead;
}



#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
   #pragma GCC diagnostic pop
#endif


/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2023 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/* ------------------------------------------------------------------
 * Push-based interface (formats/rflac.h)
 *
 * The caller hands over spans of input rather than answering read
 * callbacks. Internally the callbacks remain, serving from the current
 * span and flagging when they cannot satisfy a read; the frame pump
 * checks that flag and rewinds, so a frame that straddles two spans is
 * simply decoded again once the rest arrives and the decode core never
 * learns it was interrupted.
 *
 * Rewinding is a copy of rflac_bs, which is plain data with no pointers
 * into anything the caller owns. It carries a 4 KiB cache, so the copy
 * is only taken when an underrun is actually possible -- that is, when
 * less than a maximum frame remains -- and the steady state pays
 * nothing.
 * ------------------------------------------------------------------ */

/* Input is read as one logical run of carry followed by span. The carry
 * holds whatever a rolled-back frame had already consumed, so the frame
 * can be decoded again from its start once the rest of it arrives,
 * without the caller having to know where frames begin or having to
 * re-present bytes it already handed over. */
typedef struct rflac_push_source
{
   uint8_t       *carry;
   size_t         carry_len;
   size_t         carry_cap;
   const uint8_t *data;
   size_t         size;
   size_t         pos;
   int            underrun;
} rflac_push_source;

static int rflac__src_hold(rflac_push_source *s, size_t from);

static size_t rflac__src_total(const rflac_push_source *s)
{
   return s->carry_len + s->size;
}

static const uint8_t *rflac__src_at(const rflac_push_source *s, size_t pos,
      size_t *contiguous)
{
   if (pos < s->carry_len)
   {
      *contiguous = s->carry_len - pos;
      return s->carry + pos;
   }
   *contiguous = s->size - (pos - s->carry_len);
   return s->data + (pos - s->carry_len);
}

struct rflac_ctx
{
   rflac             *dec;
   rflac_push_source  src;
   rflac_format_t     fmt;
   /* A rewind point covers the decoder struct, not just its bitreader:
    * the current frame and PCM position live alongside it and a
    * half-decoded frame has already moved them. The sample plane that
    * trails the struct is not saved, because anything written there
    * belongs to the frame being abandoned. */
   rflac             *saved;
   size_t             saved_pos;
   int16_t           *out_s16;
   float             *out_f32;
   size_t             out_frames;
   size_t             out_done;
   int                ended;
   int                need_header;   /* set until a header is parsed */
};

static size_t rflac__on_read_push(void *pUserData, void *bufferOut,
      size_t bytesToRead)
{
   rflac_push_source *s     = (rflac_push_source*)pUserData;
   size_t             avail = rflac__src_total(s) - s->pos;
   uint8_t           *dst   = (uint8_t*)bufferOut;
   size_t             done  = 0;

   if (bytesToRead > avail)
   {
      /* Short reads are how the decode core discovers the end of a
       * stream, so this cannot fail outright; it records that input ran
       * dry and lets the caller above decide whether that was the end
       * of the stream or merely the end of a span. */
      s->underrun = 1;
      bytesToRead = avail;
   }

   while (done < bytesToRead)
   {
      size_t         run;
      const uint8_t *p    = rflac__src_at(s, s->pos, &run);
      size_t         take = bytesToRead - done;

      if (take > run)
         take = run;
      memcpy(dst + done, p, take);
      s->pos += take;
      done   += take;
   }

   return done;
}

static uint32_t rflac__on_seek_push(void *pUserData, int offset,
      rflac_seek_origin origin)
{
   rflac_push_source *s = (rflac_push_source*)pUserData;
   size_t             target;

   if (origin == rflac_seek_origin_current)
   {
      if (offset < 0 && (size_t)(-offset) > s->pos)
         return 0;
      target = (offset < 0) ? s->pos - (size_t)(-offset)
                            : s->pos + (size_t)offset;
   }
   else
   {
      if (offset < 0)
         return 0;
      target = (size_t)offset;
   }

   if (target > rflac__src_total(s))
   {
      s->underrun = 1;
      return 0;
   }

   s->pos = target;
   return 1;
}

/* Builds a decoder for a stream that carries no header, from geometry
 * the caller supplies. The allocation mirrors the header-parsing path:
 * the object and the decoded-sample plane are one block, sized by the
 * widest SIMD vector so the per-channel planes stay aligned. There is
 * no seek table and no metadata, which is most of why this is short. */
static rflac *rflac__alloc_raw(const rflac_format_t *fmt,
      rflac_push_source *src)
{
   rflac_init_info init;
   rflac           *pFlac;
   uint32_t         vectors;
   uint32_t         decodedSize;
   uint32_t         wideSize;
   uint32_t         allocationSize;

   rflac__init_cpu_caps();
   rflac__crc16_init_slices();

   memset(&init, 0, sizeof(init));
   init.sampleRate              = fmt->sample_rate;
   init.channels                = (uint8_t)fmt->channels;
   init.bitsPerSample           = (uint8_t)fmt->bits_per_sample;
   init.totalPCMFrameCount      = 0;
   init.maxBlockSizeInPCMFrames = (uint16_t)fmt->block_size;
   init.hasMetadataBlocks       = 0;
   init.bs.onRead               = rflac__on_read_push;
   init.bs.onSeek               = rflac__on_seek_push;
   init.bs.pUserData            = src;

   vectors = init.maxBlockSizeInPCMFrames
           / (RFLAC_MAX_SIMD_VECTOR_SIZE / sizeof(int32_t));
   if ((init.maxBlockSizeInPCMFrames
            % (RFLAC_MAX_SIMD_VECTOR_SIZE / sizeof(int32_t))) != 0)
      vectors++;

   decodedSize = vectors * RFLAC_MAX_SIMD_VECTOR_SIZE * init.channels;

   wideSize = 0;
   if (init.bitsPerSample == 32 && init.channels == 2)
      wideSize = init.maxBlockSizeInPCMFrames * (uint32_t)sizeof(int64_t) + 8;

   allocationSize = (uint32_t)sizeof(rflac) + decodedSize + wideSize
                  + RFLAC_MAX_SIMD_VECTOR_SIZE;

   if (!(pFlac = (rflac*)malloc(allocationSize)))
      return NULL;

   rflac__init_from_info(pFlac, &init);
   pFlac->bs.pUserData            = src;
   pFlac->firstFLACFramePosInBytes = 0;
   pFlac->pDecodedSamples         = (int32_t*)RFLAC_ALIGN(
         (size_t)pFlac->pExtraData, RFLAC_MAX_SIMD_VECTOR_SIZE);
   if (wideSize)
      pFlac->pWideSamples = (int64_t*)(((uint8_t*)pFlac->pDecodedSamples)
            + decodedSize);

   return pFlac;
}

/* A stream that carries its own header does not describe itself until
 * that header has been read, and the header cannot be read until the
 * caller has handed some over. So construction records only that one is
 * expected; the parse happens on the first process() call that has
 * input, and until it succeeds the decoder has no geometry to report. */
rflac_t *rflac_new(void)
{
   rflac_t *f;

   if (!(f = (rflac_t*)calloc(1, sizeof(*f))))
      return NULL;

   f->need_header = 1;
   return f;
}

/* Runs the header parse against the current span. A header is small and
 * arrives in one piece from every source this serves, so a span too
 * short to hold one is treated as "not yet" rather than as an error. */
static int rflac__open_from_span(rflac_t *f)
{
   rflac *dec = rflac_open_with_metadata_private(
         rflac__on_read_push, rflac__on_seek_push, NULL,
         &f->src, &f->src);

   if (!dec)
      return 0;

   dec->bs.pUserData        = &f->src;
   f->dec                   = dec;
   f->fmt.sample_rate       = dec->sampleRate;
   f->fmt.channels          = dec->channels;
   f->fmt.bits_per_sample   = dec->bitsPerSample;
   f->fmt.block_size        = dec->maxBlockSizeInPCMFrames;
   f->need_header           = 0;
   return 1;
}

rflac_t *rflac_new_raw(const rflac_format_t *fmt)
{
   rflac_t *f;

   if (!fmt || !fmt->channels || fmt->channels > RFLAC_MAX_CHANNELS)
      return NULL;
   if (!fmt->bits_per_sample || fmt->bits_per_sample > 32)
      return NULL;
   /* A headerless stream states its own block size or nothing can size
    * the sample plane. */
   if (!fmt->block_size || fmt->block_size > RFLAC_MAX_BLOCK_SIZE)
      return NULL;

   if (!(f = (rflac_t*)calloc(1, sizeof(*f))))
      return NULL;

   f->fmt = *fmt;
   if (!(f->dec = rflac__alloc_raw(fmt, &f->src)))
   {
      free(f);
      return NULL;
   }

   return f;
}

void rflac_free(rflac_t *f)
{
   if (!f)
      return;
   if (f->dec)
      free(f->dec);
   if (f->saved)
      free(f->saved);
   if (f->src.carry)
      free(f->src.carry);
   free(f);
}

/* Largest a frame can be: a full block of the widest samples, plus
 * headroom for the frame header and the per-subframe escape where a
 * block is stored verbatim. Below this much input remaining, a decode
 * might run off the end and has to be made undoable. */
/* Where the stream has actually been consumed to, as opposed to how far
 * the reader has pulled. The bitstream reader fills a cache ahead of the
 * decode position, so the raw span offset overshoots by whatever is
 * still sitting unread in it.
 *
 * Callers need the true figure, not the optimistic one: a CHD hunk puts
 * its subchannel data immediately after the last FLAC frame, and finding
 * that boundary is only possible if the decoder reports where the frames
 * really ended. */
static size_t rflac__consumed(const rflac_t *f)
{
   const rflac_bs *bs = &f->dec->bs;
   size_t          held;

   held  = ((sizeof(bs->cacheL2) / sizeof(bs->cacheL2[0])) - bs->nextL2Line)
         * sizeof(size_t);
   held += ((sizeof(bs->cache) * 8) - bs->consumedBits) / 8;
   held += bs->unalignedByteCount;

   if (held > f->src.pos)
      return 0;
   return f->src.pos - held;
}

static size_t rflac__max_frame_bytes(const rflac_t *f)
{
   return (size_t)f->fmt.block_size * f->fmt.channels
        * ((f->fmt.bits_per_sample + 7) / 8) + 64;
}

int rflac_process(rflac_t *f, size_t *read, size_t *wrote)
{
   size_t   start_pos;
   size_t   span_len;
   size_t   want;
   size_t   produced;
   int      undoable = 0;

   if (read)
      *read = 0;
   if (wrote)
      *wrote = 0;

   if (!f)
      return RFLAC_PROCESS_ERROR;
   if (f->ended)
      return RFLAC_PROCESS_END;

   span_len = f->src.size;

   if (f->need_header)
   {
      size_t held = rflac__src_total(&f->src);

      if (held == 0)
         return RFLAC_PROCESS_NEXT;

      f->src.pos      = 0;
      f->src.underrun = 0;

      if (!rflac__open_from_span(f))
      {
         /* Ran out part way through means the header is split across
          * spans; keep what arrived and ask for the rest. Anything
          * else is a stream that is not FLAC. */
         if (!f->src.underrun)
            return RFLAC_PROCESS_ERROR;
         if (!rflac__src_hold(&f->src, 0))
            return RFLAC_PROCESS_ERROR;
         f->src.data = NULL;
         f->src.size = 0;
         f->src.pos  = 0;
         if (read)
            *read = span_len;
         return RFLAC_PROCESS_NEXT;
      }
   }

   if (!f->dec)
      return RFLAC_PROCESS_ERROR;
   if (!f->out_s16 && !f->out_f32)
      return RFLAC_PROCESS_NEXT;
   if (f->out_done >= f->out_frames)
      return RFLAC_PROCESS_NEXT;

   start_pos = rflac__consumed(f);

   /* A call either drains what is already decoded, or attempts exactly
    * one new frame -- never both.
    *
    * The distinction matters because a call that spans the boundary can
    * emit some frames and then underrun on the next, and the rewind
    * that undoes the half-read frame would undo the good ones with it.
    * Splitting the two makes an attempt all or nothing: there is
    * nothing to lose by rewinding, because nothing was produced.
    *
    * It also keeps the promise that work between returns is bounded by
    * the block size, whatever the caller asked for. */
   want = f->dec->currentFLACFrame.pcmFramesRemaining;
   if (want == 0)
      want = f->fmt.block_size;
   if (want > f->out_frames - f->out_done)
      want = f->out_frames - f->out_done;

   if (rflac__src_total(&f->src) - f->src.pos < rflac__max_frame_bytes(f))
   {
      if (!f->saved && !(f->saved = (rflac*)malloc(sizeof(rflac))))
         return RFLAC_PROCESS_ERROR;
      memcpy(f->saved, f->dec, sizeof(rflac));
      f->saved_pos = f->src.pos;
      undoable     = 1;
   }

   f->src.underrun = 0;

   if (f->out_s16)
      produced = (size_t)rflac_read_pcm_frames_s16(f->dec, want,
            f->out_s16 + f->out_done * f->fmt.channels);
   else
      produced = (size_t)rflac_read_pcm_frames_f32(f->dec, want,
            f->out_f32 + f->out_done * f->fmt.channels);

   if (f->src.underrun && produced < want)
   {
      /* The span ran out mid-frame. Put everything back so the frame
       * can be decoded once from the start when the rest arrives; the
       * decode core is never told this happened. */
      if (undoable)
      {
         memcpy(f->dec, f->saved, sizeof(rflac));
         if (!rflac__src_hold(&f->src, f->saved_pos))
            return RFLAC_PROCESS_ERROR;
         f->src.pos = 0;
         f->src.data = NULL;
         f->src.size = 0;
         /* The whole span was taken over, whether it was consumed or
          * carried, so the caller is free to reuse or free it. */
         if (read)
            *read = span_len;
         return RFLAC_PROCESS_NEXT;
      }
      /* Not undoable means the span was long enough for any legal
       * frame and still ran dry, so this is the end of the stream
       * rather than the end of a span. */
      f->ended = 1;
   }

   f->out_done += produced;

   if (read)
   {
      size_t now = rflac__consumed(f);
      *read = (now > start_pos) ? now - start_pos : 0;
   }
   if (wrote)
      *wrote = produced;

   /* Producing nothing is not the end of the stream. A span that runs
    * out between frames looks exactly like one that runs out at the
    * finish, and only the caller knows which it was -- it is the one
    * holding the rest, or not. So this asks for more and lets the
    * caller stop feeding; the end is latched above, where an underrun
    * on a span long enough to hold any legal frame proves there was
    * nothing further to read. */
   return f->ended ? RFLAC_PROCESS_END : RFLAC_PROCESS_NEXT;
}

int rflac_seek(rflac_t *f, uint64_t frame, uint64_t *byte_offset)
{
   uint32_t i;
   uint32_t best = 0;
   int      found = 0;

   if (!f || !f->dec || !byte_offset)
      return RFLAC_PROCESS_ERROR;
   if (!f->dec->pSeekpoints || !f->dec->seekpointCount)
      return RFLAC_PROCESS_ERROR;

   /* The last point at or before the target. Points are in order, but
    * a placeholder point states an all-ones frame index and must not be
    * treated as the nearest to anything. */
   for (i = 0; i < f->dec->seekpointCount; i++)
   {
      uint64_t at = f->dec->pSeekpoints[i].firstPCMFrame;

      if (at == (uint64_t)-1)
         continue;
      if (at > frame)
         break;
      best  = i;
      found = 1;
   }

   if (!found)
      return RFLAC_PROCESS_ERROR;

   *byte_offset = f->dec->firstFLACFramePosInBytes
                + f->dec->pSeekpoints[best].flacFrameOffset;

   return RFLAC_PROCESS_NEXT;
}

void rflac_seek_resumed(rflac_t *f, uint64_t frame)
{
   if (!f || !f->dec)
      return;

   /* Everything buffered belongs to wherever the stream used to be. */
   f->src.carry_len = 0;
   f->src.data      = NULL;
   f->src.size      = 0;
   f->src.pos       = 0;
   f->src.underrun  = 0;
   f->out_done      = 0;
   f->ended         = 0;

   memset(&f->dec->bs, 0, sizeof(f->dec->bs));
   f->dec->bs.onRead    = rflac__on_read_push;
   f->dec->bs.onSeek    = rflac__on_seek_push;
   f->dec->bs.pUserData = &f->src;
   f->dec->currentPCMFrame = frame;
   memset(&f->dec->currentFLACFrame, 0, sizeof(f->dec->currentFLACFrame));
}

void rflac_reset(rflac_t *f)
{
   if (!f)
      return;
   rflac_set_in(f, NULL, 0);
   f->out_done = 0;
   f->ended    = 0;
}

void rflac_set_in(rflac_t *f, const uint8_t *in, size_t in_size)
{
   if (!f)
      return;
   f->src.data     = in;
   f->src.size     = in ? in_size : 0;
   /* Whatever a rolled-back frame left in the carry is still ahead of
    * this span, so reading resumes at the front of the carry rather
    * than at the front of the new bytes. */
   f->src.pos      = 0;
   f->src.underrun = 0;
}

/* Moves everything from @from to the end of the input into the carry,
 * so it survives the span being replaced. */
static int rflac__src_hold(rflac_push_source *s, size_t from)
{
   size_t   total = rflac__src_total(s);
   size_t   need  = total - from;
   uint8_t *buf;
   size_t   done  = 0;

   if (need == 0)
   {
      s->carry_len = 0;
      return 1;
   }

   if (need > s->carry_cap)
   {
      if (!(buf = (uint8_t*)realloc(s->carry, need)))
         return 0;
      s->carry     = buf;
      s->carry_cap = need;
   }

   /* The source of the copy may overlap the carry itself, so it is
    * gathered forwards into a position at or below where it started. */
   while (done < need)
   {
      size_t         run;
      const uint8_t *p    = rflac__src_at(s, from + done, &run);
      size_t         take = need - done;

      if (take > run)
         take = run;
      memmove(s->carry + done, p, take);
      done += take;
   }

   s->carry_len = need;
   return 1;
}

void rflac_set_out_s16(rflac_t *f, int16_t *out, size_t out_frames)
{
   if (!f)
      return;
   f->out_s16    = out;
   f->out_f32    = NULL;
   f->out_frames = out_frames;
   f->out_done   = 0;
}

void rflac_set_out_f32(rflac_t *f, float *out, size_t out_frames)
{
   if (!f)
      return;
   f->out_f32    = out;
   f->out_s16    = NULL;
   f->out_frames = out_frames;
   f->out_done   = 0;
}

const rflac_format_t *rflac_format(const rflac_t *f)
{
   if (!f || !f->dec || f->need_header)
      return NULL;
   return &f->fmt;
}

uint64_t rflac_total_frames(const rflac_t *f)
{
   if (!f || !f->dec)
      return 0;
   return f->dec->totalPCMFrameCount;
}

uint64_t rflac_tell(const rflac_t *f)
{
   if (!f || !f->dec)
      return 0;
   return f->dec->currentPCMFrame;
}
