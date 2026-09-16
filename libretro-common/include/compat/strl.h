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
#include <retro_inline.h>

RETRO_BEGIN_DECLS

#if defined(__MACH__) && defined(__APPLE__)
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
size_t strlcpy(char *s, const char *source, size_t len);

/**
 * @brief Portable implementation of \c strlcat(3).
 * @see https://linux.die.net/man/3/strlcpy
 */
size_t strlcat(char *s, const char *source, size_t len);

#endif

/**
 * @brief Copy a string \b literal, without scanning it at runtime.
 *
 * \c strlcpy() begins with \c strlen() of its source, and because it
 * lives in another translation unit that scan happens at runtime even
 * when the source is a literal whose length the compiler already knows.
 * The scan is roughly half the cost of a \c strlcpy() of a short string
 * -- there is a fixed setup cost before a single byte is compared -- so
 * for a literal it is pure overhead.
 *
 * @param dst  Destination buffer.
 * @param lit  A string \b literal. Anything else is a compile error:
 *             the \c "" concatenation below only parses for a literal,
 *             which is what keeps \c sizeof from silently measuring a
 *             pointer instead of the characters.
 * @param size Size of @p dst, exactly as for \c strlcpy().
 *
 * @return The length of @p lit, matching \c strlcpy().
 *
 * The destination bound is not dropped: when the literal (with its
 * terminator) does not fit, this defers to \c strlcpy() so the copy
 * truncates rather than overruns. With a compile-time @p size the test
 * folds away and only one arm survives; with a runtime @p size it costs
 * a comparison that predicts perfectly.
 */
static INLINE size_t strlcpy_lit_(char *s, const char *lit,
      size_t lit_size, size_t len)
{
   /* lit_size is sizeof() of a literal, so it is a constant at every
    * call site and this branch folds away; when @len is a constant too
    * -- sizeof(buf), the common case -- the whole call becomes the
    * stores memcpy() compiles to. */
   if (lit_size <= len)
   {
      memcpy(s, lit, lit_size);
      return lit_size - 1;
   }
   return strlcpy(s, lit, len);
}

#define strlcpy_lit(dst, lit, size) \
   strlcpy_lit_((dst), "" lit, sizeof("" lit), (size))

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
