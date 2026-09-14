/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (msvc.h).
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

#ifndef __LIBRETRO_SDK_COMPAT_MSVC_H
#define __LIBRETRO_SDK_COMPAT_MSVC_H

#ifdef _MSC_VER

#ifdef __cplusplus
extern "C"  {
#endif

/* Pre-MSVC 2015 compilers don't implement snprintf, vsnprintf in a cross-platform manner. */
#if _MSC_VER < 1900
   #include <stdio.h>
   #include <stdarg.h>
   #include <stdlib.h>

   #ifndef snprintf
      #define snprintf c99_snprintf_retro__
   #endif
   int c99_snprintf_retro__(char *s, size_t len, const char *format, ...);

   #ifndef vsnprintf
      #define vsnprintf c99_vsnprintf_retro__
   #endif
   int c99_vsnprintf_retro__(char *s, size_t len, const char *format, va_list ap);
#endif

/* strtoll/strtoull are C99.  The CRT grew _strtoi64/_strtoui64 in Visual
 * Studio 2005, and those take the same three arguments with the same
 * meaning, so for 2005 and later the mapping is exact.  Visual C++ 6.0,
 * 2002 and 2003 have neither; _atoi64 is not a stand-in for them because
 * it accepts no base and reports no end pointer, so compat_strtoll.c
 * supplies a real implementation instead. */
#if _MSC_VER >= 1900
   /* The UCRT has the real thing; do not shadow it with a macro,
    * which would also catch std::strtoll in C++ translation units. */
#elif _MSC_VER >= 1400
   #ifndef strtoll
      #define strtoll _strtoi64
   #endif
   #ifndef strtoull
      #define strtoull _strtoui64
   #endif
#else
   __int64 strtoll_retro__(const char *nptr, char **endptr, int base);
   unsigned __int64 strtoull_retro__(const char *nptr, char **endptr, int base);
   #ifndef strtoll
      #define strtoll strtoll_retro__
   #endif
   #ifndef strtoull
      #define strtoull strtoull_retro__
   #endif
#endif

#ifdef __cplusplus
}
#endif

#undef UNICODE /* Do not bother with UNICODE at this time. */
#include <direct.h>
#include <stddef.h>

#define _USE_MATH_DEFINES
#include <math.h>

/* Python headers defines ssize_t and sets HAVE_SSIZE_T.
 * Cannot duplicate these efforts.
 */
#ifndef HAVE_SSIZE_T
#if defined(_WIN64)
typedef __int64 ssize_t;
#elif defined(_WIN32)
typedef int ssize_t;
#endif
#endif

#define mkdir(dirname, unused) _mkdir(dirname)
#undef strcasecmp
#define strcasecmp _stricmp
#undef strncasecmp
#define strncasecmp _strnicmp

/* Disable some of the annoying warnings. */
#pragma warning(disable : 4800)
#pragma warning(disable : 4805)
#pragma warning(disable : 4244)
#pragma warning(disable : 4305)
#pragma warning(disable : 4146)
#pragma warning(disable : 4267)
#pragma warning(disable : 4723)
#pragma warning(disable : 4996)

/* roundf and va_copy is available since MSVC 2013 */
#if _MSC_VER < 1800
#define roundf(in) (in >= 0.0f ? floorf(in + 0.5f) : ceilf(in - 0.5f))
#define va_copy(x, y) ((x) = (y))
#endif

#if _MSC_VER <= 1310
   #ifndef __cplusplus
      /* VC6 math.h doesn't define some functions when in C mode.
       * Trying to define a prototype gives "undefined reference".
       * But providing an implementation then gives "function already has body".
       * So the equivalent of the implementations from math.h are used as
       * defines here instead, and it seems to work.
       */
      #define cosf(x) ((float)cos((double)x))
      #define powf(x, y) ((float)pow((double)x, (double)y))
      #define sinf(x) ((float)sin((double)x))
      #define ceilf(x) ((float)ceil((double)x))
      #define floorf(x) ((float)floor((double)x))
      #define sqrtf(x) ((float)sqrt((double)x))
      #define fabsf(x)    ((float)fabs((double)(x)))
   #endif

#endif

#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

#ifndef SIZE_MAX
#define SIZE_MAX _UI32_MAX
#endif

#endif
#endif
