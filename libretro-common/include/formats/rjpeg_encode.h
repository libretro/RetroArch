/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rjpeg_encode.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RJPEG_ENCODE_H__
#define __LIBRETRO_SDK_FORMAT_RJPEG_ENCODE_H__

/* Baseline JPEG encoder (ITU-T T.81): the frames of motion JPEG, and
 * still pictures.
 *
 * Takes 4:2:0 or 4:2:2 planes in JFIF's full-range BT.601 YCbCr, read
 * where they lie, and writes a complete JFIF file - SOI to EOI, the
 * quantisation tables of T.81 Annex K scaled to a quality as the IJG
 * encoder scales them, Annex K's Huffman tables - into the caller's
 * buffer. Nothing is allocated: the tables live in an rjpeg_encoder_t
 * the caller keeps, prepared once for a quality, and the output buffer
 * is the caller's, rjpeg_encode_bound() bytes being always enough. Any
 * size up to 65535 x 65535; partial blocks at the right and bottom edges
 * repeat the last column and row. The stack frame stays well inside the
 * 2 KiB tools/stack_budget.py allows. */

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum rjpeg_encode_sampling
{
   RJPEG_ENCODE_420 = 0,   /* chroma halved both ways */
   RJPEG_ENCODE_422        /* chroma halved horizontally */
};

typedef struct
{
   uint8_t  qt[2][64];       /* in zigzag order, as DQT carries them */
   uint32_t recip[2][64];    /* 2^16 over each divisor, raster order */
   uint16_t code[4][256];    /* Huffman codes: DC and AC luma, DC and AC chroma */
   uint8_t  len[4][256];
} rjpeg_encoder_t;

/* Prepares @e for @quality, 1 (smallest) to 100 (best), 75 being the
 * usual default */
void   rjpeg_encoder_init(rjpeg_encoder_t *e, unsigned quality);

/* The most bytes a picture of this size can take */
size_t rjpeg_encode_bound(unsigned width, unsigned height);

/**
 * rjpeg_encode:
 * @e        : tables from rjpeg_encoder_init()
 * @dst      : the output
 * @cap      : its size
 * @width    : the picture's size
 * @height   :
 * @sampling : how the chroma planes are subsampled
 * @y, @y_stride, @cb, @cb_stride, @cr, @cr_stride : the planes
 *
 * Returns: the bytes written, or 0 if @cap was too small or the size is
 * not one JPEG can carry.
 */
size_t rjpeg_encode(const rjpeg_encoder_t *e, uint8_t *dst, size_t cap,
      unsigned width, unsigned height, enum rjpeg_encode_sampling sampling,
      const uint8_t *y, size_t y_stride,
      const uint8_t *cb, size_t cb_stride,
      const uint8_t *cr, size_t cr_stride);

RETRO_END_DECLS

#endif
