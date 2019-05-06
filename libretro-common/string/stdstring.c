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
