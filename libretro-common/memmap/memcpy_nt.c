/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (memcpy_nt.c).
 * ---------------------------------------------------------------------
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <stdint.h>

#include <memcpy_nt.h>

/* Streaming path selection.
 *
 * x86/x86_64: SSE2 MOVNTDQ.  SSE2 is the x86_64 baseline; on 32-bit
 * it is gated the usual way (__SSE2__ from the compiler, or MSVC's
 * /arch flag via _M_IX86_FP).
 *
 * AArch64: STNP.  Not a true write-combining store, but a hint that
 * the line should not be allocated (or should be evicted first);
 * cores from A53 up honour it in exactly the way this function
 * wants.  GCC has no intrinsic for it, so a short asm block copies
 * one 64-byte block per iteration.
 *
 * Everything else (MIPS, ARMv7, PPC, ...): plain memcpy.  ARMv7 has
 * no non-allocating store; on the MIPS handhelds the JZ4770's write
 * allocate policy is fixed, and memcpy is the honest baseline.
 */
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define MEMCPY_NT_HAVE_SSE2 1
#include <emmintrin.h>
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
#define MEMCPY_NT_HAVE_STNP 1
#endif

/* Below this size the sfence/alignment preamble costs more than the
 * streaming saves, and a destination this small is not going to hurt
 * the cache anyway.  One 4 KB page is a deliberately conservative
 * threshold. */
#define MEMCPY_NT_MIN_LEN 4096

void *memcpy_nt(void *dst, const void *src, size_t len)
{
#if defined(MEMCPY_NT_HAVE_SSE2)
   uint8_t       *d = (uint8_t*)dst;
   const uint8_t *s = (const uint8_t*)src;
   size_t      head;

   if (len < MEMCPY_NT_MIN_LEN)
      return memcpy(dst, src, len);

   /* Align the destination to 16 bytes; MOVNTDQ requires it.
    * The source may stay unaligned (loads tolerate it). */
   head = (size_t)((uintptr_t)(-(intptr_t)(uintptr_t)d) & 15);
   if (head)
   {
      memcpy(d, s, head);
      d   += head;
      s   += head;
      len -= head;
   }

   while (len >= 64)
   {
      __m128i a = _mm_loadu_si128((const __m128i*)(const void*)(s +  0));
      __m128i b = _mm_loadu_si128((const __m128i*)(const void*)(s + 16));
      __m128i c = _mm_loadu_si128((const __m128i*)(const void*)(s + 32));
      __m128i e = _mm_loadu_si128((const __m128i*)(const void*)(s + 48));
      _mm_stream_si128((__m128i*)(void*)(d +  0), a);
      _mm_stream_si128((__m128i*)(void*)(d + 16), b);
      _mm_stream_si128((__m128i*)(void*)(d + 32), c);
      _mm_stream_si128((__m128i*)(void*)(d + 48), e);
      d   += 64;
      s   += 64;
      len -= 64;
   }

   if (len)
      memcpy(d, s, len);

   /* Streaming stores are weakly ordered; make them visible before
    * anything the caller does next (page flip, GPU submit). */
   _mm_sfence();
   return dst;
#elif defined(MEMCPY_NT_HAVE_STNP)
   uint8_t       *d = (uint8_t*)dst;
   const uint8_t *s = (const uint8_t*)src;
   size_t      head;

   if (len < MEMCPY_NT_MIN_LEN)
      return memcpy(dst, src, len);

   /* Align the destination to 16 bytes for the q-register pairs. */
   head = (size_t)((uintptr_t)(-(intptr_t)(uintptr_t)d) & 15);
   if (head)
   {
      memcpy(d, s, head);
      d   += head;
      s   += head;
      len -= head;
   }

   while (len >= 64)
   {
      __asm__ __volatile__(
            "ldp q0, q1, [%1]\n\t"
            "ldp q2, q3, [%1, #32]\n\t"
            "stnp q0, q1, [%0]\n\t"
            "stnp q2, q3, [%0, #32]\n\t"
            /* The clobbers name v0-v3, not q0-q3.
             *
             * AArch64 has one set of SIMD registers, v0 to v31, and the
             * q form is a width view of them used in instruction
             * operands: "ldp q0, q1" is correct there. A clobber list
             * is not an operand, and clang accepts only the register
             * name, so "q0" is rejected outright. GCC accepts both,
             * which is why this built everywhere it was tested and
             * broke on Android, iOS and Windows on ARM at once. */
            : : "r"(d), "r"(s) : "v0", "v1", "v2", "v3", "memory");
      d   += 64;
      s   += 64;
      len -= 64;
   }

   if (len)
      memcpy(d, s, len);

   /* STNP carries no ordering; pair it with a store barrier so the
    * frame is complete before a following flip/submit. */
   __asm__ __volatile__("dmb ishst" : : : "memory");
   return dst;
#else
   return memcpy(dst, src, len);
#endif
}
