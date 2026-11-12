/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (float_to_ulaw.c).
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

/* G.711 mu-law: see audio/conversion/float_to_ulaw.h. */

#include <audio/conversion/float_to_ulaw.h>

/* The level code @i (0..127, magnitude) decodes to, 16-bit scale */
static int ulaw_level(int i)
{
   return ((((i & 15) << 3) + 0x84) << (i >> 4)) - 0x84;
}

/* The code for magnitude @j (1..8192, 14-bit scale): the first whose
 * decision level - halfway to the next code's level, rounded - is above
 * it, or 127 past the last; seven steps of a binary search */
static int ulaw_code(int j)
{
   int lo = 0, hi = 127;
   while (lo < hi)
   {
      int mid = (lo + hi) >> 1;
      if (j < ((ulaw_level(mid) + ulaw_level(mid + 1) + 4) >> 3))
         hi = mid;
      else
         lo = mid + 1;
   }
   return lo;
}

static uint8_t ulaw_from_s16(int s)
{
   int idx = (s + 32768) >> 2;   /* 0..16383, 8192 at zero */
   if (idx > 8192)
      return (uint8_t)(ulaw_code(idx - 8192) ^ 0xFF);
   if (idx == 8192)
      return 0xFF;
   return (uint8_t)(ulaw_code(8192 - idx) ^ 0x7F);
}

void convert_s16_to_ulaw(uint8_t *out, const int16_t *in, size_t samples)
{
   size_t i;
   for (i = 0; i < samples; i++)
      out[i] = ulaw_from_s16(in[i]);
}

void convert_float_to_ulaw(uint8_t *out, const float *in, size_t samples)
{
   size_t i;
   for (i = 0; i < samples; i++)
   {
      float f = in[i] * 32768.0f;
      int   s;
      /* clipped, then rounded half away from zero */
      if (f >= 32767.0f)
         s = 32767;
      else if (f <= -32768.0f)
         s = -32768;
      else
         s = (int)(f < 0 ? f - 0.5f : f + 0.5f);
      out[i] = ulaw_from_s16(s);
   }
}
