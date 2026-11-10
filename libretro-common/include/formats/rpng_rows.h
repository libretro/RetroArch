/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng_rows.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RPNG_ROWS_H__
#define __LIBRETRO_SDK_FORMAT_RPNG_ROWS_H__

/* A PNG decoded a row at a time, from input handed over a piece at a time,
 * each row as the PNG stores it: the filter undone, nothing converted -
 * a palette index, gray at 1, 2, 4, 8 or 16 bits, RGB or RGBA at 8 or 16
 * bits a sample, big-endian. What is done with the samples is the
 * caller's: rpng decodes to 32-bit pixels, this is for a caller that has
 * conversions of its own to apply (a console's PNG decoder library,
 * emulated), or that wants rows as the data arrives.
 *
 * rpng_rows_step() takes what input it is given, as much as it needs for
 * the next thing to report, and says how much it took; the rest is given
 * to it again in the next call, with whatever followed. It reports:
 *
 *   RPNG_ROWS_HEADER  once, at the first IDAT: the header and every chunk
 *                     before the image data have been read (the palette,
 *                     tRNS, which ancillary chunks there were)
 *   RPNG_ROWS_ROW     a row, valid until the next call
 *   RPNG_ROWS_END     IEND: the image is complete and its chunks counted
 *   RPNG_ROWS_NEXT    the input is all taken; more is wanted
 *   RPNG_ROWS_ERROR   the file is not a PNG, or is broken
 *
 * An interlaced (Adam7) image comes out pass by pass, each row of a pass
 * naming the image row it belongs to and the columns its pixels go to.
 *
 * The decoder takes its memory when the header is read - two rows and an
 * inflate state - and nothing after. Every chunk's CRC is checked; a bad
 * one in a critical chunk is an error, in an ancillary chunk it drops the
 * chunk, as libpng does.
 *
 * The colour chunks (gAMA, cHRM, sRGB, sBIT, cICP, mDCV) are kept as the
 * file states them, for a caller that converts colour as libpng would:
 * one of the wrong length, out of place (after PLTE or the image data),
 * a second of a kind, or one holding a value the specification forbids
 * is dropped, as libpng drops it. */

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum
{
   RPNG_ROWS_ERROR  = -1,
   RPNG_ROWS_NEXT   =  0,
   RPNG_ROWS_HEADER =  1,
   RPNG_ROWS_ROW    =  2,
   RPNG_ROWS_END    =  3
};

/* The ancillary chunks seen, as bits of rpng_rows_info.chunks. A chunk
 * where the PNG specification does not allow it (gAMA after PLTE, bKGD
 * after IDAT, hIST without a palette, tRNS on an image with alpha) is
 * dropped, as libpng drops it. Bits 0 to 15 are in the order console PNG
 * libraries report them in. */
enum
{
   RPNG_ROWS_GAMA = 1 << 0,
   RPNG_ROWS_SBIT = 1 << 1,
   RPNG_ROWS_CHRM = 1 << 2,
   RPNG_ROWS_PLTE = 1 << 3,
   RPNG_ROWS_TRNS = 1 << 4,
   RPNG_ROWS_BKGD = 1 << 5,
   RPNG_ROWS_HIST = 1 << 6,
   RPNG_ROWS_PHYS = 1 << 7,
   RPNG_ROWS_OFFS = 1 << 8,
   RPNG_ROWS_TIME = 1 << 9,
   RPNG_ROWS_PCAL = 1 << 10,
   RPNG_ROWS_SRGB = 1 << 11,
   RPNG_ROWS_ICCP = 1 << 12,
   RPNG_ROWS_SPLT = 1 << 13,
   RPNG_ROWS_SCAL = 1 << 14,
   RPNG_ROWS_IDAT = 1 << 15,
   RPNG_ROWS_CICP = 1 << 16,
   RPNG_ROWS_MDCV = 1 << 17
};

struct rpng_rows_info
{
   uint32_t width;
   uint32_t height;
   uint32_t chunks;          /* RPNG_ROWS_* */
   uint32_t text_chunks;     /* tEXt, zTXt and iTXt */
   uint32_t unknown_chunks;  /* ancillary, safe to copy, not known here */
   uint16_t palette_len;     /* PLTE entries */
   uint16_t trns_len;        /* tRNS: palette alphas, or 2 (gray) or 6 (RGB) */
   uint8_t  depth;           /* bits a sample: 1, 2, 4, 8 or 16 */
   uint8_t  color_type;      /* 0 gray, 2 RGB, 3 palette, 4 gray+alpha, 6 RGBA */
   uint8_t  channels;        /* 1 to 4 */
   uint8_t  interlace;       /* 1: Adam7 */
   uint8_t  palette[256 * 3];/* RGB triples */
   uint8_t  trns[256];       /* palette alphas, or the key colour as stored */

   /* The colour chunks, each valid where its bit is in chunks */
   uint32_t gama;            /* gAMA: the image's gamma times 100000 */
   int32_t  chrm[8];         /* cHRM: white, red, green and blue x and y,
                                times 100000 */
   uint16_t mdcv[8];         /* mDCV: red, green, blue and white x and y,
                                times 50000 */
   uint32_t mdcv_luminance[2];/* mDCV: maximum and minimum, 0.0001 cd/m2 */
   uint8_t  cicp[4];         /* cICP: primaries, transfer function, matrix,
                                full range */
   uint8_t  sbit[4];         /* sBIT: significant bits of each channel,
                                R, G, B for a palette */
   uint8_t  srgb;            /* sRGB: the rendering intent */
   /* Where each came among the colour chunks, from 1: for rules that go
    * by which came first (libpng's gamma: sRGB's over gAMA's, unless a
    * cICP came before both) */
   uint8_t  gama_at, chrm_at, srgb_at, sbit_at, cicp_at, mdcv_at;
};

struct rpng_rows_row
{
   const uint8_t *data;      /* the row's pixels, packed as the PNG stores them */
   uint32_t       y;         /* the image row it is */
   uint32_t       x0;        /* the column of its first pixel */
   uint32_t       dx;        /* columns from one pixel to the next */
   uint32_t       width;     /* pixels in it */
   uint8_t        pass;      /* Adam7 pass 1 to 7; 0 for a plain image */
};

typedef struct rpng_rows rpng_rows_t;

rpng_rows_t *rpng_rows_new(void);
void rpng_rows_free(rpng_rows_t *z);

/* Back to the state rpng_rows_new() left, for the next file, keeping the
 * memory the last one took where the next fits in it. */
void rpng_rows_reset(rpng_rows_t *z);

/**
 * rpng_rows_step:
 * @in       : the next bytes of the file
 * @len      : how many
 * @consumed : receives how many it took
 * @row      : receives the row for RPNG_ROWS_ROW
 *
 * Returns: one of the RPNG_ROWS_* events, as above.
 */
int rpng_rows_step(rpng_rows_t *z, const uint8_t *in, size_t len,
      size_t *consumed, struct rpng_rows_row *row);

/* What has been read so far: complete for the header once
 * RPNG_ROWS_HEADER is reported, for the chunk counts at RPNG_ROWS_END. */
const struct rpng_rows_info *rpng_rows_info(const rpng_rows_t *z);

/* Bytes in a row of @width pixels of the image's format */
size_t rpng_rows_rowbytes(const struct rpng_rows_info *info, uint32_t width);

RETRO_END_DECLS

#endif
