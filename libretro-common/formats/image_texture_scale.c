/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_texture_scale.c).
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
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/image.h>
#include <gfx/scaler/scaler.h>

#define UPSCALE_MAX_PIXELS (256u * 1024u * 1024u)

static bool upscale_image(
      unsigned scale_factor,
      struct texture_image *image_src,
      struct texture_image *image_dst)
{
   struct scaler_ctx ctx;
   size_t total_pixels;

   /* Sanity check */
   if ((scale_factor < 2) || !image_src || !image_dst)
      return false;

   if (!image_src->pixels || (image_src->width < 1) || (image_src->height < 1))
      return false;

   /* Get output dimensions */
   image_dst->width  = image_src->width  * scale_factor;
   image_dst->height = image_src->height * scale_factor;

   total_pixels = (size_t)image_dst->width * (size_t)image_dst->height;
   if (total_pixels == 0 || total_pixels > UPSCALE_MAX_PIXELS)
      return false;

   if (!(image_dst->pixels = (uint32_t*)malloc(total_pixels * sizeof(uint32_t))))
      return false;

   /* Duplicate rather than interpolate.
    *
    * This looks like the wrong choice in isolation - at 4x, point
    * sampling leaves 75% of pixels identical to their left neighbour
    * and steps edges 4 levels at once, where interpolation would step
    * 1 - and it was changed to bilinear on that reasoning.  But this
    * texture is not what anyone sees.  The menu hands it to the GPU,
    * which samples it bilinearly into a thumbnail box that is a
    * different size again, so whatever softening happens here happens
    * a second time on top.  Interpolating twice is measurably worse
    * than the GPU interpolating once, which makes upscaling actively
    * harmful: an Amiga save state screenshot drawn into a 960px box
    * came out 3.7% softer with the upscaler on than with it off, and
    * boxart the same way to a lesser degree.  The setting existed to
    * sharpen thumbnails and was blurring them.
    *
    * Duplication survives the GPU's pass instead of compounding with
    * it.  The hard steps land on texel boundaries the bilinear sample
    * then softens by exactly one output pixel, which is the crisp
    * look the setting is for - the same screenshot comes out 12.5%
    * sharper than with no upscaling at all, against -3.7% for
    * bilinear.  It is also the only faithful option for 10-bit, where
    * point sampling copies the packed words untouched.
    *
    * This is why the factor has to stay a whole number: at a
    * fractional ratio the runs come out uneven, some source pixels
    * doubled and some tripled, which is worse than either. */
   memset(&ctx, 0, sizeof(ctx));
   ctx.in_width    = image_src->width;
   ctx.in_height   = image_src->height;
   ctx.in_stride   = image_src->width  * sizeof(uint32_t);
   ctx.out_width   = image_dst->width;
   ctx.out_height  = image_dst->height;
   ctx.out_stride  = image_dst->width  * sizeof(uint32_t);
   ctx.in_fmt      = image_src->pix10
         ? SCALER_FMT_XRGB2101010 : SCALER_FMT_ARGB8888;
   ctx.out_fmt     = ctx.in_fmt;
   ctx.scaler_type = SCALER_TYPE_POINT;

   if (!scaler_ctx_gen_filter(&ctx))
   {
      free(image_dst->pixels);
      image_dst->pixels = NULL;
      return false;
   }

   scaler_ctx_scale(&ctx, image_dst->pixels, image_src->pixels);
   scaler_ctx_gen_reset(&ctx);

   return true;
}

static uint32_t *downscale_box(const uint32_t *src,
      unsigned sw, unsigned sh, unsigned f, bool pix10,
      unsigned *out_w, unsigned *out_h)
{
   unsigned x, y, i, j;
   unsigned n  = f * f;
   unsigned dw = sw / f;
   unsigned dh = sh / f;
   uint32_t *d;

   if ((dw < 1) || (dh < 1))
      return NULL;

   if (!(d = (uint32_t*)malloc((size_t)dw * dh * sizeof(uint32_t))))
      return NULL;

   for (y = 0; y < dh; y++)
   {
      for (x = 0; x < dw; x++)
      {
         unsigned a = 0, r = 0, g = 0, b = 0;

         for (j = 0; j < f; j++)
         {
            const uint32_t *row = src + (size_t)(y * f + j) * sw + x * f;

            if (pix10)
            {
               for (i = 0; i < f; i++)
               {
                  uint32_t p  = row[i];
                  r          += (p >> 20) & 0x3ff;
                  g          += (p >> 10) & 0x3ff;
                  b          +=  p        & 0x3ff;
               }
            }
            else
            {
               for (i = 0; i < f; i++)
               {
                  uint32_t p  = row[i];
                  a          += (p >> 24) & 0xff;
                  r          += (p >> 16) & 0xff;
                  g          += (p >>  8) & 0xff;
                  b          +=  p        & 0xff;
               }
            }
         }

         if (pix10)
            d[(size_t)y * dw + x] = 0xc0000000u
                  | ((r / n) << 20) | ((g / n) << 10) | (b / n);
         else
            d[(size_t)y * dw + x] =
                  ((a / n) << 24) | ((r / n) << 16)
                | ((g / n) <<  8) |  (b / n);
      }
   }

   *out_w = dw;
   *out_h = dh;
   return d;
}

/* Cap a decoded image to 'cap' pixels on its longest side, preserving
 * aspect ratio.
 *
 * The sidebar thumbnail and the fullscreen view share one texture -
 * going fullscreen only raises a flag and fades alpha, it never
 * re-requests the image - so the cap has to be the size the larger of
 * the two views can use, which is the display, not the sidebar box.
 * At display size the fullscreen view is unchanged, since it can
 * never be drawn larger than the panel, and the sidebar downsamples
 * from it on the GPU for free.  Sizing to the sidebar instead would
 * make the fullscreen view soft.
 *
 * Returns true if the image was replaced. */
static bool downscale_image(unsigned cap, struct texture_image *img)
{
   struct scaler_ctx ctx;
   unsigned sw, sh, dw, dh, f;
   uint32_t *mid = NULL;
   uint32_t *out = NULL;
   const uint32_t *src;
   double ratio;

   if (!cap || !img || !img->pixels)
      return false;
   if ((img->width < 1) || (img->height < 1))
      return false;
   if ((img->width <= cap) && (img->height <= cap))
      return false;

   ratio = (img->width > img->height)
         ? (double)cap / (double)img->width
         : (double)cap / (double)img->height;

   dw    = (unsigned)(img->width  * ratio);
   dh    = (unsigned)(img->height * ratio);

   if ((dw < 1) || (dh < 1))
      return false;

   src   = img->pixels;
   sw    = img->width;
   sh    = img->height;

   /* Stage 1: decimate as far as can be done without undershooting
    * the target, leaving the sinc stage a ratio below 2 - and so its
    * minimum 8-tap kernel.  Stopping earlier (at a ratio of 2 or
    * more) widens the kernel to 16 taps or beyond and costs another
    * LSB of DC per step, since the accumulate truncates once per
    * tap. */
   for (f = 1; (sw / (f * 2)) >= dw; f *= 2) ;

   if (f > 1)
   {
      unsigned bw, bh;
      if ((mid = downscale_box(src, sw, sh, f, img->pix10, &bw, &bh)))
      {
         src = mid;
         sw  = bw;
         sh  = bh;
      }
   }

   /* Stage 2: sinc to the exact target. */
   if (!(out = (uint32_t*)malloc((size_t)dw * dh * sizeof(uint32_t))))
   {
      free(mid);
      return false;
   }

   memset(&ctx, 0, sizeof(ctx));
   ctx.in_width    = sw;
   ctx.in_height   = sh;
   ctx.in_stride   = sw * sizeof(uint32_t);
   ctx.out_width   = dw;
   ctx.out_height  = dh;
   ctx.out_stride  = dw * sizeof(uint32_t);
   ctx.in_fmt      = img->pix10
         ? SCALER_FMT_XRGB2101010 : SCALER_FMT_ARGB8888;
   ctx.out_fmt     = ctx.in_fmt;
   ctx.scaler_type = SCALER_TYPE_SINC;

   if (!scaler_ctx_gen_filter(&ctx))
   {
      free(out);
      free(mid);
      return false;
   }

   scaler_ctx_scale(&ctx, out, src);
   scaler_ctx_gen_reset(&ctx);

   free(mid);
   free(img->pixels);

   img->pixels = out;
   img->width  = dw;
   img->height = dh;

   return true;
}

bool image_texture_scale(struct texture_image *img,
      unsigned upscale_threshold, unsigned downscale_cap)
{
   bool changed = false;
   if (!img || !img->pixels || !img->width || !img->height)
      return false;
   /* The resamplers know 8-bit and packed 10-bit texels; a still of
    * half floats is taken at its own size. */
   if (img->fp16)
      return false;
   if (     upscale_threshold > 0
         && (img->width < upscale_threshold || img->height < upscale_threshold))
   {
      unsigned min_size = (img->width < img->height) ? img->width : img->height;
      unsigned max_size = (img->width > img->height) ? img->width : img->height;
      unsigned scale_factor_int = (upscale_threshold + min_size - 1) / min_size;

      /* The factor comes from the shorter side, so the longer one
       * lands at the threshold times the aspect ratio - which on
       * anything wider than 1:1 can overshoot the cap the image is
       * about to be reduced to anyway, filtering pixels into existence
       * only to filter them away again. So the largest whole factor
       * that stays under the cap; whole, not fractional, so the
       * duplication runs stay even (see upscale_image). */
      if (downscale_cap > 0)
      {
         unsigned cap_factor = downscale_cap / max_size;
         if (scale_factor_int > cap_factor)
            scale_factor_int = cap_factor;
      }
      /* Nothing to gain below 2x: the source is already as large as
       * the cap allows */
      if (scale_factor_int > 1)
      {
         struct texture_image img_resampled;
         img_resampled.pixels = NULL;
         img_resampled.width  = 0;
         img_resampled.height = 0;
         if (upscale_image(scale_factor_int, img, &img_resampled))
         {
            free(img->pixels);
            img->pixels = img_resampled.pixels;
            img->width  = img_resampled.width;
            img->height = img_resampled.height;
            changed     = true;
         }
      }
   }
   /* Cap oversized images before they reach the GPU: an 11000x11000
    * PNG is 462 MB of RGBA for a few hundred pixels of screen. 10-bit
    * too - a 16-bit PNG, which rpng packs as XRGB2101010, is exactly
    * the kind of file that gets this large. */
   if (downscale_cap > 0 && downscale_image(downscale_cap, img))
      changed = true;
   return changed;
}
