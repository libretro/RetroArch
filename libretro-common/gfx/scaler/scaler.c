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

static bool point_can_convert_direct(const struct scaler_ctx *ctx)
{
   return ctx->scaler_type == SCALER_TYPE_POINT
      && ctx->out_width <= ctx->in_width
      && ctx->out_height <= ctx->in_height
      /* Near unity, full-frame SIMD conversion is cheaper. */
      && (uint64_t)ctx->out_width * ctx->out_height
         <= (uint64_t)ctx->in_width * ctx->in_height / 4
      && ((ctx->in_fmt == SCALER_FMT_RGB565
               && ctx->out_fmt == SCALER_FMT_ARGB8888)
            || ((ctx->in_fmt == SCALER_FMT_RGB565
                  || ctx->in_fmt == SCALER_FMT_ARGB8888)
               && ctx->out_fmt == SCALER_FMT_BGR24));
}

static void scaler_point_convert(const struct scaler_ctx *ctx,
      void *output_, const void *input_,
      int out_width, int out_height,
      int in_width, int in_height,
      int out_stride, int in_stride)
{
   int y, x;
   uint8_t *output = (uint8_t*)output_;
   const uint8_t *input = (const uint8_t*)input_;
   int input_step = ctx->in_fmt == SCALER_FMT_RGB565
      ? (in_stride >> 1) * 2 : (in_stride >> 2) * 4;
   int output_step = ctx->out_fmt == SCALER_FMT_ARGB8888
      ? (out_stride >> 2) * 4 : out_stride;

   for (y = 0; y < out_height; y++, output += output_step)
   {
      const uint8_t *row = input
         + (ptrdiff_t)ctx->vert.filter_pos[y] * input_step;
      if (ctx->out_fmt == SCALER_FMT_ARGB8888)
      {
         uint32_t *dst = (uint32_t*)output;
         const uint16_t *src = (const uint16_t*)row;
         for (x = 0; x < out_width; x++)
            dst[x] = 0xff000000u | pixconv_rgb565_to_xrgb8888(
                  src[ctx->horiz.filter_pos[x]]);
      }
      else if (ctx->in_fmt == SCALER_FMT_RGB565)
      {
         uint8_t *dst = output;
         const uint16_t *src = (const uint16_t*)row;
         for (x = 0; x < out_width; x++, dst += 3)
         {
            uint32_t p = pixconv_rgb565_to_xrgb8888(
                  src[ctx->horiz.filter_pos[x]]);
            dst[0] = (uint8_t)p;
            dst[1] = (uint8_t)(p >> 8);
            dst[2] = (uint8_t)(p >> 16);
         }
      }
      else
      {
         uint8_t *dst = output;
         const uint32_t *src = (const uint32_t*)row;
         for (x = 0; x < out_width; x++, dst += 3)
         {
            uint32_t p = src[ctx->horiz.filter_pos[x]];
            dst[0] = (uint8_t)p;
            dst[1] = (uint8_t)(p >> 8);
            dst[2] = (uint8_t)(p >> 16);
         }
      }
   }
}

static bool allocate_frames(struct scaler_ctx *ctx)
{
   size_t stride;
   size_t scaled_bytes, input_bytes = 0, output_bytes = 0;
   size_t off_scaled, off_input, off_output, total;
   uint8_t *base;
   void *raw;

   if (     (ctx->in_width  <= 0) || (ctx->in_height  <= 0)
         || (ctx->out_width <= 0) || (ctx->out_height <= 0))
      return false;

   /* Sizes and strides first; nothing is touched until all are known. */
   stride                 = (((size_t)ctx->out_width + 7) & ~(size_t)7)
                          * sizeof(uint64_t);
   if (!(scaled_bytes = frame_bytes(stride, (size_t)ctx->in_height)))
      return false;
   /* Point sampling reads the input directly, not the horizontal pass. */
   if (ctx->scaler_type == SCALER_TYPE_POINT)
      scaled_bytes = 0;
   else
   {
      ctx->scaled.stride  = (int)stride;
      ctx->scaled.width   = ctx->out_width;
      ctx->scaled.height  = ctx->in_height;
   }

   if (       ctx->in_fmt != SCALER_FMT_ARGB8888
           && ctx->in_fmt != SCALER_FMT_XRGB2101010)
   {
      stride              = (((size_t)ctx->in_width + 7) & ~(size_t)7)
                          * sizeof(uint32_t);
      if (!(input_bytes = frame_bytes(stride, (size_t)ctx->in_height)))
         return false;
      ctx->input.stride   = (int)stride;
   }

   if (       ctx->out_fmt != SCALER_FMT_ARGB8888
           && ctx->out_fmt != SCALER_FMT_XRGB2101010)
   {
      stride              = (((size_t)ctx->out_width + 7) & ~(size_t)7)
                          * sizeof(uint32_t);
      if (!(output_bytes = frame_bytes(stride, (size_t)ctx->out_height)))
         return false;
      ctx->output.stride  = (int)stride;
   }

   if (!scaled_bytes && !input_bytes && !output_bytes)
      return true;

   /* Convert only the selected pixels, straight into the destination. */
   if (point_can_convert_direct(ctx))
      return true;

   /* One block for all three, laid out scaled | input | output with the
    * regions that are not needed given zero size. Each frame_bytes()
    * result is at most INT_MAX, so the running total cannot wrap a
    * size_t on any target with a 32-bit or wider size_t. */
   off_scaled             = 0;
   off_input              = SCALER_ARENA_NEXT(off_scaled, scaled_bytes);
   off_output             = SCALER_ARENA_NEXT(off_input,  input_bytes);
   total                  = off_output + output_bytes;

   if (!(raw = malloc(total + SCALER_ARENA_SLACK)))
      return false;

   base                   = SCALER_ARENA_BASE(raw);
   memset(base, 0, total);

   ctx->frame_arena       = raw;
   if (scaled_bytes)
      ctx->scaled.frame   = (uint64_t*)(base + off_scaled);
   if (input_bytes)
      ctx->input.frame    = (uint32_t*)(base + off_input);
   if (output_bytes)
      ctx->output.frame   = (uint32_t*)(base + off_output);

   return true;
}

static bool scaler_ctx_gen_filter_internal(struct scaler_ctx *ctx)
{
   scaler_ctx_gen_reset(ctx);

   ctx->scaler_special = NULL;
   ctx->unscaled       = false;

   /* A conversion at one size runs straight between the caller's
    * buffers, so only a scaled context gets the intermediate frames:
    * a converter set up before its size is known (0 x 0) is unscaled
    * and has nothing to allocate. */
   if (     (   ctx->in_width  != ctx->out_width
             || ctx->in_height != ctx->out_height)
         && !allocate_frames(ctx))
      return false;

   if (     ctx->in_width  == ctx->out_width
         && ctx->in_height == ctx->out_height)
   {
      ctx->unscaled     = true; /* Only pixel format conversion ... */

      /* The rows' own pixels: the output may be a rectangle inside a
       * larger image, whose stride is more than the row */
      if (ctx->in_fmt == ctx->out_fmt)
      {
         switch (ctx->in_fmt)
         {
            case SCALER_FMT_BGR24:
               ctx->direct_pixconv = conv_copy_24;
               break;
            case SCALER_FMT_0RGB1555:
            case SCALER_FMT_RGB565:
            case SCALER_FMT_YUYV:
            case SCALER_FMT_RGBA4444:
               ctx->direct_pixconv = conv_copy_16;
               break;
            default:
               ctx->direct_pixconv = conv_copy_32;
               break;
         }
      }
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
                  case SCALER_FMT_ARGB8888:
                     /* Swapping red and blue is its own inverse. */
                     ctx->direct_pixconv = conv_argb8888_abgr8888;
                     break;
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

   if (!ctx->unscaled && point_can_convert_direct(ctx))
      ctx->scaler_special = scaler_point_convert;

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
   /* The typed buffer pointers are views into the two arenas. */
   if (ctx->filter_arena)
      free(ctx->filter_arena);
   if (ctx->frame_arena)
      free(ctx->frame_arena);
   ctx->filter_arena        = NULL;
   ctx->frame_arena         = NULL;

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

   if (ctx->scaler_special == scaler_point_convert)
   {
      ctx->scaler_special(ctx, output, input,
            ctx->out_width, ctx->out_height,
            ctx->in_width, ctx->in_height,
            ctx->out_stride, ctx->in_stride);
      return;
   }

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
