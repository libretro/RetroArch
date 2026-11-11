/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (float_to_ulaw_test.c).
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

/* convert_s16_to_ulaw and convert_float_to_ulaw.
 *
 *   Silence is 0xFF, full scale 0x00 and 0x80; the codes fall as the
 *   sample rises through each half; each sample takes a code whose
 *   level (G.711's decoding, written out here on its own) is the
 *   nearest within 8 (the 2 bits the 14-bit input drops, and the
 *   rounding of the decision levels); the float path
 *   gives the 16-bit one's codes for every sample, and clips.
 *
 *   --ramp FILE writes every 16-bit sample in order, little-endian;
 *   --ffmpeg FILE then checks FFmpeg's pcm_mulaw codes for that ramp
 *   are these, all 65536.
 *
 * Build and run:  make && ./float_to_ulaw_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <audio/conversion/float_to_ulaw.h>

static unsigned failures;

#define CHECK(c, what) do { if (!(c)) { printf("  FAIL  %s (line %u)\n", what, \
      (unsigned)__LINE__); failures++; } } while (0)

/* G.711's decoding to the 16-bit scale */
static int decode(uint8_t u)
{
   int t;
   u = (uint8_t)~u;
   t = (((u & 0x0F) << 3) + 0x84) << ((u & 0x70) >> 4);
   return (u & 0x80) ? 0x84 - t : t - 0x84;
}

static uint8_t codes[65536];

static void encode_all(void)
{
   static int16_t ramp[65536];
   long s;
   for (s = -32768; s < 32768; s++)
      ramp[s + 32768] = (int16_t)s;
   convert_s16_to_ulaw(codes, ramp, 65536);
}

int main(int argc, char **argv)
{
   long s;
   int  nearest = 1, falling = 1, same = 1, c;

   encode_all();

   if (argc == 3 && !strcmp(argv[1], "--ramp"))
   {
      FILE *f = fopen(argv[2], "wb");
      if (!f)
         return 1;
      for (s = -32768; s < 32768; s++)
      {
         fputc((int)(s & 0xFF), f);
         fputc((int)((s >> 8) & 0xFF), f);
      }
      fclose(f);
      return 0;
   }
   if (argc == 3 && !strcmp(argv[1], "--ffmpeg"))
   {
      FILE *f = fopen(argv[2], "rb");
      long  n = 0, bad = 0;
      if (!f)
         return 1;
      while ((c = fgetc(f)) != EOF && n < 65536)
      {
         if ((uint8_t)c != codes[n])
         {
            if (!bad)
               printf("  sample %ld: FFmpeg's 0x%02x, ours 0x%02x\n", n - 32768, c, codes[n]);
            bad++;
         }
         n++;
      }
      fclose(f);
      printf("%ld samples, %ld unlike FFmpeg's pcm_mulaw\n", n, bad);
      return n == 65536 && !bad ? 0 : 1;
   }

   CHECK(codes[32768] == 0xFF, "silence is 0xFF");
   CHECK(codes[0] == 0x00, "full scale negative is 0x00");
   CHECK(codes[65535] == 0x80, "full scale positive is 0x80");

   for (s = -32768; s < 32768; s++)
   {
      uint8_t u = codes[s + 32768];
      int     d = abs(decode(u) - (int)s), k;
      /* within a half, the code falls (mu-law's bits are inverted) */
      if (s > -32768 && s != 0)
      {
         uint8_t p = codes[s + 32767];
         if ((p & 0x80) == (u & 0x80) && ((s > 0 && p < u) || (s < 0 && p > u)))
            falling = 0;
      }
      for (k = 0; k < 256; k++)
         if (abs(decode((uint8_t)k) - (int)s) + 8 < d)
            nearest = 0;
   }
   CHECK(falling, "the codes move one way through each half");
   CHECK(nearest, "each sample's level the nearest, within the dropped bits");

   /* the float path: every 16-bit value, and outside the range */
   {
      static float   f[65536 + 4];
      static uint8_t u[65536 + 4];
      for (s = -32768; s < 32768; s++)
         f[s + 32768] = (float)s / 32768.0f;
      f[65536] = 1.5f;
      f[65537] = -2.0f;
      f[65538] = 1.0f;
      f[65539] = 3.6f / 32768.0f;
      convert_float_to_ulaw(u, f, 65536 + 4);
      for (s = 0; s < 65536; s++)
         if (u[s] != codes[s])
            same = 0;
      CHECK(same, "floats take the 16-bit samples' codes");
      CHECK(u[65536] == 0x80 && u[65537] == 0x00 && u[65538] == 0x80, "floats clip");
      CHECK(u[65539] == codes[32768 + 4], "floats round to the nearest sample");
   }

   if (failures)
      printf("\n%u check(s) failed\n", failures);
   else
      printf("all checks passed\n");
   return failures ? 1 : 0;
}
