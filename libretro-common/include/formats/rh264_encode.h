/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rh264_encode.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RH264_ENCODE_H
#define __LIBRETRO_SDK_FORMAT_RH264_ENCODE_H

/* H.264 video encoder, Constrained Baseline profile: what a recording
 * is made in when AVC is asked for.
 *
 * IDR and P pictures, one slice each, CAVLC. Intra macroblocks are
 * 16x16 or 4x4 predicted; inter macroblocks are one 16x16 partition
 * with a quarter sample vector found by a predictive search, or
 * skipped. The in-loop deblocking filter runs. A constant quantiser, or
 * a bit rate the quantiser follows frame by frame.
 *
 * Every sample is reconstructed as the standard defines it - integer
 * transforms, interpolation and filter - so any conforming decoder
 * shows exactly the frames rh264_encoder_reconstruction() gives.
 *
 * Input is 4:2:0 planes in BT.601 limited range, read where they lie,
 * of an even width and height up to 4096; output is written into the
 * caller's buffer, rh264_encoder_bound() bytes being always enough,
 * as Annex B (start codes, the parameter sets before every IDR picture)
 * or as an MP4 sample (4-byte lengths, the parameter sets in the avcC
 * record instead). The pictures the encoder reconstructs into are
 * allocated by rh264_encoder_new() and nothing after. */

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct
{
   unsigned width, height;        /* even, up to 4096 x 4096 */
   unsigned fps_num, fps_den;     /* frames a second, e.g. 30000/1001 */
   uint32_t bitrate;              /* bits a second; 0 for a constant quantiser */
   unsigned qp;                   /* the quantiser, 1..51 (0: 26); the first one with a bit rate */
   unsigned gop;                  /* an IDR picture every gop frames; 0 for 60 */
} rh264_encoder_config_t;

typedef struct rh264_encoder rh264_encoder_t;

rh264_encoder_t *rh264_encoder_new(const rh264_encoder_config_t *config);
void             rh264_encoder_free(rh264_encoder_t *e);

/* The AVCDecoderConfigurationRecord (an MP4 avcC box's payload): the
 * profile, the level and the parameter sets. Returns the bytes
 * written, 0 if @cap is too small. */
size_t rh264_encoder_avcc(rh264_encoder_t *e, uint8_t *dst, size_t cap);

/* The most bytes rh264_encoder_encode() writes */
size_t rh264_encoder_bound(const rh264_encoder_t *e);

/**
 * rh264_encoder_encode:
 * @e      : the encoder
 * @dst    : the output, @cap bytes
 * @y, @y_stride, @cb, @cb_stride, @cr, @cr_stride : the frame's planes
 * @annexb : start codes and parameter sets, or 4-byte lengths
 * @key    : receives true for an IDR picture
 *
 * Encodes the next frame, a frame duration after the last; pictures
 * are output in the order they come in.
 *
 * Returns: the bytes written, 0 if @cap was too small.
 */
size_t rh264_encoder_encode(rh264_encoder_t *e, uint8_t *dst, size_t cap,
      const uint8_t *y, size_t y_stride, const uint8_t *cb, size_t cb_stride,
      const uint8_t *cr, size_t cr_stride, bool annexb, bool *key);

/* The last frame as a decoder shows it, valid until the next
 * rh264_encoder_encode() */
void rh264_encoder_reconstruction(const rh264_encoder_t *e,
      const uint8_t **y, const uint8_t **cb, const uint8_t **cr,
      unsigned *y_stride, unsigned *c_stride);

RETRO_END_DECLS

#endif
