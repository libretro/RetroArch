/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (scaler_filter.c).
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
#include <string.h>

#include <gfx/scaler/filter.h>
#include <gfx/scaler/scaler_int.h>
#include <retro_inline.h>
#include <filters.h>
#include <retro_math.h>

#define FILTER_UNITY (1 << 14)

static INLINE void gen_filter_point_sub(struct scaler_filter *filter,
      int len, int pos, int step)
{
   int i;
   for (i = 0; i < len; i++, pos += step)
   {
      filter->filter_pos[i] = pos >> 16;
      filter->filter[i]     = FILTER_UNITY;
   }
}

static INLINE void gen_filter_bilinear_sub(struct scaler_filter *filter,
      int len, int pos, int step)
{
   int i;
   for (i = 0; i < len; i++, pos += step)
   {
      filter->filter_pos[i]     = pos >> 16;
      filter->filter[i * 2 + 1] = (pos & 0xffff) >> 2;
      filter->filter[i * 2 + 0] = FILTER_UNITY - filter->filter[i * 2 + 1];
   }
}

static INLINE void gen_filter_sinc_sub(struct scaler_filter *filter,
      int len, int pos, int step, double phase_mul)
{
   int i, j;
   const int sinc_size = filter->filter_len;

   for (i = 0; i < len; i++, pos += step)
   {
      filter->filter_pos[i] = pos >> 16;

      for (j = 0; j < sinc_size; j++)
      {
         double sinc_phase    = M_PI * ((double)((sinc_size << 15) + (pos & 0xffff)) / 0x10000 - j);
         double lanczos_phase = sinc_phase / ((sinc_size >> 1));
         int16_t sinc_val     = FILTER_UNITY * sinc(sinc_phase * phase_mul) * sinc(lanczos_phase) * phase_mul;

         filter->filter[i * sinc_size + j] = sinc_val;
      }
   }
}

/* Renormalize each output pixel's taps to sum to FILTER_UNITY.
 *
 * Two things cost a filter its unity gain, and both are silent:
 * gen_filter_sinc_sub truncates a windowed sinc to sinc_size taps and
 * rounds each to an int16 without ever checking what they add up to,
 * and fixup_filter_sub then zeroes whichever of those taps fall
 * outside the source rectangle.  Whatever energy those steps remove
 * is removed from the picture: a flat field comes back darker, by
 * more as the ratio (and so the kernel) grows - measured at -8.6%
 * scaling 11000 down to 1080, which is plainly visible, and at -0.8%
 * even for a 2:1 bilinear reduction.
 *
 * A resampler has to reproduce a constant input exactly, so scale the
 * taps back up to unity.  The largest tap absorbs the rounding
 * remainder, which keeps the sum exact rather than merely close;
 * without that the residue is a fresh (smaller) DC error.
 *
 * A row whose taps sum to zero carries no signal and is left alone -
 * scaling it cannot produce unity and would only amplify noise. */
static void normalize_filter_sub(struct scaler_filter *filter, int len)
{
   int i, j;

   for (i = 0; i < len; i++)
   {
      int16_t *row = filter->filter + i * filter->filter_stride;
      int sum      = 0;
      int rem;
      int best     = 0;
      int best_abs = -1;

      for (j = 0; j < filter->filter_len; j++)
         sum += row[j];

      if (sum == 0 || sum == FILTER_UNITY)
         continue;

      for (j = 0; j < filter->filter_len; j++)
      {
         int scaled = (int)(((int64_t)row[j] * FILTER_UNITY) / sum);

         if (scaled >  0x7fff)
            scaled =  0x7fff;
         if (scaled < -0x8000)
            scaled = -0x8000;

         row[j] = (int16_t)scaled;
      }

      /* Hand the rounding remainder to the largest tap, so the row
       * sums to exactly FILTER_UNITY. */
      sum = 0;
      for (j = 0; j < filter->filter_len; j++)
      {
         int a = row[j] < 0 ? -row[j] : row[j];

         sum += row[j];

         if (a > best_abs)
         {
            best_abs = a;
            best     = j;
         }
      }

      rem = FILTER_UNITY - sum;

      if (rem != 0 && best_abs >= 0)
      {
         int v = (int)row[best] + rem;

         if (v >  0x7fff)
            v =  0x7fff;
         if (v < -0x8000)
            v = -0x8000;

         row[best] = (int16_t)v;
      }
   }
}

static bool validate_filter(struct scaler_ctx *ctx)
{
   int i;
   int max_h_pos;
   int max_w_pos = ctx->in_width - ctx->horiz.filter_len;

   for (i = 0; i < ctx->out_width; i++)
   {
      if (ctx->horiz.filter_pos[i] > max_w_pos || ctx->horiz.filter_pos[i] < 0)
         return false;
   }

   max_h_pos = ctx->in_height - ctx->vert.filter_len;

   for (i = 0; i < ctx->out_height; i++)
   {
      if (ctx->vert.filter_pos[i] > max_h_pos || ctx->vert.filter_pos[i] < 0)
         return false;
   }

   return true;
}

static void fixup_filter_sub(struct scaler_filter *filter,
      int out_len, int in_len)
{
   int i;
   int max_pos = in_len - filter->filter_len;

   for (i = 0; i < out_len; i++)
   {
      int postsample =  filter->filter_pos[i] - max_pos;
      int presample  = -filter->filter_pos[i];

      if (postsample > 0)
      {
         int16_t *base_filter   = NULL;

         filter->filter_pos[i] -= postsample;

         base_filter            = filter->filter + i * filter->filter_stride;

         if (postsample > (int)filter->filter_len)
            memset(base_filter, 0, filter->filter_len * sizeof(int16_t));
         else
         {
            memmove(base_filter + postsample, base_filter,
                  (filter->filter_len - postsample) * sizeof(int16_t));
            memset(base_filter, 0, postsample * sizeof(int16_t));
         }
      }

      if (presample > 0)
      {
         int16_t *base_filter   = NULL;

         filter->filter_pos[i] += presample;
         base_filter            = filter->filter + i * filter->filter_stride;

         if (presample > (int)filter->filter_len)
            memset(base_filter, 0, filter->filter_len * sizeof(int16_t));
         else
         {
            memmove(base_filter, base_filter + presample,
                  (filter->filter_len - presample) * sizeof(int16_t));
            memset(base_filter + (filter->filter_len - presample),
                  0, presample * sizeof(int16_t));
         }
      }
   }
}

bool scaler_gen_filter(struct scaler_ctx *ctx)
{
   int x_pos, x_step, y_pos, y_step;
   int sinc_size = 0;

   switch (ctx->scaler_type)
   {
      case SCALER_TYPE_POINT:
         ctx->horiz.filter_len    = 1;
         ctx->horiz.filter_stride = 1;
         ctx->vert.filter_len     = 1;
         ctx->vert.filter_stride  = 1;
         break;
      case SCALER_TYPE_BILINEAR:
         ctx->horiz.filter_len    = 2;
         ctx->horiz.filter_stride = 2;
         ctx->vert.filter_len     = 2;
         ctx->vert.filter_stride  = 2;
         break;
      case SCALER_TYPE_SINC:
         sinc_size                = 8 * ((ctx->in_width > ctx->out_width)
               ? next_pow2(ctx->in_width / ctx->out_width) : 1);
         ctx->horiz.filter_len    = sinc_size;
         ctx->horiz.filter_stride = sinc_size;
         ctx->vert.filter_len     = sinc_size;
         ctx->vert.filter_stride  = sinc_size;
         break;
      case SCALER_TYPE_UNKNOWN:
      default:
         return false;
   }

   ctx->horiz.filter     = (int16_t*)calloc(ctx->horiz.filter_stride * ctx->out_width, sizeof(int16_t));
   ctx->horiz.filter_pos = (int*)calloc(ctx->out_width, sizeof(int));

   ctx->vert.filter      = (int16_t*)calloc(ctx->vert.filter_stride * ctx->out_height, sizeof(int16_t));
   ctx->vert.filter_pos  = (int*)calloc(ctx->out_height, sizeof(int));

   if (!ctx->horiz.filter || !ctx->vert.filter)
      return false;

   x_step = (1 << 16) * ctx->in_width  / ctx->out_width;
   y_step = (1 << 16) * ctx->in_height / ctx->out_height;
   x_pos  = (1 << 15) * ctx->in_width  / ctx->out_width  - (1 << 15);
   y_pos  = (1 << 15) * ctx->in_height / ctx->out_height - (1 << 15);

   switch (ctx->scaler_type)
   {
      case SCALER_TYPE_POINT:
         gen_filter_point_sub(&ctx->horiz, ctx->out_width,  x_pos, x_step);
         gen_filter_point_sub(&ctx->vert,  ctx->out_height, y_pos, y_step);

         ctx->scaler_special = scaler_argb8888_point_special;
         break;

      case SCALER_TYPE_BILINEAR:
         gen_filter_bilinear_sub(&ctx->horiz, ctx->out_width,  x_pos, x_step);
         gen_filter_bilinear_sub(&ctx->vert,  ctx->out_height, y_pos, y_step);
         break;

      case SCALER_TYPE_SINC:
         /* Need to expand the filter when downsampling
          * to get a proper low-pass effect. */
         x_pos  -= (sinc_size << 15);
         y_pos  -= (sinc_size << 15);

         gen_filter_sinc_sub(&ctx->horiz, ctx->out_width, x_pos, x_step,
               ctx->in_width  > ctx->out_width  ? (double)ctx->out_width  / ctx->in_width  : 1.0);
         gen_filter_sinc_sub(&ctx->vert, ctx->out_height, y_pos, y_step,
               ctx->in_height > ctx->out_height ? (double)ctx->out_height / ctx->in_height : 1.0
               );
         break;
      case SCALER_TYPE_UNKNOWN:
         break;
   }

   /* Makes sure that we never sample outside our rectangle */
   fixup_filter_sub(&ctx->horiz, ctx->out_width,  ctx->in_width);
   fixup_filter_sub(&ctx->vert,  ctx->out_height, ctx->in_height);

   /* ...and that what remains still sums to unity, after both the
    * truncation above and the one in the generators. */
   normalize_filter_sub(&ctx->horiz, ctx->out_width);
   normalize_filter_sub(&ctx->vert,  ctx->out_height);

   return validate_filter(ctx);
}
