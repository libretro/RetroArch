/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (scaler.h).
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

#ifndef __LIBRETRO_SDK_SCALER_H__
#define __LIBRETRO_SDK_SCALER_H__

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <clamping.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum scaler_pix_fmt
{
   SCALER_FMT_ARGB8888 = 0,
   SCALER_FMT_ABGR8888,
   SCALER_FMT_0RGB1555,
   SCALER_FMT_RGB565,
   SCALER_FMT_BGR24,
   SCALER_FMT_YUYV,
   SCALER_FMT_RGBA4444,
   /* Packed 10-bit: A [31:30], R [29:20], G [19:10], B [9:0].
    * The layout the 10-bit upload paths and rpng agree on. */
   SCALER_FMT_XRGB2101010
};

/* Reducing by a large ratio in one step loses brightness.
 *
 * The filtered types accumulate with a mulhi that truncates on every
 * tap, and the sinc kernel widens with the ratio - 8 taps at 2:1, 128
 * at 16:1 - so the loss grows with it.  Measured at the centre of a
 * flat 8-bit field, output against input:
 *
 *    1.1:1     8 taps   -0.78%
 *    2:1      16 taps   -0.78%
 *    4:1      32 taps   -2.34%
 *    8:1      64 taps   -4.69%
 *   16:1     128 taps   -7.81%
 *
 * A uniform grey shrunk 16:1 comes back visibly darker.  Nothing
 * reports this - the scale succeeds and the image simply is not the
 * brightness it was - so it is worth knowing before it is diagnosed
 * as a decoder or upload bug.
 *
 * A caller reducing by more than about 2:1 should decimate first,
 * with an integer box average, and leave this a ratio under 2 (see
 * downscale_image() in tasks/task_image.c, or rgui_downscale_box()).
 * Averaging whole pixels is exact, so the staging costs nothing in
 * quality, and it is faster besides - the filter sees far fewer input
 * pixels.  What remains at 8 taps is a single LSB.
 *
 * SCALER_TYPE_POINT does not filter and is unaffected.
 *
 * Fixing this in the scaler would mean rounding rather than
 * truncating in the accumulate, in both the C and SSE2 paths, which
 * are deliberately bit-identical so the C version can test the SIMD
 * one.  SSE2 has no rounding mulhi (_mm_mulhrs_epi16 is SSSE3), so it
 * would cost roughly five instructions where there is now one, in the
 * inner loop of every video path.  Staging at the caller avoids the
 * error entirely and costs nothing per frame, which is why every
 * caller in tree does that instead. */
enum scaler_type
{
   SCALER_TYPE_UNKNOWN = 0,
   SCALER_TYPE_POINT,
   SCALER_TYPE_BILINEAR,
   SCALER_TYPE_SINC
};

struct scaler_filter
{
   int16_t *filter;
   int     *filter_pos;
   int      filter_len;
   int      filter_stride;
};

struct scaler_ctx
{
   void (*scaler_horiz)(const struct scaler_ctx*,
         const void*, int);
   void (*scaler_vert)(const struct scaler_ctx*,
         void*, int);
   void (*scaler_special)(const struct scaler_ctx*,
         void*, const void*, int, int, int, int, int, int);

   void (*in_pixconv)(void*, const void*, int, int, int, int);
   void (*out_pixconv)(void*, const void*, int, int, int, int);
   void (*direct_pixconv)(void*, const void*, int, int, int, int);
   struct scaler_filter horiz, vert;   /* ptr alignment */

   struct
   {
      uint32_t *frame;
      int stride;
   } input;

   struct
   {
      uint64_t *frame;
      int width;
      int height;
      int stride;
   } scaled;

   struct
   {
      uint32_t *frame;
      int stride;
   } output;

   int in_width;
   int in_height;
   int in_stride;

   int out_width;
   int out_height;
   int out_stride;

   enum scaler_pix_fmt in_fmt;
   enum scaler_pix_fmt out_fmt;
   enum scaler_type scaler_type;

   bool unscaled;
};

bool scaler_ctx_gen_filter(struct scaler_ctx *ctx);

void scaler_ctx_gen_reset(struct scaler_ctx *ctx);

/**
 * scaler_ctx_scale:
 * @ctx          : pointer to scaler context object.
 * @output       : pointer to output image.
 * @input        : pointer to input image.
 *
 * Scales an input image to an output image.
 **/
void scaler_ctx_scale(struct scaler_ctx *ctx,
      void *output, const void *input);

RETRO_END_DECLS

#endif
