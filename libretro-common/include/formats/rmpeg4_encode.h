/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmpeg4_encode.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RMPEG4_ENCODE_H
#define __LIBRETRO_SDK_FORMAT_RMPEG4_ENCODE_H

/* MPEG-4 Part 2 video encoder, Simple profile: what a recording is
 * made in when MPEG-4 is asked for.
 *
 * I and P-VOPs; one half sample vector a macroblock, found by a
 * predictive search; intra macroblocks where they cost less; not coded
 * macroblocks; H.263 quantisation; DC prediction. A constant
 * quantiser, or a bit rate the quantiser follows frame by frame. The
 * references are reconstructed with rmpeg4_video's own inverse
 * transform, so rmpeg4_video decodes exactly what the encoder predicted
 * from; another decoder may differ by what two IEEE 1180 transforms
 * may, which the I-VOPs every gop frames bound.
 *
 * Input is 4:2:0 planes in BT.601 limited range, read where they lie;
 * output is written into the caller's buffer, rmpeg4_encoder_bound()
 * bytes being always enough. The planes the encoder reconstructs into
 * are allocated by rmpeg4_encoder_new() and nothing after. */

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct
{
   unsigned width, height;        /* up to 4096 x 4096 */
   unsigned fps_num, fps_den;     /* frames a second, e.g. 30000/1001 */
   uint32_t bitrate;              /* bits a second; 0 for a constant quantiser */
   unsigned qp;                   /* the quantiser, 1..31; the first one with a bit rate */
   unsigned gop;                  /* an I-VOP every gop frames; 0 for 12 */
} rmpeg4_encoder_config_t;

typedef struct rmpeg4_encoder rmpeg4_encoder_t;

rmpeg4_encoder_t *rmpeg4_encoder_new(const rmpeg4_encoder_config_t *config);
void              rmpeg4_encoder_free(rmpeg4_encoder_t *e);

/* The VOS, VO and VOL headers, the decoder configuration a container
 * carries (an MP4 esds); every I-VOP is preceded by them too. Returns
 * the bytes written, 0 if @cap is too small. */
size_t rmpeg4_encoder_header(rmpeg4_encoder_t *e, uint8_t *dst, size_t cap);

/* The most bytes rmpeg4_encoder_encode() writes */
size_t rmpeg4_encoder_bound(const rmpeg4_encoder_t *e);

/**
 * rmpeg4_encoder_encode:
 * @e   : the encoder
 * @dst : the output, @cap bytes
 * @y, @y_stride, @cb, @cb_stride, @cr, @cr_stride : the frame's planes
 * @key : receives true for an I-VOP
 *
 * Encodes the next frame; frames are VOPs a frame duration apart.
 *
 * Returns: the bytes written, 0 if @cap was too small.
 */
size_t rmpeg4_encoder_encode(rmpeg4_encoder_t *e, uint8_t *dst, size_t cap,
      const uint8_t *y, size_t y_stride, const uint8_t *cb, size_t cb_stride,
      const uint8_t *cr, size_t cr_stride, bool *key);

/* The last frame as the encoder reconstructed it - what a decoder
 * with rmpeg4_video's transform shows for it - valid until the next
 * rmpeg4_encoder_encode() */
void rmpeg4_encoder_reconstruction(const rmpeg4_encoder_t *e,
      const uint8_t **y, const uint8_t **cb, const uint8_t **cr,
      unsigned *y_stride, unsigned *c_stride);

RETRO_END_DECLS

#endif
