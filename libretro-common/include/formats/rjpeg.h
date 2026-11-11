/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rjpeg.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RJPEG_H__
#define __LIBRETRO_SDK_FORMAT_RJPEG_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct rjpeg rjpeg_t;

bool rjpeg_start(rjpeg_t *rjpeg);

bool rjpeg_iterate_image(rjpeg_t *rjpeg);

bool rjpeg_is_valid(rjpeg_t *rjpeg);

int rjpeg_process_image(rjpeg_t *rjpeg, void **buf,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba);

bool rjpeg_set_buf_ptr(rjpeg_t *rjpeg, void *data, size_t len);

/* Optional: declare the output byte order (supports_rgba) before
 * iteration, so the fused iterate+resample can emit final pixels in
 * the caller's order.  Without it the fusion emits BGRA and
 * rjpeg_process_image repairs the order afterwards if needed. */
void rjpeg_set_out_rgba(rjpeg_t *rjpeg, bool rgba);

/* Prefix decoding support (mirrors rpng): rjpeg_set_avail declares how
 * many bytes from the buffer start are resident (monotonic, clamped to
 * the full length).  While the frontier is below the full length the
 * baseline entropy row driver yields at the frontier with
 * rjpeg_need_more() true instead of decoding past it, so a caller
 * feeding a growing read can raise avail and iterate again.
 * rjpeg_header_ready reports whether the resident bytes already contain
 * the full JPEG header (SOI..SOS), i.e. whether decode can start.
 * Never calling rjpeg_set_avail leaves the whole buffer resident. */
void rjpeg_set_avail(rjpeg_t *rjpeg, size_t avail);
bool rjpeg_need_more(const rjpeg_t *rjpeg);
bool rjpeg_header_ready(const uint8_t *data, size_t len);

/* An image decoded straight to where its rows go, in one call, with
 * the decoder's memory kept for the next image: once an image of a size
 * has been decoded, decoding another no bigger takes no memory.
 *
 *   size  the image's size, once the header is read, before any row;
 *         false stops the decode (NULL: any size)
 *   row   where row y's width * 4 bytes are to be written: 4-byte
 *         aligned memory of the caller's, or NULL for rjpeg's own row
 *         (NULL: always rjpeg's)
 *   done  row y is written, at px - where row() said, or rjpeg's own
 *         row, valid until the call returns (NULL: not told)
 *
 * Rows come in order, 0 to height - 1, as BGRA, or RGBA with @rgba. */
struct rjpeg_rows
{
   bool     (*size)(void *user, unsigned width, unsigned height);
   uint8_t *(*row)(void *user, unsigned y);
   void     (*done)(void *user, unsigned y, const uint8_t *px);
   void      *user;
};

/**
 * rjpeg_decode_rows:
 * @data, @len : the whole file, read where it lies
 * @rgba       : RGBA rather than BGRA
 * @rows       : where the rows go
 * @width, @height : receive the size (0 if the header was not read)
 *
 * Not on a decoder that rjpeg_start() or rjpeg_process_image() is
 * decoding with.
 *
 * Returns: true when every row was written; false for a file that is
 * not a JPEG or is broken, or when @rows->size said no.
 */
bool rjpeg_decode_rows(rjpeg_t *rjpeg, const uint8_t *data, size_t len,
      bool rgba, const struct rjpeg_rows *rows,
      unsigned *width, unsigned *height);

void rjpeg_free(rjpeg_t *rjpeg);

rjpeg_t *rjpeg_alloc(void);

RETRO_END_DECLS

#endif
