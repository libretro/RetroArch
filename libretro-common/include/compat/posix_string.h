/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (posix_string.h).
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

#ifndef __LIBRETRO_SDK_COMPAT_POSIX_STRING_H
#define __LIBRETRO_SDK_COMPAT_POSIX_STRING_H

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#undef strtok_r
#define strtok_r(str, delim, saveptr) retro_strtok_r__(str, delim, saveptr)

char *strtok_r(char *str, const char *delim, char **saveptr);
#endif

#ifdef _MSC_VER
#undef strcasecmp
#undef strdup
#define strcasecmp(a, b) retro_strcasecmp__(a, b)
#define strdup(orig)     retro_strdup__(orig)
int strcasecmp(const char *a, const char *b);
char *strdup(const char *orig);

#if _MSC_VER < 1200
#undef isblank
#define isblank(c)       retro_isblank__(c)
int isblank(int c);
#endif

#endif


#ifdef __cplusplus
}
#endif

#endif
