/* Copyright  (C) 2010-2020 The RetroArch team
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
#include <limits.h>
#include <math.h>

#include <gfx/scaler/scaler.h>
#include <gfx/scaler/scaler_int.h>
#include <gfx/scaler/filter.h>
#include <gfx/scaler/pixconv.h>

/* Byte size of a frame buffer, or 0 if it is not one this scaler can
 * address.
 *
 * Every frame here is described by an int stride, and the passes walk
 * rows with int arithmetic - scaler_argb8888_vert() forms its row base
 * as filter_pos[h] * (scaled.stride >> 3), which is an int multiply.
 * A frame is therefore only usable if its whole byte size fits in an
 * int, and computing that size has to happen in size_t to find out.
 *
 * It did not.  allocate_frames() multiplied the int stride by the int
 * row count directly, and for a large enough frame that product does
 * not merely overflow, it wraps to something small and plausible: a
 * 262144-wide output over an 8192-row input needs 16 GB and asked for
 * 1 byte, whereupon scaler_ctx_gen_filter() reported success and the
 * horizontal pass wrote through it:
 *
 *   ERROR: AddressSanitizer: heap-buffer-overflow
 *   WRITE of size 8
 *     #0 scaler_argb8888_horiz scaler_int.c
 *     #1 scaler_ctx_scale scaler.c
 *   0x502000000011 is located 0 bytes after 1-byte region
 *
 * Anything that wraps to a large value instead simply fails the
 * allocation, which was always the harmless half of this. */
static size_t frame_bytes(size_t stride, size_t rows)
{
   if ((stride == 0) || (rows == 0))
      return 0;
   if (stride > (size_t)INT_MAX)
      return 0;
   if (rows > (size_t)INT_MAX / stride)
      return 0;
   return stride * rows;
}

static bool allocate_frames(struct scaler_ctx *ctx)
{
   uint64_t *scaled_frame = NULL;
   size_t stride;
   size_t bytes;

   /* Nothing below is meaningful for a degenerate rectangle, and the
    * size arithmetic assumes these are positive. */
   if (     (ctx->in_width  <= 0) || (ctx->in_height  <= 0)
         || (ctx->out_width <= 0) || (ctx->out_height <= 0))
      return false;

   stride                 = (((size_t)ctx->out_width + 7) & ~(size_t)7)
                          * sizeof(uint64_t);

   if (!(bytes = frame_bytes(stride, (size_t)ctx->in_height)))
      return false;

   ctx->scaled.stride     = (int)stride;
   ctx->scaled.width      = ctx->out_width;
   ctx->scaled.height     = ctx->in_height;
   scaled_frame           = (uint64_t*)calloc(bytes >> 3, sizeof(uint64_t));

   if (!scaled_frame)
      return false;

   ctx->scaled.frame      = scaled_frame;

   if (       ctx->in_fmt != SCALER_FMT_ARGB8888
           && ctx->in_fmt != SCALER_FMT_XRGB2101010)
   {
      uint32_t *input_frame = NULL;

      stride                = (((size_t)ctx->in_width + 7) & ~(size_t)7)
                            * sizeof(uint32_t);

      if (!(bytes = frame_bytes(stride, (size_t)ctx->in_height)))
         return false;

      ctx->input.stride     = (int)stride;
      input_frame           = (uint32_t*)calloc(bytes >> 2, sizeof(uint32_t));

      if (!input_frame)
         return false;

      ctx->input.frame      = input_frame;
   }

   if (       ctx->out_fmt != SCALER_FMT_ARGB8888
           && ctx->out_fmt != SCALER_FMT_XRGB2101010)
   {
      uint32_t *output_frame = NULL;

      stride                 = (((size_t)ctx->out_width + 7) & ~(size_t)7)
                             * sizeof(uint32_t);

      if (!(bytes = frame_bytes(stride, (size_t)ctx->out_height)))
         return false;

      ctx->output.stride     = (int)stride;
      output_frame           = (uint32_t*)calloc(bytes >> 2, sizeof(uint32_t));

      if (!output_frame)
         return false;

      ctx->output.frame      = output_frame;
   }

   return true;
}

static bool scaler_ctx_gen_filter_internal(struct scaler_ctx *ctx)
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

            case SCALER_FMT_XRGB2101010:
               /* No cross-format direct conversion: every counterpart
                * here is 8-bit, and the narrowing direction is the
                * caller's decision to make, not this switch's.  The
                * same-format case never reaches here (conv_copy is
                * bound above). */
               break;
         }

         if (!ctx->direct_pixconv)
            return false;
      }
   }
   else if (   ctx->in_fmt  == SCALER_FMT_XRGB2101010
            || ctx->out_fmt == SCALER_FMT_XRGB2101010)
   {
      /* 10-bit is filtered natively rather than through the ARGB8888
       * canonical form: the 8-bit chain saturates to 8 bits at the end,
       * so routing 10-bit samples through it would discard exactly the
       * precision the format exists to carry.  Only 10-bit to 10-bit is
       * offered; mixing with the 8-bit formats would need a conversion
       * whose direction silently decides what is lost, which is better
       * left to the caller. */
      if (ctx->in_fmt != ctx->out_fmt)
         return false;

      ctx->scaler_horiz = scaler_xrgb2101010_horiz;
      ctx->scaler_vert  = scaler_xrgb2101010_vert;

      if (!scaler_gen_filter(ctx))
         return false;
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

/* Fail with nothing left attached.
 *
 * The body above bails from a dozen places, several of them after it
 * has already allocated buffers or bound a scaling function, and it
 * left all of that behind on the way out.  Two consequences, both
 * live:
 *
 * The buffers leaked.  Only rgui cleaned up after a failure, with a
 * defensive scaler_ctx_gen_reset() and a comment guessing there might
 * be leftovers; every other caller - both in tasks/task_image.c, the
 * gl2/gl3/vulkan readback paths, the camera drivers - simply returned.
 *
 * Worse, a context could fail with scaler_horiz and scaler_vert bound
 * but their filters freed or never validated, and scaler_ctx_scale()
 * has no way to tell that apart from a working context.  Callers that
 * ignore the return value - tasks/task_translation.c does, twice -
 * then scale through it.  Scaling a 1x1 source, where validate_filter()
 * rejects the generated positions, reads past the end of the source
 * row:
 *
 *   ERROR: AddressSanitizer: stack-buffer-overflow
 *     #0 scaler_argb8888_horiz scaler_int.c
 *     #1 scaler_ctx_scale scaler.c
 *
 * So release what was allocated and unbind everything.  A failed
 * context is then inert rather than half-built, and scaler_ctx_scale()
 * on one is a no-op - see the guard there. */
bool scaler_ctx_gen_filter(struct scaler_ctx *ctx)
{
   if (scaler_ctx_gen_filter_internal(ctx))
      return true;

   scaler_ctx_gen_reset(ctx);

   ctx->scaler_horiz   = NULL;
   ctx->scaler_vert    = NULL;
   ctx->scaler_special = NULL;
   ctx->direct_pixconv = NULL;
   ctx->in_pixconv     = NULL;
   ctx->out_pixconv    = NULL;
   ctx->unscaled       = false;

   return false;
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

   /* Source and destination are the same size: there is nothing to
    * filter, only a possible pixel format conversion.
    * scaler_ctx_gen_filter recognises this, binds direct_pixconv and
    * returns without generating a filter or setting scaler_horiz /
    * scaler_vert.  Honour that here: without it the generic path below
    * finds both function pointers NULL, writes nothing at all, and the
    * caller gets its output buffer back exactly as it was - which for a
    * freshly malloc'd buffer means an image of uninitialised memory, or
    * a fully transparent one where the allocation came from fresh
    * zeroed pages. */
   if (ctx->unscaled)
   {
      if (ctx->direct_pixconv)
         ctx->direct_pixconv(output, input,
               ctx->out_width, ctx->out_height,
               ctx->out_stride, ctx->in_stride);
      return;
   }

   /* Nothing bound to scale with: either scaler_ctx_gen_filter() was
    * never called on this context, or it failed and unbound itself.
    * Return before the pixel converters below, which are called
    * unconditionally on their formats and would run against the
    * buffers a failure just freed. */
   if (     !ctx->scaler_special
         && !ctx->scaler_horiz
         && !ctx->scaler_vert)
      return;

   if (       ctx->in_fmt != SCALER_FMT_ARGB8888
           && ctx->in_fmt != SCALER_FMT_XRGB2101010)
   {
      ctx->in_pixconv(ctx->input.frame, input,
            ctx->in_width, ctx->in_height,
            ctx->input.stride, ctx->in_stride);

      input_frame       = ctx->input.frame;
      input_stride      = ctx->input.stride;
   }

   if (       ctx->out_fmt != SCALER_FMT_ARGB8888
           && ctx->out_fmt != SCALER_FMT_XRGB2101010)
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
         ctx->scaler_vert (ctx, output_frame, output_stride);
   }

   if (       ctx->out_fmt != SCALER_FMT_ARGB8888
           && ctx->out_fmt != SCALER_FMT_XRGB2101010)
      ctx->out_pixconv(output, ctx->output.frame,
            ctx->out_width, ctx->out_height,
            ctx->out_stride, ctx->output.stride);
}
