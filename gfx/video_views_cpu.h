/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* What the drivers that draw views on the CPU share: a rectangle of the
 * frame scaled straight into a rectangle of an XRGB8888 surface, by a
 * scaler kept while its sizes stay, and one the surface cuts scaled
 * aside with the part shown copied. Included by each such driver. */

#ifndef __VIDEO_VIEWS_CPU_H
#define __VIDEO_VIEWS_CPU_H

#include <stdlib.h>
#include <string.h>

#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>

#include "video_views.h"
#include "video_defines.h"

typedef struct video_views_cpu
{
   struct scaler_ctx scalers[VIDEO_VIEWS_MAX_PLACEMENTS];
   /* A placement the surface cuts, scaled whole */
   uint32_t *scratch;
   size_t scratch_size;
   unsigned next;
} video_views_cpu_t;

static INLINE void video_views_cpu_free(video_views_cpu_t *cpu)
{
   unsigned i;
   for (i = 0; i < VIDEO_VIEWS_MAX_PLACEMENTS; i++)
      scaler_ctx_gen_reset(&cpu->scalers[i]);
   free(cpu->scratch);
   cpu->scratch      = NULL;
   cpu->scratch_size = 0;
}

/* A scaler from @in_w x @in_h to @out_w x @out_h through @type: one
 * kept from an earlier frame, or the oldest made again */
static INLINE struct scaler_ctx *video_views_cpu_scaler(
      video_views_cpu_t *cpu, enum scaler_pix_fmt fmt,
      enum scaler_type type, int in_w, int in_h, int in_stride,
      int out_w, int out_h, int out_stride)
{
   unsigned i;
   struct scaler_ctx *ctx;
   for (i = 0; i < VIDEO_VIEWS_MAX_PLACEMENTS; i++)
   {
      ctx = &cpu->scalers[i];
      if (     ctx->in_width    == in_w   && ctx->in_height  == in_h
            && ctx->in_stride   == in_stride
            && ctx->out_width   == out_w  && ctx->out_height == out_h
            && ctx->out_stride  == out_stride
            && ctx->in_fmt      == fmt
            && ctx->scaler_type == type)
         return ctx;
   }
   ctx              = &cpu->scalers[cpu->next];
   cpu->next        = (cpu->next + 1) % VIDEO_VIEWS_MAX_PLACEMENTS;
   scaler_ctx_gen_reset(ctx);
   ctx->in_fmt      = fmt;
   ctx->out_fmt     = SCALER_FMT_ARGB8888;
   ctx->scaler_type = type;
   ctx->in_width    = in_w;
   ctx->in_height   = in_h;
   ctx->in_stride   = in_stride;
   ctx->out_width   = out_w;
   ctx->out_height  = out_h;
   ctx->out_stride  = out_stride;
   if (!scaler_ctx_gen_filter(ctx))
   {
      ctx->in_width = 0;
      return NULL;
   }
   return ctx;
}

/* The @in_w x @in_h rectangle at @in (of @fmt, rows @in_stride bytes
 * apart) scaled over @dst of the surface at @pixels (rows @stride bytes
 * apart), drawing only the part in @cut */
static INLINE void video_views_cpu_draw(video_views_cpu_t *cpu,
      enum scaler_pix_fmt fmt, enum scaler_type type,
      const void *in, unsigned in_w, unsigned in_h, unsigned in_stride,
      uint8_t *pixels, unsigned stride,
      const video_views_rect_t *dst, const video_views_rect_t *cut)
{
   unsigned y;
   struct scaler_ctx *ctx;
   int dw      = (int)VIDEO_SCALE_W(dst->dims);
   int dh      = (int)VIDEO_SCALE_H(dst->dims);
   int cx      = VIDEO_POS_X(cut->pos);
   int cy      = VIDEO_POS_Y(cut->pos);
   unsigned cw = VIDEO_SCALE_W(cut->dims);
   unsigned ch = VIDEO_SCALE_H(cut->dims);
   size_t size = (size_t)dw * dh * sizeof(uint32_t);

   /* Whole: straight into the surface */
   if (cut->pos == dst->pos && cut->dims == dst->dims)
   {
      if ((ctx = video_views_cpu_scaler(cpu, fmt, type, (int)in_w,
               (int)in_h, (int)in_stride, dw, dh, (int)stride)))
         scaler_ctx_scale_direct(ctx, pixels + (size_t)cy * stride
               + (size_t)cx * sizeof(uint32_t), in);
      return;
   }
   /* Cut: scaled whole aside, the part shown copied */
   if (size > cpu->scratch_size)
   {
      uint32_t *grown = (uint32_t*)realloc(cpu->scratch, size);
      if (!grown)
         return;
      cpu->scratch      = grown;
      cpu->scratch_size = size;
   }
   if (!(ctx = video_views_cpu_scaler(cpu, fmt, type, (int)in_w,
            (int)in_h, (int)in_stride, dw, dh,
            dw * (int)sizeof(uint32_t))))
      return;
   scaler_ctx_scale_direct(ctx, cpu->scratch, in);
   for (y = 0; y < ch; y++)
      memcpy(pixels + (size_t)(cy + y) * stride
            + (size_t)cx * sizeof(uint32_t),
            cpu->scratch + (size_t)(cy - VIDEO_POS_Y(dst->pos) + y) * dw
            + (cx - VIDEO_POS_X(dst->pos)),
            cw * sizeof(uint32_t));
}

/* View @v of the frame at @frame (rows @pitch bytes apart, of @fmt)
 * over @dst, cut to @cut */
static INLINE void video_views_cpu_draw_view(video_views_cpu_t *cpu,
      enum scaler_pix_fmt fmt, enum scaler_type type,
      const struct retro_video_view *v, const void *frame, unsigned pitch,
      uint8_t *pixels, unsigned stride,
      const video_views_rect_t *dst, const video_views_rect_t *cut)
{
   unsigned bpp = (fmt == SCALER_FMT_RGB565) ? 2 : 4;
   video_views_cpu_draw(cpu, fmt, type,
         (const uint8_t*)frame + (size_t)v->y * pitch + (size_t)v->x * bpp,
         v->width, v->height, pitch, pixels, stride, dst, cut);
}

#endif
