/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (scaler.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gfx/scaler/scaler.h>
#include <gfx/scaler/scaler_int.h>
#include <gfx/scaler/filter.h>
#include <gfx/scaler/pixconv.h>

static bool allocate_frames(struct scaler_ctx *ctx)
{
   uint64_t *scaled_frame = NULL;
   ctx->scaled.stride     = ((ctx->out_width + 7) & ~7) * sizeof(uint64_t);
   ctx->scaled.width      = ctx->out_width;
   ctx->scaled.height     = ctx->in_height;
   scaled_frame           = (uint64_t*)calloc(sizeof(uint64_t),
            (ctx->scaled.stride * ctx->scaled.height) >> 3);

   if (!scaled_frame)
      return false;

   ctx->scaled.frame      = scaled_frame;

   if (ctx->in_fmt != SCALER_FMT_ARGB8888)
   {
      uint32_t *input_frame = NULL;
      ctx->input.stride     = ((ctx->in_width + 7) & ~7) * sizeof(uint32_t);
      input_frame           = (uint32_t*)calloc(sizeof(uint32_t),
               (ctx->input.stride * ctx->in_height) >> 2);

      if (!input_frame)
         return false;

      ctx->input.frame      = input_frame;
   }

   if (ctx->out_fmt != SCALER_FMT_ARGB8888)
   {
      uint32_t *output_frame = NULL;
      ctx->output.stride     = ((ctx->out_width + 7) & ~7) * sizeof(uint32_t);

      output_frame           = (uint32_t*)calloc(sizeof(uint32_t),
               (ctx->output.stride * ctx->out_height) >> 2);

      if (!output_frame)
         return false;

      ctx->output.frame      = output_frame;
   }

   return true;
}

bool scaler_ctx_gen_filter(struct scaler_ctx *ctx)
{
   scaler_ctx_gen_reset(ctx);

   ctx->scaler_special = NULL;
   ctx->unscaled       = false;

   if (!allocate_frames(ctx))
      return false;

   if (     ctx->in_width  == ctx->out_width
         && ctx->in_height == ctx->out_height)
   {
      ctx->unscaled     = true; /* Only pixel format conversion ... */

      if (ctx->in_fmt == ctx->out_fmt)
         ctx->direct_pixconv = conv_copy;
      else
      {
         /* Bind a pixel converter callback function to the
          * 'direct_pixconv' function pointer of the scaler context object. */
         switch (ctx->in_fmt)
         {
            case SCALER_FMT_0RGB1555:
               switch (ctx->out_fmt)
               {
                  case SCALER_FMT_ARGB8888:
                     ctx->direct_pixconv = conv_0rgb1555_argb8888;
                     break;
                  case SCALER_FMT_RGB565:
                     ctx->direct_pixconv = conv_0rgb1555_rgb565;
                     break;
                  case SCALER_FMT_BGR24:
                     ctx->direct_pixconv = conv_0rgb1555_bgr24;
                     break;
                  default:
                     break;
               }
               break;
            case SCALER_FMT_RGB565:
               switch (ctx->out_fmt)
               {
                  case SCALER_FMT_ARGB8888:
                     ctx->direct_pixconv = conv_rgb565_argb8888;
                     break;
                  case SCALER_FMT_ABGR8888:
                     ctx->direct_pixconv = conv_rgb565_abgr8888;
                     break;
                  case SCALER_FMT_BGR24:
                     ctx->direct_pixconv = conv_rgb565_bgr24;
                     break;
                  case SCALER_FMT_0RGB1555:
                     ctx->direct_pixconv = conv_rgb565_0rgb1555;
                     break;
                  default:
                     break;
               }
               break;
            case SCALER_FMT_BGR24:
               switch (ctx->out_fmt)
               {
                  case SCALER_FMT_ARGB8888:
                     ctx->direct_pixconv = conv_bgr24_argb8888;
                     break;
                  case SCALER_FMT_RGB565:
                     ctx->direct_pixconv = conv_bgr24_rgb565;
                  default:
                     break;
               }
               break;
            case SCALER_FMT_ARGB8888:
               switch (ctx->out_fmt)
               {
                  case SCALER_FMT_0RGB1555:
                     ctx->direct_pixconv = conv_argb8888_0rgb1555;
                     break;
                  case SCALER_FMT_BGR24:
                     ctx->direct_pixconv = conv_argb8888_bgr24;
                     break;
                  case SCALER_FMT_ABGR8888:
                     ctx->direct_pixconv = conv_argb8888_abgr8888;
                     break;
                  case SCALER_FMT_RGBA4444:
                     ctx->direct_pixconv = conv_argb8888_rgba4444;
                     break;
                  default:
                     break;
               }
               break;
            case SCALER_FMT_YUYV:
               switch (ctx->out_fmt)
               {
                  case SCALER_FMT_ARGB8888:
                     ctx->direct_pixconv = conv_yuyv_argb8888;
                     break;
                  default:
                     break;
               }
               break;
            case SCALER_FMT_RGBA4444:
               switch (ctx->out_fmt)
               {
                  case SCALER_FMT_ARGB8888:
                     ctx->direct_pixconv = conv_rgba4444_argb8888;
                     break;
                  case SCALER_FMT_RGB565:
                     ctx->direct_pixconv = conv_rgba4444_rgb565;
                     break;
                  default:
                     break;
               }
               break;
            case SCALER_FMT_ABGR8888:
               switch (ctx->out_fmt)
               {
                  case SCALER_FMT_BGR24:
                     ctx->direct_pixconv = conv_abgr8888_bgr24;
                     break;
                  default:
                     break;
               }
               break;
         }

         if (!ctx->direct_pixconv)
            return false;
      }
   }
   else
   {
      ctx->scaler_horiz = scaler_argb8888_horiz;
      ctx->scaler_vert  = scaler_argb8888_vert;

      switch (ctx->in_fmt)
      {
         case SCALER_FMT_ARGB8888:
            /* No need to convert :D */
            break;

         case SCALER_FMT_0RGB1555:
            ctx->in_pixconv = conv_0rgb1555_argb8888;
            break;

         case SCALER_FMT_RGB565:
            ctx->in_pixconv = conv_rgb565_argb8888;
            break;

         case SCALER_FMT_BGR24:
            ctx->in_pixconv = conv_bgr24_argb8888;
            break;

         case SCALER_FMT_RGBA4444:
            ctx->in_pixconv = conv_rgba4444_argb8888;
            break;

         default:
            return false;
      }

      switch (ctx->out_fmt)
      {
         case SCALER_FMT_ARGB8888:
            /* No need to convert :D */
            break;

         case SCALER_FMT_RGBA4444:
            ctx->out_pixconv = conv_argb8888_rgba4444;
            break;

         case SCALER_FMT_0RGB1555:
            ctx->out_pixconv = conv_argb8888_0rgb1555;
            break;

         case SCALER_FMT_BGR24:
            ctx->out_pixconv = conv_argb8888_bgr24;
            break;

         case SCALER_FMT_ABGR8888:
            ctx->out_pixconv = conv_argb8888_abgr8888;
            break;

         default:
            return false;
      }

      if (!scaler_gen_filter(ctx))
         return false;
   }

   return true;
}

void scaler_ctx_gen_reset(struct scaler_ctx *ctx)
{
   if (ctx->horiz.filter)
      free(ctx->horiz.filter);
   if (ctx->horiz.filter_pos)
      free(ctx->horiz.filter_pos);
   if (ctx->vert.filter)
      free(ctx->vert.filter);
   if (ctx->vert.filter_pos)
      free(ctx->vert.filter_pos);
   if (ctx->scaled.frame)
      free(ctx->scaled.frame);
   if (ctx->input.frame)
      free(ctx->input.frame);
   if (ctx->output.frame)
      free(ctx->output.frame);

   ctx->horiz.filter        = NULL;
   ctx->horiz.filter_len    = 0;
   ctx->horiz.filter_stride = 0;
   ctx->horiz.filter_pos    = NULL;

   ctx->vert.filter         = NULL;
   ctx->vert.filter_len     = 0;
   ctx->vert.filter_stride  = 0;
   ctx->vert.filter_pos     = NULL;

   ctx->scaled.frame        = NULL;
   ctx->scaled.width        = 0;
   ctx->scaled.height       = 0;
   ctx->scaled.stride       = 0;

   ctx->input.frame         = NULL;
   ctx->input.stride        = 0;

   ctx->output.frame        = NULL;
   ctx->output.stride       = 0;
}

/**
 * scaler_ctx_scale:
 * @ctx          : pointer to scaler context object.
 * @output       : pointer to output image.
 * @input        : pointer to input image.
 *
 * Scales an input image to an output image.
 **/
void scaler_ctx_scale(struct scaler_ctx *ctx,
      void *output, const void *input)
{
   const void *input_frame = input;
   void *output_frame      = output;
   int input_stride        = ctx->in_stride;
   int output_stride       = ctx->out_stride;

   if (ctx->in_fmt != SCALER_FMT_ARGB8888)
   {
      ctx->in_pixconv(ctx->input.frame, input,
            ctx->in_width, ctx->in_height,
            ctx->input.stride, ctx->in_stride);

      input_frame       = ctx->input.frame;
      input_stride      = ctx->input.stride;
   }

   if (ctx->out_fmt != SCALER_FMT_ARGB8888)
   {
      output_frame  = ctx->output.frame;
      output_stride = ctx->output.stride;
   }

   /* Take some special, and (hopefully) more optimized path. */
   if (ctx->scaler_special)
      ctx->scaler_special(ctx, output_frame, input_frame,
            ctx->out_width, ctx->out_height,
            ctx->in_width, ctx->in_height,
            output_stride, input_stride);
   else
   {
      /* Take generic filter path. */
      if (ctx->scaler_horiz)
         ctx->scaler_horiz(ctx, input_frame, input_stride);
      if (ctx->scaler_vert)
         ctx->scaler_vert (ctx, output, output_stride);
   }

   if (ctx->out_fmt != SCALER_FMT_ARGB8888)
      ctx->out_pixconv(output, ctx->output.frame,
            ctx->out_width, ctx->out_height,
            ctx->out_stride, ctx->output.stride);
}
