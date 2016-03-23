/* Copyright  (C) 2010-2015 The RetroArch team
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

#include <retro_common.h>

#include <stdio.h>
#include <stdarg.h>

/* http://stackoverflow.com/questions/2915672/snprintf-and-visual-studio-2010 */

int c99_vsnprintf_retro__(char *outBuf, size_t size, const char *format, va_list ap)
{
   int count = -1;

   if (size != 0)
       count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
   if (count == -1)
       count = _vscprintf(format, ap);

   return count;
}

int c99_snprintf_retro__(char *outBuf, size_t size, const char *format, ...)
{
   int count;
   va_list ap;

   va_start(ap, format);
   count = c99_vsnprintf_retro__(outBuf, size, format, ap);
   va_end(ap);

   return count;
}
