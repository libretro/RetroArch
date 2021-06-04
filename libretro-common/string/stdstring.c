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

char *string_init(const char *src)
{
   return src ? strdup(src) : NULL;
}

void string_set(char **string, const char *src)
{
   free(*string);
   *string = string_init(src);
}


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

char *string_replace_substring(const char *in,
      const char *pattern, const char *replacement)
{
   size_t numhits, pattern_len, replacement_len, outlen;
   const char *inat   = NULL;
   const char *inprev = NULL;
   char          *out = NULL;
   char        *outat = NULL;

   /* if either pattern or replacement is NULL,
    * duplicate in and let caller handle it. */
   if (!pattern || !replacement)
      return strdup(in);

   pattern_len     = strlen(pattern);
   replacement_len = strlen(replacement);
   numhits         = 0;
   inat            = in;

   while ((inat = strstr(inat, pattern)))
   {
      inat += pattern_len;
      numhits++;
   }

   outlen          = strlen(in) - pattern_len*numhits + replacement_len*numhits;
   out             = (char *)malloc(outlen+1);

   if (!out)
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
      inat += pattern_len;
      inprev = inat;
   }
   strcpy(outat, inprev);

   return out;
}

/* Remove leading whitespaces */
char *string_trim_whitespace_left(char *const s)
{
   if (s && *s)
   {
      size_t len     = strlen(s);
      char *current  = s;

      while (*current && ISSPACE((unsigned char)*current))
      {
         ++current;
         --len;
      }

      if (s != current)
         memmove(s, current, len + 1);
   }

   return s;
}

/* Remove trailing whitespaces */
char *string_trim_whitespace_right(char *const s)
{
   if (s && *s)
   {
      size_t len     = strlen(s);
      char  *current = s + len - 1;

      while (current != s && ISSPACE((unsigned char)*current))
      {
         --current;
         --len;
      }

      current[ISSPACE((unsigned char)*current) ? 0 : 1] = '\0';
   }

   return s;
}

/* Remove leading and trailing whitespaces */
char *string_trim_whitespace(char *const s)
{
   string_trim_whitespace_right(s);  /* order matters */
   string_trim_whitespace_left(s);

   return s;
}

void word_wrap(char *dst, size_t dst_size, const char *src, int line_width, int wideglyph_width, unsigned max_lines)
{
   char *lastspace     = NULL;
   unsigned counter    = 0;
   unsigned lines      = 1;
   size_t src_len      = strlen(src);
   const char *src_end = src + src_len;

   /* Prevent buffer overflow */
   if (dst_size < src_len + 1)
      return;

   /* Early return if src string length is less
    * than line width */
   if (src_len < line_width)
   {
      strcpy(dst, src);
      return;
   }

   while (*src != '\0')
   {
      unsigned char_len;

      char_len = (unsigned)(utf8skip(src, 1) - src);
      counter++;

      if (*src == ' ')
         lastspace = dst; /* Remember the location of the whitespace */
      else if (*src == '\n')
      {
         /* If newlines embedded in the input,
          * reset the index */
         lines++;
         counter = 0;

         /* Early return if remaining src string
          * length is less than line width */
         if (src_end - src <= line_width)
         {
            strcpy(dst, src);
            return;
         }
     }

      while (char_len--)
         *dst++ = *src++;

      if (counter >= (unsigned)line_width)
      {
         counter = 0;

         if (lastspace && (max_lines == 0 || lines < max_lines))
         {
            /* Replace nearest (previous) whitespace
             * with newline character */
            *lastspace = '\n';
            lines++;

            src -= dst - lastspace - 1;
            dst = lastspace + 1;
            lastspace  = NULL;

            /* Early return if remaining src string
             * length is less than line width */
            if (src_end - src < line_width)
            {
               strcpy(dst, src);
               return;
            }
         }
      }
   }

   *dst = '\0';
}

void word_wrap_wideglyph(char *dst, size_t dst_size, const char *src, int line_width, int wideglyph_width, unsigned max_lines)
{
   char *lastspace                   = NULL;
   char *lastwideglyph               = NULL;
   const char *src_end               = src + strlen(src);
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
   {
      strlcpy(dst, src, dst_size);
      return;
   }

   while (*src != '\0')
   {
      unsigned char_len;

      char_len = (unsigned)(utf8skip(src, 1) - src);
      counter_normalized += 100;

      /* Prevent buffer overflow */
      if (char_len >= dst_size)
         break;

      if (*src == ' ')
         lastspace = dst; /* Remember the location of the whitespace */
      else if (*src == '\n')
      {
         /* If newlines embedded in the input,
          * reset the index */
         lines++;
         counter_normalized = 0;

         /* Early return if remaining src string
          * length is less than line width */
         if (src_end - src <= line_width)
         {
            strlcpy(dst, src, dst_size);
            return;
         }
      }
      else if (char_len >= 3)
      {
         /* Remember the location of the first byte
          * whose length as UTF-8 >= 3*/
         lastwideglyph = dst;
         counter_normalized += additional_counter_normalized;
      }

      dst_size -= char_len;
      while (char_len--)
         *dst++ = *src++;

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
            src -= dst - lastwideglyph;
            dst = lastwideglyph + 1;
            lastwideglyph = NULL;

            /* Early return if remaining src string
             * length is less than line width */
            if (src_end - src <= line_width)
            {
               strlcpy(dst, src, dst_size);
               return;
            }
         }
         else if (lastspace)
         {
            /* Replace nearest (previous) whitespace
             * with newline character */
            *lastspace = '\n';
            lines++;
            src -= dst - lastspace - 1;
            dst = lastspace + 1;
            lastspace = NULL;

            /* Early return if remaining src string
             * length is less than line width */
            if (src_end - src < line_width)
            {
               strlcpy(dst, src, dst_size);
               return;
            }
         }
      }
   }

   *dst = '\0';
}

/* Splits string into tokens seperated by 'delim'
 * > Returned token string must be free()'d
 * > Returns NULL if token is not found
 * > After each call, 'str' is set to the position after the
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
 */
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

   str_ptr = *str;

   /* Note: we don't check string_is_empty() here,
    * empty strings are valid */
   if (!str_ptr)
      return NULL;

   /* Search for delimiter */
   delim_ptr = strstr(str_ptr, delim);

   if (delim_ptr)
      token_len = delim_ptr - str_ptr;
   else
      token_len = strlen(str_ptr);

   /* Allocate token string */
   token = (char *)malloc((token_len + 1) * sizeof(char));

   if (!token)
      return NULL;

   /* Copy token */
   strlcpy(token, str_ptr, (token_len + 1) * sizeof(char));
   token[token_len] = '\0';

   /* Update input string pointer */
   *str = delim_ptr ? delim_ptr + strlen(delim) : NULL;

   return token;
}

/* Removes every instance of character 'c' from 'str' */
void string_remove_all_chars(char *str, char c)
{
   char *read_ptr  = NULL;
   char *write_ptr = NULL;

   if (string_is_empty(str))
      return;

   read_ptr  = str;
   write_ptr = str;

   while (*read_ptr != '\0')
   {
      *write_ptr = *read_ptr++;
      write_ptr += (*write_ptr != c) ? 1 : 0;
   }

   *write_ptr = '\0';
}

/* Replaces every instance of character 'find' in 'str'
 * with character 'replace' */
void string_replace_all_chars(char *str, char find, char replace)
{
   char *str_ptr = str;

   if (string_is_empty(str))
      return;

   while ((str_ptr = strchr(str_ptr, find)))
      *str_ptr++ = replace;
}

/* Converts string to unsigned integer.
 * Returns 0 if string is invalid  */
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

/* Converts hexadecimal string to unsigned integer.
 * Handles optional leading '0x'.
 * Returns 0 if string is invalid  */
unsigned string_hex_to_unsigned(const char *str)
{
   const char *hex_str = str;
   const char *ptr     = NULL;
   size_t len;

   if (string_is_empty(str))
      return 0;

   /* Remove leading '0x', if required */
   len = strlen(str);

   if (len >= 2)
      if ((str[0] == '0') &&
          ((str[1] == 'x') || (str[1] == 'X')))
         hex_str = str + 2;

   if (string_is_empty(hex_str))
      return 0;

   /* Check for valid characters */
   for (ptr = hex_str; *ptr != '\0'; ptr++)
   {
      if (!isxdigit((unsigned char)*ptr))
         return 0;
   }

   return (unsigned)strtoul(hex_str, NULL, 16);
}
