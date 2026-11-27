/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (utf16_utf32_test.c).
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* utf16_conv_utf32: what a key event's UTF-16 characters decode to.
 * Plain and two-unit code points, every unpaired surrogate shape, a
 * pair that does not fit the room left, and the counts returned. */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include <encodings/utf.h>

static int failures;

static void expect(const char *name,
      const uint16_t *in, size_t in_units, size_t room,
      const uint32_t *want, size_t want_n)
{
   uint32_t out[8];
   size_t i;
   size_t n = utf16_conv_utf32(out, room, in, in_units);

   if (n != want_n)
   {
      printf("FAIL  %s: %u code points, want %u\n",
            name, (unsigned)n, (unsigned)want_n);
      failures++;
      return;
   }
   for (i = 0; i < n; i++)
   {
      if (out[i] != want[i])
      {
         printf("FAIL  %s: [%u] = U+%04X, want U+%04X\n",
               name, (unsigned)i, (unsigned)out[i], (unsigned)want[i]);
         failures++;
         return;
      }
   }
   printf("ok    %s\n", name);
}

int main(void)
{
   static const uint16_t ascii[]       = { 'a', 'B', '\r' };
   static const uint32_t ascii_w[]     = { 'a', 'B', '\r' };
   static const uint16_t bmp[]         = { 0x00E9, 0x20AC, 0xF700 };
   static const uint32_t bmp_w[]       = { 0x00E9, 0x20AC, 0xF700 };
   static const uint16_t pair[]        = { 0xD83D, 0xDE00, 'x' };
   static const uint32_t pair_w[]      = { 0x1F600, 'x' };
   static const uint16_t edges[]       = { 0xD800, 0xDC00, 0xDBFF, 0xDFFF };
   static const uint32_t edges_w[]     = { 0x10000, 0x10FFFF };
   static const uint16_t hi_end[]      = { 'a', 0xD83D };
   static const uint32_t hi_end_w[]    = { 'a', 0xFFFD };
   static const uint16_t hi_hi[]       = { 0xD83D, 0xD83D, 0xDE00 };
   static const uint32_t hi_hi_w[]     = { 0xFFFD, 0x1F600 };
   static const uint16_t hi_plain[]    = { 0xD83D, 'q' };
   static const uint32_t hi_plain_w[]  = { 0xFFFD, 'q' };
   static const uint16_t lo_alone[]    = { 0xDE00, 'z' };
   static const uint32_t lo_alone_w[]  = { 0xFFFD, 'z' };
   static const uint16_t room[]        = { 'a', 0xD83D, 0xDE00, 'b' };
   static const uint32_t room_w[]      = { 'a', 0x1F600 };

   expect("ascii",                       ascii,    3, 8, ascii_w,    3);
   expect("bmp, private use kept",       bmp,      3, 8, bmp_w,      3);
   expect("surrogate pair",              pair,     3, 8, pair_w,     2);
   expect("first and last pair",         edges,    4, 8, edges_w,    2);
   expect("high surrogate at the end",   hi_end,   2, 8, hi_end_w,   2);
   expect("high then pair",              hi_hi,    3, 8, hi_hi_w,    2);
   expect("high then plain unit",        hi_plain, 2, 8, hi_plain_w, 2);
   expect("low surrogate alone",         lo_alone, 2, 8, lo_alone_w, 2);
   expect("room for two: pair is one",   room,     4, 2, room_w,     2);
   expect("empty input",                 ascii,    0, 8, ascii_w,    0);
   expect("no room",                     ascii,    3, 0, ascii_w,    0);

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("all passed\n");
   return 0;
}
