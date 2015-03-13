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

#include <string/stdstring.h>

char *string_replace_substring(const char *in, const char *pattern, const char *replacement)
{
   char *needle = NULL;
   char *newstr = NULL;
   char *head   = NULL;

   /* if either pattern or replacement is NULL,
    * duplicate in and let caller handle it. */
   if (!pattern || !replacement)
      return strdup(in);

   newstr = strdup(in);
   head   = newstr;

   while ((needle = strstr (head, pattern)))
   {
      char* oldstr = newstr;
      newstr = (char*)malloc(
            strlen(oldstr) - strlen(pattern) + strlen(replacement) + 1);

      if (!newstr)
      {
         /* Failed to allocate memory,
          * free old string and return NULL. */
         free(oldstr);
         return NULL;
      }

      memcpy(newstr, oldstr, needle - oldstr);
      memcpy(newstr + (needle - oldstr), replacement, strlen(replacement));
      memcpy(newstr + (needle - oldstr) + strlen(replacement),
            needle + strlen(pattern),
            strlen(oldstr) - strlen(pattern) - (needle - oldstr));
      newstr[strlen(oldstr) - strlen(pattern) + strlen(replacement)] = '\0';

      /* Move back head right after the last replacement. */
      head = newstr + (needle - oldstr) + strlen(replacement);
      free(oldstr);
   }

   return newstr;
}
