/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (mismatch.c).
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
#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>
#include <algorithms/mismatch.h>
#include <compat/intrinsics.h>

#if defined(__x86_64__) || defined(__i386__) || defined(__i486__) || defined(__i686__)
#define CPU_X86
#endif

/* Other arches SIGBUS (usually) on unaligned accesses. */
#ifndef CPU_X86
#define NO_UNALIGNED_MEM
#endif

#if __SSE2__
#include <emmintrin.h>
#endif

/* There's no equivalent in libc, you'd think so ...
 * std::mismatch exists, but it's not optimized at all. */
size_t find_change(const uint16_t *a, const uint16_t *b)
{
#if __SSE2__
   const __m128i *a128 = (const __m128i*)a;
   const __m128i *b128 = (const __m128i*)b;
   
   for (;;)
   {
      __m128i v0    = _mm_loadu_si128(a128);
      __m128i v1    = _mm_loadu_si128(b128);
      __m128i c     = _mm_cmpeq_epi32(v0, v1);
      uint32_t mask = _mm_movemask_epi8(c);

      if (mask != 0xffff) /* Something has changed, figure out where. */
      {
         size_t ret = (((uint8_t*)a128 - (uint8_t*)a) |
               (compat_ctz(~mask))) >> 1;
         return ret | (a[ret] == b[ret]);
      }

      a128++;
      b128++;
   }
#else
   const uint16_t *a_org = a;
#ifdef NO_UNALIGNED_MEM
   while (((uintptr_t)a & (sizeof(size_t) - 1)) && *a == *b)
   {
      a++;
      b++;
   }
   if (*a == *b)
#endif
   {
      const size_t *a_big = (const size_t*)a;
      const size_t *b_big = (const size_t*)b;
      
      while (*a_big == *b_big)
      {
         a_big++;
         b_big++;
      }
      a = (const uint16_t*)a_big;
      b = (const uint16_t*)b_big;
      
      while (*a == *b)
      {
         a++;
         b++;
      }
   }
   return a - a_org;
#endif
}

size_t find_same(const uint16_t *a, const uint16_t *b)
{
   const uint16_t *a_org = a;
#ifdef NO_UNALIGNED_MEM
   if (((uintptr_t)a & (sizeof(uint32_t) - 1)) && *a != *b)
   {
      a++;
      b++;
   }
   if (*a != *b)
#endif
   {
      /* With this, it's random whether two consecutive identical
       * words are caught.
       *
       * Luckily, compression rate is the same for both cases, and 
       * three is always caught.
       *
       * (We prefer to miss two-word blocks, anyways; fewer iterations 
       * of the outer loop, as well as in the decompressor.) */
      const uint32_t *a_big = (const uint32_t*)a;
      const uint32_t *b_big = (const uint32_t*)b;
      
      while (*a_big != *b_big)
      {
         a_big++;
         b_big++;
      }
      a = (const uint16_t*)a_big;
      b = (const uint16_t*)b_big;
      
      if (a != a_org && a[-1] == b[-1])
      {
         a--;
         b--;
      }
   }
   return a - a_org;
}
