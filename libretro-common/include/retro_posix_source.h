/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_posix_source.h).
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

#ifndef __LIBRETRO_SDK_POSIX_SOURCE_H
#define __LIBRETRO_SDK_POSIX_SOURCE_H

/* The feature profile these sources are written against: strdup(),
 * strcasecmp(), fstatat(), O_CLOEXEC, M_PI, sigaltstack() and syscall()
 * all sit behind __USE_XOPEN2K8 or __USE_MISC, and a consumer is free to
 * build the library under a stricter profile than the default -- the
 * Debian/Launchpad core packaging passes -D_XOPEN_SOURCE=600.
 *
 * MUST BE THE FIRST INCLUDE: a libc settles what its headers expose when
 * the first of them is seen.
 *
 * The guards only raise the profile, and ask for what a build passing no
 * feature macros already gets, so nothing this library selects changes.
 * Linux only: elsewhere _POSIX_C_SOURCE would instead hide extensions
 * these sources use. */

/* Opt-in, because a unity build reaches its second file with system
 * headers already in scope, where these macros are inert rather than
 * wrong. __GLIBC__ arrives from <features.h>. */
#if defined(RETRO_POSIX_SOURCE_CHECK_ORDER) && defined(__GLIBC__)
#error "retro_posix_source.h must be included before any system header"
#endif

#if defined(__linux__) && !defined(_GNU_SOURCE)

#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE - 0) < 200809L
#undef  _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#if defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE - 0) < 700
#undef  _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

#endif

#endif
