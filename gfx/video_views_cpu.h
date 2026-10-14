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

/* The scale a driver whose display hardware scales a surface to the
 * screen composes the views at: the most any placement shrinks its view
 * by, so that no view is drawn larger than it is and the hardware does
 * the enlarging; at most 1, and 1 for interlaced, whose rows are the
 * screen's. */
static INLINE float video_views_cpu_scale(const video_views_map_t *map,
      const video_views_layout_t *layout)
{
   unsigned i;
   float scale = 0.0f;
   if (layout->stereo_mode == VIDEO_STEREO_MODE_INTERLACED)
      return 1.0f;
   for (i = 0; i < layout->num_placements; i++)
   {
      const video_views_placement_t *p  = &layout->placements[i];
      const struct retro_video_view *v = &map->views[p->view];
      float sw = VIDEO_SCALE_W(p->dst.dims)
         ? (float)v->width  / VIDEO_SCALE_W(p->dst.dims) : 1.0f;
      float sh = VIDEO_SCALE_H(p->dst.dims)
         ? (float)v->height / VIDEO_SCALE_H(p->dst.dims) : 1.0f;
      if (sw > scale)
         scale = sw;
      if (sh > scale)
         scale = sh;
   }
   return (scale <= 0.0f || scale > 1.0f) ? 1.0f : scale;
}

/* @dims scaled by @scale, at least 1x1 */
static INLINE unsigned video_views_cpu_dims(unsigned dims, float scale)
{
   unsigned w = (unsigned)(VIDEO_SCALE_W(dims) * scale + 0.5f);
   unsigned h = (unsigned)(VIDEO_SCALE_H(dims) * scale + 0.5f);
   return VIDEO_SCALE_PACK(w ? w : 1, h ? h : 1);
}

/* @r scaled by @scale */
static INLINE void video_views_cpu_rect(const video_views_rect_t *r,
      float scale, video_views_rect_t *out)
{
   int x0    = (int)((float)VIDEO_POS_X(r->pos) * scale + 0.5f);
   int y0    = (int)((float)VIDEO_POS_Y(r->pos) * scale + 0.5f);
   int x1    = (int)((float)(VIDEO_POS_X(r->pos)
            + (int)VIDEO_SCALE_W(r->dims)) * scale + 0.5f);
   int y1    = (int)((float)(VIDEO_POS_Y(r->pos)
            + (int)VIDEO_SCALE_H(r->dims)) * scale + 0.5f);
   out->pos  = VIDEO_POS_PACK(x0, y0);
   out->dims = VIDEO_SCALE_PACK(x1 - x0, y1 - y0);
}

/* Placement @p of @layout at @scale, in a target of the layout's size
 * at that scale, @dims, or with @offscreen the canvas of both eyes side
 * by side: the whole of it in @dst, and in @cut the part drawn, inside
 * its own eye's area and the target. False when nothing is. */
static INLINE bool video_views_cpu_cut(const video_views_layout_t *layout,
      const video_views_placement_t *p, float scale, unsigned dims,
      bool offscreen, video_views_rect_t *dst, video_views_rect_t *cut)
{
   video_views_rect_t area;
   int x0, y0, x1, y1, ax, ay;
   unsigned tdims = offscreen
      ? VIDEO_SCALE_PACK(2 * VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims))
      : dims;
   video_views_cpu_rect(&p->dst, scale, dst);
   if (offscreen)
   {
      area.pos  = VIDEO_POS_PACK(p->area ? VIDEO_SCALE_W(dims) : 0, 0);
      area.dims = dims;
   }
   else if (layout->num_areas)
      video_views_cpu_rect(&layout->areas[p->area], scale, &area);
   else
   {
      area.pos  = VIDEO_POS_PACK(0, 0);
      area.dims = dims;
   }
   if (!video_views_clip(dst, tdims, cut))
      return false;
   x0 = VIDEO_POS_X(cut->pos);
   y0 = VIDEO_POS_Y(cut->pos);
   x1 = x0 + (int)VIDEO_SCALE_W(cut->dims);
   y1 = y0 + (int)VIDEO_SCALE_H(cut->dims);
   ax = VIDEO_POS_X(area.pos);
   ay = VIDEO_POS_Y(area.pos);
   if (x0 < ax)
      x0 = ax;
   if (y0 < ay)
      y0 = ay;
   if (x1 > ax + (int)VIDEO_SCALE_W(area.dims))
      x1 = ax + (int)VIDEO_SCALE_W(area.dims);
   if (y1 > ay + (int)VIDEO_SCALE_H(area.dims))
      y1 = ay + (int)VIDEO_SCALE_H(area.dims);
   if (     x1 <= x0 || y1 <= y0
         || !VIDEO_SCALE_W(dst->dims) || !VIDEO_SCALE_H(dst->dims))
      return false;
   cut->pos  = VIDEO_POS_PACK(x0, y0);
   cut->dims = VIDEO_SCALE_PACK(x1 - x0, y1 - y0);
   return true;
}

/* @layout's placements drawn at @scale into the XRGB8888 @target, rows
 * @tstride pixels apart: the layout's size at that scale, @dims, or with
 * @offscreen the canvas of both eyes side by side, unblended. Cleared
 * first. Each view straight from the frame. */
static INLINE void video_views_cpu_draw_placements(video_views_cpu_t *cpu,
      const video_views_map_t *map, const video_views_layout_t *layout,
      const void *frame, unsigned pitch, enum scaler_pix_fmt fmt,
      enum scaler_type type, float scale, unsigned dims, bool offscreen,
      uint32_t *target, unsigned tstride)
{
   unsigned i, y;
   unsigned tw = offscreen ? 2 * VIDEO_SCALE_W(dims) : VIDEO_SCALE_W(dims);

   for (y = 0; y < VIDEO_SCALE_H(dims); y++)
      memset(target + (size_t)y * tstride, 0, tw * sizeof(uint32_t));
   for (i = 0; i < layout->num_placements; i++)
   {
      video_views_rect_t dst, cut;
      const video_views_placement_t *p = &layout->placements[i];
      if (video_views_cpu_cut(layout, p, scale, dims, offscreen,
               &dst, &cut))
         video_views_cpu_draw_view(cpu, fmt, type, &map->views[p->view],
               frame, pitch, (uint8_t*)target, tstride * sizeof(uint32_t),
               &dst, &cut);
   }
}

/* @layout's views composed at @scale into the XRGB8888 surface at
 * @pixels, rows @stride pixels apart, of the layout's size at that
 * scale (video_views_cpu_dims). Anaglyph and interlaced through
 * @canvas, the canvas's size at that scale, whose halves are blended in.
 * Each view straight from the frame. */
static INLINE void video_views_cpu_compose(video_views_cpu_t *cpu,
      const video_views_map_t *map, const video_views_layout_t *layout,
      const void *frame, unsigned pitch, enum scaler_pix_fmt fmt,
      enum scaler_type type, float scale, unsigned dims,
      uint32_t *pixels, unsigned stride, uint32_t *canvas)
{
   bool offscreen = layout->offscreen && canvas;
   if (!offscreen)
   {
      video_views_cpu_draw_placements(cpu, map, layout, frame, pitch, fmt,
            type, scale, dims, false, pixels, stride);
      return;
   }
   video_views_cpu_draw_placements(cpu, map, layout, frame, pitch, fmt,
         type, scale, dims, true, canvas, 2 * VIDEO_SCALE_W(dims));
   video_views_blend_xrgb8888(canvas, 2 * VIDEO_SCALE_W(dims), pixels,
         stride, dims, layout->stereo_mode);
}

#endif
