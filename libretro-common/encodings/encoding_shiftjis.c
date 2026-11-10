/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_shiftjis.c).
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

#include <encodings/shiftjis.h>

#include "encoding_shiftjis_tables.h"

/* The row of sjis_decode for lead octet @b, or -1 */
static int sjis_row(unsigned b)
{
   if (b >= 0x81 && b <= 0x9F)
      return (int)(b - 0x81);
   if (b >= 0xE0 && b <= 0xEF)
      return (int)(b - 0xE0 + 31);
   if (b >= 0xFA && b <= 0xFC)
      return (int)(b - 0xFA + 47);
   return -1;
}

/* Puts @cp at @out + *pos as UTF-8 if it fits in @cap octets */
static int sjis_put_utf8(char *out, size_t cap, size_t *pos, uint32_t cp)
{
   size_t n = cp < 0x80 ? 1 : cp < 0x800 ? 2 : 3;
   char  *p = out + *pos;
   if (cap - *pos < n)
      return 0;
   if (n == 1)
      p[0] = (char)cp;
   else if (n == 2)
   {
      p[0] = (char)(0xC0 | (cp >> 6));
      p[1] = (char)(0x80 | (cp & 0x3F));
   }
   else
   {
      p[0] = (char)(0xE0 | (cp >> 12));
      p[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
      p[2] = (char)(0x80 | (cp & 0x3F));
   }
   *pos += n;
   return 1;
}

size_t sjis_to_utf8(char *out, size_t out_size, const char *in, size_t in_len)
{
   const unsigned char *s = (const unsigned char*)in;
   size_t               i = 0, pos = 0;

   if (!out_size)
      return 0;
   while (i < in_len)
   {
      unsigned b = s[i];
      uint32_t cp;
      int      row;
      if (b < 0x80)
      {
         cp = b;
         i++;
      }
      else if (b >= 0xA1 && b <= 0xDF)
      {
         /* half-width katakana */
         cp = 0xFF61 + (b - 0xA1);
         i++;
      }
      else
      {
         unsigned t;
         if ((row = sjis_row(b)) < 0 || i + 1 >= in_len)
         {
            i++;
            continue;
         }
         t = s[i + 1];
         if (t < 0x40 || t > 0xFC || !(cp = sjis_decode[row][t - 0x40]))
         {
            i++;
            continue;
         }
         i += 2;
      }
      if (!sjis_put_utf8(out, out_size - 1, &pos, cp))
         break;
   }
   out[pos] = '\0';
   return pos;
}

/* The two-octet code for @cp, or 0 */
static unsigned sjis_lookup(uint32_t cp)
{
   size_t lo = 0, hi = SJIS_ENCODE_COUNT;
   while (lo < hi)
   {
      size_t mid = lo + (hi - lo) / 2;
      if (sjis_encode[mid][0] < cp)
         lo = mid + 1;
      else
         hi = mid;
   }
   return lo < SJIS_ENCODE_COUNT && sjis_encode[lo][0] == cp ? sjis_encode[lo][1] : 0;
}

/* The code point UTF-8 starts at @s (@len octets left), its length at
 * @n; 0 at @n where the octet starts no valid character */
static uint32_t sjis_utf8_next(const unsigned char *s, size_t len, size_t *n)
{
   unsigned b = s[0];
   uint32_t cp, min;
   size_t   k, more;

   *n = 0;
   if (b < 0x80)
   {
      *n = 1;
      return b;
   }
   if (b >= 0xC2 && b <= 0xDF)
   {
      more = 1;
      cp   = b & 0x1F;
      min  = 0x80;
   }
   else if (b >= 0xE0 && b <= 0xEF)
   {
      more = 2;
      cp   = b & 0x0F;
      min  = 0x800;
   }
   else if (b >= 0xF0 && b <= 0xF4)
   {
      more = 3;
      cp   = b & 0x07;
      min  = 0x10000;
   }
   else
      return 0;
   if (len <= more)
      return 0;
   for (k = 1; k <= more; k++)
   {
      if ((s[k] & 0xC0) != 0x80)
         return 0;
      cp = (cp << 6) | (s[k] & 0x3F);
   }
   if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
      return 0;
   *n = more + 1;
   return cp;
}

size_t utf8_to_sjis(char *out, size_t out_size, const char *in, size_t in_len)
{
   const unsigned char *s = (const unsigned char*)in;
   size_t               i = 0, pos = 0;

   if (!out_size)
      return 0;
   while (i < in_len)
   {
      size_t   n;
      uint32_t cp = sjis_utf8_next(s + i, in_len - i, &n);
      unsigned code;
      if (!n)
      {
         i++;
         continue;
      }
      i += n;
      if (cp < 0x80)
         code = cp;
      else if (cp >= 0xFF61 && cp <= 0xFF9F)
         code = 0xA1 + (cp - 0xFF61);
      else if (!(code = sjis_lookup(cp)))
         continue;
      if (code > 0xFF)
      {
         if (out_size - 1 - pos < 2)
            break;
         out[pos++] = (char)(code >> 8);
         out[pos++] = (char)(code & 0xFF);
      }
      else
      {
         if (out_size - 1 - pos < 1)
            break;
         out[pos++] = (char)code;
      }
   }
   out[pos] = '\0';
   return pos;
}

/* Code page 1252's 0x80-0x9F; 0 where it has nothing */
static const uint16_t cp1252_high[32] = {
   0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
   0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
   0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
   0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178
};

size_t cp1252_to_utf8(char *out, size_t out_size, const char *in, size_t in_len)
{
   const unsigned char *s = (const unsigned char*)in;
   size_t               i, pos = 0;

   if (!out_size)
      return 0;
   for (i = 0; i < in_len; i++)
   {
      uint32_t cp = s[i];
      if (cp >= 0x80 && cp <= 0x9F && !(cp = cp1252_high[cp - 0x80]))
         continue;
      if (!sjis_put_utf8(out, out_size - 1, &pos, cp))
         break;
   }
   out[pos] = '\0';
   return pos;
}
