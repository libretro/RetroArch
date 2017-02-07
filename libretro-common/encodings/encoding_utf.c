/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_utf.c).
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
#include <stddef.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
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

/* Acts mostly like strlcpy.
 *
 * Copies the given number of UTF-8 characters,
 * but at most d_len bytes.
 *
 * Always NULL terminates.
 * Does not copy half a character.
 *
 * Returns number of bytes. 's' is assumed valid UTF-8.
 * Use only if 'chars' is considerably less than 'd_len'. */
size_t utf8cpy(char *d, size_t d_len, const char *s, size_t chars)
{
   const uint8_t *sb     = (const uint8_t*)s;
   const uint8_t *sb_org = sb;

   while (*sb && chars-- > 0)
   {
      sb++;
      while ((*sb&0xC0) == 0x80) sb++;
   }

   if ((size_t)(sb - sb_org) > d_len-1 /* NUL */)
   {
      sb = sb_org + d_len-1;
      while ((*sb&0xC0) == 0x80) sb--;
   }

   memcpy(d, sb_org, sb-sb_org);
   d[sb-sb_org] = '\0';

   return sb-sb_org;
}

const char *utf8skip(const char *str, size_t chars)
{
   const uint8_t *strb = (const uint8_t*)str;
   if (!chars)
      return str;
   do
   {
      strb++;
      while ((*strb&0xC0)==0x80) strb++;
      chars--;
   } while(chars);
   return (const char*)strb;
}

size_t utf8len(const char *string)
{
   size_t ret = 0;
   while (*string)
   {
      if ((*string & 0xC0) != 0x80)
         ret++;
      string++;
   }
   return ret;
}

static INLINE uint8_t utf8_walkbyte(const char **string)
{
   return *((*string)++);
}

/* Does not validate the input, returns garbage if it's not UTF-8. */
uint32_t utf8_walk(const char **string)
{
   uint8_t first = utf8_walkbyte(string);
   uint32_t ret;

   if (first<128)
      return first;

   ret = 0;
   ret = (ret<<6) | (utf8_walkbyte(string)    & 0x3F);
   if (first >= 0xE0)
      ret = (ret<<6) | (utf8_walkbyte(string) & 0x3F);
   if (first >= 0xF0)
      ret = (ret<<6) | (utf8_walkbyte(string) & 0x3F);

   if (first >= 0xF0)
      return ret | (first&31)<<18;
   if (first >= 0xE0)
      return ret | (first&15)<<12;
   return ret | (first&7)<<6;
}

static bool utf16_to_char(uint8_t **utf_data,
      size_t *dest_len, const uint16_t *in)
{
   unsigned len    = 0;

   while (in[len] != '\0')
      len++;

   utf16_conv_utf8(NULL, dest_len, in, len);
   *dest_len  += 1;
   *utf_data   = (uint8_t*)malloc(*dest_len);
   if (*utf_data == 0)
      return false;

   return utf16_conv_utf8(*utf_data, dest_len, in, len);
}

bool utf16_to_char_string(const uint16_t *in, char *s, size_t len)
{
   size_t     dest_len  = 0;
   uint8_t *utf16_data  = NULL;
   bool            ret  = utf16_to_char(&utf16_data, &dest_len, in);

   if (ret)
   {
      utf16_data[dest_len] = 0;
      strlcpy(s, (const char*)utf16_data, len);
   }

   free(utf16_data);
   utf16_data = NULL;

   return ret;
}
