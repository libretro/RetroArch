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
   for ( ; *cs != '\0'; cs++)
      *cs = toupper((unsigned char)*cs);
   return s;
}

char *string_to_lower(char *s)
{
   char *cs = (char *)s;
   for ( ; *cs != '\0'; cs++)
      *cs = tolower((unsigned char)*cs);
   return s;
}

char *string_ucwords(char *s)
{
   char *cs = (char *)s;
   for ( ; *cs != '\0'; cs++)
   {
      if (*cs == ' ')
         *(cs+1) = toupper((unsigned char)*(cs+1));
   }

   s[0] = toupper((unsigned char)s[0]);
   return s;
}

char *string_replace_substring(
      const char *in,          size_t in_len,
      const char *pattern,     size_t pattern_len,
      const char *replacement, size_t replacement_len)
{
   size_t outlen;
   size_t numhits     = 0;
   const char *inat   = NULL;
   const char *inprev = NULL;
   char          *out = NULL;
   char        *outat = NULL;

   /* if either pattern or replacement is NULL,
    * duplicate in and let caller handle it. */
   if (!pattern || !replacement)
      return strdup(in);

   inat            = in;

   while ((inat = strstr(inat, pattern)))
   {
      inat += pattern_len;
      numhits++;
   }

   outlen = in_len - pattern_len * numhits + replacement_len*numhits;

   if (!(out = (char *)malloc(outlen+1)))
      return NULL;

   outat           = out;
   inat            = in;
   inprev          = in;

   while ((inat = strstr(inat, pattern)))
   {
      memcpy(outat, inprev, inat-inprev);
      outat += inat-inprev;
      memcpy(outat, replacement, replacement_len);
      outat += replacement_len;
      inat  += pattern_len;
      inprev = inat;
   }
   strcpy(outat, inprev);

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
      size_t _len    = strlen(s);
      char *current  = s;

      while (*current && ISSPACE((unsigned char)*current))
      {
         ++current;
         --_len;
      }

      if (s != current)
         memmove(s, current, _len + 1);
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
   char *lastspace                   = NULL;
   char *lastwideglyph               = NULL;
   const char *src_end               = src + src_len;
   unsigned lines                    = 1;
   /* 'line_width' means max numbers of characters per line,
    * but this metric is only meaningful when dealing with
    * 'regular' glyphs that have an on-screen pixel width
    * similar to that of regular Latin characters.
    * When handing so-called 'wide' Unicode glyphs, it is
    * necessary to consider the actual on-screen pixel width
    * of each character.
    * In order to do this, we create a distinction between
    * regular Latin 'non-wide' glyphs and 'wide' glyphs, and
    * normalise all values relative to the on-screen pixel
    * width of regular Latin characters:
    * - Regular 'non-wide' glyphs have a normalised width of 100
    * - 'line_width' is therefore normalised to 100 * (width_in_characters)
    * - 'wide' glyphs have a normalised width of
    *   100 * (wide_character_pixel_width / latin_character_pixel_width)
    * - When a character is detected, the position in the current
    *   line is incremented by the regular normalised width of 100
    * - If that character is then determined to be a 'wide'
    *   glyph, the position in the current line is further incremented
    *   by the difference between the normalised 'wide' and 'non-wide'
    *   width values */
   unsigned counter_normalized       = 0;
   int line_width_normalized         = line_width * 100;
   int additional_counter_normalized = wideglyph_width - 100;

   /* Early return if src string length is less
    * than line width */
   if (src_end - src < line_width)
      return strlcpy(s, src, len);

   while (*src != '\0')
   {
      unsigned char_len   = (unsigned)(utf8skip(src, 1) - src);
      counter_normalized += 100;

      /* Prevent buffer overflow */
      if (char_len >= len)
         break;

      if (*src == ' ')
         lastspace          = s; /* Remember the location of the whitespace */
      else if (*src == '\n')
      {
         /* If newlines embedded in the input,
          * reset the index */
         lines++;
         counter_normalized = 0;

         /* Early return if remaining src string
          * length is less than line width */
         if (src_end - src <= line_width)
            return strlcpy(s, src, len);
      }
      else if (char_len >= 3)
      {
         /* Remember the location of the first byte
          * whose length as UTF-8 >= 3*/
         lastwideglyph       = s;
         counter_normalized += additional_counter_normalized;
      }

      len -= char_len;
      while (char_len--)
         *s++ = *src++;

      if (counter_normalized >= (unsigned)line_width_normalized)
      {
         counter_normalized = 0;

         if (max_lines != 0 && lines >= max_lines)
            continue;
         else if (lastwideglyph && (!lastspace || lastwideglyph > lastspace))
         {
            /* Insert newline character */
            *lastwideglyph = '\n';
            lines++;
            src           -= s - lastwideglyph;
            s              = lastwideglyph + 1;
            lastwideglyph  = NULL;

            /* Early return if remaining src string
             * length is less than line width */
            if (src_end - src <= line_width)
               return strlcpy(s, src, len);
         }
         else if (lastspace)
         {
            /* Replace nearest (previous) whitespace
             * with newline character */
            *lastspace = '\n';
            lines++;
            src       -= s - lastspace - 1;
            s          = lastspace + 1;
            lastspace  = NULL;

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
char* string_tokenize(char **str, const char *delim)
{
   /* Taken from https://codereview.stackexchange.com/questions/216956/strtok-function-thread-safe-supports-empty-tokens-doesnt-change-string# */
   char *str_ptr    = NULL;
   char *delim_ptr  = NULL;
   char *token      = NULL;
   size_t token_len = 0;

   /* Sanity checks */
   if (!str || string_is_empty(delim))
      return NULL;

   /* Note: we don't check string_is_empty() here,
    * empty strings are valid */
   if (!(str_ptr = *str))
      return NULL;

   /* Search for delimiter */
   if ((delim_ptr = strstr(str_ptr, delim)))
      token_len = delim_ptr - str_ptr;
   else
      token_len = strlen(str_ptr);

   /* Allocate token string */
   if (!(token = (char *)malloc((token_len + 1) * sizeof(char))))
      return NULL;

   /* Copy token */
   strlcpy(token, str_ptr, (token_len + 1) * sizeof(char));
   token[token_len] = '\0';

   /* Update input string pointer */
   *str = delim_ptr ? delim_ptr + strlen(delim) : NULL;

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
   int count = 0;

   for (; *str; str++)
      if (*str == c)
         count++;

   return count;
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
   char *str_trimmed  = s;
   bool prev_is_space = false;
   bool curr_is_space = false;

   for (; *s; s++)
   {
      curr_is_space  = ISSPACE(*s);
      if (prev_is_space && curr_is_space)
         continue;
      *str_trimmed++ = *s;
      prev_is_space  = curr_is_space;
   }
   *str_trimmed = '\0';
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
