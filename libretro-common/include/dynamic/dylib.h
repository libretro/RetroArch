/* Copyright  (C) 2010-2020 The RetroArch team
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

/**
 * Opaque handle to a dynamic library.
 * @see dylib_load
 */
typedef void *dylib_t;

/**
 * Opaque handle to a function exposed by a dynamic library.
 *
 * Should be cast to a known function pointer type before being used.
 * @see dylib_proc
 */
typedef void (*function_t)(void);

#ifdef NEED_DYNAMIC
/**
 * Loads a dynamic library in a platform-independent manner.
 *
 * @param path Path to the library to load.
 * May be either a complete path or a filename without an extension.
 * If not a complete path, the operating system will search for a library by this name;
 * details will depend on the platform.
 * @note The returned library must be freed with \c dylib_close
 * before the core or frontend exits.
 *
 * @return Handle to the loaded library, or \c NULL on failure.
 * Upon failure, \c dylib_error may be used
 * to retrieve a string describing the error.
 * @see dylib_close
 * @see dylib_error
 * @see dylib_proc
 **/
dylib_t dylib_load(const char *path);

/**
 * Frees the resources associated with a dynamic library.
 *
 * Any function pointers obtained from the library may become invalid,
 * depending on whether the operating system manages reference counts to dynamic libraries.
 *
 * If there was an error closing the library,
 * it will be reported by \c dylib_error.
 *
 * @param lib Handle to the library to close.
 * Behavior is undefined if \c NULL.
 **/
void dylib_close(dylib_t lib);

/**
 * Returns a string describing the most recent error that occurred
 * within the other \c dylib functions.
 *
 * @return Pointer to the most recent error string,
 * \c or NULL if there was no error.
 * @warning The returned string is only valid
 * until the next call to a \c dylib function.
 * Additionally, the string is managed by the library
 * and should not be modified or freed by the caller.
 */
char *dylib_error(void);

/**
 * Returns a pointer to a function exposed by a dynamic library.
 *
 * @param lib The library to get a function pointer from.
 * @param proc The name of the function to get a pointer to.
 * @return Pointer to the requested function,
 * or \c NULL if there was an error.
 * This must be cast to the correct function pointer type before being used.
 * @warning The returned pointer is only valid for the lifetime of \c lib.
 * Once \c lib is closed, all function pointers returned from it will be invalidated;
 * using them is undefined behavior.
 */
function_t dylib_proc(dylib_t lib, const char *proc);
#endif

RETRO_END_DECLS

#endif
