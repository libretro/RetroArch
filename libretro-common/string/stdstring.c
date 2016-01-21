/* Copyright  (C) 2010-2015 The RetroArch team
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

#include <ctype.h>

#include <string/stdstring.h>

bool string_is_empty(const char *data)
{
   return data==NULL || *data=='\0';
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

char *string_replace_substring(const char *in, const char *pattern, const char *replacement)
{
   size_t numhits;
   size_t pattern_len;
   size_t replacement_len;
   const char *inat;
   const char *inprev;
   size_t outlen;
   char *out;
   char *outat;
   
   /* if either pattern or replacement is NULL,
    * duplicate in and let caller handle it. */
   if (!pattern || !replacement)
      return strdup(in);
   
   pattern_len     = strlen(pattern);
   replacement_len = strlen(replacement);
   
   numhits = 0;
   inat    = in;
   while ((inat = strstr(inat, pattern)))
   {
      inat += pattern_len;
      numhits++;
   }
   
   outlen = strlen(in) - pattern_len*numhits + replacement_len*numhits;
   out    = (char *)malloc(outlen+1);
   outat  = out;
   
   inat   = in;
   inprev = in;
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
