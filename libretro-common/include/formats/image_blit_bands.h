/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_blit_bands.h).
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_FORMAT_IMAGE_BLIT_BANDS_H__
#define __LIBRETRO_SDK_FORMAT_IMAGE_BLIT_BANDS_H__

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* A row-parallel blit: the rows of one frame split into bands that a
 * thread pool converts side by side. The frame's destination is one
 * buffer nobody else touches until the whole blit has returned, so
 * bands need no ordering among themselves, only the join at the end.
 *
 * @fn converts rows [row0, row0 + rows) of the frame described by
 * @ctx; the caller offsets its plane and destination pointers by row0
 * inside it. @align is the row granularity a band may start on: 2 for
 * 4:2:0 material, whose chroma rows are shared by pairs of luma rows,
 * 1 otherwise. */
typedef void (*image_blit_rows_t)(void *ctx, unsigned row0, unsigned rows);

/* Convert @h rows through @fn in up to @bands bands: bands - 1 go to
 * @pool (an rthreads tpool_t) and the last runs on the calling thread,
 * which then waits for the others. With no pool, one band, or too few
 * rows for a split to pay, the whole frame is one call on the calling
 * thread - the same thing a single-threaded blit always was. */
void image_blit_bands(void *pool, unsigned bands, unsigned h,
      unsigned align, image_blit_rows_t fn, void *ctx);

RETRO_END_DECLS

#endif
