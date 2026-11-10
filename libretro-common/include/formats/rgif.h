/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rgif.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RGIF_H__
#define __LIBRETRO_SDK_FORMAT_RGIF_H__

/* A GIF's first image, decoded where the caller wants it.
 *
 * The file is read where it lies, nothing copied; the pixels go straight
 * into the caller's memory at the caller's row pitch, in the byte order
 * asked for, eight bits a channel. The decoder's own memory (the LZW
 * table, about 20 KB) is taken by rgif_new() and kept, so decoding one
 * image after another takes nothing more.
 *
 * The picture is the GIF's whole logical screen. Where the first image
 * does not cover it, the screen's background colour shows, opaque, if
 * the file names one other than entry 0 of its colour table; otherwise
 * those pixels are transparent black, as are the image's own transparent
 * pixels and indices past its colour table - as stb_image gives the first
 * frame. A file cut short gives what was decoded before the cut. */

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

/* The byte order of each pixel written */
enum rgif_order
{
   RGIF_RGBA = 0,
   RGIF_ARGB,
   RGIF_BGRA,
   RGIF_ABGR
};

typedef struct rgif rgif_t;

rgif_t *rgif_new(void);
void rgif_free(rgif_t *g);

/**
 * rgif_size:
 * @data, @len : the file
 *
 * The logical screen's size from the header, without decoding.
 *
 * Returns: false if it is not a GIF.
 */
bool rgif_size(const uint8_t *data, size_t len,
      unsigned *width, unsigned *height);

/**
 * rgif_decode:
 * @data, @len : the file
 * @out        : rgif_size()'s width x height pixels of 4 bytes, @stride
 *               bytes from one row to the next (at least width * 4)
 * @order      : RGIF_RGBA, RGIF_ARGB, RGIF_BGRA or RGIF_ABGR
 *
 * Decodes the first image into @out.
 *
 * Returns: false if it is not a GIF, or is broken before its first
 * image's pixels: an image past the screen, no colour table, a bad LZW
 * stream, a block that is no GIF block. @out may be partly written.
 */
bool rgif_decode(rgif_t *g, const uint8_t *data, size_t len,
      uint8_t *out, size_t stride, unsigned order);

/* The image_transfer interface (formats/image.h), as the other image
 * decoders have it: the first image as a 32-bit texture of its own,
 * ARGB32 or, with @supports_rgba, ABGR32, in native byte order.
 *
 * rgif_set_avail() says how much of the buffer is resident yet; until
 * the whole file is, rgif_process_image() returns IMAGE_PROCESS_WAIT and
 * takes nothing (a GIF decodes in one go). rgif_header_ready() says
 * whether @len bytes hold the header. */
bool rgif_header_ready(const uint8_t *data, size_t len);
bool rgif_set_buf_ptr(rgif_t *g, void *data, size_t len);
void rgif_set_avail(rgif_t *g, size_t avail);
int rgif_process_image(rgif_t *g, void **buf, size_t size,
      unsigned *width, unsigned *height, bool supports_rgba);

RETRO_END_DECLS

#endif
