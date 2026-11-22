/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (memdesc.h).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_MEMDESC_H
#define __LIBRETRO_SDK_MEMDESC_H

#include <stddef.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Completes a core's memory map in place, as RetroArch reads it: a
 * select or len left 0 is worked out, and address bits too high to
 * index the buffer are disconnected. Returns false at the first
 * descriptor it cannot complete and leaves the ones after it as they
 * were. One refused for start bits outside select keeps the select or
 * len just worked out for it, so memdesc_find matches it by select.
 * @first may be NULL when @count is 0. */
bool memdesc_preprocess(struct retro_memory_descriptor *first,
      unsigned count);

/* The first of @count descriptors that claims @address, or NULL.
 * On a match, *offset is where @address lies after the descriptor's
 * ptr + offset; on NULL, *offset means nothing. @descs may be NULL
 * when @count is 0. */
const struct retro_memory_descriptor *memdesc_find(
      const struct retro_memory_descriptor *descs, unsigned count,
      size_t address, size_t *offset);

RETRO_END_DECLS

#endif
