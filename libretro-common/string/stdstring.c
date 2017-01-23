/* Copyright  (C) 2010-2017 The RetroArch team
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

bool string_is_empty(const char *data)
{
   return (data == NULL) || (*data == '\0');
}

bool string_is_equal(const char *a, const char *b)
{
   if (!a || !b)
      return false;
   return (strcmp(a, b) == 0);
}

bool string_is_equal_noncase(const char *a, const char *b)
{
   if (!a || !b)
      return false;
   return (strcasecmp(a, b) == 0);
}

char *string_to_upper(char *s)
{
   char *cs = (char *)s;
   for ( ; *cs != '\0'; cs++)
      *cs = toupper(*cs);
   return s;
}

char *string_to_lower(char *s)
{
   char *cs = (char *)s;
   for ( ; *cs != '\0'; cs++)
      *cs = tolower(*cs);
   return s;
}

char *string_ucwords(char *s)
{
  char *cs = (char *)s;
  for ( ; *cs != '\0'; cs++)
  {
    if (*cs == ' ')
    {
      *(cs+1) = toupper(*(cs+1));
    }
  }

  s[0] = toupper(s[0]);
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
      size_t len = strlen(s);
      char *cur  = s;

      while(*cur && isspace(*cur))
         ++cur, --len;

      if(s != cur)
         memmove(s, cur, len + 1);

   }

   return s;
}

/* Remove trailing whitespaces */
char *string_trim_whitespace_right(char *const s)
{
   if(s && *s)
   {
      size_t len = strlen(s);
      char  *cur = s + len - 1;

      while(cur != s && isspace(*cur))
         --cur, --len;

      cur[isspace(*cur) ? 0 : 1] = '\0';
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

char* word_wrap(char* buffer, char* string, int line_width)
{
   unsigned i = 0;
   int k, counter;

   while(i < strlen(string))
   {
      /* copy string until the end of the line is reached */
      for (counter = 1; counter <= line_width; counter++)
      {
         /* check if end of string reached */
         if (i == strlen(string))
         {
            buffer[i] = 0;
            return buffer;
         }

         buffer[i] = string[i];

         /* check for newlines embedded in the original input
          * and reset the index */
         if (buffer[i] == '\n')
            counter = 1;
         i++;
      }
      /* check for whitespace */
      if (string[i] == ' ')
      {
         buffer[i] = '\n';
         i++;
      }
      else
      {
         /* check for nearest whitespace back in string */
         for (k = i; k > 0; k--)
         {
            if (string[k] == ' ')
            {
               buffer[k] = '\n';
               /* set string index back to character after this one */
               i = k + 1;
               break;
            }
         }
      }
   }

   buffer[i] = 0;

   return buffer;
}
