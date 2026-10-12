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

/* Harness for gfx/video_xr.c: quad placement, swapchain image sizes,
 * recentering, ray hits and quad points mapped back to the core's
 * frame, and the headset pacing maths. Exit 0 on pass, 1 on any
 * failure. */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "gfx/video_xr.h"
#include "gfx/video_defines.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
   printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1e-3)

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

/* 3DS: top eyes side by side, bottom centred below. Frame 800x480. */
static void map_3ds(video_views_map_t *map)
{
   struct retro_video_view v[3];
   v[0] = mkview(  0,   0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
   v[1] = mkview(400,   0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_RIGHT);
   v[2] = mkview(240, 240, 320, 240, 1, RETRO_VIDEO_VIEW_EYE_NONE);
   video_views_validate(v, 3, map);
}

/* DS: two 256x192 screens stacked. Frame 256x384. */
static void map_ds(video_views_map_t *map)
{
   struct retro_video_view v[2];
   v[0] = mkview(0,   0, 256, 192, 0, RETRO_VIDEO_VIEW_EYE_NONE);
   v[1] = mkview(0, 192, 256, 192, 1, RETRO_VIDEO_VIEW_EYE_NONE);
   video_views_validate(v, 2, map);
}

static void params_init(video_xr_params_t *p, const video_views_map_t *map)
{
   memset(p, 0, sizeof(*p));
   p->map        = map;
   video_xr_pose_identity(&p->anchor);
   p->distance   = 1.8f;
   p->width      = 1.6f;
   p->px_per_rad = 1000.0f;
   p->max_dim    = 4096;
}

static int pos_is(const video_xr_quad_t *q, double x, double y, double z)
{
   return NEAR(q->pose.position.x, x)
      && NEAR(q->pose.position.y, y)
      && NEAR(q->pose.position.z, z);
}

static int dims_is(unsigned dims, unsigned w, unsigned h)
{
   return VIDEO_SCALE_W(dims) == w && VIDEO_SCALE_H(dims) == h;
}

static void test_image_dims(void)
{
   /* 1.6 m at 1.8 m spans 0.8364 rad: 837 px at 1000 px/rad. */
   CHECK(dims_is(video_xr_image_dims(1.6f, 0.96f, 1.8f, 1000.0f,
               VIDEO_SCALE_PACK(400, 240), 4096), 837, 502));
   /* Capped, keeping the shape. */
   CHECK(dims_is(video_xr_image_dims(1.6f, 0.96f, 1.8f, 1000.0f,
               VIDEO_SCALE_PACK(400, 240), 500), 500, 300));
   CHECK(dims_is(video_xr_image_dims(1.28f, 0.96f, 1.8f, 1000.0f,
               VIDEO_SCALE_PACK(320, 240), 4096), 684, 513));
   /* Never below the source's own pixels. */
   CHECK(dims_is(video_xr_image_dims(1.6f, 0.96f, 1.8f, 0.0f,
               VIDEO_SCALE_PACK(400, 240), 4096), 400, 240));
   CHECK(dims_is(video_xr_image_dims(1.6f, 0.96f, 1.8f, 200.0f,
               VIDEO_SCALE_PACK(400, 240), 4096), 400, 240));
   /* No floor without a source size. */
   CHECK(dims_is(video_xr_image_dims(1.6f, 0.96f, 1.8f, 200.0f, 0, 4096),
            168, 101));
   CHECK(video_xr_image_dims(0.0f, 0.96f, 1.8f, 1000.0f, 0, 4096) == 0);
}

static void test_shrinks(void)
{
   unsigned top = VIDEO_SCALE_PACK(400, 240);
   /* Smaller in either dimension shrinks; the same size or larger does
    * not. */
   CHECK( video_xr_shrinks(VIDEO_SCALE_PACK(181, 109), top, 0));
   CHECK( video_xr_shrinks(VIDEO_SCALE_PACK(399, 300), top, 0));
   CHECK( video_xr_shrinks(VIDEO_SCALE_PACK(500, 239), top, 0));
   CHECK(!video_xr_shrinks(top, top, 0));
   CHECK(!video_xr_shrinks(VIDEO_SCALE_PACK(437, 262), top, 0));
   /* A quarter turn shows the source's width down the image. */
   CHECK(!video_xr_shrinks(VIDEO_SCALE_PACK(240, 400), top, 1));
   CHECK( video_xr_shrinks(VIDEO_SCALE_PACK(240, 400), top, 0));
   CHECK( video_xr_shrinks(top, top, 3));
   CHECK(!video_xr_shrinks(top, top, 2));
   /* Nothing to shrink, or nothing to shrink into. */
   CHECK(!video_xr_shrinks(VIDEO_SCALE_PACK(181, 109), 0, 0));
   CHECK(!video_xr_shrinks(0, top, 0));
}

static void test_place_3ds(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;

   map_3ds(&map);
   params_init(&p, &map);
   video_xr_place(&p, &set);

   CHECK(set.num_quads == 3);
   /* Screen 0: one image with an eye per layer, one quad per eye. */
   CHECK(set.quads[0].eye == VIDEO_XR_EYE_LEFT  && set.quads[0].layer == 0);
   CHECK(set.quads[1].eye == VIDEO_XR_EYE_RIGHT && set.quads[1].layer == 1);
   CHECK(set.quads[0].slot == 0 && set.quads[1].slot == 0);
   CHECK(set.quads[0].kind == VIDEO_XR_QUAD_SCREEN && set.quads[0].screen == 0);
   CHECK(pos_is(&set.quads[0], 0.0, 0.0, -1.8));
   CHECK(pos_is(&set.quads[1], 0.0, 0.0, -1.8));
   CHECK(NEAR(set.quads[0].width, 1.6) && NEAR(set.quads[0].height, 0.96));
   CHECK(NEAR(set.quads[0].pose.orientation.w, 1.0));
   CHECK(set.slots[0].layers == 2);
   CHECK(set.slots[0].view[0] == 0 && set.slots[0].view[1] == 1);
   CHECK(dims_is(set.slots[0].dims, 837, 502));
   /* Screen 1: 320 of 400 px wide, a 2% gap below screen 0. */
   CHECK(set.quads[2].eye == VIDEO_XR_EYE_BOTH && set.quads[2].slot == 1);
   CHECK(set.quads[2].screen == 1 && set.quads[2].layer == 0);
   CHECK(pos_is(&set.quads[2], 0.0, -0.992, -1.8));
   CHECK(NEAR(set.quads[2].width, 1.28) && NEAR(set.quads[2].height, 0.96));
   CHECK(set.slots[1].layers == 1 && set.slots[1].view[0] == 2);
   CHECK(dims_is(set.slots[1].dims, 684, 513));
   /* Nothing else, and no menu. */
   CHECK(set.slots[2].dims == 0 && set.slots[VIDEO_XR_MENU_SLOT].dims == 0);
}

static void test_place_skip(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;

   map_3ds(&map);
   params_init(&p, &map);
   p.skip_screens = 1u << 0;
   video_xr_place(&p, &set);

   /* Screen 1 where it was beside screen 0, which the projection shows. */
   CHECK(set.num_quads == 1);
   CHECK(set.quads[0].screen == 1 && set.quads[0].slot == 1);
   CHECK(set.quads[0].eye == VIDEO_XR_EYE_BOTH);
   CHECK(pos_is(&set.quads[0], 0.0, -0.992, -1.8));
   CHECK(NEAR(set.quads[0].width, 1.28) && NEAR(set.quads[0].height, 0.96));
   CHECK(set.slots[0].dims == 0 && set.slots[0].layers == 0);
   CHECK(set.slots[1].layers == 1 && set.slots[1].view[0] == 2);
   CHECK(dims_is(set.slots[1].dims, 684, 513));
}

static void test_place_swap_horizontal(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;

   map_3ds(&map);
   params_init(&p, &map);
   p.swap_eyes = true;
   video_xr_place(&p, &set);
   CHECK(set.quads[0].eye == VIDEO_XR_EYE_LEFT  && set.quads[0].layer == 1);
   CHECK(set.quads[1].eye == VIDEO_XR_EYE_RIGHT && set.quads[1].layer == 0);
   /* The image still holds the left view in layer 0. */
   CHECK(set.slots[0].view[0] == 0 && set.slots[0].view[1] == 1);

   params_init(&p, &map);
   p.screen_layout = VIDEO_SCREEN_LAYOUT_HORIZONTAL;
   video_xr_place(&p, &set);
   CHECK(pos_is(&set.quads[2], 1.472, 0.0, -1.8));
}

static void test_place_ds_rotation(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;

   map_ds(&map);
   params_init(&p, &map);
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 2);
   CHECK(set.quads[0].eye == VIDEO_XR_EYE_BOTH && set.quads[1].eye == VIDEO_XR_EYE_BOTH);
   CHECK(NEAR(set.quads[1].width, 1.6) && NEAR(set.quads[1].height, 1.2));
   CHECK(pos_is(&set.quads[1], 0.0, -1.232, -1.8));

   /* A quarter turn swaps each view's display width and height. */
   map_3ds(&map);
   params_init(&p, &map);
   p.rotation = 1;
   video_xr_place(&p, &set);
   CHECK(NEAR(set.quads[0].width, 1.6) && NEAR(set.quads[0].height, 2.6667));
   CHECK(NEAR(set.quads[2].width, 1.6) && NEAR(set.quads[2].height, 2.1333));
}

static void test_place_frame_menu(void)
{
   video_xr_params_t p;
   video_xr_quad_set_t set;

   /* No map: one quad of the whole frame at its display aspect. */
   params_init(&p, NULL);
   p.frame_dims   = VIDEO_SCALE_PACK(800, 480);
   p.frame_aspect = 800.0f / 480.0f;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 1);
   CHECK(set.quads[0].kind == VIDEO_XR_QUAD_FRAME && set.quads[0].slot == 0);
   CHECK(NEAR(set.quads[0].width, 1.6) && NEAR(set.quads[0].height, 0.96));
   CHECK(set.slots[0].layers == 1 && set.slots[0].view[0] == -1);
   CHECK(dims_is(set.slots[0].dims, 837, 502));

   /* The UI floats 10 cm in front of screen 0, as wide, at its size. */
   p.ui_dims = VIDEO_SCALE_PACK(1600, 960);
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 2);
   CHECK(set.quads[1].kind == VIDEO_XR_QUAD_MENU);
   CHECK(set.quads[1].slot == VIDEO_XR_MENU_SLOT);
   CHECK(pos_is(&set.quads[1], 0.0, 0.0, -1.7));
   CHECK(NEAR(set.quads[1].width, 1.6) && NEAR(set.quads[1].height, 0.96));
   CHECK(set.slots[VIDEO_XR_MENU_SLOT].dims == VIDEO_SCALE_PACK(1600, 960));

   /* Nothing to place. */
   p.width = 0.0f;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 0);
}

static void test_place_menu_size(void)
{
   video_xr_params_t p;
   video_xr_quad_set_t set;

   params_init(&p, NULL);
   p.frame_dims = VIDEO_SCALE_PACK(800, 480);

   /* At 200 px/rad the headset shows 176 px across the menu: its image
    * gets twice that, in the UI's shape. */
   p.px_per_rad = 200.0f;
   p.ui_dims    = VIDEO_SCALE_PACK(1600, 960);
   video_xr_place(&p, &set);
   CHECK(set.quads[1].kind == VIDEO_XR_QUAD_MENU);
   CHECK(NEAR(set.quads[1].width, 1.6) && NEAR(set.quads[1].height, 0.96));
   CHECK(dims_is(set.slots[VIDEO_XR_MENU_SLOT].dims, 352, 211));

   /* Wider than a swapchain may be: capped, keeping the shape, with or
    * without a density; a lower density caps it first. */
   p.ui_dims    = VIDEO_SCALE_PACK(5120, 2880);
   p.px_per_rad = 0.0f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[VIDEO_XR_MENU_SLOT].dims, 4096, 2304));
   p.px_per_rad = 3000.0f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[VIDEO_XR_MENU_SLOT].dims, 4096, 2304));
   p.px_per_rad = 1000.0f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[VIDEO_XR_MENU_SLOT].dims, 1760, 990));

   /* Taller than 3:4, as one eye of a side-by-side window: as tall as
    * 3/4 of the screen width allows, narrower, at the UI's own size. */
   p.ui_dims = VIDEO_SCALE_PACK(800, 960);
   video_xr_place(&p, &set);
   CHECK(pos_is(&set.quads[1], 0.0, 0.0, -1.7));
   CHECK(NEAR(set.quads[1].width, 1.0) && NEAR(set.quads[1].height, 1.2));
   CHECK(set.slots[VIDEO_XR_MENU_SLOT].dims == VIDEO_SCALE_PACK(800, 960));
}

static void test_place_stock(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;

   /* Shown larger than the source: the shown size, as with a preset. */
   map_3ds(&map);
   params_init(&p, &map);
   p.stock = true;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[0].dims, 837, 502));
   CHECK(dims_is(set.slots[1].dims, 684, 513));

   /* Shown smaller: the stock chain's images are the shown size, a
    * preset's never below the source's. */
   p.px_per_rad = 200.0f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[0].dims, 168, 101));
   CHECK(dims_is(set.slots[1].dims, 137, 103));
   p.stock = false;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[0].dims, 400, 240));
   CHECK(dims_is(set.slots[1].dims, 320, 240));

   /* Still at most max_dim a side, in its shape. */
   p.stock   = true;
   p.max_dim = 100;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[0].dims, 100, 60));

   /* A whole frame the same; the menu's image as before. */
   params_init(&p, NULL);
   p.stock      = true;
   p.frame_dims = VIDEO_SCALE_PACK(800, 480);
   p.px_per_rad = 200.0f;
   p.ui_dims    = VIDEO_SCALE_PACK(1600, 960);
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[0].dims, 168, 101));
   CHECK(dims_is(set.slots[VIDEO_XR_MENU_SLOT].dims, 352, 211));
   p.stock = false;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[0].dims, 800, 480));
   CHECK(dims_is(set.slots[VIDEO_XR_MENU_SLOT].dims, 352, 211));

   /* Without a density, the source's size either way. */
   p.stock      = true;
   p.px_per_rad = 0.0f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[0].dims, 800, 480));
}

static void test_anchor(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_xr_pose_t head, anchor;
   double s45 = sin(M_PI / 4.0), c45 = cos(M_PI / 4.0);
   double s15 = sin(M_PI / 12.0), c15 = cos(M_PI / 12.0);

   /* Turned 90 degrees left and tilted 30 degrees up: the anchor keeps
    * the position and the heading only. */
   head.position.x    = 0.5f;
   head.position.y    = 0.2f;
   head.position.z    = 0.3f;
   head.orientation.x = (float)(c45 * s15);
   head.orientation.y = (float)(s45 * c15);
   head.orientation.z = (float)(-s45 * s15);
   head.orientation.w = (float)(c45 * c15);
   CHECK(video_xr_anchor_from_head(&head, &anchor));
   CHECK(NEAR(anchor.orientation.x, 0.0) && NEAR(anchor.orientation.z, 0.0));
   CHECK(NEAR(anchor.orientation.y, 0.70711) && NEAR(anchor.orientation.w, 0.70711));
   CHECK(NEAR(anchor.position.x, 0.5) && NEAR(anchor.position.y, 0.2)
         && NEAR(anchor.position.z, 0.3));

   /* Placed from that anchor, screen 0 is 1.8 m to the left. */
   map_3ds(&map);
   params_init(&p, &map);
   p.anchor = anchor;
   video_xr_place(&p, &set);
   CHECK(pos_is(&set.quads[0], -1.3, 0.2, 0.3));
   CHECK(pos_is(&set.quads[2], -1.3, -0.792, 0.3));
   CHECK(NEAR(set.quads[0].pose.orientation.y, 0.70711));

   /* Straight up has no heading. */
   head.orientation.x = (float)s45;
   head.orientation.y = 0.0f;
   head.orientation.z = 0.0f;
   head.orientation.w = (float)c45;
   CHECK(!video_xr_anchor_from_head(&head, &anchor));
}

static void test_ray_hit(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_xr_vec3_t o, d;
   video_xr_quad_t q;
   float u, v, t;

   map_3ds(&map);
   params_init(&p, &map);
   video_xr_place(&p, &set);
   q = set.quads[0];

   o.x = 0.0f; o.y = 0.0f; o.z = 0.0f;
   d.x = 0.0f; d.y = 0.0f; d.z = -1.0f;
   CHECK(video_xr_ray_hit(&q, &o, &d, &u, &v, &t));
   CHECK(NEAR(u, 0.5) && NEAR(v, 0.5) && NEAR(t, 1.8));

   /* The top-right corner is on the quad. */
   d.x = 0.8f; d.y = 0.48f; d.z = -1.8f;
   CHECK(video_xr_ray_hit(&q, &o, &d, &u, &v, &t));
   CHECK(NEAR(u, 1.0) && NEAR(v, 0.0));

   d.x = -0.4f; d.y = -0.24f; d.z = -1.8f;
   CHECK(video_xr_ray_hit(&q, &o, &d, &u, &v, &t));
   CHECK(NEAR(u, 0.25) && NEAR(v, 0.75));

   /* Beside it, pointing away, and from behind. */
   d.x = 1.0f; d.y = 0.0f; d.z = -1.8f;
   CHECK(!video_xr_ray_hit(&q, &o, &d, &u, &v, &t));
   d.x = 0.0f; d.y = 0.0f; d.z = 1.0f;
   CHECK(!video_xr_ray_hit(&q, &o, &d, &u, &v, &t));
   o.z = -3.0f;
   CHECK(!video_xr_ray_hit(&q, &o, &d, &u, &v, &t));

   /* A quad turned to face +X, 1.8 m to the left. Its right edge is
    * towards -Z. */
   video_xr_pose_identity(&p.anchor);
   p.anchor.orientation.y = 0.70711f;
   p.anchor.orientation.w = 0.70711f;
   video_xr_place(&p, &set);
   q   = set.quads[0];
   o.x = 0.0f; o.y = 0.0f; o.z = 0.0f;
   d.x = -1.0f; d.y = 0.0f; d.z = 0.0f;
   CHECK(video_xr_ray_hit(&q, &o, &d, &u, &v, &t));
   CHECK(NEAR(u, 0.5) && NEAR(v, 0.5) && NEAR(t, 1.8));
   d.x = -1.8f; d.y = 0.0f; d.z = -0.4f;
   CHECK(video_xr_ray_hit(&q, &o, &d, &u, &v, &t));
   CHECK(NEAR(u, 0.75) && NEAR(v, 0.5));
}

static void test_quad_to_frame(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   int16_t x, y;
   unsigned frame = VIDEO_SCALE_PACK(800, 480);

   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims = VIDEO_SCALE_PACK(1600, 960);
   video_xr_place(&p, &set);

   /* The right eye's copy lands in the left view, at the same point. */
   CHECK(video_xr_quad_to_frame(&set.quads[1], 0.5f, 0.5f, &map, frame, &x, &y));
   CHECK(x == -16364 && y == -16351);
   CHECK(video_xr_quad_to_frame(&set.quads[0], 0.5f, 0.5f, &map, frame, &x, &y));
   CHECK(x == -16364 && y == -16351);
   /* The bottom screen's corners. */
   CHECK(video_xr_quad_to_frame(&set.quads[2], 0.0f, 0.0f, &map, frame, &x, &y));
   CHECK(x == -13083 && y == 67);
   CHECK(video_xr_quad_to_frame(&set.quads[2], 1.0f, 1.0f, &map, frame, &x, &y));
   CHECK(x == 13081 && y == 32767);
   /* The menu is not the core's. */
   CHECK(!video_xr_quad_to_frame(&set.quads[3], 0.5f, 0.5f, &map, frame, &x, &y));

   /* A whole frame spreads over the frame. */
   params_init(&p, NULL);
   p.frame_dims = frame;
   video_xr_place(&p, &set);
   CHECK(video_xr_quad_to_frame(&set.quads[0], 0.25f, 0.75f, NULL, frame, &x, &y));
   CHECK(x == -16364 && y == 16485);
}

static void test_live(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;

   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims = VIDEO_SCALE_PACK(1600, 960);
   video_xr_place(&p, &set);
   /* 0 and 1 are screen 0's eyes, 2 is screen 1, 3 the menu. */
   CHECK(!video_xr_quad_live(&set.quads[0], VIDEO_OPENXR_LASER_AUTO, true));
   CHECK( video_xr_quad_live(&set.quads[0], VIDEO_OPENXR_LASER_ALWAYS, false));
   CHECK(!video_xr_quad_live(&set.quads[0], VIDEO_OPENXR_LASER_OFF, false));
   CHECK( video_xr_quad_live(&set.quads[2], VIDEO_OPENXR_LASER_AUTO, false));
   CHECK( video_xr_quad_live(&set.quads[2], VIDEO_OPENXR_LASER_ALWAYS, false));
   CHECK(!video_xr_quad_live(&set.quads[2], VIDEO_OPENXR_LASER_OFF, false));
   CHECK( video_xr_quad_live(&set.quads[3], VIDEO_OPENXR_LASER_AUTO, true));
   CHECK(!video_xr_quad_live(&set.quads[3], VIDEO_OPENXR_LASER_AUTO, false));
   CHECK( video_xr_quad_live(&set.quads[3], VIDEO_OPENXR_LASER_ALWAYS, true));
   CHECK(!video_xr_quad_live(&set.quads[3], VIDEO_OPENXR_LASER_ALWAYS, false));
   CHECK(!video_xr_quad_live(&set.quads[3], VIDEO_OPENXR_LASER_OFF, true));

   /* A whole frame is screen 0. */
   params_init(&p, NULL);
   p.frame_dims = VIDEO_SCALE_PACK(800, 480);
   video_xr_place(&p, &set);
   CHECK(!video_xr_quad_live(&set.quads[0], VIDEO_OPENXR_LASER_AUTO, false));
   CHECK( video_xr_quad_live(&set.quads[0], VIDEO_OPENXR_LASER_ALWAYS, false));
}

static void test_pick(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_xr_vec3_t o, d;
   float u, v, t;

   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims = VIDEO_SCALE_PACK(1600, 960);
   video_xr_place(&p, &set);
   o.x = 0.0f; o.y = 0.0f; o.z = 0.0f;

   /* Screen 0 in Auto, the menu closed though its quad is up: nothing. */
   d.x = 0.0f; d.y = 0.0f; d.z = -1.0f;
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_AUTO, false,
            &o, &d, &u, &v, &t) == -1);
   /* The bottom screen. */
   d.x = -0.32f; d.y = -1.232f; d.z = -1.8f;
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_AUTO, false,
            &o, &d, &u, &v, &t) == 2);
   CHECK(NEAR(u, 0.25) && NEAR(v, 0.75) && NEAR(t, 1.0));
   /* The open menu, in front of screen 0. */
   d.x = 0.4f; d.y = 0.24f; d.z = -1.7f;
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_AUTO, true,
            &o, &d, &u, &v, &t) == 3);
   CHECK(NEAR(u, 0.75) && NEAR(v, 0.25) && NEAR(t, 1.0));
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_ALWAYS, true,
            &o, &d, &u, &v, &t) == 3);
   /* Closed, the ray goes through it to screen 0's left eye. */
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_ALWAYS, false,
            &o, &d, &u, &v, &t) == 0);
   CHECK(NEAR(u, 0.76471) && NEAR(v, 0.23529) && NEAR(t, 1.05882));
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_OFF, true,
            &o, &d, &u, &v, &t) == -1);
   /* Beside every screen. */
   d.x = 3.0f; d.y = 0.0f; d.z = -1.8f;
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_ALWAYS, true,
            &o, &d, &u, &v, &t) == -1);

   /* A whole frame: only Always points at it. */
   params_init(&p, NULL);
   p.frame_dims = VIDEO_SCALE_PACK(800, 480);
   video_xr_place(&p, &set);
   d.x = 0.0f; d.y = 0.0f; d.z = -1.0f;
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_AUTO, false,
            &o, &d, &u, &v, &t) == -1);
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_ALWAYS, false,
            &o, &d, &u, &v, &t) == 0);
   CHECK(NEAR(u, 0.5) && NEAR(v, 0.5) && NEAR(t, 1.8));
}

static void test_window(void)
{
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_xr_vec3_t o, d;
   float u, v, t;
   int16_t x, y;
   unsigned frame = VIDEO_SCALE_PACK(800, 480);

   params_init(&p, NULL);
   p.frame_dims = frame;
   p.window     = true;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 1 && set.quads[0].kind == VIDEO_XR_QUAD_WINDOW);

   /* The menu's while it is open; otherwise a whole frame. */
   CHECK( video_xr_quad_live(&set.quads[0], VIDEO_OPENXR_LASER_AUTO, true));
   CHECK(!video_xr_quad_live(&set.quads[0], VIDEO_OPENXR_LASER_AUTO, false));
   CHECK( video_xr_quad_live(&set.quads[0], VIDEO_OPENXR_LASER_ALWAYS, false));
   CHECK(!video_xr_quad_live(&set.quads[0], VIDEO_OPENXR_LASER_OFF, true));
   o.x = 0.0f; o.y = 0.0f; o.z = 0.0f;
   d.x = 0.0f; d.y = 0.0f; d.z = -1.0f;
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_AUTO, true,
            &o, &d, &u, &v, &t) == 0);
   CHECK(NEAR(u, 0.5) && NEAR(v, 0.5));
   CHECK(video_xr_quad_to_frame(&set.quads[0], 0.25f, 0.75f, NULL, frame,
            &x, &y));
   CHECK(x == -16364 && y == 16485);
}

/* A 256x224 core at 3x in a 1280x720 window: 768x672 at 256,24, bars
 * on every side. */
static void test_window_point(void)
{
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_viewport_t vp;
   float u, v;
   int16_t x, y;

   params_init(&p, NULL);
   p.frame_dims = VIDEO_SCALE_PACK(1280, 720);
   p.window     = true;
   video_xr_place(&p, &set);
   vp.pos       = VIDEO_POS_PACK(256, 24);
   vp.dims      = VIDEO_SCALE_PACK(768, 672);
   vp.full_dims = VIDEO_SCALE_PACK(1280, 720);

   /* Each bar hits no frame pixel. */
   u = 0.1f; v = 0.5f;
   CHECK(!video_xr_window_point(&vp, &u, &v));
   u = 0.9f; v = 0.5f;
   CHECK(!video_xr_window_point(&vp, &u, &v));
   u = 0.5f; v = 0.02f;
   CHECK(!video_xr_window_point(&vp, &u, &v));
   u = 0.5f; v = 0.98f;
   CHECK(!video_xr_window_point(&vp, &u, &v));

   /* The drawn area's corners and middle. */
   u = 0.2f; v = 24.0f / 720.0f;
   CHECK(video_xr_window_point(&vp, &u, &v) && NEAR(u, 0.0) && NEAR(v, 0.0));
   u = 0.8f; v = 696.0f / 720.0f;
   CHECK(video_xr_window_point(&vp, &u, &v) && NEAR(u, 1.0) && NEAR(v, 1.0));
   u = 0.5f; v = 0.5f;
   CHECK(video_xr_window_point(&vp, &u, &v) && NEAR(u, 0.5) && NEAR(v, 0.5));

   /* The middle of window pixel 449,361 is frame pixel 64,112. */
   u = 449.5f / 1280.0f; v = 361.5f / 720.0f;
   CHECK(video_xr_window_point(&vp, &u, &v));
   CHECK(video_xr_quad_to_frame(&set.quads[0], u, v, NULL,
            VIDEO_SCALE_PACK(256, 224), &x, &y));
   CHECK(x == -16320 && y == 146);

   /* An 8-pixel bar above, 40 below: the viewport is from the top. */
   vp.pos = VIDEO_POS_PACK(256, 8);
   u = 0.5f; v = 20.0f / 720.0f;
   CHECK(video_xr_window_point(&vp, &u, &v) && NEAR(v, 12.0 / 672.0));
   u = 0.5f; v = 4.0f / 720.0f;
   CHECK(!video_xr_window_point(&vp, &u, &v));

   /* Wider than the window, cropped at both sides. */
   vp.pos  = VIDEO_POS_PACK(-100, 0);
   vp.dims = VIDEO_SCALE_PACK(1480, 720);
   u = 0.0f; v = 0.0f;
   CHECK(video_xr_window_point(&vp, &u, &v)
         && NEAR(u, 100.0 / 1480.0) && NEAR(v, 0.0));

   /* No viewport, no frame. */
   vp.dims = 0;
   u = 0.5f; v = 0.5f;
   CHECK(!video_xr_window_point(&vp, &u, &v));
}

/* VB's stereo pair side by side. Frame 768x224. */
static void map_vb(video_views_map_t *map)
{
   struct retro_video_view v[2];
   v[0] = mkview(  0, 0, 384, 224, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
   v[1] = mkview(384, 0, 384, 224, 0, RETRO_VIDEO_VIEW_EYE_RIGHT);
   video_views_validate(v, 2, map);
}

static void vp_set(video_viewport_t *vp, int x, int y, unsigned w,
      unsigned h)
{
   vp->pos       = VIDEO_POS_PACK(x, y);
   vp->dims      = VIDEO_SCALE_PACK(w, h);
   vp->full_dims = VIDEO_SCALE_PACK(1280, 720);
}

static int rect_is(const video_views_rect_t *r, int x, int y,
      unsigned w, unsigned h)
{
   return VIDEO_POS_X(r->pos) == x && VIDEO_POS_Y(r->pos) == y
      && dims_is(r->dims, w, h);
}

static int quad_is(const video_xr_quad_t *q, unsigned kind, unsigned eye,
      unsigned slot, unsigned layer)
{
   return q->kind == kind && q->eye == eye && q->slot == slot
      && q->layer == layer;
}

static void window_params(video_xr_params_t *p, const video_views_map_t *map)
{
   params_init(p, map);
   p->frame_dims = VIDEO_SCALE_PACK(1280, 720);
   p->window     = true;
}

static void test_window_views_stereo(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_viewport_t vp;
   video_views_rect_t rects[VIDEO_XR_MAX_SLOTS][2];

   /* Letterboxed: each eye's image is its half of the window's frame. */
   map_vb(&map);
   window_params(&p, &map);
   vp_set(&vp, 0, 173, 1280, 374);
   video_xr_place_window(&p, false, false, &vp,
         VIDEO_SCALE_PACK(768, 224), &set, rects);
   CHECK(set.num_quads == 2);
   CHECK(quad_is(&set.quads[0], VIDEO_XR_QUAD_SCREEN, VIDEO_XR_EYE_LEFT,
            0, 0));
   CHECK(quad_is(&set.quads[1], VIDEO_XR_QUAD_SCREEN, VIDEO_XR_EYE_RIGHT,
            0, 1));
   CHECK(set.slots[0].layers == 2);
   CHECK(set.slots[0].view[0] == 0 && set.slots[0].view[1] == 1);
   CHECK(rect_is(&rects[0][0],   0, 173, 640, 374));
   CHECK(rect_is(&rects[0][1], 640, 173, 640, 374));

   /* Tracked: its eyes are the projection's, the window stays whole. */
   video_xr_place_window(&p, false, true, &vp,
         VIDEO_SCALE_PACK(768, 224), &set, rects);
   CHECK(set.num_quads == 1);
   CHECK(quad_is(&set.quads[0], VIDEO_XR_QUAD_WINDOW, VIDEO_XR_EYE_BOTH,
            0, 0));
   CHECK(set.slots[0].layers == 1 && set.slots[0].view[0] == -1);
   CHECK(rect_is(&rects[0][0], 0, 0, 1280, 720));
   /* And once the projection shows it, nothing. */
   p.width = 0.0f;
   video_xr_place_window(&p, false, true, &vp,
         VIDEO_SCALE_PACK(768, 224), &set, rects);
   CHECK(set.num_quads == 0);

   /* No map: the window whole, as before. */
   window_params(&p, NULL);
   video_xr_place_window(&p, false, false, &vp,
         VIDEO_SCALE_PACK(768, 224), &set, rects);
   CHECK(set.num_quads == 1);
   CHECK(quad_is(&set.quads[0], VIDEO_XR_QUAD_WINDOW, VIDEO_XR_EYE_BOTH,
            0, 0));
   CHECK(rect_is(&rects[0][0], 0, 0, 1280, 720));
}

static void test_window_views_3ds(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_viewport_t vp;
   video_views_rect_t rects[VIDEO_XR_MAX_SLOTS][2];
   video_xr_quad_t win;
   int16_t sx, sy, wx, wy;
   float u, v;

   /* Pillarboxed and letterboxed, 800x480 at 1.35: the top pair per
    * eye, the bottom screen to both, below it. */
   map_3ds(&map);
   window_params(&p, &map);
   vp_set(&vp, 100, 24, 1080, 648);
   video_xr_place_window(&p, false, false, &vp,
         VIDEO_SCALE_PACK(800, 480), &set, rects);
   CHECK(set.num_quads == 3);
   CHECK(quad_is(&set.quads[0], VIDEO_XR_QUAD_SCREEN, VIDEO_XR_EYE_LEFT,
            0, 0));
   CHECK(quad_is(&set.quads[1], VIDEO_XR_QUAD_SCREEN, VIDEO_XR_EYE_RIGHT,
            0, 1));
   CHECK(quad_is(&set.quads[2], VIDEO_XR_QUAD_SCREEN, VIDEO_XR_EYE_BOTH,
            1, 0));
   CHECK(set.quads[2].screen == 1 && set.quads[2].pose.position.y < 0.0f);
   CHECK(set.slots[1].layers == 1 && set.slots[1].view[0] == 2);
   CHECK(rect_is(&rects[0][0], 100,  24, 540, 324));
   CHECK(rect_is(&rects[0][1], 640,  24, 540, 324));
   CHECK(rect_is(&rects[1][0], 424, 348, 432, 324));

   /* The laser on the bottom screen hits the frame pixel the window
    * shows there: frame pixel 336,360. */
   u = 96.5f / 320.0f;
   v = 120.5f / 240.0f;
   CHECK(video_xr_quad_to_frame(&set.quads[2], u, v, &map,
            VIDEO_SCALE_PACK(800, 480), &sx, &sy));
   u = (424.0f + u * 432.0f) / 1280.0f;
   v = (348.0f + v * 324.0f) / 720.0f;
   CHECK(video_xr_window_point(&vp, &u, &v));
   memset(&win, 0, sizeof(win));
   win.kind = VIDEO_XR_QUAD_WINDOW;
   CHECK(video_xr_quad_to_frame(&win, u, v, &map,
            VIDEO_SCALE_PACK(800, 480), &wx, &wy));
   CHECK(sx == wx && sy == wy);

   /* The menu is drawn over the window: it shows whole. */
   video_xr_place_window(&p, true, false, &vp,
         VIDEO_SCALE_PACK(800, 480), &set, rects);
   CHECK(set.num_quads == 1);
   CHECK(quad_is(&set.quads[0], VIDEO_XR_QUAD_WINDOW, VIDEO_XR_EYE_BOTH,
            0, 0));
   CHECK(set.slots[0].layers == 1 && !set.slots[1].dims);
   CHECK(rect_is(&rects[0][0], 0, 0, 1280, 720));
}

static void test_window_rect_rotation(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_viewport_t vp;
   video_views_rect_t rects[VIDEO_XR_MAX_SLOTS][2];
   video_views_rect_t r;

   /* DS turned a quarter anticlockwise: the top screen on the left. */
   map_ds(&map);
   window_params(&p, &map);
   p.rotation = 1;
   vp_set(&vp, 64, 0, 1152, 768);
   vp.full_dims = VIDEO_SCALE_PACK(1280, 768);
   video_xr_place_window(&p, false, false, &vp,
         VIDEO_SCALE_PACK(256, 384), &set, rects);
   CHECK(set.num_quads == 2);
   CHECK(rect_is(&rects[0][0],  64, 0, 576, 768));
   CHECK(rect_is(&rects[1][0], 640, 0, 576, 768));
   /* The screens' shapes are the rects'. */
   CHECK(NEAR(set.quads[0].width / set.quads[0].height, 576.0 / 768.0));

   /* Three quarters: on the right. */
   video_xr_window_rect(&vp, VIDEO_SCALE_PACK(256, 384), 3,
         &map.views[0], &r);
   CHECK(rect_is(&r, 640, 0, 576, 768));
   /* A half: upside down, the top screen below. */
   video_xr_window_rect(&vp, VIDEO_SCALE_PACK(256, 384), 2,
         &map.views[0], &r);
   CHECK(rect_is(&r, 64, 384, 1152, 384));

   /* No frame size, nowhere. */
   video_xr_window_rect(&vp, 0, 0, &map.views[1], &r);
   CHECK(r.pos == 0 && r.dims == 0);
}

static void test_cursor(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_xr_pose_t pose;
   float w;

   map_3ds(&map);
   params_init(&p, &map);
   video_xr_place(&p, &set);
   /* On the bottom screen, 1.2% of the ray's length wide. */
   w = video_xr_cursor(&set.quads[2], 0.25f, 0.75f, 2.2f, &pose);
   CHECK(NEAR(w, 0.0264));
   CHECK(NEAR(pose.position.x, -0.32) && NEAR(pose.position.y, -1.232)
         && NEAR(pose.position.z, -1.798));
   CHECK(NEAR(pose.orientation.w, 1.0));
   /* Never smaller than 5 mm. */
   CHECK(NEAR(video_xr_cursor(&set.quads[2], 0.5f, 0.5f, 0.1f, &pose), 0.005));

   /* A quad turned to face +X: u 0.75 is towards -Z, lifted along +X. */
   video_xr_pose_identity(&p.anchor);
   p.anchor.orientation.y = 0.70711f;
   p.anchor.orientation.w = 0.70711f;
   video_xr_place(&p, &set);
   video_xr_cursor(&set.quads[0], 0.75f, 0.5f, 1.8f, &pose);
   CHECK(NEAR(pose.position.x, -1.798) && NEAR(pose.position.y, 0.0)
         && NEAR(pose.position.z, -0.4));
   CHECK(NEAR(pose.orientation.y, 0.70711));
}

static void test_cursor_pixels(void)
{
   static uint8_t px[VIDEO_XR_CURSOR_DIM * VIDEO_XR_CURSOR_DIM * 4];
   const uint8_t *centre = px + ((size_t)(VIDEO_XR_CURSOR_DIM / 2)
         * VIDEO_XR_CURSOR_DIM + VIDEO_XR_CURSOR_DIM / 2) * 4;
   const uint8_t *rim    = px + ((size_t)2 * VIDEO_XR_CURSOR_DIM
         + VIDEO_XR_CURSOR_DIM / 2) * 4;

   video_xr_cursor_pixels(px);
   /* White in the middle, a dark rim, nothing in the corners; grey, so
    * RGBA and BGRA are the same bytes. */
   CHECK(centre[0] == 255 && centre[1] == 255 && centre[2] == 255
         && centre[3] == 255);
   CHECK(rim[0] == 38 && rim[1] == 38 && rim[2] == 38 && rim[3] == 255);
   CHECK(!px[0] && !px[1] && !px[2] && !px[3]);
}

static int pose_same(const video_xr_pose_t *a, const video_xr_pose_t *b)
{
   return a->orientation.x == b->orientation.x
      && a->orientation.y == b->orientation.y
      && a->orientation.z == b->orientation.z
      && a->orientation.w == b->orientation.w
      && a->position.x    == b->position.x
      && a->position.y    == b->position.y
      && a->position.z    == b->position.z;
}

static void test_compose(void)
{
   unsigned i;
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t at, from;
   video_xr_pose_t a, b, out;

   /* Screen 0's place in front of a head turned a quarter left. */
   video_xr_pose_identity(&a);
   a.orientation.y = 0.70711f;
   a.orientation.w = 0.70711f;
   a.position.x    = 0.5f;
   a.position.y    = 0.2f;
   a.position.z    = 0.3f;
   video_xr_pose_identity(&b);
   b.position.z    = -1.8f;
   video_xr_compose(&a, &b, &out);
   CHECK(NEAR(out.position.x, -1.3) && NEAR(out.position.y, 0.2)
         && NEAR(out.position.z, 0.3));
   CHECK(NEAR(out.orientation.y, 0.70711) && NEAR(out.orientation.w, 0.70711));
   video_xr_compose(&a, &b, &b);
   CHECK(pose_same(&b, &out));

   /* Placed from an identity anchor and composed with a, a 3DS layout
    * and its menu are exactly where placing them at a puts them. */
   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims = VIDEO_SCALE_PACK(1600, 960);
   p.anchor  = a;
   video_xr_place(&p, &at);
   video_xr_pose_identity(&p.anchor);
   video_xr_place(&p, &from);
   CHECK(at.num_quads == 4 && from.num_quads == 4);
   for (i = 0; i < from.num_quads; i++)
   {
      video_xr_compose(&a, &from.quads[i].pose, &out);
      CHECK(pose_same(&out, &at.quads[i].pose));
   }
}

static void test_lazy_follow(void)
{
   unsigned i;
   bool glide;
   video_xr_pose_t a, t;
   /* Half angles: 20, 90 and 170 degrees. */
   double s10 = sin(M_PI / 18.0), c10 = cos(M_PI / 18.0);
   double s45 = sin(M_PI / 4.0),  c45 = cos(M_PI / 4.0);
   double s85 = sin(M_PI * 85.0 / 180.0), c85 = cos(M_PI * 85.0 / 180.0);

   /* Turned 20 degrees, or 0.3 m away: they stay. */
   video_xr_pose_identity(&a);
   video_xr_pose_identity(&t);
   t.orientation.y = (float)s10;
   t.orientation.w = (float)c10;
   CHECK(!video_xr_lazy_follow(&a, &t, 0.0139, false));
   CHECK(NEAR(a.orientation.y, 0.0) && NEAR(a.orientation.w, 1.0));
   video_xr_pose_identity(&t);
   t.position.x = 0.3f;
   CHECK(!video_xr_lazy_follow(&a, &t, 0.0139, false));
   CHECK(NEAR(a.position.x, 0.0));

   /* 0.6 m away: a step towards it. */
   t.position.x = 0.6f;
   CHECK(video_xr_lazy_follow(&a, &t, 0.0139, false));
   CHECK(a.position.x > 0.0f && a.position.x < 0.1f);

   /* Turned 90 degrees: a step at a time, then exactly in front. */
   video_xr_pose_identity(&a);
   video_xr_pose_identity(&t);
   t.orientation.y = (float)s45;
   t.orientation.w = (float)c45;
   CHECK(video_xr_lazy_follow(&a, &t, 0.0139, false));
   CHECK(a.orientation.y > 0.0f && a.orientation.y < 0.1f);
   glide = true;
   for (i = 0; i < 200 && glide; i++)
      glide = video_xr_lazy_follow(&a, &t, 0.0139, true);
   CHECK(!glide);
   CHECK(NEAR(a.orientation.y, s45) && NEAR(a.orientation.w, c45));

   /* Once gliding, a turn under the start goes on to the end. */
   video_xr_pose_identity(&a);
   video_xr_pose_identity(&t);
   t.orientation.y = (float)s10;
   t.orientation.w = (float)c10;
   CHECK(video_xr_lazy_follow(&a, &t, 0.0139, true));

   /* The short way round: 170 to -170 degrees is 20. */
   video_xr_pose_identity(&a);
   a.orientation.y = (float)s85;
   a.orientation.w = (float)c85;
   video_xr_pose_identity(&t);
   t.orientation.y = (float)-s85;
   t.orientation.w = (float)c85;
   CHECK(!video_xr_lazy_follow(&a, &t, 0.0139, false));
}

/* A target turned deg about y and moved x along it. */
static void lazy_target(video_xr_pose_t *t, double deg, double x)
{
   video_xr_pose_identity(t);
   t->orientation.y = (float)sin(deg * M_PI / 360.0);
   t->orientation.w = (float)cos(deg * M_PI / 360.0);
   t->position.x    = (float)x;
}

/* Either side of each threshold. */
static void test_lazy_edges(void)
{
   video_xr_pose_t a, t;

   /* Resting: a turn or a step just under the start stays, just over
    * glides. */
   video_xr_pose_identity(&a);
   lazy_target(&t, VIDEO_XR_LAZY_START_DEG - 2.0, 0.0);
   CHECK(!video_xr_lazy_follow(&a, &t, 0.0139, false));
   lazy_target(&t, VIDEO_XR_LAZY_START_DEG + 2.0, 0.0);
   CHECK(video_xr_lazy_follow(&a, &t, 0.0139, false));
   video_xr_pose_identity(&a);
   lazy_target(&t, 0.0, VIDEO_XR_LAZY_START_M - 0.02);
   CHECK(!video_xr_lazy_follow(&a, &t, 0.0139, false));
   lazy_target(&t, 0.0, VIDEO_XR_LAZY_START_M + 0.02);
   CHECK(video_xr_lazy_follow(&a, &t, 0.0139, false));

   /* Gliding: within both stops it lands on the target; past either it
    * glides on. */
   video_xr_pose_identity(&a);
   lazy_target(&t, VIDEO_XR_LAZY_STOP_DEG - 0.2, VIDEO_XR_LAZY_STOP_M - 0.002);
   CHECK(!video_xr_lazy_follow(&a, &t, 0.0139, true));
   CHECK(   NEAR(a.orientation.y, t.orientation.y)
         && NEAR(a.orientation.w, t.orientation.w)
         && NEAR(a.position.x, t.position.x));
   video_xr_pose_identity(&a);
   lazy_target(&t, VIDEO_XR_LAZY_STOP_DEG + 0.2, VIDEO_XR_LAZY_STOP_M - 0.002);
   CHECK(video_xr_lazy_follow(&a, &t, 0.0139, true));
   video_xr_pose_identity(&a);
   lazy_target(&t, VIDEO_XR_LAZY_STOP_DEG - 0.2, VIDEO_XR_LAZY_STOP_M + 0.002);
   CHECK(video_xr_lazy_follow(&a, &t, 0.0139, true));
}

/* A 120 Hz and a 72 Hz headset's predicted display periods, in ns. */
#define P120 8333333
#define P72  13888889

/* How many times n frames of period changed the published one. */
static unsigned feed(video_xr_period_t *f, int64_t period, unsigned n)
{
   unsigned i;
   unsigned changes = 0;
   for (i = 0; i < n; i++)
      if (video_xr_period_add(f, period))
         changes++;
   return changes;
}

static void test_period_settle(void)
{
   video_xr_period_t f;
   video_xr_period_init(&f);
   CHECK(feed(&f, P120, 15) == 0);
   CHECK(f.published == 0);
   CHECK(feed(&f, P120, 1) == 1);
   CHECK(f.published == P120);
   CHECK(feed(&f, P120, 40) == 0);
   /* Nonsense periods are ignored. */
   CHECK(!video_xr_period_add(&f, 0));
   CHECK(!video_xr_period_add(&f, -5));
   CHECK(!video_xr_period_add(&f, (int64_t)VIDEO_XR_PERIOD_MAX_NS + 1));
   CHECK(f.published == P120);
}

static void test_period_jitter(void)
{
   video_xr_period_t f;
   unsigned i;
   unsigned changes = 0;
   video_xr_period_init(&f);
   /* 0.4% either way settles on the median. */
   for (i = 0; i < VIDEO_XR_PERIODS; i++)
      if (video_xr_period_add(&f, P120 + ((i & 1) ? 33000 : -33000)))
         changes++;
   CHECK(changes == 1);
   CHECK(f.published == P120);
   /* A stall among agreeing frames publishes nothing... */
   CHECK(!video_xr_period_add(&f, 16666667));
   CHECK(feed(&f, P120, 15) == 0);
   /* ...nor does a steady drift inside 1%. */
   CHECK(feed(&f, P120 + 41667, 32) == 0);
   CHECK(f.published == P120);
}

static void test_period_change(void)
{
   video_xr_period_t f;
   video_xr_period_init(&f);
   CHECK(feed(&f, P72, 16) == 1);
   CHECK(f.published == P72);
   /* The user picks 120 Hz: a mixed window publishes nothing. */
   CHECK(feed(&f, P120, 15) == 0);
   CHECK(f.published == P72);
   CHECK(feed(&f, P120, 1) == 1);
   CHECK(f.published == P120);
}

static void test_pace_interval(void)
{
   CHECK(video_xr_pace_interval(120.0f, 60.0988f, 0.05f, 16) == 2);
   CHECK(video_xr_pace_interval(120.0f, 60.0f, 0.05f, 16) == 2);
   CHECK(video_xr_pace_interval(72.0f, 60.0f, 0.05f, 16) == 0);
   CHECK(video_xr_pace_interval(90.0f, 60.0f, 0.05f, 16) == 0);
   CHECK(video_xr_pace_interval(144.0f, 48.0f, 0.05f, 16) == 3);
   CHECK(video_xr_pace_interval(60.0f, 59.94f, 0.05f, 16) == 1);
   /* The end-to-end cases: Monado's 20 Hz and the layer's 10 Hz. */
   CHECK(video_xr_pace_interval(20.0f, 10.0f, 0.05f, 16) == 2);
   CHECK(video_xr_pace_interval(10.0f, 10.0f, 0.05f, 16) == 1);
   CHECK(video_xr_pace_interval(20.0f, 16.0f, 0.05f, 16) == 0);
   CHECK(video_xr_pace_interval(20.0f, 60.0f, 0.05f, 16) == 0);
   /* Past the ceiling the rule falls back to 1, which doesn't fit. */
   CHECK(video_xr_pace_interval(240.0f, 10.0f, 0.05f, 16) == 0);
   CHECK(video_xr_pace_interval(0.0f, 60.0f, 0.05f, 16) == 0);
   CHECK(video_xr_pace_interval(120.0f, 0.0f, 0.05f, 16) == 0);
}

static void test_pick_rate(void)
{
   static const float quest[4]  = { 72.0f, 90.0f, 120.0f, 144.0f };
   static const float frame[1]  = { 120.0f };
   static const float only72[1] = { 72.0f };
   static const float tie[3]    = { 60.0f, 90.0f, 120.0f };
   static const float e2e[2]    = { 20.0f, 10.0f };
   CHECK(NEAR(video_xr_pick_rate(quest, 4, 60.0988f, 16), 120.0));
   /* 50 fps: 144 / 3 = 48 is the closest. */
   CHECK(NEAR(video_xr_pick_rate(quest, 4, 50.0f, 16), 144.0));
   CHECK(NEAR(video_xr_pick_rate(frame, 1, 60.0f, 16), 120.0));
   /* The best on offer, even when it doesn't fit. */
   CHECK(NEAR(video_xr_pick_rate(only72, 1, 60.0f, 16), 72.0));
   CHECK(NEAR(video_xr_pick_rate(tie, 3, 30.0f, 16), 120.0));
   CHECK(NEAR(video_xr_pick_rate(e2e, 2, 10.0f, 16), 20.0));
   CHECK(NEAR(video_xr_pick_rate(quest, 0, 60.0f, 16), 0.0));
   CHECK(NEAR(video_xr_pick_rate(quest, 4, 0.0f, 16), 0.0));
}

static void test_request_rate(void)
{
   static const float quest[4] = { 72.0f, 90.0f, 120.0f, 144.0f };
   static const float e2e[2]   = { 20.0f, 10.0f };
   CHECK(NEAR(video_xr_request_rate(VIDEO_OPENXR_REFRESH_AUTO,
               quest, 4, 60.0f, 16), 120.0));
   CHECK(NEAR(video_xr_request_rate(VIDEO_OPENXR_REFRESH_AUTO,
               quest, 0, 60.0f, 16), 0.0));
   CHECK(NEAR(video_xr_request_rate(VIDEO_OPENXR_REFRESH_HEADSET,
               quest, 4, 60.0f, 16), 0.0));
   CHECK(NEAR(video_xr_request_rate(90, quest, 4, 60.0f, 16), 90.0));
   CHECK(NEAR(video_xr_request_rate(100, quest, 4, 60.0f, 16), 0.0));
   CHECK(NEAR(video_xr_request_rate(10, e2e, 2, 10.0f, 16), 10.0));
}

int main(void)
{
   test_image_dims();
   test_shrinks();
   test_place_3ds();
   test_place_skip();
   test_place_swap_horizontal();
   test_place_ds_rotation();
   test_place_frame_menu();
   test_place_menu_size();
   test_place_stock();
   test_anchor();
   test_ray_hit();
   test_quad_to_frame();
   test_live();
   test_pick();
   test_window();
   test_window_point();
   test_window_views_stereo();
   test_window_views_3ds();
   test_window_rect_rotation();
   test_cursor();
   test_cursor_pixels();
   test_compose();
   test_lazy_follow();
   test_lazy_edges();
   test_period_settle();
   test_period_jitter();
   test_period_change();
   test_pace_interval();
   test_pick_rate();
   test_request_rate();

   if (failures)
   {
      printf("video_xr: FAILED (%d)\n", failures);
      return 1;
   }
   printf("video_xr: ok\n");
   return 0;
}
