/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include "filter.h"
#include "scaler_int.h"
#include "../../general.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool allocate_filters(struct scaler_ctx *ctx)
{
   ctx->horiz.filter     = (int16_t*)scaler_alloc(sizeof(int16_t), ctx->horiz.filter_stride * ctx->out_width);
   ctx->horiz.filter_pos = (int*)scaler_alloc(sizeof(int), ctx->out_width);

   ctx->vert.filter      = (int16_t*)scaler_alloc(sizeof(int16_t), ctx->vert.filter_stride * ctx->out_height);
   ctx->vert.filter_pos  = (int*)scaler_alloc(sizeof(int), ctx->out_height);

   return ctx->horiz.filter && ctx->vert.filter;
}

static void gen_filter_point_sub(struct scaler_filter *filter, int len, int pos, int step)
{
   for (int i = 0; i < len; i++, pos += step)
   {
      filter->filter_pos[i] = pos >> 16;
      filter->filter[i]     = FILTER_UNITY;
   }
}

static bool gen_filter_point(struct scaler_ctx *ctx)
{
   ctx->horiz.filter_len    = 1;
   ctx->horiz.filter_stride = 1;
   ctx->vert.filter_len     = 1;
   ctx->vert.filter_stride  = 1;

   if (!allocate_filters(ctx))
      return false;

   int x_pos  = (1 << 15) * ctx->in_width / ctx->out_width - (1 << 15);
   int x_step = (1 << 16) * ctx->in_width / ctx->out_width;
   int y_pos  = (1 << 15) * ctx->in_height / ctx->out_height - (1 << 15);
   int y_step = (1 << 16) * ctx->in_height / ctx->out_height;

   gen_filter_point_sub(&ctx->horiz, ctx->out_width, x_pos, x_step);
   gen_filter_point_sub(&ctx->vert, ctx->out_height, y_pos, y_step);

   ctx->scaler_special = scaler_argb8888_point_special;

   return true;
}

static void gen_filter_bilinear_sub(struct scaler_filter *filter, int len, int pos, int step)
{
   for (int i = 0; i < len; i++, pos += step)
   {
      filter->filter_pos[i]     = pos >> 16;
      filter->filter[i * 2 + 1] = (pos & 0xffff) >> 2;
      filter->filter[i * 2 + 0] = FILTER_UNITY - filter->filter[i * 2 + 1];
   }
}

static bool gen_filter_bilinear(struct scaler_ctx *ctx)
{
   ctx->horiz.filter_len    = 2;
   ctx->horiz.filter_stride = 2;
   ctx->vert.filter_len     = 2;
   ctx->vert.filter_stride  = 2;

   if (!allocate_filters(ctx))
      return false;

   int x_pos  = (1 << 15) * ctx->in_width / ctx->out_width - (1 << 15);
   int x_step = (1 << 16) * ctx->in_width / ctx->out_width;
   int y_pos  = (1 << 15) * ctx->in_height / ctx->out_height - (1 << 15);
   int y_step = (1 << 16) * ctx->in_height / ctx->out_height;

   gen_filter_bilinear_sub(&ctx->horiz, ctx->out_width, x_pos, x_step);
   gen_filter_bilinear_sub(&ctx->vert, ctx->out_height, y_pos, y_step);

   return true;
}

static inline double filter_sinc(double phase)
{
   if (fabs(phase) < 0.0001)
      return 1.0;
   else
      return sin(phase) / phase;
}

static void gen_filter_sinc_sub(struct scaler_filter *filter, int len, int pos, int step, double phase_mul)
{
   const int sinc_size = filter->filter_len;

   for (int i = 0; i < len; i++, pos += step)
   {
      filter->filter_pos[i] = pos >> 16;

      //int16_t sinc_sum = 0;
      for (int j = 0; j < sinc_size; j++)
      {
         double sinc_phase    = M_PI * ((double)((sinc_size << 15) + (pos & 0xffff)) / 0x10000 - j);
         double lanczos_phase = sinc_phase / ((sinc_size >> 1));
         int16_t sinc_val     = FILTER_UNITY * filter_sinc(sinc_phase * phase_mul) * filter_sinc(lanczos_phase) * phase_mul;
         //sinc_sum += sinc_val;

         filter->filter[i * sinc_size + j] = sinc_val;
      }
      //fprintf(stderr, "Sinc sum = %.3lf\n", (double)sinc_sum / FILTER_UNITY);
   }
}

static bool gen_filter_sinc(struct scaler_ctx *ctx)
{
   // Need to expand the filter when downsampling to get a proper low-pass effect.
   const int sinc_size      = 8 * (ctx->in_width > ctx->out_width ? next_pow2(ctx->in_width / ctx->out_width) : 1);
   ctx->horiz.filter_len    = sinc_size;
   ctx->horiz.filter_stride = sinc_size;
   ctx->vert.filter_len     = sinc_size;
   ctx->vert.filter_stride  = sinc_size;

   if (!allocate_filters(ctx))
      return false;

   int x_pos  = (1 << 15) * ctx->in_width / ctx->out_width - (1 << 15) - (sinc_size << 15);
   int x_step = (1 << 16) * ctx->in_width / ctx->out_width;
   int y_pos  = (1 << 15) * ctx->in_height / ctx->out_height - (1 << 15) - (sinc_size << 15);
   int y_step = (1 << 16) * ctx->in_height / ctx->out_height;

   double phase_mul_horiz = ctx->in_width  > ctx->out_width  ? (double)ctx->out_width  / ctx->in_width  : 1.0;
   double phase_mul_vert  = ctx->in_height > ctx->out_height ? (double)ctx->out_height / ctx->in_height : 1.0;

   gen_filter_sinc_sub(&ctx->horiz, ctx->out_width, x_pos, x_step, phase_mul_horiz);
   gen_filter_sinc_sub(&ctx->vert, ctx->out_height, y_pos, y_step, phase_mul_vert);

   return true;
}


static bool validate_filter(struct scaler_ctx *ctx)
{
   int max_w_pos = ctx->in_width - ctx->horiz.filter_len;
   for (int i = 0; i < ctx->out_width; i++)
   {
      if (ctx->horiz.filter_pos[i] > max_w_pos || ctx->horiz.filter_pos[i] < 0)
      {
         fprintf(stderr, "Out X = %d => In X = %d\n", i, ctx->horiz.filter_pos[i]); 
         return false;
      }
   }

   int max_h_pos = ctx->in_height - ctx->vert.filter_len;
   for (int i = 0; i < ctx->out_height; i++)
   {
      if (ctx->vert.filter_pos[i] > max_h_pos || ctx->vert.filter_pos[i] < 0)
      {
         fprintf(stderr, "Out Y = %d => In Y = %d\n", i, ctx->vert.filter_pos[i]); 
         return false;
      }
   }

   return true;
}

static void fixup_filter_sub(struct scaler_filter *filter, int out_len, int in_len)
{
   int max_pos = in_len - filter->filter_len;

   for (int i = 0; i < out_len; i++)
   {
      int postsample = filter->filter_pos[i] - max_pos;
      int presample  = -filter->filter_pos[i];

      if (postsample > 0)
      {
         filter->filter_pos[i] -= postsample;

         int16_t *base_filter = filter->filter + i * filter->filter_stride;

         if (postsample > (int)filter->filter_len)
            memset(base_filter, 0, filter->filter_len * sizeof(int16_t));
         else
         {
            memmove(base_filter + postsample, base_filter, (filter->filter_len - postsample) * sizeof(int16_t));
            memset(base_filter, 0, postsample * sizeof(int16_t));
         }
      }

      if (presample > 0)
      {
         filter->filter_pos[i] += presample;
         int16_t *base_filter = filter->filter + i * filter->filter_stride;

         if (presample > (int)filter->filter_len)
            memset(base_filter, 0, filter->filter_len * sizeof(int16_t));
         else
         {
            memmove(base_filter, base_filter + presample, (filter->filter_len - presample) * sizeof(int16_t));
            memset(base_filter + (filter->filter_len - presample), 0, presample * sizeof(int16_t));
         }
      }
   }
}

// Makes sure that we never sample outside our rectangle.
static void fixup_filter(struct scaler_ctx *ctx)
{
   fixup_filter_sub(&ctx->horiz, ctx->out_width, ctx->in_width);
   fixup_filter_sub(&ctx->vert, ctx->out_height, ctx->in_height);
}


bool scaler_gen_filter(struct scaler_ctx *ctx)
{
   bool ret = true;

   switch (ctx->scaler_type)
   {
      case SCALER_TYPE_POINT:
         ret = gen_filter_point(ctx);
         break;

      case SCALER_TYPE_BILINEAR:
         ret = gen_filter_bilinear(ctx);
         break;

      case SCALER_TYPE_SINC:
         ret = gen_filter_sinc(ctx);
         break;

      default:
         return false;
   }

   if (!ret)
      return false;

   fixup_filter(ctx);

   return validate_filter(ctx);
}

