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

static unsigned leading_ones(uint8_t c)
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
   size_t out_pos            = 0;
   size_t in_pos             = 0;
   static const 
      uint8_t utf8_limits[5] = { 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

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

      for (num_adds = 1; num_adds < 5; num_adds++)
         if (value < (((uint32_t)1) << (num_adds * 5 + 6)))
            break;
      if (out)
         out[out_pos] = (char)(utf8_limits[num_adds - 1]
               + (value >> (6 * num_adds)));
      out_pos++;
      do
      {
         num_adds--;
         if (out)
            out[out_pos] = (char)(0x80
                  + ((value >> (6 * num_adds)) & 0x3F));
         out_pos++;
      }while (num_adds != 0);
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

   if (!s)
      return 0;

   while (*sb && chars-- > 0)
   {
      sb++;
      while ((*sb & 0xC0) == 0x80)
         sb++;
   }

   if ((size_t)(sb - sb_org) > d_len-1 /* NUL */)
   {
      sb = sb_org + d_len-1;
      while ((*sb & 0xC0) == 0x80)
         sb--;
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
      while ((*strb & 0xC0)==0x80)
         strb++;
      chars--;
   }while (chars);

   return (const char*)strb;
}

size_t utf8len(const char *string)
{
   size_t ret = 0;

   if (!string)
      return 0;

   while (*string)
   {
      if ((*string & 0xC0) != 0x80)
         ret++;
      string++;
   }
   return ret;
}

/* Does not validate the input, returns garbage if it's not UTF-8. */
uint32_t utf8_walk(const char **string)
{
   uint8_t first = UTF8_WALKBYTE(string);
   uint32_t ret  = 0;

   if (first < 128)
      return first;

   ret    = (ret << 6) | (UTF8_WALKBYTE(string) & 0x3F);
   if (first >= 0xE0)
   {
      ret = (ret << 6) | (UTF8_WALKBYTE(string) & 0x3F);
      if (first >= 0xF0)
      {
         ret = (ret << 6) | (UTF8_WALKBYTE(string) & 0x3F);
         return ret | (first & 7) << 18;
      }
      return ret | (first & 15) << 12;
   }

   return ret | (first & 31) << 6;
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

#if defined(_WIN32) && !defined(_XBOX) && !defined(UNICODE)
/* Returned pointer MUST be freed by the caller if non-NULL. */
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
      calloc(path_buf_wide_len + sizeof(wchar_t), sizeof(wchar_t));

   if (path_buf_wide)
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

/* Returned pointer MUST be freed by the caller if non-NULL. */
char* utf8_to_local_string_alloc(const char *str)
{
   if (str && *str)
   {
#if defined(_WIN32) && !defined(_XBOX) && !defined(UNICODE)
      return mb_to_mb_string_alloc(str, CODEPAGE_UTF8, CODEPAGE_LOCAL);
#else
      /* assume string needs no modification if not on Windows */
      return strdup(str);
#endif
   }
   return NULL;
}

/* Returned pointer MUST be freed by the caller if non-NULL. */
char* local_to_utf8_string_alloc(const char *str)
{
   if (str && *str)
   {
#if defined(_WIN32) && !defined(_XBOX) && !defined(UNICODE)
      return mb_to_mb_string_alloc(str, CODEPAGE_LOCAL, CODEPAGE_UTF8);
#else
      /* assume string needs no modification if not on Windows */
      return strdup(str);
#endif
   }
   return NULL;
}

/* Returned pointer MUST be freed by the caller if non-NULL. */
wchar_t* utf8_to_utf16_string_alloc(const char *str)
{
#ifdef _WIN32
   int len        = 0;
   int out_len    = 0;
#else
   size_t len     = 0;
   size_t out_len = 0;
#endif
   wchar_t *buf   = NULL;

   if (!str || !*str)
      return NULL;

#ifdef _WIN32
   len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);

   if (len)
   {
      buf = (wchar_t*)calloc(len, sizeof(wchar_t));

      if (!buf)
         return NULL;

      out_len = MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, len);
   }
   else
   {
      /* fallback to ANSI codepage instead */
      len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);

      if (len)
      {
         buf = (wchar_t*)calloc(len, sizeof(wchar_t));

         if (!buf)
            return NULL;

         out_len = MultiByteToWideChar(CP_ACP, 0, str, -1, buf, len);
      }
   }

   if (out_len < 0)
   {
      free(buf);
      return NULL;
   }
#else
   /* NOTE: For now, assume non-Windows platforms' locale is already UTF-8. */
   len = mbstowcs(NULL, str, 0) + 1;

   if (len)
   {
      buf = (wchar_t*)calloc(len, sizeof(wchar_t));

      if (!buf)
         return NULL;

      out_len = mbstowcs(buf, str, len);
   }

   if (out_len == (size_t)-1)
   {
      free(buf);
      return NULL;
   }
#endif

   return buf;
}

/* Returned pointer MUST be freed by the caller if non-NULL. */
char* utf16_to_utf8_string_alloc(const wchar_t *str)
{
#ifdef _WIN32
   int len        = 0;
#else
   size_t len     = 0;
#endif
   char *buf      = NULL;

   if (!str || !*str)
      return NULL;

#ifdef _WIN32
   {
      UINT code_page = CP_UTF8;
      len            = WideCharToMultiByte(code_page,
            0, str, -1, NULL, 0, NULL, NULL);

      /* fallback to ANSI codepage instead */
      if (!len)
      {
         code_page   = CP_ACP;
         len         = WideCharToMultiByte(code_page,
               0, str, -1, NULL, 0, NULL, NULL);
      }

      buf = (char*)calloc(len, sizeof(char));

      if (!buf)
         return NULL;

      if (WideCharToMultiByte(code_page,
            0, str, -1, buf, len, NULL, NULL) < 0)
      {
         free(buf);
         return NULL;
      }
   }
#else
   /* NOTE: For now, assume non-Windows platforms' 
    * locale is already UTF-8. */
   len = wcstombs(NULL, str, 0) + 1;

   if (len)
   {
      buf = (char*)calloc(len, sizeof(char));

      if (!buf)
         return NULL;

      if (wcstombs(buf, str, len) == (size_t)-1)
      {
         free(buf);
         return NULL;
      }
   }
#endif

   return buf;
}
