/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (compat_snprintf.c).
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

/* THIS FILE HAS NOT BEEN VALIDATED ON PLATFORMS BESIDES MSVC */
#ifdef _MSC_VER

#include <stdio.h>
#include <stdarg.h>

#if _MSC_VER < 1800
#define va_copy(dst, src) ((dst) = (src))
#endif

#if _MSC_VER < 1300
#define _vscprintf c89_vscprintf_retro__

static int c89_vscprintf_retro__(const char *fmt, va_list pargs)
{
   int retval;
   va_list argcopy;
   va_copy(argcopy, pargs);
   retval = vsnprintf(NULL, 0, fmt, argcopy);
   va_end(argcopy);
   return retval;
}
#endif

/* http://stackoverflow.com/questions/2915672/snprintf-and-visual-studio-2010 */

int c99_vsnprintf_retro__(char *s, size_t len, const char *fmt, va_list ap)
{
   int count = -1;

   if (len != 0)
   {
#if (_MSC_VER <= 1310)
      count = _vsnprintf(s, len - 1, fmt, ap);
#else
      count = _vsnprintf_s(s, len, len - 1, fmt, ap);
#endif
   }

   if (count == -1)
       count = _vscprintf(fmt, ap);

   /* there was no room for a NULL, so truncate the last character */
   if (count == len && len)
      s[len - 1] = '\0';

   return count;
}

int c99_snprintf_retro__(char *s, size_t len, const char *fmt, ...)
{
   int count;
   va_list ap;

   va_start(ap, fmt);
   count = c99_vsnprintf_retro__(s, len, fmt, ap);
   va_end(ap);

   return count;
}
#endif
