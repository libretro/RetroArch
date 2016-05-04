#ifndef _VIDEO_FRAME_H
#define _VIDEO_FRAME_H

#include <stdint.h>

#include <gfx/scaler/scaler.h>

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

   scaler_ctx_scale(scaler, output, input);
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
   scaler_ctx_scale(scaler, output, input);
}

static INLINE void video_frame_convert_to_bgr24(
      struct scaler_ctx *scaler,
      void *output, const void *input,
      int width, int height, int in_pitch,
      bool bgr24)
{
   scaler->in_width    = width;
   scaler->in_height   = height;
   scaler->out_width   = width;
   scaler->out_height  = height;
   if (bgr24)
      scaler->in_fmt   = SCALER_FMT_BGR24;
   else if (video_driver_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888)
      scaler->in_fmt   = SCALER_FMT_ARGB8888;
   else
      scaler->in_fmt   = SCALER_FMT_RGB565;
   scaler->out_fmt     = SCALER_FMT_BGR24;
   scaler->scaler_type = SCALER_TYPE_POINT;
   scaler_ctx_gen_filter(scaler);

   scaler->in_stride   = in_pitch;
   scaler->out_stride  = width * 3;

   scaler_ctx_scale(scaler, output, input);
}

#endif
