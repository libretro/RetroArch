/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (compat_strcasestr.c).
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

#include <compat/strcasestr.h>

/* Pretty much strncasecmp. */
static int casencmp(const char *a, const char *b, size_t n)
{
   size_t i;

   for (i = 0; i < n; i++)
   {
      int a_lower = tolower(a[i]);
      int b_lower = tolower(b[i]);
      if (a_lower != b_lower)
         return a_lower - b_lower;
   }

   return 0;
}

char *strcasestr_retro__(const char *haystack, const char *needle)
{
   size_t i, search_off;
   size_t hay_len    = strlen(haystack);
   size_t needle_len = strlen(needle);

   if (needle_len > hay_len)
      return NULL;

   search_off = hay_len - needle_len;
   for (i = 0; i <= search_off; i++)
      if (!casencmp(haystack + i, needle, needle_len))
         return (char*)haystack + i;

   return NULL;
}
