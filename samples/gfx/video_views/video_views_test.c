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

/* Harness for gfx/video_views.c: map validation, frame helpers,
 * layout, and touch and UI pointer mapping. Exit 0 on pass, 1 on any
 * failure. */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "gfx/video_views.h"
#include "gfx/video_defines.h"
#include "gfx/video_views_cpu.h"

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
   printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

static struct retro_video_view mkview(unsigned x, unsigned y,
      unsigned w, unsigned h, unsigned screen, unsigned eye)
{
   struct retro_video_view v;
   v.x            = x;
   v.y            = y;
   v.width        = w;
   v.height       = h;
   v.screen       = screen;
   v.eye          = eye;
   v.aspect_ratio = 0.0f;
   return v;
}

/* 3DS packing: both top eyes side by side, bottom centred below.
 * Frame 800x480. */
static void map_3ds(video_views_map_t *map)
{
   struct retro_video_view v[3];
   v[0] = mkview(  0,   0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
   v[1] = mkview(400,   0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_RIGHT);
   v[2] = mkview(240, 240, 320, 240, 1, RETRO_VIDEO_VIEW_EYE_NONE);
   video_views_validate(v, 3, map);
}

/* DS packing: two 256x192 screens stacked. Frame 256x384. */
static void map_ds(video_views_map_t *map)
{
   struct retro_video_view v[2];
   v[0] = mkview(0,   0, 256, 192, 0, RETRO_VIDEO_VIEW_EYE_NONE);
   v[1] = mkview(0, 192, 256, 192, 1, RETRO_VIDEO_VIEW_EYE_NONE);
   video_views_validate(v, 2, map);
}

/* Virtual Boy packing: one screen, eyes side by side. Frame 768x224. */
static void map_vb(video_views_map_t *map)
{
   struct retro_video_view v[2];
   v[0] = mkview(  0, 0, 384, 224, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
   v[1] = mkview(384, 0, 384, 224, 0, RETRO_VIDEO_VIEW_EYE_RIGHT);
   video_views_validate(v, 2, map);
}

static void test_validate(void)
{
   video_views_map_t map;
   struct retro_video_view v[RETRO_VIDEO_VIEWS_MAX + 1];
   unsigned i;

   CHECK(video_views_validate(NULL, 0, &map));
   CHECK(map.num_views == 0);

   v[0] = mkview(0, 0, 256, 192, 0, RETRO_VIDEO_VIEW_EYE_NONE);
   CHECK(video_views_validate(v, 1, &map));
   CHECK(map.num_views == 1 && map.num_screens == 1);

   map_3ds(&map);
   CHECK(map.num_views == 3 && map.num_screens == 2);

   /* Zero size. */
   v[0] = mkview(0, 0, 0, 192, 0, RETRO_VIDEO_VIEW_EYE_NONE);
   CHECK(!video_views_validate(v, 1, &map));
   /* Unknown eye. */
   v[0] = mkview(0, 0, 256, 192, 0, 3);
   CHECK(!video_views_validate(v, 1, &map));
   /* Gap in screen numbers. */
   v[0] = mkview(0, 0, 256, 192, 0, RETRO_VIDEO_VIEW_EYE_NONE);
   v[1] = mkview(0, 0, 256, 192, 2, RETRO_VIDEO_VIEW_EYE_NONE);
   CHECK(!video_views_validate(v, 2, &map));
   /* A mono view and an eye on one screen. */
   v[1] = mkview(0, 0, 256, 192, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
   CHECK(!video_views_validate(v, 2, &map));
   /* A left eye alone. */
   v[0] = mkview(0, 0, 256, 192, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
   CHECK(!video_views_validate(v, 1, &map));
   /* Eyes of different sizes. */
   v[1] = mkview(256, 0, 255, 192, 0, RETRO_VIDEO_VIEW_EYE_RIGHT);
   CHECK(!video_views_validate(v, 2, &map));
   /* Too many views. */
   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX + 1; i++)
      v[i] = mkview(0, 0, 1, 1, i, RETRO_VIDEO_VIEW_EYE_NONE);
   CHECK(!video_views_validate(v, RETRO_VIDEO_VIEWS_MAX + 1, &map));
   CHECK(video_views_validate(v, RETRO_VIDEO_VIEWS_MAX, &map));
   /* Overflowing rectangle. */
   v[0] = mkview(UINT_MAX, 0, 2, 1, 0, RETRO_VIDEO_VIEW_EYE_NONE);
   CHECK(!video_views_validate(v, 1, &map));
   /* Absurd aspect ratio; zero is fine. */
   v[0] = mkview(0, 0, 256, 192, 0, RETRO_VIDEO_VIEW_EYE_NONE);
   v[0].aspect_ratio = 1000.0f;
   CHECK(!video_views_validate(v, 1, &map));
   v[0].aspect_ratio = 0.0f;
   CHECK(video_views_validate(v, 1, &map));
}

static void test_frame_helpers(void)
{
   video_views_map_t map, out;
   struct retro_video_view v[2];

   map_3ds(&map);
   CHECK(video_views_fit_frame(&map, VIDEO_SCALE_PACK(800, 480)));
   CHECK(!video_views_fit_frame(&map, VIDEO_SCALE_PACK(799, 480)));
   CHECK(!video_views_fit_frame(&map, VIDEO_SCALE_PACK(800, 479)));

   /* A 2x filter doubles every rectangle. */
   v[0] = mkview(100, 0, 100, 50, 0, RETRO_VIDEO_VIEW_EYE_NONE);
   video_views_validate(v, 1, &map);
   video_views_scale(&map, VIDEO_SCALE_PACK(200, 50),
         VIDEO_SCALE_PACK(400, 100));
   CHECK(map.views[0].x == 200 && map.views[0].width  == 200);
   CHECK(map.views[0].y == 0   && map.views[0].height == 100);

   /* An NTSC-style 512 -> 602 stretch keeps the eyes the same size. */
   v[0] = mkview(  0, 0, 256, 224, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
   v[1] = mkview(256, 0, 256, 224, 0, RETRO_VIDEO_VIEW_EYE_RIGHT);
   video_views_validate(v, 2, &map);
   video_views_scale(&map, VIDEO_SCALE_PACK(512, 224),
         VIDEO_SCALE_PACK(602, 224));
   CHECK(map.views[0].width == map.views[1].width);
   CHECK(map.views[0].width == 301 && map.views[1].x == 301);
   CHECK(video_views_fit_frame(&map, VIDEO_SCALE_PACK(602, 224)));
   /* ... and keeps each view's display aspect. */
   CHECK(fabs(video_views_aspect(&map.views[0]) - 256.0 / 224.0) < 1e-5);
   CHECK(fabs(video_views_aspect(&map.views[1]) - 256.0 / 224.0) < 1e-5);

   /* A view's own aspect ratio wins over its shape; zero or below doesn't. */
   v[0] = mkview(0, 0, 400, 200, 0, RETRO_VIDEO_VIEW_EYE_NONE);
   CHECK(video_views_aspect(&v[0]) == 2.0);
   v[0].aspect_ratio = 1.5f;
   CHECK(video_views_aspect(&v[0]) == 1.5);
   v[0].aspect_ratio = -1.0f;
   CHECK(video_views_aspect(&v[0]) == 2.0);

   map_3ds(&map);
   CHECK(video_views_find(&map, 0, RETRO_VIDEO_VIEW_EYE_LEFT)  == 0);
   CHECK(video_views_find(&map, 0, RETRO_VIDEO_VIEW_EYE_RIGHT) == 1);
   CHECK(video_views_find(&map, 0, RETRO_VIDEO_VIEW_EYE_NONE)  == 0);
   CHECK(video_views_find(&map, 1, RETRO_VIDEO_VIEW_EYE_LEFT)  == 2);
   CHECK(video_views_find(&map, 1, RETRO_VIDEO_VIEW_EYE_RIGHT) == 2);
   CHECK(video_views_find(&map, 2, RETRO_VIDEO_VIEW_EYE_LEFT)  == -1);

   /* Snapshot: copied and scaled when it fits, empty when not. */
   map_3ds(&map);
   CHECK(video_views_snapshot(&map, VIDEO_SCALE_PACK(800, 480),
            VIDEO_SCALE_PACK(1600, 960), &out));
   CHECK(out.num_views == 3 && out.views[2].x == 480 && out.views[2].width == 640);
   CHECK(!video_views_snapshot(&map, VIDEO_SCALE_PACK(640, 480),
            VIDEO_SCALE_PACK(640, 480), &out));
   CHECK(out.num_views == 0);
   map.num_views = 0;
   CHECK(!video_views_snapshot(&map, VIDEO_SCALE_PACK(800, 480),
            VIDEO_SCALE_PACK(800, 480), &out));
}

static void params_init(video_views_layout_params_t *p,
      const video_views_map_t *map, unsigned w, unsigned h)
{
   memset(p, 0, sizeof(*p));
   p->map  = map;
   p->dims = VIDEO_SCALE_PACK(w, h);
}

static int rect_is(const video_views_rect_t *r, int x, int y,
      unsigned w, unsigned h)
{
   if (     r->pos  == VIDEO_POS_PACK(x, y)
         && r->dims == VIDEO_SCALE_PACK(w, h))
      return 1;
   printf("    got (%d,%d %ux%u), want (%d,%d %ux%u)\n",
         VIDEO_POS_X(r->pos), VIDEO_POS_Y(r->pos),
         VIDEO_SCALE_W(r->dims), VIDEO_SCALE_H(r->dims), x, y, w, h);
   return 0;
}

static void test_layout_screens(void)
{
   video_views_map_t map;
   video_views_layout_params_t p;
   video_views_layout_t l;

   /* DS stacked in 1024x768: scale 2, centred. */
   map_ds(&map);
   params_init(&p, &map, 1024, 768);
   video_views_layout(&p, &l);
   CHECK(l.num_areas == 1 && l.num_placements == 2 && !l.offscreen);
   CHECK(l.canvas_dims == VIDEO_SCALE_PACK(1024, 768));
   CHECK(l.placements[0].view == 0 && rect_is(&l.placements[0].dst, 256,   0, 512, 384));
   CHECK(l.placements[1].view == 1 && rect_is(&l.placements[1].dst, 256, 384, 512, 384));
   CHECK(!l.ui_per_eye && l.ui_dims == VIDEO_SCALE_PACK(1024, 768));

   /* DS side by side. */
   p.screen_layout = VIDEO_SCREEN_LAYOUT_HORIZONTAL;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst,   0, 192, 512, 384));
   CHECK(rect_is(&l.placements[1].dst, 512, 192, 512, 384));

   /* Non-integer fit, and rounding that leaves no seam. */
   p.screen_layout = VIDEO_SCREEN_LAYOUT_VERTICAL;
   p.dims          = VIDEO_SCALE_PACK(1100, 800);
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst, 283,   0, 534, 400));
   CHECK(rect_is(&l.placements[1].dst, 283, 400, 534, 400));

   /* Integer scaling floors the factor to 2. */
   p.scale_integer = true;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst, 294,  16, 512, 384));
   CHECK(rect_is(&l.placements[1].dst, 294, 400, 512, 384));

   /* Rotated 90 degrees: each screen is 192x256 on screen. */
   p.scale_integer = false;
   p.dims          = VIDEO_SCALE_PACK(1024, 768);
   p.rotation      = 1;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst, 368,   0, 288, 384));
   CHECK(rect_is(&l.placements[1].dst, 368, 384, 288, 384));

   /* The view's own aspect ratio. */
   map_ds(&map);
   map.views[0].aspect_ratio = 2.0f;
   map.num_views   = 1;
   map.num_screens = 1;
   params_init(&p, &map, 800, 800);
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst, 0, 200, 800, 400));

   /* An empty map places nothing. */
   map.num_views = 0;
   video_views_layout(&p, &l);
   CHECK(l.num_placements == 0);
}

static void test_layout_stereo(void)
{
   video_views_map_t map;
   video_views_layout_params_t p;
   video_views_layout_t l;

   map_3ds(&map);

   /* 2D shows the left eye and the bottom screen. */
   params_init(&p, &map, 800, 960);
   video_views_layout(&p, &l);
   CHECK(l.num_areas == 1 && l.num_placements == 2);
   CHECK(l.placements[0].view == 0 && rect_is(&l.placements[0].dst,  0,   0, 800, 480));
   CHECK(l.placements[1].view == 2 && rect_is(&l.placements[1].dst, 80, 480, 640, 480));

   /* Full side by side: each half laid out at its own shape. */
   params_init(&p, &map, 1600, 960);
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_FULL;
   video_views_layout(&p, &l);
   CHECK(l.num_areas == 2 && l.num_placements == 4);
   CHECK(rect_is(&l.areas[0],   0, 0, 800, 960));
   CHECK(rect_is(&l.areas[1], 800, 0, 800, 960));
   CHECK(l.placements[0].area == 0 && l.placements[0].view == 0 && rect_is(&l.placements[0].dst,   0,   0, 800, 480));
   CHECK(l.placements[1].area == 0 && l.placements[1].view == 2 && rect_is(&l.placements[1].dst,  80, 480, 640, 480));
   CHECK(l.placements[2].area == 1 && l.placements[2].view == 1 && rect_is(&l.placements[2].dst, 800,   0, 800, 480));
   CHECK(l.placements[3].area == 1 && l.placements[3].view == 2 && rect_is(&l.placements[3].dst, 880, 480, 640, 480));
   CHECK(l.ui_per_eye && l.ui_dims == VIDEO_SCALE_PACK(800, 960));
   CHECK(video_views_ui_dims(&l, VIDEO_SCALE_PACK(1600, 960))
         == VIDEO_SCALE_PACK(800, 960));
   CHECK(video_views_ui_dims(NULL, VIDEO_SCALE_PACK(1600, 960))
         == VIDEO_SCALE_PACK(1600, 960));

   /* Swap Eyes exchanges the eyes, not the mono screen. */
   p.swap_eyes = true;
   video_views_layout(&p, &l);
   CHECK(l.placements[0].view == 1 && l.placements[2].view == 0);
   CHECK(l.placements[1].view == 2 && l.placements[3].view == 2);
   p.swap_eyes = false;

   /* Half side by side: laid out for the window, then squeezed. */
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_HALF;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst,  200,   0, 400, 480));
   CHECK(rect_is(&l.placements[1].dst,  240, 480, 320, 480));
   CHECK(rect_is(&l.placements[2].dst, 1000,   0, 400, 480));
   CHECK(rect_is(&l.placements[3].dst, 1040, 480, 320, 480));
   CHECK(l.ui_per_eye && l.ui_dims == VIDEO_SCALE_PACK(1600, 960));

   /* Top-bottom: squeezed vertically. */
   params_init(&p, &map, 800, 1920);
   p.stereo_mode = VIDEO_STEREO_MODE_TOP_BOTTOM;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.areas[0], 0,   0, 800, 960));
   CHECK(rect_is(&l.areas[1], 0, 960, 800, 960));
   CHECK(rect_is(&l.placements[0].dst,  0,  240, 800, 240));
   CHECK(rect_is(&l.placements[1].dst, 80,  480, 640, 240));
   CHECK(rect_is(&l.placements[2].dst,  0, 1200, 800, 240));
   CHECK(rect_is(&l.placements[3].dst, 80, 1440, 640, 240));

   /* Anaglyph: an offscreen canvas twice the window's width. */
   params_init(&p, &map, 800, 960);
   p.stereo_mode = VIDEO_STEREO_MODE_ANAGLYPH;
   video_views_layout(&p, &l);
   CHECK(l.offscreen && l.canvas_dims == VIDEO_SCALE_PACK(1600, 960));
   CHECK(l.stereo_mode == VIDEO_STEREO_MODE_ANAGLYPH);
   CHECK(!l.ui_per_eye && l.ui_dims == VIDEO_SCALE_PACK(800, 960));
   /* Not the layout's UI size, so ignoring ui_per_eye fails here. */
   CHECK(video_views_ui_dims(&l, VIDEO_SCALE_PACK(640, 480))
         == VIDEO_SCALE_PACK(640, 480));
   CHECK(rect_is(&l.placements[0].dst,   0,   0, 800, 480));
   CHECK(rect_is(&l.placements[1].dst,  80, 480, 640, 480));
   CHECK(rect_is(&l.placements[2].dst, 800,   0, 800, 480));
   CHECK(rect_is(&l.placements[3].dst, 880, 480, 640, 480));
   p.stereo_mode = VIDEO_STEREO_MODE_INTERLACED;
   video_views_layout(&p, &l);
   CHECK(l.offscreen && VIDEO_SCALE_W(l.canvas_dims) == 1600);
}

static void test_layout_one_screen(void)
{
   video_views_map_t map;
   video_views_layout_params_t p;
   video_views_layout_t l;
   video_views_rect_t custom;

   map_vb(&map);

   /* A 4:3 aspect setting overrides the view's own. */
   params_init(&p, &map, 1200, 900);
   p.single_aspect = 4.0f / 3.0f;
   video_views_layout(&p, &l);
   CHECK(l.num_placements == 1 && rect_is(&l.placements[0].dst, 0, 0, 1200, 900));

   /* A custom viewport applies when the area is the whole window... */
   custom.pos  = VIDEO_POS_PACK(100, 50);
   custom.dims = VIDEO_SCALE_PACK(640, 480);
   params_init(&p, &map, 1024, 768);
   p.custom_vp = &custom;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst, 100, 50, 640, 480));

   /* ...and is squeezed with it in half side by side... */
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_HALF;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst,  50, 50, 320, 480));
   CHECK(rect_is(&l.placements[1].dst, 562, 50, 320, 480));

   /* ...but not in full side by side, where the area has its own shape. */
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_FULL;
   video_views_layout(&p, &l);
   CHECK(     VIDEO_POS_X(l.placements[0].dst.pos)    == 0
         && VIDEO_SCALE_W(l.placements[0].dst.dims) == 512);

   /* Custom viewports are one-screen only. */
   map_ds(&map);
   params_init(&p, &map, 1024, 768);
   p.custom_vp = &custom;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst, 256, 0, 512, 384));

   /* Stretched, one screen fills its area, custom viewport or not... */
   map_vb(&map);
   params_init(&p, &map, 1000, 400);
   p.stretch   = true;
   p.custom_vp = &custom;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst, 0, 0, 1000, 400));
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_HALF;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst,   0, 0, 500, 400));
   CHECK(rect_is(&l.placements[1].dst, 500, 0, 500, 400));
   /* ...unless the scale is an integer one... */
   params_init(&p, &map, 1000, 400);
   p.scale_integer = true;
   video_views_layout(&p, &l);
   custom = l.placements[0].dst;
   p.stretch = true;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst, VIDEO_POS_X(custom.pos),
            VIDEO_POS_Y(custom.pos), VIDEO_SCALE_W(custom.dims),
            VIDEO_SCALE_H(custom.dims)));
   /* ...and several screens keep their shapes. */
   map_ds(&map);
   params_init(&p, &map, 1024, 768);
   p.stretch = true;
   video_views_layout(&p, &l);
   CHECK(rect_is(&l.placements[0].dst, 256, 0, 512, 384));
}

static void test_clip(void)
{
   video_views_rect_t r, out, custom;
   video_views_map_t map;
   video_views_layout_params_t p;
   video_views_layout_t l;

   /* Inside, cut at the right and bottom edges, and outside. */
   r.pos  = VIDEO_POS_PACK(100, 50);
   r.dims = VIDEO_SCALE_PACK(200, 100);
   CHECK(video_views_clip(&r, VIDEO_SCALE_PACK(1000, 1000), &out));
   CHECK(rect_is(&out, 100, 50, 200, 100));
   CHECK(video_views_clip(&r, VIDEO_SCALE_PACK(250, 120), &out));
   CHECK(rect_is(&out, 100, 50, 150, 70));
   CHECK(!video_views_clip(&r, VIDEO_SCALE_PACK(100, 1000), &out));
   CHECK(!video_views_clip(&r, VIDEO_SCALE_PACK(1000, 50), &out));
   /* Cut at the left and top edges too; an empty rectangle is not
    * drawn. */
   VIDEO_POS_PUT_X(r.pos, -1);
   CHECK(video_views_clip(&r, VIDEO_SCALE_PACK(1000, 1000), &out));
   CHECK(rect_is(&out, 0, 50, 199, 100));
   r.pos = VIDEO_POS_PACK(0, -1);
   CHECK(video_views_clip(&r, VIDEO_SCALE_PACK(1000, 1000), &out));
   CHECK(rect_is(&out, 0, 0, 200, 99));
   r.pos = VIDEO_POS_PACK(-200, 0);
   CHECK(!video_views_clip(&r, VIDEO_SCALE_PACK(1000, 1000), &out));
   r.pos = VIDEO_POS_PACK(0, 0);
   VIDEO_SCALE_PUT_W(r.dims, 0);
   CHECK(!video_views_clip(&r, VIDEO_SCALE_PACK(1000, 1000), &out));

   /* 3DS anaglyph in 800x960: a canvas of both eyes side by side. */
   map_3ds(&map);
   params_init(&p, &map, 800, 960);
   p.stereo_mode = VIDEO_STEREO_MODE_ANAGLYPH;
   video_views_layout(&p, &l);

   /* The mono bottom screen is placed in both eyes; the first sizes it. */
   CHECK(video_views_first_drawn(&l, 2, true,
            VIDEO_SCALE_PACK(1600, 960), &out));
   CHECK(rect_is(&out, 80, 480, 640, 480));
   /* The right eye's view is in the second area only. */
   CHECK(video_views_first_drawn(&l, 1, true,
            VIDEO_SCALE_PACK(1600, 960), &out));
   CHECK(rect_is(&out, 800, 0, 800, 480));
   CHECK(!video_views_first_drawn(&l, 1, false,
            VIDEO_SCALE_PACK(1600, 960), &out));
   CHECK(!video_views_first_drawn(&l, 3, true,
            VIDEO_SCALE_PACK(1600, 960), &out));
   /* A target shrunk since the layout: whole while part of it shows,
    * or not drawn. */
   CHECK(video_views_first_drawn(&l, 0, true,
            VIDEO_SCALE_PACK(700, 300), &out));
   CHECK(rect_is(&out, 0, 0, 800, 480));
   CHECK(video_views_visible(&l, &l.placements[0],
            VIDEO_SCALE_PACK(700, 300), &out));
   CHECK(rect_is(&out, 0, 0, 700, 300));
   CHECK(!video_views_first_drawn(&l, 2, true,
            VIDEO_SCALE_PACK(1600, 400), &out));
   /* The first placement off the target, the second on it. */
   VIDEO_POS_PUT_X(l.placements[1].dst.pos, 1600);
   CHECK(video_views_first_drawn(&l, 2, true,
            VIDEO_SCALE_PACK(1600, 960), &out));
   CHECK(rect_is(&out, 880, 480, 640, 480));

   /* A custom viewport larger than the window keeps its size and is cut
    * at the window... */
   map_vb(&map);
   custom.pos  = VIDEO_POS_PACK(-100, -50);
   custom.dims = VIDEO_SCALE_PACK(2000, 1500);
   params_init(&p, &map, 1024, 768);
   p.custom_vp = &custom;
   video_views_layout(&p, &l);
   CHECK(video_views_first_drawn(&l, 0, true,
            VIDEO_SCALE_PACK(1024, 768), &out));
   CHECK(rect_is(&out, -100, -50, 2000, 1500));
   CHECK(video_views_visible(&l, &l.placements[0],
            VIDEO_SCALE_PACK(1024, 768), &out));
   CHECK(rect_is(&out, 0, 0, 1024, 768));
   /* ...and in side by side at each eye's half. */
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_HALF;
   video_views_layout(&p, &l);
   CHECK(video_views_visible(&l, &l.placements[0],
            VIDEO_SCALE_PACK(1024, 768), &out));
   CHECK(rect_is(&out, 0, 0, 512, 768));
   CHECK(video_views_visible(&l, &l.placements[1],
            VIDEO_SCALE_PACK(1024, 768), &out));
   CHECK(rect_is(&out, 512, 0, 512, 768));
}

static void test_map_point(void)
{
   video_views_map_t map;
   video_views_layout_params_t p;
   video_views_layout_t l;
   uint32_t pos;

   /* DS stacked in 1024x768: screens at (256,0) and (256,384), 512x384. */
   map_ds(&map);
   params_init(&p, &map, 1024, 768);
   video_views_layout(&p, &l);

   CHECK(video_views_map_point(&l, &map, VIDEO_SCALE_PACK(256, 384), 256, 0, true, &pos));
   CHECK(VIDEO_POS_X(pos) == -0x7fff && VIDEO_POS_Y(pos) == -0x7fff);

   CHECK(video_views_map_point(&l, &map, VIDEO_SCALE_PACK(256, 384), 767, 767, true, &pos));
   CHECK(VIDEO_POS_X(pos) == 0x7fff && VIDEO_POS_Y(pos) == 0x7fff);

   /* Middle of the bottom screen: packed pixel (128, 288). */
   CHECK(video_views_map_point(&l, &map, VIDEO_SCALE_PACK(256, 384), 512, 576, true, &pos));
   CHECK(VIDEO_POS_X(pos) == 128 && VIDEO_POS_Y(pos) == 16511);

   /* Beside both screens. */
   CHECK(video_views_map_point(&l, &map, VIDEO_SCALE_PACK(256, 384), 100, 100, true, &pos));
   CHECK(VIDEO_POS_X(pos) == -0x8000 && VIDEO_POS_Y(pos) == -0x8000);
   /* Confined: clamps into the nearest screen, packed pixel (0, 50). */
   CHECK(video_views_map_point(&l, &map, VIDEO_SCALE_PACK(256, 384), 100, 100, false, &pos));
   CHECK(VIDEO_POS_X(pos) == -0x7fff && VIDEO_POS_Y(pos) == -24213);

   /* 3DS, full side by side: a tap on the right eye maps to the left eye's
    * rectangle, packed pixel (200, 120). */
   map_3ds(&map);
   params_init(&p, &map, 1600, 960);
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_FULL;
   video_views_layout(&p, &l);
   CHECK(video_views_map_point(&l, &map, VIDEO_SCALE_PACK(800, 480), 1200, 240, true, &pos));
   CHECK(VIDEO_POS_X(pos) == -16364 && VIDEO_POS_Y(pos) == -16351);
   /* The bottom screen in the left half: packed pixel (400, 360). */
   CHECK(video_views_map_point(&l, &map, VIDEO_SCALE_PACK(800, 480), 400, 720, true, &pos));
   CHECK(VIDEO_POS_X(pos) == 40 && VIDEO_POS_Y(pos) == 16485);

   /* Anaglyph: the window is the left half of the canvas. */
   params_init(&p, &map, 800, 960);
   p.stereo_mode = VIDEO_STEREO_MODE_ANAGLYPH;
   video_views_layout(&p, &l);
   CHECK(video_views_map_point(&l, &map, VIDEO_SCALE_PACK(800, 480), 400, 240, true, &pos));
   CHECK(VIDEO_POS_X(pos) == -16364 && VIDEO_POS_Y(pos) == -16351);

   /* Nothing to map. */
   map.num_views = 0;
   video_views_layout(&p, &l);
   CHECK(!video_views_map_point(&l, &map, VIDEO_SCALE_PACK(800, 480), 400, 240, true, &pos));
}

static int ui_point_is(const video_views_layout_t *l, int x, int y,
      int want_x, int want_y)
{
   int rx = INT_MIN, ry = INT_MIN;
   video_views_ui_point(l, x, y, &rx, &ry);
   if (rx == want_x && ry == want_y)
      return 1;
   printf("    (%d,%d) got (%d,%d), want (%d,%d)\n",
         x, y, rx, ry, want_x, want_y);
   return 0;
}

static void test_ui_point(void)
{
   video_views_map_t map;
   video_views_layout_params_t p;
   video_views_layout_t l;

   map_3ds(&map);

   /* A frame drawn whole, and 2D: the UI is the window. */
   CHECK(ui_point_is(NULL, 1200, 300, 1200, 300));
   params_init(&p, &map, 1600, 960);
   video_views_layout(&p, &l);
   CHECK(ui_point_is(&l, 1200, 300, 1200, 300));
   CHECK(ui_point_is(&l, -10, 2000, -10, 2000));

   /* Full side by side: one eye's size, so each copy maps as it is. */
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_FULL;
   video_views_layout(&p, &l);
   CHECK(ui_point_is(&l,  400, 300, 400, 300));
   CHECK(ui_point_is(&l, 1200, 300, 400, 300));
   CHECK(ui_point_is(&l,  799, 959, 799, 959));
   CHECK(ui_point_is(&l,  800,   0,   0,   0));
   CHECK(ui_point_is(&l, 1599, 959, 799, 959));
   /* Outside the window stays outside the UI, by the nearer copy. */
   CHECK(ui_point_is(&l,  -10,  50, -10,  50));
   CHECK(ui_point_is(&l, 1700,  -5, 900,  -5));
   CHECK(ui_point_is(&l, 1200, 970, 400, 970));

   /* Swap Eyes moves the views, not the areas. */
   p.swap_eyes = true;
   video_views_layout(&p, &l);
   CHECK(ui_point_is(&l,  400, 300, 400, 300));
   CHECK(ui_point_is(&l, 1200, 300, 400, 300));
   p.swap_eyes = false;

   /* An odd width: the right copy is a pixel wider than the UI. */
   p.dims = VIDEO_SCALE_PACK(1601, 960);
   video_views_layout(&p, &l);
   CHECK(ui_point_is(&l,  799, 0, 799, 0));
   CHECK(ui_point_is(&l,  800, 0,   0, 0));
   CHECK(ui_point_is(&l,  801, 0,   0, 0));
   CHECK(ui_point_is(&l, 1600, 0, 799, 0));
   p.dims = VIDEO_SCALE_PACK(1600, 960);

   /* Half side by side: the window's size, squeezed into each half. */
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_HALF;
   video_views_layout(&p, &l);
   CHECK(ui_point_is(&l,  300, 300,  600, 300));
   CHECK(ui_point_is(&l, 1100, 300,  600, 300));
   CHECK(ui_point_is(&l,  799, 959, 1598, 959));
   CHECK(ui_point_is(&l,  800,   0,    0,   0));
   CHECK(ui_point_is(&l,   -1,   0,   -2,   0));
   CHECK(ui_point_is(&l, 1700,  -5, 1800,  -5));

   /* Top-bottom: squeezed into each half's height. */
   p.stereo_mode = VIDEO_STEREO_MODE_TOP_BOTTOM;
   video_views_layout(&p, &l);
   CHECK(ui_point_is(&l,  400, 200,  400,  400));
   CHECK(ui_point_is(&l,  400, 680,  400,  400));
   CHECK(ui_point_is(&l, 1599, 479, 1599,  958));
   CHECK(ui_point_is(&l,    0, 480,    0,    0));
   CHECK(ui_point_is(&l,  100,  -3,  100,   -6));
   CHECK(ui_point_is(&l,  100, 1000, 100, 1040));
   p.swap_eyes = true;
   video_views_layout(&p, &l);
   CHECK(ui_point_is(&l,  400, 680,  400,  400));
   p.swap_eyes = false;

   /* Areas with a gap between them: the nearer one, the first on a
    * tie, and the point stays outside the UI. */
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_FULL;
   video_views_layout(&p, &l);
   l.areas[0].dims = VIDEO_SCALE_PACK(700, 960);
   l.areas[1].pos  = VIDEO_POS_PACK(900, 0);
   l.areas[1].dims = VIDEO_SCALE_PACK(700, 960);
   l.ui_dims       = VIDEO_SCALE_PACK(700, 960);
   CHECK(ui_point_is(&l,  750, 10, 750, 10));
   CHECK(ui_point_is(&l,  860, 10, -40, 10));
   l.areas[1].pos  = VIDEO_POS_PACK(901, 0);
   CHECK(ui_point_is(&l,  800, 10, 800, 10));
   CHECK(ui_point_is(&l,  801, 10, -100, 10));

   /* Anaglyph and interlaced blend both eyes into the window and draw
    * the UI once, over it. */
   params_init(&p, &map, 800, 960);
   p.stereo_mode = VIDEO_STEREO_MODE_ANAGLYPH;
   video_views_layout(&p, &l);
   CHECK(ui_point_is(&l, 400, 240, 400, 240));
   CHECK(ui_point_is(&l, 799, 959, 799, 959));
   CHECK(ui_point_is(&l, 1200, 240, 1200, 240));
   p.stereo_mode = VIDEO_STEREO_MODE_INTERLACED;
   video_views_layout(&p, &l);
   CHECK(ui_point_is(&l, 400, 241, 400, 241));

   /* Nothing laid out, and areas without a size. */
   map.num_views = 0;
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_HALF;
   video_views_layout(&p, &l);
   CHECK(ui_point_is(&l, 400, 240, 400, 240));
   memset(&l, 0, sizeof(l));
   l.ui_per_eye = true;
   l.num_areas  = 2;
   l.ui_dims    = VIDEO_SCALE_PACK(800, 960);
   CHECK(ui_point_is(&l, 400, 240, 400, 240));
}

static void test_eye_pair(void)
{
   video_views_map_t map;
   struct retro_video_view v[4];
   int eyes[2];

   map_3ds(&map);
   CHECK(video_views_eye_pair(&map, eyes) && eyes[0] == 0 && eyes[1] == 1);
   map_vb(&map);
   CHECK(video_views_eye_pair(&map, eyes) && eyes[0] == 0 && eyes[1] == 1);
   map_ds(&map);
   CHECK(!video_views_eye_pair(&map, eyes) && eyes[0] == -1 && eyes[1] == -1);

   /* Two pairs are not one. */
   v[0] = mkview(  0,   0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
   v[1] = mkview(400,   0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_RIGHT);
   v[2] = mkview(  0, 240, 400, 240, 1, RETRO_VIDEO_VIEW_EYE_LEFT);
   v[3] = mkview(400, 240, 400, 240, 1, RETRO_VIDEO_VIEW_EYE_RIGHT);
   CHECK(video_views_validate(v, 4, &map));
   CHECK(!video_views_eye_pair(&map, eyes) && eyes[0] == -1 && eyes[1] == -1);

   /* The right eye first. */
   v[0] = mkview(400, 0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_RIGHT);
   v[1] = mkview(  0, 0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
   CHECK(video_views_validate(v, 2, &map));
   CHECK(video_views_eye_pair(&map, eyes) && eyes[0] == 1 && eyes[1] == 0);
}

/* A screen's views as a map of their own: the 3DS driver's top and
 * bottom screens. */
static void test_select_screens(void)
{
   video_views_map_t map, sub;
   struct retro_video_view v[4];
   unsigned index[RETRO_VIDEO_VIEWS_MAX];

   map_3ds(&map);
   CHECK(video_views_select_screens(&map, 0, 1, &sub, index));
   CHECK(sub.num_views == 2 && sub.num_screens == 1);
   CHECK(index[0] == 0 && index[1] == 1);
   CHECK(sub.views[1].eye == RETRO_VIDEO_VIEW_EYE_RIGHT);
   CHECK(video_views_find(&sub, 0, RETRO_VIDEO_VIEW_EYE_RIGHT) == 1);

   CHECK(video_views_select_screens(&map, 1, 1, &sub, index));
   CHECK(sub.num_views == 1 && sub.num_screens == 1);
   CHECK(index[0] == 2 && sub.views[0].screen == 0);
   CHECK(sub.views[0].x == 240 && sub.views[0].y == 240);

   /* The count is cut to the screens there are. */
   CHECK(video_views_select_screens(&map, 1, 5, &sub, index));
   CHECK(sub.num_screens == 1 && sub.num_views == 1);
   CHECK(!video_views_select_screens(&map, 2, 1, &sub, index));
   CHECK(sub.num_views == 0 && sub.num_screens == 0);
   CHECK(!video_views_select_screens(&map, 0, 0, &sub, index));

   /* Screens 1 and 2 of three, in the map's order, which needn't be the
    * screens'. */
   v[0] = mkview(0,   0, 100, 50, 2, RETRO_VIDEO_VIEW_EYE_NONE);
   v[1] = mkview(0,  50, 100, 50, 0, RETRO_VIDEO_VIEW_EYE_NONE);
   v[2] = mkview(0, 100, 100, 50, 1, RETRO_VIDEO_VIEW_EYE_LEFT);
   v[3] = mkview(100, 100, 100, 50, 1, RETRO_VIDEO_VIEW_EYE_RIGHT);
   CHECK(video_views_validate(v, 4, &map));
   CHECK(video_views_select_screens(&map, 1, 2, &sub, index));
   CHECK(sub.num_views == 3 && sub.num_screens == 2);
   CHECK(index[0] == 0 && sub.views[0].screen == 1);
   CHECK(index[1] == 2 && sub.views[1].screen == 0);
   CHECK(index[2] == 3 && sub.views[2].screen == 0);
   /* A layout of the selection places its screens like a map of two. */
   {
      video_views_layout_params_t p;
      video_views_layout_t l;
      memset(&p, 0, sizeof(p));
      p.map         = &sub;
      p.dims        = VIDEO_SCALE_PACK(320, 240);
      p.stereo_mode = VIDEO_STEREO_MODE_2D;
      video_views_layout(&p, &l);
      CHECK(l.num_placements == 2);
      CHECK(l.placements[0].view == 1 && l.placements[1].view == 0);
   }
}

/* A console's touchscreen through the views it shows: the 3DS's bottom
 * screen showing the DS map's second screen. */
static void test_touch(void)
{
   video_views_map_t map, sub;
   video_views_layout_params_t p;
   video_views_layout_t l;
   video_views_touch_t t;
   unsigned index[RETRO_VIDEO_VIEWS_MAX];
   int16_t x = 123, y = 456;
   unsigned frame = VIDEO_SCALE_PACK(256, 384);

   memset(&t, 0, sizeof(t));
   CHECK(!video_views_touch_point(&t, 0, 0, &x, &y));
   CHECK(x == 123 && y == 456);

   map_ds(&map);
   CHECK(video_views_select_screens(&map, 1, 1, &sub, index));
   memset(&p, 0, sizeof(p));
   p.map         = &sub;
   p.dims        = VIDEO_SCALE_PACK(320, 240);
   p.stereo_mode = VIDEO_STEREO_MODE_2D;
   video_views_layout(&p, &l);
   /* 256x192 into 320x240: the whole screen, aspect kept. */
   CHECK(l.num_placements == 1
         && l.placements[0].dst.dims == VIDEO_SCALE_PACK(320, 240));
   video_views_touch_publish(&t, &map, &l, index, p.dims, frame);
   CHECK(!(retro_atomic_load_acquire_int(&t.seq) & 1));

   /* The display's middle is the second screen's: (128, 288) packed. */
   CHECK(video_views_touch_point(&t, 0, 0, &x, &y));
   CHECK(x >= -2 && x <= 2);
   CHECK(y >= 16383 - 200 && y <= 16383 + 200);
   /* The top left corner is the second screen's: (0, 192). */
   CHECK(video_views_touch_point(&t, -0x7fff, -0x7fff, &x, &y));
   CHECK(x == -0x7fff && y >= -2 && y <= 2);

   /* Pillarboxed: off the view is off the frame. */
   p.dims = VIDEO_SCALE_PACK(400, 240);
   video_views_layout(&p, &l);
   video_views_touch_publish(&t, &map, &l, index, p.dims, frame);
   CHECK(video_views_touch_point(&t, -0x7fff, 0, &x, &y));
   CHECK(x == -0x8000 && y == -0x8000);

   /* Taken back: none. */
   video_views_touch_publish(&t, NULL, NULL, NULL, 0, 0);
   x = 7;
   CHECK(!video_views_touch_point(&t, 0, 0, &x, &y) && x == 7);
}

/* The CPU drivers' composer: a layout made for a screen twice the DS
 * map's size is composed at half that, each view copied as it is, for
 * display hardware to enlarge; and side by side at full size. */
/* What a blitter scales into each placement's visible part */
static void test_cut_source(void)
{
   struct retro_video_view v = { 0 };
   video_views_rect_t dst, cut, out;

   v.x = 10; v.y = 20; v.width = 100; v.height = 50;
   /* Drawn whole, at twice its size: all of it */
   dst.pos  = VIDEO_POS_PACK(4, 6);
   dst.dims = VIDEO_SCALE_PACK(200, 100);
   video_views_cut_source(&v, &dst, &dst, &out);
   CHECK(VIDEO_POS_X(out.pos) == 10 && VIDEO_POS_Y(out.pos) == 20);
   CHECK(VIDEO_SCALE_W(out.dims) == 100 && VIDEO_SCALE_H(out.dims) == 50);
   /* Its right half and bottom half cut away */
   cut.pos  = dst.pos;
   cut.dims = VIDEO_SCALE_PACK(100, 50);
   video_views_cut_source(&v, &dst, &cut, &out);
   CHECK(VIDEO_POS_X(out.pos) == 10 && VIDEO_POS_Y(out.pos) == 20);
   CHECK(VIDEO_SCALE_W(out.dims) == 50 && VIDEO_SCALE_H(out.dims) == 25);
   /* Off the left and top of the screen: the view's far quarter */
   dst.pos  = VIDEO_POS_PACK(-150, -75);
   cut.pos  = VIDEO_POS_PACK(0, 0);
   cut.dims = VIDEO_SCALE_PACK(50, 25);
   video_views_cut_source(&v, &dst, &cut, &out);
   CHECK(VIDEO_POS_X(out.pos) == 85 && VIDEO_POS_Y(out.pos) == 57);
   CHECK(VIDEO_SCALE_W(out.dims) == 25 && VIDEO_SCALE_H(out.dims) == 13);
   /* Shrunk to less than a pixel's worth: still a pixel */
   dst.pos  = VIDEO_POS_PACK(0, 0);
   dst.dims = VIDEO_SCALE_PACK(10, 5);
   cut.pos  = VIDEO_POS_PACK(9, 4);
   cut.dims = VIDEO_SCALE_PACK(1, 1);
   video_views_cut_source(&v, &dst, &cut, &out);
   CHECK(VIDEO_POS_X(out.pos) == 100 && VIDEO_POS_Y(out.pos) == 60);
   CHECK(VIDEO_SCALE_W(out.dims) == 10 && VIDEO_SCALE_H(out.dims) == 10);
   /* A one-pixel view stays one pixel */
   v.width  = 1;
   v.height = 1;
   dst.dims = VIDEO_SCALE_PACK(40, 40);
   cut.pos  = VIDEO_POS_PACK(39, 39);
   video_views_cut_source(&v, &dst, &cut, &out);
   CHECK(VIDEO_POS_X(out.pos) == 10 && VIDEO_POS_Y(out.pos) == 20);
   CHECK(VIDEO_SCALE_W(out.dims) == 1 && VIDEO_SCALE_H(out.dims) == 1);
}

static void test_cpu_compose(void)
{
   unsigned i, x, y;
   video_views_map_t map;
   video_views_layout_params_t p;
   video_views_layout_t l;
   video_views_cpu_t cpu;
   static uint32_t frame[256 * 384];
   static uint32_t out[512 * 384];
   unsigned dims;
   float scale;
   bool ok = true;

   memset(&cpu, 0, sizeof(cpu));
   for (y = 0; y < 384; y++)
      for (x = 0; x < 256; x++)
         frame[y * 256 + x] = (y << 12) | (x << 1) | (y >= 192);

   map_ds(&map);
   memset(&p, 0, sizeof(p));
   p.map         = &map;
   p.dims        = VIDEO_SCALE_PACK(512, 768);
   p.stereo_mode = VIDEO_STEREO_MODE_2D;
   video_views_layout(&p, &l);
   scale = video_views_cpu_scale(&map, &l);
   CHECK(scale == 0.5f);
   dims  = video_views_cpu_dims(p.dims, scale);
   CHECK(dims == VIDEO_SCALE_PACK(256, 384));
   video_views_cpu_compose(&cpu, &map, &l, frame, 256 * 4,
         SCALER_FMT_ARGB8888, SCALER_TYPE_POINT, scale, dims, out, 256,
         NULL);
   /* The screens stacked as they are packed: the frame itself */
   for (i = 0; i < 256 * 384; i++)
      if ((out[i] & 0xffffff) != (frame[i] & 0xffffff))
         ok = false;
   CHECK(ok);

   /* Side by side, full: each eye's half its own. */
   p.dims        = VIDEO_SCALE_PACK(512, 384);
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_FULL;
   video_views_layout(&p, &l);
   video_views_cpu_compose(&cpu, &map, &l, frame, 256 * 4,
         SCALER_FMT_ARGB8888, SCALER_TYPE_POINT, 1.0f, p.dims, out, 512,
         NULL);
   /* Both halves show the top screen at their top left */
   CHECK((out[0] & 0xffffff) == (frame[0] & 0xffffff));
   CHECK((out[256] & 0xffffff) == (frame[0] & 0xffffff));

   /* Interlaced is composed at the screen's own size. */
   p.stereo_mode = VIDEO_STEREO_MODE_INTERLACED;
   video_views_layout(&p, &l);
   CHECK(video_views_cpu_scale(&map, &l) == 1.0f);
   video_views_cpu_free(&cpu);
}

/* The CPU drivers' blend of a canvas's halves. */
static void test_blend(void)
{
   /* Two 2x2 halves side by side, and an output wider than the eye. */
   uint32_t canvas[2 * 4];
   uint32_t out[2 * 3];
   unsigned i;

   for (i = 0; i < 2; i++)
   {
      canvas[i * 4 + 0] = 0xffffff;
      canvas[i * 4 + 1] = 0x000000;
      canvas[i * 4 + 2] = 0x000000;
      canvas[i * 4 + 3] = 0xffffff;
   }
   for (i = 0; i < 6; i++)
      out[i] = 0x123456;

   video_views_blend_xrgb8888(canvas, 4, out, 3, VIDEO_SCALE_PACK(2, 2),
         VIDEO_STEREO_MODE_ANAGLYPH);
   /* White left only is red, white right only cyan. */
   CHECK(out[0] == 0xff0000 && out[1] == 0x00ffff);
   CHECK(out[3] == 0xff0000 && out[4] == 0x00ffff);
   /* Past the eye's width is left alone. */
   CHECK(out[2] == 0x123456 && out[5] == 0x123456);

   canvas[4] = 0x0000ff;
   canvas[6] = 0x00ff00;
   video_views_blend_xrgb8888(canvas, 4, out, 3, VIDEO_SCALE_PACK(2, 2),
         VIDEO_STEREO_MODE_INTERLACED);
   /* Row 0 from the left half, row 1 from the right. */
   CHECK(out[0] == 0xffffff && out[1] == 0x000000);
   CHECK(out[3] == 0x00ff00 && out[4] == 0xffffff);
}

/* ---- the compositor, instantiated over fakes ---- */

typedef struct fake_chain
{
   int id;
} fake_chain_t;

typedef struct fake_target
{
   int made;
} fake_target_t;

typedef struct fake_drv
{
   int stock_made, stock_fail, synced;
   int made, retired, retire_refuses, make_fails;
} fake_drv_t;

typedef struct fake_ctx
{
   int tag;
} fake_ctx_t;

static const fake_ctx_t fake_ctx = { 42 };
static int bad_ctx;

#define MAX_DRAWS 16
static struct
{
   int view;              /* -1: a pass */
   fake_chain_t *chain;
   video_views_rect_t dst, cut;
   int again;
} draws[MAX_DRAWS];
static int num_draws;
static fake_chain_t chain_objs[RETRO_VIDEO_VIEWS_MAX * 2];

static void *fake_stock(fake_drv_t *d)
{
   if (d->stock_fail)
      return NULL;
   return &chain_objs[RETRO_VIDEO_VIEWS_MAX + d->stock_made++];
}

static void fake_sync(fake_drv_t *d, fake_chain_t *c)
{
   (void)c;
   d->synced++;
}

static void fake_direct(fake_drv_t *d, const fake_ctx_t *x, unsigned view,
      const video_views_rect_t *dst, const video_views_rect_t *cut)
{
   (void)d;
   if (x != &fake_ctx)
      bad_ctx++;
   if (num_draws >= MAX_DRAWS)
      return;
   draws[num_draws].view  = (int)view;
   draws[num_draws].chain = NULL;
   draws[num_draws].dst   = *dst;
   draws[num_draws].cut   = *cut;
   draws[num_draws].again = 0;
   num_draws++;
}

static void fake_pass(fake_drv_t *d, const fake_ctx_t *x, fake_chain_t *c,
      const video_views_rect_t *dst, const video_views_rect_t *cut,
      bool again)
{
   (void)d;
   if (x != &fake_ctx)
      bad_ctx++;
   if (num_draws >= MAX_DRAWS)
      return;
   draws[num_draws].view  = -1;
   draws[num_draws].chain = c;
   draws[num_draws].dst   = *dst;
   draws[num_draws].cut   = *cut;
   draws[num_draws].again = again ? 1 : 0;
   num_draws++;
}

static bool fake_make(fake_drv_t *d, fake_target_t *t, unsigned dims)
{
   (void)dims;
   d->made++;
   t->made = !d->make_fails;
   return !d->make_fails;
}

static bool fake_retire(fake_drv_t *d, fake_target_t *t)
{
   if (d->retire_refuses)
      return false;
   d->retired++;
   t->made = 0;
   return true;
}

#define VV_FN(name)                     fake_vv_##name
#define VV_DRV                          fake_drv_t
#define VV_CHAIN                        fake_chain_t
#define VV_CREATE_STOCK(drv)            fake_stock(drv)
#define VV_SYNC_PARAMS(drv, c)          fake_sync(drv, c)
#define VV_DRAW                         fake_ctx_t
#define VV_DIRECT(drv, x, v, dst, cut)  fake_direct(drv, x, v, dst, cut)
#define VV_PASS(drv, x, c, dst, cut, a) fake_pass(drv, x, c, dst, cut, a)
#define VV_TARGET                       fake_target_t
#define VV_TARGET_MAKE(drv, t, dims)    fake_make(drv, t, dims)
#define VV_TARGET_RETIRE(drv, t)        fake_retire(drv, t)
#define VV_ROWS_UP
#include "gfx/video_views_draw.h"

static void layout_3ds(video_views_map_t *map, video_views_layout_t *l,
      unsigned w, unsigned h, unsigned mode)
{
   video_views_layout_params_t p;
   map_3ds(map);
   params_init(&p, map, w, h);
   p.stereo_mode = mode;
   video_views_layout(&p, l);
}

static void test_pick_chains(void)
{
   video_views_map_t map;
   video_views_layout_t l;
   video_views_chains_t set;
   fake_drv_t d;
   fake_chain_t *chains[RETRO_VIDEO_VIEWS_MAX];
   video_views_rect_t rects[RETRO_VIDEO_VIEWS_MAX];
   unsigned full = VIDEO_SCALE_PACK(1600, 960);

   layout_3ds(&map, &l, 1600, 960, VIDEO_STEREO_MODE_SBS_FULL);
   memset(&set, 0, sizeof(set));
   memset(&d, 0, sizeof(d));
   set.count = 3;

   /* Stock chains made on first use, once each, and nothing synced. */
   CHECK(fake_vv_pick_chains(&d, &set, &map, &l, true, true, full, chains,
            rects));
   CHECK(d.stock_made == 3 && d.synced == 0);
   CHECK(chains[0] && chains[1] && chains[2] && chains[0] != chains[1]);
   CHECK(rect_is(&rects[1], 800, 0, 800, 480));
   CHECK(fake_vv_pick_chains(&d, &set, &map, &l, true, true, full, chains,
            rects));
   CHECK(d.stock_made == 3);

   /* The left area only: the right eye isn't drawn, so has no chain. */
   CHECK(fake_vv_pick_chains(&d, &set, &map, &l, true, false, full, chains,
            rects));
   CHECK(chains[0] && !chains[1] && chains[2]);

   /* Preset chains are the driver's: none made here, each synced. */
   CHECK(!fake_vv_pick_chains(&d, &set, &map, &l, false, true, full, chains,
            rects));
   set.preset[0] = &chain_objs[0];
   set.preset[1] = &chain_objs[1];
   set.preset[2] = &chain_objs[2];
   CHECK(fake_vv_pick_chains(&d, &set, &map, &l, false, true, full, chains,
            rects));
   CHECK(d.synced == 3 && chains[2] == &chain_objs[2] && d.stock_made == 3);

   /* A frame queued before the count shrank. */
   set.count = 2;
   CHECK(!fake_vv_pick_chains(&d, &set, &map, &l, false, true, full, chains,
            rects));

   /* A stock chain that can't be made is not tried every frame. */
   memset(&set, 0, sizeof(set));
   set.count    = 3;
   d.stock_fail = 1;
   CHECK(!fake_vv_pick_chains(&d, &set, &map, &l, true, true, full, chains,
            rects));
   CHECK(set.stock_failed);
   d.stock_fail = 0;
   CHECK(!fake_vv_pick_chains(&d, &set, &map, &l, true, true, full, chains,
            rects));
   CHECK(d.stock_made == 3);
}

static void test_draw_placements(void)
{
   video_views_map_t map;
   video_views_layout_t l;
   fake_drv_t d;
   fake_chain_t *chains[RETRO_VIDEO_VIEWS_MAX];
   unsigned full = VIDEO_SCALE_PACK(1600, 960);

   layout_3ds(&map, &l, 1600, 960, VIDEO_STEREO_MODE_SBS_FULL);
   memset(&d, 0, sizeof(d));
   memset(chains, 0, sizeof(chains));
   chains[0] = &chain_objs[0];
   chains[1] = &chain_objs[1];
   chains[2] = &chain_objs[2];

   /* Each placement through its chain; the bottom screen, in both eyes,
    * the second time again. */
   num_draws = 0;
   fake_vv_draw_placements(&d, &fake_ctx, &l, chains, true, full, false, false);
   CHECK(num_draws == 4);
   CHECK(draws[0].chain == chains[0] && !draws[0].again);
   CHECK(draws[1].chain == chains[2] && !draws[1].again);
   CHECK(draws[2].chain == chains[1] && !draws[2].again);
   CHECK(draws[3].chain == chains[2] && draws[3].again);
   CHECK(rect_is(&draws[3].dst, 880, 480, 640, 480));

   /* The left area only, and a view without a chain left out. */
   num_draws = 0;
   chains[2] = NULL;
   fake_vv_draw_placements(&d, &fake_ctx, &l,
         chains, false, full, false, false);
   CHECK(num_draws == 1 && draws[0].chain == chains[0]);
   chains[2] = &chain_objs[2];

   /* Straight from the frame: by view, never through a chain. */
   num_draws = 0;
   fake_vv_draw_placements(&d, &fake_ctx, &l, chains, true, full, true, false);
   CHECK(num_draws == 4 && draws[0].view == 0 && draws[2].view == 1
         && draws[3].view == 2);

   /* A placement cut to its target. */
   num_draws = 0;
   fake_vv_draw_placements(&d, &fake_ctx, &l,
         chains, true, VIDEO_SCALE_PACK(1200, 960),
         false, false);
   CHECK(num_draws == 4 && rect_is(&draws[3].cut, 880, 480, 320, 480)
         && rect_is(&draws[3].dst, 880, 480, 640, 480));

   /* Interlaced into a 3200x960 canvas whose rows count from the bottom:
    * with an even height the eyes trade halves - the left eye's top
    * screen at 400, the right's at 2000. */
   layout_3ds(&map, &l, 1600, 960, VIDEO_STEREO_MODE_INTERLACED);
   CHECK(l.offscreen);
   num_draws = 0;
   fake_vv_draw_placements(&d, &fake_ctx, &l,
         chains, true, l.canvas_dims, false, true);
   CHECK(num_draws == 4);
   CHECK(VIDEO_POS_X(draws[0].dst.pos) == 2000
         && VIDEO_POS_X(draws[0].cut.pos) == 2000);
   CHECK(VIDEO_POS_X(draws[2].dst.pos) == 400
         && VIDEO_POS_X(draws[2].cut.pos) == 400);
   /* From the top they stay. */
   num_draws = 0;
   fake_vv_draw_placements(&d, &fake_ctx, &l,
         chains, true, l.canvas_dims, false, false);
   CHECK(num_draws == 4 && VIDEO_POS_X(draws[0].dst.pos) == 400);
   /* And with an odd height. */
   layout_3ds(&map, &l, 1600, 961, VIDEO_STEREO_MODE_INTERLACED);
   num_draws = 0;
   fake_vv_draw_placements(&d, &fake_ctx, &l,
         chains, true, l.canvas_dims, false, true);
   CHECK(num_draws == 4
         && VIDEO_POS_X(draws[0].dst.pos)
            == VIDEO_POS_X(l.placements[0].dst.pos));
   CHECK(!bad_ctx);
}

static void test_targets(void)
{
   video_views_target_t t;
   fake_target_t obj;
   fake_drv_t d;
   memset(&t, 0, sizeof(t));
   memset(&obj, 0, sizeof(obj));
   memset(&d, 0, sizeof(d));

   CHECK(!fake_vv_target_ensure(&d, &t, &obj, 0) && d.made == 0);
   CHECK(fake_vv_target_ensure(&d, &t, &obj, VIDEO_SCALE_PACK(64, 32))
         && d.made == 1);
   /* The same size is kept. */
   CHECK(fake_vv_target_ensure(&d, &t, &obj, VIDEO_SCALE_PACK(64, 32))
         && d.made == 1 && d.retired == 0);
   /* Another is remade, the old one let go first. */
   CHECK(fake_vv_target_ensure(&d, &t, &obj, VIDEO_SCALE_PACK(80, 32))
         && d.made == 2 && d.retired == 1);
   /* Not while it can't be let go. */
   d.retire_refuses = 1;
   CHECK(!fake_vv_target_ensure(&d, &t, &obj, VIDEO_SCALE_PACK(96, 32))
         && d.made == 2);
   fake_vv_target_drop(&d, &t, &obj);
   CHECK(t.made && t.dims == VIDEO_SCALE_PACK(80, 32));
   d.retire_refuses = 0;
   /* A size that failed isn't tried again until the size changes. */
   d.make_fails = 1;
   CHECK(!fake_vv_target_ensure(&d, &t, &obj, VIDEO_SCALE_PACK(96, 32))
         && d.made == 3);
   CHECK(!fake_vv_target_ensure(&d, &t, &obj, VIDEO_SCALE_PACK(96, 32))
         && d.made == 3 && d.retired == 2);
   d.make_fails = 0;
   CHECK(fake_vv_target_ensure(&d, &t, &obj, VIDEO_SCALE_PACK(64, 64))
         && d.made == 4 && d.retired == 2);
   fake_vv_target_drop(&d, &t, &obj);
   CHECK(!t.made && !t.dims && d.retired == 3);
   fake_vv_target_drop(&d, &t, &obj);
   CHECK(d.retired == 3);
}

static int widget_polls;
static bool poll_widgets(void)
{
   widget_polls++;
   return true;
}

static void test_ui_helpers(void)
{
   video_views_map_t map;
   video_views_layout_t l;
   video_views_ui_dims_t saved;
   unsigned dims = 11, video_dims = 22;
   const char *none = NULL;

   layout_3ds(&map, &l, 1600, 960, VIDEO_STEREO_MODE_SBS_FULL);
   CHECK(video_views_ui_per_eye(&map, &l, true));
   CHECK(!video_views_ui_per_eye(&map, &l, false));
   layout_3ds(&map, &l, 1600, 960, VIDEO_STEREO_MODE_2D);
   CHECK(!video_views_ui_per_eye(&map, &l, true));

   CHECK(!VIDEO_VIEWS_UI_WANTED(true, false, none, false, false));
   CHECK(!VIDEO_VIEWS_UI_WANTED(true, false, "", false, false));
   CHECK(VIDEO_VIEWS_UI_WANTED(true, false, "msg", false, false));
   CHECK(VIDEO_VIEWS_UI_WANTED(true, true, none, false, false));
   CHECK(VIDEO_VIEWS_UI_WANTED(true, false, none, true, false));
   CHECK(VIDEO_VIEWS_UI_WANTED(true, false, none, false, true));
   CHECK(!VIDEO_VIEWS_UI_WANTED(false, true, "msg", true, true));
   /* The widgets aren't asked when something else already wants it. */
   CHECK(VIDEO_VIEWS_UI_WANTED(true, true, none, false, poll_widgets())
         && !widget_polls);
   CHECK(!VIDEO_VIEWS_UI_WANTED(false, false, none, false, poll_widgets())
         && !widget_polls);
   CHECK(VIDEO_VIEWS_UI_WANTED(true, false, none, false, poll_widgets())
         && widget_polls == 1);

   video_views_ui_enter(&saved, &dims, &video_dims, 33);
   CHECK(dims == 33 && video_dims == 33);
   video_views_ui_leave(&saved, &dims, &video_dims);
   CHECK(dims == 11 && video_dims == 22);
}

int main(void)
{
   test_validate();
   test_frame_helpers();
   test_eye_pair();
   test_select_screens();
   test_blend();
   test_cpu_compose();
   test_cut_source();
   test_touch();
   test_layout_screens();
   test_layout_stereo();
   test_layout_one_screen();
   test_clip();
   test_map_point();
   test_ui_point();
   test_pick_chains();
   test_draw_placements();
   test_targets();
   test_ui_helpers();

   if (failures)
   {
      printf("video_views: FAILED (%d)\n", failures);
      return 1;
   }
   printf("video_views: ok\n");
   return 0;
}
