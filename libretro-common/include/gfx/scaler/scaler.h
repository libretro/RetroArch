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
   SCALER_FMT_RGBA4444
};

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
