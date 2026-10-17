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

static void test_screen_defaults(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t base, set;
   video_screen_xr_t screens[RETRO_VIDEO_VIEWS_MAX];
   unsigned layout, rotation, window;
   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims = VIDEO_SCALE_PACK(800, 480);
   video_views_xr_reset(screens, RETRO_VIDEO_VIEWS_MAX);
   for (layout = 0; layout < VIDEO_SCREEN_LAYOUT_LAST; layout++)
      for (rotation = 0; rotation < 4; rotation++)
      {
         p.screen_layout = layout;
         p.rotation = rotation;
         p.screens = NULL;
         video_xr_place(&p, &base);
         p.screens = screens;
         video_xr_place(&p, &set);
         CHECK(memcmp(&base, &set, sizeof(set)) == 0);
      }
   /* Without a map too, the frame and gl's window. */
   p.map        = NULL;
   p.frame_dims = VIDEO_SCALE_PACK(800, 480);
   for (window = 0; window < 2; window++)
   {
      p.window  = window ? true : false;
      p.screens = NULL;
      video_xr_place(&p, &base);
      /* Placed alike; without the settings it is no item. */
      CHECK(base.slots[0].fixed);
      base.slots[0].fixed = false;
      p.screens = screens;
      video_xr_place(&p, &set);
      CHECK(memcmp(&base, &set, sizeof(set)) == 0);
   }
   p.window = false;
   /* Without a map the whole frame is screen 1: hidden, only the menu. */
   screens[0].visible = false;
   screens[0].x = 2.0f;
   p.map = NULL;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 1 && set.quads[0].kind == VIDEO_XR_QUAD_MENU);
   screens[0].visible = true;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 2);
   CHECK(pos_is(&set.quads[0], 2.0, 0.0, -1.8));
   CHECK(NEAR(set.quads[0].width, 1.6f));
}

static void test_screen_transform(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_screen_xr_t screens[RETRO_VIDEO_VIEWS_MAX];
   video_xr_vec3_t axis, result, origin, dir;
   float u, v, t;
   int16_t fx, fy;
   map_3ds(&map);
   params_init(&p, &map);
   video_views_xr_reset(screens, RETRO_VIDEO_VIEWS_MAX);
   p.screens = screens;
   screens[1].scale = 0.5f;
   screens[1].x = 0.25f;
   screens[1].z = 0.4f;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 3);
   CHECK(NEAR(set.quads[2].width, 0.64f));
   CHECK(NEAR(set.quads[2].height, 0.48f));
   CHECK(pos_is(&set.quads[2], 0.25, -0.992, -1.4));
   CHECK(pos_is(&set.quads[0], 0.0, 0.0, -1.8));
   CHECK(set.slots[0].view[0] == 0 && set.slots[0].view[1] == 1);
   CHECK(pos_is(&set.quads[1], 0.0, 0.0, -1.8));
   CHECK(NEAR(set.quads[0].width, 1.6) && NEAR(set.quads[1].height, 0.96));
   axis.x = axis.y = 0.0f;
   axis.z = 1.0f;
   screens[1].yaw = 90.0f;
   video_xr_place(&p, &set);
   video_xr_rotate(&set.quads[2].pose.orientation, &axis, &result);
   CHECK(NEAR(result.x, 1.0) && NEAR(result.y, 0.0) && NEAR(result.z, 0.0));
   origin.x = 1.25f;
   origin.y = -0.87248f;
   origin.z = -1.24064f;
   dir.x = -1.0f;
   dir.y = dir.z = 0.0f;
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_AUTO, false,
            &origin, &dir, &u, &v, &t) == 2);
   CHECK(NEAR(u, 0.251) && NEAR(v, 0.251) && NEAR(t, 1.0));
   CHECK(video_xr_quad_to_frame(&set.quads[2], u, v, &map,
            VIDEO_SCALE_PACK(800, 480), &fx, &fy));
   CHECK(fx == -6522 && fy == 8276);
   screens[1].yaw = 0.0f;
   screens[1].pitch = 90.0f;
   video_xr_place(&p, &set);
   video_xr_rotate(&set.quads[2].pose.orientation, &axis, &result);
   CHECK(NEAR(result.x, 0.0) && NEAR(result.y, -1.0) && NEAR(result.z, 0.0));
   screens[1].pitch = 0.0f;
   screens[1].roll = 90.0f;
   axis.x = 1.0f;
   axis.z = 0.0f;
   video_xr_place(&p, &set);
   video_xr_rotate(&set.quads[2].pose.orientation, &axis, &result);
   CHECK(NEAR(result.x, 0.0) && NEAR(result.y, 1.0) && NEAR(result.z, 0.0));
   screens[1].yaw = screens[1].pitch = 90.0f;
   video_xr_place(&p, &set);
   video_xr_rotate(&set.quads[2].pose.orientation, &axis, &result);
   CHECK(NEAR(result.x, 1.0) && NEAR(result.y, 0.0) && NEAR(result.z, 0.0));
   screens[1].pitch = screens[1].roll = 0.0f;
   p.anchor.orientation.y = (float)sin(M_PI / 4.0);
   p.anchor.orientation.w = (float)cos(M_PI / 4.0);
   p.anchor.position.x = 3.0f;
   p.anchor.position.y = 2.0f;
   p.anchor.position.z = 1.0f;
   video_xr_place(&p, &set);
   CHECK(pos_is(&set.quads[2], 1.6, 1.008, 0.75));
   axis.x = 0.0f;
   axis.z = 1.0f;
   video_xr_rotate(&set.quads[2].pose.orientation, &axis, &result);
   CHECK(NEAR(result.x, 0.0) && NEAR(result.y, 0.0) && NEAR(result.z, -1.0));
}

static void test_screen_visibility(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_screen_xr_t screens[RETRO_VIDEO_VIEWS_MAX];
   video_views_rect_t rects[VIDEO_XR_MAX_SLOTS][2];
   video_viewport_t vp;
   map_3ds(&map);
   params_init(&p, &map);
   video_views_xr_reset(screens, RETRO_VIDEO_VIEWS_MAX);
   p.screens = screens;
   p.ui_dims = VIDEO_SCALE_PACK(800, 480);
   screens[1].visible = false;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 3);
   CHECK(pos_is(&set.quads[0], 0.0, 0.0, -1.8));
   CHECK(pos_is(&set.quads[1], 0.0, 0.0, -1.8));
   CHECK(set.quads[2].kind == VIDEO_XR_QUAD_MENU);
   CHECK(pos_is(&set.quads[2], 0.0, 0.0, -1.7));
   CHECK(set.slots[1].dims == 0 && set.slots[1].layers == 0);
   CHECK(set.slots[1].view[0] == -1 && set.slots[1].view[1] == -1);
   screens[0].visible = false;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 1 && set.quads[0].kind == VIDEO_XR_QUAD_MENU);
   screens[1].visible = true;
   screens[0].scale = 10.0f;
   screens[0].y = 5.0f;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 2 && set.quads[0].screen == 1);
   CHECK(pos_is(&set.quads[0], 0.0, -0.992, -1.8));
   screens[0].visible = true;
   p.skip_screens = 1u;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 2 && set.quads[0].screen == 1);
   CHECK(set.slots[0].dims == 0);
   CHECK(pos_is(&set.quads[0], 0.0, -0.992, -1.8));
   memset(&vp, 0, sizeof(vp));
   vp.dims = vp.full_dims = VIDEO_SCALE_PACK(800, 480);
   screens[0].visible = screens[1].visible = false;
   video_xr_place_window(&p, true, false, &vp, vp.dims, &set, rects);
   CHECK(set.num_quads == 2);
   CHECK(pos_is(&set.quads[0], 0.0, 0.0, -1.8));
   CHECK(NEAR(set.quads[0].width, 1.6));
}

static void test_screen_identity(void)
{
   video_views_map_t map;
   struct retro_video_view v[3];
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_screen_xr_t screens[RETRO_VIDEO_VIEWS_MAX];
   map_3ds(&map);
   v[0] = map.views[2];
   v[1] = map.views[1];
   v[2] = map.views[0];
   CHECK(video_views_validate(v, 3, &map));
   params_init(&p, &map);
   video_views_xr_reset(screens, RETRO_VIDEO_VIEWS_MAX);
   screens[1].x = 0.25f;
   screens[1].scale = 0.5f;
   p.screens = screens;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 3 && set.quads[2].screen == 1);
   CHECK(pos_is(&set.quads[2], 0.25, -0.992, -1.8));
   CHECK(NEAR(set.quads[2].width, 0.64));
   CHECK(set.slots[0].view[0] == 2 && set.slots[0].view[1] == 1);
   CHECK(set.slots[1].view[0] == 0);
}

static void test_screen_density(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_screen_xr_t screens[RETRO_VIDEO_VIEWS_MAX];
   map_3ds(&map);
   params_init(&p, &map);
   video_views_xr_reset(screens, RETRO_VIDEO_VIEWS_MAX);
   p.screens = screens;
   screens[1].scale = 0.5f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[1].dims, 352, 264));
   screens[1].x = 0.25f;
   screens[1].z = 0.4f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[1].dims, 365, 274));
   p.anchor.position.x = 10.0f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[1].dims, 365, 274));
   screens[1].scale = 0.1f;
   screens[1].x = screens[1].z = 0.0f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[1].dims, 320, 240));
   p.stock = true;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[1].dims, 72, 54));
   p.px_per_rad = 0.0f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[1].dims, 320, 240));
   p.stock = false;
   p.px_per_rad = 1000.0f;
   screens[1].scale = 1.0f;
   screens[1].y = 0.992f;
   screens[1].z = 1.8f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[1].dims, 2986, 2240));
   p.max_dim = 500;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[1].dims, 500, 375));
}

static int vec_is(const video_xr_vec3_t *v, double x, double y, double z)
{
   return NEAR(v->x, x) && NEAR(v->y, y) && NEAR(v->z, z);
}

/* Degrees, the same turn either side of +-180. */
static int angle_near(float a, float b)
{
   double d = fmod(fabs((double)a - (double)b), 360.0);
   return d < 0.01 || d > 359.99;
}

/* Eight screens of 160x100 stacked, one view each. */
static void map_8(video_views_map_t *map)
{
   struct retro_video_view v[RETRO_VIDEO_VIEWS_MAX];
   unsigned s;
   for (s = 0; s < RETRO_VIDEO_VIEWS_MAX; s++)
      v[s] = mkview(0, s * 100, 160, 100, s, RETRO_VIDEO_VIEW_EYE_NONE);
   CHECK(video_views_validate(v, RETRO_VIDEO_VIEWS_MAX, map));
}

static void test_pose_invert(void)
{
   video_xr_pose_t a, inv, id;
   a.orientation.x = 0.1f;
   a.orientation.y = 0.3f;
   a.orientation.z = -0.2f;
   a.orientation.w = (float)sqrt(0.86);
   a.position.x    = 1.0f;
   a.position.y    = -2.0f;
   a.position.z    = 0.5f;
   video_xr_pose_invert(&a, &inv);
   video_xr_compose(&a, &inv, &id);
   CHECK(NEAR(id.position.x, 0.0) && NEAR(id.position.y, 0.0)
         && NEAR(id.position.z, 0.0) && NEAR(fabs(id.orientation.w), 1.0));
   video_xr_compose(&inv, &a, &id);
   CHECK(NEAR(id.position.x, 0.0) && NEAR(id.position.z, 0.0)
         && NEAR(fabs(id.orientation.w), 1.0));
   /* In place. */
   inv = a;
   video_xr_pose_invert(&inv, &inv);
   video_xr_compose(&a, &inv, &id);
   CHECK(NEAR(id.position.y, 0.0) && NEAR(fabs(id.orientation.w), 1.0));
}

/* The same turn: q and -q are one. */
static int quat_same(const video_xr_quat_t *a, const video_xr_quat_t *b)
{
   return fabs((double)a->x * b->x + (double)a->y * b->y
         + (double)a->z * b->z + (double)a->w * b->w) > 0.9999;
}

static void test_euler(void)
{
   static const float yaws[]    = { -170.0f, -90.0f, -30.0f, 0.0f,
      45.0f, 120.0f, 179.0f };
   static const float pitches[] = { -80.0f, -45.0f, 0.0f, 30.0f, 85.0f };
   static const float rolls[]   = { -150.0f, -60.0f, 0.0f, 10.0f,
      90.0f, 175.0f };
   unsigned i, j, k;
   video_xr_vec3_t base;
   video_screen_xr_t s, got;
   video_xr_pose_t pose, again;

   base.x = base.y = base.z = 0.0f;
   for (i = 0; i < sizeof(yaws) / sizeof(yaws[0]); i++)
      for (j = 0; j < sizeof(pitches) / sizeof(pitches[0]); j++)
         for (k = 0; k < sizeof(rolls) / sizeof(rolls[0]); k++)
         {
            video_views_xr_reset(&s, 1);
            s.yaw   = yaws[i];
            s.pitch = pitches[j];
            s.roll  = rolls[k];
            video_xr_item_pose(&base, &s, &pose);
            video_xr_euler(&pose.orientation, &got.yaw, &got.pitch,
                  &got.roll);
            CHECK(angle_near(got.yaw, s.yaw) && angle_near(got.pitch, s.pitch)
                  && angle_near(got.roll, s.roll));
         }

   /* Straight up the roll folds into the yaw: the same turn. */
   video_views_xr_reset(&s, 1);
   s.yaw   = 30.0f;
   s.pitch = 90.0f;
   s.roll  = 20.0f;
   video_xr_item_pose(&base, &s, &pose);
   video_views_xr_reset(&got, 1);
   video_xr_euler(&pose.orientation, &got.yaw, &got.pitch, &got.roll);
   CHECK(fabs(got.pitch - 90.0f) < 0.05 && got.roll == 0.0f);
   CHECK(angle_near(got.yaw, 10.0f));
   video_xr_item_pose(&base, &got, &again);
   CHECK(quat_same(&pose.orientation, &again.orientation));
   /* Straight down it adds. */
   s.pitch = -90.0f;
   video_xr_item_pose(&base, &s, &pose);
   video_views_xr_reset(&got, 1);
   video_xr_euler(&pose.orientation, &got.yaw, &got.pitch, &got.roll);
   CHECK(fabs(got.pitch + 90.0f) < 0.05 && got.roll == 0.0f);
   CHECK(angle_near(got.yaw, 50.0f));
   video_xr_item_pose(&base, &got, &again);
   CHECK(quat_same(&pose.orientation, &again.orientation));
}

static void test_level(void)
{
   unsigned i;
   video_xr_vec3_t base;
   video_screen_xr_t s;
   video_xr_pose_t pose;
   float yaw, pitch, roll;
   base.x = base.y = base.z = 0.0f;
   video_views_xr_reset(&s, 1);
   s.yaw   = 30.0f;
   s.pitch = 20.0f;
   s.roll  = 40.0f;
   video_xr_item_pose(&base, &s, &pose);
   video_xr_level(&pose.orientation, 0.0f);
   video_xr_euler(&pose.orientation, &yaw, &pitch, &roll);
   CHECK(angle_near(yaw, 30.0f) && angle_near(pitch, 20.0f)
         && angle_near(roll, 0.0f));
   video_xr_level(&pose.orientation, -15.0f);
   video_xr_euler(&pose.orientation, &yaw, &pitch, &roll);
   CHECK(angle_near(yaw, 30.0f) && angle_near(pitch, 20.0f)
         && angle_near(roll, -15.0f));

   /* Straight up or down the roll is the yaw's: levelling a rolled
    * item to its own roll leaves it as it is. */
   for (i = 0; i < 2; i++)
   {
      video_xr_quat_t q;
      s.pitch = i ? -90.0f : 90.0f;
      s.roll  = 30.0f;
      video_xr_item_pose(&base, &s, &pose);
      q = pose.orientation;
      video_xr_level(&q, 30.0f);
      CHECK(quat_same(&q, &pose.orientation));
   }
}

static void test_layout(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_layout_t lay;
   video_xr_quad_set_t set;

   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims = VIDEO_SCALE_PACK(800, 480);
   video_xr_layout(&p, &lay);
   CHECK(lay.count == 2);
   CHECK(vec_is(&lay.pos[0], 0.0, 0.0, -1.8));
   CHECK(NEAR(lay.width[0], 1.6) && NEAR(lay.height[0], 0.96));
   CHECK(vec_is(&lay.pos[1], 0.0, -0.992, -1.8));
   CHECK(NEAR(lay.width[1], 1.28) && NEAR(lay.height[1], 0.96));
   CHECK(lay.native[1] == VIDEO_SCALE_PACK(320, 240));
   CHECK(vec_is(&lay.menu, 0.0, 0.0, -1.7));

   /* video_xr_place starts from the same spots, and says so. */
   video_xr_place(&p, &set);
   CHECK(vec_is(&set.slots[0].base, 0.0, 0.0, -1.8));
   CHECK(vec_is(&set.slots[1].base, 0.0, -0.992, -1.8));
   CHECK(vec_is(&set.slots[VIDEO_XR_MENU_SLOT].base, 0.0, 0.0, -1.7));

   p.screen_layout = VIDEO_SCREEN_LAYOUT_HORIZONTAL;
   video_xr_layout(&p, &lay);
   CHECK(vec_is(&lay.pos[1], 1.472, 0.0, -1.8));

   /* A whole frame is screen 0's spot. */
   params_init(&p, NULL);
   p.frame_dims = VIDEO_SCALE_PACK(800, 480);
   video_xr_layout(&p, &lay);
   CHECK(lay.count == 1 && vec_is(&lay.pos[0], 0.0, 0.0, -1.8));
   CHECK(NEAR(lay.width[0], 1.6) && NEAR(lay.height[0], 0.96));
   video_xr_place(&p, &set);
   CHECK(vec_is(&set.slots[0].base, 0.0, 0.0, -1.8));
}

/* Each screen's quad turned back into settings gives its settings:
 * both layouts, three scales, all eight screens, a turned anchor. */
static void test_settings_identity(void)
{
   static const float scales[] = { 0.5f, 1.0f, 2.5f };
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_xr_layout_t lay;
   video_screen_xr_t screens[RETRO_VIDEO_VIEWS_MAX];
   video_xr_pose_t inv;
   unsigned layout, k, s, i;

   map_8(&map);
   params_init(&p, &map);
   p.anchor.orientation.y = (float)sin(M_PI / 8.0);
   p.anchor.orientation.w = (float)cos(M_PI / 8.0);
   p.anchor.position.x    = 0.4f;
   p.anchor.position.z    = -0.3f;
   video_xr_pose_invert(&p.anchor, &inv);
   for (layout = 0; layout < VIDEO_SCREEN_LAYOUT_LAST; layout++)
      for (k = 0; k < sizeof(scales) / sizeof(scales[0]); k++)
      {
         video_views_xr_reset(screens, RETRO_VIDEO_VIEWS_MAX);
         for (s = 0; s < RETRO_VIDEO_VIEWS_MAX; s++)
         {
            screens[s].x     = 0.1f * (float)s - 0.3f;
            screens[s].y     = 0.05f * (float)s;
            screens[s].z     = -0.2f + 0.07f * (float)s;
            screens[s].yaw   = -40.0f + 11.0f * (float)s;
            screens[s].pitch = 25.0f - 9.0f * (float)s;
            screens[s].roll  = 5.0f * (float)s - 15.0f;
            screens[s].scale = scales[k];
         }
         p.screen_layout = layout;
         p.screens       = screens;
         video_xr_place(&p, &set);
         video_xr_layout(&p, &lay);
         CHECK(set.num_quads == RETRO_VIDEO_VIEWS_MAX);
         for (i = 0; i < set.num_quads; i++)
         {
            video_screen_xr_t got;
            video_xr_pose_t local;
            const video_xr_quad_t *q = &set.quads[i];
            s = q->screen;
            video_xr_compose(&inv, &q->pose, &local);
            video_xr_item_settings(&set.slots[s].base, &local, &got);
            CHECK(NEAR(got.x, screens[s].x) && NEAR(got.y, screens[s].y)
                  && NEAR(got.z, screens[s].z));
            CHECK(angle_near(got.yaw, screens[s].yaw)
                  && angle_near(got.pitch, screens[s].pitch)
                  && angle_near(got.roll, screens[s].roll));
            CHECK(NEAR(q->width / lay.width[s], scales[k]));
         }
      }
}

static void test_grab(void)
{
   video_xr_grab_t g;
   video_xr_pose_t aim, pose, inv;
   video_screen_xr_t s;

   memset(&g, 0, sizeof(g));
   video_xr_pose_identity(&aim);
   /* An item 1.8 m ahead at its own spot, held from the eyes. */
   g.base.z = -1.8f;
   video_xr_pose_identity(&g.rel);
   g.rel.position.z = -1.8f;
   CHECK(!video_xr_grab_pose(&g, &aim, &pose, &s));
   CHECK(NEAR(pose.position.z, -1.8) && NEAR(s.x, 0.0) && NEAR(s.z, 0.0));
   /* The hand moves: the item with it. */
   aim.position.x = 0.2f;
   CHECK(!video_xr_grab_pose(&g, &aim, &pose, &s));
   CHECK(NEAR(s.x, 0.2) && NEAR(s.y, 0.0) && NEAR(s.z, 0.0));
   /* The wrist rolls 30 degrees: level, the item does not... */
   aim.orientation.z = (float)sin(M_PI / 12.0);
   aim.orientation.w = (float)cos(M_PI / 12.0);
   video_xr_grab_pose(&g, &aim, &pose, &s);
   CHECK(angle_near(s.roll, 0.0f) && angle_near(s.yaw, 0.0f)
         && angle_near(s.pitch, 0.0f));
   /* ...tilting, it does. */
   g.tilt = true;
   video_xr_grab_pose(&g, &aim, &pose, &s);
   CHECK(angle_near(s.roll, 30.0f));
   /* Level again at the roll it was left at. */
   g.tilt            = false;
   g.roll            = 30.0f;
   aim.orientation.z = 0.0f;
   aim.orientation.w = 1.0f;
   video_xr_grab_pose(&g, &aim, &pose, &s);
   CHECK(angle_near(s.roll, 30.0f));
   /* At a limit the item stops: 10 m right of its spot at most. */
   aim.position.x = 10.5f;
   CHECK(video_xr_grab_pose(&g, &aim, &pose, &s));
   CHECK(NEAR(s.x, 10.0) && NEAR(pose.position.x, 10.0));
   /* Held again from there, as the caller does, it resumes from the
    * limit, not from where the hand went. */
   video_xr_pose_invert(&aim, &inv);
   video_xr_compose(&inv, &pose, &g.rel);
   aim.position.x = 10.4f;
   CHECK(!video_xr_grab_pose(&g, &aim, &pose, &s));
   CHECK(NEAR(s.x, 9.9));
   /* out may be NULL: only the pose. */
   CHECK(!video_xr_grab_pose(&g, &aim, &pose, NULL));
}

static void test_grab_stick(void)
{
   video_xr_grab_t g;
   float scale = 1.0f;
   memset(&g, 0, sizeof(g));
   video_xr_pose_identity(&g.rel);
   g.rel.position.z = -1.0f;
   /* Inside the deadzone: nothing. */
   video_xr_grab_stick(&g, 0.1f, 0.15f, 1.0, &scale);
   CHECK(NEAR(g.rel.position.z, -1.0) && scale == 1.0f);
   /* Full push for half a second, then half past the deadzone for one. */
   video_xr_grab_stick(&g, 0.0f, 1.0f, 0.5, &scale);
   CHECK(NEAR(g.rel.position.z, -1.75));
   video_xr_grab_stick(&g, 0.0f, 0.6f, 1.0, &scale);
   CHECK(NEAR(g.rel.position.z, -2.5));
   /* Pulled in, no nearer than the reach. */
   video_xr_grab_stick(&g, 0.0f, -1.0f, 10.0, &scale);
   CHECK(NEAR(g.rel.position.z, -VIDEO_XR_EDIT_REACH));
   /* Right doubles it in a second, left halves it, within the range. */
   video_xr_grab_stick(&g, 1.0f, 0.0f, 1.0, &scale);
   CHECK(NEAR(scale, 2.0));
   video_xr_grab_stick(&g, -1.0f, 0.0f, 2.0, &scale);
   CHECK(NEAR(scale, 0.5));
   video_xr_grab_stick(&g, 1.0f, 0.0f, 10.0, &scale);
   CHECK(scale == 10.0f);
   video_xr_grab_stick(&g, -1.0f, 0.0f, 20.0, &scale);
   CHECK(NEAR(scale, 0.1));
}

static void test_menu_handle(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_xr_quad_t menu, h;
   video_xr_vec3_t o, d;
   video_xr_pose_t pose;
   float u, v, t;

   /* Under the menu's bottom edge, 1 cm down, 40% as wide, 3 cm tall. */
   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims = VIDEO_SCALE_PACK(1600, 960);
   video_xr_place(&p, &set);
   CHECK(set.quads[3].kind == VIDEO_XR_QUAD_MENU);
   video_xr_menu_handle(&set.quads[3], &h);
   CHECK(pos_is(&h, 0.0, -0.505, -1.7));
   CHECK(NEAR(h.width, 0.64) && NEAR(h.height, 0.03));
   CHECK(h.kind == VIDEO_XR_QUAD_MENU && h.slot == VIDEO_XR_MENU_SLOT);
   CHECK(NEAR(h.pose.orientation.w, 1.0));

   /* Its down is the menu's: rolled a quarter turn, towards +X. */
   memset(&menu, 0, sizeof(menu));
   menu.pose.orientation.z = (float)sin(M_PI / 4.0);
   menu.pose.orientation.w = (float)cos(M_PI / 4.0);
   menu.pose.position.x    = 1.0f;
   menu.pose.position.y    = 2.0f;
   menu.pose.position.z    = 3.0f;
   menu.width              = 1.0f;
   menu.height             = 0.5f;
   menu.kind               = VIDEO_XR_QUAD_MENU;
   video_xr_menu_handle(&menu, &h);
   CHECK(pos_is(&h, 1.275, 2.0, 3.0));
   CHECK(NEAR(h.width, 0.4) && NEAR(h.pose.orientation.z, sin(M_PI / 4.0)));

   /* Picked there, 10 cm along it, the point is the menu's; through
    * the menu, nothing. */
   memset(&set, 0, sizeof(set));
   set.quads[0]  = menu;
   set.num_quads = 1;
   o.x = 1.275f; o.y = 2.1f; o.z = 4.0f;
   d.x = 0.0f;   d.y = 0.0f; d.z = -1.0f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 0);
   CHECK(NEAR(u, 0.6) && NEAR(v, 1.05) && NEAR(t, 1.0));
   video_xr_cursor(&set.quads[0], u, v, t, &pose);
   CHECK(NEAR(pose.position.x, 1.275) && NEAR(pose.position.y, 2.1));
   o.x = 1.0f; o.y = 2.0f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == -1);

   /* A menu placed a quarter turn left: its handle 1.7 m to the left,
    * 16 cm along it towards -Z, nearer than screen 1 behind. */
   params_init(&p, &map);
   p.ui_dims              = VIDEO_SCALE_PACK(1600, 960);
   p.anchor.orientation.y = (float)sin(M_PI / 4.0);
   p.anchor.orientation.w = (float)cos(M_PI / 4.0);
   video_xr_place(&p, &set);
   o.x = o.y = o.z = 0.0f;
   d.x = -1.7f; d.y = -0.505f; d.z = -0.16f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 3);
   CHECK(NEAR(u, 0.6) && NEAR(v, 1.02604) && NEAR(t, 1.0));
}

static void test_live_edit(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_xr_vec3_t o, d;
   video_xr_pose_t pose;
   float u, v, t;
   unsigned i;

   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims = VIDEO_SCALE_PACK(1600, 960);
   video_xr_place(&p, &set);
   /* Every item, screen 0 and the closed menu too. */
   for (i = 0; i < set.num_quads; i++)
      CHECK(video_xr_quad_live(&set.quads[i], VIDEO_XR_LIVE_EDIT, false));
   o.x = o.y = o.z = 0.0f;
   /* Straight ahead the menu covers screen 0: through it, screen 0. */
   d.x = 0.0f; d.y = 0.0f; d.z = -1.0f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 0);
   /* Its handle, nearer than screen 1 behind it, takes the menu; the
    * point is in the menu's frame, so the dot lands on the handle. */
   d.x = 0.0f; d.y = -0.505f; d.z = -1.7f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 3);
   CHECK(NEAR(u, 0.5) && NEAR(v, 1.02604) && NEAR(t, 1.0));
   video_xr_cursor(&set.quads[3], u, v, t, &pose);
   CHECK(NEAR(pose.position.x, 0.0) && NEAR(pose.position.y, -0.505));
   /* The handle is edit mode's alone. */
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_AUTO, true,
            &o, &d, &u, &v, &t) == 2);
   /* Between the menu and its handle, what is behind: screen 1. */
   o.y = 1.47f;
   d.x = 0.0f; d.y = -1.15f; d.z = -1.0f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 2);
   /* Other lasers still see the whole menu. */
   o.y = 0.0f;
   d.x = 0.0f; d.y = 0.0f; d.z = -1.0f;
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_AUTO, true,
            &o, &d, &u, &v, &t) == 3);

   /* A whole frame and a window are items too. */
   params_init(&p, NULL);
   p.frame_dims = VIDEO_SCALE_PACK(800, 480);
   video_xr_place(&p, &set);
   CHECK(video_xr_quad_live(&set.quads[0], VIDEO_XR_LIVE_EDIT, false));
   p.window = true;
   video_xr_place(&p, &set);
   CHECK(video_xr_quad_live(&set.quads[0], VIDEO_XR_LIVE_EDIT, false));
}

/* A frame shown whole without the screens' settings, a views core's
 * fallback, is no item for edit mode, which passes it; other lasers
 * still meet it. A no-map frame with the settings, screens and the
 * menu are items. */
static void test_edit_fixed(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_screen_xr_t screens[RETRO_VIDEO_VIEWS_MAX];
   video_xr_vec3_t o, d;
   float u, v, t;
   unsigned s;

   o.x = o.y = o.z = 0.0f;
   d.x = 0.0f; d.y = 0.0f; d.z = -1.0f;
   video_views_xr_reset(screens, RETRO_VIDEO_VIEWS_MAX);
   params_init(&p, NULL);
   p.frame_dims = VIDEO_SCALE_PACK(800, 480);
   p.ui_dims    = VIDEO_SCALE_PACK(1600, 960);
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 2 && set.slots[0].fixed
         && !set.slots[VIDEO_XR_MENU_SLOT].fixed);
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == -1);
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_ALWAYS, false,
            &o, &d, &u, &v, &t) == 0);
   /* gl's window, shown whole while the menu is open or the eyes are
    * tracked, likewise. */
   p.window = true;
   video_xr_place(&p, &set);
   CHECK(set.slots[0].fixed);
   p.window  = false;
   p.screens = screens;
   video_xr_place(&p, &set);
   CHECK(!set.slots[0].fixed);
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 0);

   map_3ds(&map);
   params_init(&p, &map);
   p.screens = screens;
   video_xr_place(&p, &set);
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
      CHECK(!set.slots[s].fixed);
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 0);
}

/* A frame without a map takes screen 1's place, turn and size; gl's
 * window too while the menu is closed, but not with it open over it. */
static void test_frame_as_screen(void)
{
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_screen_xr_t screens[RETRO_VIDEO_VIEWS_MAX];
   video_views_rect_t rects[VIDEO_XR_MAX_SLOTS][2];
   video_viewport_t vp;
   video_xr_vec3_t axis, result;

   params_init(&p, NULL);
   p.frame_dims   = VIDEO_SCALE_PACK(800, 480);
   p.frame_aspect = 800.0f / 480.0f;
   video_views_xr_reset(screens, RETRO_VIDEO_VIEWS_MAX);
   screens[0].x     = 0.25f;
   screens[0].y     = 0.1f;
   screens[0].z     = 0.3f;
   screens[0].yaw   = 90.0f;
   screens[0].scale = 1.5f;
   screens[1].x     = 5.0f;
   p.screens        = screens;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 1 && set.quads[0].kind == VIDEO_XR_QUAD_FRAME);
   CHECK(pos_is(&set.quads[0], 0.25, 0.1, -1.5));
   CHECK(NEAR(set.quads[0].width, 2.4) && NEAR(set.quads[0].height, 1.44));
   CHECK(dims_is(set.slots[0].dims, 1335, 801));
   axis.x = axis.y = 0.0f;
   axis.z = 1.0f;
   video_xr_rotate(&set.quads[0].pose.orientation, &axis, &result);
   CHECK(NEAR(result.x, 1.0) && NEAR(result.z, 0.0));
   /* Clamped to the setting's range. */
   screens[0].scale = 20.0f;
   video_xr_place(&p, &set);
   CHECK(NEAR(set.quads[0].width, 16.0));
   screens[0].scale = 1.5f;

   p.window = true;
   memset(&vp, 0, sizeof(vp));
   vp.dims = vp.full_dims = VIDEO_SCALE_PACK(800, 480);
   video_xr_place_window(&p, false, false, &vp, vp.dims, &set, rects);
   CHECK(set.num_quads == 1 && set.quads[0].kind == VIDEO_XR_QUAD_WINDOW);
   CHECK(pos_is(&set.quads[0], 0.25, 0.1, -1.5));
   video_xr_place_window(&p, true, false, &vp, vp.dims, &set, rects);
   CHECK(pos_is(&set.quads[0], 0.0, 0.0, -1.8));
   CHECK(NEAR(set.quads[0].width, 1.6));
   screens[0].visible = false;
   video_xr_place_window(&p, true, false, &vp, vp.dims, &set, rects);
   CHECK(set.num_quads == 1);
   p.window = false;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 0);
}

/* The menu 10 cm in front of screen 1 and facing as it does, after the
 * screens, as large to the eye as at its spot and no nearer than 0.4 m;
 * at its own place once placed; at its spot when screen 1 does not
 * float or show, or would leave it facing away. */
static void test_menu_spot(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_screen_xr_t screens[RETRO_VIDEO_VIEWS_MAX];
   video_screen_xr_t menu;
   const video_xr_quad_t *m;

   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims = VIDEO_SCALE_PACK(1600, 960);
   video_views_xr_reset(screens, RETRO_VIDEO_VIEWS_MAX);
   p.screens = screens;
   screens[0].z   = 0.6f;
   screens[0].yaw = 30.0f;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 4);
   m = &set.quads[set.num_quads - 1];
   CHECK(m->kind == VIDEO_XR_QUAD_MENU);
   CHECK(pos_is(m, 0.05, 0.0, -1.11340));
   CHECK(NEAR(m->pose.orientation.y, 0.25882) && NEAR(m->pose.orientation.w, 0.96593));
   /* 1.1145 m away: 1.6 x 1.1145 / 1.7 wide. */
   CHECK(NEAR(m->width, 1.04896) && NEAR(m->height, 0.62938));
   CHECK(vec_is(&set.slots[VIDEO_XR_MENU_SLOT].base, 0.0, 0.0, -1.7));
   /* Its image is the one at its spot. */
   p.px_per_rad = 200.0f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[VIDEO_XR_MENU_SLOT].dims, 352, 211));

   /* Far and enlarged: as wide to the eye, the same image. */
   screens[0].z     = -8.0f;
   screens[0].yaw   = 0.0f;
   screens[0].scale = 5.0f;
   video_xr_place(&p, &set);
   m = &set.quads[set.num_quads - 1];
   CHECK(pos_is(m, 0.0, 0.0, -9.7));
   CHECK(NEAR(m->width, 9.12941) && NEAR(m->height, 5.47765));
   CHECK(dims_is(set.slots[VIDEO_XR_MENU_SLOT].dims, 352, 211));
   p.px_per_rad = 1000.0f;

   /* Pulled to the eyes: 0.4 m out along the line from the anchor, or
    * straight ahead when at it; behind them, facing away, at its spot. */
   screens[0].scale = 1.0f;
   screens[0].z     = 1.6f;
   video_xr_place(&p, &set);
   m = &set.quads[set.num_quads - 1];
   CHECK(pos_is(m, 0.0, 0.0, -0.4));
   CHECK(NEAR(m->width, 0.37647) && NEAR(m->height, 0.22588));
   screens[0].z = 1.7f;
   video_xr_place(&p, &set);
   CHECK(pos_is(&set.quads[set.num_quads - 1], 0.0, 0.0, -0.4));
   screens[0].z = 2.0f;
   video_xr_place(&p, &set);
   CHECK(pos_is(&set.quads[set.num_quads - 1], 0.0, 0.0, -1.7));
   screens[0].z = 2.5f;
   video_xr_place(&p, &set);
   m = &set.quads[set.num_quads - 1];
   CHECK(pos_is(m, 0.0, 0.0, -1.7));
   CHECK(NEAR(m->width, 1.6) && NEAR(m->height, 0.96));
   CHECK(NEAR(m->pose.orientation.w, 1.0));
   /* Beside the eyes: followed while it faces them (yaw -90 turns the
    * face to -X), at its spot when turned away. */
   screens[0].x   = 3.0f;
   screens[0].z   = 1.8f;
   screens[0].yaw = -90.0f;
   video_xr_place(&p, &set);
   m = &set.quads[set.num_quads - 1];
   CHECK(pos_is(m, 2.9, 0.0, 0.0));
   CHECK(NEAR(m->width, 2.72941));
   screens[0].yaw = 90.0f;
   video_xr_place(&p, &set);
   CHECK(pos_is(&set.quads[set.num_quads - 1], 0.0, 0.0, -1.7));
   screens[0].x   = 0.0f;
   screens[0].yaw = 0.0f;
   screens[0].z = 1.25f;
   video_xr_place(&p, &set);
   CHECK(pos_is(&set.quads[set.num_quads - 1], 0.0, 0.0, -0.45));
   screens[0].z = 0.6f;

   /* Placed: from its own spot. */
   video_views_xr_reset(&menu, 1);
   menu.x     = 0.5f;
   menu.z     = 0.2f;
   menu.scale = 0.5f;
   p.menu     = &menu;
   video_xr_place(&p, &set);
   m = &set.quads[set.num_quads - 1];
   CHECK(m->kind == VIDEO_XR_QUAD_MENU && pos_is(m, 0.5, 0.0, -1.5));
   CHECK(NEAR(m->width, 0.8) && NEAR(m->height, 0.48));
   /* Clamped to the setting's range. */
   menu.scale = 20.0f;
   video_xr_place(&p, &set);
   CHECK(NEAR(set.quads[set.num_quads - 1].width, 16.0));
   p.menu = NULL;

   /* Screen 1 hidden or the projection's: the menu at its spot. */
   screens[0].visible = false;
   screens[0].x       = 3.0f;
   video_xr_place(&p, &set);
   m = &set.quads[set.num_quads - 1];
   CHECK(m->kind == VIDEO_XR_QUAD_MENU && pos_is(m, 0.0, 0.0, -1.7));
   screens[0].visible = true;
   p.skip_screens     = 1u;
   video_xr_place(&p, &set);
   m = &set.quads[set.num_quads - 1];
   CHECK(m->kind == VIDEO_XR_QUAD_MENU && pos_is(m, 0.0, 0.0, -1.7));
   p.skip_screens = 0;

   /* A whole frame is screen 1: the menu goes with it. */
   params_init(&p, NULL);
   p.frame_dims = VIDEO_SCALE_PACK(800, 480);
   p.ui_dims    = VIDEO_SCALE_PACK(1600, 960);
   video_views_xr_reset(screens, RETRO_VIDEO_VIEWS_MAX);
   screens[0].z = 0.6f;
   p.screens    = screens;
   video_xr_place(&p, &set);
   CHECK(set.num_quads == 2 && set.quads[1].kind == VIDEO_XR_QUAD_MENU);
   CHECK(pos_is(&set.quads[1], 0.0, 0.0, -1.1));
   CHECK(NEAR(set.quads[1].width, 1.03529));
   /* Its image to the pixel where sizing it from the moved size would
    * round up one more. */
   p.px_per_rad   = 355.240692f;
   screens[0].z   = -8.0f;
   screens[0].yaw = -23.0f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[VIDEO_XR_MENU_SLOT].dims, 625, 375));
}

/* Every slot of a plan carries the edit count it was planned at, and a
 * plan is the same bytes whatever was under it: a driver that compares
 * plans byte for byte sees no change that is not one. */
static void test_edit_seq(void)
{
   static video_xr_quad_set_t a, b;
   video_views_map_t map;
   video_xr_params_t p;
   unsigned s;
   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims  = VIDEO_SCALE_PACK(1600, 960);
   p.edit_seq = 7;
   memset(&a, 0x00, sizeof(a));
   memset(&b, 0xa5, sizeof(b));
   video_xr_place(&p, &a);
   video_xr_place(&p, &b);
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
      CHECK(a.slots[s].edit_seq == 7);
   CHECK(!memcmp(&a, &b, sizeof(a)));
}

/* While editing, a slot keeps its image for a plan within 0.8 to 1.25
 * times it on both axes; out of edit mode every plan is as planned. */
static void test_keep_dims(void)
{
   static video_xr_quad_set_t plan, set;
   video_views_map_t map;
   video_xr_params_t p;
   video_screen_xr_t screens[RETRO_VIDEO_VIEWS_MAX];
   unsigned s, w, h, cur0;
   unsigned cur = VIDEO_SCALE_PACK(800, 480);

   for (w = 1; w <= 1300; w += 7)
      for (h = 1; h <= 800; h += 5)
      {
         unsigned d = VIDEO_SCALE_PACK(w, h);
         CHECK(video_xr_keep_dims(d, cur, false) == d);
         CHECK(video_xr_keep_dims(d, 0, false) == d);
      }
   CHECK(video_xr_keep_dims(0, cur, false) == 0);

   /* The band's edges, each axis on its own. */
   CHECK(video_xr_keep_dims(VIDEO_SCALE_PACK(640, 384), cur, true) == cur);
   CHECK(video_xr_keep_dims(VIDEO_SCALE_PACK(1000, 600), cur, true) == cur);
   CHECK(video_xr_keep_dims(VIDEO_SCALE_PACK(801, 479), cur, true) == cur);
   CHECK(dims_is(video_xr_keep_dims(VIDEO_SCALE_PACK(639, 480), cur, true),
            639, 480));
   CHECK(dims_is(video_xr_keep_dims(VIDEO_SCALE_PACK(1001, 480), cur, true),
            1001, 480));
   CHECK(dims_is(video_xr_keep_dims(VIDEO_SCALE_PACK(800, 383), cur, true),
            800, 383));
   CHECK(dims_is(video_xr_keep_dims(VIDEO_SCALE_PACK(800, 601), cur, true),
            800, 601));
   /* No image yet; a slot planned empty stays empty. */
   CHECK(dims_is(video_xr_keep_dims(VIDEO_SCALE_PACK(700, 420), 0, true),
            700, 420));
   CHECK(video_xr_keep_dims(0, cur, true) == 0);

   /* Screen 1 20 cm nearer keeps its image while editing; twice as near
    * is past the band. Out of edit mode the plan's bytes are its own. */
   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims = VIDEO_SCALE_PACK(1600, 960);
   video_views_xr_reset(screens, RETRO_VIDEO_VIEWS_MAX);
   p.screens = screens;
   video_xr_place(&p, &plan);
   cur0 = plan.slots[0].dims;
   CHECK(dims_is(cur0, 837, 502));
   screens[0].z = 0.2f;
   video_xr_place(&p, &set);
   CHECK(dims_is(set.slots[0].dims, 928, 557));
   CHECK(video_xr_keep_dims(set.slots[0].dims, cur0, true) == cur0);
   screens[0].z = 1.0f;
   video_xr_place(&p, &set);
   CHECK(video_xr_keep_dims(set.slots[0].dims, cur0, true)
         == set.slots[0].dims && set.slots[0].dims != cur0);
   memcpy(&set, &plan, sizeof(set));
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
      set.slots[s].dims = video_xr_keep_dims(set.slots[s].dims,
            VIDEO_SCALE_PACK(VIDEO_SCALE_W(set.slots[s].dims) + 1,
               VIDEO_SCALE_H(set.slots[s].dims) + 1), false);
   CHECK(!memcmp(&plan, &set, sizeof(set)));
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

static uint8_t t_atlas[VIDEO_XR_CURSOR_W * VIDEO_XR_CURSOR_H * 4];

static const uint8_t *texel(unsigned x, unsigned y)
{
   return t_atlas + ((size_t)y * VIDEO_XR_CURSOR_W + x) * 4;
}

/* Whether every texel of column x is clear. */
static int column_clear(unsigned x)
{
   unsigned y;
   for (y = 0; y < VIDEO_XR_CURSOR_H; y++)
      if (texel(x, y)[3])
         return 0;
   return 1;
}

/* The laser's image: the dot, edit mode's faint and bright outlines,
 * the menu's faint and bright handle bars, a clear column between
 * each. */
static void test_cursor_pixels(void)
{
   const uint8_t *centre = texel(VIDEO_XR_CURSOR_DIM / 2,
         VIDEO_XR_CURSOR_Y + VIDEO_XR_CURSOR_DIM / 2);
   const uint8_t *rim    = texel(VIDEO_XR_CURSOR_DIM / 2,
         VIDEO_XR_CURSOR_Y + 2);
   const uint8_t *corner = texel(0, VIDEO_XR_CURSOR_Y);
   const uint8_t *faint  = texel(VIDEO_XR_OUTLINE_X(0), 64);
   const uint8_t *edge   = texel(VIDEO_XR_OUTLINE_X(0)
         + VIDEO_XR_OUTLINE_DIM - 1, 64);
   const uint8_t *inside = texel(VIDEO_XR_OUTLINE_X(0)
         + VIDEO_XR_OUTLINE_EDGE, 64);
   const uint8_t *bright = texel(VIDEO_XR_OUTLINE_X(1) + 64,
         VIDEO_XR_OUTLINE_DIM - 1);
   const uint8_t *bar    = texel(VIDEO_XR_HANDLE_X(0)
         + VIDEO_XR_CURSOR_DIM / 2, VIDEO_XR_CURSOR_Y);
   const uint8_t *lit    = texel(VIDEO_XR_HANDLE_X(1)
         + VIDEO_XR_CURSOR_DIM - 1, VIDEO_XR_CURSOR_Y
         + VIDEO_XR_CURSOR_DIM - 1);

   memset(t_atlas, 0x7f, sizeof(t_atlas));
   video_xr_cursor_pixels(t_atlas);
   /* The dot: white in the middle, a dark rim, nothing in the corners;
    * grey, so RGBA and BGRA are the same bytes. */
   CHECK(centre[0] == 255 && centre[1] == 255 && centre[2] == 255
         && centre[3] == 255);
   CHECK(rim[0] == 38 && rim[1] == 38 && rim[2] == 38 && rim[3] == 255);
   CHECK(!corner[0] && !corner[1] && !corner[2] && !corner[3]);
   CHECK(!texel(4, 4)[3]);
   /* Mid-height: the same rows counted from either end. */
   CHECK(VIDEO_XR_CURSOR_Y + VIDEO_XR_CURSOR_DIM + VIDEO_XR_CURSOR_Y
         == VIDEO_XR_CURSOR_H);
   /* The outlines: a rim, faint then bright, clear inside. */
   CHECK(faint[0] == 64 && faint[1] == 64 && faint[3] == 64);
   CHECK(edge[0] == 64 && edge[3] == 64);
   CHECK(!inside[0] && !inside[3]);
   CHECK(bright[0] == 255 && bright[2] == 255 && bright[3] == 255);
   /* The handle bars: filled at the dot's rows, faint then bright, and
    * nothing above or below them. */
   CHECK(bar[0] == 64 && bar[3] == 64);
   CHECK(lit[0] == 255 && lit[1] == 255 && lit[3] == 255);
   CHECK(!texel(VIDEO_XR_HANDLE_X(0), VIDEO_XR_CURSOR_Y - 1)[3]);
   CHECK(!texel(VIDEO_XR_HANDLE_X(1), VIDEO_XR_CURSOR_Y
            + VIDEO_XR_CURSOR_DIM)[3]);
   /* Filtering at a part's edge meets a clear texel, not the next. */
   CHECK(column_clear(VIDEO_XR_CURSOR_DIM));
   CHECK(column_clear(VIDEO_XR_OUTLINE_X(1) - 1));
   CHECK(column_clear(VIDEO_XR_HANDLE_X(0) - 1));
   CHECK(column_clear(VIDEO_XR_HANDLE_X(1) - 1));
   CHECK(VIDEO_XR_HANDLE_X(1) + VIDEO_XR_CURSOR_DIM == VIDEO_XR_CURSOR_W);
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
   test_screen_defaults();
   test_screen_transform();
   test_screen_visibility();
   test_screen_identity();
   test_screen_density();
   test_pose_invert();
   test_euler();
   test_level();
   test_layout();
   test_settings_identity();
   test_grab();
   test_grab_stick();
   test_menu_handle();
   test_live_edit();
   test_edit_fixed();
   test_frame_as_screen();
   test_menu_spot();
   test_edit_seq();
   test_keep_dims();
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
