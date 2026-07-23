/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RPNG_H__
#define __LIBRETRO_SDK_FORMAT_RPNG_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct rpng rpng_t;

rpng_t *rpng_alloc(void);

bool rpng_is_valid(rpng_t *rpng);

bool rpng_set_buf_ptr(rpng_t *rpng, void *data, size_t len);

/* Prefix decoding support: rpng_set_avail declares how many bytes of
 * the buffer passed to rpng_set_buf_ptr are actually resident (a byte
 * count from the start; monotonic, clamped to the full length).  While
 * the resident frontier is below the full length, rpng_iterate_image
 * stops at a chunk that reaches past it with rpng_need_more() true -
 * distinct from a malformed or final chunk - so a caller feeding a
 * growing read can raise avail and iterate again.  Never calling
 * rpng_set_avail leaves the whole buffer resident (classic behaviour). */
void rpng_set_avail(rpng_t *rpng, size_t avail);
bool rpng_need_more(const rpng_t *rpng);

/* Prefix early-start gate: true once the resident bytes contain the
 * signature and the whole IHDR chunk (33 bytes), i.e. the chunk walk
 * can begin.  Mirrors rjpeg_header_ready. */
bool rpng_header_ready(const uint8_t *data, size_t len);

rpng_t *rpng_alloc(void);

void rpng_free(rpng_t *rpng);

bool rpng_iterate_image(rpng_t *rpng);

int rpng_process_image(rpng_t *rpng,
      void **data, size_t size, unsigned *width, unsigned *height,
      bool supports_rgba);

bool rpng_start(rpng_t *rpng);

bool rpng_save_image_argb(const char *path, const uint32_t *data,
      unsigned width, unsigned height, unsigned pitch);
bool rpng_save_image_bgr24(const char *path, const uint8_t *data,
      unsigned width, unsigned height, unsigned pitch);

uint8_t* rpng_save_image_bgr24_string(const uint8_t *data,
      unsigned width, unsigned height, signed pitch, uint64_t *bytes);

/* Optional HDR colour-space signalling, written as PNG 3rd-edition
 * cICP / cLLI / mDCV chunks. These label the colour space and light
 * levels; they do not alter the pixel data (which for an HDR image is
 * expected to already be encoded, e.g. PQ). For HDR10 use cICP code
 * points primaries=9 (BT.2100), transfer=16 (PQ), matrix=0 (always 0
 * for PNG, since PNG stores RGB), full_range=1. */
struct rpng_hdr_metadata
{
   /* cICP - always written when this struct is passed. */
   uint8_t colour_primaries;      /* H.273 code point, e.g. 9 = BT.2100  */
   uint8_t transfer_function;     /* H.273 code point, e.g. 16 = PQ      */
   uint8_t matrix_coefficients;   /* Must be 0 for PNG (RGB).            */
   uint8_t video_full_range_flag; /* 1 = full range, 0 = narrow.        */

   /* cLLI - written when either value is non-zero. Units: cd/m^2. */
   float max_cll;                 /* Maximum Content Light Level.        */
   float max_fall;                /* Maximum Frame-Average Light Level.  */

   /* mDCV - written when write_mdcv is true. Chromaticities in xy,
    * luminance in cd/m^2. Primaries are ordered R, G, B. */
   uint8_t write_mdcv;
   float primary_chromaticity[3][2]; /* R,G,B -> {x,y}                   */
   float white_point[2];             /* {x,y}                           */
   float max_luminance;              /* Mastering display max, cd/m^2.   */
   float min_luminance;              /* Mastering display min, cd/m^2.   */
};

/* As rpng_save_image_bgr24_string, but also writes the given HDR
 * colour-space chunks (cICP, and cLLI / mDCV as populated). Passing
 * NULL for hdr is identical to rpng_save_image_bgr24_string. */
uint8_t* rpng_save_image_bgr24_hdr_string(const uint8_t *data,
      unsigned width, unsigned height, signed pitch,
      const struct rpng_hdr_metadata *hdr, uint64_t *bytes);

/* Encode a 16-bit-per-channel RGB image (three uint16_t per pixel, in
 * host byte order, R,G,B order) as a 48-bit PNG, optionally with the
 * given HDR colour-space chunks. This is the higher-precision companion
 * to the 8-bit bgr24 path, suitable for HDR content. `pitch` is the
 * row stride in BYTES (typically width * 6). Samples are written to the
 * PNG big-endian per the spec. */
uint8_t* rpng_save_image_rgb48_hdr_string(const uint16_t *data,
      unsigned width, unsigned height, signed pitch,
      const struct rpng_hdr_metadata *hdr, uint64_t *bytes);

/* As rpng_save_image_rgb48_hdr_string, but writes directly to a file
 * at @path. `pitch` is the row stride in bytes (typically width * 6). */
bool rpng_save_image_rgb48_hdr(const char *path, const uint16_t *data,
      unsigned width, unsigned height, unsigned pitch,
      const struct rpng_hdr_metadata *hdr);

/* After a successful decode, retrieve any HDR colour-space metadata
 * parsed from cICP / cLLI / mDCV chunks. Returns true and fills *out
 * when the image carried a cICP chunk; returns false otherwise (in
 * which case the image should be treated as sRGB). */
bool rpng_get_hdr_metadata(rpng_t *rpng, struct rpng_hdr_metadata *out);

/* Request native 10-bit output: when set before decoding, a 16-bit RGB
 * source is decoded to packed XRGB2101010 (R in bits [29:20], G [19:10],
 * B [9:0]) instead of being narrowed to 8-bit ARGB, letting HDR PNGs feed
 * a 10-bit display path. Ignored for 8-bit sources and for RGBA. */
void rpng_set_want_10bit(rpng_t *rpng, int want);

/* True when the decode produced packed 10-bit output (10-bit was requested
 * and the source is a 16-bit RGB image). */
bool rpng_is_10bit(const rpng_t *rpng);

RETRO_END_DECLS

#endif
