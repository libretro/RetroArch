/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (stdstring.c).
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
#include <ctype.h>
#include <string.h>

#include <compat/strl.h>
#include <string/stdstring.h>
#include <encodings/utf.h>

const uint8_t lr_char_props[256] = {
	/*x0   x1   x2   x3   x4   x5   x6   x7   x8   x9   xA   xB   xC   xD   xE   xF */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x00,0x00,0x80,0x00,0x00, /* 0x                  */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 1x                  */
	0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 2x  !"#$%&'()*+,-./ */
	0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x00,0x00,0x00,0x00,0x00,0x00, /* 3x 0123456789:;<=>? */
	0x00,0x23,0x23,0x23,0x23,0x23,0x23,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22, /* 4x @ABCDEFGHIJKLMNO */
	0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x00,0x00,0x00,0x00,0x08, /* 5x PQRSTUVWXYZ[\]^_ */
	0x00,0x25,0x25,0x25,0x25,0x25,0x25,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24, /* 6x `abcdefghijklmno */
	0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x00,0x00,0x00,0x00,0x00, /* 7x pqrstuvwxyz{|}~  */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 8x                  */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* 9x                  */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* Ax                  */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* Bx                  */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* Cx                  */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* Dx                  */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* Ex                  */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* Fx                  */
};

char *string_to_upper(char *s)
{
   char *cs = (char *)s;
   /* Use lr_char_props bit 0x01 (ISALPHA) + bit 0x04 (islower) to detect
    * lowercase ASCII letters without a function call, then clear bit 5 (0x20)
    * to convert a-z -> A-Z.  Non-ASCII bytes are untouched. */
   for ( ; *cs != '\0'; cs++)
   {
      unsigned char uc = (unsigned char)*cs;
      if ((lr_char_props[uc] & 0x05) == 0x05) /* lower alpha */
         *cs = (char)(uc & ~0x20);
   }
   return s;
}

char *string_to_lower(char *s)
{
   char *cs = (char *)s;
   /* bit 0x03 set means upper alpha (0x23 & 0x03 == 3, 0x25 & 0x03 == 1... use
    * mask 0x02 to distinguish upper from lower: upper has bit 0x02, lower doesn't) */
   for ( ; *cs != '\0'; cs++)
   {
      unsigned char uc = (unsigned char)*cs;
      if ((lr_char_props[uc] & 0x06) == 0x02) /* upper alpha: bits 0x02 set, 0x04 clear */
         *cs = (char)(uc | 0x20);
   }
   return s;
}

char *string_ucwords(char *s)
{
   /* Single-pass: capitalise the first character of every word.
    * 'cap' starts true so the very first non-space character is uppercased. */
   char *cs  = (char *)s;
   int   cap = 1;
   for ( ; *cs != '\0'; cs++)
   {
      if (*cs == ' ')
         cap = 1;
      else if (cap)
      {
         *cs = (char)toupper((unsigned char)*cs);
         cap = 0;
      }
   }
   return s;
}

char *string_replace_substring(
      const char *in,          size_t in_len,
      const char *pattern,     size_t pattern_len,
      const char *replacement, size_t replacement_len)
{
   /* Single-pass implementation: build output in one scan rather than
    * first counting hits and then copying.  We use a dynamically grown
    * buffer so we never pay for a second full traversal of the input. */
   size_t      out_cap  = 0;
   size_t      out_len  = 0;
   char       *out      = NULL;
   char       *tmp      = NULL;
   const char *inat     = NULL;
   const char *inprev   = NULL;
   size_t      seg_len  = 0;
   size_t      need     = 0;

   /* Guard against NULL input string. */
   if (!in)
      return NULL;

   /* If pattern or replacement is NULL, duplicate in. */
   if (!pattern || !replacement)
      return strdup(in);

   /* A zero-length pattern would cause an infinite loop. */
   if (pattern_len == 0)
      return strdup(in);

   /* Initial capacity: same as input (common case: zero or few matches). */
   out_cap = in_len + 1;
   if (!(out = (char *)malloc(out_cap)))
      return NULL;

   inat   = in;
   inprev = in;

   while ((inat = strstr(inat, pattern)))
   {
      seg_len = (size_t)(inat - inprev);
      need    = out_len + seg_len + replacement_len;
      if (need >= out_cap)
      {
         /* Grow: at least double, or exactly what we need. */
         out_cap = (need * 2) + 1;
         if (!(tmp = (char *)realloc(out, out_cap)))
         {
            free(out);
            return NULL;
         }
         out = tmp;
      }
      memcpy(out + out_len, inprev, seg_len);
      out_len += seg_len;
      memcpy(out + out_len, replacement, replacement_len);
      out_len += replacement_len;
      inat   += pattern_len;
      inprev  = inat;
   }

   /* Append the tail after the last match. */
   seg_len = in_len - (size_t)(inprev - in);
   need    = out_len + seg_len + 1;
   if (need > out_cap)
   {
      if (!(tmp = (char *)realloc(out, need)))
      {
         free(out);
         return NULL;
      }
      out = tmp;
   }
   memcpy(out + out_len, inprev, seg_len);
   out[out_len + seg_len] = '\0';

   return out;
}

/**
 * string_trim_whitespace_left:
 *
 * Remove leading whitespaces
 **/
char *string_trim_whitespace_left(char *const s)
{
   if (s && *s)
   {
      char *current = s;

      while (*current && ISSPACE((unsigned char)*current))
         ++current;

      if (s != current)
      {
         /* Avoid strlen: scan to end while copying forward. */
         char *dst = s;
         while (*current)
            *dst++ = *current++;
         *dst = '\0';
      }
   }

   return s;
}

/**
 * string_trim_whitespace_right:
 *
 * Remove trailing whitespaces
 **/
char *string_trim_whitespace_right(char *const s)
{
   if (s && *s)
   {
      size_t _len    = strlen(s);
      char  *current = s + _len - 1;

      while (current != s && ISSPACE((unsigned char)*current))
      {
         --current;
         --_len;
      }

      current[ISSPACE((unsigned char)*current) ? 0 : 1] = '\0';
   }

   return s;
}

/**
 * string_trim_whitespace:
 *
 * Remove leading and trailing whitespaces
 **/
char *string_trim_whitespace(char *const s)
{
   string_trim_whitespace_right(s);  /* order matters */
   string_trim_whitespace_left(s);

   return s;
}

/**
 * word_wrap:
 * @s                  : pointer to destination buffer.
 * @len                : size of destination buffer.
 * @src                : pointer to input string.
 * @line_width         : max number of characters per line.
 * @wideglyph_width    : not used, but is necessary to keep
 *                       compatibility with word_wrap_wideglyph().
 * @max_lines          : max lines of destination string.
 *                       0 means no limit.
 *
 * Wraps string specified by @src to destination buffer
 * specified by @s and @len.
 * This function assumes that all glyphs in the string
 * have an on-screen pixel width similar to that of
 * regular Latin characters - i.e. it will not wrap
 * correctly any text containing so-called 'wide' Unicode
 * characters (e.g. CJK languages, emojis, etc.).
 **/
size_t word_wrap(
      char *s,         size_t len,
      const char *src, size_t src_len,
      int line_width,  int wideglyph_width, unsigned max_lines)
{
   char *last_space    = NULL;
   unsigned counter    = 0;
   unsigned lines      = 1;
   const char *src_end = src + src_len;

   /* Prevent buffer overflow */
   if (len < src_len + 1)
      return 0;

   /* Early return if src string length is less
    * than line width */
   if (src_len < (size_t)line_width)
      return strlcpy(s, src, len);

   while (*src != '\0')
   {
      unsigned char_len = (unsigned)(utf8skip(src, 1) - src);
      counter++;

      if (*src == ' ')
         last_space = s; /* Remember the location of the whitespace */
      else if (*src == '\n')
      {
         /* If newlines embedded in the input,
          * reset the index */
         lines++;
         counter   = 0;

         /* Early return if remaining src string
          * length is less than line width */
         if (src_end - src <= line_width)
            return strlcpy(s, src, len);
     }

      while (char_len--)
         *s++ = *src++;

      if (counter >= (unsigned)line_width)
      {
         counter = 0;

         if (last_space && (max_lines == 0 || lines < max_lines))
         {
            /* Replace nearest (previous) whitespace
             * with newline character */
            *last_space = '\n';
            lines++;

            src        -= s - last_space - 1;
            s           = last_space + 1;
            last_space  = NULL;

            /* Early return if remaining src string
             * length is less than line width */
            if (src_end - src < line_width)
               return strlcpy(s, src, len);
         }
      }
   }

   *s = '\0';
   return 0;
}

/**
 * word_wrap_wideglyph:
 * @dst                : pointer to destination buffer.
 * @len                : size of destination buffer.
 * @src                : pointer to input string.
 * @line_width         : max number of characters per line.
 * @wideglyph_width    : effective width of 'wide' Unicode glyphs.
 *                       the value here is normalised relative to the
 *                       typical on-screen pixel width of a regular
 *                       Latin character:
 *                       - a regular Latin character is defined to
 *                         have an effective width of 100
 *                       - wideglyph_width = 100 * (wide_character_pixel_width / latin_character_pixel_width)
 *                       - e.g. if 'wide' Unicode characters in 'src'
 *                         have an on-screen pixel width twice that of
 *                         regular Latin characters, wideglyph_width
 *                         would be 200
 * @max_lines          : max lines of destination string.
 *                       0 means no limit.
 *
 * Wraps string specified by @src to destination buffer
 * specified by @dst and @len.
 * This function assumes that all glyphs in the string
 * are:
 * - EITHER 'non-wide' Unicode glyphs, with an on-screen
 *   pixel width similar to that of regular Latin characters
 * - OR 'wide' Unicode glyphs (e.g. CJK languages, emojis, etc.)
 *   with an on-screen pixel width defined by @wideglyph_width
 * Note that wrapping may occur in inappropriate locations
 * if @src string contains 'wide' Unicode characters whose
 * on-screen pixel width deviates greatly from the set
 * @wideglyph_width value.
 **/
size_t word_wrap_wideglyph(char *s, size_t len,
      const char *src, size_t src_len, int line_width,
      int wideglyph_width, unsigned max_lines)
{
   char         *dst            = s;
   char         *lastspace      = NULL;
   /* Points to the byte immediately AFTER the last wide glyph in dst.
    * A '\n' is written there when we break on a wide glyph. */
   char         *lastwideglyph  = NULL;
   const char   *lastwg_src     = NULL;  /* matching position in src */

   const char   *src_end        = src + src_len;
   unsigned      lines          = 1;
   unsigned      counter_norm   = 0;

   /* Pre-compute normalised thresholds once */
   const unsigned line_norm     = (unsigned)line_width * 100;
   const unsigned wide_extra    = (wideglyph_width > 100)
                                ? (unsigned)(wideglyph_width - 100) : 0;

   /* ── Fast-path: entire string fits on one line ── */
   if (src_len < (size_t)line_width)
      return strlcpy(s, src, len);

   while (src < src_end)
   {
      /* ── Decode one UTF-8 codepoint ── */
      const char *next     = utf8skip(src, 1);
      unsigned    char_len = (unsigned)(next - src);

      /* Bounds guard – before any mutation */
      if (char_len >= len)
         break;

      /* ── NUL terminator ── */
      if (*src == '\0')
         break;

      /* ── Classify character ── */
      if (*src == '\n')
      {
         /* Embedded newline: copy, reset line state */
         *dst = '\n';
         dst++;
         src++;
         len--;
         lines++;
         counter_norm  = 0;
         lastspace     = NULL;
         lastwideglyph = NULL;
         lastwg_src    = NULL;

         if ((size_t)(src_end - src) < (size_t)line_width)
            return strlcpy(dst, src, len);
         continue;
      }

      if (*src == ' ')
         lastspace = dst;
      else if (char_len >= 3)
      {
         /* Record wrap point AFTER the glyph */
         lastwideglyph = dst + char_len;
         lastwg_src    = next;                /* == src + char_len */
      }

      /* ── Accumulate normalised width ──
       * Branchless: (char_len >= 3) is 0 or 1; multiply by wide_extra. */
      counter_norm += 100 + ((char_len >= 3) ? wide_extra : 0);

      memcpy(dst, src, char_len);
      dst += char_len;
      src  = next;
      len -= char_len;

      /* ── Hot path: no wrap needed ── */
      if (counter_norm < line_norm)
         continue;

      /* ── Line overflow ── */
      counter_norm = 0;

      if (max_lines != 0 && lines >= max_lines)
         break;   /* Truncate: hard line limit reached */

      if (lastwideglyph && (!lastspace || lastwideglyph > lastspace))
      {
         /* Insert '\n' after the wide glyph; rewind src to just past it */
         *lastwideglyph = '\n';
         lines++;
         src           = lastwg_src;
         dst           = lastwideglyph + 1;
         lastwideglyph = NULL;
         lastwg_src    = NULL;
         lastspace     = NULL;

         if ((size_t)(src_end - src) < (size_t)line_width)
            return strlcpy(dst, src, len);
      }
      else if (lastspace)
      {
         /* Replace the space with '\n'; rewind src to the char after it */
         unsigned rewind = (unsigned)(dst - lastspace) - 1;
         *lastspace = '\n';
         lines++;
         src       -= rewind;
         dst        = lastspace + 1;
         lastspace  = NULL;

         if ((size_t)(src_end - src) < (size_t)line_width)
            return strlcpy(dst, src, len);
      }
      /* No break point found: continue filling current line */
   }

   *dst = '\0';
   return 0;
}

/**
 * string_tokenize:
 *
 * Splits string into tokens separated by @delim
 * > Returned token string must be free()'d
 * > Returns NULL if token is not found
 * > After each call, @str is set to the position after the
 *   last found token
 * > Tokens *include* empty strings
 * Usage example:
 *    char *str      = "1,2,3,4,5,6,7,,,10,";
 *    char **str_ptr = &str;
 *    char *token    = NULL;
 *    while ((token = string_tokenize(str_ptr, ",")))
 *    {
 *        printf("%s\n", token);
 *        free(token);
 *        token = NULL;
 *    }
 **/
char *string_tokenize(char **str, const char *delim)
{
   char   *str_ptr   = NULL;
   size_t  delim_len = 0;
   size_t  token_len = 0;
   char   *token     = NULL;

   if (!str || string_is_empty(delim))
      return NULL;

   if (!(str_ptr = *str))
      return NULL;

   delim_len = strlen(delim);

   if (delim_len == 1)
   {
      /* Fast path: single-character delimiter – use memchr. */
      const char *found = (const char *)memchr(
            str_ptr, (unsigned char)delim[0], strlen(str_ptr));
      token_len = found ? (size_t)(found - str_ptr) : strlen(str_ptr);
   }
   else
   {
      /* Multi-character delimiter. */
      while (str_ptr[token_len] != '\0')
      {
         if (memcmp(str_ptr + token_len, delim, delim_len) == 0)
            break;
         token_len++;
      }
   }

   if (!(token = (char *)malloc(token_len + 1)))
      return NULL;

   memcpy(token, str_ptr, token_len);
   token[token_len] = '\0';

   /* Advance past delimiter, or set to NULL if at end. */
   *str = (str_ptr[token_len] != '\0')
      ? str_ptr + token_len + delim_len
      : NULL;

   return token;
}

/**
 * string_remove_all_chars:
 * @s                 : input string (must be non-NULL, otherwise UB)
 *
 * Leaf function.
 *
 * Removes every instance of character @c from @s
 **/
void string_remove_all_chars(char *s, char c)
{
   char *read_ptr  = s;
   char *write_ptr = s;

   while (*read_ptr != '\0')
   {
      /* Only write if the character is not the one to remove */
      if (*read_ptr != c)
         *write_ptr++ = *read_ptr;
      read_ptr++;
   }

   *write_ptr = '\0';
}

/**
 * string_replace_all_chars:
 * @s                  : input string (must be non-NULL, otherwise UB)
 * @find               : character to find
 * @replace            : character to replace @find with
 *
 * Replaces every instance of character @find in @s
 * with character @replace
 **/
void string_replace_all_chars(char *s, char find, char replace)
{
   char *str_ptr = s;
   while ((str_ptr = strchr(str_ptr, find)))
      *str_ptr++ = replace;
}

/**
 * string_to_unsigned:
 * @str                : input string
 *
 * Converts string to unsigned integer.
 *
 * @return 0 if string is invalid, otherwise > 0
 **/
unsigned string_to_unsigned(const char *str)
{
   const char *ptr = NULL;

   if (string_is_empty(str))
      return 0;

   for (ptr = str; *ptr != '\0'; ptr++)
   {
      if (!ISDIGIT((unsigned char)*ptr))
         return 0;
   }

   return (unsigned)strtoul(str, NULL, 10);
}

/**
 * string_hex_to_unsigned:
 * @str                : input string (must be non-NULL, otherwise UB)
 *
 * Converts hexadecimal string to unsigned integer.
 * Handles optional leading '0x'.
 *
 * @return 0 if string is invalid, otherwise > 0
 **/
unsigned string_hex_to_unsigned(const char *str)
{
   const char *hex_str = str;
   const char *ptr     = NULL;

   /* Remove leading '0x', if required */
   if (str[0] != '\0' && str[1] != '\0')
   {
      if (     (str[0] == '0')
           && ((str[1] == 'x')
           ||  (str[1] == 'X')))
      {
         hex_str = str + 2;
         if (string_is_empty(hex_str))
            return 0;
      }
   }
   else
      return 0;

   /* Check for valid characters */
   for (ptr = hex_str; *ptr != '\0'; ptr++)
   {
      if (!isxdigit((unsigned char)*ptr))
         return 0;
   }

   return (unsigned)strtoul(hex_str, NULL, 16);
}

/**
 * string_count_occurrences_single_character:
 *
 * Leaf function.
 *
 * Get the total number of occurrences of character @c in @str.
 *
 * @return Total number of occurrences of character @c
 */
int string_count_occurrences_single_character(const char *str, char c)
{
   unsigned count = 0;

   /* 4-way unrolled loop to exploit instruction-level parallelism. */
   for (; str[0] && str[1] && str[2] && str[3]; str += 4)
   {
      count += (str[0] == c);
      count += (str[1] == c);
      count += (str[2] == c);
      count += (str[3] == c);
   }
   /* Handle remaining bytes. */
   for (; *str; str++)
      count += (*str == c);

   return (int)count;
}

/**
 * string_replace_whitespace_with_single_character:
 *
 * Leaf function.
 *
 * Replaces all spaces with given character @c.
 **/
void string_replace_whitespace_with_single_character(char *s, char c)
{
   for (; *s; s++)
      if (ISSPACE(*s))
         *s = c;
}

/**
 * string_replace_multi_space_with_single_space:
 *
 * Leaf function.
 *
 * Replaces multiple spaces with a single space in a string.
 **/
void string_replace_multi_space_with_single_space(char *s)
{
   char *dst        = s;
   int   prev_space = 0;
   int   curr_space;

   for (; *s; s++)
   {
      curr_space = ISSPACE((unsigned char)*s);
      if (!(prev_space && curr_space))
         *dst++ = *s;
      prev_space = curr_space;
   }
   *dst = '\0';
}

/**
 * string_remove_all_whitespace:
 *
 * Leaf function.
 *
 * Remove all spaces from the given string.
 **/
void string_remove_all_whitespace(char *s, const char *str)
{
   for (; *str; str++)
      if (!ISSPACE(*str))
         *s++ = *str;
   *s = '\0';
}

/**
 * Retrieve the last occurance of the given character in a string.
 */
int string_index_last_occurance(const char *str, char c)
{
   const char *pos = strrchr(str, c);
   if (pos)
      return (int)(pos - str);
   return -1;
}

/**
 * string_find_index_substring_string:
 * @str                : input string (must be non-NULL, otherwise UB)
 * @substr             : substring to find in @str
 *
 * Find the position of substring @substr in string @str.
 **/
int string_find_index_substring_string(const char *str, const char *substr)
{
   const char *pos = strstr(str, substr);
   if (pos)
      return (int)(pos - str);
   return -1;
}

/**
 * string_copy_only_ascii:
 *
 * Leaf function.
 *
 * Strips non-ASCII characters from a string.
 **/
void string_copy_only_ascii(char *s, const char *str)
{
   for (; *str; str++)
      if (*str > 0x1F && *str < 0x7F)
         *s++ = *str;
   *s = '\0';
}
