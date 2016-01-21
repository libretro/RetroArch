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
   char *needle           = NULL;
   char *newstr           = NULL;
   char *head             = NULL;
   size_t pattern_len     = 0;
   size_t replacement_len = 0;

   /* if either pattern or replacement is NULL,
    * duplicate in and let caller handle it. */
   if (!pattern || !replacement)
      return strdup(in);

   pattern_len     = strlen(pattern);
   replacement_len = strlen(replacement);

   newstr          = strdup(in);
   head            = newstr;

   while ((needle = strstr(head, pattern)))
   {
      char      *oldstr = newstr;
      size_t oldstr_len = strlen(oldstr);

      newstr = (char*)malloc(oldstr_len - pattern_len + replacement_len + 1);

      if (!newstr)
      {
         /* Failed to allocate memory,
          * free old string and return NULL. */
         free(oldstr);
         return NULL;
      }

      memcpy(newstr, oldstr, needle - oldstr);
      memcpy(newstr + (needle - oldstr), replacement, replacement_len);
      memcpy(newstr + (needle - oldstr) + replacement_len,
            needle + pattern_len,
            oldstr_len - pattern_len - (needle - oldstr));
      newstr[oldstr_len - pattern_len + replacement_len] = '\0';

      /* Move back head right after the last replacement. */
      head = newstr + (needle - oldstr) + replacement_len;
      free(oldstr);
   }

   return newstr;
}
