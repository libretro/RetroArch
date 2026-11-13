/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtga.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RTGA_H__
#define __LIBRETRO_SDK_FORMAT_RTGA_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct rtga rtga_t;

/**
 * Raises the resident-byte frontier for a decode running against a
 * partially filled buffer. Bytes at or past @avail are not read; a
 * slice that reaches the frontier with more of the file still to
 * arrive returns \c IMAGE_PROCESS_WAIT and sets rtga_need_more()
 * rather than treating the wall as EOF. The frontier only ever moves
 * forward. Never calling this decodes the whole buffer exactly as
 * before.
 */
/** True when @len bytes from the head of a TGA are enough for
 * rtga_process_image() to parse the header, id field and colour map
 * and begin painting pixels, i.e. when a partial-buffer decode can
 * usefully start. False for a type this decoder does not slice, so a
 * caller falls back to loading the file whole. */
bool rtga_header_ready(const uint8_t *data, size_t len);

void rtga_set_avail(rtga_t *rtga, size_t avail);

/** True when the last rtga_process_image() stopped at the frontier
 * set by rtga_set_avail() rather than finishing. */
bool rtga_need_more(rtga_t *rtga);

int rtga_process_image(rtga_t *rtga, void **buf,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba);

bool rtga_set_buf_ptr(rtga_t *rtga, void *data);

/* An image decoded straight to where its rows go, in one call, with
 * the decoder's memory kept for the next image.
 *
 *   size  the image's size, once the header is read, before any row;
 *         false stops the decode (NULL: any size)
 *   row   where row y's width * 4 bytes are to be written: 4-byte
 *         aligned memory of the caller's, or NULL for rtga's own row
 *         (NULL: always rtga's)
 *   done  row y is written, at px - where row() said, or rtga's own
 *         row, valid until the next row is begun (NULL: not told)
 *
 * Each row is asked for once and told done once, in the file's order:
 * bottom to top for a TGA stored bottom-up.  y counts from the top.
 * Pixels are BGRA, or RGBA with @rgba, in byte order.  A file that
 * ends early is finished as rtga_process_image() finishes it. */
struct rtga_rows
{
   bool     (*size)(void *user, unsigned width, unsigned height);
   uint8_t *(*row)(void *user, unsigned y);
   void     (*done)(void *user, unsigned y, const uint8_t *px);
   void      *user;
};

/**
 * rtga_decode_rows:
 * @data, @len : the whole file, read where it lies
 * @rgba       : RGBA rather than BGRA
 * @rows       : where the rows go
 * @width, @height : receive the size (0 if the header was not read)
 *
 * Abandons any rtga_process_image() decode on @rtga.
 *
 * Returns: true when every row was written; false for a header this
 * decoder does not take, or when @rows->size said no.
 */
bool rtga_decode_rows(rtga_t *rtga, const uint8_t *data, size_t len,
      bool rgba, const struct rtga_rows *rows,
      unsigned *width, unsigned *height);

void rtga_free(rtga_t *rtga);

rtga_t *rtga_alloc(void);

RETRO_END_DECLS

#endif
