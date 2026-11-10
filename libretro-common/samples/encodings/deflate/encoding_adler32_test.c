/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_adler32_test.c).
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

/* encoding_adler32: known values, a checksum continued across pieces
 * equal to the one taken whole, and long runs against the textbook loop
 * at the lengths that matter to the vector kernel (block edges, the
 * 5552-octet bound, all 0xff from the largest seed). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <encodings/deflate.h>

static int failures = 0;

static void check(uint32_t got, uint32_t want, const char *what, size_t n)
{
   if (got != want)
   {
      printf("FAIL %s (%u): got %08x, want %08x\n", what, (unsigned)n,
            (unsigned)got, (unsigned)want);
      failures++;
   }
}

static uint32_t ref_adler32(uint32_t adler, const uint8_t *buf, size_t len)
{
   uint32_t a = adler & 0xffff;
   uint32_t b = adler >> 16;
   size_t   i;
   for (i = 0; i < len; i++)
   {
      a = (a + buf[i]) % 65521;
      b = (b + a) % 65521;
   }
   return (b << 16) | a;
}

int main(void)
{
   static const size_t lens[] = { 0, 1, 15, 16, 17, 31, 32, 63, 64, 65,
      127, 128, 129, 5551, 5552, 5553, 11104, 65536, 100003 };
   const size_t big = 100003;
   uint8_t *buf     = (uint8_t*)malloc(big);
   size_t   i, k;

   if (!buf)
      return 1;

   check(encoding_adler32(1, NULL, 0), 1, "empty", 0);
   check(encoding_adler32(1, (const uint8_t*)"Wikipedia", 9), 0x11e60398,
         "Wikipedia", 9);

   srand(1);
   for (i = 0; i < big; i++)
      buf[i] = (uint8_t)rand();
   for (k = 0; k < sizeof(lens) / sizeof(lens[0]); k++)
   {
      size_t off;
      for (off = 0; off < 4; off++)
         if (lens[k] + off <= big)
            check(encoding_adler32(1, buf + off, lens[k]),
                  ref_adler32(1, buf + off, lens[k]), "random", lens[k]);
   }

   /* continued across pieces of every size from 1 to 97 */
   for (k = 1; k < 98; k++)
   {
      uint32_t a = 1;
      for (i = 0; i < big; i += k)
         a = encoding_adler32(a, buf + i, i + k > big ? big - i : k);
      check(a, ref_adler32(1, buf, big), "pieces", k);
   }

   /* the worst case for the 32-bit sums: all 0xff from the largest seed */
   memset(buf, 0xff, big);
   for (k = 0; k < sizeof(lens) / sizeof(lens[0]); k++)
      check(encoding_adler32(0xfff0fff0u, buf, lens[k]),
            ref_adler32(0xfff0fff0u, buf, lens[k]), "0xff", lens[k]);

   free(buf);
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("encoding_adler32: ok\n");
   return 0;
}
