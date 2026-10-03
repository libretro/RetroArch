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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <compat/strl.h>

/* Implementation of strlcpy()/strlcat() based on OpenBSD. */

#if !(defined(__MACH__) && defined(__APPLE__))
size_t strlcpy(char *s, const char *in, size_t len)
{
   size_t src_len = strlen(in);
#ifdef LIBRETRO_STRL_CHECK_OVERLAP
   /* Test builds: fail the way macOS's fortified libc does on an
    * overlapping copy (__chk_fail_overlap), so the Linux test suites
    * catch what only Apple's runtime would otherwise catch. */
   if (len && src_len)
   {
      const char *s_end  = s + (src_len < len - 1 ? src_len : len - 1) + 1;
      const char *in_end = in + src_len + 1;
      if (!(s_end <= in || in_end <= s))
      {
         fprintf(stderr, "strlcpy: overlapping copy (dst %p, src %p, %u bytes)\n",
               (void*)s, (const void*)in, (unsigned)src_len);
         abort();
      }
   }
#endif
   if (len)
   {
      size_t cpy_len = src_len < len - 1 ? src_len : len - 1;
      memcpy(s, in, cpy_len);
      s[cpy_len] = '\0';
   }
   return src_len;
}

/* The destination scan is bounded by 'len': 's' is not required to
 * contain a NUL within the first 'len' bytes. When it does not, no
 * bytes are written and the return value is len + strlen(source),
 * matching OpenBSD, Darwin and glibc. */
size_t strlcat(char *s, const char *source, size_t len)
{
   const char *nul = (const char*)memchr(s, 0, len);
   size_t dst_len  = nul ? (size_t)(nul - s) : len;
   if (dst_len == len)
      return len + strlen(source);
   return dst_len + strlcpy(s + dst_len, source, len - dst_len);
}
#endif
