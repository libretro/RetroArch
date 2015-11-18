/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encodings_utf.c).
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

#include <boolean.h>
#include <retro_inline.h>

static INLINE unsigned leading_ones(uint8_t c)
{
   unsigned ones = 0;
   while (c & 0x80)
   {
      ones++;
      c <<= 1;
   }

   return ones;
}

/* Simple implementation. Assumes the sequence is 
 * properly synchronized and terminated. */

size_t utf8_conv_utf32(uint32_t *out, size_t out_chars,
      const char *in, size_t in_size)
{
   unsigned i;
   size_t ret = 0;
   while (in_size && out_chars)
   {
      unsigned extra, shift;
      uint32_t c;
      uint8_t first = *in++;
      unsigned ones = leading_ones(first);

      if (ones > 6 || ones == 1) /* Invalid or desync. */
         break;

      extra = ones ? ones - 1 : ones;
      if (1 + extra > in_size) /* Overflow. */
         break;

      shift = (extra - 1) * 6;
      c     = (first & ((1 << (7 - ones)) - 1)) << (6 * extra);

      for (i = 0; i < extra; i++, in++, shift -= 6)
         c |= (*in & 0x3f) << shift;

      *out++ = c;
      in_size -= 1 + extra;
      out_chars--;
      ret++;
   }

   return ret;
}


bool utf16_conv_utf8(uint8_t *out, size_t *out_chars,
      const uint16_t *in, size_t in_size)
{
   static uint8_t kUtf8Limits[5] = { 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
   size_t out_pos = 0;
   size_t in_pos  = 0;

   for (;;)
   {
      unsigned numAdds;
      uint32_t value;

      if (in_pos == in_size)
      {
         *out_chars = out_pos;
         return true;
      }

      value = in[in_pos++];

      if (value < 0x80)
      {
         if (out)
            out[out_pos] = (char)value;
         out_pos++;
         continue;
      }

      if (value >= 0xD800 && value < 0xE000)
      {
         uint32_t c2;

         if (value >= 0xDC00 || in_pos == in_size)
            break;
         c2 = in[in_pos++];
         if (c2 < 0xDC00 || c2 >= 0xE000)
            break;
         value = (((value - 0xD800) << 10) | (c2 - 0xDC00)) + 0x10000;
      }

      for (numAdds = 1; numAdds < 5; numAdds++)
         if (value < (((uint32_t)1) << (numAdds * 5 + 6)))
            break;

      if (out)
         out[out_pos] = (char)(kUtf8Limits[numAdds - 1] 
               + (value >> (6 * numAdds)));
      out_pos++;

      do
      {
         numAdds--;
         if (out)
            out[out_pos] = (char)(0x80 
                  + ((value >> (6 * numAdds)) & 0x3F));
         out_pos++;
      }while (numAdds != 0);
   }

   *out_chars = out_pos;
   return false;
}
