/* Copyright  (C) 2010-2015 The RetroArch team
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

/**
 * scaler_alloc:
 * @elem_size    : size of the elements to be used.
 * @siz          : size of the image that the scaler needs to handle.
 *
 * Allocate and returns a scaler object.
 *
 * Returns: pointer to a scaler object of type 'void *' on success,
 * NULL in case of error. Has to be freed manually.
 **/
void *scaler_alloc(size_t elem_size, size_t size)
{
   void *ptr = calloc(elem_size, size);
   if (!ptr)
      return NULL;
   return ptr;
}

/**
 * scaler_free:
 * @ptr          : pointer to scaler object.
 *
 * Frees a scaler object.
 **/
void scaler_free(void *ptr)
{
   if (ptr)
      free(ptr);
}

static bool allocate_frames(struct scaler_ctx *ctx)
{
   ctx->scaled.stride = ((ctx->out_width + 7) & ~7) * sizeof(uint64_t);
   ctx->scaled.width  = ctx->out_width;
   ctx->scaled.height = ctx->in_height;
   ctx->scaled.frame  = (uint64_t*)
      scaler_alloc(sizeof(uint64_t),
            (ctx->scaled.stride * ctx->scaled.height) >> 3);
   if (!ctx->scaled.frame)
      return false;

   if (ctx->in_fmt != SCALER_FMT_ARGB8888)
   {
      ctx->input.stride = ((ctx->in_width + 7) & ~7) * sizeof(uint32_t);
      ctx->input.frame = (uint32_t*)
         scaler_alloc(sizeof(uint32_t),
               (ctx->input.stride * ctx->in_height) >> 2);
      if (!ctx->input.frame)
         return false;
   }

   if (ctx->out_fmt != SCALER_FMT_ARGB8888)
   {
      ctx->output.stride = ((ctx->out_width + 7) & ~7) * sizeof(uint32_t);
      ctx->output.frame  = (uint32_t*)
         scaler_alloc(sizeof(uint32_t),
               (ctx->output.stride * ctx->out_height) >> 2);
      if (!ctx->output.frame)
         return false;
   }

   return true;
}

/**
 * set_direct_pix_conv:
 * @ctx          : pointer to scaler context object.
 *
 * Bind a pixel converter callback function to the 'direct_pixconv' function pointer
 * of the scaler context object.
 *
 * Returns: true if a pixel converter function callback could be bound, false if not.
 * If false, the function callback 'direct_pixconv' is still unbound.
 **/
static bool set_direct_pix_conv(struct scaler_ctx *ctx)
{
   if (ctx->in_fmt == ctx->out_fmt)
   {
      ctx->direct_pixconv = conv_copy;
      return true;
   }

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
         /* FIXME/TODO */
         break;
   }

   if (!ctx->direct_pixconv)
      return false;

   return true;
}

static bool set_pix_conv(struct scaler_ctx *ctx)
{
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

      default:
         return false;
   }

   return true;
}

bool scaler_ctx_gen_filter(struct scaler_ctx *ctx)
{
   scaler_ctx_gen_reset(ctx);

   if (ctx->in_width == ctx->out_width && ctx->in_height == ctx->out_height)
      ctx->unscaled = true; /* Only pixel format conversion ... */
   else
   {
      ctx->scaler_horiz = scaler_argb8888_horiz;
      ctx->scaler_vert  = scaler_argb8888_vert;
      ctx->unscaled     = false;
   }

   ctx->scaler_special = NULL;

   if (!allocate_frames(ctx))
      return false;

   if (ctx->unscaled)
   {
      if (!set_direct_pix_conv(ctx))
         return false;
   }
   else
   {
      if (!set_pix_conv(ctx))
         return false;
   }

   if (!ctx->unscaled && !scaler_gen_filter(ctx))
      return false;

   return true;
}

void scaler_ctx_gen_reset(struct scaler_ctx *ctx)
{
   scaler_free(ctx->horiz.filter);
   scaler_free(ctx->horiz.filter_pos);
   scaler_free(ctx->vert.filter);
   scaler_free(ctx->vert.filter_pos);
   scaler_free(ctx->scaled.frame);
   scaler_free(ctx->input.frame);
   scaler_free(ctx->output.frame);

   memset(&ctx->horiz, 0, sizeof(ctx->horiz));
   memset(&ctx->vert, 0, sizeof(ctx->vert));
   memset(&ctx->scaled, 0, sizeof(ctx->scaled));
   memset(&ctx->input, 0, sizeof(ctx->input));
   memset(&ctx->output, 0, sizeof(ctx->output));
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

   if (ctx->unscaled)
   {
      /* Just perform straight pixel conversion. */
      ctx->direct_pixconv(output, input,
            ctx->out_width, ctx->out_height,
            ctx->out_stride, ctx->in_stride);
      return;
   }

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

   if (ctx->scaler_special)
   {
      /* Take some special, and (hopefully) more optimized path. */
      ctx->scaler_special(ctx, output_frame, input_frame,
            ctx->out_width, ctx->out_height,
            ctx->in_width, ctx->in_height,
            output_stride, input_stride);
   }
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
