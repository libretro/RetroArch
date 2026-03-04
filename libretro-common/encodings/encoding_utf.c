/* Copyright  (C) 2010-2020 The RetroArch team
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

#include <encodings/utf.h>

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#elif defined(_XBOX)
#include <xtl.h>
#endif

#define UTF8_WALKBYTE(string) (*((*(string))++))

/* Lookup table: number of leading ones for each byte value (0x80..0xFF).
 * Entries for bytes < 0x80 are 0 (accessed via the branch guard below).
 * Using a 128-entry table covering 0x80-0xFF avoids the loop entirely. */
static const uint8_t leading_ones_table[128] = {
   /* 0x80-0xBF: 1 leading one  (10xxxxxx continuation bytes) */
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
   /* 0xC0-0xDF: 2 leading ones (110xxxxx) */
   2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
   2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
   /* 0xE0-0xEF: 3 leading ones (1110xxxx) */
   3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
   /* 0xF0-0xF7: 4 leading ones (11110xxx) */
   4,4,4,4,4,4,4,4,
   /* 0xF8-0xFB: 5 leading ones (111110xx) */
   5,5,5,5,
   /* 0xFC-0xFD: 6 leading ones (1111110x) */
   6,6,
   /* 0xFE-0xFF: 7 leading ones (rarely valid, keep for completeness) */
   7,7
};

static INLINE unsigned leading_ones(uint8_t c)
{
   if (c < 0x80)
      return 0;
   return leading_ones_table[c - 0x80];
}

/**
 * utf8_conv_utf32:
 *
 * Simple implementation. Assumes the sequence is
 * properly synchronized and terminated.
 **/
size_t utf8_conv_utf32(uint32_t *out, size_t out_chars,
      const char *in, size_t in_size)
{
   size_t ret = 0;

   while (in_size && out_chars)
   {
      unsigned i, extra, shift;
      uint32_t c;
      uint8_t first = (uint8_t)*in++;
      unsigned ones = leading_ones(first);

      if (ones > 6 || ones == 1) /* Invalid or desync. */
         break;

      extra = ones ? ones - 1 : 0;
      if (1 + extra > in_size) /* Overflow. */
         break;

      /* Mask out the leading-one bits from first byte, then shift */
      c = (uint32_t)(first & ((1u << (7 - ones)) - 1)) << (6 * extra);

      shift = (extra > 0) ? (extra - 1) * 6 : 0;
      for (i = 0; i < extra; i++, in++, shift -= 6)
         c |= (uint32_t)((uint8_t)*in & 0x3F) << shift;

      *out++    = c;
      in_size  -= 1 + extra;
      out_chars--;
      ret++;
   }
   return ret;
}

/**
 * utf16_conv_utf8:
 *
 * Leaf function.
 **/
bool utf16_conv_utf8(uint8_t *out, size_t *out_chars,
     const uint16_t *in, size_t in_size)
{
   size_t out_pos = 0;
   size_t in_pos  = 0;
   /* Precomputed limits for 1..4 extra continuation bytes */
   static const uint8_t  utf8_limits[5]  = { 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
   /* Precomputed bit-capacity thresholds: 1<<(n*5+6) for n=1..4 */
   static const uint32_t utf8_thresholds[5] = {
      (1u <<  7),   /* n=0: single byte  (value < 128)   — handled separately */
      (1u << 11),   /* n=1: 2-byte seq   (value < 2048)  */
      (1u << 16),   /* n=2: 3-byte seq   (value < 65536) */
      (1u << 21),   /* n=3: 4-byte seq   (value < 2097152) */
      (1u << 26)    /* n=4: 5-byte seq   (value < 67108864) */
   };

   for (;;)
   {
      unsigned num_adds;
      uint32_t value;

      if (in_pos == in_size)
      {
         *out_chars = out_pos;
         return true;
      }

      value = in[in_pos++];

      /* Fast path: ASCII */
      if (value < 0x80)
      {
         if (out)
            out[out_pos] = (uint8_t)value;
         out_pos++;
         continue;
      }

      /* Surrogate pair handling */
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

      /* Determine number of continuation bytes using the threshold table */
      for (num_adds = 1; num_adds < 5; num_adds++)
         if (value < utf8_thresholds[num_adds])
            break;

      if (out)
         out[out_pos] = (uint8_t)(utf8_limits[num_adds - 1]
               + (value >> (6 * num_adds)));
      out_pos++;

      do
      {
         num_adds--;
         if (out)
            out[out_pos] = (uint8_t)(0x80 + ((value >> (6 * num_adds)) & 0x3F));
         out_pos++;
      } while (num_adds != 0);
   }

   *out_chars = out_pos;
   return false;
}

/**
 * utf8cpy:
 *
 * Acts mostly like strlcpy.
 *
 * Copies the given number of UTF-8 characters,
 * but at most @len bytes.
 *
 * Always NULL terminates. Does not copy half a character.
 * @s is assumed valid UTF-8.
 * Use only if @chars is considerably less than @len.
 *
 * @return Number of bytes.
 **/
size_t utf8cpy(char *s, size_t len, const char *in, size_t chars)
{
   const uint8_t *sb;
   const uint8_t *sb_org;
   size_t nb;

   if (!in || len == 0)
      return 0;

   sb     = (const uint8_t*)in;
   sb_org = sb;

   /* Advance past @chars codepoints */
   while (*sb && chars-- > 0)
   {
      /* Skip the leading byte */
      sb++;
      /* Skip continuation bytes (10xxxxxx) */
      while ((*sb & 0xC0) == 0x80)
         sb++;
   }

   nb = (size_t)(sb - sb_org);

   /* Clamp to len-1 bytes, backing up to a codepoint boundary */
   if (nb > len - 1)
   {
      sb = sb_org + len - 1;
      while ((*sb & 0xC0) == 0x80)
         sb--;
      nb = (size_t)(sb - sb_org);
   }

   memcpy(s, sb_org, nb);
   s[nb] = '\0';
   return nb;
}

/**
 * utf8skip:
 *
 * Leaf function
 **/
const char *utf8skip(const char *str, size_t chars)
{
   const uint8_t *strb = (const uint8_t*)str;

   if (!chars)
      return str;

   do
   {
      strb++;
      while ((*strb & 0xC0) == 0x80)
         strb++;
   } while (--chars);

   return (const char*)strb;
}

/**
 * utf8len:
 *
 * Leaf function.
 **/
size_t utf8len(const char *string)
{
   size_t ret           = 0;
   const uint8_t *s;

   if (!string)
      return 0;

   /* Count bytes that are NOT continuation bytes (10xxxxxx).
    * This is equivalent to counting codepoints. */
   for (s = (const uint8_t*)string; *s; s++)
      ret += ((*s & 0xC0) != 0x80);

   return ret;
}

/**
 * utf8_walk:
 *
 * Does not validate the input.
 *
 * Leaf function.
 *
 * @return Returns garbage if it's not UTF-8.
 **/
uint32_t utf8_walk(const char **string)
{
   uint8_t  first = UTF8_WALKBYTE(string);
   uint32_t ret   = 0;

   if (first < 0x80)
      return first;

   /* 2-byte: 110xxxxx 10xxxxxx  → first >= 0xC0, < 0xE0 */
   /* 3-byte: 1110xxxx …         → first >= 0xE0, < 0xF0 */
   /* 4-byte: 11110xxx …         → first >= 0xF0           */

   ret = (ret << 6) | (UTF8_WALKBYTE(string) & 0x3F);
   if (first >= 0xE0)
   {
      ret = (ret << 6) | (UTF8_WALKBYTE(string) & 0x3F);
      if (first >= 0xF0)
      {
         ret = (ret << 6) | (UTF8_WALKBYTE(string) & 0x3F);
         return ret | ((uint32_t)(first & 0x07) << 18);
      }
      return ret | ((uint32_t)(first & 0x0F) << 12);
   }
   return ret | ((uint32_t)(first & 0x1F) << 6);
}

static bool utf16_to_char(uint8_t **utf_data,
      size_t *dest_len, const uint16_t *in)
{
   size_t _len = 0;

   /* Compute length without calling strlen on uint16_t — walk manually */
   while (in[_len] != '\0')
      _len++;

   utf16_conv_utf8(NULL, dest_len, in, _len);
   *dest_len += 1;

   if ((*utf_data = (uint8_t*)malloc(*dest_len)) != NULL)
      return utf16_conv_utf8(*utf_data, dest_len, in, _len);

   return false;
}

/**
 * utf16_to_char_string:
 **/
bool utf16_to_char_string(const uint16_t *in, char *s, size_t len)
{
   size_t   _len       = 0;
   uint8_t *utf16_data = NULL;
   bool     ret        = utf16_to_char(&utf16_data, &_len, in);

   if (ret)
   {
      utf16_data[_len] = 0;
      strlcpy(s, (const char*)utf16_data, len);
   }

   free(utf16_data);
   return ret;
}

#if defined(_WIN32) && !defined(_XBOX) && !defined(UNICODE)
/**
 * mb_to_mb_string_alloc:
 *
 * @return Returned pointer MUST be freed by the caller if non-NULL.
 **/
static char *mb_to_mb_string_alloc(const char *str,
      enum CodePage cp_in, enum CodePage cp_out)
{
   wchar_t *path_buf_wide = NULL;
   int path_buf_wide_len  = MultiByteToWideChar(cp_in, 0, str, -1, NULL, 0);

   /* Windows 95 will return 0 from these functions with
    * a UTF8 codepage set without MSLU.
    *
    * From an unknown MSDN version (others omit this info):
    *   - CP_UTF8 Windows 98/Me, Windows NT 4.0 and later:
    *   Translate using UTF-8. When this is set, dwFlags must be zero.
    *   - Windows 95: Under the Microsoft Layer for Unicode,
    *   MultiByteToWideChar also supports CP_UTF7 and CP_UTF8.
    */
   if (!path_buf_wide_len)
      return strdup(str);

   path_buf_wide = (wchar_t*)
      calloc(path_buf_wide_len + 1, sizeof(wchar_t));

   if (path_buf_wide)
   {
      MultiByteToWideChar(cp_in, 0, str, -1,
            path_buf_wide, path_buf_wide_len);

      if (*path_buf_wide)
      {
         int path_buf_len = WideCharToMultiByte(cp_out, 0,
               path_buf_wide, -1, NULL, 0, NULL, NULL);

         if (path_buf_len)
         {
            char *path_buf = (char*)calloc(path_buf_len + 1, sizeof(char));

            if (path_buf)
            {
               WideCharToMultiByte(cp_out, 0,
                     path_buf_wide, -1, path_buf,
                     path_buf_len, NULL, NULL);
               free(path_buf_wide);

               if (*path_buf)
                  return path_buf;

               free(path_buf);
               return NULL;
            }
         }
         else
         {
            free(path_buf_wide);
            return strdup(str);
         }
      }

      free(path_buf_wide);
   }

   return NULL;
}
#endif

/**
 * utf8_to_local_string_alloc:
 *
 * @return Returned pointer MUST be freed by the caller if non-NULL.
 **/
char *utf8_to_local_string_alloc(const char *str)
{
   if (str && *str)
#if defined(_WIN32) && !defined(_XBOX) && !defined(UNICODE)
      return mb_to_mb_string_alloc(str, CODEPAGE_UTF8, CODEPAGE_LOCAL);
#else
      return strdup(str);
#endif
   return NULL;
}

/**
 * local_to_utf8_string_alloc:
 *
 * @return Returned pointer MUST be freed by the caller if non-NULL.
 **/
char *local_to_utf8_string_alloc(const char *str)
{
   if (str && *str)
#if defined(_WIN32) && !defined(_XBOX) && !defined(UNICODE)
      return mb_to_mb_string_alloc(str, CODEPAGE_LOCAL, CODEPAGE_UTF8);
#else
      return strdup(str);
#endif
   return NULL;
}

/**
 * utf8_to_utf16_string_alloc:
 *
 * @return Returned pointer MUST be freed by the caller if non-NULL.
 **/
wchar_t *utf8_to_utf16_string_alloc(const char *str)
{
#ifdef _WIN32
   int     _len = 0;
#else
   size_t  _len = 0;
#endif
   wchar_t *buf = NULL;

   if (!str || !*str)
      return NULL;

#ifdef _WIN32
   _len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
   if (!_len)
      _len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);

   if (_len)
   {
      UINT cp = CP_UTF8;
      if (!(buf = (wchar_t*)calloc(_len, sizeof(wchar_t))))
         return NULL;
      /* Re-determine which codepage actually worked */
      if (!MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0))
         cp = CP_ACP;
      if (MultiByteToWideChar(cp, 0, str, -1, buf, _len) < 0)
      {
         free(buf);
         return NULL;
      }
   }
#else
   _len = mbstowcs(NULL, str, 0);
   if (_len == (size_t)-1)
      return NULL;
   _len++; /* room for NUL */

   if (!(buf = (wchar_t*)calloc(_len, sizeof(wchar_t))))
      return NULL;

   if (mbstowcs(buf, str, _len) == (size_t)-1)
   {
      free(buf);
      return NULL;
   }
#endif

   return buf;
}

/**
 * utf16_to_utf8_string_alloc:
 *
 * @return Returned pointer MUST be freed by the caller if non-NULL.
 **/
char *utf16_to_utf8_string_alloc(const wchar_t *str)
{
#ifdef _WIN32
   int    _len = 0;
#else
   size_t _len = 0;
#endif
   char  *buf  = NULL;

   if (!str || !*str)
      return NULL;

#ifdef _WIN32
   {
      UINT code_page = CP_UTF8;

      _len = WideCharToMultiByte(code_page, 0, str, -1, NULL, 0, NULL, NULL);
      if (!_len)
      {
         code_page = CP_ACP;
         _len      = WideCharToMultiByte(code_page, 0, str, -1,
               NULL, 0, NULL, NULL);
      }

      if (!_len)
         return NULL;

      if (!(buf = (char*)calloc(_len, sizeof(char))))
         return NULL;

      if (WideCharToMultiByte(code_page, 0, str, -1,
            buf, _len, NULL, NULL) < 0)
      {
         free(buf);
         return NULL;
      }
   }
#else
   _len = wcstombs(NULL, str, 0);
   if (_len == (size_t)-1)
      return NULL;
   _len++;

   if (!(buf = (char*)calloc(_len, sizeof(char))))
      return NULL;

   if (wcstombs(buf, str, _len) == (size_t)-1)
   {
      free(buf);
      return NULL;
   }
#endif

   return buf;
}
