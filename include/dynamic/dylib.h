/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (dylib.h).
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

#ifndef __DYLIB_H
#define __DYLIB_H

#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <retro_common_api.h>

#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
#define NEED_DYNAMIC
#else
#undef NEED_DYNAMIC
#endif

RETRO_BEGIN_DECLS

typedef void *dylib_t;
typedef void (*function_t)(void);

#ifdef NEED_DYNAMIC
/**
 * dylib_load:
 * @path                         : Path to libretro core library.
 *
 * Platform independent dylib loading.
 *
 * Returns: library handle on success, otherwise NULL.
 **/
dylib_t dylib_load(const char *path);

/**
 * dylib_close:
 * @lib                          : Library handle.
 *
 * Frees library handle.
 **/
void dylib_close(dylib_t lib);

char *dylib_error(void);

function_t dylib_proc(dylib_t lib, const char *proc);
#endif

RETRO_END_DECLS

#endif
