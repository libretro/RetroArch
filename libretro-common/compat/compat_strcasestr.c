/* Copyright  (C) 2010-2020 The RetroArch team
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

/* ASCII case folding, done here rather than through tolower.
 *
 * tolower takes an int whose value must be representable as unsigned
 * char or EOF; passing a plain char sign-extends anything above 0x7F
 * into a negative value, which is undefined.  This is called on
 * arbitrary bytes - file paths, HTTP responses, config text - so that
 * was reachable.  Folding only A-Z is also what every caller here
 * means: extensions, window manager names, config keys and header
 * fields are all ASCII, and locale-dependent folding of anything else
 * would make the result depend on the user's locale. */
#define STRCASESTR_LOWER(c) \
   (((c) >= 'A' && (c) <= 'Z') ? ((c) + ('a' - 'A')) : (c))

/* Pretty much strncasecmp. */
static int casencmp(const char *a, const char *b, size_t n)
{
   size_t i;

   for (i = 0; i < n; i++)
   {
      int a_lower = STRCASESTR_LOWER((unsigned char)a[i]);
      int b_lower = STRCASESTR_LOWER((unsigned char)b[i]);
      if (a_lower != b_lower)
         return a_lower - b_lower;
   }

   return 0;
}

/* Let memchr find the candidate positions.
 *
 * Comparing at every offset means folding every byte of the haystack,
 * which measured some sixty times slower than strstr on the same
 * inputs - enough to matter where this is called per file over a
 * directory.  Only the first character of the needle decides whether
 * an offset is worth examining, and memchr finds those far faster than
 * a byte loop can, so it does the skipping and the fold only runs
 * where a match is actually possible.  A needle whose first character
 * has no case of its own - a digit, a punctuation mark - costs one
 * memchr rather than two. */
char *compat_strcasestr(const char *haystack, const char *needle)
{
   size_t      nlen = strlen(needle);
   size_t      hlen = strlen(haystack);
   const char *end  = haystack + hlen;
   int         lo;
   int         up;

   if (!nlen)
      return (char*)haystack;
   if (nlen > hlen)
      return NULL;

   lo = STRCASESTR_LOWER((unsigned char)needle[0]);
   up = (lo >= 'a' && lo <= 'z') ? (lo - ('a' - 'A')) : lo;

   while ((size_t)(end - haystack) >= nlen)
   {
      size_t      left = (size_t)(end - haystack);
      const char *a    = (const char*)memchr(haystack, lo, left);
      const char *b    = (up != lo)
         ? (const char*)memchr(haystack, up, left) : NULL;
      const char *p    = !a ? b : (!b ? a : (a < b ? a : b));

      if (!p || (size_t)(end - p) < nlen)
         return NULL;
      if (!casencmp(p + 1, needle + 1, nlen - 1))
         return (char*)p;
      haystack = p + 1;
   }

   return NULL;
}
