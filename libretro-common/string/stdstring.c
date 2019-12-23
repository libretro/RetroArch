/* Copyright  (C) 2010-2018 The RetroArch team
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

#include <string/stdstring.h>
#include <encodings/utf.h>

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
   if(s && *s)
   {
      size_t len     = strlen(s);
      char *current  = s;

      while(*current && isspace((unsigned char)*current))
      {
         ++current;
         --len;
      }

      if(s != current)
         memmove(s, current, len + 1);
   }

   return s;
}

/* Remove trailing whitespaces */
char *string_trim_whitespace_right(char *const s)
{
   if(s && *s)
   {
      size_t len     = strlen(s);
      char  *current = s + len - 1;

      while(current != s && isspace((unsigned char)*current))
      {
         --current;
         --len;
      }

      current[isspace((unsigned char)*current) ? 0 : 1] = '\0';
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

char *word_wrap(char* buffer, const char *string, int line_width, bool unicode, unsigned max_lines)
{
   unsigned i     = 0;
   unsigned len   = (unsigned)strlen(string);
   unsigned lines = 1;

   while (i < len)
   {
      unsigned counter;
      int pos = (int)(&buffer[i] - buffer);

      /* copy string until the end of the line is reached */
      for (counter = 1; counter <= (unsigned)line_width; counter++)
      {
         const char *character;
         unsigned char_len;
         unsigned j = i;

         /* check if end of string reached */
         if (i == len)
         {
            buffer[i] = 0;
            return buffer;
         }

         character = utf8skip(&string[i], 1);
         char_len  = (unsigned)(character - &string[i]);

         if (!unicode)
            counter += char_len - 1;

         do
         {
            buffer[i] = string[i];
            char_len--;
            i++;
         } while(char_len);

         /* check for newlines embedded in the original input
          * and reset the index */
         if (buffer[j] == '\n')
         {
            lines++;
            counter = 1;
         }
      }

      /* check for whitespace */
      if (string[i] == ' ')
      {
         if ((max_lines == 0 || lines < max_lines))
         {
            buffer[i] = '\n';
            i++;
            lines++;
         }
      }
      else
      {
         int k;

         /* check for nearest whitespace back in string */
         for (k = i; k > 0; k--)
         {
            if (string[k] != ' ' || (max_lines != 0 && lines >= max_lines))
               continue;

            buffer[k] = '\n';
            /* set string index back to character after this one */
            i         = k + 1;
            lines++;
            break;
         }

         if (&buffer[i] - buffer == pos)
            return buffer;
      }
   }

   buffer[i] = 0;

   return buffer;
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
 *    while((token = string_tokenize(str_ptr, ",")))
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

   while((str_ptr = strchr(str_ptr, find)) != NULL)
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
      if (!isdigit(*ptr))
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
      if (!isxdigit(*ptr))
         return 0;
   }

   return (unsigned)strtoul(hex_str, NULL, 16);
}
