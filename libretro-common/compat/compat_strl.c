/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (compat_strl.c).
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

#include <compat/strl.h>

/* Implementation of strlcpy()/strlcat() based on OpenBSD. */

#if !(defined(__MACH__) && defined(__APPLE__))
size_t strlcpy(char *s, const char *in, size_t len)
{
   size_t src_len = strlen(in);
   if (len)
   {
      size_t cpy_len = src_len < len - 1 ? src_len : len - 1;
      memcpy(s, in, cpy_len);
      s[cpy_len] = '\0';
   }
   return src_len;
}

/* NOTE: When 'len' is smaller than strlen(s), the return value is
 * strlen(s) + strlen(source), whereas OpenBSD returns
 * len + strlen(source). No bytes are written in either case, and the
 * usual 'return value >= len means truncated' test holds for both,
 * so this only matters to callers that use the return value as an
 * exact required-buffer-size figure. */
size_t strlcat(char *s, const char *source, size_t len)
{
   size_t dst_len = strlen(s);
   s += dst_len;
   if (dst_len > len)
      len = 0;
   else
      len -= dst_len;
   return dst_len + strlcpy(s, source, len);
}
#endif
