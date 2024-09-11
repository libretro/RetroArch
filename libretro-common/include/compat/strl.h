/* Copyright  (C) 2010-2020 The RetroArch team
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

/**
 * @file strl.h
 *
 * Portable implementation of \c strlcpy(3) and \c strlcat(3).
 * If these functions are available on the target platform,
 * then the originals should be imported instead.
 *
 * @see https://linux.die.net/man/3/strlcpy
 */
#include <string.h>
#include <stddef.h>

#if defined(RARCH_INTERNAL) && defined(HAVE_CONFIG_H)
#include "../../../config.h"
#endif

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#ifdef __MACH__
#ifndef HAVE_STRL
#define HAVE_STRL
#endif
#endif

#ifndef HAVE_STRL
/* Avoid possible naming collisions during link since
 * we prefer to use the actual name. */
#define strlcpy(dst, src, size) strlcpy_retro__(dst, src, size)

#define strlcat(dst, src, size) strlcat_retro__(dst, src, size)

/**
 * @brief Portable implementation of \c strlcpy(3).
 * @see https://linux.die.net/man/3/strlcpy
 */
size_t strlcpy(char *dest, const char *source, size_t size);

/**
 * @brief Portable implementation of \c strlcat(3).
 * @see https://linux.die.net/man/3/strlcpy
 */
size_t strlcat(char *dest, const char *source, size_t size);

#endif

/**
 * A version of \c strndup(3) that guarantees the result will be null-terminated.
 *
 * @param s The string to duplicate.
 * @param n The maximum number of characters to copy from \c s.
 * The result will allocate one more byte than this value.
 * @return Pointer to the cloned string.
 * Must be freed with \c free().
 */
char *strldup(const char *s, size_t n);

RETRO_END_DECLS

#endif
