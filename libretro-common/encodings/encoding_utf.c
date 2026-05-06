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

/* Lookup table replaces leading_ones() bit-counting loop.
 * Index by high byte value >> 3 (32 entries) to get
 * the number of leading 1-bits for any byte.
 * Only values 0..7 are meaningful for UTF-8;
 * entries for invalid prefixes are set to 0xFF. */
static const uint8_t utf8_lut[256] = {
   /* 0x00..0x7F: 0 leading ones (ASCII) */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   /* 0x80..0xBF: 1 leading one (continuation byte) */
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
   /* 0xC0..0xDF: 2 leading ones (2-byte sequence) */
   2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
   2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
   /* 0xE0..0xEF: 3 leading ones (3-byte sequence) */
   3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
   /* 0xF0..0xF7: 4 leading ones (4-byte sequence) */
   4,4,4,4,4,4,4,4,
   /* 0xF8..0xFB: 5 leading ones */
   5,5,5,5,
   /* 0xFC..0xFD: 6 leading ones */
   6,6,
   /* 0xFE..0xFF: 7+ leading ones (invalid) */
   7,7
};

/**
 * utf8_conv_utf32:
 *
 * Simple implementation. Assumes the sequence is
 * properly synchronized and terminated.
 *
 * Optimized: replaced leading_ones() loop with LUT,
 * fast-path for ASCII, and unrolled continuation-byte reads.
 **/
size_t utf8_conv_utf32(uint32_t *out, size_t out_chars,
      const char *in, size_t in_size)
{
   size_t ret = 0;
   while (in_size && out_chars)
   {
      uint32_t c;
      uint8_t first;
      unsigned ones;

      /* Fast path: batch ASCII characters */
      while (in_size && out_chars && (uint8_t)*in < 0x80)
      {
         *out++ = (uint8_t)*in++;
         in_size--;
         out_chars--;
         ret++;
      }

      if (!in_size || !out_chars)
         break;

      first = (uint8_t)*in++;
      ones  = utf8_lut[first];

      if (ones > 6 || ones < 2) /* Invalid or desync. */
         break;

      /* ones includes the lead byte; we already consumed it,
       * but need (ones - 1) more continuation bytes */
      if (ones > in_size)       /* Not enough data. */
         break;

      /* Decode based on sequence length to avoid inner loop */
      c = first & ((1 << (7 - ones)) - 1);
      switch (ones)
      {
         case 4:
            c = (c << 6) | ((uint8_t)*in++ & 0x3F);
            /* fall through */
         case 3:
            c = (c << 6) | ((uint8_t)*in++ & 0x3F);
            /* fall through */
         case 2:
            c = (c << 6) | ((uint8_t)*in++ & 0x3F);
            break;
         default:
         {
            /* 5 or 6 byte sequences (ones == 5 or 6) */
            unsigned i;
            unsigned extra = ones - 1;
            for (i = 0; i < extra; i++)
               c = (c << 6) | ((uint8_t)*in++ & 0x3F);
            break;
         }
      }

      *out++   = c;
      in_size -= ones;
      out_chars--;
      ret++;
   }
   return ret;
}

/**
 * utf16_conv_utf8:
 *
 * Leaf function.
 *
 * Optimized: separated counting-only path (out==NULL) from
 * encoding path to eliminate per-byte branch on `out`.
 * Added explicit fast-path for BMP 2-byte and 3-byte encodings.
 **/
bool utf16_conv_utf8(uint8_t *out, size_t *out_chars,
     const uint16_t *in, size_t in_size)
{
   size_t out_pos = 0;
   size_t in_pos  = 0;

   if (!out)
   {
      /* Counting-only pass: no stores, 
         no per-byte `if (out)` branches */
      for (;;)
      {
         uint32_t value;
         if (in_pos == in_size)
         {
            *out_chars = out_pos;
            return true;
         }
         value = in[in_pos++];

         if (value < 0x80)
         {
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

         if (value < 0x800)
            out_pos += 2;
         else if (value < 0x10000)
            out_pos += 3;
         else
            out_pos += 4;
      }
      *out_chars = out_pos;
      return false;
   }

   /* Encoding pass */
   for (;;)
   {
      uint32_t value;
      if (in_pos == in_size)
      {
         *out_chars = out_pos;
         return true;
      }

      /* Batch ASCII run: avoid per-char branch into multi-byte path */
      while (in_pos < in_size && in[in_pos] < 0x80)
         out[out_pos++] = (uint8_t)in[in_pos++];

      if (in_pos == in_size)
      {
         *out_chars = out_pos;
         return true;
      }

      value = in[in_pos++];

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

      if (value < 0x800)
      {
         /* 2-byte sequence */
         out[out_pos]     = (uint8_t)(0xC0 | (value >> 6));
         out[out_pos + 1] = (uint8_t)(0x80 | (value & 0x3F));
         out_pos += 2;
      }
      else if (value < 0x10000)
      {
         /* 3-byte sequence */
         out[out_pos]     = (uint8_t)(0xE0 | (value >> 12));
         out[out_pos + 1] = (uint8_t)(0x80 | ((value >> 6) & 0x3F));
         out[out_pos + 2] = (uint8_t)(0x80 | (value & 0x3F));
         out_pos += 3;
      }
      else
      {
         /* 4-byte sequence */
         out[out_pos]     = (uint8_t)(0xF0 | (value >> 18));
         out[out_pos + 1] = (uint8_t)(0x80 | ((value >> 12) & 0x3F));
         out[out_pos + 2] = (uint8_t)(0x80 | ((value >> 6) & 0x3F));
         out[out_pos + 3] = (uint8_t)(0x80 | (value & 0x3F));
         out_pos += 4;
      }
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
   size_t byte_count;
   const uint8_t *sb     = (const uint8_t*)in;
   const uint8_t *sb_org = sb;

   if (!in)
      return 0;

   while (*sb && chars-- > 0)
   {
      /* Use LUT to skip entire character at once
       * instead of byte-by-byte continuation check */
      unsigned ones = utf8_lut[*sb];
      if (ones < 2)
         sb++;          /* ASCII or (invalid) standalone continuation */
      else
         sb += ones;    /* Skip full multi-byte character */
   }

   if ((size_t)(sb - sb_org) > len - 1)
   {
      sb = sb_org + len - 1;
      while ((*sb & 0xC0) == 0x80)
         sb--;
   }

   byte_count = (size_t)(sb - sb_org);
   memcpy(s, sb_org, byte_count);
   s[byte_count] = '\0';
   return byte_count;
}

/**
 * utf8skip:
 *
 * Leaf function.
 *
 * Optimized: use LUT to jump over entire multi-byte
 * characters instead of scanning continuation bytes.
 **/
const char *utf8skip(const char *str, size_t chars)
{
   const uint8_t *strb = (const uint8_t*)str;

   if (!chars)
      return str;

   do
   {
      unsigned ones;
      if (!*strb)
         break;
      ones = utf8_lut[*strb];
      if (ones < 2)
         strb++;
      else
      {
         /* Verify we don't walk past a NUL inside a multi-byte seq */
         unsigned i;
         for (i = 0; i < ones && strb[i]; i++)
            ;
         strb += i;
      }
   } while (--chars);

   return (const char*)strb;
}

/**
 * utf8len:
 *
 * Leaf function.
 *
 * Optimized: use LUT to skip entire multi-byte sequences
 * instead of testing each byte individually.
 **/
size_t utf8len(const char *string)
{
   size_t ret = 0;

   if (!string)
      return 0;

   while (*string)
   {
      unsigned ones = utf8_lut[(uint8_t)*string];
      ret++;
      /* ASCII (ones==0) or continuation byte (ones==1, shouldn't
       * appear at sequence start in valid UTF-8). Either way,
       * count it and advance one byte. */
      if (ones < 2)
         string++;
      else /* Multi-byte lead: count one character, skip `ones` bytes */
         string += ones;
   }
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
   const uint8_t *s = (const uint8_t*)*string;
   uint8_t first    = *s++;
   uint32_t ret;

   if (first < 0x80)
   {
      *string = (const char*)s;
      return first;
   }

   /* Use LUT + switch to decode, matching utf8_conv_utf32 style */
   ret = first & ((1 << (7 - utf8_lut[first])) - 1);
   switch (utf8_lut[first])
   {
      case 4:
         ret = (ret << 6) | (*s++ & 0x3F);
         /* fall through */
      case 3:
         ret = (ret << 6) | (*s++ & 0x3F);
         /* fall through */
      case 2:
         ret = (ret << 6) | (*s++ & 0x3F);
         break;
      default:
         break;
   }

   *string = (const char*)s;
   return ret;
}

static bool utf16_to_char(uint8_t **utf_data,
      size_t *dest_len, const uint16_t *in)
{
   const uint16_t *p = in;
   /* Find length in a single scan */
   while (*p != 0)
      p++;
   {
      size_t in_len = (size_t)(p - in);
      utf16_conv_utf8(NULL, dest_len, in, in_len);
      *dest_len  += 1;
      if ((*utf_data = (uint8_t*)malloc(*dest_len)) != 0)
         return utf16_conv_utf8(*utf_data, dest_len, in, in_len);
   }
   return false;
}

/**
 * utf16_to_char_string:
 **/
bool utf16_to_char_string(const uint16_t *in, char *s, size_t len)
{
   size_t  _len        = 0;
   uint8_t *utf16_data = NULL;
   bool            ret = utf16_to_char(&utf16_data, &_len, in);
   if (ret)
   {
      utf16_data[_len] = 0;
      strlcpy(s, (const char*)utf16_data, len);
   }
   free(utf16_data);
   utf16_data          = NULL;
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

   if ((path_buf_wide = (wchar_t*)
      calloc(path_buf_wide_len + sizeof(wchar_t), sizeof(wchar_t))))
   {
      MultiByteToWideChar(cp_in, 0,
            str, -1, path_buf_wide, path_buf_wide_len);

      if (*path_buf_wide)
      {
         int path_buf_len = WideCharToMultiByte(cp_out, 0,
               path_buf_wide, -1, NULL, 0, NULL, NULL);

         if (path_buf_len)
         {
            char *path_buf = (char*)
               calloc(path_buf_len + sizeof(char), sizeof(char));

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
char* utf8_to_local_string_alloc(const char *str)
{
   if (str && *str)
#if defined(_WIN32) && !defined(_XBOX) && !defined(UNICODE)
      return mb_to_mb_string_alloc(str, CODEPAGE_UTF8, CODEPAGE_LOCAL);
#else
      return strdup(str); /* Assume string needs no modification if not on Windows */
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
      return strdup(str); /* Assume string needs no modification if not on Windows */
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
   int _len       = 0;
#else
   size_t _len    = 0;
#endif
   wchar_t *buf   = NULL;

   if (!str || !*str)
      return NULL;

#ifdef _WIN32
   if ((_len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0)))
   {
      if (!(buf = (wchar_t*)calloc(_len, sizeof(wchar_t))))
         return NULL;

      if ((MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, _len)) < 0)
      {
         free(buf);
         return NULL;
      }
   }
   else
   {
      /* Fallback to ANSI codepage instead */
      if ((_len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0)))
      {
         if (!(buf = (wchar_t*)calloc(_len, sizeof(wchar_t))))
            return NULL;

         if ((MultiByteToWideChar(CP_ACP, 0, str, -1, buf, _len)) < 0)
         {
            free(buf);
            return NULL;
         }
      }
   }
#else
   /* NOTE: For now, assume non-Windows platforms' locale is already UTF-8. */
   if ((_len = mbstowcs(NULL, str, 0) + 1))
   {
      if (!(buf = (wchar_t*)calloc(_len, sizeof(wchar_t))))
         return NULL;

      if ((mbstowcs(buf, str, _len)) == (size_t)-1)
      {
         free(buf);
         return NULL;
      }
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
   int _len       = 0;
#else
   size_t _len    = 0;
#endif
   char *buf      = NULL;

   if (!str || !*str)
      return NULL;

#ifdef _WIN32
   {
      UINT code_page = CP_UTF8;

      /* fallback to ANSI codepage instead */
      if (!(_len = WideCharToMultiByte(code_page,
            0, str, -1, NULL, 0, NULL, NULL)))
      {
         code_page   = CP_ACP;
         _len        = WideCharToMultiByte(code_page,
               0, str, -1, NULL, 0, NULL, NULL);
      }

      if (!(buf = (char*)calloc(_len, sizeof(char))))
         return NULL;

      if (WideCharToMultiByte(code_page,
            0, str, -1, buf, _len, NULL, NULL) < 0)
      {
         free(buf);
         return NULL;
      }
   }
#else
   /* NOTE: For now, assume non-Windows platforms'
    * locale is already UTF-8. */
   if ((_len = wcstombs(NULL, str, 0) + 1))
   {
      if (!(buf = (char*)calloc(_len, sizeof(char))))
         return NULL;

      if (wcstombs(buf, str, _len) == (size_t)-1)
      {
         free(buf);
         return NULL;
      }
   }
#endif

   return buf;
}
