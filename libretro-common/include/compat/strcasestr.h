/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (strcasestr.h).
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

#ifndef __LIBRETRO_SDK_COMPAT_STRCASESTR_H
#define __LIBRETRO_SDK_COMPAT_STRCASESTR_H

#include <string.h>

#if defined(RARCH_INTERNAL) && defined(HAVE_CONFIG_H)
#include "../../../config.h"
#endif

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/**
 * Case-insensitive substring search.
 *
 * Called by its own name rather than shadowing strcasestr, and used
 * unconditionally rather than only where the C library lacks one.  The
 * library's version is not reliably the better choice: glibc's measured
 * some fifty times slower than this on a representative search, and the
 * handheld targets that set HAVE_STRCASESTR are the slowest machines
 * RetroArch runs on and the least likely to have a good implementation.
 * Using one function everywhere also means one set of semantics, rather
 * than case folding that varies with the platform's locale.
 *
 * Folds ASCII only, which is what the callers mean - extensions, driver
 * names, search terms - and matches the behaviour of the C library
 * versions it replaces for every input that is not undefined.
 *
 * @param haystack String to search in.
 * @param needle   String to search for.
 * @return Pointer to the first occurrence within \c haystack,
 *         or NULL if there is none.
 */
char *compat_strcasestr(const char *haystack, const char *needle);

RETRO_END_DECLS

#endif
