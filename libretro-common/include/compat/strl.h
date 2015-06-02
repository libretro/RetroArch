/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (strl.h).
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

#ifndef __LIBRETRO_SDK_COMPAT_STRL_H
#define __LIBRETRO_SDK_COMPAT_STRL_H

#include <string.h>
#include <stddef.h>

#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_STRL
/* Avoid possible naming collisions during link since 
 * we prefer to use the actual name. */
#define strlcpy(dst, src, size) strlcpy_rarch__(dst, src, size)

#define strlcat(dst, src, size) strlcat_rarch__(dst, src, size)

size_t strlcpy(char *dest, const char *source, size_t size);
size_t strlcat(char *dest, const char *source, size_t size);

#endif

#ifdef __cplusplus
}
#endif

#endif

