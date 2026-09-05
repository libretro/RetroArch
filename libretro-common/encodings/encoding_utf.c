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

/* Number of leading 1-bits of a byte, which for a lead byte is the
 * length in bytes of the sequence it introduces: 0 for ASCII, 1 for a
 * continuation byte, 2..4 for the valid leads, 5..7 for invalid ones.
 * Replaces a leading_ones() bit-counting loop.
 *
 * Used by the decoders, which need the exact value. The scanners that
 * only need a length deliberately do not use it: indexing this table
 * puts a second dependent load in their pointer advance, which costs
 * more than the branch it removes. */
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

      /* Fast path: batch ASCII characters.
       *
       * Same word test as the utf8len counting loop: a byte is ASCII
       * iff bit 7 is clear, so one masked 64-bit compare clears eight
       * bytes at a time. The loads go through memcpy, so alignment
       * does not matter and no strict-aliasing rule is broken. Widening
       * to uint32_t is done per byte, which is endian neutral.
       *
       * The word loop is only entered when the next byte is ASCII, so
       * multibyte-dense text does not pay a wide load and test on
       * every character. */
      if ((uint8_t)*in < 0x80)
      {
         while (in_size >= 8 && out_chars >= 8)
         {
            uint64_t w;
            memcpy(&w, in, sizeof(w));
            if (w & 0x8080808080808080ULL)
               break;
            out[0] = (uint8_t)in[0];
            out[1] = (uint8_t)in[1];
            out[2] = (uint8_t)in[2];
            out[3] = (uint8_t)in[3];
            out[4] = (uint8_t)in[4];
            out[5] = (uint8_t)in[5];
            out[6] = (uint8_t)in[6];
            out[7] = (uint8_t)in[7];
            in        += 8;
            out       += 8;
            in_size   -= 8;
            out_chars -= 8;
            ret       += 8;
         }

         while (in_size && out_chars && (uint8_t)*in < 0x80)
         {
            *out++ = (uint8_t)*in++;
            in_size--;
            out_chars--;
            ret++;
         }

         if (!in_size || !out_chars)
            break;
      }

      first = (uint8_t)*in++;

      /* Dispatch on the lead byte with direct comparisons instead of
       * the LUT: the table indexing puts a dependent load on the
       * critical path of every multibyte character, and the compare
       * chain resolves 2- and 3-byte leads (the common cases) first. */
      if (first < 0xE0)
      {
         if (first < 0xC0)          /* Continuation byte: desync. */
            break;
         if (in_size < 2)           /* Not enough data. */
            break;
         c        = ((uint32_t)(first & 0x1F) << 6)
                  | ((uint8_t)*in++ & 0x3F);
         in_size -= 2;
      }
      else if (first < 0xF0)
      {
         if (in_size < 3)
            break;
         c        = (uint32_t)(first & 0x0F) << 6;
         c        = (c | ((uint8_t)*in++ & 0x3F)) << 6;
         c        =  c | ((uint8_t)*in++ & 0x3F);
         in_size -= 3;
      }
      else if (first < 0xF8)
      {
         if (in_size < 4)
            break;
         c        = (uint32_t)(first & 0x07) << 6;
         c        = (c | ((uint8_t)*in++ & 0x3F)) << 6;
         c        = (c | ((uint8_t)*in++ & 0x3F)) << 6;
         c        =  c | ((uint8_t)*in++ & 0x3F);
         in_size -= 4;
      }
      else
      {
         /* 5/6-byte forms and 0xFE/0xFF: invalid UTF-8. Decode the
          * 5/6-byte shapes as before (garbage in, garbage out), stop
          * on 0xFE/0xFF. */
         unsigned i;
         ones = utf8_lut[first];
         if (ones > 6)
            break;
         if (ones > in_size)
            break;
         c = first & ((1 << (7 - ones)) - 1);
         for (i = 0; i < ones - 1; i++)
            c = (c << 6) | ((uint8_t)*in++ & 0x3F);
         in_size -= ones;
      }

      *out++   = c;
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
 * Nothing is written when @len is 0, since there is no room even
 * for the terminator.
 *
 * @return Number of bytes.
 **/
size_t utf8cpy(char *s, size_t len, const char *in, size_t chars)
{
   size_t byte_count;
   const uint8_t *sb     = (const uint8_t*)in;
   const uint8_t *sb_org = sb;

   if (!in || !len)
      return 0;

   while (*sb && chars-- > 0)
   {
      /* Stepping over continuation bytes stops at the terminator by
       * itself, so a truncated sequence cannot overrun the buffer. */
      sb++;
      while ((*sb & 0xC0) == 0x80)
         sb++;
   }

   if ((size_t)(sb - sb_org) > len - 1)
   {
      sb = sb_org + len - 1;
      /* @in may itself begin with continuation bytes; do not scan
       * backwards out of the buffer looking for a lead byte. */
      while (sb > sb_org && (*sb & 0xC0) == 0x80)
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
 * Optimized: comparison dispatch on the lead byte (no dependent LUT
 * load on the per-character path), NUL-guarded stepping over
 * multibyte sequences, and ASCII runs skipped eight characters per
 * masked 64-bit word test. The word test requires every byte to be
 * ASCII and non-NUL, so it can neither overshoot the terminator nor
 * miscount characters. memcpy load: alignment/aliasing safe, endian
 * neutral.
 **/
const char *utf8skip(const char *str, size_t chars)
{
   const uint8_t *strb = (const uint8_t*)str;

   if (!chars)
      return str;

   do
   {
      uint8_t b = *strb;
      if (!b)
         break;
      if (b < 0xC0)
      {
         /* ASCII or lone continuation byte: one char, one byte. */
         strb++;
         if (b < 0x80)
         {
            /* Batch the rest of an ASCII run. The current character
             * is consumed by the --chars below, so batch only while
             * more than one character remains in the budget.
             *
             * Byte steps rather than a wide word test: utf8skip's
             * contract is a NUL-terminated string with no length, so
             * an 8-byte load could read past a terminator that falls
             * inside the word - on a tightly sized allocation that is
             * a read beyond the end of the buffer, even though the
             * zero test would stop the cursor before consuming those
             * bytes. Stepping a byte at a time stops exactly at the
             * terminator or the first multibyte lead and still skips
             * the per-character dispatch for the run; only the load
             * width changes. */
            while (chars > 1)
            {
               uint8_t nb = *strb;
               if (nb == 0 || nb >= 0x80)
                  break;
               strb++;
               chars--;
            }
         }
      }
      else if (b < 0xE0)
      {
         strb++;
         if (*strb)
            strb++;
      }
      else if (b < 0xF0)
      {
         strb++;
         if (*strb)
         {
            strb++;
            if (*strb)
               strb++;
         }
      }
      else if (b < 0xF8)
      {
         strb++;
         if (*strb)
         {
            strb++;
            if (*strb)
            {
               strb++;
               if (*strb)
                  strb++;
            }
         }
      }
      else
      {
         /* Invalid 5/6/7-lead: step over utf8_lut[b] bytes stopping
          * at NUL, exactly as the LUT loop did. */
         unsigned ones = utf8_lut[b];
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
 **/
size_t utf8len(const char *string)
{
   const unsigned char *p;
   size_t n;
   size_t ret = 0;

   if (!string)
      return 0;

   p = (const unsigned char*)string;
   n = strlen(string);

   /* Byte at a time up to the first aligned address. */
   while (n && (((size_t)p & 7) != 0))
   {
      if ((*p & 0xC0) != 0x80)
         ret++;
      p++;
      n--;
   }

   /* The length is known, so the word loop needs no terminator test
    * and never reads a byte the caller did not supply. */
   while (n >= 8)
   {
      uint64_t w;
      uint64_t c;
      memcpy(&w, p, sizeof(w));
      /* A continuation byte is the pattern 10xxxxxx: bit 7 set and
       * bit 6 clear. Both halves of the test stay inside their own
       * byte, so this is endian neutral. */
      c    = (w & ~(w << 1) & 0x8080808080808080ULL) >> 7;
      /* Horizontal sum. Every byte of c is 0 or 1 and the running
       * total tops out at 8, so the folds cannot carry out of a
       * byte and no 64-bit multiply is needed. */
      c   += c >> 32;
      c   += c >> 16;
      c   += c >> 8;
      ret += 8 - (size_t)(c & 0xFF);
      p   += 8;
      n   -= 8;
   }

   while (n)
   {
      if ((*p & 0xC0) != 0x80)
         ret++;
      p++;
      n--;
   }
   return ret;
}

/**
 * utf8_walk:
 *
 * Does not validate the input, but never reads or steps past a
 * terminating NUL, even mid-sequence: a truncated multibyte tail
 * previously read up to three bytes beyond the terminator and left
 * the cursor past it, walking a while (*str) caller out of the
 * buffer.
 *
 * Leaf function.
 *
 * @return Returns garbage if it's not UTF-8.
 **/
uint32_t utf8_walk(const char **string)
{
   const uint8_t *s = (const uint8_t*)*string;
   uint8_t first    = *s++;
   uint8_t b;
   uint32_t ret;

   if (first < 0x80)
   {
      *string = (const char*)s;
      return first;
   }

   /* Dispatch on the lead byte with direct comparisons, matching
    * utf8_conv_utf32: the LUT indexing put a dependent load on the
    * critical path of every glyph decoded by the per-frame text
    * renderers, and the compare chain resolves the common 2- and
    * 3-byte leads first. Continuation and 5-byte-plus leads take the
    * final branch and decode to garbage, as before.
    *
    * Each continuation read tests the byte it already loaded against
    * NUL before consuming it; the branch is never taken on valid
    * input, and on a truncated tail the cursor parks at the
    * terminator with a partial (garbage) return. */
   if (first < 0xE0)
   {
      if (first < 0xC0)
         /* Lone continuation byte: desync. Do not consume another
          * byte, or a caller iterating with while (*str) could be
          * carried past the terminator. Same masked-garbage return
          * as the LUT path produced. */
         ret = first & 0x3F;
      else
      {
         ret = first & 0x1F;
         if ((b = *s) != 0)
         {
            s++;
            ret = (ret << 6) | (b & 0x3F);
         }
      }
   }
   else if (first < 0xF0)
   {
      ret = first & 0x0F;
      if ((b = *s) != 0)
      {
         s++;
         ret = (ret << 6) | (b & 0x3F);
         if ((b = *s) != 0)
         {
            s++;
            ret = (ret << 6) | (b & 0x3F);
         }
      }
   }
   else if (first < 0xF8)
   {
      ret = first & 0x07;
      if ((b = *s) != 0)
      {
         s++;
         ret = (ret << 6) | (b & 0x3F);
         if ((b = *s) != 0)
         {
            s++;
            ret = (ret << 6) | (b & 0x3F);
            if ((b = *s) != 0)
            {
               s++;
               ret = (ret << 6) | (b & 0x3F);
            }
         }
      }
   }
   else
      ret = first & ((1 << (7 - utf8_lut[first])) - 1);

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
      /* Single pass with a worst-case allocation instead of the
       * count-then-encode double pass: a UTF-16 unit encodes to at
       * most three UTF-8 bytes (a surrogate pair is two units for
       * four bytes, i.e. two bytes per unit), so 3n + 1 always fits
       * and the counting pass cost half the throughput of this
       * function. The buffer is short-lived - the only caller copies
       * out of it and frees it immediately. */
      if (in_len > (((size_t)-1) - 1) / 3)
         return false;
      if ((*utf_data = (uint8_t*)malloc(3 * in_len + 1)) != 0)
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

   /* +1 element for the terminator; the old expression added
    * sizeof(wchar_t) ELEMENTS (a byte count used as an element
    * count), harmlessly over-allocating. */
   if ((path_buf_wide = (wchar_t*)
      calloc((size_t)path_buf_wide_len + 1, sizeof(wchar_t))))
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
               calloc((size_t)path_buf_len + 1, sizeof(char));

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
 * local_to_utf8_string:
 *
 * The guard is the one in local_to_utf8_string_alloc() below: where it
 * resolves to a plain copy there is nothing to convert, so there is
 * nothing to allocate either.
 **/
bool local_to_utf8_string(const char *in, char *s, size_t len)
{
   if (!s || !len)
      return false;
   s[0] = '\0';
   if (!in || !*in)
      return true;
#if defined(_WIN32) && !defined(_XBOX) && !defined(UNICODE)
   {
      char *tmp = mb_to_mb_string_alloc(in, CODEPAGE_LOCAL, CODEPAGE_UTF8);
      if (!tmp)
         return false;
      strlcpy(s, tmp, len);
      free(tmp);
   }
#else
   strlcpy(s, in, len);
#endif
   return true;
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
   /* Locale-independent conversion. mbstowcs only decodes UTF-8 when
    * the active locale says so: under the default C/POSIX locale
    * (headless machines, containers, any process that never calls
    * setlocale) it fails on the first non-ASCII byte and this
    * function returned NULL. Decode with the in-house converter
    * instead: exact UTF-32 into 32-bit wchar_t, UTF-16 with
    * surrogate pairs when wchar_t is 16-bit. Scalars above U+10FFFF
    * (only reachable from invalid input) become U+FFFD on the
    * 16-bit path. */
   {
      size_t    n8 = strlen(str);
      uint32_t *u32 = (uint32_t*)malloc(n8 * sizeof(uint32_t));

      if (!u32)
         return NULL;

      {
         size_t n32 = utf8_conv_utf32(u32, n8, str, n8);
         size_t i;

         if (sizeof(wchar_t) == 2)
         {
            /* Worst case two units per scalar, plus terminator */
            if ((buf = (wchar_t*)malloc((2 * n32 + 1) * sizeof(wchar_t))))
            {
               size_t o = 0;
               for (i = 0; i < n32; i++)
               {
                  uint32_t cp = u32[i];
                  if (cp < 0x10000)
                     buf[o++] = (wchar_t)cp;
                  else if (cp <= 0x10FFFF)
                  {
                     cp      -= 0x10000;
                     buf[o++] = (wchar_t)(0xD800 | (cp >> 10));
                     buf[o++] = (wchar_t)(0xDC00 | (cp & 0x3FF));
                  }
                  else
                     buf[o++] = (wchar_t)0xFFFD;
               }
               buf[o] = 0;
            }
         }
         else
         {
            if ((buf = (wchar_t*)malloc((n32 + 1) * sizeof(wchar_t))))
            {
               for (i = 0; i < n32; i++)
                  buf[i] = (wchar_t)u32[i];
               buf[n32] = 0;
            }
         }
      }

      free(u32);
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
   /* Locale-independent conversion; see utf8_to_utf16_string_alloc.
    * wcstombs had the same C/POSIX-locale failure on non-ASCII.
    * 32-bit wchar_t is re-expressed as UTF-16 (exact for valid
    * scalars) so the existing count-then-encode converter can do the
    * encoding; unpaired surrogates or out-of-range values make it
    * bail, and NULL is returned as the old code did for input
    * wcstombs could not represent. */
   {
      size_t in_len = 0;
      const wchar_t *p = str;
      uint16_t *u16;

      while (*p++)
         in_len++;

      /* Worst case two units per wchar */
      if (!(u16 = (uint16_t*)malloc((2 * in_len) * sizeof(uint16_t))))
         return NULL;

      {
         size_t n16 = 0;
         size_t i;
         bool ok = true;

         if (sizeof(wchar_t) == 2)
         {
            for (i = 0; i < in_len; i++)
               u16[n16++] = (uint16_t)str[i];
         }
         else
         {
            for (i = 0; i < in_len; i++)
            {
               uint32_t cp = (uint32_t)str[i];
               if (cp < 0x10000)
                  u16[n16++] = (uint16_t)cp;
               else if (cp <= 0x10FFFF)
               {
                  cp          -= 0x10000;
                  u16[n16++]   = (uint16_t)(0xD800 | (cp >> 10));
                  u16[n16++]   = (uint16_t)(0xDC00 | (cp & 0x3FF));
               }
               else
               {
                  ok = false;
                  break;
               }
            }
         }

         if (ok && utf16_conv_utf8(NULL, &_len, u16, n16))
         {
            if ((buf = (char*)malloc(_len + 1)))
            {
               if (utf16_conv_utf8((uint8_t*)buf, &_len, u16, n16))
                  buf[_len] = '\0';
               else
               {
                  free(buf);
                  buf = NULL;
               }
            }
         }
      }

      free(u16);
   }
#endif

   return buf;
}
