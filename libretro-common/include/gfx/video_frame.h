/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (video_frame.h).
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

#ifndef _LIBRETRO_SDK_VIDEO_FRAME_H
#define _LIBRETRO_SDK_VIDEO_FRAME_H

#include <stdint.h>
#include <retro_common_api.h>
#include <retro_inline.h>

#include <gfx/scaler/scaler.h>

#include <libretro.h>

RETRO_BEGIN_DECLS

#define scaler_ctx_scale_direct(ctx, output, input) \
{ \
   if (ctx && ctx->unscaled && ctx->direct_pixconv) \
      /* Just perform straight pixel conversion. */ \
      ctx->direct_pixconv(output, input, \
            ctx->out_width,  ctx->out_height, \
            ctx->out_stride, ctx->in_stride); \
   else \
      scaler_ctx_scale(ctx, output, input); \
}

static INLINE void video_frame_convert_rgb16_to_rgb32(
      struct scaler_ctx *scaler,
      void *output,
      const void *input,
      int width, int height,
      int in_pitch)
{
   if (width != scaler->in_width || height != scaler->in_height)
   {
      scaler->in_width    = width;
      scaler->in_height   = height;
      scaler->out_width   = width;
      scaler->out_height  = height;
      scaler->in_fmt      = SCALER_FMT_RGB565;
      scaler->out_fmt     = SCALER_FMT_ARGB8888;
      scaler->scaler_type = SCALER_TYPE_POINT;
      scaler_ctx_gen_filter(scaler);
   }

   scaler->in_stride  = in_pitch;
   scaler->out_stride = width * sizeof(uint32_t);

   scaler_ctx_scale_direct(scaler, output, input);
}

static INLINE void video_frame_scale(
      struct scaler_ctx *scaler,
      void *output,
      const void *input,
      enum scaler_pix_fmt format,
      unsigned scaler_width,
      unsigned scaler_height,
      unsigned scaler_pitch,
      unsigned width,
      unsigned height,
      unsigned pitch)
{
   if (
            width  != (unsigned)scaler->in_width
         || height != (unsigned)scaler->in_height
         || format != scaler->in_fmt
         || pitch  != (unsigned)scaler->in_stride
      )
   {
      scaler->in_fmt    = format;
      scaler->in_width  = width;
      scaler->in_height = height;
      scaler->in_stride = pitch;

      scaler->out_width  = scaler_width;
      scaler->out_height = scaler_height;
      scaler->out_stride = scaler_pitch;

      scaler_ctx_gen_filter(scaler);
   }

   scaler_ctx_scale_direct(scaler, output, input);
}

static INLINE void video_frame_record_scale(
      struct scaler_ctx *scaler,
      void *output,
      const void *input,
      unsigned scaler_width,
      unsigned scaler_height,
      unsigned scaler_pitch,
      unsigned width,
      unsigned height,
      unsigned pitch,
      bool bilinear)
{
   if (
            width  != (unsigned)scaler->in_width
         || height != (unsigned)scaler->in_height
      )
   {
      scaler->in_width    = width;
      scaler->in_height   = height;
      scaler->in_stride   = pitch;

      scaler->scaler_type = bilinear ?
         SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT;

      scaler->out_width  = scaler_width;
      scaler->out_height = scaler_height;
      scaler->out_stride = scaler_pitch;

      scaler_ctx_gen_filter(scaler);
   }

   scaler_ctx_scale_direct(scaler, output, input);
}

static INLINE void video_frame_convert_argb8888_to_abgr8888(
      struct scaler_ctx *scaler,
      void *output, const void *input,
      int width, int height, int in_pitch)
{
   if (width != scaler->in_width || height != scaler->in_height)
   {
      scaler->in_width    = width;
      scaler->in_height   = height;
      scaler->out_width   = width;
      scaler->out_height  = height;
      scaler->in_fmt      = SCALER_FMT_ARGB8888;
      scaler->out_fmt     = SCALER_FMT_ABGR8888;
      scaler->scaler_type = SCALER_TYPE_POINT;
      scaler_ctx_gen_filter(scaler);
   }

   scaler->in_stride  = in_pitch;
   scaler->out_stride = width * sizeof(uint32_t);

   scaler_ctx_scale_direct(scaler, output, input);
}

static INLINE void video_frame_convert_to_bgr24(
      struct scaler_ctx *scaler,
      void *output, const void *input,
      int width, int height, int in_pitch)
{
   scaler->in_width    = width;
   scaler->in_height   = height;
   scaler->out_width   = width;
   scaler->out_height  = height;
   scaler->out_fmt     = SCALER_FMT_BGR24;
   scaler->scaler_type = SCALER_TYPE_POINT;

   scaler_ctx_gen_filter(scaler);

   scaler->in_stride   = in_pitch;
   scaler->out_stride  = width * 3;

   scaler_ctx_scale_direct(scaler, output, input);
}

static INLINE void video_frame_convert_rgba_to_bgr(
      const void *src_data,
      void *dst_data,
      unsigned width)
{
   unsigned x;
   uint8_t      *dst  = (uint8_t*)dst_data;
   const uint8_t *src = (const uint8_t*)src_data;

   for (x = 0; x < width; x++, dst += 3, src += 4)
   {
      dst[0] = src[2];
      dst[1] = src[1];
      dst[2] = src[0];
   }
}

static INLINE bool video_pixel_frame_scale(
      struct scaler_ctx *scaler,
      void *output, const void *data,
      unsigned width, unsigned height,
      size_t pitch)
{
   scaler->in_width      = width;
   scaler->in_height     = height;
   scaler->out_width     = width;
   scaler->out_height    = height;
   scaler->in_stride     = (int)pitch;
   scaler->out_stride    = width * sizeof(uint16_t);

   scaler_ctx_scale_direct(scaler, output, data);

   return true;
}

RETRO_END_DECLS

#endif
