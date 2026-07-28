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
 * Two implementations, in descending order of speed:
 *
 *   1. ARMv8 CRC32 instructions (chosen at compile time).
 *   2. Slicing-by-8 over a const table (every other target, both endians).
 *
 * The byte-at-a-time loop survives only as the tail handler for the
 * table path; no target uses it as a whole-buffer implementation.
 */

#if defined(__ARM_FEATURE_CRC32)
#define CRC32_HAVE_ARM_PATH 1
#endif

/* ------------------------------------------------------------------ */
/* Portable slicing-by-8                                              */
/* ------------------------------------------------------------------ */

#if !defined(CRC32_HAVE_ARM_PATH)

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
#endif /* !CRC32_HAVE_ARM_PATH */

/* ------------------------------------------------------------------ */
/* ARMv8 CRC32 instructions                                           */
/* ------------------------------------------------------------------ */

#if defined(CRC32_HAVE_ARM_PATH)

#ifdef _M_ARM64
#include <arm64_neon.h>
#else
#include <arm_acle.h>
#endif

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

uint32_t encoding_crc32(uint32_t crc, const uint8_t *data, size_t len)
{
   crc = ~crc;

#if defined(CRC32_HAVE_ARM_PATH)
   crc = crc32_arm(crc, data, len);
#else
   crc = crc32_slice_by_8(crc, data, len);
#endif

   return ~crc;
}
