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

char *string_replace_substring(const char *in, const char *pattern, const char *by)
{
   char *needle;
   size_t outsize = strlen(in) + 1;
   /* use this to iterate over the output */
   size_t resoffset = 0;

   /* TODO maybe avoid reallocating by counting the 
    * non-overlapping occurences of pattern */
   char *res = malloc(outsize);

   if (!res)
      return NULL;

   while ((needle = strstr(in, pattern)))
   {
      /* copy everything up to the pattern */
      memcpy(res + resoffset, in, needle - in);
      resoffset += needle - in;

      /* skip the pattern in the input-string */
      in = needle + strlen(pattern);

      /* adjust space for replacement */
      outsize = outsize - strlen(pattern) + strlen(by);
      res = realloc(res, outsize);

      /* copy the pattern */
      memcpy(res + resoffset, by, strlen(by));
      resoffset += strlen(by);
   }

   /* copy the remaining input */
   strcpy(res + resoffset, in);

   return res;
}
