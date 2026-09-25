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

#include <string.h>
#include <limits.h>
#include <math.h>

#include "video_views.h"
#include "video_defines.h"

bool video_views_validate(const struct retro_video_view *views,
      unsigned num_views, video_views_map_t *out)
{
   unsigned i;
   unsigned num_screens = 0;
   unsigned count_none[RETRO_VIDEO_VIEWS_MAX];
   unsigned count_left[RETRO_VIDEO_VIEWS_MAX];
   unsigned count_right[RETRO_VIDEO_VIEWS_MAX];
   int left[RETRO_VIDEO_VIEWS_MAX];
   int right[RETRO_VIDEO_VIEWS_MAX];

   if (num_views == 0)
   {
      if (out)
         memset(out, 0, sizeof(*out));
      return true;
   }
   if (!views || num_views > RETRO_VIDEO_VIEWS_MAX)
      return false;

   memset(count_none,  0, sizeof(count_none));
   memset(count_left,  0, sizeof(count_left));
   memset(count_right, 0, sizeof(count_right));

   for (i = 0; i < num_views; i++)
   {
      const struct retro_video_view *v = &views[i];
      if (!v->width || !v->height)
         return false;
      if (v->x > UINT_MAX - v->width || v->y > UINT_MAX - v->height)
         return false;
      if (v->screen >= RETRO_VIDEO_VIEWS_MAX)
         return false;
      /* Rejects infinities too; NaN falls through to width / height. */
      if (     v->aspect_ratio > 0.0f
            && (v->aspect_ratio < 0.01f || v->aspect_ratio > 100.0f))
         return false;
      switch (v->eye)
      {
         case RETRO_VIDEO_VIEW_EYE_NONE:
            count_none[v->screen]++;
            break;
         case RETRO_VIDEO_VIEW_EYE_LEFT:
            count_left[v->screen]++;
            left[v->screen]  = (int)i;
            break;
         case RETRO_VIDEO_VIEW_EYE_RIGHT:
            count_right[v->screen]++;
            right[v->screen] = (int)i;
            break;
         default:
            return false;
      }
      if (v->screen + 1 > num_screens)
         num_screens = v->screen + 1;
   }

   for (i = 0; i < num_screens; i++)
   {
      if (count_none[i] == 1 && !count_left[i] && !count_right[i])
         continue;
      if (     !count_none[i] && count_left[i] == 1 && count_right[i] == 1
            && views[left[i]].width  == views[right[i]].width
            && views[left[i]].height == views[right[i]].height)
         continue;
      return false;
   }

   if (out)
   {
      memset(out, 0, sizeof(*out));
      memcpy(out->views, views, num_views * sizeof(*views));
      out->num_views   = num_views;
      out->num_screens = num_screens;
   }
   return true;
}

bool video_views_fit_frame(const video_views_map_t *map,
      unsigned frame_dims)
{
   unsigned i;
   unsigned frame_width  = VIDEO_SCALE_W(frame_dims);
   unsigned frame_height = VIDEO_SCALE_H(frame_dims);
   if (!map || !map->num_views)
      return false;
   for (i = 0; i < map->num_views; i++)
   {
      const struct retro_video_view *v = &map->views[i];
      if (     v->x + v->width  > frame_width
            || v->y + v->height > frame_height)
         return false;
   }
   return true;
}

void video_views_scale(video_views_map_t *map,
      unsigned in_dims, unsigned out_dims)
{
   unsigned i;
   unsigned in_width   = VIDEO_SCALE_W(in_dims);
   unsigned in_height  = VIDEO_SCALE_H(in_dims);
   unsigned out_width  = VIDEO_SCALE_W(out_dims);
   unsigned out_height = VIDEO_SCALE_H(out_dims);
   if (!map || !in_width || !in_height || in_dims == out_dims)
      return;
   for (i = 0; i < map->num_views; i++)
   {
      struct retro_video_view *v = &map->views[i];
      v->aspect_ratio = (float)video_views_aspect(v);
      v->x      = (unsigned)(((double)v->x      * out_width)  / in_width);
      v->width  = (unsigned)(((double)v->width  * out_width)  / in_width);
      v->y      = (unsigned)(((double)v->y      * out_height) / in_height);
      v->height = (unsigned)(((double)v->height * out_height) / in_height);
      if (!v->width)
         v->width  = 1;
      if (!v->height)
         v->height = 1;
   }
}

double video_views_aspect(const struct retro_video_view *v)
{
   /* NaN falls through to width / height, as validate expects. */
   if (v->aspect_ratio > 0.0f)
      return v->aspect_ratio;
   return (double)v->width / v->height;
}

int video_views_find(const video_views_map_t *map,
      unsigned screen, unsigned eye)
{
   unsigned i;
   int left = -1;
   for (i = 0; i < map->num_views; i++)
   {
      const struct retro_video_view *v = &map->views[i];
      if (v->screen != screen)
         continue;
      if (v->eye == RETRO_VIDEO_VIEW_EYE_NONE || v->eye == eye)
         return (int)i;
      if (v->eye == RETRO_VIDEO_VIEW_EYE_LEFT)
         left = (int)i;
   }
   return (eye == RETRO_VIDEO_VIEW_EYE_NONE) ? left : -1;
}

bool video_views_snapshot(const video_views_map_t *map,
      unsigned frame_dims, unsigned out_dims,
      video_views_map_t *out)
{
   if (!video_views_fit_frame(map, frame_dims))
   {
      memset(out, 0, sizeof(*out));
      return false;
   }
   *out = *map;
   video_views_scale(out, frame_dims, out_dims);
   return true;
}

static int video_views_round(double v)
{
   return (int)floor(v + 0.5);
}

/* Screen rectangles within an area of size cdims, in screen order. */
static void video_views_fit(const video_views_layout_params_t *p,
      unsigned cdims, video_views_rect_t *fit)
{
   unsigned s;
   double disp_w[RETRO_VIDEO_VIEWS_MAX];
   double disp_h[RETRO_VIDEO_VIEWS_MAX];
   double tw = 0.0, th = 0.0, scale, ox, oy, pos = 0.0;
   const video_views_map_t *map = p->map;
   unsigned cw   = VIDEO_SCALE_W(cdims);
   unsigned ch   = VIDEO_SCALE_H(cdims);
   bool vertical = (p->screen_layout != VIDEO_SCREEN_LAYOUT_HORIZONTAL);

   if (map->num_screens == 1 && p->stretch && !p->scale_integer)
   {
      fit[0].pos  = VIDEO_POS_PACK(0, 0);
      fit[0].dims = cdims;
      return;
   }

   if (map->num_screens == 1 && p->custom_vp && cdims == p->dims)
   {
      fit[0] = *p->custom_vp;
      return;
   }

   for (s = 0; s < map->num_screens; s++)
   {
      const struct retro_video_view *v = &map->views[
         video_views_find(map, s, RETRO_VIDEO_VIEW_EYE_NONE)];
      double w      = v->width;
      double h      = v->height;
      double aspect = video_views_aspect(v);
      if (map->num_screens == 1 && p->single_aspect > 0.0f)
         aspect = p->single_aspect;
      if (p->rotation & 1)
      {
         h      = w;
         aspect = 1.0 / aspect;
      }
      disp_h[s] = h;
      disp_w[s] = h * aspect;
      if (vertical)
      {
         if (disp_w[s] > tw)
            tw = disp_w[s];
         th += disp_h[s];
      }
      else
      {
         tw += disp_w[s];
         if (disp_h[s] > th)
            th = disp_h[s];
      }
   }

   scale = (double)cw / tw;
   if ((double)ch / th < scale)
      scale = (double)ch / th;
   if (p->scale_integer && scale >= 1.0)
      scale = floor(scale);
   ox = ((double)cw - tw * scale) / 2.0;
   oy = ((double)ch - th * scale) / 2.0;

   for (s = 0; s < map->num_screens; s++)
   {
      double x, y;
      int x0, y0;
      double w = disp_w[s] * scale;
      double h = disp_h[s] * scale;
      if (vertical)
      {
         x    = ox + (tw - disp_w[s]) * scale / 2.0;
         y    = oy + pos;
         pos += h;
      }
      else
      {
         x    = ox + pos;
         y    = oy + (th - disp_h[s]) * scale / 2.0;
         pos += w;
      }
      x0          = video_views_round(x);
      y0          = video_views_round(y);
      fit[s].pos  = VIDEO_POS_PACK(x0, y0);
      fit[s].dims = VIDEO_SCALE_PACK(video_views_round(x + w) - x0,
            video_views_round(y + h) - y0);
   }
}

/* Map r from an area of size cdims into area, scaling as needed. */
static void video_views_place(const video_views_rect_t *r,
      unsigned cdims, const video_views_rect_t *area,
      video_views_rect_t *out)
{
   int rx    = VIDEO_POS_X(r->pos);
   int ry    = VIDEO_POS_Y(r->pos);
   double sx = (double)VIDEO_SCALE_W(area->dims) / VIDEO_SCALE_W(cdims);
   double sy = (double)VIDEO_SCALE_H(area->dims) / VIDEO_SCALE_H(cdims);
   int x0    = video_views_round(rx * sx);
   int y0    = video_views_round(ry * sy);
   int x1    = video_views_round((rx + (int)VIDEO_SCALE_W(r->dims)) * sx);
   int y1    = video_views_round((ry + (int)VIDEO_SCALE_H(r->dims)) * sy);
   out->pos  = VIDEO_POS_PACK(VIDEO_POS_X(area->pos) + x0,
         VIDEO_POS_Y(area->pos) + y0);
   out->dims = VIDEO_SCALE_PACK(x1 - x0, y1 - y0);
}

void video_views_layout(const video_views_layout_params_t *p,
      video_views_layout_t *out)
{
   unsigned a, s;
   unsigned w, h, cdims;
   video_views_rect_t fit[RETRO_VIDEO_VIEWS_MAX];
   const video_views_map_t *map = p->map;

   memset(out, 0, sizeof(*out));
   w = VIDEO_SCALE_W(p->dims);
   h = VIDEO_SCALE_H(p->dims);
   if (!map || !map->num_views || !w || !h)
      return;

   out->stereo_mode   = p->stereo_mode;
   cdims              = p->dims;
   out->num_areas     = 2;
   out->canvas_dims   = p->dims;
   out->ui_dims       = p->dims;

   switch (p->stereo_mode)
   {
      case VIDEO_STEREO_MODE_SBS_HALF:
      case VIDEO_STEREO_MODE_SBS_FULL:
         out->areas[0].dims = VIDEO_SCALE_PACK(w / 2, h);
         out->areas[1].pos  = VIDEO_POS_PACK(w / 2, 0);
         out->areas[1].dims = VIDEO_SCALE_PACK(w - w / 2, h);
         out->ui_per_eye    = true;
         if (p->stereo_mode == VIDEO_STEREO_MODE_SBS_FULL)
         {
            cdims        = out->areas[0].dims;
            out->ui_dims = cdims;
         }
         break;
      case VIDEO_STEREO_MODE_TOP_BOTTOM:
         out->areas[0].dims = VIDEO_SCALE_PACK(w, h / 2);
         out->areas[1].pos  = VIDEO_POS_PACK(0, h / 2);
         out->areas[1].dims = VIDEO_SCALE_PACK(w, h - h / 2);
         out->ui_per_eye    = true;
         break;
      case VIDEO_STEREO_MODE_ANAGLYPH:
      case VIDEO_STEREO_MODE_INTERLACED:
         out->areas[0].dims = p->dims;
         out->areas[1].pos  = VIDEO_POS_PACK(w, 0);
         out->areas[1].dims = p->dims;
         out->canvas_dims   = VIDEO_SCALE_PACK(w * 2, h);
         out->offscreen     = true;
         break;
      default:
         out->num_areas     = 1;
         out->areas[0].dims = p->dims;
         break;
   }

   video_views_fit(p, cdims, fit);

   for (a = 0; a < out->num_areas; a++)
   {
      unsigned eye = RETRO_VIDEO_VIEW_EYE_LEFT;
      if (out->num_areas == 2 && ((a == 1) != p->swap_eyes))
         eye = RETRO_VIDEO_VIEW_EYE_RIGHT;
      for (s = 0; s < map->num_screens; s++)
      {
         video_views_placement_t *pl = &out->placements[out->num_placements++];
         pl->view = (unsigned)video_views_find(map, s, eye);
         pl->area = a;
         video_views_place(&fit[s], cdims, &out->areas[a], &pl->dst);
      }
   }
}

unsigned video_views_ui_dims(const video_views_layout_t *layout,
      unsigned dims)
{
   if (layout && layout->ui_per_eye)
      return layout->ui_dims;
   return dims;
}

/* The part of r inside the w x h rectangle at (x, y). */
static bool video_views_cut(const video_views_rect_t *r,
      int x, int y, int w, int h, video_views_rect_t *out)
{
   int x0 = VIDEO_POS_X(r->pos);
   int y0 = VIDEO_POS_Y(r->pos);
   int x1 = x0 + (int)VIDEO_SCALE_W(r->dims);
   int y1 = y0 + (int)VIDEO_SCALE_H(r->dims);
   if (x0 < x)
      x0 = x;
   if (y0 < y)
      y0 = y;
   if (x1 > x + w)
      x1 = x + w;
   if (y1 > y + h)
      y1 = y + h;
   if (x1 <= x0 || y1 <= y0)
      return false;
   out->pos  = VIDEO_POS_PACK(x0, y0);
   out->dims = VIDEO_SCALE_PACK(x1 - x0, y1 - y0);
   return true;
}

bool video_views_clip(const video_views_rect_t *r,
      unsigned dims, video_views_rect_t *out)
{
   return video_views_cut(r, 0, 0, (int)VIDEO_SCALE_W(dims),
         (int)VIDEO_SCALE_H(dims), out);
}

bool video_views_visible(const video_views_layout_t *layout,
      const video_views_placement_t *p, unsigned dims,
      video_views_rect_t *out)
{
   video_views_rect_t r;
   const video_views_rect_t *a = &layout->areas[p->area];
   return video_views_cut(&p->dst, VIDEO_POS_X(a->pos), VIDEO_POS_Y(a->pos),
            (int)VIDEO_SCALE_W(a->dims), (int)VIDEO_SCALE_H(a->dims), &r)
      && video_views_clip(&r, dims, out);
}

bool video_views_first_drawn(const video_views_layout_t *layout,
      unsigned view, bool all_areas, unsigned dims,
      video_views_rect_t *out)
{
   unsigned i;
   for (i = 0; i < layout->num_placements; i++)
   {
      video_views_rect_t r;
      const video_views_placement_t *p = &layout->placements[i];
      if (     p->view == view
            && (all_areas || p->area == 0)
            && video_views_visible(layout, p, dims, &r))
      {
         *out = p->dst;
         return true;
      }
   }
   return false;
}

static int16_t video_views_norm(unsigned p, unsigned size)
{
   if (p == 0 || size < 2)
      return -0x7fff;
   if (p >= size - 1)
      return 0x7fff;
   return (int16_t)((int)(((unsigned long)p * 0xffffUL) / (size - 1)) - 0x8000);
}

/* Squared distance from a point to r, 0 inside it. */
static double video_views_rect_dist(const video_views_rect_t *r,
      int x, int y)
{
   int x0    = VIDEO_POS_X(r->pos);
   int y0    = VIDEO_POS_Y(r->pos);
   int w     = (int)VIDEO_SCALE_W(r->dims);
   int h     = (int)VIDEO_SCALE_H(r->dims);
   double dx = 0.0, dy = 0.0;
   if (x < x0)
      dx = x0 - x;
   else if (x >= x0 + w)
      dx = x - (x0 + w - 1);
   if (y < y0)
      dy = y0 - y;
   else if (y >= y0 + h)
      dy = y - (y0 + h - 1);
   return dx * dx + dy * dy;
}

bool video_views_map_point(const video_views_layout_t *layout,
      const video_views_map_t *map, unsigned frame_dims,
      int x, int y, bool report_oob, uint32_t *res_pos)
{
   unsigned i;
   int best      = -1;
   double best_d = 0.0;
   const video_views_placement_t *pl;
   const struct retro_video_view *ref;
   int cx, cy, r, dst_w, dst_h;
   int16_t px, py;

   for (i = 0; i < layout->num_placements; i++)
   {
      const video_views_rect_t *d = &layout->placements[i].dst;
      double dist;
      /* The offscreen canvas's right half never reaches the window. */
      if (layout->offscreen && layout->placements[i].area != 0)
         continue;
      if (!VIDEO_SCALE_W(d->dims) || !VIDEO_SCALE_H(d->dims))
         continue;
      dist = video_views_rect_dist(d, x, y);
      if (best < 0 || dist < best_d)
      {
         best   = (int)i;
         best_d = dist;
         if (dist == 0.0)
            break;
      }
   }
   if (best < 0 || !map->num_views)
      return false;
   if (best_d > 0.0 && report_oob)
   {
      *res_pos = VIDEO_POS_PACK(-0x8000, -0x8000);
      return true;
   }

   pl = &layout->placements[best];
   r  = video_views_find(map, map->views[pl->view].screen,
         RETRO_VIDEO_VIEW_EYE_NONE);
   if (r < 0)
      return false;
   ref = &map->views[r];

   dst_w = (int)VIDEO_SCALE_W(pl->dst.dims);
   dst_h = (int)VIDEO_SCALE_H(pl->dst.dims);
   cx    = x - VIDEO_POS_X(pl->dst.pos);
   cy    = y - VIDEO_POS_Y(pl->dst.pos);
   if (cx < 0)
      cx = 0;
   if (cx >= dst_w)
      cx = dst_w - 1;
   if (cy < 0)
      cy = 0;
   if (cy >= dst_h)
      cy = dst_h - 1;

   px       = video_views_norm(ref->x
         + (unsigned)cx * ref->width  / (unsigned)dst_w,
         VIDEO_SCALE_W(frame_dims));
   py       = video_views_norm(ref->y
         + (unsigned)cy * ref->height / (unsigned)dst_h,
         VIDEO_SCALE_H(frame_dims));
   *res_pos = VIDEO_POS_PACK(px, py);
   return true;
}

void video_views_ui_point(const video_views_layout_t *layout,
      int x, int y, int *res_x, int *res_y)
{
   unsigned i;
   const video_views_rect_t *best = NULL;
   double best_d                  = 0.0;

   *res_x = x;
   *res_y = y;
   if (!layout || !layout->ui_per_eye)
      return;

   for (i = 0; i < layout->num_areas; i++)
   {
      const video_views_rect_t *a = &layout->areas[i];
      double dist;
      if (!VIDEO_SCALE_W(a->dims) || !VIDEO_SCALE_H(a->dims))
         continue;
      dist = video_views_rect_dist(a, x, y);
      if (!best || dist < best_d)
      {
         best   = a;
         best_d = dist;
      }
   }
   if (!best)
      return;

   *res_x = (int)floor((double)(x - VIDEO_POS_X(best->pos))
         * VIDEO_SCALE_W(layout->ui_dims) / VIDEO_SCALE_W(best->dims));
   *res_y = (int)floor((double)(y - VIDEO_POS_Y(best->pos))
         * VIDEO_SCALE_H(layout->ui_dims) / VIDEO_SCALE_H(best->dims));
}
