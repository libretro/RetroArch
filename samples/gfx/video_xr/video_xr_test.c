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
   g.turn           = g.rel.orientation;
   CHECK(!video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL));
   CHECK(NEAR(pose.position.z, -1.8) && NEAR(s.x, 0.0) && NEAR(s.z, 0.0));
   /* The hand moves: the item with it. */
   aim.position.x = 0.2f;
   CHECK(!video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL));
   CHECK(NEAR(s.x, 0.2) && NEAR(s.y, 0.0) && NEAR(s.z, 0.0));
   /* The wrist rolls 30 degrees: level, the item does not... */
   aim.orientation.z = (float)sin(M_PI / 12.0);
   aim.orientation.w = (float)cos(M_PI / 12.0);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.roll, 0.0f) && angle_near(s.yaw, 0.0f)
         && angle_near(s.pitch, 0.0f));
   /* ...tilting, it does. */
   g.tilt = true;
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.roll, 30.0f));
   /* Level again at the roll it was left at. */
   g.tilt            = false;
   video_xr_grab_hold(&g, &aim, &pose.orientation);
   aim.orientation.z = 0.0f;
   aim.orientation.w = 1.0f;
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.roll, 30.0f));
   /* At a limit the item stops: 10 m right of its spot at most. */
   aim.position.x = 10.5f;
   CHECK(video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL));
   CHECK(NEAR(s.x, 10.0) && NEAR(pose.position.x, 10.0));
   /* Held again from there, as the caller does, it resumes from the
    * limit, not from where the hand went. */
   video_xr_pose_invert(&aim, &inv);
   video_xr_compose(&inv, &pose, &g.rel);
   aim.position.x = 10.4f;
   CHECK(!video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL));
   CHECK(NEAR(s.x, 9.9));
   /* out may be NULL: only the pose. */
   CHECK(!video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL));
   /* The ray rises 20 degrees: the item rides up with it, its pitch
    * held... */
   video_xr_pose_identity(&aim);
   video_xr_pose_identity(&g.rel);
   g.turn            = g.rel.orientation;
   g.rel.position.z  = -1.8f;
   aim.orientation.x = (float)sin(M_PI / 18.0);
   aim.orientation.w = (float)cos(M_PI / 18.0);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.pitch, 0.0f) && angle_near(s.yaw, 0.0f)
         && angle_near(s.roll, 0.0f));
   CHECK(NEAR(pose.position.y, 1.8 * sin(M_PI / 9.0)));
   /* ...tilting, it follows... */
   g.tilt = true;
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.pitch, 20.0f));
   /* ...and stays at the pitch it was left at. */
   g.tilt = false;
   video_xr_grab_hold(&g, &aim, &pose.orientation);
   video_xr_pose_identity(&aim);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.pitch, 20.0f) && angle_near(s.roll, 0.0f));
}

/* A stick axis past the deadzone, as a fraction of the travel left. */
static double past_deadzone(double v)
{
   double a = (fabs(v) - VIDEO_XR_EDIT_DEADZONE)
      / (1.0 - VIDEO_XR_EDIT_DEADZONE);
   if (a <= 0.0)
      return 0.0;
   return (v < 0.0) ? -a : a;
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
   /* 15 degrees off right pushes too: no lock to the nearest axis. */
   g.rel.position.z = -1.0f;
   scale            = 1.0f;
   video_xr_grab_stick(&g, (float)cos(M_PI / 12.0), (float)sin(M_PI / 12.0),
         1.0, &scale);
   CHECK(NEAR(g.rel.position.z, -1.0 - 1.5 * past_deadzone(sin(M_PI / 12.0)))
         && NEAR(scale, pow(2.0, past_deadzone(cos(M_PI / 12.0)))));
}

/* The free hand's stick turns the held item about the vertical through
 * its middle: right anticlockwise seen from above, as fast as the stick
 * is pushed past the deadzone; a pitched item keeps its pitch. */
static void test_grab_turn(void)
{
   video_xr_grab_t g;
   video_xr_pose_t aim, pose;
   video_screen_xr_t s;
   memset(&g, 0, sizeof(g));
   video_xr_pose_identity(&aim);
   video_xr_pose_identity(&g.rel);
   g.rel.position.x = 0.3f;
   g.rel.position.z = -1.8f;
   g.base           = g.rel.position;
   g.turn           = g.rel.orientation;
   /* Inside the deadzone: nothing. */
   video_xr_grab_turn(&g, &aim, 0.15f, 0.0f, 1.0);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.yaw, 0.0f));
   /* Full right for half a second: 45 degrees, in place. */
   video_xr_grab_turn(&g, &aim, 1.0f, 0.0f, 0.5);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.yaw, 45.0f) && NEAR(s.x, 0.0) && NEAR(s.y, 0.0)
         && NEAR(s.z, 0.0));
   /* Half past the deadzone left for a second: back 45. */
   video_xr_grab_turn(&g, &aim, -0.6f, 0.0f, 1.0);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.yaw, 0.0f));
   /* Pitched 15 degrees, a quarter turn keeps the pitch, no roll. */
   g.turn.x = (float)sin(M_PI / 24.0);
   g.turn.y = 0.0f;
   g.turn.w = (float)cos(M_PI / 24.0);
   video_xr_grab_turn(&g, &aim, 1.0f, 0.0f, 1.0);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.yaw, 90.0f) && angle_near(s.pitch, 15.0f)
         && angle_near(s.roll, 0.0f));
}

/* aim at the anchor turned to this yaw, pitch and roll. */
static void turn_aim(video_xr_pose_t *aim, float yaw, float pitch,
      float roll)
{
   video_xr_vec3_t at;
   video_screen_xr_t s;
   at.x    = 0.0f;
   at.y    = 0.0f;
   at.z    = 0.0f;
   video_views_xr_reset(&s, 1);
   s.yaw   = yaw;
   s.pitch = pitch;
   s.roll  = roll;
   video_xr_item_pose(&at, &s, aim);
}

/* g holding an item 1.8 m ahead at this yaw, pitch and roll, taken
 * with aim. */
static void turn_grab(video_xr_grab_t *g, const video_xr_pose_t *aim,
      float yaw, float pitch, float roll)
{
   video_xr_pose_t item, inv;
   video_screen_xr_t s;
   memset(g, 0, sizeof(*g));
   g->base.z = -1.8f;
   video_views_xr_reset(&s, 1);
   s.yaw   = yaw;
   s.pitch = pitch;
   s.roll  = roll;
   video_xr_item_pose(&g->base, &s, &item);
   video_xr_pose_invert(aim, &inv);
   video_xr_compose(&inv, &item, &g->rel);
   video_xr_grab_hold(g, aim, &item.orientation);
}

/* Whether ten steps of 0.1 s of the free hand's stick at full right
 * each turn g's item, held by aim, 9 degrees of shown yaw, keeping its
 * shown pitch and roll and its middle. */
static int turns_evenly(video_xr_grab_t *g, const video_xr_pose_t *aim)
{
   video_xr_pose_t start, pose;
   video_screen_xr_t s0, s;
   unsigned i;
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &start, &s0, NULL);
   for (i = 1; i <= 10; i++)
   {
      video_xr_grab_turn(g, aim, 1.0f, 0.0f, 0.1);
      video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, &s, NULL);
      /* Read back near straight up or down, a float turn's pitch is
       * good to a few hundredths of a degree. */
      if (!angle_near(s.yaw, s0.yaw + 9.0f * (float)i)
            || fabs((double)s.pitch - s0.pitch) > 0.1
            || !angle_near(s.roll, s0.roll)
            || !vec_is(&pose.position, start.position.x,
               start.position.y, start.position.z))
         return 0;
   }
   return 1;
}

/* The turn is about the vertical through the item's middle whatever its
 * pitch and wherever the ray points, carried level or tilting. */
static void test_grab_turn_pitched(void)
{
   video_xr_grab_t g;
   video_xr_pose_t taken, aim;
   video_xr_pose_identity(&taken);
   video_xr_pose_identity(&aim);
   turn_grab(&g, &taken, 0.0f, 60.0f, 0.0f);
   CHECK(turns_evenly(&g, &aim));
   turn_grab(&g, &taken, 0.0f, 80.0f, 0.0f);
   CHECK(turns_evenly(&g, &aim));
   /* Floor and ceiling screens. */
   turn_grab(&g, &taken, 30.0f, 90.0f, 0.0f);
   CHECK(turns_evenly(&g, &aim));
   turn_grab(&g, &taken, 30.0f, -90.0f, 0.0f);
   CHECK(turns_evenly(&g, &aim));
   /* Upright, the ray raised 60 degrees since the grab. */
   aim.orientation.x = (float)sin(M_PI / 6.0);
   aim.orientation.w = (float)cos(M_PI / 6.0);
   turn_grab(&g, &taken, 0.0f, 0.0f, 0.0f);
   CHECK(turns_evenly(&g, &aim));
   turn_grab(&g, &taken, 0.0f, 0.0f, 0.0f);
   g.tilt = true;
   CHECK(turns_evenly(&g, &aim));
   /* Rolled, taken by a rolled aim and turned by another. */
   turn_aim(&taken, 10.0f, 15.0f, 25.0f);
   turn_aim(&aim, -20.0f, 35.0f, -40.0f);
   turn_grab(&g, &taken, 20.0f, 40.0f, 30.0f);
   CHECK(turns_evenly(&g, &aim));
   turn_grab(&g, &taken, 20.0f, 40.0f, 30.0f);
   g.tilt = true;
   CHECK(turns_evenly(&g, &aim));
}

/* Whether an upright item, taken square to a level ray and turned a
 * quarter with the ray raised this many degrees, keeps that yaw as the
 * ray comes back down a degree at a time. */
static int keeps_turn(unsigned raised)
{
   video_xr_grab_t g;
   video_xr_pose_t aim, pose;
   video_screen_xr_t s;
   unsigned i;
   turn_aim(&aim, 40.0f, 0.0f, 0.0f);
   turn_grab(&g, &aim, 40.0f, 0.0f, 0.0f);
   turn_aim(&aim, 40.0f, (float)raised, 0.0f);
   if (!turns_evenly(&g, &aim))
      return 0;
   for (i = 0; i <= raised; i++)
   {
      turn_aim(&aim, 40.0f, (float)(raised - i), 0.0f);
      video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
      if (!angle_near(s.yaw, 130.0f) || !angle_near(s.pitch, 0.0f)
            || !angle_near(s.roll, 0.0f))
         return 0;
   }
   return 1;
}

/* Carried, a turn made with the ray raised stays as the ray comes down:
 * squared up there, the item is still square, inside
 * VIDEO_XR_SNAP_ANGLE, once lowered. */
static void test_grab_turn_lowered(void)
{
   CHECK(keeps_turn(60));
   CHECK(keeps_turn(20));
}

/* Tilting, the turned item rides the hand from where the turn left it:
 * turned side-on to the ray, raising the ray rolls it. */
static void test_grab_turn_tilt_ray(void)
{
   video_xr_grab_t g;
   video_xr_pose_t aim, pose;
   video_screen_xr_t s;
   turn_aim(&aim, 40.0f, 0.0f, 0.0f);
   turn_grab(&g, &aim, 40.0f, 0.0f, 0.0f);
   g.tilt = true;
   CHECK(turns_evenly(&g, &aim));
   turn_aim(&aim, 40.0f, 30.0f, 0.0f);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.yaw, 130.0f) && angle_near(s.pitch, 0.0f)
         && angle_near(s.roll, 30.0f));
   turn_aim(&aim, 40.0f, 0.0f, 0.0f);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.yaw, 130.0f) && angle_near(s.pitch, 0.0f)
         && angle_near(s.roll, 0.0f));
}

/* The trigger pressed or let go with g's hand at aim, as the hand's
 * move does it, where it last put the item. */
static void carry_tilt(video_xr_grab_t *g, const video_xr_pose_t *aim,
      bool tilt)
{
   video_xr_pose_t inv;
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &g->last, NULL, NULL);
   video_xr_pose_invert(aim, &inv);
   if (tilt && !g->tilt)
      video_xr_compose(&inv, &g->last, &g->rel);
   else if (!tilt && g->tilt)
      video_xr_grab_hold(g, aim, &g->last.orientation);
   g->tilt = tilt;
}

/* Degrees between two turns. */
static double turn_apart(const video_xr_quat_t *a, const video_xr_quat_t *b)
{
   double x = (double)a->w * b->x - (double)a->x * b->w
      - (double)a->y * b->z + (double)a->z * b->y;
   double y = (double)a->w * b->y + (double)a->x * b->z
      - (double)a->y * b->w - (double)a->z * b->x;
   double z = (double)a->w * b->z - (double)a->x * b->y
      + (double)a->y * b->x - (double)a->z * b->w;
   double w = (double)a->w * b->w + (double)a->x * b->x
      + (double)a->y * b->y + (double)a->z * b->z;
   return 2.0 * atan2(sqrt(x * x + y * y + z * z), fabs(w)) * 180.0 / M_PI;
}

/* Whether g shows its item at this yaw, pitch and roll for aim. */
static int carried_at(const video_xr_grab_t *g, const video_xr_pose_t *aim,
      float yaw, float pitch, float roll)
{
   video_xr_pose_t pose;
   video_screen_xr_t s;
   video_views_xr_reset(&s, 1);
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, &s, NULL);
   return angle_near(s.yaw, yaw) && angle_near(s.pitch, pitch)
      && angle_near(s.roll, roll);
}

/* Carried, an item keeps its yaw from the hand's heading: raising,
 * lowering or twisting the hand leaves it, and turning the heading
 * turns it as much. */
static void test_carry_heading(void)
{
   unsigned i;
   video_xr_grab_t g;
   video_xr_pose_t aim;
   int kept = 1;
   /* Square to a hand at 40, turned 45 by the stick, the ray raised
    * 60 and lowered again a degree at a time. */
   turn_aim(&aim, 40.0f, 0.0f, 0.0f);
   turn_grab(&g, &aim, 40.0f, 0.0f, 0.0f);
   for (i = 0; i < 5; i++)
      video_xr_grab_turn(&g, &aim, 1.0f, 0.0f, 0.1);
   for (i = 0; i <= 120; i++)
   {
      turn_aim(&aim, 40.0f, (float)(i <= 60 ? i : 120 - i), 0.0f);
      if (!carried_at(&g, &aim, 85.0f, 0.0f, 0.0f))
         kept = 0;
   }
   CHECK(kept);
   /* Taken 45 off the ray, the ray raised 60. */
   turn_aim(&aim, 40.0f, 0.0f, 0.0f);
   turn_grab(&g, &aim, 85.0f, 0.0f, 0.0f);
   turn_aim(&aim, 40.0f, 60.0f, 0.0f);
   CHECK(carried_at(&g, &aim, 85.0f, 0.0f, 0.0f));
   /* The wrist twisted 30, the ray level and raised 30. */
   turn_aim(&aim, 40.0f, 0.0f, 30.0f);
   CHECK(carried_at(&g, &aim, 85.0f, 0.0f, 0.0f));
   turn_aim(&aim, 40.0f, 30.0f, 30.0f);
   CHECK(carried_at(&g, &aim, 85.0f, 0.0f, 0.0f));
   /* The heading turned 30, the ray raised and twisted. */
   turn_aim(&aim, 70.0f, 20.0f, -10.0f);
   CHECK(carried_at(&g, &aim, 115.0f, 0.0f, 0.0f));
   /* A pitched and rolled item likewise. */
   turn_aim(&aim, -20.0f, 10.0f, 5.0f);
   turn_grab(&g, &aim, 25.0f, 30.0f, -20.0f);
   turn_aim(&aim, 10.0f, 60.0f, 30.0f);
   CHECK(carried_at(&g, &aim, 55.0f, 30.0f, -20.0f));
}

/* A full turn of the stick, a degree at a time, brings an item back
 * where it began; each step turns it a degree about the vertical. Near
 * straight down and rolled, the readback of its yaw is unsure of the
 * roll, which must not add up. */
static void test_carry_full_turn(void)
{
   unsigned k;
   video_xr_grab_t g;
   video_xr_pose_t taken, aim, start, pose, want, about;
   double worst = 0.0;
   turn_aim(&taken, -59.2f, 22.3f, 29.2f);
   turn_aim(&aim, -44.9f, 14.0f, 18.7f);
   turn_grab(&g, &taken, -14.429f, -89.743f, -31.768f);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &start, NULL, NULL);
   video_xr_pose_identity(&about);
   for (k = 1; k <= 360; k++)
   {
      double d;
      video_xr_grab_turn(&g, &aim, 1.0f, 0.0f, 1.0 / 90.0);
      video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL);
      about.orientation.y = (float)sin(k * M_PI / 360.0);
      about.orientation.w = (float)cos(k * M_PI / 360.0);
      video_xr_compose(&about, &start, &want);
      d = turn_apart(&want.orientation, &pose.orientation);
      if (!(d <= worst))
         worst = d;
   }
   CHECK(worst < 0.01);
   CHECK(turn_apart(&start.orientation, &pose.orientation) < 0.1);
   CHECK(vec_is(&pose.position, start.position.x, start.position.y,
            start.position.z));
}

/* A turn of deg degrees about +X (axis 0) or +Y (axis 1). */
static void axis_turn(video_xr_pose_t *out, int axis, double deg)
{
   video_xr_pose_identity(out);
   if (axis)
      out->orientation.y = (float)sin(deg * M_PI / 360.0);
   else
      out->orientation.x = (float)sin(deg * M_PI / 360.0);
   out->orientation.w = (float)cos(deg * M_PI / 360.0);
}

static double dist(const video_xr_vec3_t *a, const video_xr_vec3_t *b)
{
   double x = (double)a->x - b->x;
   double y = (double)a->y - b->y;
   double z = (double)a->z - b->z;
   return sqrt(x * x + y * y + z * z);
}

/* How far the top and bottom edges' middles of an item 0.9 m high at
 * pose are from at. */
static void edges_from(const video_xr_pose_t *pose,
      const video_xr_vec3_t *at, double *top, double *bottom)
{
   video_xr_pose_t edge, p;
   video_xr_pose_identity(&edge);
   edge.position.y = 0.45f;
   video_xr_compose(pose, &edge, &p);
   *top            = dist(&p.position, at);
   edge.position.y = -0.45f;
   video_xr_compose(pose, &edge, &p);
   *bottom         = dist(&p.position, at);
}

/* The free hand's stick up tips the held item's top away from the user
 * about its own left-right line through its middle, past the deadzone
 * as the turn is; down tips it back. */
static void test_grab_tilt(void)
{
   video_xr_grab_t g;
   video_xr_pose_t aim, pose;
   video_screen_xr_t s;
   double top0, bottom0, top, bottom;
   memset(&g, 0, sizeof(g));
   video_xr_pose_identity(&aim);
   video_xr_pose_identity(&g.rel);
   g.rel.position.z = -1.8f;
   g.base           = g.rel.position;
   g.turn           = g.rel.orientation;
   /* Inside the deadzone: nothing. */
   video_xr_grab_turn(&g, &aim, 0.0f, 0.15f, 1.0);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(turn_apart(&pose.orientation, &aim.orientation) < 0.01);
   edges_from(&pose, &aim.position, &top0, &bottom0);
   /* Full up for 0.1 s, a screen facing the user at the aim: 9 degrees,
    * its top edge farther and its bottom edge nearer, in place. */
   video_xr_grab_turn(&g, &aim, 0.0f, 1.0f, 0.1);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   edges_from(&pose, &aim.position, &top, &bottom);
   CHECK(top > top0 + 0.03 && bottom < bottom0 - 0.03);
   CHECK(angle_near(s.pitch, -9.0f) && angle_near(s.yaw, 0.0f)
         && angle_near(s.roll, 0.0f) && NEAR(s.x, 0.0) && NEAR(s.y, 0.0)
         && NEAR(s.z, 0.0));
   /* Half past the deadzone down for a second: 45 back. */
   video_xr_grab_turn(&g, &aim, 0.0f, -0.6f, 1.0);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.pitch, 36.0f) && angle_near(s.yaw, 0.0f)
         && angle_near(s.roll, 0.0f));
}

/* Whether ten steps of 0.1 s of the free hand's stick full up each tip
 * g's item, held by aim, 9 degrees more about its own left-right line,
 * keeping its middle. */
static int tilts_evenly(video_xr_grab_t *g, const video_xr_pose_t *aim)
{
   video_xr_pose_t start, pose, tip, want;
   unsigned i;
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &start, NULL, NULL);
   for (i = 1; i <= 10; i++)
   {
      video_xr_grab_turn(g, aim, 0.0f, 1.0f, 0.1);
      video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
      axis_turn(&tip, 0, -9.0 * i);
      video_xr_compose(&start, &tip, &want);
      if (!(turn_apart(&want.orientation, &pose.orientation) < 0.01)
            || !vec_is(&pose.position, start.position.x,
               start.position.y, start.position.z))
         return 0;
   }
   return 1;
}

/* Carried or tilting with the trigger, upright or turned every way. */
static void test_grab_tilt_evenly(void)
{
   video_xr_grab_t g;
   video_xr_pose_t taken, aim;
   video_xr_pose_identity(&taken);
   video_xr_pose_identity(&aim);
   turn_grab(&g, &taken, 0.0f, 0.0f, 0.0f);
   CHECK(tilts_evenly(&g, &aim));
   turn_grab(&g, &taken, 0.0f, 0.0f, 0.0f);
   g.tilt = true;
   CHECK(tilts_evenly(&g, &aim));
   /* Rolled, taken by a rolled aim and tilted by another. */
   turn_aim(&taken, 10.0f, 15.0f, 25.0f);
   turn_aim(&aim, -20.0f, 35.0f, -40.0f);
   turn_grab(&g, &taken, 20.0f, 40.0f, 30.0f);
   CHECK(tilts_evenly(&g, &aim));
   turn_grab(&g, &taken, 20.0f, 40.0f, 30.0f);
   g.tilt = true;
   CHECK(tilts_evenly(&g, &aim));
}

/* Whether the stick right and up together for 0.1 s turns g's item 9
 * degrees about the vertical and tips it 9 about its own left-right
 * line at once, keeping its middle. */
static int turns_and_tilts(video_xr_grab_t *g, const video_xr_pose_t *aim)
{
   video_xr_pose_t start, pose, about, tip, want;
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &start, NULL, NULL);
   video_xr_grab_turn(g, aim, 1.0f, 1.0f, 0.1);
   video_xr_grab_pose(g, aim, NULL, 0, NULL, &pose, NULL, NULL);
   axis_turn(&about, 1, 9.0);
   axis_turn(&tip, 0, -9.0);
   video_xr_compose(&start, &tip, &want);
   want.position = about.position;
   video_xr_compose(&about, &want, &want);
   return turn_apart(&want.orientation, &pose.orientation) < 0.01
      && vec_is(&pose.position, start.position.x, start.position.y,
            start.position.z);
}

static void test_grab_turn_and_tilt(void)
{
   video_xr_grab_t g;
   video_xr_pose_t taken, aim;
   turn_aim(&taken, 10.0f, 15.0f, 25.0f);
   turn_aim(&aim, -20.0f, 35.0f, -40.0f);
   turn_grab(&g, &taken, 20.0f, 40.0f, 30.0f);
   CHECK(turns_and_tilts(&g, &aim));
   turn_grab(&g, &taken, 20.0f, 40.0f, 30.0f);
   g.tilt = true;
   CHECK(turns_and_tilts(&g, &aim));
}

/* The free hand's stick pushed full, deg degrees anticlockwise from
 * right, for half a second: within VIDEO_XR_EDIT_TURN_LOCK degrees of
 * left or right it only turns an item facing the user, within that of
 * up or down it only tilts it, and between it does both. */
static void test_grab_turn_lock(void)
{
   static const struct
   {
      float deg;
      int turns, tilts;
   } pushes[] =
   {
      {  15.0f, 1, 0 }, { 165.0f, 1, 0 }, { 195.0f, 1, 0 },
      { 345.0f, 1, 0 }, {  75.0f, 0, 1 }, { 105.0f, 0, 1 },
      { 255.0f, 0, 1 }, { 285.0f, 0, 1 }, {  45.0f, 1, 1 },
      { 135.0f, 1, 1 }, { 225.0f, 1, 1 }, { 315.0f, 1, 1 },
      { VIDEO_XR_EDIT_TURN_LOCK - 1.0f, 1, 0 },
      { VIDEO_XR_EDIT_TURN_LOCK + 1.0f, 1, 1 },
      { 91.0f - VIDEO_XR_EDIT_TURN_LOCK, 0, 1 },
      { 89.0f - VIDEO_XR_EDIT_TURN_LOCK, 1, 1 },
      { 180.0f + VIDEO_XR_EDIT_TURN_LOCK - 1.0f, 1, 0 },
      { 269.0f + VIDEO_XR_EDIT_TURN_LOCK, 0, 1 }
   };
   unsigned i;
   for (i = 0; i < sizeof(pushes) / sizeof(pushes[0]); i++)
   {
      video_xr_grab_t g;
      video_xr_pose_t aim, pose;
      video_screen_xr_t s;
      double r = pushes[i].deg * M_PI / 180.0;
      float  x = (float)cos(r);
      float  y = (float)sin(r);
      float  yaw, pitch;
      yaw   = pushes[i].turns ? (float)(past_deadzone(x) * 45.0) : 0.0f;
      pitch = pushes[i].tilts ? (float)(-past_deadzone(y) * 45.0) : 0.0f;
      video_xr_pose_identity(&aim);
      turn_grab(&g, &aim, 0.0f, 0.0f, 0.0f);
      video_xr_grab_turn(&g, &aim, x, y, 0.5);
      video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
      if (!(angle_near(s.yaw, yaw) && angle_near(s.pitch, pitch)
            && angle_near(s.roll, 0.0f)))
      {
         printf("  FAIL %s:%d: stick at %.1f deg: yaw %.2f pitch %.2f,"
               " want %.2f %.2f\n", __FILE__, __LINE__, pushes[i].deg,
               s.yaw, s.pitch, yaw, pitch);
         failures++;
      }
   }
}

/* An item turned 45 degrees tips about its own left-right line, not the
 * world's: its yaw stays and it takes no roll. */
static void test_grab_tilt_turned(void)
{
   video_xr_grab_t g;
   video_xr_pose_t aim, pose;
   video_screen_xr_t s;
   video_xr_pose_identity(&aim);
   turn_grab(&g, &aim, 45.0f, 0.0f, 0.0f);
   video_xr_grab_turn(&g, &aim, 0.0f, 1.0f, 30.0 / VIDEO_XR_EDIT_TURN_RATE);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.yaw, 45.0f) && angle_near(s.pitch, -30.0f)
         && angle_near(s.roll, 0.0f));
   /* Turned 45 by the stick, tilting with the trigger. */
   turn_grab(&g, &aim, 0.0f, 0.0f, 0.0f);
   g.tilt = true;
   video_xr_grab_turn(&g, &aim, 1.0f, 0.0f, 0.5);
   video_xr_grab_turn(&g, &aim, 0.0f, 1.0f, 30.0 / VIDEO_XR_EDIT_TURN_RATE);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
   CHECK(angle_near(s.yaw, 45.0f) && angle_near(s.pitch, -30.0f)
         && angle_near(s.roll, 0.0f));
}

/* A full tilt of the stick, a degree at a time, through straight up
 * and down with no stop, brings an item back where it began; each step
 * tips it a degree about its own left-right line. */
static void test_tilt_full_turn(void)
{
   unsigned k, tilt;
   video_xr_grab_t g;
   video_xr_pose_t taken, aim, start, pose, tip, want;
   for (tilt = 0; tilt < 2; tilt++)
   {
      double worst = 0.0;
      turn_aim(&taken, -59.2f, 22.3f, 29.2f);
      turn_aim(&aim, -44.9f, 14.0f, 18.7f);
      turn_grab(&g, &taken, 30.0f, 20.0f, -15.0f);
      g.tilt = tilt != 0;
      video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &start, NULL, NULL);
      for (k = 1; k <= 360; k++)
      {
         double d;
         video_xr_grab_turn(&g, &aim, 0.0f, 1.0f, 1.0 / 90.0);
         video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL);
         axis_turn(&tip, 0, -(double)k);
         video_xr_compose(&start, &tip, &want);
         d = turn_apart(&want.orientation, &pose.orientation);
         if (!(d <= worst))
            worst = d;
      }
      CHECK(worst < 0.01);
      CHECK(turn_apart(&start.orientation, &pose.orientation) < 0.1);
      CHECK(vec_is(&pose.position, start.position.x, start.position.y,
               start.position.z));
   }
}

/* Whether the ray, at heading 40 and twisted roll, swept in 0.1 degree
 * steps from from to to and back, moves g's item less than a degree a
 * step and never to NaN; and, untwisted, keeps it at yaw. */
static int sweeps_smoothly(const video_xr_grab_t *g, float roll, int from,
      int to, float yaw)
{
   int i;
   video_xr_pose_t aim, pose, prev;
   video_screen_xr_t s;
   int n    = to > from ? to - from : from - to;
   int step = to > from ? 1 : -1;
   for (i = 0; i <= 2 * n; i++)
   {
      int at = from + step * (i <= n ? i : 2 * n - i);
      turn_aim(&aim, 40.0f, (float)at / 10.0f, roll);
      video_views_xr_reset(&s, 1);
      video_xr_grab_pose(g, &aim, NULL, 0, NULL, &pose, &s, NULL);
      if (!(fabs(pose.orientation.w) <= 1.0f) || !(s.yaw == s.yaw))
         return 0;
      if (i && !(turn_apart(&prev.orientation, &pose.orientation) < 1.0))
         return 0;
      if (roll == 0.0f && !angle_near(s.yaw, yaw))
         return 0;
      prev = pose;
   }
   return 1;
}

/* Pointing at the ceiling or the floor, a little past them, or
 * twisting there: nothing jumps or fails. Untwisted, the item keeps its
 * yaw; twisted, it is back on it once the ray is lowered. */
static void test_carry_vertical(void)
{
   unsigned i, r;
   video_xr_grab_t g;
   video_xr_pose_t aim, pose, prev;
   static const float rolls[] = { 0.0f, 30.0f, -60.0f };
   turn_aim(&aim, 40.0f, 0.0f, 0.0f);
   turn_grab(&g, &aim, 85.0f, 0.0f, 0.0f);
   for (r = 0; r < 3; r++)
   {
      int past = r ? 910 : 930;
      CHECK(sweeps_smoothly(&g, rolls[r], 600, past, 85.0f));
      CHECK(sweeps_smoothly(&g, rolls[r], -600, -past, 85.0f));
      turn_aim(&aim, 40.0f, 60.0f, rolls[r]);
      CHECK(carried_at(&g, &aim, 85.0f, 0.0f, 0.0f));
   }
   for (r = 0; r < 2; r++)
   {
      int ok = 1;
      for (i = 0; i <= 720; i++)
      {
         turn_aim(&aim, 40.0f, r ? -90.0f : 90.0f, (float)i * 0.5f);
         video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL);
         if (!(fabs(pose.orientation.w) <= 1.0f) || (i
               && !(turn_apart(&prev.orientation, &pose.orientation)
                  < 1.0)))
            ok = 0;
         prev = pose;
      }
      CHECK(ok);
   }
}

/* Some heading must jump on the path over the top: an untwisted ray
 * turns the item round once tipped 5 degrees past straight up or down. */
static void test_carry_flip(void)
{
   int up;
   video_xr_grab_t g;
   video_xr_pose_t aim;
   turn_aim(&aim, 40.0f, 0.0f, 0.0f);
   turn_grab(&g, &aim, 85.0f, 0.0f, 0.0f);
   for (up = 1; up >= -1; up -= 2)
   {
      turn_aim(&aim, 40.0f, (float)up * 94.0f, 0.0f);
      CHECK(carried_at(&g, &aim, 85.0f, 0.0f, 0.0f));
      turn_aim(&aim, 40.0f, (float)up * 96.0f, 0.0f);
      CHECK(carried_at(&g, &aim, -95.0f, 0.0f, 0.0f));
   }
}

/* Taking an item, and pressing or letting go of the trigger, leave it
 * where it is: at any pitch, straight up or down too, and rolled; with
 * the hand near level, past 80 degrees, or straight up or down. */
static void test_carry_still(void)
{
   static const float pitches[] = { 0.0f, 45.0f, -70.0f, 89.8f, -89.9f,
      90.0f, -90.0f };
   static const float rolls[]   = { 0.0f, 30.0f, -120.0f };
   /* The hand's yaw, pitch and roll at the grab, the trigger pressed
    * and the trigger let go. */
   static const float aims[3][3][3] = {
      { { -30.0f, 25.0f, 15.0f }, { 10.0f, -40.0f, -35.0f },
         { 60.0f, 50.0f, 70.0f } },
      { { -30.0f, 84.0f, 15.0f }, { 10.0f, 87.0f, -35.0f },
         { 60.0f, -82.0f, 70.0f } },
      { { -30.0f, 90.0f, 15.0f }, { 10.0f, 90.0f, -35.0f },
         { 60.0f, -90.0f, 70.0f } }
   };
   unsigned i, j;
   int still = 1;
   for (i = 0; i < sizeof(pitches) / sizeof(pitches[0]); i++)
      for (j = 0; j < 9; j++)
      {
         video_xr_grab_t g;
         video_xr_pose_t aim, item, pose, before;
         video_screen_xr_t s;
         const float (*a)[3] = aims[j / 3];
         video_views_xr_reset(&s, 1);
         s.yaw   = 20.0f;
         s.pitch = pitches[i];
         s.roll  = rolls[j % 3];
         turn_aim(&aim, a[0][0], a[0][1], a[0][2]);
         turn_grab(&g, &aim, s.yaw, s.pitch, s.roll);
         video_xr_item_pose(&g.base, &s, &item);
         video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL);
         if (turn_apart(&item.orientation, &pose.orientation) > 0.001
               || !vec_is(&pose.position, item.position.x,
                  item.position.y, item.position.z))
            still = 0;
         /* The trigger pressed with the hand moved, let go with it
          * moved again. */
         turn_aim(&aim, a[1][0], a[1][1], a[1][2]);
         video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &before, NULL, NULL);
         carry_tilt(&g, &aim, true);
         video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL);
         if (turn_apart(&before.orientation, &pose.orientation) > 0.001)
            still = 0;
         turn_aim(&aim, a[2][0], a[2][1], a[2][2]);
         video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &before, NULL, NULL);
         carry_tilt(&g, &aim, false);
         video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL);
         if (turn_apart(&before.orientation, &pose.orientation) > 0.001
               || !vec_is(&pose.position, before.position.x,
                  before.position.y, before.position.z))
            still = 0;
      }
   CHECK(still);
}

static void test_menu_handle(void)
{
   video_views_map_t map;
   video_xr_params_t p;
   video_xr_quad_set_t set;
   video_xr_quad_t menu, h, rim;
   video_xr_vec3_t o, d;
   video_xr_pose_t pose;
   float u, v, t;
   unsigned i;

   /* Under the menu's outline, 5 mm clear, 40% as wide, 3 cm tall. */
   map_3ds(&map);
   params_init(&p, &map);
   p.ui_dims = VIDEO_SCALE_PACK(1600, 960);
   video_xr_place(&p, &set);
   CHECK(set.quads[3].kind == VIDEO_XR_QUAD_MENU);
   video_xr_menu_handle(&set.quads[3], &h);
   CHECK(pos_is(&h, 0.0, -0.5309, -1.7));
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
   CHECK(pos_is(&h, 1.2886, 2.0, 3.0));
   CHECK(NEAR(h.width, 0.4) && NEAR(h.pose.orientation.z, sin(M_PI / 4.0)));

   /* Picked there, 10 cm along it, the point is the menu's; through
    * the menu, nothing. */
   memset(&set, 0, sizeof(set));
   set.quads[0]  = menu;
   set.num_quads = 1;
   o.x = 1.2886f; o.y = 2.1f; o.z = 4.0f;
   d.x = 0.0f;   d.y = 0.0f; d.z = -1.0f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 0);
   CHECK(NEAR(u, 0.6) && NEAR(v, 1.0773) && NEAR(t, 1.0));
   video_xr_cursor(&set.quads[0], u, v, t, &pose);
   CHECK(NEAR(pose.position.x, 1.2886) && NEAR(pose.position.y, 2.1));
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
   d.x = -1.7f; d.y = -0.5309f; d.z = -0.16f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 3);
   CHECK(NEAR(u, 0.6) && NEAR(v, 1.0531) && NEAR(t, 1.0));

   /* Its top 5 mm below the outline's outer edge, which grows with the
    * menu. */
   for (i = 0; i < 3; i++)
   {
      memset(&menu, 0, sizeof(menu));
      video_xr_pose_identity(&menu.pose);
      menu.width  = 0.5f + (float)i;
      menu.height = 0.3f + 0.6f * (float)i;
      menu.kind   = VIDEO_XR_QUAD_MENU;
      video_xr_outline(&menu, &rim);
      video_xr_menu_handle(&menu, &h);
      CHECK(fabs(-rim.height * 0.5 - 0.005
               - (h.pose.position.y + h.height * 0.5)) < 1e-5);
   }
}

/* Edit mode takes the menu by its handle bar before any screen, even a
 * nearer one; within VIDEO_XR_HANDLE_MARGIN of the bar only where the
 * ray meets nothing else. */
static void test_handle_first(void)
{
   video_xr_quad_set_t set;
   video_xr_vec3_t o, d;
   float u, v, t;

   /* The menu 1.6 x 0.96 at 1.7 m: its bar 0.64 x 0.03, centred 0.531
    * below its middle. A screen 1.28 x 0.96 at 1.5 m whose top edge is
    * 7 mm under the bar's middle. */
   memset(&set, 0, sizeof(set));
   video_xr_pose_identity(&set.quads[0].pose);
   set.quads[0].pose.position.y = -1.018f;
   set.quads[0].pose.position.z = -1.5f;
   set.quads[0].width           = 1.28f;
   set.quads[0].height          = 0.96f;
   set.quads[0].kind            = VIDEO_XR_QUAD_SCREEN;
   set.quads[0].slot            = 1;
   video_xr_pose_identity(&set.quads[1].pose);
   set.quads[1].pose.position.z = -1.7f;
   set.quads[1].width           = 1.6f;
   set.quads[1].height          = 0.96f;
   set.quads[1].kind            = VIDEO_XR_QUAD_MENU;
   set.quads[1].slot            = VIDEO_XR_MENU_SLOT;
   set.num_quads                = 2;
   d.x = 0.0f; d.y = 0.0f; d.z = -1.0f;
   o.x = 0.0f; o.z = 0.0f;

   /* On the bar, through the nearer screen: the menu. */
   o.y = -0.541f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 1);
   CHECK(NEAR(u, 0.5) && NEAR(t, 1.7) && NEAR(v, 0.5 + 0.541 / 0.96));
   /* 1.5 cm under the bar, in its margin over the screen: the screen. */
   o.y = -0.561f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 0);
   CHECK(NEAR(t, 1.5));
   /* In the margin over nothing, 2.5 cm past the bar's end or 1.5 cm
    * above it: the menu. */
   o.x = 0.345f;
   o.y = -0.531f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 1);
   CHECK(NEAR(u, 0.5 + 0.345 / 1.6));
   o.x = 0.0f;
   o.y = -0.501f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 1);
   CHECK(NEAR(v, 0.5 + 0.501 / 0.96) && NEAR(t, 1.7));
   /* Past the margin: the screen below, nothing beside. */
   o.x = 0.0f;
   o.y = -0.586f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 0);
   CHECK(NEAR(t, 1.5));
   o.x = 0.36f;
   o.y = -0.531f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == -1);
   /* Outside edit mode the bar is nothing: the screen. */
   o.x = 0.0f;
   o.y = -0.541f;
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_ALWAYS, false,
            &o, &d, &u, &v, &t) == 0);
}

/* Edit mode's sticky pick: a hand whose laser was on the bar keeps the
 * menu while the ray stays in the bar's 3 cm margin, even over the
 * nearer screen; it lets go past the margin. Only the bar sticks: the
 * margin over nothing takes the menu but does not stick, and arriving
 * from the screen the screen keeps its whole area. */
static void test_handle_sticky(void)
{
   video_xr_quad_set_t set;
   video_xr_vec3_t o, d;
   float u, v, t;
   bool stick = false;

   memset(&set, 0, sizeof(set));
   video_xr_pose_identity(&set.quads[0].pose);
   set.quads[0].pose.position.y = -1.018f;
   set.quads[0].pose.position.z = -1.5f;
   set.quads[0].width           = 1.28f;
   set.quads[0].height          = 0.96f;
   set.quads[0].kind            = VIDEO_XR_QUAD_SCREEN;
   set.quads[0].slot            = 1;
   video_xr_pose_identity(&set.quads[1].pose);
   set.quads[1].pose.position.z = -1.7f;
   set.quads[1].width           = 1.6f;
   set.quads[1].height          = 0.96f;
   set.quads[1].kind            = VIDEO_XR_QUAD_MENU;
   set.quads[1].slot            = VIDEO_XR_MENU_SLOT;
   set.num_quads                = 2;
   d.x = 0.0f; d.y = 0.0f; d.z = -1.0f;
   o.x = 0.0f; o.z = 0.0f;

   /* On the bar: the menu, and it sticks. */
   o.y = -0.541f;
   CHECK(video_xr_pick_edit(&set, false, &o, &d, &u, &v, &t, &stick) == 1
         && stick);
   /* Stuck, 1.5 cm under the bar over the screen: still the menu. */
   o.y = -0.561f;
   CHECK(video_xr_pick_edit(&set, true, &o, &d, &u, &v, &t, &stick) == 1
         && stick);
   CHECK(NEAR(t, 1.7) && NEAR(v, 0.5 + 0.561 / 0.96) && NEAR(u, 0.5));
   /* Not stuck there: the screen, as video_xr_pick. */
   CHECK(video_xr_pick_edit(&set, false, &o, &d, &u, &v, &t, &stick) == 0
         && !stick && NEAR(t, 1.5));
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false, &o, &d, &u, &v,
            &t) == 0);
   /* Stuck, past the margin: the screen, unstuck. */
   o.y = -0.586f;
   CHECK(video_xr_pick_edit(&set, true, &o, &d, &u, &v, &t, &stick) == 0
         && !stick);
   /* Stuck, beside the bar past its margin, over nothing: nothing. */
   o.x = 0.36f;
   o.y = -0.531f;
   CHECK(video_xr_pick_edit(&set, true, &o, &d, &u, &v, &t, &stick) == -1
         && !stick);
   /* In the margin over nothing: the menu, but it does not stick. */
   o.x = 0.345f;
   CHECK(video_xr_pick_edit(&set, false, &o, &d, &u, &v, &t, &stick) == 1
         && !stick);
   /* Stuck with the menu gone: the screen, unstuck. */
   o.x           = 0.0f;
   o.y           = -0.561f;
   set.num_quads = 1;
   CHECK(video_xr_pick_edit(&set, true, &o, &d, &u, &v, &t, &stick) == 0
         && !stick);
   /* stick may be NULL. */
   set.num_quads = 2;
   CHECK(video_xr_pick_edit(&set, true, &o, &d, &u, &v, &t, NULL) == 1);
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
   d.x = 0.0f; d.y = -0.5309f; d.z = -1.7f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 3);
   CHECK(NEAR(u, 0.5) && NEAR(v, 1.0531) && NEAR(t, 1.0));
   video_xr_cursor(&set.quads[3], u, v, t, &pose);
   CHECK(NEAR(pose.position.x, 0.0) && NEAR(pose.position.y, -0.5309));
   /* The handle is edit mode's alone. */
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_AUTO, true,
            &o, &d, &u, &v, &t) == 2);
   /* Through the gap between the menu and its handle, in the bar's
    * margin and just above it: what is behind, screen 1. */
   o.y = 1.455f;
   d.x = 0.0f; d.y = -1.15f; d.z = -1.0f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 2);
   o.y = 1.472f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 2);
   /* From the head, screen 1's top middle, 3.2 cm under screen 0: above
    * and below the bar, in its margin, screen 1; on the bar, the
    * menu. */
   o.y = 0.0f;
   d.x = 0.0f; d.y = -0.53f; d.z = -1.8f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 2);
   CHECK(NEAR(t, 1.0));
   d.y = -0.595f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 2);
   d.y = -0.56f;
   CHECK(video_xr_pick(&set, VIDEO_XR_LIVE_EDIT, false,
            &o, &d, &u, &v, &t) == 3);
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

/* By default the menu at its spot, wherever screen 1 is, as before
 * screens moved. With menu_follow: 10 cm in front of screen 1 and
 * facing as it does, after the screens, as large to the eye as at its
 * spot and no nearer than 0.4 m; at its own place once placed; at its
 * spot when screen 1 does not float or show, or would leave it facing
 * away. */
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
   CHECK(m->kind == VIDEO_XR_QUAD_MENU && pos_is(m, 0.0, 0.0, -1.7));
   CHECK(NEAR(m->pose.orientation.w, 1.0));
   CHECK(NEAR(m->width, 1.6) && NEAR(m->height, 0.96));

   p.menu_follow = true;
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
   p.menu_follow = true;
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

/* One of a pose's own axes, in its space. */
static void pose_axis(const video_xr_pose_t *pose, float x, float y,
      float z, video_xr_vec3_t *out)
{
   video_xr_vec3_t v;
   v.x = x;
   v.y = y;
   v.z = z;
   video_xr_rotate(&pose->orientation, &v, out);
}

/* The laser's beam: centred on its ray, its x down the ray, turned
 * about it so its face is towards the eye; seen end on, still square to
 * the ray. */
static void test_beam(void)
{
   video_xr_vec3_t o, d, eye, ax, ay, az;
   video_xr_pose_t pose;
   const video_xr_quat_t *q = &pose.orientation;

   o.x   = 0.2f;
   o.y   = -0.3f;
   o.z   = 0.0f;
   d.x   = 0.0f;
   d.y   = 0.0f;
   d.z   = -1.0f;
   eye.x = 0.0f;
   eye.y = 0.0f;
   eye.z = 0.0f;
   video_xr_beam(&o, &d, 1.8f, &eye, &pose);
   pose_axis(&pose, 1.0f, 0.0f, 0.0f, &ax);
   pose_axis(&pose, 0.0f, 1.0f, 0.0f, &ay);
   pose_axis(&pose, 0.0f, 0.0f, 1.0f, &az);
   CHECK(vec_is(&pose.position, 0.2, -0.3, -0.9));
   CHECK(vec_is(&ax, 0.0, 0.0, -1.0));
   CHECK(vec_is(&az, -0.5547, 0.8321, 0.0));
   CHECK(vec_is(&ay, -0.8321, -0.5547, 0.0));
   CHECK(NEAR(q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w, 1.0));

   /* A ray down and to the right, the eye above it. */
   d.x   = 0.6f;
   d.z   = -0.8f;
   eye.y = 1.0f;
   video_xr_beam(&o, &d, 1.0f, &eye, &pose);
   pose_axis(&pose, 1.0f, 0.0f, 0.0f, &ax);
   pose_axis(&pose, 0.0f, 0.0f, 1.0f, &az);
   CHECK(vec_is(&pose.position, 0.5, -0.3, -0.4));
   CHECK(vec_is(&ax, 0.6, 0.0, -0.8));
   CHECK(vec_is(&az, -0.0977, 0.9925, -0.0733));

   /* The eye on the ray's line: any face square to the ray. */
   o.x   = 0.0f;
   o.y   = 0.0f;
   d.x   = 0.0f;
   d.z   = -1.0f;
   eye.y = 0.0f;
   eye.z = 0.5f;
   video_xr_beam(&o, &d, 1.5f, &eye, &pose);
   pose_axis(&pose, 1.0f, 0.0f, 0.0f, &ax);
   pose_axis(&pose, 0.0f, 0.0f, 1.0f, &az);
   CHECK(vec_is(&ax, 0.0, 0.0, -1.0));
   CHECK(vec_is(&az, 0.0, 1.0, 0.0));
   d.y   = 1.0f;
   d.z   = 0.0f;
   eye.y = -1.0f;
   eye.z = 0.0f;
   video_xr_beam(&o, &d, 1.0f, &eye, &pose);
   pose_axis(&pose, 1.0f, 0.0f, 0.0f, &ax);
   pose_axis(&pose, 0.0f, 0.0f, 1.0f, &az);
   CHECK(vec_is(&ax, 0.0, 1.0, 0.0));
   CHECK(vec_is(&az, 0.0, 0.0, 1.0));
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

/* The laser's image: the dot; edit mode's faint and bright outlines,
 * each rim three bands, dark, light, dark, clear inside; the menu's
 * faint and bright handle bars, the same bands across; the beam's
 * strips; a clear column between each. */
static void test_cursor_pixels(void)
{
   const unsigned mid    = VIDEO_XR_OUTLINE_DIM / 2;
   const unsigned b      = VIDEO_XR_OUTLINE_BAND;
   const unsigned hx     = VIDEO_XR_HANDLE_COLS / 2;
   const uint8_t *centre = texel(VIDEO_XR_CURSOR_DIM / 2,
         VIDEO_XR_CURSOR_Y + VIDEO_XR_CURSOR_DIM / 2);
   const uint8_t *rim    = texel(VIDEO_XR_CURSOR_DIM / 2,
         VIDEO_XR_CURSOR_Y + 2);
   const uint8_t *corner = texel(0, VIDEO_XR_CURSOR_Y);
   const uint8_t *hit_in   = texel(VIDEO_XR_BEAM_X(0), VIDEO_XR_BEAM_Y + 1);
   const uint8_t *hit_mid  = texel(VIDEO_XR_BEAM_X(0)
         + VIDEO_XR_BEAM_LEN / 2, VIDEO_XR_BEAM_Y + 1);
   const uint8_t *hit_end  = texel(VIDEO_XR_BEAM_X(0)
         + VIDEO_XR_BEAM_LEN - 1, VIDEO_XR_BEAM_Y + VIDEO_XR_BEAM_ROWS - 1);
   const uint8_t *miss_in  = texel(VIDEO_XR_BEAM_X(1), VIDEO_XR_BEAM_Y);
   const uint8_t *miss_mid = texel(VIDEO_XR_BEAM_X(1)
         + VIDEO_XR_BEAM_LEN / 2, VIDEO_XR_BEAM_Y + 2);
   const uint8_t *miss_end = texel(VIDEO_XR_BEAM_X(1)
         + VIDEO_XR_BEAM_LEN - 1, VIDEO_XR_BEAM_Y + 1);
   unsigned k;

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
   CHECK(VIDEO_XR_HANDLE_Y * 2 + VIDEO_XR_HANDLE_ROWS == VIDEO_XR_CURSOR_H);
   /* Two texels a band at least, so filtering keeps three. */
   CHECK(b >= 2 && VIDEO_XR_OUTLINE_EDGE == 3 * b);
   for (k = 0; k < 2; k++)
   {
      unsigned x0     = VIDEO_XR_OUTLINE_X(k);
      unsigned h0     = VIDEO_XR_HANDLE_X(k);
      uint8_t  a      = k ? 255 : VIDEO_XR_OUTLINE_FAINT;
      const uint8_t *top_dark  = texel(x0 + mid, 0);
      const uint8_t *top_light = texel(x0 + mid, b);
      const uint8_t *top_inner = texel(x0 + mid, 2 * b);
      const uint8_t *inside    = texel(x0 + mid, 3 * b);
      const uint8_t *left      = texel(x0, mid);
      const uint8_t *left_lit  = texel(x0 + b + 1, mid);
      const uint8_t *right     = texel(x0 + VIDEO_XR_OUTLINE_DIM - 1, mid);
      const uint8_t *bottom    = texel(x0 + mid, VIDEO_XR_OUTLINE_DIM - 1);
      const uint8_t *bot_light = texel(x0 + mid,
            VIDEO_XR_OUTLINE_DIM - 1 - b);
      const uint8_t *bar_dark  = texel(h0 + hx, VIDEO_XR_HANDLE_Y);
      const uint8_t *bar_light = texel(h0 + hx, VIDEO_XR_HANDLE_Y
            + VIDEO_XR_HANDLE_ROWS / 2);
      const uint8_t *bar_low   = texel(h0 + hx, VIDEO_XR_HANDLE_Y
            + VIDEO_XR_HANDLE_ROWS - 1);
      const uint8_t *bar_end   = texel(h0, VIDEO_XR_HANDLE_Y
            + VIDEO_XR_HANDLE_ROWS / 2);
      CHECK(!top_dark[0] && top_dark[3] == a);
      CHECK(top_light[0] == a && top_light[2] == a && top_light[3] == a);
      CHECK(!top_inner[0] && top_inner[3] == a);
      CHECK(!inside[0] && !inside[3]);
      CHECK(!left[0] && left[3] == a);
      CHECK(left_lit[0] == a && left_lit[3] == a);
      CHECK(!right[0] && right[3] == a);
      CHECK(!bottom[0] && bottom[3] == a);
      CHECK(bot_light[0] == a && bot_light[3] == a);
      CHECK(!bar_dark[0] && bar_dark[3] == a);
      CHECK(bar_light[0] == a && bar_light[3] == a);
      CHECK(!bar_low[0] && bar_low[3] == a);
      CHECK(!bar_end[0] && bar_end[3] == a);
      CHECK(!texel(h0 + hx, VIDEO_XR_HANDLE_Y - 1)[3]);
      CHECK(!texel(h0 + hx, VIDEO_XR_HANDLE_Y + VIDEO_XR_HANDLE_ROWS)[3]);
   }
   /* Filtering at a part's edge meets a clear texel, not the next. */
   CHECK(column_clear(VIDEO_XR_CURSOR_DIM));
   CHECK(column_clear(VIDEO_XR_OUTLINE_X(1) - 1));
   CHECK(column_clear(VIDEO_XR_HANDLE_X(0) - 1));
   CHECK(column_clear(VIDEO_XR_HANDLE_X(1) - 1));
   CHECK(column_clear(VIDEO_XR_BEAM_X(0) - 1));
   CHECK(column_clear(VIDEO_XR_BEAM_X(1) - 1));
   CHECK(VIDEO_XR_BEAM_X(1) + VIDEO_XR_BEAM_LEN + 1 == VIDEO_XR_BACK_X);
   /* The beam's strips: white, faded in at the hand; the one into
    * nothing faded out at its far end too; mid-height, nothing above or
    * below them. */
   CHECK(hit_mid[0] == VIDEO_XR_BEAM_LEVEL && hit_mid[2] == VIDEO_XR_BEAM_LEVEL
         && hit_mid[3] == VIDEO_XR_BEAM_LEVEL);
   CHECK(hit_end[3] == VIDEO_XR_BEAM_LEVEL);
   CHECK(hit_in[3] > 0 && hit_in[3] < VIDEO_XR_BEAM_LEVEL / 8
         && hit_in[0] == hit_in[3]);
   CHECK(miss_mid[3] == VIDEO_XR_BEAM_LEVEL);
   CHECK(miss_in[3] == hit_in[3]);
   CHECK(miss_end[3] > 0 && miss_end[3] < VIDEO_XR_BEAM_LEVEL / 32);
   CHECK(VIDEO_XR_BEAM_Y * 2 + VIDEO_XR_BEAM_ROWS == VIDEO_XR_CURSOR_H);
   CHECK(!texel(VIDEO_XR_BEAM_X(0) + 64, VIDEO_XR_BEAM_Y - 1)[3]);
   CHECK(!texel(VIDEO_XR_BEAM_X(1) + 64, VIDEO_XR_BEAM_Y
            + VIDEO_XR_BEAM_ROWS)[3]);
}

/* Edit mode's tags: a cell for each item it can choose after the
 * hinge's strip, each centred on the middle row with a clear column
 * between, empty until text goes in. Text is light over a dark halo two
 * texels wide, at the tag's faint level; bottom up, its rows mirror
 * within the cell. */
static void test_tag_pixels(void)
{
   static uint8_t cov[VIDEO_XR_TAG_COLS * VIDEO_XR_TAG_ROWS];
   const unsigned x0 = VIDEO_XR_TAG_X(3);
   const unsigned y0 = VIDEO_XR_TAG_Y;
   const unsigned r  = VIDEO_XR_TAG_ROWS - 1;
   const uint8_t f   = VIDEO_XR_TAG_FAINT;
   unsigned x, y;
   int clear = 1;
   CHECK(VIDEO_XR_TAGS == VIDEO_XR_MENU_SLOT + 2);
   CHECK(VIDEO_XR_TAG_X(0) == VIDEO_XR_HINGE_X + VIDEO_XR_HINGE_COLS + 1);
   CHECK(VIDEO_XR_TAG_X(VIDEO_XR_TAGS - 1) + VIDEO_XR_TAG_COLS
         == VIDEO_XR_CURSOR_TAGGED_W);
   CHECK(2 * VIDEO_XR_TAG_Y + VIDEO_XR_TAG_ROWS == VIDEO_XR_CURSOR_H);
   video_xr_cursor_pixels(t_atlas);
   for (x = VIDEO_XR_TAG_X(0) - 1; x < VIDEO_XR_CURSOR_W; x++)
      clear &= column_clear(x);
   CHECK(clear);
   /* A 4 x 2 block, its top left at 60, 9: off the middle, so the rows
    * bottom up differ. */
   memset(cov, 0, sizeof(cov));
   for (y = 9; y < 11; y++)
      for (x = 60; x < 64; x++)
         cov[y * VIDEO_XR_TAG_COLS + x] = 255;
   video_xr_tag_pixels(t_atlas, 3, cov, false);
   CHECK(texel(x0 + 61, y0 + 10)[0] == f && texel(x0 + 61, y0 + 10)[3] == f);
   CHECK(texel(x0 + 58, y0 + 10)[0] == 0 && texel(x0 + 58, y0 + 10)[3] == f);
   CHECK(texel(x0 + 57, y0 + 10)[3] == 0);
   CHECK(texel(x0 + 61, y0 + 7)[0] == 0 && texel(x0 + 61, y0 + 7)[3] == f);
   CHECK(texel(x0 + 61, y0 + 6)[3] == 0);
   CHECK(column_clear(x0 - 1) && column_clear(x0 + VIDEO_XR_TAG_COLS));
   /* Bottom up: row y at r - y. */
   video_xr_tag_pixels(t_atlas, 3, cov, true);
   CHECK(texel(x0 + 61, y0 + r - 10)[0] == f
         && texel(x0 + 61, y0 + r - 7)[0] == 0
         && texel(x0 + 61, y0 + r - 7)[3] == f
         && texel(x0 + 61, y0 + r - 6)[3] == 0
         && texel(x0 + 61, y0 + 10)[3] == 0);
}

/* Edit mode's cut outlines after the tags, a clear column before each:
 * for each cut, faint and bright, the whole outline's texels but those
 * VIDEO_XR_OUTLINE_CROP in from each cut edge, which are clear, its
 * corners included; bottom up, the top's and the bottom's rows swap. */
static void test_cut_pixels(void)
{
   static const unsigned cuts[VIDEO_XR_OUTLINE_CUTS] = {
      1u, 2u, 4u, 8u, 3u, 12u };
   const unsigned d = VIDEO_XR_OUTLINE_DIM;
   const unsigned e = VIDEO_XR_OUTLINE_CROP;
   unsigned up, k, c, x, y;
   CHECK(VIDEO_XR_CURSOR_W == 5582 && VIDEO_XR_CURSOR_TAGGED_W == 2498
         && VIDEO_XR_CURSOR_UNTAGGED_W == 1208);
   CHECK(VIDEO_XR_CUT_X(0, 0) == VIDEO_XR_CURSOR_TAGGED_W + 1);
   CHECK(VIDEO_XR_CUT_X(VIDEO_XR_OUTLINE_CUTS - 1, 1) + d
         == VIDEO_XR_CURSOR_W);
   for (up = 0; up < 2; up++)
   {
      unsigned bad = 0;
      memset(t_atlas, 0x7f, sizeof(t_atlas));
      video_xr_cursor_pixels(t_atlas);
      video_xr_cut_pixels(t_atlas, up != 0);
      for (k = 0; k < 2; k++)
         for (c = 0; c < VIDEO_XR_OUTLINE_CUTS; c++)
         {
            unsigned x0  = VIDEO_XR_CUT_X(c, k);
            unsigned cut = cuts[c];
            if (up)
               cut = (cut & 12u) | ((cut & 1u) << 1) | ((cut >> 1) & 1u);
            bad += !column_clear(x0 - 1);
            for (y = 0; y < d; y++)
               for (x = 0; x < d; x++)
               {
                  const uint8_t *t     = texel(x0 + x, y);
                  const uint8_t *whole = texel(VIDEO_XR_OUTLINE_X(k) + x, y);
                  int gone = ((cut & 1u) && y < e)
                     || ((cut & 2u) && y >= d - e)
                     || ((cut & 4u) && x >= d - e)
                     || ((cut & 8u) && x < e);
                  bad += gone ? (t[3] != 0) : (memcmp(t, whole, 4) != 0);
               }
         }
      CHECK(!bad);
   }
}

/* A screen's back: a solid cell after the beam strips, before the
 * tags, so a runtime too narrow for the tags keeps it; centred on the
 * middle row, the square a back shows kept its margin inside, so
 * filtering takes in nothing else. Opaque dark grey. */
static void test_back_pixels(void)
{
   const unsigned x0 = VIDEO_XR_BACK_X;
   const unsigned y0 = VIDEO_XR_BACK_Y;
   const uint8_t l   = VIDEO_XR_BACK_LEVEL;
   unsigned x, y;
   int solid = 1;
   CHECK(VIDEO_XR_BACK_X == VIDEO_XR_BEAM_X(1) + VIDEO_XR_BEAM_LEN + 1);
   CHECK(VIDEO_XR_BACK_CELL
         == VIDEO_XR_BACK_DIM + 2 * VIDEO_XR_BACK_MARGIN);
   CHECK(VIDEO_XR_BACK_MARGIN >= 2);
   CHECK(VIDEO_XR_HINGE_X == x0 + VIDEO_XR_BACK_CELL + 1);
   CHECK(2 * VIDEO_XR_BACK_Y + VIDEO_XR_BACK_CELL == VIDEO_XR_CURSOR_H);
   CHECK(l > 0 && l < 64);
   video_xr_cursor_pixels(t_atlas);
   for (y = y0; y < y0 + VIDEO_XR_BACK_CELL; y++)
      for (x = x0; x < x0 + VIDEO_XR_BACK_CELL; x++)
         solid &= texel(x, y)[0] == l && texel(x, y)[1] == l
            && texel(x, y)[2] == l && texel(x, y)[3] == 255;
   CHECK(solid);
   CHECK(column_clear(x0 - 1) && column_clear(x0 + VIDEO_XR_BACK_CELL));
   CHECK(!texel(x0, y0 - 1)[3] && !texel(x0, y0 + VIDEO_XR_BACK_CELL)[3]);
}

/* A hinge's line: a uniform green strip, red equal to blue so RGBA and
 * BGRA hold the same bytes, premultiplied at VIDEO_XR_HINGE_LEVEL;
 * after the back's cell and before the tags, so a runtime too narrow
 * for the tags keeps it; centred on the middle row, a clear column
 * either side. */
static void test_hinge_pixels(void)
{
   const unsigned x0 = VIDEO_XR_HINGE_X;
   const unsigned y0 = VIDEO_XR_HINGE_Y;
   const unsigned rb = (VIDEO_XR_HINGE_RB * VIDEO_XR_HINGE_LEVEL + 127) / 255;
   const unsigned g  = (VIDEO_XR_HINGE_G * VIDEO_XR_HINGE_LEVEL + 127) / 255;
   unsigned x, y;
   int solid = 1;
   CHECK(VIDEO_XR_HINGE_X == VIDEO_XR_BACK_X + VIDEO_XR_BACK_CELL + 1);
   CHECK(VIDEO_XR_TAG_X(0) == VIDEO_XR_HINGE_X + VIDEO_XR_HINGE_COLS + 1);
   CHECK(VIDEO_XR_CURSOR_UNTAGGED_W == x0 + VIDEO_XR_HINGE_COLS);
   CHECK(2 * VIDEO_XR_HINGE_Y + VIDEO_XR_HINGE_ROWS == VIDEO_XR_CURSOR_H);
   CHECK(VIDEO_XR_HINGE_G > VIDEO_XR_HINGE_RB && g > rb);
   video_xr_cursor_pixels(t_atlas);
   for (y = y0; y < y0 + VIDEO_XR_HINGE_ROWS; y++)
      for (x = x0; x < x0 + VIDEO_XR_HINGE_COLS; x++)
         solid &= texel(x, y)[0] == rb && texel(x, y)[1] == g
            && texel(x, y)[2] == rb
            && texel(x, y)[3] == VIDEO_XR_HINGE_LEVEL;
   CHECK(solid);
   CHECK(column_clear(x0 - 1) && column_clear(x0 + VIDEO_XR_HINGE_COLS));
   CHECK(!texel(x0 + 64, y0 - 1)[3]
         && !texel(x0 + 64, y0 + VIDEO_XR_HINGE_ROWS)[3]);
}

/* A quad's back: its plane, its front the other way, its up the same;
 * the head is behind it on the far side of its front. Edit mode picks
 * by the plane, so a hand behind meets the back where the front is;
 * other lasers only the front. */
static void test_back_pick(void)
{
   video_xr_quad_set_t set;
   video_xr_quad_t q;
   video_xr_vec3_t o, d, eye, ax, az;
   float u = 0.0f, v = 0.0f, t = 0.0f;
   memset(&q, 0, sizeof(q));
   q.pose.orientation.y = (float)sin(M_PI / 12.0);
   q.pose.orientation.w = (float)cos(M_PI / 12.0);
   q.pose.position.z    = -1.8f;
   q.width              = 1.6f;
   q.height             = 0.96f;
   q.kind               = VIDEO_XR_QUAD_SCREEN;
   q.screen             = 1;
   set.quads[0]         = q;
   video_xr_turn_back(&set.quads[0].pose);
   ax.x = 1.0f; ax.y = 0.0f; ax.z = 0.0f;
   video_xr_rotate(&set.quads[0].pose.orientation, &ax, &ax);
   az.x = 0.0f; az.y = 0.0f; az.z = 1.0f;
   video_xr_rotate(&set.quads[0].pose.orientation, &az, &az);
   CHECK(pos_is(&set.quads[0], 0.0, 0.0, -1.8));
   CHECK(vec_is(&ax, -cos(M_PI / 6.0), 0.0, sin(M_PI / 6.0)));
   CHECK(vec_is(&az, -sin(M_PI / 6.0), 0.0, -cos(M_PI / 6.0)));
   eye.x = 0.0f; eye.y = 0.3f; eye.z = 0.0f;
   CHECK(!video_xr_quad_behind(&q, &eye));
   CHECK(video_xr_quad_behind(&set.quads[0], &eye));
   eye.z = -3.0f;
   CHECK(video_xr_quad_behind(&q, &eye));
   CHECK(!video_xr_quad_behind(&set.quads[0], &eye));

   /* Square on, from behind, 40 cm right and 20 up of its middle. */
   memset(&set, 0, sizeof(set));
   set.quads[0]                  = q;
   video_xr_pose_identity(&set.quads[0].pose);
   set.quads[0].pose.position.z  = -1.8f;
   set.num_quads                 = 1;
   o.x = 0.4f; o.y = 0.2f; o.z = -3.0f;
   d.x = 0.0f; d.y = 0.0f; d.z = 1.0f;
   CHECK(video_xr_pick_edit(&set, false, &o, &d, &u, &v, &t, NULL) == 0);
   CHECK(NEAR(u, 0.75) && NEAR(v, 0.5 - 0.2 / 0.96) && NEAR(t, 1.2));
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_ALWAYS, true,
            &o, &d, &u, &v, &t) == -1);
   /* The menu's handle bar too. */
   set.quads[0].kind = VIDEO_XR_QUAD_MENU;
   o.x = 0.0f;
   o.y = -0.531f;
   CHECK(video_xr_pick_edit(&set, false, &o, &d, &u, &v, &t, NULL) == 0);
   CHECK(video_xr_pick(&set, VIDEO_OPENXR_LASER_ALWAYS, true,
            &o, &d, &u, &v, &t) == -1);
}

/* The tag: 5 cm over the hand, its face towards the eye, its x level;
 * with the eye straight above, facing up. */
static void test_tag_pose(void)
{
   double n;
   video_xr_vec3_t hand, eye, ax, az;
   video_xr_pose_t pose;
   hand.x = 0.2f;
   hand.y = -0.3f;
   hand.z = -0.3f;
   eye.x  = 0.0f;
   eye.y  = 0.0f;
   eye.z  = 0.0f;
   video_xr_tag(&hand, &eye, &pose);
   CHECK(vec_is(&pose.position, 0.2, -0.25, -0.3));
   pose_axis(&pose, 1.0f, 0.0f, 0.0f, &ax);
   pose_axis(&pose, 0.0f, 0.0f, 1.0f, &az);
   n = sqrt(0.2 * 0.2 + 0.25 * 0.25 + 0.3 * 0.3);
   CHECK(vec_is(&az, -0.2 / n, 0.25 / n, 0.3 / n));
   CHECK(NEAR(ax.y, 0.0) && ax.x > 0.0f);
   eye.x = 0.2f;
   eye.y = 1.0f;
   eye.z = -0.3f;
   video_xr_tag(&hand, &eye, &pose);
   pose_axis(&pose, 0.0f, 0.0f, 1.0f, &az);
   CHECK(vec_is(&az, 0.0, 1.0, 0.0));
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

#define SNAP_ALL (VIDEO_XR_SNAP_TURN | VIDEO_XR_SNAP_X | VIDEO_XR_SNAP_Y \
      | VIDEO_XR_SNAP_Z)
#define GAP_ALL (SNAP_ALL | VIDEO_XR_SNAP_PULLING(SNAP_ALL) \
      | VIDEO_XR_SNAP_HINGE | VIDEO_XR_SNAP_GAP \
      | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP))

/* The target: screen 1, 1.6 x 0.9, at 2 m facing ahead. */
static void snap_set(video_xr_quad_set_t *set)
{
   memset(set, 0, sizeof(*set));
   video_xr_pose_identity(&set->quads[0].pose);
   set->quads[0].pose.position.z = -2.0f;
   set->quads[0].width           = 1.6f;
   set->quads[0].height          = 0.9f;
   set->quads[0].kind            = VIDEO_XR_QUAD_SCREEN;
   set->num_quads                = 1;
}

/* At x, y, z turned yaw degrees about +Y. */
static void snap_pose(video_xr_pose_t *pose, double x, double y,
      double z, double yaw)
{
   video_xr_pose_identity(pose);
   pose->position.x    = (float)x;
   pose->position.y    = (float)y;
   pose->position.z    = (float)z;
   pose->orientation.y = (float)sin(yaw * M_PI / 360.0);
   pose->orientation.w = (float)cos(yaw * M_PI / 360.0);
}

/* Screen 2, 1 x 0.6, held at x, y, z turned yaw degrees about +Y,
 * snapped to set. */
static unsigned snap_at(const video_xr_quad_set_t *set, double x,
      double y, double z, double yaw, video_xr_pose_t *pose)
{
   snap_pose(pose, x, y, z, yaw);
   return video_xr_snap(set, 1, 0, 1.0f, 0.6f, 1.0f, pose)
      & VIDEO_XR_SNAP_ALL;
}

/* A 0.5 x 0.4 target in slot at x, 0, -2 turned yaw degrees. */
static void snap_add(video_xr_quad_set_t *set, unsigned slot, double x,
      double yaw)
{
   video_xr_quad_t *q = &set->quads[set->num_quads++];
   snap_pose(&q->pose, x, 0.0, -2.0, yaw);
   q->width  = 0.5f;
   q->height = 0.4f;
   q->kind   = VIDEO_XR_QUAD_SCREEN;
   q->slot   = slot;
   q->screen = slot;
}

/* Screen 3, 0.5 x 0.4, held likewise. */
static unsigned snap_small_at(const video_xr_quad_set_t *set, double x,
      double y, double z, double yaw, video_xr_pose_t *pose)
{
   snap_pose(pose, x, y, z, yaw);
   return video_xr_snap(set, 2, 0, 0.5f, 0.4f, 1.0f, pose)
      & VIDEO_XR_SNAP_ALL;
}

static double snap_yaw(const video_xr_pose_t *pose)
{
   float yaw, pitch, roll;
   video_xr_euler(&pose->orientation, &yaw, &pitch, &roll);
   return yaw;
}

/* Parallel and near: into its plane, edge to edge or level, each exact
 * within its range; a quarter turn about its up meets it at a corner. */
static void test_snap_lines(void)
{
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   snap_set(&set);
   CHECK(snap_at(&set, 1.308, 0.01, -1.98, 0.0, &p) == SNAP_ALL);
   CHECK(vec_is(&p.position, 1.30, 0.0, -2.0));
   CHECK(snap_at(&set, 0.1, 0.16, -1.99, 0.0, &p)
         == (VIDEO_XR_SNAP_TURN | VIDEO_XR_SNAP_Y | VIDEO_XR_SNAP_Z));
   CHECK(vec_is(&p.position, 0.1, 0.15, -2.0));
   CHECK(snap_at(&set, 0.0, 0.76, -2.0, 0.0, &p) == SNAP_ALL);
   CHECK(vec_is(&p.position, 0.0, 0.75, -2.0));
   CHECK(snap_at(&set, 0.808, 0.01, -1.49, 90.0, &p) == SNAP_ALL);
   CHECK(vec_is(&p.position, 0.8, 0.0, -1.5)
         && angle_near((float)snap_yaw(&p), 90.0f));
}

/* Above the snaps engaged, each axis says whether it pulls at all,
 * whole or fading: the hand's buzz re-arms only once its axis lets go
 * entirely. */
static void test_snap_pulling(void)
{
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   const unsigned no_x = VIDEO_XR_SNAP_TURN | VIDEO_XR_SNAP_Y
      | VIDEO_XR_SNAP_Z;
   snap_set(&set);
   snap_pose(&p, 1.308, 0.01, -1.98, 0.0);
   CHECK(video_xr_snap(&set, 1, 0, 1.0f, 0.6f, 1.0f, &p) == GAP_ALL);
   /* 1.5 cm off flush: the edge fades, pulling but not engaged. */
   snap_pose(&p, 1.315, 0.0, -2.0, 0.0);
   CHECK(video_xr_snap(&set, 1, 0, 1.0f, 0.6f, 1.0f, &p)
         == (no_x | VIDEO_XR_SNAP_PULLING(SNAP_ALL) | VIDEO_XR_SNAP_HINGE
            | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   /* 2.5 cm off: the edge lets go, the rest still hold. */
   snap_pose(&p, 1.325, 0.0, -2.0, 0.0);
   CHECK(video_xr_snap(&set, 1, 0, 1.0f, 0.6f, 1.0f, &p)
         == (no_x | VIDEO_XR_SNAP_PULLING(no_x) | VIDEO_XR_SNAP_HINGE));
   /* Half faded in: every axis pulls, none is engaged. */
   snap_pose(&p, 1.308, 0.01, -1.98, 0.0);
   CHECK(video_xr_snap(&set, 1, 0, 1.0f, 0.6f, 0.5f, &p)
         == (VIDEO_XR_SNAP_PULLING(SNAP_ALL)
            | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   /* Too far for any pull. */
   snap_pose(&p, 3.0, 0.0, -2.0, 0.0);
   CHECK(video_xr_snap(&set, 1, 0, 1.0f, 0.6f, 1.0f, &p) == 0);
   /* Midway from the edge to a screen's centre 8 cm on: X still pulls
    * between its two lines, so its buzz stays spent. */
   snap_add(&set, 2, 1.38, 0.0);
   snap_pose(&p, 1.34, 0.0, -2.0, 0.0);
   CHECK(video_xr_snap(&set, 1, 0, 1.0f, 0.6f, 1.0f, &p)
         == (no_x | VIDEO_XR_SNAP_PULLING(SNAP_ALL) | VIDEO_XR_SNAP_HINGE));
}

/* Within 5 degrees exactly parallel, to 10 closing at twice the hand's
 * turn, past it as held; lines exact to 3 cm, fading to 6, edge to
 * edge to 1 either way, fading to 2; a 30 to 60 cm gap fades every
 * pull, past 60 none. */
static void test_snap_zones(void)
{
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   snap_set(&set);
   CHECK(snap_at(&set, 1.30, 0.0, -2.0, 4.0, &p) == SNAP_ALL);
   CHECK(fabs(snap_yaw(&p)) < 0.01 && vec_is(&p.position, 1.30, 0.0, -2.0));
   CHECK(snap_at(&set, 1.30, 0.0, -2.0, 7.5, &p) == 0);
   CHECK(fabs(snap_yaw(&p) - 5.0) < 0.02);
   CHECK(snap_at(&set, 1.30, 0.0, -2.0, 12.0, &p) == 0);
   CHECK(fabs(snap_yaw(&p) - 12.0) < 0.01
         && vec_is(&p.position, 1.30, 0.0, -2.0));
   CHECK(snap_at(&set, 1.295, 0.0, -2.0, 0.0, &p) == SNAP_ALL);
   CHECK(NEAR(p.position.x, 1.30));
   snap_at(&set, 1.305, 0.0, -2.0, 0.0, &p);
   CHECK(NEAR(p.position.x, 1.30));
   snap_at(&set, 1.315, 0.0, -2.0, 0.0, &p);
   CHECK(NEAR(p.position.x, 1.31));
   snap_at(&set, 1.325, 0.0, -2.0, 0.0, &p);
   CHECK(NEAR(p.position.x, 1.325));
   snap_at(&set, 1.36, 0.0, -2.0, 0.0, &p);
   CHECK(NEAR(p.position.x, 1.36));
   CHECK(snap_at(&set, 1.75, 0.0, -2.0, 4.0, &p) == 0);
   CHECK(fabs(snap_yaw(&p) - 2.0) < 0.02);
   CHECK(snap_at(&set, 1.95, 0.0, -2.0, 4.0, &p) == 0);
   CHECK(fabs(snap_yaw(&p) - 4.0) < 0.01
         && vec_is(&p.position, 1.95, 0.0, -2.0));
}

/* No target: the menu, a fixed slot, a held slot, the held screen's own
 * slot, no set; the menu held snaps to nothing; a stereo pair counts
 * once. */
static void test_snap_targets(void)
{
   video_xr_quad_set_t set, two;
   video_xr_pose_t p, q;
   snap_set(&set);
   set.quads[0].kind = VIDEO_XR_QUAD_MENU;
   set.quads[0].slot = VIDEO_XR_MENU_SLOT;
   CHECK(snap_at(&set, 1.32, 0.01, -1.98, 0.0, &p) == 0);
   CHECK(vec_is(&p.position, 1.32, 0.01, -1.98));
   snap_set(&set);
   set.slots[0].fixed = true;
   CHECK(snap_at(&set, 1.32, 0.01, -1.98, 0.0, &p) == 0);
   snap_set(&set);
   video_xr_pose_identity(&p);
   p.position.x = 1.32f;
   p.position.y = 0.01f;
   p.position.z = -1.98f;
   CHECK(video_xr_snap(&set, 1, 1u, 1.0f, 0.6f, 1.0f, &p) == 0);
   CHECK(video_xr_snap(&set, 0, 0, 1.0f, 0.6f, 1.0f, &p) == 0);
   CHECK(video_xr_snap(&set, VIDEO_XR_MENU_SLOT, 0, 1.0f, 0.6f, 1.0f, &p) == 0);
   CHECK(video_xr_snap(NULL, 1, 0, 1.0f, 0.6f, 1.0f, &p) == 0);
   CHECK(vec_is(&p.position, 1.32, 0.01, -1.98));
   two              = set;
   two.quads[0].eye = VIDEO_XR_EYE_LEFT;
   two.quads[1]     = two.quads[0];
   two.quads[1].eye = VIDEO_XR_EYE_RIGHT;
   two.num_quads    = 2;
   snap_at(&set, 1.32, 0.01, -1.98, 0.0, &p);
   snap_at(&two, 1.32, 0.01, -1.98, 0.0, &q);
   CHECK(!memcmp(&p, &q, sizeof(p)));
}

/* No jumps: across every zone's edges the snapped pose moves at most
 * twice the hand's step, but past touching, where the stop lets go at
 * up to 3.4 times. */
static void test_snap_continuous(void)
{
   unsigned i;
   video_xr_quad_set_t set;
   video_xr_pose_t p, last;
   snap_set(&set);
   video_xr_pose_identity(&last);
   /* Through the flush line's magnet, both sides: 0.5 mm steps. */
   for (i = 0; i <= 400; i++)
   {
      snap_at(&set, 1.20 + 0.0005 * i, 0.0, -2.0, 0.0, &p);
      if (i)
         CHECK(fabs(p.position.x - last.position.x) <= 0.00171);
      last = p;
   }
   /* Turning through 5 and 10 degrees 2 cm off the line. */
   for (i = 0; i <= 240; i++)
   {
      snap_at(&set, 1.32, 0.0, -2.0, 0.05 * i, &p);
      if (i)
         CHECK(fabs(snap_yaw(&p) - snap_yaw(&last)) <= 0.101
               && fabs(p.position.x - last.position.x) <= 0.005
               && fabs(p.position.z - last.position.z) <= 0.005);
      last = p;
   }
   /* Away through the 30 and 60 cm gaps, turned 4 degrees. */
   for (i = 0; i <= 450; i++)
   {
      snap_at(&set, 1.55 + 0.001 * i, 0.0, -2.0, 4.0, &p);
      if (i)
         CHECK(fabs(snap_yaw(&p) - snap_yaw(&last)) <= 0.02);
      last = p;
   }
   /* Square, through the corner's magnet both ways. */
   for (i = 0; i <= 200; i++)
   {
      snap_at(&set, 0.70 + 0.001 * i, 0.0, -1.49, 90.0, &p);
      if (i)
         CHECK(fabs(p.position.x - last.position.x) <= 0.00341
               && fabs(p.position.z - last.position.z) <= 0.00341);
      last = p;
   }
   for (i = 0; i <= 200; i++)
   {
      snap_at(&set, 0.81, 0.0, -1.40 - 0.001 * i, 90.0, &p);
      if (i)
         CHECK(fabs(p.position.x - last.position.x) <= 0.0021
               && fabs(p.position.z - last.position.z) <= 0.0021);
      last = p;
   }
}

/* Several targets, each ranked by what its pull leaves: in a row of
 * screens 5 cm apart, screen 3 held 0.8 cm right of screen 2 lines up
 * with it as if alone, in either quad order, though screen 1 is at the
 * same angle and on the same lines in its 30-60 cm fade; carried right
 * through screen 1's 60 cm edge in 0.5 mm steps, it never turns and no
 * step moves it more than twice the hand's. */
static void test_snap_ranked(void)
{
   unsigned i, o;
   video_xr_quad_set_t set;
   video_xr_pose_t p, last;
   for (o = 0; o < 2; o++)
   {
      memset(&set, 0, sizeof(set));
      snap_add(&set, o, o ? 0.0 : -0.55, 0.0);
      snap_add(&set, !o, o ? -0.55 : 0.0, 0.0);
      CHECK(snap_small_at(&set, 0.508, 0.01, -1.99, 3.0, &p) == SNAP_ALL);
      CHECK(fabs(snap_yaw(&p)) < 0.01
            && vec_is(&p.position, 0.50, 0.0, -2.0));
      /* Level with both: the tie goes to the whole pull. */
      CHECK(snap_small_at(&set, 0.508, 0.0, -1.99, 3.0, &p) == SNAP_ALL);
      video_xr_pose_identity(&last);
      for (i = 0; i <= 200; i++)
      {
         snap_small_at(&set, 0.50 + 0.0005 * i, 0.01, -1.99, 3.0, &p);
         if (i)
            CHECK(fabs(snap_yaw(&p) - snap_yaw(&last)) <= 0.01
                  && fabs(p.position.x - last.position.x) <= 0.00101
                  && fabs(p.position.y - last.position.y) <= 0.00101
                  && fabs(p.position.z - last.position.z) <= 0.00101);
         last = p;
      }
      /* The whole pull still wins over one a rounding error nearer. */
      set.quads[!o].pose.position.y = 2e-6f;
      CHECK(snap_small_at(&set, 0.508, 0.0, -1.99, 3.0, &p) == SNAP_ALL);
   }
   /* Among whole pulls the smallest correction wins: 0.88 high, 1.5 cm
    * low of a 0.9 high target, its bottom edge levels, not its top. */
   snap_set(&set);
   snap_pose(&p, 0.01, -0.015, -1.99, 0.0);
   CHECK((video_xr_snap(&set, 1, 0, 1.0f, 0.88f, 1.0f, &p)
            & VIDEO_XR_SNAP_ALL) == SNAP_ALL
         && vec_is(&p.position, 0.0, -0.01, -2.0));
}

/* A whole line beats a fading one however near: screen 3 held 0.8 cm
 * right of a target at y 0, with another 1 cm higher whose gap is 0.31
 * or 0.45 m, carried up through that one's line in 1 um steps, stays
 * level with the whole one in either quad order. */
static void test_snap_whole_line(void)
{
   unsigned i, j, o;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   for (j = 0; j < 2; j++)
   {
      for (o = 0; o < 2; o++)
      {
         double x = j ? 1.47 : 1.33;
         memset(&set, 0, sizeof(set));
         snap_add(&set, o, o ? x : 0.0, 0.0);
         snap_add(&set, !o, o ? 0.0 : x, 0.0);
         set.quads[!o].pose.position.y = 0.01f;
         for (i = 0; i <= 800; i++)
            CHECK(snap_small_at(&set, 0.508, 0.0096 + 0.000001 * i, -1.99,
                  0.0, &p) == SNAP_ALL && fabs(p.position.y) < 1e-6);
      }
   }
}

/* A turn tie: targets 3 degrees either way, the near one whole, the
 * far one in its 30-60 cm fade; the near one turns it, in either quad
 * order, as it does when the far one is parallel already. Of two in
 * the turn's whole snap, the nearer wins, though the other's turn is
 * smaller. */
static void test_snap_turn_tie(void)
{
   unsigned o;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   for (o = 0; o < 2; o++)
   {
      memset(&set, 0, sizeof(set));
      snap_add(&set, o, o ? 1.45 : 0.0, o ? 3.0 : -3.0);
      snap_add(&set, !o, o ? 0.0 : 1.45, o ? -3.0 : 3.0);
      CHECK(snap_small_at(&set, 0.508, 0.0, -2.0, 0.0, &p)
            & VIDEO_XR_SNAP_TURN);
      CHECK(angle_near((float)snap_yaw(&p), -3.0f));
      set.quads[!o].pose.orientation.y = 0.0f;
      set.quads[!o].pose.orientation.w = 1.0f;
      CHECK(snap_small_at(&set, 0.508, 0.0, -2.0, 0.0, &p)
            & VIDEO_XR_SNAP_TURN);
      CHECK(angle_near((float)snap_yaw(&p), -3.0f));
      memset(&set, 0, sizeof(set));
      snap_add(&set, o, o ? 1.04 : 0.0, o ? 1.0 : -4.0);
      snap_add(&set, !o, o ? 0.0 : 1.04, o ? -4.0 : 1.0);
      CHECK(snap_small_at(&set, 0.508, 0.0, -2.0, 0.0, &p)
            & VIDEO_XR_SNAP_TURN);
      CHECK(angle_near((float)snap_yaw(&p), -4.0f));
   }
}

/* A target turned 30 degrees snaps in its own frame: screen 2 held 0.8 cm
 * off standing flush right of it, 1 cm high, 2 cm in front and turned
 * 3 degrees more lands flush, level, in its plane and parallel. */
static void test_snap_turned(void)
{
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   double c = cos(M_PI / 6.0);
   double s = sin(M_PI / 6.0);
   snap_set(&set);
   snap_pose(&set.quads[0].pose, 0.0, 0.0, -2.0, 30.0);
   CHECK(snap_at(&set, 1.308 * c + 0.02 * s, 0.01,
            -2.0 - 1.308 * s + 0.02 * c, 33.0, &p) == SNAP_ALL);
   CHECK(vec_is(&p.position, 1.30 * c, 0.0, -2.0 - 1.30 * s)
         && angle_near((float)snap_yaw(&p), 30.0f));
}

/* Metres between two poses' middles. */
static double pose_gap(const video_xr_pose_t *a, const video_xr_pose_t *b)
{
   double dx = (double)a->position.x - b->position.x;
   double dy = (double)a->position.y - b->position.y;
   double dz = (double)a->position.z - b->position.z;
   return sqrt(dx * dx + dy * dy + dz * dz);
}

/* Screen 1 of the 3DS layout alone, 1.6 x 0.96 at 1.8 m: its bottom
 * edge runs along x at y -0.48, z -1.8, its right edge up y at x 0.8. */
static void hinge_set(video_xr_quad_set_t *set)
{
   memset(set, 0, sizeof(*set));
   video_xr_pose_identity(&set->quads[0].pose);
   set->quads[0].pose.position.z = -1.8f;
   set->quads[0].width           = 1.6f;
   set->quads[0].height          = 0.96f;
   set->quads[0].kind            = VIDEO_XR_QUAD_SCREEN;
   set->num_quads                = 1;
}

/* deg degrees about x, y, z. */
static void hinge_turn(video_xr_quat_t *q, double x, double y, double z,
      double deg)
{
   double n = sqrt(x * x + y * y + z * z);
   double s = sin(deg * M_PI / 360.0);
   q->x     = (float)(x / n * s);
   q->y     = (float)(y / n * s);
   q->z     = (float)(z / n * s);
   q->w     = (float)cos(deg * M_PI / 360.0);
}

/* b, then a: a b. */
static void hinge_then(const video_xr_quat_t *a, const video_xr_quat_t *b,
      video_xr_quat_t *out)
{
   video_xr_pose_t pa, pb;
   video_xr_pose_identity(&pa);
   video_xr_pose_identity(&pb);
   pa.orientation = *a;
   pb.orientation = *b;
   video_xr_compose(&pa, &pb, &pa);
   *out = pa.orientation;
}

/* Turned q, with its own point mx, my at x, y, z. */
static void hinge_place(video_xr_pose_t *p, const video_xr_quat_t *q,
      double mx, double my, double x, double y, double z)
{
   video_xr_vec3_t m, w;
   m.x            = (float)mx;
   m.y            = (float)my;
   m.z            = 0.0f;
   video_xr_rotate(q, &m, &w);
   p->orientation = *q;
   p->position.x  = (float)(x - w.x);
   p->position.y  = (float)(y - w.y);
   p->position.z  = (float)(z - w.z);
}

/* The gap hinge_snap and hinge_grab snap with, metres; 0 touching. */
static float t_gap;

/* Screen 2 of the 3DS layout, 1.28 x 0.96, held at p, snapped to set. */
static unsigned hinge_snap(const video_xr_quad_set_t *set,
      video_xr_pose_t *p, float fade, video_xr_hinge_t *h)
{
   return video_xr_snap_hinge(set, 1, 0, 1.28f, 0.96f, t_gap, fade, p, h);
}

/* Screen in slot, w x h, at p, added to set. */
static void rest_add(video_xr_quad_set_t *set, unsigned slot, float w,
      float h, const video_xr_pose_t *p)
{
   video_xr_quad_t *q = &set->quads[set->num_quads++];
   memset(q, 0, sizeof(*q));
   q->pose   = *p;
   q->width  = w;
   q->height = h;
   q->kind   = VIDEO_XR_QUAD_SCREEN;
   q->slot   = slot;
   q->screen = slot;
}

/* hinge_set's screen 1 and the 3DS layout's screen 2 resting under it,
 * tipped fold degrees toward the user about its top edge, that edge
 * gap metres under screen 1's bottom edge, in its plane. */
static void rest_under(video_xr_quad_set_t *set, double fold, double gap)
{
   video_xr_pose_t p;
   video_xr_quat_t q;
   hinge_set(set);
   hinge_turn(&q, 1.0, 0.0, 0.0, -fold);
   hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.48 - gap, -1.8);
   rest_add(set, 1, 1.28f, 0.96f, &p);
}

/* Whether r is slot's edge edge on target's edge on. */
static int rest_is(const video_xr_rest_t *r, unsigned slot, unsigned edge,
      unsigned target, unsigned on)
{
   return r->slot == slot && r->edge == edge && r->target == target
      && r->on == on;
}

/* Screens resting exactly where a hinge holds one carried, each pair
 * once: the 3DS pair flat, 2 cm apart and folded 30 degrees, screen 1
 * carried where either can be, screen 2 where only it can (folded 2 cm
 * apart, screen 1's edge lies 1 cm off screen 2's plane); side by side;
 * a square corner; a 2 x 2 grid's four sides, not its corners. None
 * 12 cm apart, 1 mm out of the plane, skipped or fixed; max caps them.
 * None only near a hinge: 7 mm into Hinge Gap 0's click, 3.2 cm apart
 * at Hinge Gap 2 (inside its click), centres 2 cm off level; 3.2 cm at
 * Hinge Gap 0 is a free gap, held there. None inside a settle band off
 * its point, which a let-go would move: 4.5 cm at Hinge Gap 2, 2.5 at
 * 0, 0.5 at 3, 7.5 at 5; 5.5 cm at Hinge Gap 2 is past it, and touching
 * rests at every gap. */
static void test_rest_hinges(void)
{
   unsigned i;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_rest_t r[8];
   static const double grid[4][2] = {
      { -0.8, 0.48 }, { 0.8, 0.48 }, { -0.8, -0.48 }, { 0.8, -0.48 } };
   /* Hinge Gap, apart, metres. */
   static const double band[4][2] = {
      { 0.02, 0.045 }, { 0.0, 0.025 }, { 0.03, 0.005 }, { 0.05, 0.075 } };
   static const double gaps[6] = { 0.0, 0.01, 0.02, 0.03, 0.05, 0.10 };

   rest_under(&set, 0.0, 0.0);
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 8) == 1
         && rest_is(&r[0], 0, 1, 1, 0));
   rest_under(&set, 0.0, 0.02);
   CHECK(video_xr_rest_hinges(&set, 0, 0.02f, r, 8) == 1
         && rest_is(&r[0], 0, 1, 1, 0));
   rest_under(&set, 30.0, 0.0);
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 8) == 1
         && rest_is(&r[0], 0, 1, 1, 0));
   rest_under(&set, 30.0, 0.02);
   CHECK(video_xr_rest_hinges(&set, 0, 0.02f, r, 8) == 1
         && rest_is(&r[0], 1, 0, 0, 1));

   hinge_set(&set);
   snap_pose(&p, 1.44, 0.0, -1.8, 0.0);
   rest_add(&set, 1, 1.28f, 0.96f, &p);
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 8) == 1
         && rest_is(&r[0], 0, 2, 1, 3));
   hinge_set(&set);
   snap_pose(&p, 0.8, 0.0, -1.16, -90.0);
   rest_add(&set, 1, 1.28f, 0.96f, &p);
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 8) == 1
         && rest_is(&r[0], 0, 2, 1, 3));

   memset(&set, 0, sizeof(set));
   for (i = 0; i < 4; i++)
   {
      snap_pose(&p, grid[i][0], grid[i][1], -1.8, 0.0);
      rest_add(&set, i, 1.6f, 0.96f, &p);
   }
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 8) == 4
         && rest_is(&r[0], 0, 2, 1, 3) && rest_is(&r[1], 0, 1, 2, 0)
         && rest_is(&r[2], 1, 1, 3, 0) && rest_is(&r[3], 2, 2, 3, 3));
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 2) == 2);
   CHECK(video_xr_rest_hinges(&set, 1u << 0, 0.0f, r, 8) == 2
         && rest_is(&r[0], 1, 1, 3, 0) && rest_is(&r[1], 2, 2, 3, 3));

   rest_under(&set, 0.0, 0.12);
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 8) == 0);
   rest_under(&set, 0.0, 0.0);
   set.quads[1].pose.position.z += 0.001f;
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 8) == 0);
   rest_under(&set, 0.0, 0.0);
   CHECK(video_xr_rest_hinges(&set, 1u << 1, 0.0f, r, 8) == 0);
   set.slots[1].fixed = true;
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 8) == 0);

   rest_under(&set, 0.0, 0.007);
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 8) == 0);
   rest_under(&set, 0.0, 0.032);
   CHECK(video_xr_rest_hinges(&set, 0, 0.02f, r, 8) == 0);
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 8) == 1
         && rest_is(&r[0], 0, 1, 1, 0));
   rest_under(&set, 0.0, 0.0);
   set.quads[1].pose.position.x = 0.02f;
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 8) == 0);

   for (i = 0; i < 4; i++)
   {
      rest_under(&set, 0.0, band[i][1]);
      CHECK(video_xr_rest_hinges(&set, 0, (float)band[i][0], r, 8) == 0);
   }
   rest_under(&set, 0.0, 0.055);
   CHECK(video_xr_rest_hinges(&set, 0, 0.02f, r, 8) == 1
         && rest_is(&r[0], 0, 1, 1, 0));
   rest_under(&set, 0.0, 0.0);
   for (i = 0; i < 6; i++)
      CHECK(video_xr_rest_hinges(&set, 0, (float)gaps[i], r, 8) == 1
            && rest_is(&r[0], 0, 1, 1, 0));
}

/* Whether a and b are the same pose to rounding. */
static int pose_near(const video_xr_pose_t *a, const video_xr_pose_t *b)
{
   return fabs(a->position.x - b->position.x) < 1e-5
      && fabs(a->position.y - b->position.y) < 1e-5
      && fabs(a->position.z - b->position.z) < 1e-5
      && fabs(a->orientation.x - b->orientation.x) < 1e-5
      && fabs(a->orientation.y - b->orientation.y) < 1e-5
      && fabs(a->orientation.z - b->orientation.z) < 1e-5
      && fabs(a->orientation.w - b->orientation.w) < 1e-5;
}

/* The hinge a screen is taken in where it rests, by the same test that
 * shows it: the 3DS pair folded 2 cm apart, shown, and taken hinged
 * from screen 2's side alone (one-sided at a fold: screen 1's edge lies
 * 1 cm off screen 2's plane), not with screen 1 held; neither shown
 * nor taken hinged where the hinge, whole, would still move it (7 mm
 * into Hinge Gap 0's click); of two, the one the snap ranks best:
 * screen 1's touching edge over screen 3's 4 cm away. */
static void test_rest_hinge(void)
{
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_hinge_t h;
   video_xr_rest_t r[2];
   rest_under(&set, 30.0, 0.02);
   CHECK(video_xr_rest_hinges(&set, 0, 0.02f, r, 2) == 1
         && rest_is(&r[0], 1, 0, 0, 1));
   CHECK(video_xr_rest_hinge(&set, 1, 0, 1.28f, 0.96f, 0.02f,
            &set.quads[1].pose, &h)
         && h.slot == 0 && h.on == 1 && h.edge == 0
         && h.axis == VIDEO_XR_STEP_TILT);
   CHECK(!video_xr_rest_hinge(&set, 0, 0, 1.6f, 0.96f, 0.02f,
            &set.quads[0].pose, &h));
   CHECK(!video_xr_rest_hinge(&set, 1, 1u << 0, 1.28f, 0.96f, 0.02f,
            &set.quads[1].pose, &h));
   rest_under(&set, 0.0, 0.007);
   CHECK(video_xr_rest_hinges(&set, 0, 0.0f, r, 2) == 0);
   CHECK(!video_xr_rest_hinge(&set, 1, 0, 1.28f, 0.96f, 0.0f,
            &set.quads[1].pose, &h));
   rest_under(&set, 0.0, 0.0);
   snap_pose(&p, 0.0, -0.48 - 0.96 - 0.04 - 0.48, -1.8, 0.0);
   rest_add(&set, 2, 1.28f, 0.96f, &p);
   CHECK(video_xr_rest_hinge(&set, 1, 0, 1.28f, 0.96f, 0.0f,
            &set.quads[1].pose, &h)
         && h.slot == 0 && h.on == 1 && h.edge == 0);
}

/* The 3DS pair resting folded 30 degrees 2 cm apart, screen 2 gripped
 * where it rests by an aim at the origin, as edit mode takes it. */
static void rest_grab(video_xr_quad_set_t *set, video_xr_grab_t *g,
      video_xr_pose_t *aim)
{
   rest_under(set, 30.0, 0.02);
   memset(g, 0, sizeof(*g));
   video_xr_pose_identity(aim);
   g->rel    = set->quads[1].pose;
   g->turn   = g->rel.orientation;
   g->last   = g->rel;
   g->base   = g->rel.position;
   g->width  = 1.28f;
   g->height = 0.96f;
   g->slot   = 1;
   g->state  = VIDEO_XR_GRAB_HELD;
   g->gap    = 2;
}

/* A grab carrying the hinge its screen rested in is hinged whole where
 * it was taken, its fade still none, nothing moved; 2 cm toward the
 * user it is still held on the line, where the fade alone would not
 * hold it; without the hinge, neither. 4.8 cm off, past
 * VIDEO_XR_HINGE_REACH, it holds no hinge but its target still pulls
 * whole, the fade short of whole; 6 cm off, the hinge kept ends. */
static void test_grab_kept(void)
{
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, start, pose;
   video_xr_hinge_t h;
   rest_grab(&set, &g, &aim);
   start = g.last;
   CHECK(!video_xr_grab_kept(&g, &aim, &set, &start, &h));
   CHECK(!(video_xr_grab_hinge(&g, &aim, &set, 0, &start, &pose, &h)
            & VIDEO_XR_SNAP_HINGE));
   g.rest_slot = 1;
   g.rest_on   = 1;
   g.rest_edge = 0;
   CHECK(video_xr_grab_kept(&g, &aim, &set, &start, &h)
         && h.slot == 0 && h.on == 1 && h.edge == 0);
   CHECK((video_xr_grab_hinge(&g, &aim, &set, 0, &start, &pose, &h)
            & VIDEO_XR_SNAP_HINGE)
         && h.slot == 0 && h.on == 1 && h.edge == 0
         && pose_near(&pose, &start));
   aim.position.z = 0.02f;
   CHECK((video_xr_grab_hinge(&g, &aim, &set, 0, &start, &pose, &h)
            & VIDEO_XR_SNAP_HINGE)
         && NEAR(pose.position.z, start.position.z));
   g.rest_slot = 0;
   CHECK(!(video_xr_grab_hinge(&g, &aim, &set, 0, &start, &pose, &h)
            & VIDEO_XR_SNAP_HINGE));
   g.rest_slot    = 1;
   aim.position.z = 0.048f;
   CHECK(video_xr_grab_kept(&g, &aim, &set, &start, &h));
   CHECK(!(video_xr_grab_hinge(&g, &aim, &set, 0, &start, &pose, &h)
            & VIDEO_XR_SNAP_HINGE));
   aim.position.z = 0.06f;
   CHECK(!video_xr_grab_kept(&g, &aim, &set, &start, &h));
   CHECK(!(video_xr_grab_hinge(&g, &aim, &set, 0, &start, &pose, &h)
            & VIDEO_XR_SNAP_HINGE));
}

/* rest_grab's screen 2, the hand x metres past touching screen 1 along
 * screen 1's up, its fade whole: the snaps held, its edge's gap below
 * screen 1's in *across, and h's hinge near and pushed through. */
static unsigned through_snap(const video_xr_quad_set_t *set,
      const video_xr_grab_t *g, const video_xr_hinge_t *h, double x,
      double *across, bool *near, bool *through)
{
   unsigned bits;
   video_xr_pose_t aim, start, pose;
   video_xr_hinge_t got;
   video_xr_vec3_t m, up, w, e;
   const video_xr_quad_t *q = &set->quads[0];
   m.x = 0.0f;
   m.y = 1.0f;
   m.z = 0.0f;
   video_xr_rotate(&q->pose.orientation, &m, &up);
   video_xr_pose_identity(&aim);
   aim.position.x = (float)(up.x * (x + 0.02));
   aim.position.y = (float)(up.y * (x + 0.02));
   aim.position.z = (float)(up.z * (x + 0.02));
   start             = g->last;
   start.position.y -= 0.3f;
   bits = video_xr_grab_hinge(g, &aim, set, 0, &start, &pose, &got);
   m.y = g->height * 0.5f;
   video_xr_rotate(&pose.orientation, &m, &w);
   m.y = -q->height * 0.5f;
   video_xr_rotate(&q->pose.orientation, &m, &e);
   *across = -(((double)pose.position.x + w.x - q->pose.position.x - e.x)
         * up.x + ((double)pose.position.y + w.y - q->pose.position.y - e.y)
         * up.y + ((double)pose.position.z + w.z - q->pose.position.z - e.z)
         * up.z);
   *near    = video_xr_grab_near_hinge(g, &aim, set, h,
         VIDEO_XR_HINGE_REACH);
   *through = video_xr_grab_through(g, &aim, set, h);
   return bits;
}

/* Pushed past touching, rest_grab's screen 2 is hinged touching and its
 * hinge near to VIDEO_XR_SNAP_STOP past; past that the push lets it go,
 * neither held nor near, overlapping. Once a push has (g->through), from
 * about VIDEO_XR_SNAP_STOP_ARM short of there it overlaps a little,
 * neither held nor near, and back short of that it is hinged touching;
 * apart, as before. Swept in and back, setting and clearing g->through
 * as the main thread does, nothing hops: it moves no more than the
 * stop's fade, 3.4 times the hand, and the slip as it lets go is
 * about a tenth of a millimetre. */
static void test_grab_through(void)
{
   static const double whole[] = { 0.005, 0.015, 0.029 };
   unsigned i;
   double a, last = 0.0;
   bool near, through;
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim;
   video_xr_hinge_t h;
   rest_grab(&set, &g, &aim);
   memset(&h, 0, sizeof(h));
   h.slot = 0;
   h.on   = 1;
   h.edge = 0;
   for (i = 0; i < 3; i++)
   {
      CHECK((through_snap(&set, &g, &h, whole[i], &a, &near, &through)
               & VIDEO_XR_SNAP_HINGE) && fabs(a) < 1e-5 && near
            && !through);
   }
   CHECK(!(through_snap(&set, &g, &h, 0.031, &a, &near, &through)
            & VIDEO_XR_SNAP_HINGE) && a < -1e-5 && !near && through);
   through_snap(&set, &g, &h, 0.04, &a, &near, &through);
   CHECK(a < -0.001 && a > -0.0399 && !near);
   through_snap(&set, &g, &h, 0.06, &a, &near, &through);
   CHECK(fabs(a + 0.06) < 1e-5);
   g.through = 1;
   for (i = 0; i < 2; i++)
   {
      CHECK(!(through_snap(&set, &g, &h, i ? 0.0299 : 0.026, &a, &near,
                  &through) & VIDEO_XR_SNAP_HINGE)
            && a < -1e-5 && a > -0.00015 && !near && through);
   }
   CHECK((through_snap(&set, &g, &h, 0.024, &a, &near, &through)
            & VIDEO_XR_SNAP_HINGE) && fabs(a) < 1e-5 && near && !through);
   CHECK((through_snap(&set, &g, &h, -0.023, &a, &near, &through)
            & VIDEO_XR_SNAP_HINGE) && fabs(a - 0.02) < 1e-5);
   g.through = 0;
   for (i = 0; i <= 600; i++)
   {
      double x   = 0.02 + 0.0001 * ((i <= 300) ? i : 600 - i);
      bool slip  = false;
      unsigned bits;
      if (i <= 300 && !g.through && x > 0.03 + 1e-5)
      {
         g.through = 1;
         slip      = true;
      }
      bits = through_snap(&set, &g, &h, x, &a, &near, &through);
      if (bits & VIDEO_XR_SNAP_HINGE)
         g.through = 0;
      if (i)
         CHECK(fabs(a - last) <= (slip ? 0.00045 : 0.00035));
      CHECK(((bits & VIDEO_XR_SNAP_HINGE) == 0) == (i <= 300
            ? x > 0.03 + 1e-5 : x > 0.025 + 1e-5));
      last = a;
   }
}

/* Through (g->through), rest_grab's screen 2 carried 1 um at a time
 * from 2.45 to 2.65 cm past touching, and let go where it shows, saved
 * to the micrometre: it rests hinged exactly where it was hinged
 * carried. */
static void test_grab_through_rest(void)
{
   unsigned i;
   unsigned odd = 0;
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, start, pose;
   video_xr_hinge_t h;
   video_xr_vec3_t m, up;
   rest_grab(&set, &g, &aim);
   g.through = 1;
   m.x = 0.0f;
   m.y = 1.0f;
   m.z = 0.0f;
   video_xr_rotate(&set.quads[0].pose.orientation, &m, &up);
   start             = g.last;
   start.position.y -= 0.3f;
   for (i = 0; i <= 2000; i++)
   {
      double x = 0.0245 + 1e-6 * i;
      bool carried, rests;
      video_xr_pose_identity(&aim);
      aim.position.x = (float)(up.x * (x + 0.02));
      aim.position.y = (float)(up.y * (x + 0.02));
      aim.position.z = (float)(up.z * (x + 0.02));
      carried = (video_xr_grab_hinge(&g, &aim, &set, 0, &start, &pose,
               NULL) & VIDEO_XR_SNAP_HINGE) != 0;
      pose.position.x = (float)(floor(pose.position.x * 1e6 + 0.5) / 1e6);
      pose.position.y = (float)(floor(pose.position.y * 1e6 + 0.5) / 1e6);
      pose.position.z = (float)(floor(pose.position.z * 1e6 + 0.5) / 1e6);
      rests = video_xr_rest_hinge(&set, 1, 0, g.width, g.height, 0.02f,
            &pose, &h);
      odd  += (carried != rests) ? 1u : 0u;
   }
   if (odd)
      printf("  through, carried and rest differ at %u of 2001\n", odd);
   CHECK(odd == 0);
}

/* At Hinge Gap 1 the click reaches touching, so a screen held where it
 * lies touching would click apart. The 3DS pair resting touching, flat
 * or folded 30, rests hinged at Gap 1 as at 0 and 2; taken where the
 * hand holds it (video_xr_rest_take, 1 cm past touching, 5 mm at Gap
 * 2), it is hinged whole and unmoved. Resting 1 cm apart, the hand
 * holds it where it lies. */
static void test_rest_gap_one(void)
{
   unsigned i;
   for (i = 0; i < 3; i++)
   {
      video_xr_quad_set_t set;
      video_xr_rest_t r[4];
      video_xr_hinge_t h;
      video_xr_grab_t g;
      video_xr_pose_t aim, taken, pose;
      const video_xr_pose_t *at = &set.quads[1].pose;
      float gap                 = (i == 2) ? 0.02f : 0.01f;
      rest_under(&set, i ? 30.0 : 0.0, 0.0);
      CHECK(video_xr_rest_hinges(&set, 0, gap, r, 4) == 1);
      CHECK(video_xr_rest_hinge(&set, 1, 0, 1.28f, 0.96f, gap, at, &h)
            && h.slot == 0);
      taken = *at;
      video_xr_rest_take(&set, 1, 1.28f, 0.96f, gap, &h, &taken);
      CHECK(fabs(taken.position.y - at->position.y
               - ((i == 2) ? 0.005 : 0.01)) < 1e-6
            && fabs(taken.position.z - at->position.z) < 1e-6);
      memset(&g, 0, sizeof(g));
      video_xr_pose_identity(&aim);
      g.rel       = taken;
      g.turn      = taken.orientation;
      g.last      = *at;
      g.base      = at->position;
      g.width     = 1.28f;
      g.height    = 0.96f;
      g.slot      = 1;
      g.state     = VIDEO_XR_GRAB_HELD;
      g.gap       = (unsigned char)(gap * 100.0f + 0.5f);
      g.rest_slot = (unsigned char)(h.slot + 1);
      g.rest_on   = (unsigned char)h.on;
      g.rest_edge = (unsigned char)h.edge;
      CHECK((video_xr_grab_hinge(&g, &aim, &set, 0, &taken, &pose, NULL)
               & VIDEO_XR_SNAP_HINGE)
            && fabs(pose.position.x - at->position.x) < 1e-5
            && fabs(pose.position.y - at->position.y) < 1e-5
            && fabs(pose.position.z - at->position.z) < 1e-5);
   }
   {
      video_xr_quad_set_t set;
      video_xr_rest_t r[4];
      video_xr_hinge_t h;
      video_xr_pose_t taken;
      rest_under(&set, 30.0, 0.01);
      CHECK(video_xr_rest_hinges(&set, 0, 0.01f, r, 4) == 1);
      CHECK(video_xr_rest_hinge(&set, 1, 0, 1.28f, 0.96f, 0.01f,
            &set.quads[1].pose, &h));
      taken = set.quads[1].pose;
      video_xr_rest_take(&set, 1, 1.28f, 0.96f, 0.01f, &h, &taken);
      CHECK(!memcmp(&taken, &set.quads[1].pose, sizeof(taken)));
   }
}

/* Whether set's screen in slot, w x 0.96 where it rests, is taken in
 * the hinge of its edge edge on target's edge on, gap metres apart. */
static int rest_takes(const video_xr_quad_set_t *set, unsigned slot,
      float w, float gap, unsigned edge, unsigned target, unsigned on)
{
   video_xr_hinge_t h;
   return video_xr_rest_hinge(set, slot, 0, w, 0.96f, gap,
         &set->quads[slot].pose, &h)
      && h.edge == edge && h.slot == target && h.on == on;
}

/* Flat, or at Hinge Gap 0, a pair resting hinged is taken hinged from
 * either side: the 3DS pair flat touching and 2 cm apart, folded 30
 * degrees touching, and a square corner touching. */
static void test_rest_hinge_sides(void)
{
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   rest_under(&set, 0.0, 0.0);
   CHECK(rest_takes(&set, 1, 1.28f, 0.0f, 0, 0, 1));
   CHECK(rest_takes(&set, 0, 1.6f, 0.0f, 1, 1, 0));
   rest_under(&set, 0.0, 0.02);
   CHECK(rest_takes(&set, 1, 1.28f, 0.02f, 0, 0, 1));
   CHECK(rest_takes(&set, 0, 1.6f, 0.02f, 1, 1, 0));
   rest_under(&set, 30.0, 0.0);
   CHECK(rest_takes(&set, 1, 1.28f, 0.0f, 0, 0, 1));
   CHECK(rest_takes(&set, 0, 1.6f, 0.0f, 1, 1, 0));
   hinge_set(&set);
   snap_pose(&p, 0.8, 0.0, -1.16, -90.0);
   rest_add(&set, 1, 1.28f, 0.96f, &p);
   CHECK(rest_takes(&set, 0, 1.6f, 0.0f, 2, 1, 3));
   CHECK(rest_takes(&set, 1, 1.28f, 0.0f, 3, 0, 2));
}

/* The pick-up tests the size it is given, as the pairs at rest test
 * each quad's own: given the quad's, it agrees with them; 2 cm taller,
 * screen 2's top edge 1 cm into the 2 cm gap's click, it is not taken
 * hinged. */
static void test_rest_hinge_size(void)
{
   video_xr_quad_set_t set;
   video_xr_hinge_t h;
   video_xr_rest_t r[2];
   const video_xr_quad_t *q = &set.quads[1];
   rest_under(&set, 0.0, 0.02);
   CHECK(video_xr_rest_hinges(&set, 0, 0.02f, r, 2) == 1
         && rest_is(&r[0], 0, 1, 1, 0));
   CHECK(video_xr_rest_hinge(&set, 1, 0, q->width, q->height, 0.02f,
            &q->pose, &h)
         && h.slot == 0 && h.on == 1 && h.edge == 0);
   CHECK(!video_xr_rest_hinge(&set, 1, 0, q->width, q->height + 0.02f,
            0.02f, &q->pose, &h));
}

/* Whether a and b are the same pose to 0.01 mm and 0.01 degrees. */
static int settle_same(const video_xr_pose_t *a, const video_xr_pose_t *b)
{
   return fabs(a->position.x - b->position.x) <= 1e-5
      && fabs(a->position.y - b->position.y) <= 1e-5
      && fabs(a->position.z - b->position.z) <= 1e-5
      && turn_apart(&a->orientation, &b->orientation) <= 0.01;
}

/* Whether screen 2, w wide, settled at pose in h from hand, rests there
 * exactly: put there in set, the pair is listed at rest and taken in h,
 * settling again from where a hand takes it leaves it, and it keeps
 * hand's fold. */
static int settle_rests(video_xr_quad_set_t *set, float w, float gap,
      const video_xr_hinge_t *h, const video_xr_pose_t *hand,
      const video_xr_pose_t *pose)
{
   video_xr_rest_t r[8];
   video_xr_hinge_t k;
   video_xr_pose_t take, again;
   set->quads[1].pose  = *pose;
   set->quads[1].width = w;
   take                = *pose;
   video_xr_rest_take(set, 1, w, 0.96f, gap, h, &take);
   return video_xr_rest_hinges(set, 0, gap, r, 8) == 1
      && (rest_is(&r[0], 1, 0, 0, 1) || rest_is(&r[0], 0, 1, 1, 0))
      && video_xr_rest_hinge(set, 1, 0, w, 0.96f, gap, pose, &k)
      && k.slot == h->slot && k.edge == h->edge && k.on == h->on
      && video_xr_hinge_settle(set, 1, 0, w, 0.96f, gap, h, &take,
            &again)
      && settle_same(&again, pose)
      && turn_apart(&pose->orientation, &hand->orientation) <= 0.01;
}

#define SETTLE_FREE 100.0
#define SETTLE_NONE 200.0

/* Screen 2 let go under screen 1, flat and folded 30 degrees, in the
 * hinge of its top edge on screen 1's bottom: to the set gap from within
 * VIDEO_XR_HINGE_SETTLE of it apart, touching from the stop's hold,
 * where it is at a free gap past that, and not at all past the hinge's
 * reach (Hinge Gap 10 to 11 cm, where it engages). Along, at Hinge Gap
 * 2 cm, level from within twice VIDEO_XR_SNAP_DIST: centres, or a
 * narrower screen's ends. Turned 4.9 degrees off parallel, by a roll or
 * a yaw, it settles parallel; past VIDEO_XR_SNAP_ANGLE, where a grab's
 * hinge lets go, not. Folded 7.5 degrees, it settles at the 5 the turn
 * to flat's fade shows. None in front of screen 1's plane, overlapping
 * too little along, folded past square, with the target skipped, or
 * with no set. Each settled pose rests there. 4 m tall and folded 8
 * degrees, its middle too far out of screen 1's plane for the turn's
 * whole pull, it does not rest, and the settle refuses the pose it
 * gives. */
static void test_hinge_settle(void)
{
   /* Hinge Gap, the hand apart (pushed below 0), settled apart, in cm. */
   static const double row[][3] = {
      { 0.0, 0.0, 0.0 }, { 0.0, 0.3, 0.0 }, { 0.0, 1.5, 0.0 },
      { 0.0, 2.9, 0.0 }, { 0.0, -1.0, 0.0 }, { 0.0, -2.9, 0.0 },
      { 0.0, 3.1, SETTLE_FREE }, { 0.0, 9.9, SETTLE_FREE },
      { 0.0, 10.5, SETTLE_NONE }, { 0.0, -3.2, SETTLE_NONE },
      { 1.0, 0.0, 1.0 }, { 1.0, 3.9, 1.0 }, { 1.0, -0.3, 1.0 },
      { 1.0, -1.0, 0.0 }, { 1.0, -3.4, 0.0 },
      { 1.0, 4.1, SETTLE_FREE }, { 1.0, -3.6, SETTLE_NONE },
      { 2.0, 0.0, 0.0 }, { 2.0, 0.3, 2.0 }, { 2.0, 3.5, 2.0 },
      { 2.0, 4.9, 2.0 }, { 2.0, 5.1, SETTLE_FREE },
      { 2.0, 9.9, SETTLE_FREE }, { 2.0, -2.9, 0.0 },
      { 3.0, 0.5, 3.0 }, { 3.0, 5.9, 3.0 }, { 3.0, 6.1, SETTLE_FREE },
      { 5.0, 1.9, SETTLE_FREE }, { 5.0, 8.1, SETTLE_FREE },
      { 5.0, 2.1, 5.0 }, { 5.0, 7.9, 5.0 },
      { 10.0, 6.9, SETTLE_FREE }, { 10.0, 7.1, 10.0 },
      { 10.0, 11.0, 10.0 }, { 10.0, 11.5, SETTLE_NONE }
   };
   /* Screen 2's centre off screen 1's, its width, where it settles. */
   static const double along[][3] = {
      { 0.029, 1.28, 0.0 }, { 0.04, 1.28, 0.0 }, { 0.059, 1.28, 0.0 },
      { 0.061, 1.28, 0.061 }, { 0.31, 0.9, 0.35 }, { -0.31, 0.9, -0.35 }
   };
   unsigned i, f;
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t hand, pose, want, aim;
   video_xr_hinge_t h, k;
   video_xr_quat_t q;
   memset(&h, 0, sizeof(h));
   h.slot = 0;
   h.edge = 0;
   h.on   = 1;
   for (f = 0; f < 2; f++)
   {
      double fold = f ? 30.0 : 0.0;
      for (i = 0; i < sizeof(row) / sizeof(row[0]); i++)
      {
         float gap = (float)(row[i][0] * 0.01);
         bool ok;
         rest_under(&set, fold, row[i][1] * 0.01);
         hand = set.quads[1].pose;
         ok   = video_xr_hinge_settle(&set, 1, 0, 1.28f, 0.96f, gap, &h,
               &hand, &pose);
         if (row[i][2] == SETTLE_NONE)
            ok = !ok;
         else
         {
            if (row[i][2] == SETTLE_FREE)
               want = hand;
            else
            {
               rest_under(&set, fold, row[i][2] * 0.01);
               want = set.quads[1].pose;
            }
            ok = ok && settle_same(&pose, &want)
               && settle_rests(&set, 1.28f, gap, &h, &hand, &pose);
         }
         if (!ok)
         {
            printf("  FAIL %s:%d: fold %.0f, Hinge Gap %.0f, %.1f cm apart\n",
                  __FILE__, __LINE__, fold, row[i][0], row[i][1]);
            failures++;
         }
      }
      for (i = 0; i < sizeof(along) / sizeof(along[0]); i++)
      {
         float w = (float)along[i][1];
         rest_under(&set, fold, 0.02);
         set.quads[1].width            = w;
         set.quads[1].pose.position.x += (float)along[i][0];
         hand                          = set.quads[1].pose;
         want                          = hand;
         want.position.x               = (float)along[i][2];
         if (!(video_xr_hinge_settle(&set, 1, 0, w, 0.96f, 0.02f, &h,
                     &hand, &pose)
               && settle_same(&pose, &want)
               && settle_rests(&set, w, 0.02f, &h, &hand, &pose)))
         {
            printf("  FAIL %s:%d: fold %.0f, %.1f cm along\n", __FILE__,
                  __LINE__, fold, along[i][0] * 100.0);
            failures++;
         }
      }
   }

   rest_under(&set, 0.0, 0.02);
   hand             = set.quads[1].pose;
   hand.position.z += 0.031f;
   CHECK(!video_xr_hinge_settle(&set, 1, 0, 1.28f, 0.96f, 0.02f, &h,
            &hand, &pose));
   hinge_turn(&q, 0.0, 0.0, 1.0, 5.5);
   hinge_place(&hand, &q, 0.0, 0.48, 0.0, -0.48 - 0.02, -1.8);
   CHECK(!video_xr_hinge_settle(&set, 1, 0, 1.28f, 0.96f, 0.02f, &h,
            &hand, &pose));
   for (i = 0; i < 4; i++)
   {
      bool ok;
      double deg = (i & 1) ? 5.1 : 4.9;
      rest_under(&set, 0.0, 0.02);
      want = set.quads[1].pose;
      hinge_turn(&q, 0.0, (i & 2) ? 1.0 : 0.0, (i & 2) ? 0.0 : 1.0, deg);
      hinge_place(&hand, &q, 0.0, 0.48, 0.0, -0.48 - 0.02, -1.8);
      ok = video_xr_hinge_settle(&set, 1, 0, 1.28f, 0.96f, 0.02f, &h,
            &hand, &pose);
      if ((i & 1) ? ok : !(ok && settle_same(&pose, &want)
               && settle_rests(&set, 1.28f, 0.02f, &h, &want, &pose)))
      {
         printf("  FAIL %s:%d: %s %.1f deg off parallel\n", __FILE__,
               __LINE__, (i & 2) ? "yaw" : "roll", deg);
         failures++;
      }
   }
   rest_under(&set, 0.0, 0.02);
   hand             = set.quads[1].pose;
   hand.position.x += 1.411f;
   CHECK(!video_xr_hinge_settle(&set, 1, 0, 1.28f, 0.96f, 0.02f, &h,
            &hand, &pose));
   hand = set.quads[1].pose;
   CHECK(!video_xr_hinge_settle(&set, 1, 1u << 0, 1.28f, 0.96f, 0.02f,
            &h, &hand, &pose));
   CHECK(!video_xr_hinge_settle(NULL, 1, 0, 1.28f, 0.96f, 0.02f, &h,
            &hand, &pose));
   rest_under(&set, 95.0, 0.02);
   CHECK(!video_xr_hinge_settle(&set, 1, 0, 1.28f, 0.96f, 0.02f, &h,
            &set.quads[1].pose, &pose));
   rest_under(&set, 5.0, 0.02);
   want = set.quads[1].pose;
   rest_under(&set, 7.5, 0.02);
   hand = set.quads[1].pose;
   CHECK(video_xr_hinge_settle(&set, 1, 0, 1.28f, 0.96f, 0.02f, &h,
            &hand, &pose)
         && settle_same(&pose, &want));
   hinge_set(&set);
   hinge_turn(&q, 1.0, 0.0, 0.0, -8.0);
   hinge_place(&want, &q, 0.0, 2.0, 0.0, -0.48, -1.8);
   rest_add(&set, 1, 1.28f, 4.0f, &want);
   hinge_turn(&q, 1.0, 0.0, 0.0, -9.0);
   hinge_place(&hand, &q, 0.0, 2.0, 0.0, -0.48, -1.8);
   pose.position.x = 1000.0f;
   CHECK(!video_xr_rest_hinge(&set, 1, 0, 1.28f, 4.0f, 0.0f, &want, &k)
         && !video_xr_hinge_settle(&set, 1, 0, 1.28f, 4.0f, 0.0f, &h,
            &hand, &pose)
         && pose.position.x != 1000.0f);

   /* A grab settles from where its aim puts its item. */
   rest_grab(&set, &g, &aim);
   g.rel.position.y -= 0.01f;
   hand              = set.quads[1].pose;
   hand.position.y  -= 0.01f;
   CHECK(video_xr_hinge_settle(&set, 1, 0, 1.28f, 0.96f, 0.02f, &h,
            &hand, &want)
         && video_xr_grab_settle(&g, &aim, &set, 0, &h, &pose)
         && settle_same(&pose, &want)
         && settle_same(&want, &set.quads[1].pose));
   CHECK(!video_xr_grab_settle(&g, &aim, NULL, 0, &h, &pose));
}

/* Screen 2 resting touching under screen 1, flat and folded 30 degrees,
 * taken where the hand holds it (video_xr_rest_take) and let go after
 * the hand drifts across the edge: touching while it drifts less than
 * VIDEO_XR_SNAP_STOP_ARM apart, so a still re-grab never moves it to
 * the gap; past that, by the settle bands. */
static void test_hinge_settle_taken(void)
{
   /* Hinge Gap in cm, the drift apart (pushed below 0) in mm, settled
    * apart in cm. */
   static const double row[][3] = {
      { 0.0, 0.02, 0.0 }, { 0.0, 1.0, 0.0 }, { 0.0, 4.9, 0.0 },
      { 0.0, -1.0, 0.0 }, { 0.0, 5.1, 0.0 }, { 0.0, 10.0, 0.0 },
      { 1.0, 0.02, 0.0 }, { 1.0, 1.0, 0.0 }, { 1.0, 4.9, 0.0 },
      { 1.0, -1.0, 0.0 }, { 1.0, 5.1, 1.0 }, { 1.0, 10.0, 1.0 },
      { 2.0, 0.02, 0.0 }, { 2.0, 1.0, 0.0 }, { 2.0, 4.9, 0.0 },
      { 2.0, -1.0, 0.0 }, { 2.0, 5.1, 2.0 }, { 2.0, 10.0, 2.0 },
      { 3.0, 0.02, 0.0 }, { 3.0, 1.0, 0.0 }, { 3.0, 4.9, 0.0 },
      { 3.0, -1.0, 0.0 }, { 3.0, 5.1, 3.0 }, { 3.0, 10.0, 3.0 },
      { 5.0, 0.02, 0.0 }, { 5.0, 1.0, 0.0 }, { 5.0, 4.9, 0.0 },
      { 5.0, -1.0, 0.0 }, { 5.0, 5.1, SETTLE_FREE },
      { 5.0, 10.0, SETTLE_FREE },
      { 10.0, 0.02, 0.0 }, { 10.0, 1.0, 0.0 }, { 10.0, 4.9, 0.0 },
      { 10.0, -1.0, 0.0 }, { 10.0, 5.1, SETTLE_FREE },
      { 10.0, 10.0, SETTLE_FREE }
   };
   unsigned i, f;
   video_xr_quad_set_t set;
   video_xr_pose_t hand, pose, want;
   video_xr_hinge_t h;
   memset(&h, 0, sizeof(h));
   h.slot = 0;
   h.edge = 0;
   h.on   = 1;
   for (f = 0; f < 2; f++)
   {
      double fold = f ? 30.0 : 0.0;
      for (i = 0; i < sizeof(row) / sizeof(row[0]); i++)
      {
         float gap = (float)(row[i][0] * 0.01);
         rest_under(&set, fold, 0.0);
         hand = set.quads[1].pose;
         video_xr_rest_take(&set, 1, 1.28f, 0.96f, gap, &h, &hand);
         hand.position.y -= (float)(row[i][1] * 0.001);
         if (row[i][2] == SETTLE_FREE)
            want = hand;
         else
         {
            rest_under(&set, fold, row[i][2] * 0.01);
            want = set.quads[1].pose;
         }
         if (!(video_xr_hinge_settle(&set, 1, 0, 1.28f, 0.96f, gap, &h,
                     &hand, &pose)
               && settle_same(&pose, &want)
               && settle_rests(&set, 1.28f, gap, &h, &hand, &pose)))
         {
            printf("  FAIL %s:%d: fold %.0f, Hinge Gap %.0f, %.2f mm "
                  "apart\n", __FILE__, __LINE__, fold, row[i][0],
                  row[i][1]);
            failures++;
         }
      }
   }
}

/* Screen 2 held deg degrees off flat with screen 1 in the hinge of
 * kind k, its edge gap metres out from screen 1's: 0 its top edge
 * under screen 1's bottom, tipped toward the user; 1 its bottom over
 * screen 1's top, likewise; 2 its left edge on screen 1's right,
 * turned toward the user about the vertical. */
static void fold_place(unsigned k, double deg, double gap,
      video_xr_pose_t *p)
{
   video_xr_quat_t q;
   if (k == 0)
   {
      hinge_turn(&q, 1.0, 0.0, 0.0, -deg);
      hinge_place(p, &q, 0.0, 0.48, 0.0, -0.48 - gap, -1.8);
   }
   else if (k == 1)
   {
      hinge_turn(&q, 1.0, 0.0, 0.0, deg);
      hinge_place(p, &q, 0.0, -0.48, 0.0, 0.48 + gap, -1.8);
   }
   else
   {
      hinge_turn(&q, 0.0, 1.0, 0.0, -deg);
      hinge_place(p, &q, -0.64, 0.0, 0.8 + gap, 0.0, -1.8);
   }
}

/* Screen 2 of the 3DS layout gripped at pose, held still: its fade
 * whole, or none but the hinge it rests in (taken, unless NULL). */
static void fold_grab(video_xr_grab_t *g, const video_xr_pose_t *pose,
      float gap, const video_xr_hinge_t *taken)
{
   memset(g, 0, sizeof(*g));
   g->rel    = *pose;
   g->turn   = pose->orientation;
   g->last   = *pose;
   g->base   = pose->position;
   g->width  = 1.28f;
   g->height = 0.96f;
   g->slot   = 1;
   g->state  = VIDEO_XR_GRAB_HELD;
   g->tilt   = true;
   g->gap    = (unsigned char)(gap * 100.0f + 0.5f);
   if (taken)
   {
      g->rest_slot = (unsigned char)(taken->slot + 1);
      g->rest_edge = (unsigned char)taken->edge;
      g->rest_on   = (unsigned char)taken->on;
   }
}

/* Screen 2 held hinged deg degrees off flat with screen 1, by its top,
 * bottom or side edge, or off square at a corner, at Hinge Gap 0 and
 * 2: shown where the turn puts it, whole within VIDEO_XR_SNAP_ANGLE,
 * then catching up at twice the hand's speed to twice that, let go it
 * settles there, rests listed, and taken up it shows unmoved with the
 * hand back where it held it. With screen 1 turned, so rounding leaves
 * a flat rest a hair off its goal, a rest flat, square or in the fade
 * is taken likewise and keeps its hinge. */
static void test_hinge_fold_rest(void)
{
   static const double flat[] = { 0.0, 1.0, 3.0, 4.9, 5.1, 6.0, 7.5,
      9.0, 9.9, 10.1, 12.0, 30.0 };
   static const double square[] = { 75.0, 79.0, 80.5, 82.0, 83.7, 84.9,
      85.1, 87.0, 89.9, 90.0 };
   /* Screen 1's yaw, pitch and roll; 1 puts screen 2 back through its
    * settings. */
   static const float turned[3][4] = { { 61.7f, -8.2f, 0.0f, 0.0f },
      { 123.4f, 5.6f, -7.8f, 0.0f }, { -33.0f, 12.5f, 2.0f, 1.0f } };
   /* fold_place's kind, the fold resting, the hand's fold taking it. */
   static const double rests[4][3] = { { 0.0, 0.0, 0.0 },
      { 0.0, 7.5, 8.75 }, { 2.0, 90.0, 90.0 }, { 2.0, 82.5, 81.25 } };
   static const unsigned he[3] = { 0, 1, 3 };
   static const unsigned te[3] = { 1, 0, 2 };
   unsigned row;
   const unsigned nf = sizeof(flat) / sizeof(flat[0]);
   const unsigned n  = 6 * nf
      + 2 * (unsigned)(sizeof(square) / sizeof(square[0]));
   for (row = 0; row < n; row++)
   {
      unsigned i, nr;
      video_xr_quad_set_t set;
      video_xr_rest_t r[8];
      video_xr_grab_t g;
      video_xr_pose_t hand, aim, start, shown, pose, take, again;
      video_xr_hinge_t h, k;
      double a, off, want, back;
      int listed = 0;
      int ok;
      bool sq      = row >= 6 * nf;
      unsigned j   = sq ? 2 : row / (2 * nf);
      float gap    = (sq ? (row - 6 * nf) % 2 : (row / nf) % 2)
         ? 0.02f : 0.0f;
      double deg   = sq ? square[(row - 6 * nf) / 2] : flat[row % nf];
      const video_xr_quat_t *q1;
      memset(&h, 0, sizeof(h));
      h.edge = he[j];
      h.on   = te[j];
      hinge_set(&set);
      fold_place(j, deg, gap, &hand);
      rest_add(&set, 1, 1.28f, 0.96f, &hand);
      q1 = &set.quads[0].pose.orientation;
      a  = sq ? 90.0 - deg : deg;
      off = (a <= VIDEO_XR_SNAP_ANGLE) ? 0.0
         : (a < 2.0 * VIDEO_XR_SNAP_ANGLE)
         ? 2.0 * (a - VIDEO_XR_SNAP_ANGLE) : a;
      want = sq ? 90.0 - off : off;
      back = (a <= VIDEO_XR_SNAP_ANGLE) ? want : deg;

      video_xr_pose_identity(&aim);
      fold_grab(&g, &hand, gap, NULL);
      start             = hand;
      start.position.x += 1.0f;
      ok = (video_xr_grab_hinge(&g, &aim, &set, 0, &start, &shown, &k)
            & VIDEO_XR_SNAP_HINGE)
         && k.slot == 0 && k.edge == h.edge && k.on == h.on
         && fabs(turn_apart(&shown.orientation, q1) - want) <= 0.01
         && video_xr_hinge_settle(&set, 1, 0, 1.28f, 0.96f, gap, &h,
               &hand, &pose)
         && settle_same(&pose, &shown);
      if (ok)
      {
         set.quads[1].pose = pose;
         nr = video_xr_rest_hinges(&set, 0, gap, r, 8);
         for (i = 0; i < nr; i++)
            if (     rest_is(&r[i], 1, h.edge, 0, h.on)
                  || rest_is(&r[i], 0, h.on, 1, h.edge))
               listed = 1;
         take = pose;
         video_xr_rest_take(&set, 1, 1.28f, 0.96f, gap, &h, &take);
         fold_grab(&g, &take, gap, &h);
         ok = listed
            && (video_xr_grab_hinge(&g, &aim, &set, 0, &pose, &again, &k)
               & VIDEO_XR_SNAP_HINGE)
            && settle_same(&again, &pose)
            && fabs(turn_apart(&take.orientation, q1) - back) <= 0.01;
      }
      if (!ok)
      {
         printf("  FAIL %s:%d: %s edge, Hinge Gap %.0f, held %.1f deg "
               "off %s\n", __FILE__, __LINE__,
               (j == 0) ? "top" : (j == 1) ? "bottom" : "side",
               gap * 100.0f, a, sq ? "square" : "flat");
         failures++;
      }
   }
   for (row = 0; row < 24; row++)
   {
      unsigned i, nr;
      video_xr_quad_set_t set;
      video_xr_rest_t r[8];
      video_xr_grab_t g;
      video_screen_xr_t s;
      video_xr_vec3_t base;
      video_xr_pose_t rest, take, aim, again, pose;
      video_xr_hinge_t h, k;
      int listed = 0;
      int ok;
      const float *t  = turned[row / 8];
      const double *f = rests[(row / 2) % 4];
      unsigned j      = (unsigned)f[0];
      float gap       = (row & 1) ? 0.02f : 0.0f;
      const video_xr_quat_t *q1;
      memset(&h, 0, sizeof(h));
      h.edge = he[j];
      h.on   = te[j];
      memset(&s, 0, sizeof(s));
      s.yaw   = t[0];
      s.pitch = t[1];
      s.roll  = t[2];
      s.scale = 1.0f;
      base.x  = 0.0f;
      base.y  = 0.0f;
      base.z  = -1.8f;
      hinge_set(&set);
      video_xr_item_pose(&base, &s, &set.quads[0].pose);
      q1 = &set.quads[0].pose.orientation;
      fold_place(j, f[1], gap, &rest);
      rest.position.z += 1.8f;
      video_xr_compose(&set.quads[0].pose, &rest, &rest);
      if (f[1] == 0.0)
         rest.orientation = *q1;
      if (t[3] != 0.0f)
      {
         memset(&s, 0, sizeof(s));
         s.scale = 1.0f;
         video_xr_item_settings(&base, &rest, &s);
         video_xr_item_pose(&base, &s, &rest);
      }
      rest_add(&set, 1, 1.28f, 0.96f, &rest);
      nr = video_xr_rest_hinges(&set, 0, gap, r, 8);
      for (i = 0; i < nr; i++)
         if (     rest_is(&r[i], 1, h.edge, 0, h.on)
               || rest_is(&r[i], 0, h.on, 1, h.edge))
            listed = 1;
      take = rest;
      video_xr_rest_take(&set, 1, 1.28f, 0.96f, gap, &h, &take);
      video_xr_pose_identity(&aim);
      fold_grab(&g, &take, gap, &h);
      ok = listed
         && fabs(turn_apart(&take.orientation, q1) - f[2]) <= 0.01
         && fabs(turn_apart(&take.orientation, &rest.orientation)
               - fabs(f[2] - f[1])) <= 0.01
         && (video_xr_grab_hinge(&g, &aim, &set, 0, &rest, &again, &k)
            & VIDEO_XR_SNAP_HINGE)
         && k.slot == 0 && k.edge == h.edge && k.on == h.on
         && settle_same(&again, &rest)
         && video_xr_hinge_settle(&set, 1, 0, 1.28f, 0.96f, gap, &h,
               &take, &pose)
         && settle_same(&pose, &rest);
      if (!ok)
      {
         printf("  FAIL %s:%d: screen 1 turned %.1f %.1f %.1f%s, %s "
               "edge, Hinge Gap %.0f, resting %.1f deg off flat\n",
               __FILE__, __LINE__, t[0], t[1], t[2],
               t[3] != 0.0f ? " (from settings)" : "",
               j ? "side" : "top", gap * 100.0f, f[1]);
         failures++;
      }
   }
}

/* The hinge a grab was taken in turns it whole whatever the fade, even
 * with a nearer screen still fading in: screen 2 of the flat 3DS pair
 * at Hinge Gap 0, its kept hinge on screen 1, held 5 mm toward the user
 * and tipped 2 degrees, with a third screen 2 cm right of it in the
 * hand's plane; taken there (a fade of none) or where it rests (fading
 * in), it is hinged flat where it rests, as without the third. */
static void test_grab_kept_turn(void)
{
   unsigned i;
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, rest, start, pose, p;
   video_xr_hinge_t h;
   for (i = 0; i < 2; i++)
   {
      rest_under(&set, 0.0, 0.0);
      rest = set.quads[1].pose;
      memset(&g, 0, sizeof(g));
      video_xr_pose_identity(&aim);
      g.rel             = rest;
      g.rel.position.z += 0.005f;
      hinge_turn(&g.rel.orientation, 1.0, 0.0, 0.0, 2.0);
      g.turn            = g.rel.orientation;
      g.last            = g.rel;
      g.base            = rest.position;
      g.width           = 1.28f;
      g.height          = 0.96f;
      g.slot            = 1;
      g.state           = VIDEO_XR_GRAB_HELD;
      g.rest_slot       = 1;
      g.rest_on         = 1;
      g.rest_edge       = 0;
      start             = i ? rest : g.rel;
      CHECK((video_xr_grab_hinge(&g, &aim, &set, 0, &start, &pose, &h)
               & VIDEO_XR_SNAP_HINGE)
            && h.slot == 0 && pose_near(&pose, &rest));
      snap_pose(&p, 1.28 + 0.02, -0.96, -1.795, 0.0);
      rest_add(&set, 2, 1.28f, 0.96f, &p);
      CHECK((video_xr_grab_hinge(&g, &aim, &set, 0, &start, &pose, &h)
               & VIDEO_XR_SNAP_HINGE)
            && h.slot == 0 && pose_near(&pose, &rest));
   }
}

#define HINGE_LINES (VIDEO_XR_SNAP_X | VIDEO_XR_SNAP_Y | VIDEO_XR_SNAP_Z)

/* The 3DS pair: screen 2 tipped 30 degrees toward the user about its
 * top edge, that edge 0.8 cm under and 1.3 cm in front of screen 1's
 * bottom edge and turned 3 degrees off it, by a yaw or a roll, snaps
 * touching and exactly parallel and keeps its 30 degrees: a hinge, its
 * top edge on that line, swung by the tilt. */
static void test_hinge_3ds(void)
{
   unsigned i;
   video_xr_quad_set_t set;
   video_xr_pose_t p, want;
   video_xr_quat_t tip, off, q;
   video_xr_hinge_t h;
   hinge_set(&set);
   hinge_turn(&tip, 1.0, 0.0, 0.0, -30.0);
   hinge_place(&want, &tip, 0.0, 0.48, 0.0, -0.48, -1.8);
   for (i = 0; i < 2; i++)
   {
      unsigned bits;
      hinge_turn(&off, 0.0, i ? 0.0 : 1.0, i ? 1.0 : 0.0, 3.0);
      hinge_then(&off, &tip, &q);
      hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.488, -1.787);
      memset(&h, 0, sizeof(h));
      bits = hinge_snap(&set, &p, 1.0f, &h);
      CHECK((bits & VIDEO_XR_SNAP_HINGE)
            && (bits & VIDEO_XR_SNAP_ALL) == HINGE_LINES);
      CHECK(vec_is(&p.position, 0.0, -0.895692, -1.56)
            && turn_apart(&p.orientation, &want.orientation) < 0.01);
      CHECK(h.edge == 0 && h.axis == VIDEO_XR_STEP_TILT
            && fabs(h.angle + 30.0) < 0.01);
      CHECK(vec_is(&h.point, 0.0, -0.48, -1.8)
            && vec_is(&h.dir, 1.0, 0.0, 0.0)
            && vec_is(&h.flat, 0.0, -1.0, 0.0));
      CHECK(fabs(video_xr_hinge_angle(&h, &p.orientation) + 30.0) < 0.01);
   }
   /* No hinge out asked: the same pose. */
   hinge_turn(&off, 0.0, 1.0, 0.0, 3.0);
   hinge_then(&off, &tip, &q);
   hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.488, -1.787);
   want = p;
   hinge_snap(&set, &p, 1.0f, &h);
   CHECK((video_xr_snap(&set, 1, 0, 1.28f, 0.96f, 1.0f, &want)
            & VIDEO_XR_SNAP_HINGE) && pose_same(&p, &want));
}

/* Tipped square about the line, as the 90 degree step leaves it, a
 * rounding past square still hinges; 0.05 degrees past does not. */
static void test_hinge_square_fold(void)
{
   static const double past[3] = { 1e-4, -1e-4, 0.05 };
   unsigned i;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_quat_t q;
   video_xr_hinge_t h;
   hinge_set(&set);
   for (i = 0; i < 3; i++)
   {
      hinge_turn(&q, 1.0, 0.0, 0.0, -90.0 - past[i]);
      hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.48, -1.8);
      CHECK(((hinge_snap(&set, &p, 1.0f, &h) & VIDEO_XR_SNAP_HINGE) != 0)
            == (i < 2));
   }
}

/* Side by side like a triptych: screen 2's left edge, 3 degrees tipped
 * off upright, 0.8 cm right of and 1.3 cm in front of screen 1's right
 * edge, turned 30 degrees toward the user: hinged there, swung by the
 * turn. On its side, its top edge hinges the same way. */
static void test_hinge_side(void)
{
   unsigned bits;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_quat_t turn, off, q;
   video_xr_hinge_t h;
   hinge_set(&set);
   hinge_turn(&turn, 0.0, 1.0, 0.0, -30.0);
   hinge_turn(&off, 1.0, 0.0, 0.0, 3.0);
   hinge_then(&off, &turn, &q);
   hinge_place(&p, &q, -0.64, 0.0, 0.808, 0.0, -1.787);
   bits = hinge_snap(&set, &p, 1.0f, &h);
   CHECK((bits & VIDEO_XR_SNAP_HINGE)
         && (bits & VIDEO_XR_SNAP_ALL) == HINGE_LINES);
   CHECK(vec_is(&p.position, 1.354256, 0.0, -1.48)
         && turn_apart(&p.orientation, &turn) < 0.01);
   CHECK(h.edge == 3 && h.axis == VIDEO_XR_STEP_TURN
         && fabs(h.angle + 30.0) < 0.01
         && vec_is(&h.point, 0.8, 0.0, -1.8) && vec_is(&h.dir, 0.0, 1.0, 0.0)
         && vec_is(&h.flat, 1.0, 0.0, 0.0));
   /* Turned a quarter in its own plane, then as before. */
   hinge_turn(&q, 0.0, 0.0, 1.0, 90.0);
   hinge_then(&turn, &q, &q);
   hinge_then(&off, &q, &q);
   hinge_place(&p, &q, 0.0, 0.48, 0.808, 0.0, -1.787);
   bits = hinge_snap(&set, &p, 1.0f, &h);
   CHECK((bits & VIDEO_XR_SNAP_HINGE) && h.edge == 0
         && h.axis == VIDEO_XR_STEP_TURN && fabs(h.angle + 30.0) < 0.01);
   CHECK(vec_is(&p.position, 1.215692, 0.0, -1.56));
}

/* Screens that overlap never hinge: screen 2 flat 1 cm in front of
 * screen 1, top edges 1 cm apart, levels as before; folded back over
 * it from its bottom edge, 170 or 135 degrees from flat, the pull only
 * fades, whole again by 90. */
static void test_hinge_overlap(void)
{
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_quat_t q;
   hinge_set(&set);
   video_xr_pose_identity(&p);
   p.position.x = 0.1f;
   p.position.y = -0.01f;
   p.position.z = -1.79f;
   CHECK(hinge_snap(&set, &p, 1.0f, NULL)
         == (VIDEO_XR_SNAP_TURN | VIDEO_XR_SNAP_Y | VIDEO_XR_SNAP_Z
            | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_TURN | VIDEO_XR_SNAP_Y
               | VIDEO_XR_SNAP_Z)));
   CHECK(vec_is(&p.position, 0.1, 0.0, -1.8));
   hinge_turn(&q, 1.0, 0.0, 0.0, -170.0);
   hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.49, -1.79);
   CHECK(hinge_snap(&set, &p, 1.0f, NULL)
         == (VIDEO_XR_SNAP_PULLING(HINGE_LINES)
            | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   hinge_turn(&q, 1.0, 0.0, 0.0, -135.0);
   hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.49, -1.79);
   CHECK(hinge_snap(&set, &p, 1.0f, NULL)
         == (VIDEO_XR_SNAP_PULLING(HINGE_LINES)
            | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   hinge_turn(&q, 1.0, 0.0, 0.0, -90.0);
   hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.49, -1.79);
   CHECK(hinge_snap(&set, &p, 1.0f, NULL) & VIDEO_XR_SNAP_HINGE);
   CHECK(vec_is(&p.position, 0.0, -0.48, -1.32));
}

/* Out of range nothing; in a fade band a part pull, as the other snaps:
 * 7 and 4.5 cm under, hinged where held, the gap free; the grab's fade
 * half in, half way; 7.5 degrees off parallel, turned to 5; 12, none.
 * Along screen 1's edge, 1 cm past its end, none; overlapping it
 * 1.5 cm, half way; 4 cm, a hinge. Flat 1.5 cm in, the corner's X line
 * leaves 1 cm overlapping: lined up, but no hinge. */
static void test_hinge_range(void)
{
   video_xr_quad_set_t set;
   video_xr_pose_t p, was;
   video_xr_quat_t tip, off, q;
   video_xr_hinge_t h;
   hinge_set(&set);
   hinge_turn(&tip, 1.0, 0.0, 0.0, -30.0);
   hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.55, -1.8);
   was = p;
   CHECK(hinge_snap(&set, &p, 1.0f, &h) == (VIDEO_XR_SNAP_HINGE
            | HINGE_LINES | VIDEO_XR_SNAP_PULLING(HINGE_LINES))
         && pose_same(&p, &was) && fabs(h.gap - 0.07) < 1e-5);
   hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.525, -1.8);
   CHECK(hinge_snap(&set, &p, 1.0f, &h) == (VIDEO_XR_SNAP_HINGE
            | HINGE_LINES | VIDEO_XR_SNAP_PULLING(HINGE_LINES)));
   CHECK(vec_is(&p.position, 0.0, -0.940692, -1.56)
         && fabs(h.gap - 0.045) < 1e-5);
   hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.49, -1.8);
   CHECK(hinge_snap(&set, &p, 0.5f, NULL)
         == (VIDEO_XR_SNAP_PULLING(HINGE_LINES)
            | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   CHECK(vec_is(&p.position, 0.0, -0.900692, -1.56));
   hinge_turn(&off, 0.0, 1.0, 0.0, 7.5);
   hinge_then(&off, &tip, &q);
   hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.49, -1.8);
   CHECK(hinge_snap(&set, &p, 1.0f, NULL)
         == (VIDEO_XR_SNAP_PULLING(HINGE_LINES)
            | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   CHECK(fabs(snap_yaw(&p) - 5.0) < 0.02);
   hinge_turn(&off, 0.0, 1.0, 0.0, 12.0);
   hinge_then(&off, &tip, &q);
   hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.49, -1.8);
   was = p;
   CHECK(hinge_snap(&set, &p, 1.0f, NULL) == 0 && pose_same(&p, &was));
   hinge_place(&p, &tip, 0.0, 0.48, 1.45, -0.49, -1.8);
   was = p;
   CHECK(hinge_snap(&set, &p, 1.0f, NULL) == 0 && pose_same(&p, &was));
   hinge_place(&p, &tip, 0.0, 0.48, 1.425, -0.49, -1.8);
   CHECK(hinge_snap(&set, &p, 1.0f, NULL)
         == VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_Y | VIDEO_XR_SNAP_Z
            | VIDEO_XR_SNAP_GAP));
   CHECK(vec_is(&p.position, 1.425, -0.900692, -1.56));
   hinge_place(&p, &tip, 0.0, 0.48, 1.4, -0.49, -1.8);
   CHECK(hinge_snap(&set, &p, 1.0f, NULL) & VIDEO_XR_SNAP_HINGE);
   snap_pose(&p, 1.425, -0.968, -1.8, 0.0);
   CHECK(hinge_snap(&set, &p, 1.0f, NULL) == (VIDEO_XR_SNAP_TURN
            | VIDEO_XR_SNAP_Y | VIDEO_XR_SNAP_Z
            | VIDEO_XR_SNAP_PULLING(SNAP_ALL)
            | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   CHECK(vec_is(&p.position, 1.43, -0.96, -1.8));
}

/* A hinge at 0 and at 90 degrees is the plain flush and corner snap:
 * the same poses, to rounding, and also a hinge. Flat: yawed 3
 * degrees, 0.8 cm under and 1.3 cm out. Square: 2 degrees short,
 * behind screen 1's right corner, 0.5 cm out and 1 cm back. */
static void test_hinge_flush_corner(void)
{
   unsigned bits;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_quat_t q;
   video_xr_hinge_t h;
   hinge_set(&set);
   hinge_turn(&q, 0.0, 1.0, 0.0, 3.0);
   hinge_place(&p, &q, 0.0, 0.48, 0.05, -0.488, -1.787);
   bits = hinge_snap(&set, &p, 1.0f, &h);
   CHECK(bits == (VIDEO_XR_SNAP_HINGE | VIDEO_XR_SNAP_TURN | VIDEO_XR_SNAP_Y
            | VIDEO_XR_SNAP_Z | VIDEO_XR_SNAP_PULLING(SNAP_ALL)
            | VIDEO_XR_SNAP_GAP | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   CHECK(fabs(p.position.x - 0.04) < 1e-6 && fabs(p.position.y + 0.96) < 1e-6
         && fabs(p.position.z + 1.8) < 1e-6
         && turn_apart(&p.orientation, &set.quads[0].pose.orientation) < 1e-3);
   CHECK(h.edge == 0 && fabs(h.angle) < 1e-3);
   hinge_turn(&q, 0.0, 1.0, 0.0, 88.0);
   hinge_place(&p, &q, 0.64, 0.0, 0.805, 0.01, -1.81);
   bits = hinge_snap(&set, &p, 1.0f, &h);
   CHECK(bits == GAP_ALL);
   CHECK(fabs(p.position.x - 0.8) < 1e-6 && fabs(p.position.y) < 1e-6
         && fabs(p.position.z + 1.16) < 1e-6
         && angle_near((float)snap_yaw(&p), 90.0f));
   CHECK(h.edge == 2 && h.axis == VIDEO_XR_STEP_TURN
         && fabs(fabs(h.angle) - 90.0) < 1e-3);
}

/* A turn to the hinge's own screen turns about the shared edge: tipped
 * 4.5 degrees about it, on it, screen 2 snaps flat and stays on the
 * line, flush, where turning about its middle would lift that edge
 * 3.8 cm off it and leave it short of screen 1's plane. */
static void test_hinge_pivot(void)
{
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_quat_t q;
   hinge_set(&set);
   hinge_turn(&q, 1.0, 0.0, 0.0, -4.5);
   hinge_place(&p, &q, 0.0, 0.48, 0.05, -0.48, -1.8);
   CHECK(hinge_snap(&set, &p, 1.0f, NULL) & VIDEO_XR_SNAP_HINGE);
   CHECK(vec_is(&p.position, 0.04, -0.96, -1.8)
         && turn_apart(&p.orientation, &set.quads[0].pose.orientation) < 1e-3);
}

/* Of several hinges the one its pull leaves nearest wins, as for every
 * snap: two screens above screen 2, A level, C turned 2 degrees, their
 * bottom edges as one line, screen 2 tipped 30 and yawed 3 under both:
 * whole on A beats fading on C, whole on C beats fading on A, and of two
 * whole, the nearer (C's 1 cm to A's 2). A screen the whole turn goes
 * to keeps it: no hinge turns it from there. */
static void test_hinge_ranked(void)
{
   unsigned i;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_quat_t tip, off, q, c;
   video_xr_hinge_t h;
   const double ya[3] = { -0.48, -0.385, -0.48 };
   const double yc[3] = { -0.385, -0.48, -0.49 };
   const double yh[3] = { -0.5, -0.5, -0.49 };
   hinge_turn(&tip, 1.0, 0.0, 0.0, -30.0);
   hinge_turn(&off, 0.0, 1.0, 0.0, 3.0);
   hinge_then(&off, &tip, &q);
   hinge_turn(&c, 0.0, 1.0, 0.0, 2.0);
   for (i = 0; i < 3; i++)
   {
      memset(&set, 0, sizeof(set));
      snap_add(&set, 0, -0.32, 0.0);
      set.quads[0].pose.position.y = (float)(ya[i] + 0.2);
      set.quads[0].width           = 0.6f;
      snap_add(&set, 2, 0.32, 2.0);
      set.quads[1].pose.position.y = (float)(yc[i] + 0.2);
      set.quads[1].width           = 0.6f;
      set.quads[1].pose.position.z = -1.8f;
      set.quads[0].pose.position.z = -1.8f;
      hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.5, -1.8);
      CHECK(hinge_snap(&set, &p, 1.0f, &h) & VIDEO_XR_SNAP_HINGE);
      CHECK(h.edge == 0 && fabs(h.angle + 30.0) < 0.01);
      if (i)
         CHECK(NEAR(h.point.y, yh[i]) && vec_is(&h.dir, 0.999391, 0.0,
                  -0.034899));
      else
         CHECK(NEAR(h.point.y, yh[i]) && vec_is(&h.dir, 1.0, 0.0, 0.0));
   }
   /* C nearer, 1 cm to A's 2.5, but 7.5 degrees off, its pull fading:
    * A, whole at 3, still wins. C turned 10.5 about where its edge line
    * crosses under screen 2's middle. */
   set.quads[0].pose.position.y  = -0.275f;
   hinge_turn(&set.quads[1].pose.orientation, 0.0, 1.0, 0.0, 10.5);
   set.quads[1].pose.position.x  = (float)(0.32 * cos(10.5 * M_PI / 180.0));
   set.quads[1].pose.position.z  = (float)(-1.8
         - 0.32 * sin(10.5 * M_PI / 180.0));
   hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.5, -1.8);
   CHECK(hinge_snap(&set, &p, 1.0f, &h) & VIDEO_XR_SNAP_HINGE);
   CHECK(NEAR(h.point.y, -0.5) && vec_is(&h.dir, 1.0, 0.0, 0.0));
   /* A screen 15 cm right of screen 2's place, as it is turned 2 more:
    * the turn goes whole to it, and screen 1's hinge 3 degrees off is
    * left at that. */
   hinge_set(&set);
   hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.49, -1.8);
   {
      video_xr_quad_t *t = &set.quads[set.num_quads++];
      video_xr_vec3_t o, w;
      *t        = set.quads[0];
      hinge_then(&q, &c, &t->pose.orientation);
      o.x       = 1.04f;
      o.y       = 0.0f;
      o.z       = 0.0f;
      video_xr_rotate(&q, &o, &w);
      t->pose.position.x = p.position.x + w.x;
      t->pose.position.y = p.position.y + w.y;
      t->pose.position.z = p.position.z + w.z;
      t->width  = 0.5f;
      t->height = 0.4f;
      t->slot   = 2;
      t->screen = 2;
   }
   CHECK(hinge_snap(&set, &p, 1.0f, NULL) == (SNAP_ALL
            | VIDEO_XR_SNAP_PULLING(SNAP_ALL)
            | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   CHECK(turn_apart(&p.orientation, &set.quads[1].pose.orientation) < 0.02);
}

/* Turned whole to a screen on its right, screen 2 keeps a hinge to
 * screen 1 as that turn leaves it: rolled 5.5 degrees off screen 1,
 * its top edge 1 cm under, the other screen rolled 3.9 to 4.9 more, so
 * the pair ends 9.4 to 10.4 off parallel. No step hops, and at 10.4
 * the hinge still pulls that edge up toward screen 1's. */
static void test_hinge_turned_away(void)
{
   unsigned i, bits;
   video_xr_quad_set_t set;
   video_xr_pose_t p, last;
   video_xr_quat_t q;
   video_xr_vec3_t o, w;
   video_xr_quad_t *t;
   hinge_set(&set);
   hinge_turn(&q, 0.0, 0.0, 1.0, -5.5);
   t         = &set.quads[set.num_quads++];
   *t        = set.quads[0];
   t->width  = 0.5f;
   t->height = 0.4f;
   t->slot   = 2;
   t->screen = 2;
   o.x       = 1.04f;
   o.y       = 0.06f;
   o.z       = 0.0f;
   video_xr_pose_identity(&last);
   for (i = 0; i <= 100; i++)
   {
      hinge_place(&p, &q, 0.0, 0.48, -0.04, -0.49, -1.8);
      video_xr_rotate(&q, &o, &w);
      t->pose.position.x = p.position.x + w.x;
      t->pose.position.y = p.position.y + w.y;
      t->pose.position.z = p.position.z + w.z;
      hinge_turn(&t->pose.orientation, 0.0, 0.0, 1.0, -9.4 - 0.01 * i);
      bits = hinge_snap(&set, &p, 1.0f, NULL);
      CHECK(turn_apart(&p.orientation, &t->pose.orientation) < 1e-3);
      if (i)
         CHECK(pose_gap(&p, &last) <= 0.0005);
      last = p;
   }
   CHECK(bits == (VIDEO_XR_SNAP_TURN | VIDEO_XR_SNAP_Z
            | VIDEO_XR_SNAP_PULLING(SNAP_ALL)
            | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   CHECK(vec_is(&p.position, -0.088357, -0.963336, -1.8));
}

/* A whole line to another screen beats a hinge's part pull: screen 2
 * tipped 7 degrees, its top edge 5 mm under screen 1's bottom edge and
 * overlapping it 1.5 cm, a screen parallel to it on its right, carried
 * from 3 cm behind screen 1's line to 3 cm in front in 0.1 mm steps.
 * It stays in that screen's plane, and no step hops. */
static void test_hinge_outranked(void)
{
   unsigned i, bits;
   video_xr_quad_set_t set;
   video_xr_pose_t p, last;
   video_xr_quat_t q;
   video_xr_vec3_t o, w;
   video_xr_quad_t *t;
   hinge_set(&set);
   hinge_turn(&q, 1.0, 0.0, 0.0, -7.0);
   t         = &set.quads[set.num_quads++];
   *t        = set.quads[0];
   t->width  = 0.5f;
   t->height = 0.4f;
   t->slot   = 2;
   t->screen = 2;
   o.x       = 1.04f;
   o.y       = 0.14f;
   o.z       = 0.0f;
   video_xr_pose_identity(&last);
   for (i = 0; i <= 600; i++)
   {
      hinge_place(&p, &q, 0.0, 0.48, 1.425, -0.485, -1.83 + 0.0001 * i);
      video_xr_rotate(&q, &o, &w);
      t->pose.orientation = q;
      t->pose.position.x  = p.position.x + w.x;
      t->pose.position.y  = p.position.y + w.y;
      t->pose.position.z  = p.position.z + w.z;
      bits = hinge_snap(&set, &p, 1.0f, NULL);
      CHECK(bits & VIDEO_XR_SNAP_Z);
      if (i)
         CHECK(pose_gap(&p, &last) <= 0.0005);
      last = p;
   }
}

/* No jumps: screen 2, its top edge 1.5 cm under and 1.3 cm out from
 * screen 1's bottom edge and yawed 3, folded from -100 to 100 degrees
 * about that edge in quarter degrees; on the line, through flat and
 * square's fades in tenths, and tipped to 12 degrees in hundredths
 * 1 cm past the end of screen 1's edge, a diagonal neighbour that never
 * hinges nor pulls as one; carried away from the line in 0.5 mm steps
 * and slid along it from 5 cm of overlap to 2 cm past the end in 1 mm
 * steps, at 30 degrees, let go of there, and in flat's fade at 7.5
 * and 9.5.
 * No step moves it more than the bound, the hand's own step a few
 * times over. */
static void test_hinge_continuous(void)
{
   unsigned i, j, bits;
   video_xr_quad_set_t set;
   video_xr_pose_t p, last, was;
   video_xr_quat_t tip, off, q;
   double most = 0.0;
   hinge_set(&set);
   video_xr_pose_identity(&last);
   hinge_turn(&off, 0.0, 1.0, 0.0, 3.0);
   for (i = 0; i <= 800; i++)
   {
      hinge_turn(&tip, 1.0, 0.0, 0.0, -100.0 + 0.25 * i);
      hinge_then(&off, &tip, &q);
      hinge_place(&p, &q, 0.0, 0.48, 0.05, -0.495, -1.787);
      hinge_snap(&set, &p, 1.0f, NULL);
      if (i)
         CHECK(pose_gap(&p, &last) <= 0.0080
               && turn_apart(&p.orientation, &last.orientation) <= 0.5);
      last = p;
   }
   for (j = 0; j < 3; j++)
   {
      for (i = 0; i <= ((j == 2) ? 1200u : 2000u); i++)
      {
         if (j == 2)
         {
            hinge_turn(&q, 1.0, 0.0, 0.0, -0.01 * i);
            hinge_place(&p, &q, 0.0, 0.48, 1.45, -0.48, -1.8);
         }
         else if (j)
         {
            hinge_turn(&q, 0.0, 1.0, 0.0, -80.0 - 0.01 * i);
            hinge_place(&p, &q, -0.64, 0.0, 0.814, 0.01, -1.786);
         }
         else
         {
            hinge_turn(&q, 1.0, 0.0, 0.0, -0.1 * i);
            hinge_place(&p, &q, 0.0, 0.48, 0.05, -0.48, -1.8);
         }
         bits = hinge_snap(&set, &p, 1.0f, NULL);
         if (j == 2)
            CHECK(!(bits & VIDEO_XR_SNAP_HINGE)
                  && (!i || pose_gap(&p, &last) <= 0.0005));
         if (j == 2 && i == 600 && t_gap == 0.0f)
            CHECK(fabs(p.position.x - 1.442) < 1e-5
                  && fabs(p.position.y + 0.959006) < 1e-5
                  && fabs(p.position.z + 1.763161) < 1e-5);
         if (i && pose_gap(&p, &last) > most)
            most = pose_gap(&p, &last);
         last = p;
      }
   }
   CHECK(most <= 0.0040);
   for (j = 0; j < 2; j++)
   {
      hinge_turn(&tip, 1.0, 0.0, 0.0, j ? -7.5 : -30.0);
      hinge_then(&off, &tip, &q);
      for (i = 0; i <= 160; i++)
      {
         hinge_place(&p, &q, 0.0, 0.48, 0.05, -0.48 - 0.0005 * i, -1.8);
         hinge_snap(&set, &p, 1.0f, NULL);
         if (i)
            CHECK(pose_gap(&p, &last) <= 0.0015);
         last = p;
      }
   }
   for (j = 0; j < 3; j++)
   {
      hinge_turn(&tip, 1.0, 0.0, 0.0, (j == 2) ? -9.5 : j ? -7.5 : -30.0);
      hinge_then(&off, &tip, &q);
      for (i = 0; i <= 70; i++)
      {
         hinge_place(&p, &q, 0.0, 0.48, 1.39 + 0.001 * i, -0.495, -1.787);
         was  = p;
         bits = hinge_snap(&set, &p, 1.0f, NULL);
         if (!i && !j)
            CHECK(bits & VIDEO_XR_SNAP_HINGE);
         if (i)
            CHECK(pose_gap(&p, &last) <= 0.0030);
         last = p;
      }
      if (!j)
         CHECK(bits == 0 && pose_same(&p, &was));
   }
}

/* Rolled 1 to 4 degrees off the line, its edge's middle on it 10 cm
 * along, screen 2 swung through flat's fade band under screen 1, or
 * square's beside it: that middle keeps its place along the line. */
static void test_hinge_roll_band(void)
{
   static const double rolls[] = { 1.0, 2.0, 4.0 };
   unsigned k, ri, i;
   double most = 0.0;
   for (k = 0; k < 2; k++)
   for (ri = 0; ri < 3; ri++)
   for (i = 0; i <= 240; i++)
   {
      video_xr_quad_set_t set;
      video_xr_pose_t p;
      video_xr_quat_t a, b, q;
      video_xr_vec3_t m, w;
      double along;
      hinge_set(&set);
      hinge_turn(&b, 0.0, 0.0, 1.0, rolls[ri]);
      if (k)
      {
         hinge_turn(&a, 0.0, 1.0, 0.0, -78.0 - 0.05 * i);
         hinge_then(&a, &b, &q);
         hinge_place(&p, &q, -0.64, 0.0, 0.8, 0.10, -1.8);
      }
      else
      {
         hinge_turn(&a, 1.0, 0.0, 0.0, -12.0 + 0.05 * i);
         hinge_then(&b, &a, &q);
         hinge_place(&p, &q, 0.0, 0.48, 0.10, -0.48, -1.8);
      }
      CHECK(hinge_snap(&set, &p, 1.0f, NULL) & VIDEO_XR_SNAP_HINGE);
      m.x   = k ? -0.64f : 0.0f;
      m.y   = k ? 0.0f : 0.48f;
      m.z   = 0.0f;
      video_xr_rotate(&p.orientation, &m, &w);
      along = k ? (double)p.position.y + w.y : (double)p.position.x + w.x;
      if (fabs(along - 0.10) > most)
         most = fabs(along - 0.10);
   }
   if (most > 1e-4)
   {
      printf("  FAIL %s:%d: the edge's middle %.4f m along the line off"
            " its place\n", __FILE__, __LINE__, most);
      failures++;
   }
}

/* The 3DS pair held: screen 2 flat under screen 1, its top edge on
 * screen 1's bottom edge, taken a metre away so its snaps are whole,
 * carried or tilting. */
static void hinge_grab(video_xr_quad_set_t *set, video_xr_grab_t *g,
      video_xr_pose_t *start, bool tilt)
{
   hinge_set(set);
   memset(g, 0, sizeof(*g));
   video_xr_pose_identity(&g->rel);
   g->turn           = g->rel.orientation;
   g->rel.position.y = -0.96f;
   g->rel.position.z = -1.8f;
   g->base.y         = -0.992f;
   g->base.z         = -1.8f;
   g->width          = 1.28f;
   g->height         = 0.96f;
   g->slot           = 1;
   g->state          = VIDEO_XR_GRAB_HELD;
   g->tilt           = tilt;
   g->gap            = (unsigned char)(t_gap * 100.0f + 0.5f);
   video_xr_pose_identity(start);
   start->position.x = 1.0f;
}

/* Metres from screen 2's top edge middle to screen 1's bottom edge. */
static double hinge_off_line(const video_xr_pose_t *p)
{
   double dy, dz;
   video_xr_vec3_t m, w;
   m.x = 0.0f;
   m.y = 0.48f;
   m.z = 0.0f;
   video_xr_rotate(&p->orientation, &m, &w);
   dy  = (double)p->position.y + w.y + 0.48;
   dz  = (double)p->position.z + w.z + 1.8;
   return sqrt(dy * dy + dz * dz);
}

/* The hinge a grab holds is whole, its own edge in the hinge's range,
 * not only put in the plane by the flat snap's lines: screen 2, 2 cm
 * under screen 1's bottom edge, 2.5 cm in front and tipped 3 degrees
 * toward the user, snaps into the plane, a hinge at 0, its own edge
 * 5 cm off; flat, 1.5 cm in front, 1.5 cm off. The plane and the gap
 * count apart: 0.9 cm under and 2.9 cm in front, clicked flush, it is
 * whole; 4 cm over, pulled to 2.9, the snap holds no hinge there; at
 * Hinge Gap 1, 0.3 cm over, pulled to 0.4 under by the click's fade,
 * the hand's own gap is in the stop's whole range: whole. */
static void test_hinge_whole(void)
{
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, start, pose;
   video_xr_hinge_t h;
   hinge_grab(&set, &g, &start, false);
   video_xr_pose_identity(&aim);
   hinge_turn(&g.rel.orientation, 1.0, 0.0, 0.0, 3.0);
   g.turn           = g.rel.orientation;
   g.rel.position.y = -0.98f;
   g.rel.position.z = -1.775f;
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL);
   CHECK(video_xr_snap_hinge(&set, g.slot, 2u, g.width, g.height, 0.0f,
            1.0f, &pose, &h) & VIDEO_XR_SNAP_HINGE);
   CHECK(!(video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h)
            & VIDEO_XR_SNAP_HINGE));
   hinge_turn(&g.rel.orientation, 1.0, 0.0, 0.0, 0.0);
   g.turn           = g.rel.orientation;
   g.rel.position.z = -1.785f;
   CHECK(video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h)
         & VIDEO_XR_SNAP_HINGE);
   g.rel.position.y = -0.969f;
   g.rel.position.z = -1.771f;
   CHECK(video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h)
         & VIDEO_XR_SNAP_HINGE);
   g.rel.position.y = -0.92f;
   g.rel.position.z = -1.785f;
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL);
   CHECK(!(video_xr_snap_hinge(&set, g.slot, 2u, g.width, g.height, 0.0f,
            1.0f, &pose, &h) & VIDEO_XR_SNAP_HINGE));
   CHECK(NEAR(hinge_off_line(&pose), 0.029));
   CHECK(!(video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h)
            & VIDEO_XR_SNAP_HINGE));
   g.gap            = 1;
   g.rel.position.y = -0.957f;
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL);
   CHECK(video_xr_snap_hinge(&set, g.slot, 2u, g.width, g.height, 0.01f,
            1.0f, &pose, &h) & VIDEO_XR_SNAP_HINGE);
   CHECK(fabs(h.gap - 0.004) < 1e-5);
   CHECK(video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h)
         & VIDEO_XR_SNAP_HINGE);
}

/* The free stick down at full travel, polls 10 ms apart, swings the
 * hinged screen about the shared edge, never off it: flat until 5
 * degrees, then catching up, held at each 15 degree step from flat
 * over its 2 degree window, to 90, past which it lets go; carried or
 * tilting alike. */
static void test_hinge_swing(void)
{
   unsigned i, t, bits, axes, held;
   float rest[2];
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, start, pose;
   video_xr_hinge_t h;
   for (t = 0; t < 2; t++)
   {
      double worst = 0.0;
      hinge_grab(&set, &g, &start, t != 0);
      video_xr_pose_identity(&aim);
      rest[0] = 0.0f;
      rest[1] = 0.0f;
      bits    = video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h);
      CHECK((bits & VIDEO_XR_SNAP_HINGE) && h.axis == VIDEO_XR_STEP_TILT
            && fabs(h.angle) < 1e-3);
      video_xr_grab_put(&g, &aim, &pose);
      for (i = 1; i <= 106; i++)
      {
         bits = video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h);
         if (!(bits & VIDEO_XR_SNAP_HINGE))
            break;
         axes = video_xr_grab_turn_hinge(&g, &aim, &h, 0.0f, -1.0f, 0.01);
         held = video_xr_grab_step_hinge(&g, &aim, &h, 15.0f, axes, rest,
               &set, 2u, &start);
         bits = video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h);
         if ((bits & VIDEO_XR_SNAP_HINGE) && hinge_off_line(&pose) > worst)
            worst = hinge_off_line(&pose);
         if (i == 6)
            CHECK(fabs(h.angle - 0.8) < 0.05);
         if (i == 12)
            CHECK(fabs(h.angle - 10.8) < 0.05 && !held);
         if (i >= 15 && i <= 18)
            CHECK(fabs(h.angle - 15.0) < 0.01 && held == VIDEO_XR_STEP_TILT);
         if (i == 24)
            CHECK(fabs(h.angle - 21.6) < 0.05 && !held);
         if (i >= 32 && i <= 35)
            CHECK(fabs(h.angle - 30.0) < 0.01 && held == VIDEO_XR_STEP_TILT);
         if (i >= 98 && i <= 102)
            CHECK(fabs(h.angle - 90.0) < 0.01);
      }
      CHECK(i == 104 && worst < 1e-4);
   }
}

/* The other axis acts as before: the turn about the vertical through
 * screen 2's middle, hinged at 30 degrees, twists its edge off the
 * line's way, and past the snap's 5 degrees it is no longer hinged. */
static void test_hinge_other_axis(void)
{
   unsigned i, bits = 0;
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, start, pose;
   video_xr_hinge_t h;
   hinge_grab(&set, &g, &start, false);
   video_xr_pose_identity(&aim);
   video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h);
   video_xr_grab_put(&g, &aim, &pose);
   video_xr_grab_swing(&g, &aim, &h, 30.0);
   for (i = 0; i < 7; i++)
   {
      bits = video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h);
      if (i == 5)
         CHECK(bits & VIDEO_XR_SNAP_HINGE);
      if (bits & VIDEO_XR_SNAP_HINGE)
         CHECK(video_xr_grab_turn_hinge(&g, &aim, &h, 1.0f, 0.0f, 0.01)
               == VIDEO_XR_STEP_TURN);
   }
   CHECK(!(bits & VIDEO_XR_SNAP_HINGE));
}

/* A step's buzz arms by the angle from flat: inside 15's window not,
 * outside it, and back in by the stick's rest. A commit keeps the
 * screen's own angle from flat where the flat snap only fades (shown
 * 4, its own 7), the snap's where it is whole. */
static void test_hinge_step_put(void)
{
   float rest[2];
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, start, pose, under;
   video_xr_hinge_t h, at;
   hinge_grab(&set, &g, &start, false);
   video_xr_pose_identity(&aim);
   video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h);
   video_xr_grab_put(&g, &aim, &pose);
   rest[0] = 0.0f;
   rest[1] = 0.0f;
   video_xr_grab_swing(&g, &aim, &h, 15.0);
   CHECK(video_xr_grab_off_hinge(&g, &aim, &h, 15.0f, rest) == 0);
   video_xr_grab_swing(&g, &aim, &h, 3.0);
   CHECK(video_xr_grab_off_hinge(&g, &aim, &h, 15.0f, rest)
         == VIDEO_XR_STEP_TILT);
   rest[1] = -3.0f;
   CHECK(video_xr_grab_off_hinge(&g, &aim, &h, 15.0f, rest) == 0);
   CHECK(video_xr_grab_off_hinge(&g, &aim, &h, 0.0f, rest)
         == VIDEO_XR_STEP_TILT);
   hinge_grab(&set, &g, &start, false);
   video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &h);
   video_xr_grab_put(&g, &aim, &pose);
   video_xr_grab_swing(&g, &aim, &h, 7.0);
   CHECK(video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &pose, &at)
         & VIDEO_XR_SNAP_HINGE);
   CHECK(fabs(at.angle - 4.0) < 0.05);
   video_xr_grab_put_hinge(&g, &aim, &at, &pose, false);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &under, NULL, NULL);
   CHECK(fabs(video_xr_hinge_angle(&at, &under.orientation) - 7.0) < 0.01
         && hinge_off_line(&under) < 1e-4);
   video_xr_grab_put_hinge(&g, &aim, &at, &pose, true);
   video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &under, NULL, NULL);
   CHECK(fabs(video_xr_hinge_angle(&at, &under.orientation) - 4.0) < 0.01);
}

/* Neither commit moves the shown screen, so a stick push or let-go
 * never hops it: screen 2 hinged under screen 1 near flat, at 30
 * either way and near square, or beside it near square, rolled up to
 * 4.8 degrees, its edge 1.2 or 2.8 cm off the line and 0 to 8 cm along
 * it from the centres' level, carried or tilting, turned or not.
 * put_line keeps the angle from flat. */
static void test_hinge_put_still(void)
{
   static const double tips[]  = { 2.0, 7.0, -30.0, 30.0, -86.0, -88.0,
      -83.0 };
   static const double rolls[] = { 0.0, 2.0, 4.0, 4.8 };
   static const double xs[]    = { 0.0, 0.02, 0.035, 0.045, 0.055, 0.06,
      0.08 };
   static const double gaps[]  = { 0.012, 0.028 };
   unsigned t, ti, ri, xi, gi, k;
   unsigned n[2];
   double moved[2], turned[2], flat = 0.0;
   n[0] = n[1] = 0;
   moved[0] = moved[1] = turned[0] = turned[1] = 0.0;
   for (t = 0; t < 2; t++)
   for (ti = 0; ti < 7; ti++)
   for (ri = 0; ri < 4; ri++)
   for (xi = 0; xi < 7; xi++)
   for (gi = 0; gi < 2; gi++)
   {
      video_xr_quad_set_t set;
      video_xr_grab_t g, was;
      video_xr_pose_t aim, start, want, shown, now, under;
      video_xr_quat_t a, b, q;
      video_xr_hinge_t h;
      unsigned bits;
      double gp = gaps[gi] / sqrt(2.0);
      hinge_grab(&set, &g, &start, t != 0);
      hinge_turn(&b, 0.0, 0.0, 1.0, rolls[ri]);
      if (ti >= 5)
      {
         hinge_turn(&a, 0.0, 1.0, 0.0, tips[ti]);
         hinge_then(&a, &b, &q);
         hinge_place(&want, &q, -0.64, 0.0, 0.8 + gp, xs[xi], -1.8 + gp);
      }
      else
      {
         hinge_turn(&a, 1.0, 0.0, 0.0, tips[ti]);
         hinge_then(&b, &a, &q);
         hinge_place(&want, &q, 0.0, 0.48, xs[xi], -0.48 - gp, -1.8 + gp);
      }
      video_xr_pose_identity(&aim);
      hinge_turn(&aim.orientation, 0.0, 1.0, 0.0, 20.0);
      aim.position.x = 0.1f;
      aim.position.z = 0.2f;
      video_xr_grab_put(&g, &aim, &want);
      bits = video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &shown, &h);
      if (!(bits & VIDEO_XR_SNAP_HINGE))
         continue;
      n[(bits & VIDEO_XR_SNAP_TURN) != 0]++;
      was = g;
      for (k = 0; k < 2; k++)
      {
         g = was;
         if (k)
            video_xr_grab_put_hinge(&g, &aim, &h, &shown,
                  (bits & VIDEO_XR_SNAP_TURN) != 0);
         else
            video_xr_grab_put_line(&g, &aim, &h);
         CHECK(video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &now, NULL)
               & VIDEO_XR_SNAP_HINGE);
         if (pose_gap(&now, &shown) > moved[k])
            moved[k] = pose_gap(&now, &shown);
         if (turn_apart(&now.orientation, &shown.orientation) > turned[k])
            turned[k] = turn_apart(&now.orientation, &shown.orientation);
      }
      g = was;
      video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &under, NULL, NULL);
      video_xr_grab_put_line(&g, &aim, &h);
      video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &now, NULL, NULL);
      if (fabs(video_xr_hinge_angle(&h, &now.orientation)
               - video_xr_hinge_angle(&h, &under.orientation)) > flat)
         flat = fabs(video_xr_hinge_angle(&h, &now.orientation)
               - video_xr_hinge_angle(&h, &under.orientation));
   }
   CHECK(n[0] >= 200 && n[1] >= 80);
   if (     moved[0] > 1e-4 || turned[0] > 0.01 || moved[1] > 1e-4
         || turned[1] > 0.01 || flat > 0.01)
   {
      printf("  FAIL %s:%d: put_line moved %.4f m %.3f deg, put_hinge"
            " %.4f m %.3f deg, angle from flat %.3f\n", __FILE__,
            __LINE__, moved[0], turned[0], moved[1], turned[1], flat);
      failures++;
   }
}

/* Side by side, from 0.8 cm past it, the edges click to each Hinge Gap
 * exactly; 1.5 cm past 2 cm, half way, still hinged; square, the corner
 * leaves the gap between its plane and the other's edge. */
static void test_gap_click(void)
{
   static const float gaps[] = { 0.0f, 0.01f, 0.02f, 0.03f, 0.05f,
      0.10f };
   unsigned i;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_hinge_t h;
   snap_set(&set);
   for (i = 0; i < 6; i++)
   {
      snap_pose(&p, 1.30 + gaps[i] + 0.008, 0.01, -1.98, 0.0);
      CHECK(video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, gaps[i], 1.0f,
               &p, &h) == GAP_ALL);
      CHECK(vec_is(&p.position, 1.30 + gaps[i], 0.0, -2.0)
            && fabs(h.gap - gaps[i]) < 1e-5);
   }
   snap_pose(&p, 1.335, 0.0, -2.0, 0.0);
   CHECK(video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, 0.02f, 1.0f, &p,
            NULL) == (VIDEO_XR_SNAP_HINGE | VIDEO_XR_SNAP_TURN
            | VIDEO_XR_SNAP_Y | VIDEO_XR_SNAP_Z
            | VIDEO_XR_SNAP_PULLING(SNAP_ALL)
            | VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   CHECK(NEAR(p.position.x, 1.33));
   snap_pose(&p, 0.828, 0.01, -1.49, 90.0);
   CHECK(video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, 0.02f, 1.0f, &p,
            NULL) == GAP_ALL);
   CHECK(vec_is(&p.position, 0.82, 0.0, -1.5)
         && angle_near((float)snap_yaw(&p), 90.0f));
}

/* A hinge's line: along it where the held edge overlaps the target's,
 * midway across the gap; on the edges at gap 0; none without overlap.
 * Hand-built under screen 1 of hinge_set: a 1.28 m edge 6 cm along and
 * 2 cm under runs -0.58 to 0.70 at y -0.49, from either end as its way
 * turns; 1.2 m along it stops at screen 1's end, 1.5 m along there is
 * none; moved and turned with screen 1, the same ends moved. Snapped:
 * the 3DS pair folded 30, the square corner, and side by side 2 cm
 * apart. */
static void test_hinge_line(void)
{
   video_xr_quad_set_t set;
   video_xr_quad_t moved;
   video_xr_pose_t p, inv, at;
   video_xr_quat_t tip, off, q;
   video_xr_hinge_t h, m;
   video_xr_vec3_t a, b;
   t_gap = 0.0f;
   hinge_set(&set);
   memset(&h, 0, sizeof(h));
   h.point.x = 0.06f;
   h.point.y = -0.50f;
   h.point.z = -1.8f;
   h.dir.x   = 1.0f;
   h.flat.y  = -1.0f;
   h.gap     = 0.02f;
   h.edge    = 0;
   h.on      = 1;
   CHECK(video_xr_hinge_line(&h, 1.28f, 0.96f, &set.quads[0], &a, &b));
   CHECK(vec_is(&a, -0.58, -0.49, -1.8) && vec_is(&b, 0.70, -0.49, -1.8));
   video_xr_pose_identity(&p);
   hinge_turn(&p.orientation, 1.0, 2.0, 0.5, 40.0);
   p.position.x = 0.3f;
   p.position.y = 1.1f;
   p.position.z = 0.4f;
   moved        = set.quads[0];
   video_xr_compose(&p, &set.quads[0].pose, &moved.pose);
   m            = h;
   video_xr_pose_identity(&at);
   at.position  = h.point;
   video_xr_compose(&p, &at, &at);
   m.point      = at.position;
   video_xr_rotate(&p.orientation, &h.dir, &m.dir);
   video_xr_rotate(&p.orientation, &h.flat, &m.flat);
   CHECK(video_xr_hinge_line(&m, 1.28f, 0.96f, &moved, &a, &b));
   video_xr_pose_invert(&p, &inv);
   at.position = a;
   video_xr_compose(&inv, &at, &at);
   CHECK(vec_is(&at.position, -0.58, -0.49, -1.8));
   at.position = b;
   video_xr_compose(&inv, &at, &at);
   CHECK(vec_is(&at.position, 0.70, -0.49, -1.8));
   h.dir.x = -1.0f;
   CHECK(video_xr_hinge_line(&h, 1.28f, 0.96f, &set.quads[0], &a, &b));
   CHECK(vec_is(&a, 0.70, -0.49, -1.8) && vec_is(&b, -0.58, -0.49, -1.8));
   h.dir.x   = 1.0f;
   h.point.x = 1.2f;
   CHECK(video_xr_hinge_line(&h, 1.28f, 0.96f, &set.quads[0], &a, &b));
   CHECK(vec_is(&a, 0.56, -0.49, -1.8) && vec_is(&b, 0.8, -0.49, -1.8));
   h.point.x = 1.5f;
   CHECK(!video_xr_hinge_line(&h, 1.28f, 0.96f, &set.quads[0], &a, &b));
   h.point.x = 0.06f;
   h.point.y = -0.48f;
   h.gap     = 0.0f;
   CHECK(video_xr_hinge_line(&h, 1.28f, 0.96f, &set.quads[0], &a, &b));
   CHECK(vec_is(&a, -0.58, -0.48, -1.8) && vec_is(&b, 0.70, -0.48, -1.8));
   /* The 3DS pair as test_hinge_3ds takes it, folded 30, touching. */
   hinge_turn(&tip, 1.0, 0.0, 0.0, -30.0);
   hinge_turn(&off, 0.0, 1.0, 0.0, 3.0);
   hinge_then(&off, &tip, &q);
   hinge_place(&p, &q, 0.0, 0.48, 0.0, -0.488, -1.787);
   CHECK(hinge_snap(&set, &p, 1.0f, &h) & VIDEO_XR_SNAP_HINGE);
   CHECK(video_xr_hinge_line(&h, 1.28f, 0.96f, &set.quads[0], &a, &b));
   CHECK(vec_is(&a, -0.64, -0.48, -1.8) && vec_is(&b, 0.64, -0.48, -1.8));
   /* The square corner of test_hinge_flush_corner, up screen 1's right
    * edge. */
   hinge_turn(&q, 0.0, 1.0, 0.0, 88.0);
   hinge_place(&p, &q, 0.64, 0.0, 0.805, 0.01, -1.81);
   CHECK(hinge_snap(&set, &p, 1.0f, &h) & VIDEO_XR_SNAP_HINGE);
   CHECK(h.edge == 2
         && video_xr_hinge_line(&h, 1.28f, 0.96f, &set.quads[0], &a, &b));
   CHECK(vec_is(&a, 0.8, -0.48, -1.8) && vec_is(&b, 0.8, 0.48, -1.8));
   /* Side by side at Hinge Gap 2 cm, as test_gap_click clicks it: up
    * the gap's middle. */
   snap_set(&set);
   snap_pose(&p, 1.30 + 0.02 + 0.008, 0.01, -1.98, 0.0);
   CHECK(video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, 0.02f, 1.0f, &p, &h)
         & VIDEO_XR_SNAP_HINGE);
   CHECK(video_xr_hinge_line(&h, 1.0f, 0.6f, &set.quads[0], &a, &b));
   CHECK(vec_is(&a, 0.81, -0.3, -2.0) && vec_is(&b, 0.81, 0.3, -2.0));
}

/* Metres from v to x, y, z. */
static double pt_off(const video_xr_vec3_t *v, double x, double y,
      double z)
{
   double dx = v->x - x;
   double dy = v->y - y;
   double dz = v->z - z;
   return sqrt(dx * dx + dy * dy + dz * dz);
}

/* Screen 2 (1.28 x 0.96) at p under screen 1 of set, hinged by k as
 * kept: placed on its edges, the middle of its line (false without
 * one), and its gap. */
static bool place_mid(const video_xr_quad_set_t *set,
      const video_xr_hinge_t *k, const video_xr_pose_t *p, bool over,
      video_xr_vec3_t *mid, float *gap)
{
   video_xr_vec3_t a, b;
   video_xr_hinge_t h = *k;
   video_xr_hinge_place(&h, p, 1.28f, 0.96f, &set->quads[0], over);
   if (!video_xr_hinge_line(&h, 1.28f, 0.96f, &set->quads[0], &a, &b))
      return false;
   mid->x = (a.x + b.x) * 0.5f;
   mid->y = (a.y + b.y) * 0.5f;
   mid->z = (a.z + b.z) * 0.5f;
   *gap   = h.gap;
   return true;
}

/* A kept hinge's line on the real edges: whole, where the snap's own
 * hinge draws it, the 3DS pair folded 30 at gaps 0 and 2; 1 cm and
 * 0 apart, midway across; 1 to 3 cm crossed, on the held edge drawn
 * over the other, else on the other's; 1 cm under and 3.5 cm in front,
 * midway in depth too; turned 3 degrees in the plane, its way midway
 * and its middle midway between them. */
static void test_hinge_place(void)
{
   static const float gaps[] = { 0.0f, 0.02f };
   static const double ys[] = { -0.488, -0.508 };
   unsigned i;
   float gap;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_quat_t tip;
   video_xr_hinge_t k, w;
   video_xr_vec3_t a, b, c, m;
   hinge_set(&set);
   hinge_turn(&tip, 1.0, 0.0, 0.0, -30.0);
   for (i = 0; i < 2; i++)
   {
      hinge_place(&p, &tip, 0.0, 0.48, 0.06, ys[i], -1.787);
      CHECK(video_xr_snap_hinge(&set, 1, 0, 1.28f, 0.96f, gaps[i], 1.0f,
               &p, &k) & VIDEO_XR_SNAP_HINGE);
      CHECK(video_xr_hinge_line(&k, 1.28f, 0.96f, &set.quads[0], &a, &b));
      w = k;
      video_xr_hinge_place(&w, &p, 1.28f, 0.96f, &set.quads[0], true);
      CHECK(video_xr_hinge_line(&w, 1.28f, 0.96f, &set.quads[0], &c, &m));
      CHECK(     pt_off(&c, a.x, a.y, a.z) < 1e-6
            && pt_off(&m, b.x, b.y, b.z) < 1e-6
            && fabs(w.gap - k.gap) < 1e-6);
      CHECK(place_mid(&set, &k, &p, true, &m, &gap)
            && pt_off(&m, 0.06, -0.48 - gaps[i] * 0.5, -1.8) < 1e-5);
   }
   memset(&k, 0, sizeof(k));
   k.point.y = -0.50f;
   k.point.z = -1.8f;
   k.dir.x   = 1.0f;
   k.flat.y  = -1.0f;
   k.gap     = 0.02f;
   k.on      = 1;
   video_xr_pose_identity(&p);
   for (i = 0; i < 2; i++)
   {
      p.position.x = -0.2f;
      p.position.y = (float)(-0.96 - 0.01 * i);
      p.position.z = -1.8f;
      CHECK(place_mid(&set, &k, &p, i == 0, &m, &gap)
            && pt_off(&m, -0.18, -0.48 - 0.005 * i, -1.8) < 1e-5
            && fabs(gap - 0.01 * i) < 1e-5);
   }
   for (i = 1; i <= 3; i++)
   {
      p.position.y = (float)(-0.96 + 0.01 * i);
      CHECK(place_mid(&set, &k, &p, true, &m, &gap)
            && pt_off(&m, -0.18, -0.48 + 0.01 * i, -1.8) < 1e-5
            && gap < 1e-5);
      CHECK(place_mid(&set, &k, &p, false, &m, &gap)
            && pt_off(&m, -0.18, -0.48, -1.8) < 1e-5
            && gap < 1e-5);
   }
   hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.49, -1.765);
   CHECK(place_mid(&set, &k, &p, true, &m, &gap)
         && pt_off(&m, 0.0, -0.485, -1.7825) < 1e-5
         && fabs(gap - sqrt(0.01 * 0.01 + 0.035 * 0.035)) < 1e-5);
   hinge_turn(&p.orientation, 0.0, 0.0, 1.0, 3.0);
   hinge_place(&p, &p.orientation, 0.0, 0.48, 0.0, -0.49, -1.8);
   w = k;
   video_xr_hinge_place(&w, &p, 1.28f, 0.96f, &set.quads[0], true);
   CHECK(     vec_is(&w.dir, cos(1.5 * M_PI / 180.0),
            sin(1.5 * M_PI / 180.0), 0.0)
         && place_mid(&set, &k, &p, true, &m, &gap));
   CHECK(fabs(m.y + 0.485) < 5e-4 && fabs(m.z + 1.8) < 1e-5);
}

/* Elsewhere from touching to 10 cm the gap stays where the hand puts
 * it, hinged: side by side at 4 and 8 cm, the 3DS pair at 30 degrees
 * at 8; 2.8 cm under clicks to 2, 3.5 to 3 (half way); an edge 2 cm
 * over the other's, overlapping, does not hinge. */
static void test_gap_free(void)
{
   unsigned i, bits;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_quat_t tip;
   video_xr_hinge_t h;
   snap_set(&set);
   for (i = 0; i < 2; i++)
   {
      double x = i ? 1.38 : 1.34;
      snap_pose(&p, x, 0.01, -1.98, 0.0);
      bits = video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, 0.02f, 1.0f, &p,
            &h);
      CHECK((bits & VIDEO_XR_SNAP_HINGE) && !(bits & VIDEO_XR_SNAP_X)
            && !(bits & VIDEO_XR_SNAP_GAP));
      CHECK(vec_is(&p.position, x, 0.0, -2.0)
            && fabs(h.gap - (x - 1.30)) < 1e-5);
   }
   hinge_set(&set);
   hinge_turn(&tip, 1.0, 0.0, 0.0, -30.0);
   t_gap = 0.02f;
   hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.508, -1.787);
   bits = hinge_snap(&set, &p, 1.0f, &h);
   CHECK((bits & VIDEO_XR_SNAP_GAP) && (bits & VIDEO_XR_SNAP_HINGE)
         && (bits & VIDEO_XR_SNAP_ALL) == HINGE_LINES);
   CHECK(vec_is(&p.position, 0.0, -0.915692, -1.56)
         && fabs(h.gap - 0.02) < 1e-5 && fabs(h.angle + 30.0) < 0.01);
   hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.515, -1.787);
   bits = hinge_snap(&set, &p, 1.0f, &h);
   CHECK((bits & VIDEO_XR_SNAP_HINGE) && !(bits & VIDEO_XR_SNAP_GAP)
         && (bits & VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP)));
   CHECK(vec_is(&p.position, 0.0, -0.925692, -1.56)
         && fabs(h.gap - 0.03) < 1e-5);
   hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.56, -1.787);
   CHECK(hinge_snap(&set, &p, 1.0f, &h) == (VIDEO_XR_SNAP_HINGE
            | HINGE_LINES | VIDEO_XR_SNAP_PULLING(HINGE_LINES)));
   CHECK(vec_is(&p.position, 0.0, -0.975692, -1.56)
         && fabs(h.gap - 0.08) < 1e-5);
   hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.445, -1.787);
   CHECK(!(hinge_snap(&set, &p, 1.0f, NULL) & VIDEO_XR_SNAP_HINGE));
   t_gap = 0.0f;
}

/* Carried straight out from the other's edge to 15 cm, at 30 and at
 * 7.5 degrees from flat, a 2 cm gap set: hinged to 10 cm exactly, then
 * fading, with no step over the carry sweep's 1.5 mm. */
static void test_gap_fade_out(void)
{
   unsigned i, j, last_hinged;
   video_xr_quad_set_t set;
   video_xr_pose_t p, last;
   video_xr_quat_t tip;
   hinge_set(&set);
   t_gap = 0.02f;
   for (j = 0; j < 2; j++)
   {
      last_hinged = 0;
      hinge_turn(&tip, 1.0, 0.0, 0.0, j ? -7.5 : -30.0);
      for (i = 0; i <= 300; i++)
      {
         hinge_place(&p, &tip, 0.0, 0.48, 0.05, -0.48 - 0.0005 * i,
               -1.8);
         if (hinge_snap(&set, &p, 1.0f, NULL) & VIDEO_XR_SNAP_HINGE)
            last_hinged = i;
         if (i)
            CHECK(pose_gap(&p, &last) <= 0.0015);
         last = p;
      }
      CHECK(last_hinged == 200);
   }
   t_gap = 0.0f;
}

/* Layout k's held screen pushed c metres past touching its target's
 * edge (c below 0 apart), 2 cm out of plane: flat side by side, the
 * 3DS pair folded 30, square at the corner, or the 3DS pair flat;
 * snapped with gap set. Returns the snaps, in *across how far past the
 * edge it shows. */
static unsigned stop_snap(unsigned k, double c, float gap,
      double *across)
{
   unsigned bits;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_quat_t tip;
   video_xr_vec3_t m, w;
   if (k == 1 || k == 3)
   {
      hinge_set(&set);
      hinge_turn(&tip, 1.0, 0.0, 0.0, (k == 1) ? -30.0 : 0.0);
      hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.48 + c, -1.787);
      bits = video_xr_snap_hinge(&set, 1, 0, 1.28f, 0.96f, gap, 1.0f,
            &p, NULL);
      m.x  = 0.0f;
      m.y  = 0.48f;
      m.z  = 0.0f;
      video_xr_rotate(&p.orientation, &m, &w);
      *across = -0.48 - (p.position.y + w.y);
      return bits;
   }
   snap_set(&set);
   if (k == 0)
      snap_pose(&p, 1.30 - c, 0.01, -1.98, 0.0);
   else
      snap_pose(&p, 0.80 - c, 0.01, -1.49, 90.0);
   bits = video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, gap, 1.0f, &p,
         NULL);
   *across = p.position.x - ((k == 0) ? 1.30 : 0.80);
   return bits;
}

/* Hinged, pushed past touching it stops there, at every gap set and
 * flat, folded or at a corner: whole 0.5 to 2.9 cm past, partly 4 cm
 * past, none from 5.5 (6 at Hinge Gap 1, test_gap_stop_one); 3 mm apart
 * it is as the gap leaves it (the click at 0, its fade at 2, free at
 * 10). Swept from 1 cm apart to 10 past, nothing hops, the click
 * handing over to the stop, which lets go at 4 times the hand at
 * most. */
static void test_gap_stop(void)
{
   static const float gaps[] = { 0.0f, 0.01f, 0.02f, 0.10f };
   static const double apart[] = { 0.0, -1.0, 0.006, 0.003 };
   static const double whole[] = { 0.005, 0.01, 0.02, 0.029 };
   unsigned k, i, j;
   for (k = 0; k < 3; k++)
   for (i = 0; i < 4; i++)
   {
      double a, last = 0.0;
      unsigned bits;
      for (j = 0; j < 4; j++)
      {
         bits = stop_snap(k, whole[j], gaps[i], &a);
         CHECK((bits & VIDEO_XR_SNAP_HINGE) && fabs(a) < 1e-5);
      }
      bits = stop_snap(k, 0.04, gaps[i], &a);
      CHECK(!(bits & VIDEO_XR_SNAP_HINGE) && a < -0.001 && a > -0.0399);
      for (j = 0; j < 2; j++)
      {
         double c = ((i == 1) ? 0.06 : 0.055) + 0.005 * j;
         stop_snap(k, c, gaps[i], &a);
         CHECK(fabs(a + c) < 1e-5);
      }
      if (apart[i] >= 0.0)
      {
         bits = stop_snap(k, -0.003, gaps[i], &a);
         CHECK((bits & VIDEO_XR_SNAP_HINGE) && fabs(a - apart[i]) < 1e-5);
      }
      for (j = 0; j <= 220; j++)
      {
         stop_snap(k, -0.01 + 0.0005 * j, gaps[i], &a);
         if (j)
            CHECK(fabs(a - last) <= 0.002);
         last = a;
      }
   }
}

/* Hinge Gap 1: its click's fade shows touching 5 mm past, so the stop
 * holds it there from 5 to 35 mm past, flat, folded or at a corner,
 * partly at 4.5 cm and not at all from 6; shaken 3 mm about 1 and
 * 3 cm past at 8 Hz, it shows touching on every poll. */
static void test_gap_stop_one(void)
{
   static const double past[] = { 0.005, 0.01, 0.014, 0.025, 0.034 };
   unsigned k, j;
   for (k = 0; k < 3; k++)
   {
      double a;
      double worst = 0.0;
      for (j = 0; j < 5; j++)
      {
         unsigned bits = stop_snap(k, past[j], 0.01f, &a);
         CHECK((bits & VIDEO_XR_SNAP_HINGE) && fabs(a) < 1e-5);
      }
      stop_snap(k, 0.045, 0.01f, &a);
      CHECK(a < -0.001 && a > -0.0449);
      for (j = 0; j < 900; j++)
      {
         stop_snap(k, ((j < 450) ? 0.01 : 0.03) + 0.003 * sin(j * 0.56),
               0.01f, &a);
         if (fabs(a) > worst)
            worst = fabs(a);
      }
      CHECK(worst < 1e-5);
   }
}

/* Flat, at a corner, stacked and folded 30, at Gaps 3, 5 and 10 (no
 * click reaches touching): 3 mm apart the stop's tick bits count as
 * pulling, 8 mm apart they do not, and past touching the stop holds. */
static void test_gap_stop_arm(void)
{
   static const unsigned layouts[] = { 0, 2, 3, 1 };
   static const unsigned axes[]    = { VIDEO_XR_SNAP_X, VIDEO_XR_SNAP_Z,
      VIDEO_XR_SNAP_Y, VIDEO_XR_SNAP_GAP };
   static const float gaps[]       = { 0.03f, 0.05f, 0.10f };
   unsigned k, i;
   for (k = 0; k < 4; k++)
   for (i = 0; i < 3; i++)
   {
      double a;
      unsigned x    = axes[k] | VIDEO_XR_SNAP_GAP;
      unsigned bits = stop_snap(layouts[k], -0.003, gaps[i], &a);
      CHECK(     (bits & VIDEO_XR_SNAP_PULLING(x)) == VIDEO_XR_SNAP_PULLING(x)
            && !(bits & x));
      bits = stop_snap(layouts[k], -0.008, gaps[i], &a);
      CHECK(!(bits & VIDEO_XR_SNAP_PULLING(x)));
      bits = stop_snap(layouts[k], 0.002, gaps[i], &a);
      CHECK((bits & x) == x && fabs(a) < 1e-5);
   }
}

/* Flat side by side (corner 0) or square at the corner, the held screen
 * turned yaw degrees further about the vertical, the middle of its edge
 * c metres past touching the target's; snapped with gap set. */
static unsigned stop_bits_turned(unsigned corner, double yaw, double c,
      float gap)
{
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_quat_t q;
   snap_set(&set);
   hinge_turn(&q, 0.0, 1.0, 0.0, (corner ? 90.0 : 0.0) + yaw);
   hinge_place(&p, &q, corner ? 0.5 : -0.5, 0.0, 0.80 - c, 0.01, -1.99);
   return video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, gap, 1.0f, &p,
         NULL);
}

/* Pushed 0.5 or 5 mm past touching, flat or at a corner, the hand's turn
 * shaken 0.2 degrees at 8 Hz about the turn's whole edge, 5 degrees
 * either way, at Hinge Gap 2, 3 and 10 cm: on every poll both of the
 * stop's tick bits, its axis's and the gap's, count as pulling, so
 * neither re-arms and the tick never repeats. */
static void test_gap_stop_arm_turned(void)
{
   static const float gaps[]   = { 0.02f, 0.03f, 0.10f };
   static const double past[]  = { 0.0005, 0.005 };
   unsigned corner, i, j, k, side;
   for (corner = 0; corner < 2; corner++)
   for (side = 0; side < 2; side++)
   for (i = 0; i < 3; i++)
   for (j = 0; j < 2; j++)
   {
      unsigned want = VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_GAP
            | (corner ? VIDEO_XR_SNAP_Z : VIDEO_XR_SNAP_X));
      unsigned off  = 0;
      for (k = 0; k < 450; k++)
      {
         double yaw    = (side ? -5.0 : 5.0) * (1.0 + 0.04 * sin(k * 0.56));
         unsigned bits = stop_bits_turned(corner, yaw, past[j], gaps[i]);
         if ((bits & want) != want)
            off++;
      }
      if (off)
         printf("  %s %+.0f deg, gap %.0f cm, %.1f mm past: %u polls "
               "re-arm\n", corner ? "corner" : "flat", side ? -5.0 : 5.0,
               gaps[i] * 100.0, past[j] * 1e3, off);
      CHECK(off == 0);
   }
}

/* At the corner, the held screen turned yaw degrees past square, its
 * edge's middle c metres past touching the target's; snapped with gap
 * set. Returns how far past that middle shows. */
static double turned_stop_snap(double yaw, double c, float gap)
{
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_quat_t q;
   video_xr_vec3_t m, w;
   snap_set(&set);
   hinge_turn(&q, 0.0, 1.0, 0.0, 90.0 + yaw);
   hinge_place(&p, &q, 0.5, 0.0, 0.80 - c, 0.01, -1.99);
   video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, gap, 1.0f, &p, NULL);
   m.x = 0.5f;
   m.y = 0.0f;
   m.z = 0.0f;
   video_xr_rotate(&p.orientation, &m, &w);
   return 0.80 - (p.position.x + w.x);
}

/* A corner held up to 5 degrees off square either way, at each gap
 * set, pushed from touching to 4 cm past in 0.1 mm steps: the stop
 * lets go with the hinge, no step over twice the larger beside it (a
 * hop), nor over 1.2 mm, the turn's own swing; up to 3 degrees past
 * square, where the turn swings the edge back toward touching, none
 * over the 0.4 mm the stop's own fade takes. */
static void test_gap_stop_turned(void)
{
   static const float gaps[] = { 0.0f, 0.01f, 0.02f, 0.05f, 0.10f };
   unsigned y, i, j;
   for (y = 0; y <= 20; y++)
   for (i = 0; i < 5; i++)
   {
      double yaw    = -5.0 + 0.5 * y;
      double most   = (yaw >= 0.0 && yaw <= 3.0) ? 0.0004 : 0.0012;
      double last   = turned_stop_snap(yaw, 0.0, gaps[i]);
      double before = 0.0;
      double prev   = 0.0;
      double worst  = 0.0;
      double hop    = 0.0;
      for (j = 1; j <= 401; j++)
      {
         double step = 0.0;
         if (j <= 400)
         {
            double a = turned_stop_snap(yaw, 0.0001 * j, gaps[i]);
            step     = fabs(a - last);
            last     = a;
         }
         if (j >= 2)
         {
            double side = (before > step) ? before : step;
            if (prev - 2.0 * side > hop)
               hop = prev - 2.0 * side;
         }
         if (step > worst)
            worst = step;
         before = prev;
         prev   = step;
      }
      if (hop > 0.00005 || worst > most)
         printf("  yaw %+.1f gap %.0f cm: hop %.2f mm, step %.2f mm\n",
               yaw, gaps[i] * 100.0, hop * 1e3, worst * 1e3);
      CHECK(hop <= 0.00005 && worst <= most);
   }
}

/* Swung by the stick from flat to square, carried or tilting, a 2 cm
 * gap set: taken 2.6 cm under it clicks to 2 and stays; 3.5 under, in
 * the click's fade, it shows 3 and stays; 6 under, free, it stays 6. */
static void test_gap_swing(void)
{
   static const double under[] = { 0.026, 0.035, 0.06 };
   static const double shown[] = { 0.02, 0.03, 0.06 };
   unsigned i, k, u;
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, start, want, now;
   video_xr_quat_t tip;
   video_xr_hinge_t h;
   t_gap = 0.02f;
   for (u = 0; u < 3; u++)
   for (k = 0; k < 2; k++)
   {
      float angle = 0.0f;
      hinge_grab(&set, &g, &start, k != 0);
      hinge_turn(&tip, 1.0, 0.0, 0.0, -0.5);
      hinge_place(&want, &tip, 0.0, 0.48, 0.0, -0.48 - under[u], -1.8);
      video_xr_pose_identity(&aim);
      video_xr_grab_put(&g, &aim, &want);
      for (i = 0; i < 89; i++)
      {
         CHECK(video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &now, &h)
               & VIDEO_XR_SNAP_HINGE);
         CHECK(fabs(h.gap - shown[u]) < 1e-5);
         if (!i)
            video_xr_grab_put_line(&g, &aim, &h);
         angle = h.angle;
         video_xr_grab_turn_hinge(&g, &aim, &h, 0.0f, 1.0f, 1.0 / 90.0);
      }
      CHECK(angle <= -88.0f);
   }
   t_gap = 0.0f;
}

/* Screen 1 turned th degrees about its right edge at x 0.8, and the
 * small screen parallel 10 cm right of a screen held 1 cm plus the gap
 * g right of screen 1, in quad order o. */
static void gap_nearer_set(video_xr_quad_set_t *set, double th, double g,
      unsigned o)
{
   double r = th * M_PI / 180.0;
   video_xr_quad_t *q;
   memset(set, 0, sizeof(*set));
   if (o)
      snap_add(set, 2, 2.16 + g, 0.0);
   q = &set->quads[set->num_quads++];
   snap_pose(&q->pose, 0.8 - 0.8 * cos(r), 0.0, -2.0 + 0.8 * sin(r), th);
   q->width  = 1.6f;
   q->height = 0.9f;
   q->kind   = VIDEO_XR_QUAD_SCREEN;
   if (!o)
      snap_add(set, 2, 2.16 + g, 0.0);
}

/* Of two screens, the nearer edge wins and clicks, in either order:
 * tipped 30 degrees, 1 cm under A, level or turned 3, and 9 under C (a
 * 2 cm gap set, 2.8 and 9); flat, 1 cm right of screen 1 and 9 cm left
 * of a small screen, either a little off parallel, or screen 1 turned
 * up to 4.9 degrees, which the held screen turns to. Nearer is from
 * where the click puts the edge. */
static void test_gap_nearer(void)
{
   /* Screen 1's yaw and the small screen's. */
   static const double noise[][2] = {
      { 0.0, 0.0 }, { 0.1, 0.0 }, { 0.0, 0.1 }, { 0.001, 0.0 }
   };
   static const double turned[] = { 0.6, 1.0, 3.0, 4.9 };
   unsigned i, j, o, n, bits;
   video_xr_quad_set_t set, one;
   video_xr_pose_t p;
   video_xr_quat_t tip;
   video_xr_hinge_t h;
   hinge_turn(&tip, 1.0, 0.0, 0.0, -30.0);
   for (i = 0; i < 2; i++)
   for (o = 0; o < 2; o++)
   {
      double ya = i ? -0.472 : -0.49;
      memset(&set, 0, sizeof(set));
      for (j = 0; j < 2; j++)
      {
         bool a = j == o;
         snap_add(&set, a ? 0 : 2, a ? -0.32 : 0.32, 0.0);
         set.quads[j].pose.position.y = (float)((a ? ya : -0.41) + 0.2);
         set.quads[j].pose.position.z = -1.8f;
         set.quads[j].width           = 0.6f;
      }
      t_gap = i ? 0.02f : 0.0f;
      hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.5, -1.8);
      bits = hinge_snap(&set, &p, 1.0f, &h);
      CHECK((bits & VIDEO_XR_SNAP_HINGE) && (bits & VIDEO_XR_SNAP_GAP));
      CHECK(NEAR(h.point.y, ya - t_gap) && fabs(h.gap - t_gap) < 1e-5);
      /* With screen 1 turned 3 degrees, inside the whole snap, its edge
       * still wins over the parallel one. */
      hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.5, -1.8);
      snap_pose(&set.quads[o].pose, -0.32, ya + 0.2, -1.8, 3.0);
      bits = hinge_snap(&set, &p, 1.0f, &h);
      CHECK((bits & VIDEO_XR_SNAP_HINGE) && (bits & VIDEO_XR_SNAP_GAP)
            && h.slot == 0);
   }
   t_gap = 0.0f;
   snap_set(&one);
   for (n = 0; n < 4; n++)
   for (o = 0; o < 2; o++)
   {
      double r = noise[n][0] * M_PI / 180.0;
      snap_pose(&one.quads[0].pose, 0.0, 0.0, -2.0, noise[n][0]);
      memset(&set, 0, sizeof(set));
      if (!o)
         set.quads[set.num_quads++] = one.quads[0];
      snap_add(&set, 2, 2.15, noise[n][1]);
      if (o)
         set.quads[set.num_quads++] = one.quads[0];
      snap_pose(&p, 1.31, 0.0, -2.0, 0.0);
      CHECK(video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, 0.0f, 1.0f, &p,
               &h) == GAP_ALL);
      CHECK(h.edge == 3 && h.slot == 0 && h.on == 2
            && vec_is(&p.position, 1.30 * cos(r), 0.0,
               -2.0 - 1.30 * sin(r)));
   }
   /* Screen 1 turned up to 4.9 degrees about its right edge, the small
    * screen 10 cm out and parallel: both are in the turn's whole snap,
    * so the nearer wins and the held screen turns to it, in either
    * order, at gaps 0 and 2. */
   for (n = 0; n < 4; n++)
   for (i = 0; i < 2; i++)
   for (o = 0; o < 2; o++)
   {
      double g = i ? 0.02 : 0.0;
      double r = turned[n] * M_PI / 180.0;
      gap_nearer_set(&set, turned[n], g, o);
      snap_pose(&p, 1.31 + g, 0.0, -2.0, 0.0);
      CHECK(video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, (float)g, 1.0f,
               &p, &h) == GAP_ALL);
      CHECK(h.edge == 3 && h.slot == 0 && h.on == 2
            && fabs(snap_yaw(&p) - turned[n]) < 1e-3
            && vec_is(&p.position, 0.8 + (0.5 + g) * cos(r), 0.0,
               -2.0 - (0.5 + g) * sin(r)));
   }
   /* Nearer is the real distance from where the click puts the edge:
    * with 2 cm set, screen 1 touching is 2 cm off it and the small
    * screen 2.5 cm out only 0.5, so the turn and the hinge both go to
    * the small one, in either order. */
   snap_pose(&one.quads[0].pose, 0.0, 0.0, -2.0, 0.0);
   for (o = 0; o < 2; o++)
   {
      memset(&set, 0, sizeof(set));
      if (!o)
         set.quads[set.num_quads++] = one.quads[0];
      snap_add(&set, 2, 2.075, 0.0);
      if (o)
         set.quads[set.num_quads++] = one.quads[0];
      snap_pose(&p, 1.30, 0.0, -2.0, 0.0);
      CHECK(video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, 0.02f, 1.0f, &p,
               &h) == GAP_ALL);
      CHECK(h.edge == 2 && h.slot == 2 && h.on == 3
            && vec_is(&p.position, 1.305, 0.0, -2.0));
   }
}

/* gap_nearer_set's screen 1 turned from 0 to 6 degrees, in either order
 * and at gaps 0 and 2: through the turn's whole snap the held screen
 * follows it, hinged with the gap, no step more than twice screen 1's;
 * past it the small screen's whole turn wins, a switch as big as the
 * turn between the two. Fading in at 3 degrees, it turns toward
 * screen 1 alone, smoothly. */
static void test_gap_nearer_sweep(void)
{
   unsigned i, j, o, bits;
   video_xr_quad_set_t set;
   video_xr_pose_t p, last;
   video_xr_hinge_t h;
   for (j = 0; j < 2; j++)
   for (o = 0; o < 2; o++)
   {
      double g = j ? 0.02 : 0.0;
      for (i = 0; i < 1200; i++)
      {
         double th = 0.0025 + 0.005 * i;
         gap_nearer_set(&set, th, g, o);
         snap_pose(&p, 1.31 + g, 0.0, -2.0, 0.0);
         bits = video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, (float)g, 1.0f,
               &p, &h);
         if (th < 5.0)
            CHECK(bits == GAP_ALL && h.slot == 0
                  && fabs(snap_yaw(&p) - th) < 1e-3);
         else
            CHECK(fabs(snap_yaw(&p)) < 1e-3);
         if (i && (th < 5.0 || th > 5.005))
            CHECK(fabs(snap_yaw(&p) - snap_yaw(&last)) <= 0.0101
                  && pose_gap(&p, &last) <= 0.0005);
         else if (i)
            CHECK(fabs(snap_yaw(&p) - snap_yaw(&last)) <= 5.01);
         last = p;
      }
      for (i = 0; i <= 100; i++)
      {
         gap_nearer_set(&set, 3.0, g, o);
         snap_pose(&p, 1.31 + g, 0.0, -2.0, 0.0);
         video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, (float)g, 0.01f * i,
               &p, &h);
         CHECK(fabs(snap_yaw(&p) - 0.03 * i) < 1e-3);
         if (i)
            CHECK(pose_gap(&p, &last) <= 0.001);
         last = p;
      }
   }
}

/* Hinged to A at a free 4 to 9 cm (a 2 cm gap set) or touching (5 or
 * 10 set), with a larger screen stacked 2 to 9 cm behind and turned 3
 * or 4.9 degrees, inside the turn's whole snap, or one turned 3 corner
 * to corner 1 cm right, its edges off the held one's ends: A's edge is
 * the nearer, so the held screen neither turns nor lets go of A, in
 * either order. */
static void test_gap_nearer_stacked(void)
{
   static const double gf[][2] = {
      { 0.02, 0.04 }, { 0.02, 0.08 }, { 0.02, 0.09 }, { 0.05, 0.0 },
      { 0.10, 0.0 }
   };
   static const double yaw[] = { 3.0, -3.0, 4.9 };
   unsigned i, j, k, o;
   video_xr_quad_set_t set;
   video_xr_pose_t p;
   video_xr_hinge_t h;
   for (i = 0; i < 5; i++)
   for (j = 0; j < 3; j++)
   for (o = 0; o < 2; o++)
   for (k = 0; k <= 70; k++)
   {
      memset(&set, 0, sizeof(set));
      if (o)
         snap_add(&set, 2, 0.05, yaw[j]);
      snap_add(&set, 0, -0.5 - gf[i][1], 0.0);
      if (!o)
         snap_add(&set, 2, 0.05, yaw[j]);
      set.quads[!o].pose.position.z = (float)(-2.02 - 0.001 * k);
      set.quads[!o].width           = 0.6f;
      set.quads[!o].height          = 0.5f;
      snap_pose(&p, 0.0, 0.0, -2.0, 0.0);
      CHECK((video_xr_snap_hinge(&set, 1, 0, 0.5f, 0.4f, (float)gf[i][0],
                  1.0f, &p, &h) & VIDEO_XR_SNAP_HINGE)
            && h.slot == 0 && fabs(h.gap - gf[i][1]) < 1e-5);
      CHECK(fabs(snap_yaw(&p)) < 1e-3 && vec_is(&p.position, 0.0, 0.0,
               -2.0));
   }
   /* The corner's x line clicks the held screen 1 cm left to the gap. */
   for (i = 0; i < 4; i++)
   for (j = 0; j < 2; j++)
   for (o = 0; o < 2; o++)
   {
      double fr = (i & 1) ? 0.09 : 0.06;
      memset(&set, 0, sizeof(set));
      if (o)
         snap_add(&set, 2, 0.51, j ? 3.0 : -3.0);
      snap_add(&set, 0, -0.5 - fr, 0.0);
      if (!o)
         snap_add(&set, 2, 0.51, j ? 3.0 : -3.0);
      set.quads[!o].pose.position.y = (i & 2) ? 0.41f : 0.4f;
      snap_pose(&p, 0.0, 0.0, -2.0, 0.0);
      CHECK((video_xr_snap_hinge(&set, 1, 0, 0.5f, 0.4f, 0.02f, 1.0f, &p,
                  &h) & VIDEO_XR_SNAP_HINGE)
            && h.slot == 0 && fabs(h.gap - (fr - 0.01)) < 1e-5);
      CHECK(fabs(snap_yaw(&p)) < 1e-3 && vec_is(&p.position, -0.01, 0.0,
               -2.0));
   }
}

/* Square at A's corner, a 2 cm gap set, turned by hand from 4 degrees
 * short of square to 5 past, with C on the side wall up to 2 mm short
 * of its click and 0.5 to 2 degrees off square, or off where the hand
 * is 4 past: the held screen stays square, hinged to A, in either
 * order. */
static void test_gap_nearer_corner(void)
{
   static const double gc[] = { 0.018, 0.019, 0.02 };
   static const double dc[] = { 0.5, 1.0, 2.0 };
   unsigned i, j, k, o, w;
   video_xr_quad_set_t set;
   video_xr_pose_t p, last;
   video_xr_hinge_t h;
   for (w = 0; w < 2; w++)
   for (i = 0; i < 3; i++)
   for (j = 0; j < 3; j++)
   for (o = 0; o < 2; o++)
   for (k = 0; k < 900; k++)
   {
      double at  = w ? -94.0 : -90.0;
      double c   = at * M_PI / 180.0;
      double e   = (at + (w ? dc[j] : 0.0)) * M_PI / 180.0;
      double psi = -86.0 - 0.01 * k;
      double r   = psi * M_PI / 180.0;
      memset(&set, 0, sizeof(set));
      if (o)
         snap_add(&set, 2, 0.0, at + dc[j]);
      snap_add(&set, 0, 0.0, 0.0);
      if (!o)
         snap_add(&set, 2, 0.0, at + dc[j]);
      /* Turned about its middle, or 4 past about its near edge. */
      set.quads[!o].pose.position.x = (float)(0.27
            + (0.5 + gc[i]) * cos(c) + 0.25 * cos(e));
      set.quads[!o].pose.position.z = (float)(-2.0
            - (0.5 + gc[i]) * sin(c) - 0.25 * sin(e));
      snap_pose(&p, 0.27 + 0.25 * cos(r), 0.0, -2.0 - 0.25 * sin(r), psi);
      CHECK((video_xr_snap_hinge(&set, 1, 0, 0.5f, 0.4f, 0.02f, 1.0f, &p,
                  &h) & VIDEO_XR_SNAP_HINGE) && h.slot == 0);
      CHECK(fabs(snap_yaw(&p) + 90.0) < 1e-3);
      if (k)
         CHECK(fabs(snap_yaw(&p) - snap_yaw(&last)) <= 0.101
               && pose_gap(&p, &last) <= 0.005);
      last = p;
   }
}

/* A commit takes a gap the click holds to the set one, which it shows
 * already, and leaves one in the click's fade or free where the hand
 * has it: a 2 cm gap set, taken 2.6, 3.5 and 6 cm under at 30 degrees,
 * shown at 2, 3 and 6 before and after. */
static void test_gap_commit(void)
{
   static const double under[] = { 0.026, 0.035, 0.06 };
   static const double kept[]  = { 0.02, 0.035, 0.06 };
   unsigned u;
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, start, want, shown, now, hand;
   video_xr_quat_t tip;
   video_xr_hinge_t h;
   t_gap = 0.02f;
   hinge_turn(&tip, 1.0, 0.0, 0.0, -30.0);
   video_xr_pose_identity(&aim);
   for (u = 0; u < 3; u++)
   {
      hinge_grab(&set, &g, &start, false);
      hinge_place(&want, &tip, 0.0, 0.48, 0.0, -0.48 - under[u], -1.8);
      video_xr_grab_put(&g, &aim, &want);
      CHECK(video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &shown, &h)
            & VIDEO_XR_SNAP_HINGE);
      video_xr_grab_put_line(&g, &aim, &h);
      CHECK(video_xr_grab_hinge(&g, &aim, &set, 2u, &start, &now, NULL)
            & VIDEO_XR_SNAP_HINGE);
      video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &hand, NULL, NULL);
      CHECK(pose_gap(&now, &shown) < 1e-5
            && fabs(hinge_off_line(&hand) - kept[u]) < 1e-5);
   }
   t_gap = 0.0f;
}

/* Square, 8 cm out with a 2 cm gap set, the corner puts its edge in
 * the other's plane and leaves the gap; with that gap, 88 degrees from
 * flat hinges and 92 does not; 13.5 cm under at 30 degrees nothing
 * moves it. */
static void test_gap_limits(void)
{
   unsigned bits;
   video_xr_quad_set_t set;
   video_xr_pose_t p, was;
   video_xr_quat_t tip;
   video_xr_hinge_t h;
   snap_set(&set);
   snap_pose(&p, 0.88, 0.01, -1.49, 90.0);
   bits = video_xr_snap_hinge(&set, 1, 0, 1.0f, 0.6f, 0.02f, 1.0f, &p,
         &h);
   CHECK((bits & VIDEO_XR_SNAP_HINGE) && (bits & VIDEO_XR_SNAP_X)
         && !(bits & (VIDEO_XR_SNAP_Z | VIDEO_XR_SNAP_GAP)));
   CHECK(vec_is(&p.position, 0.88, 0.0, -1.5) && fabs(h.gap - 0.08) < 1e-5
         && angle_near((float)snap_yaw(&p), 90.0f));
   hinge_set(&set);
   t_gap = 0.02f;
   hinge_turn(&tip, 1.0, 0.0, 0.0, -88.0);
   hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.5, -1.8);
   bits = hinge_snap(&set, &p, 1.0f, &h);
   CHECK((bits & VIDEO_XR_SNAP_HINGE) && (bits & VIDEO_XR_SNAP_GAP)
         && fabs(h.angle + 88.0) < 0.01);
   hinge_turn(&tip, 1.0, 0.0, 0.0, -92.0);
   hinge_place(&p, &tip, 0.0, 0.48, 0.0, -0.5, -1.8);
   CHECK(!(hinge_snap(&set, &p, 1.0f, NULL) & VIDEO_XR_SNAP_HINGE));
   hinge_turn(&tip, 1.0, 0.0, 0.0, -30.0);
   hinge_place(&p, &tip, 0.0, 0.48, 0.05, -0.615, -1.8);
   was = p;
   CHECK(hinge_snap(&set, &p, 1.0f, NULL) == 0 && pose_same(&p, &was));
   t_gap = 0.0f;
}

/* The hinge's sweeps again with a gap: no hop or slide, no creep, a
 * commit never moves it (test_hinge_continuous, test_hinge_roll_band,
 * test_hinge_put_still at 2, 5 and 8 cm). */
static void test_gap_sweeps(void)
{
   static const float gaps[] = { 0.02f, 0.05f, 0.08f };
   unsigned i;
   for (i = 0; i < 3; i++)
   {
      t_gap = gaps[i];
      test_hinge_continuous();
      test_hinge_roll_band();
      test_hinge_put_still();
   }
   t_gap = 0.0f;
}

/* In the grab: snapped after levelling, its settings from the snapped
 * pose, no clamp for a snap, the clamp still winning; the XR thread's
 * call shape (no settings, no snaps) gives the same pose. Taken 10.8 cm
 * from where the hand now puts it, so the fade-in is whole. */
static void test_grab_snap(void)
{
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, pose, bare, start;
   video_screen_xr_t s;
   unsigned snapped = 0;
   snap_set(&set);
   memset(&g, 0, sizeof(g));
   video_xr_pose_identity(&g.rel);
   g.rel.position.z = -2.0f;
   g.turn           = g.rel.orientation;
   g.base.z         = -2.0f;
   g.width          = 1.0f;
   g.height         = 0.6f;
   g.slot           = 1;
   g.state          = VIDEO_XR_GRAB_HELD;
   video_xr_pose_identity(&start);
   start.position.x = 1.2f;
   start.position.z = -2.0f;
   video_xr_pose_identity(&aim);
   aim.position.x = 1.308f;
   aim.position.y = 0.01f;
   aim.position.z = 0.02f;
   CHECK(!video_xr_grab_pose(&g, &aim, &set, 0, &start, &pose, &s,
            &snapped));
   CHECK((snapped & VIDEO_XR_SNAP_ALL) == SNAP_ALL
         && vec_is(&pose.position, 1.30, 0.0, -2.0));
   CHECK(NEAR(s.x, 1.30) && NEAR(s.y, 0.0) && NEAR(s.z, 0.0));
   CHECK(!video_xr_grab_pose(&g, &aim, &set, 0, &start, &bare, NULL, NULL));
   CHECK(!memcmp(&pose, &bare, sizeof(pose)));
   CHECK(!video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, &snapped));
   CHECK(snapped == 0 && vec_is(&pose.position, 1.308, 0.01, -1.98));
   video_xr_grab_pose(&g, &aim, &set, 1u, &start, &pose, &s, &snapped);
   CHECK(snapped == 0 && vec_is(&pose.position, 1.308, 0.01, -1.98));
   /* A snap past +10 m: the clamp stops it there, and says so. */
   set.quads[0].pose.position.x = 11.303f;
   aim.position.x               = 9.995f;
   aim.position.y               = 0.0f;
   aim.position.z               = 0.0f;
   CHECK(video_xr_grab_pose(&g, &aim, &set, 0, &start, &pose, &s,
            &snapped));
   CHECK(NEAR(s.x, 10.0) && NEAR(pose.position.x, 10.0));
}

/* Turned by the stick into square at screen 1's right corner, a held
 * screen snaps exactly square there and says so. */
static void test_turn_snap(void)
{
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, pose, start;
   video_screen_xr_t s;
   unsigned snapped = 0;
   snap_set(&set);
   memset(&g, 0, sizeof(g));
   video_xr_pose_identity(&aim);
   snap_pose(&g.rel, 0.773, 0.0, -1.5, 82.0);
   g.base   = g.rel.position;
   g.turn   = g.rel.orientation;
   g.width  = 1.0f;
   g.height = 0.6f;
   g.slot   = 1;
   /* Taken a metre away, so the snaps are whole. */
   snap_pose(&start, 0.8, 0.0, -0.5, 82.0);
   video_xr_grab_pose(&g, &aim, &set, 0, &start, &pose, &s, &snapped);
   CHECK(!(snapped & VIDEO_XR_SNAP_TURN)
         && (snapped & VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_TURN))
         && !angle_near((float)snap_yaw(&pose), 90.0f));
   /* Four degrees more by the stick: square, at the corner. */
   video_xr_grab_turn(&g, &aim, 1.0f, 0.0f, 4.0 / VIDEO_XR_EDIT_TURN_RATE);
   video_xr_grab_pose(&g, &aim, &set, 0, &start, &pose, &s, &snapped);
   CHECK((snapped & VIDEO_XR_SNAP_ALL) == SNAP_ALL
         && angle_near(s.yaw, 90.0f)
         && vec_is(&pose.position, 0.8, 0.0, -1.5));
}

/* Tilted by the stick into parallel beside screen 1, a held screen
 * snaps exactly parallel and says so, as a turn does. */
static void test_tilt_snap(void)
{
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, pose, start;
   video_screen_xr_t s;
   unsigned snapped = 0;
   snap_set(&set);
   memset(&g, 0, sizeof(g));
   video_xr_pose_identity(&aim);
   axis_turn(&g.rel, 0, 8.0);
   g.rel.position.x = 1.3f;
   g.rel.position.z = -2.0f;
   g.base   = g.rel.position;
   g.turn   = g.rel.orientation;
   g.width  = 1.0f;
   g.height = 0.6f;
   g.slot   = 1;
   /* Taken a metre away, so the snaps are whole. */
   start            = g.rel;
   start.position.z = -1.0f;
   video_xr_grab_pose(&g, &aim, &set, 0, &start, &pose, &s, &snapped);
   CHECK(!(snapped & VIDEO_XR_SNAP_TURN)
         && (snapped & VIDEO_XR_SNAP_PULLING(VIDEO_XR_SNAP_TURN))
         && !angle_near(s.pitch, 0.0f));
   /* Four degrees back by the stick: parallel. */
   video_xr_grab_turn(&g, &aim, 0.0f, 1.0f, 4.0 / VIDEO_XR_EDIT_TURN_RATE);
   video_xr_grab_pose(&g, &aim, &set, 0, &start, &pose, &s, &snapped);
   CHECK((snapped & VIDEO_XR_SNAP_TURN)
         && turn_apart(&pose.orientation, &aim.orientation) < 0.01);
}

/* The angle the free hand's stick has given, as steps every size
 * degrees show it: within VIDEO_XR_EDIT_TURN_HOLD of a step, the step,
 * and *held. Size 0: no steps. */
static double step_shown(double deg, double size, int *held)
{
   double s;
   *held = 0;
   if (size <= 0.0)
      return deg;
   s     = floor(deg / size + 0.5) * size;
   *held = fabs(deg - s) <= VIDEO_XR_EDIT_TURN_HOLD;
   return *held ? s : deg;
}

/* Degrees between two angles, either side of +-180. */
static double angle_off(double a, double b)
{
   double d = fmod(fabs(a - b), 360.0);
   return (d > 180.0) ? 360.0 - d : d;
}

/* g holding an item at x, y, z turned yaw then pitch degrees, taken
 * with aim. */
static void step_grab(video_xr_grab_t *g, const video_xr_pose_t *aim,
      double x, double y, double z, double yaw, double pitch)
{
   video_xr_pose_t item, tip, inv;
   memset(g, 0, sizeof(*g));
   snap_pose(&item, x, y, z, yaw);
   axis_turn(&tip, 0, pitch);
   video_xr_compose(&item, &tip, &item);
   g->base = item.position;
   video_xr_pose_invert(aim, &inv);
   video_xr_compose(&inv, &item, &g->rel);
   video_xr_grab_hold(g, aim, &item.orientation);
}

/* The free hand's stick for 10 ms at x, y on g's item, then its steps
 * every size degrees from facing head; *buzzes counts the steps taken
 * with their buzz armed, as the main thread arms it in *armed. */
static unsigned step_stick(video_xr_grab_t *g, const video_xr_pose_t *aim,
      const video_xr_vec3_t *head, float size, float *rest, float x,
      float y, unsigned *armed, unsigned *buzzes)
{
   unsigned held;
   *armed |= video_xr_grab_off_step(g, aim, head, size, rest);
   held    = video_xr_grab_step(g, aim, head, size,
         video_xr_grab_turn(g, aim, x, y, 0.01), rest, NULL, 0, NULL,
         NULL);
   if (held & *armed)
      (*buzzes)++;
   *armed &= ~held;
   return held;
}

/* Turned by the free hand's stick, an item off to the side steps every
 * size degrees from facing the head across the floor, from where the
 * head is, not where the hand points: within VIDEO_XR_EDIT_TURN_HOLD
 * of a step it holds there, and pushed on, it leaves it, its buzz armed
 * again for the next step it comes to. Left, then right, and on round a
 * whole turn. Size 0: never held. */
static void test_step_turn_at(float size)
{
   unsigned k, held, bad = 0, holds = 0, faced = 0, armed = 0, buzzes = 0;
   unsigned entered = 0;
   int was_at = 0;
   float rest[2];
   double zero, deg = 20.1;
   video_xr_grab_t g;
   video_xr_pose_t aim, pose;
   video_xr_vec3_t head;
   video_screen_xr_t s;
   rest[0] = 0.0f;
   rest[1] = 0.0f;
   head.x  = -0.3f;
   head.y  = 0.2f;
   head.z  = 0.4f;
   /* The yaw that faces the head from 0.9, -1.8. */
   zero    = atan2(-1.2, 2.2) * 180.0 / M_PI;
   turn_aim(&aim, 30.0f, 10.0f, 0.0f);
   step_grab(&g, &aim, 0.9, 0.0, -1.8, zero + deg, 0.0);
   for (k = 0; k < 600; k++)
   {
      float x     = (k < 100) ? -1.0f : 1.0f;
      int at;
      double want;
      deg        += 0.9 * x;
      want        = step_shown(deg, size, &at);
      held        = step_stick(&g, &aim, &head, size, rest, x, 0.0f,
            &armed, &buzzes);
      video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, &s, NULL);
      if (held)
         holds++;
      if (at && !was_at)
         entered++;
      was_at = at;
      if (     angle_off(s.yaw, zero + want) > 0.02
            || held != (at ? VIDEO_XR_STEP_TURN : 0u)
            || !vec_is(&pose.position, 0.9, 0.0, -1.8))
      {
         if (!bad++)
            printf("  FAIL %s:%d: size %.0f frame %u at %.2f: yaw %.3f"
                  " held %u, want %.3f\n", __FILE__, __LINE__, size, k,
                  deg, s.yaw, held, zero + want);
      }
      if (size > 0.0f && want == 0.0)
      {
         /* At 0, its front looks at the head across the floor. */
         video_xr_vec3_t v, front;
         v.x = 0.0f;
         v.y = 0.0f;
         v.z = 1.0f;
         video_xr_rotate(&pose.orientation, &v, &front);
         if (     fabs(front.x * (head.z - pose.position.z)
                  - front.z * (head.x - pose.position.x)) < 1e-3
               && front.x * (head.x - pose.position.x)
                  + front.z * (head.z - pose.position.z) > 0.0)
            faced++;
         else
            bad++;
      }
   }
   CHECK(bad == 0);
   /* One buzz a step, about four frames each. */
   CHECK(buzzes == entered && holds >= 4 * entered && holds <= 5 * entered);
   if (size > 0.0f)
      CHECK(entered >= (unsigned)(500.0f / size) && faced >= 8);
   else
      CHECK(holds == 0 && buzzes == 0);
}

static void test_step_turn(void)
{
   test_step_turn_at(15.0f);
   test_step_turn_at(30.0f);
   test_step_turn_at(45.0f);
   test_step_turn_at(90.0f);
   test_step_turn_at(0.0f);
}

/* Tilted by the free hand's stick, an item steps every size degrees
 * from upright about its own left-right line, holding within
 * VIDEO_XR_EDIT_TURN_HOLD of each and buzzing once each time it comes
 * to one, either way and on all the way round; carried or with Tilt
 * held. Size 0: never held. */
static void test_step_tilt_at(float size)
{
   unsigned tilt;
   for (tilt = 0; tilt < 2; tilt++)
   {
      unsigned k, held, bad = 0, holds = 0, armed = 0, buzzes = 0;
      unsigned entered = 0;
      int was_at = 0;
      float rest[2];
      double deg = -6.3;
      video_xr_grab_t g;
      video_xr_pose_t aim, pose, want_q, tip;
      rest[0] = 0.0f;
      rest[1] = 0.0f;
      turn_aim(&aim, -20.0f, 15.0f, 5.0f);
      step_grab(&g, &aim, 0.4, 0.1, -1.8, 25.0, deg);
      g.tilt = tilt != 0;
      for (k = 0; k < 480; k++)
      {
         /* Down 30 frames, then up 450: a whole turn and more. */
         float y = (k < 30) ? -1.0f : 1.0f;
         int at;
         double want;
         deg   -= 0.9 * y;
         want   = step_shown(deg, size, &at);
         held   = step_stick(&g, &aim, NULL, size, rest, 0.0f, y, &armed,
               &buzzes);
         video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL);
         snap_pose(&want_q, 0.0, 0.0, 0.0, 25.0);
         axis_turn(&tip, 0, want);
         video_xr_compose(&want_q, &tip, &want_q);
         if (held)
            holds++;
         if (at && !was_at)
            entered++;
         was_at = at;
         if (     turn_apart(&pose.orientation, &want_q.orientation) > 0.02
               || held != (at ? VIDEO_XR_STEP_TILT : 0u)
               || !vec_is(&pose.position, 0.4, 0.1, -1.8))
         {
            if (!bad++)
               printf("  FAIL %s:%d: size %.0f tilt %u frame %u at %.2f:"
                     " %.3f off, held %u\n", __FILE__, __LINE__, size,
                     tilt, k, deg,
                     turn_apart(&pose.orientation, &want_q.orientation),
                     held);
         }
      }
      CHECK(bad == 0);
      CHECK(buzzes == entered && holds >= 4 * entered
            && holds <= 5 * entered);
      if (size > 0.0f)
         CHECK(entered >= (unsigned)(400.0f / size));
      else
         CHECK(holds == 0 && buzzes == 0);
   }
}

static void test_step_tilt(void)
{
   test_step_tilt_at(15.0f);
   test_step_tilt_at(30.0f);
   test_step_tilt_at(45.0f);
   test_step_tilt_at(90.0f);
   test_step_tilt_at(0.0f);
}

/* Lying flat, face up, an item still turns in steps from facing the
 * head, its top away from it at 0, and never to NaN. Its heading there
 * blends in a little of its front's, so a little off. */
static void test_step_flat(void)
{
   unsigned k, held, bad = 0, armed = 0, buzzes = 0;
   float rest[2];
   double deg = 0.0;
   video_xr_grab_t g;
   video_xr_pose_t aim, pose, want_q, tip;
   video_xr_vec3_t head;
   rest[0] = 0.0f;
   rest[1] = 0.0f;
   head.x  = 0.0f;
   head.y  = 0.0f;
   head.z  = 0.0f;
   video_xr_pose_identity(&aim);
   step_grab(&g, &aim, 0.0, -0.5, -1.8, 0.0, -90.0);
   for (k = 0; k < 50; k++)
   {
      int at;
      double want;
      deg  += 0.9;
      want  = step_shown(deg, 15.0, &at);
      held  = step_stick(&g, &aim, &head, 15.0f, rest, 1.0f, 0.0f, &armed,
            &buzzes);
      video_xr_grab_pose(&g, &aim, NULL, 0, NULL, &pose, NULL, NULL);
      snap_pose(&want_q, 0.0, 0.0, 0.0, want);
      axis_turn(&tip, 0, -90.0);
      video_xr_compose(&want_q, &tip, &want_q);
      if (     !(turn_apart(&pose.orientation, &want_q.orientation) < 0.2)
            || held != (at ? VIDEO_XR_STEP_TURN : 0u))
      {
         if (!bad++)
            printf("  FAIL %s:%d: frame %u at %.2f: %.3f off\n", __FILE__,
                  __LINE__, k, deg,
                  turn_apart(&pose.orientation, &want_q.orientation));
      }
   }
   CHECK(bad == 0);
}

/* A snap to another screen that would engage beats a step: turned on
 * by the stick from holding at a step 6.5 degrees short of parallel,
 * the held screen snaps parallel once the stick has taken it within
 * VIDEO_XR_SNAP_ANGLE, still inside the step's hold. */
static void test_step_snap_first(void)
{
   unsigned k, held[7], snapped = 0;
   float rest[2];
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, pose, start;
   video_xr_vec3_t head;
   rest[0] = 0.0f;
   rest[1] = 0.0f;
   snap_set(&set);
   video_xr_pose_identity(&aim);
   /* Facing the head 23.5 degrees left of parallel: the 30 step is
    * 6.5 right of it. */
   head.x   = (float)(1.3 - 2.0 * tan(23.5 * M_PI / 180.0));
   head.y   = 0.0f;
   head.z   = 0.0f;
   step_grab(&g, &aim, 1.3, 0.0, -2.0, 10.0, 0.0);
   g.width  = 1.0f;
   g.height = 0.6f;
   g.slot   = 1;
   /* Taken a metre away, so the snaps are whole. */
   snap_pose(&start, 1.3, 0.0, -1.0, 10.0);
   for (k = 1; k <= 6; k++)
   {
      held[k] = video_xr_grab_step(&g, &aim, &head, 15.0f,
            video_xr_grab_turn(&g, &aim, -1.0f, 0.0f, 0.01), rest, &set, 0,
            &start, NULL);
      video_xr_grab_pose(&g, &aim, &set, 0, &start, &pose, NULL, &snapped);
   }
   /* 31.7 to 29.0 held at 30; at 28.1, 4.6 from parallel, the snap. */
   CHECK(held[1] == 0 && held[2] == VIDEO_XR_STEP_TURN
         && held[5] == VIDEO_XR_STEP_TURN);
   CHECK(held[6] == 0 && (snapped & VIDEO_XR_SNAP_TURN)
         && turn_apart(&pose.orientation, &aim.orientation) < 0.01
         && rest[0] == 0.0f);
}

/* g's item where aim puts it with no snap, and the point x, y of it,
 * in its own frame, where that puts it. */
static void lid_edge(const video_xr_grab_t *g, const video_xr_pose_t *aim,
      float x, float y, video_xr_pose_t *pose, video_xr_vec3_t *edge)
{
   video_xr_vec3_t v;
   v.x = x;
   v.y = y;
   v.z = 0.0f;
   video_xr_grab_pose(g, aim, NULL, 0, NULL, pose, NULL, NULL);
   video_xr_rotate(&pose->orientation, &v, edge);
   edge->x += pose->position.x;
   edge->y += pose->position.y;
   edge->z += pose->position.z;
}

/* Let go off a hinge on its top edge, an item tilted 1.7 degrees past
 * the 90 step holds there about that edge, as a lid: the hold catching
 * it, holding it again and letting it go leave that edge where it was.
 * With no lid, or a lid on the turn, about its middle. Turned 1.7 past
 * facing square to the head, one let go off its right edge holds about
 * that edge. */
static void test_step_lid(void)
{
   unsigned k, held, again, off;
   float rest[2];
   video_xr_grab_t g;
   video_xr_hinge_t lid;
   video_xr_pose_t aim, pose, want_q, tip;
   video_xr_vec3_t e0, e1, e2, e3, head;
   turn_aim(&aim, -20.0f, 15.0f, 5.0f);
   memset(&lid, 0, sizeof(lid));
   for (k = 0; k < 3; k++)
   {
      rest[0] = 0.0f;
      rest[1] = 0.0f;
      lid.edge = 0;
      lid.axis = (k == 2) ? VIDEO_XR_STEP_TURN : VIDEO_XR_STEP_TILT;
      step_grab(&g, &aim, 0.4, 0.1, -1.8, 25.0, -91.7);
      g.width  = 1.0f;
      g.height = 0.6f;
      lid_edge(&g, &aim, 0.0f, 0.3f, &pose, &e0);
      held = video_xr_grab_step(&g, &aim, NULL, 15.0f, VIDEO_XR_STEP_TILT,
            rest, NULL, 0, NULL, k ? &lid : NULL);
      lid_edge(&g, &aim, 0.0f, 0.3f, &pose, &e1);
      snap_pose(&want_q, 0.0, 0.0, 0.0, 25.0);
      axis_turn(&tip, 0, -90.0);
      video_xr_compose(&want_q, &tip, &want_q);
      CHECK(held == VIDEO_XR_STEP_TILT && NEAR(rest[1], -1.7)
            && turn_apart(&pose.orientation, &want_q.orientation) < 0.02);
      /* 2 sin 0.85 of 0.3 m: the edge or the middle moves 8.9 mm. */
      if (k == 1)
         CHECK(dist(&e0, &e1) < 1e-5
               && !vec_is(&pose.position, 0.4, 0.1, -1.8)
               && fabs(sqrt(pow(pose.position.x - 0.4, 2.0)
                        + pow(pose.position.y - 0.1, 2.0)
                        + pow(pose.position.z + 1.8, 2.0)) - 0.0089)
                  < 1e-4);
      else
         CHECK(vec_is(&pose.position, 0.4, 0.1, -1.8)
               && fabs(dist(&e0, &e1) - 0.0089) < 1e-4);
      again = video_xr_grab_step(&g, &aim, NULL, 15.0f, VIDEO_XR_STEP_TILT,
            rest, NULL, 0, NULL, k ? &lid : NULL);
      lid_edge(&g, &aim, 0.0f, 0.3f, &pose, &e2);
      /* Carried 1.3 on past the hold, it lets go where the stick put it. */
      rest[1] -= 1.3f;
      off = video_xr_grab_step(&g, &aim, NULL, 15.0f, VIDEO_XR_STEP_TILT,
            rest, NULL, 0, NULL, k ? &lid : NULL);
      lid_edge(&g, &aim, 0.0f, 0.3f, &pose, &e3);
      CHECK(again == VIDEO_XR_STEP_TILT && dist(&e1, &e2) < 1e-5
            && off == 0 && rest[1] == 0.0f);
      if (k == 1)
         CHECK(dist(&e0, &e3) < 1e-5);
      else
         CHECK(vec_is(&pose.position, 0.4, 0.1, -1.8));
   }
   rest[0]  = 0.0f;
   rest[1]  = 0.0f;
   lid.edge = 2;
   lid.axis = VIDEO_XR_STEP_TURN;
   head.x   = 0.4f;
   head.y   = 0.1f;
   head.z   = 0.0f;
   step_grab(&g, &aim, 0.4, 0.1, -1.8, 91.7, 0.0);
   g.width  = 1.0f;
   g.height = 0.6f;
   lid_edge(&g, &aim, 0.5f, 0.0f, &pose, &e0);
   held = video_xr_grab_step(&g, &aim, &head, 15.0f, VIDEO_XR_STEP_TURN,
         rest, NULL, 0, NULL, &lid);
   lid_edge(&g, &aim, 0.5f, 0.0f, &pose, &e1);
   CHECK(held == VIDEO_XR_STEP_TURN && NEAR(fabs(rest[0]), 1.7)
         && dist(&e0, &e1) < 1e-5
         && !vec_is(&pose.position, 0.4, 0.1, -1.8));
}

/* The stick on an item let go off a hinge on its top edge: its tilt
 * keeps that edge, as video_xr_grab_step holds it, and its turn the
 * middle; with no lid, as video_xr_grab_turn. */
static void test_turn_lid(void)
{
   unsigned k, acted;
   video_xr_grab_t g, plain;
   video_xr_hinge_t lid;
   video_xr_pose_t aim, pose, want;
   video_xr_vec3_t e0, e1;
   turn_aim(&aim, -20.0f, 15.0f, 5.0f);
   memset(&lid, 0, sizeof(lid));
   lid.edge = 0;
   lid.axis = VIDEO_XR_STEP_TILT;
   for (k = 0; k < 3; k++)
   {
      float x = (k == 1) ? 1.0f : 0.0f;
      float y = (k == 1) ? 0.0f : 1.0f;
      step_grab(&g, &aim, 0.4, 0.1, -1.8, 25.0, -60.0);
      g.width  = 1.0f;
      g.height = 0.6f;
      plain    = g;
      lid_edge(&g, &aim, 0.0f, 0.3f, &pose, &e0);
      acted = video_xr_grab_turn_lid(&g, &aim, k < 2 ? &lid : NULL, x, y,
            0.1);
      lid_edge(&g, &aim, 0.0f, 0.3f, &pose, &e1);
      video_xr_grab_turn(&plain, &aim, x, y, 0.1);
      video_xr_grab_pose(&plain, &aim, NULL, 0, NULL, &want, NULL, NULL);
      CHECK(acted == (k == 1 ? VIDEO_XR_STEP_TURN : VIDEO_XR_STEP_TILT)
            && turn_apart(&pose.orientation, &want.orientation) < 1e-4
            && vec_is(&want.position, 0.4, 0.1, -1.8));
      /* 9 degrees about the edge: 2 sin 4.5 of 0.3 m, 4.7 cm. */
      if (k == 0)
         CHECK(dist(&e0, &e1) < 1e-5
               && fabs(dist(&pose.position, &want.position) - 0.0471)
                  < 1e-4);
      else
         CHECK(dist(&pose.position, &want.position) < 1e-6);
   }
}

/* An upright item's top edge hinged on the bottom edge of the screen
 * above it, then carried: in reach within 3 cm of that screen's plane,
 * from 3 cm over to 13 cm out from its edge and while the edges
 * overlap; measured against where that screen is now, not with it
 * gone. */
static void test_near_hinge(void)
{
   /* x, the gap, off the plane, in reach */
   static const double at[][4] = {
      { 0.0,    0.0,    0.0,    1 },
      { 0.0,    0.0,    0.029,  1 },
      { 0.0,    0.0,   -0.029,  1 },
      { 0.0,    0.0,    0.031,  0 },
      { 0.0,    0.0,   -0.031,  0 },
      { 0.0,    0.129,  0.0,    1 },
      { 0.0,    0.131,  0.0,    0 },
      { 0.0,   -0.029,  0.0,    1 },
      { 0.0,   -0.031,  0.0,    0 },
      { 0.99,   0.0,    0.0,    1 },
      { -1.01,  0.0,    0.0,    0 }
   };
   unsigned k;
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_hinge_t h;
   video_xr_pose_t aim;
   turn_aim(&aim, -20.0f, 15.0f, 5.0f);
   memset(&set, 0, sizeof(set));
   snap_add(&set, 0, 0.0, 0.0);
   set.quads[0].pose.position.y = 0.6f;
   set.quads[0].pose.position.z = -1.8f;
   set.quads[0].width           = 1.0f;
   set.quads[0].height          = 0.6f;
   memset(&h, 0, sizeof(h));
   h.edge = 0;
   h.slot = 0;
   h.on   = 1;
   h.axis = VIDEO_XR_STEP_TILT;
   for (k = 0; k < sizeof(at) / sizeof(at[0]); k++)
   {
      step_grab(&g, &aim, at[k][0], -at[k][1], -1.8 + at[k][2], 0.0,
            0.0);
      g.width  = 1.0f;
      g.height = 0.6f;
      CHECK(video_xr_grab_near_hinge(&g, &aim, &set, &h,
               VIDEO_XR_SNAP_DIST) == (at[k][3] != 0.0));
   }
   step_grab(&g, &aim, 0.0, 0.0, -1.8, 0.0, 0.0);
   g.width  = 1.0f;
   g.height = 0.6f;
   set.quads[0].pose.position.z = -1.765f;
   CHECK(!video_xr_grab_near_hinge(&g, &aim, &set, &h,
            VIDEO_XR_SNAP_DIST));
   set.quads[0].pose.position.z = -1.79f;
   CHECK(video_xr_grab_near_hinge(&g, &aim, &set, &h, VIDEO_XR_SNAP_DIST));
   h.slot = 2;
   CHECK(!video_xr_grab_near_hinge(&g, &aim, &set, &h,
            VIDEO_XR_SNAP_DIST));
   h.slot = 0;
   CHECK(!video_xr_grab_near_hinge(&g, &aim, NULL, &h,
            VIDEO_XR_SNAP_DIST));
   /* The hinge a hold was taken in reaches 4.5 cm off its plane. */
   CHECK(fabs(VIDEO_XR_HINGE_REACH - 0.045) < 1e-6);
   set.quads[0].pose.position.z = -1.8f;
   step_grab(&g, &aim, 0.0, 0.0, -1.8 + 0.044, 0.0, 0.0);
   g.width  = 1.0f;
   g.height = 0.6f;
   CHECK(     video_xr_grab_near_hinge(&g, &aim, &set, &h,
               VIDEO_XR_HINGE_REACH)
         && !video_xr_grab_near_hinge(&g, &aim, &set, &h,
               VIDEO_XR_SNAP_DIST));
   step_grab(&g, &aim, 0.0, 0.0, -1.8 - 0.046, 0.0, 0.0);
   g.width  = 1.0f;
   g.height = 0.6f;
   CHECK(!video_xr_grab_near_hinge(&g, &aim, &set, &h,
            VIDEO_XR_HINGE_REACH));
}

#define SMOOTH_MAX 400
static double t_smooth_raw[SMOOTH_MAX];
static double t_smooth_out[SMOOTH_MAX];

/* Edit Smoothing at level, hz frames a second for secs, on a yaw that
 * turns rate degrees a second with a shake of amp degrees at f Hz: each
 * frame's raw and smoothed yaw into t_smooth_raw and t_smooth_out.
 * Returns the frames. */
static unsigned smooth_run(unsigned level, double hz, double secs,
      double rate, double amp, double f)
{
   unsigned i;
   video_xr_smooth_t s;
   video_xr_pose_t raw, out;
   unsigned n = (unsigned)(secs * hz) + 1;
   memset(&s, 0, sizeof(s));
   for (i = 0; i < n && i < SMOOTH_MAX; i++)
   {
      double t   = i / hz;
      double yaw = rate * t + amp * sin(2.0 * M_PI * f * t);
      snap_pose(&raw, 0.0, 0.0, 0.0, yaw);
      video_xr_smooth_step(&s, level, &raw, i ? 1.0 / hz : 0.0, &out);
      t_smooth_raw[i] = yaw;
      t_smooth_out[i] = snap_yaw(&out);
   }
   return i;
}

/* The root mean square of a run's second differences: what a shake
 * adds and a steady turn does not. */
static double smooth_jerk(const double *a, unsigned n)
{
   unsigned i;
   double sum = 0.0;
   for (i = 1; i + 1 < n; i++)
   {
      double d = a[i + 1] - 2.0 * a[i] + a[i - 1];
      sum += d * d;
   }
   return sqrt(sum / (n - 2));
}

/* Steady after a step of 2 cm and 5 degrees, Medium settles on the
 * hand within two seconds, part way after one frame. */
static void test_smooth_steady(void)
{
   unsigned i;
   video_xr_smooth_t s;
   video_xr_pose_t raw, out;
   memset(&s, 0, sizeof(s));
   snap_pose(&raw, 0.0, 0.0, 0.0, 0.0);
   video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM, &raw, 0.0,
         &out);
   CHECK(s.on && pose_same(&out, &raw));
   snap_pose(&raw, 0.02, 0.0, 0.0, 5.0);
   video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM, &raw,
         1.0 / 90.0, &out);
   CHECK(out.position.x > 0.0f && out.position.x < 0.02f
         && snap_yaw(&out) > 0.0 && snap_yaw(&out) < 5.0);
   for (i = 0; i < 180; i++)
      video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM, &raw,
            1.0 / 90.0, &out);
   CHECK(fabs(snap_yaw(&out) - 5.0) < 0.01
         && fabs(out.position.x - 0.02) < 0.0001);
}

/* A 0.3 degree shake at 8 to 12 Hz, the hand still or turning 10
 * degrees a second: Medium cuts it by at least half (about a fifth). */
static void test_smooth_tremor(void)
{
   unsigned k, m;
   static const double f[3] = { 8.0, 10.0, 12.0 };
   for (k = 0; k < 3; k++)
      for (m = 0; m < 2; m++)
      {
         unsigned n = smooth_run(VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM, 90.0,
               3.0, m ? 10.0 : 0.0, 0.3, f[k]);
         /* After a second's settling. */
         CHECK(smooth_jerk(t_smooth_out + 90, n - 90)
               < 0.5 * smooth_jerk(t_smooth_raw + 90, n - 90));
      }
}

/* A fast 90 degree sweep in 0.25 s, at 72, 90 and 120 Hz: 100 ms after
 * it ends Medium is within a degree of the hand (about 0.5 at most). */
static void test_smooth_sweep(void)
{
   unsigned k;
   static const double hz[3] = { 72.0, 90.0, 120.0 };
   for (k = 0; k < 3; k++)
   {
      unsigned i;
      video_xr_smooth_t s;
      video_xr_pose_t raw, out;
      unsigned end = (unsigned)(0.35 * hz[k] + 0.5);
      memset(&s, 0, sizeof(s));
      for (i = 0; i <= end; i++)
      {
         double t = i / hz[k];
         snap_pose(&raw, 0.0, 0.0, 0.0, (t < 0.25 ? t / 0.25 : 1.0) * 90.0);
         video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM, &raw,
               i ? 1.0 / hz[k] : 0.0, &out);
      }
      CHECK(fabs(snap_yaw(&out) - 90.0) < 1.0);
   }
}

/* The same move, 30 degrees and 10 cm over a second then still, at 72
 * and 120 Hz: at every 24th of a second the two agree within a degree
 * (about 0.4 at most) and 3 mm. */
static void test_smooth_rates(void)
{
   unsigned i, j;
   static const double hz[2]       = { 72.0, 120.0 };
   static const unsigned every[2]  = { 3, 5 };
   double yaw[2][49];
   double x[2][49];
   for (j = 0; j < 2; j++)
   {
      video_xr_smooth_t s;
      video_xr_pose_t raw, out;
      memset(&s, 0, sizeof(s));
      for (i = 0; i <= 48 * every[j]; i++)
      {
         double t = i / hz[j];
         double k = (t < 1.0) ? t : 1.0;
         snap_pose(&raw, 0.1 * k, 0.0, 0.0, 30.0 * k);
         video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM, &raw,
               i ? 1.0 / hz[j] : 0.0, &out);
         if (i % every[j] == 0)
         {
            yaw[j][i / every[j]] = snap_yaw(&out);
            x[j][i / every[j]]   = out.position.x;
         }
      }
   }
   for (i = 0; i < 49; i++)
      CHECK(fabs(yaw[0][i] - yaw[1][i]) < 1.0
            && fabs(x[0][i] - x[1][i]) < 0.003);
}

/* Off is the raw aim and keeps nothing. A first step, a step of no
 * time, a filter not on, and a jump of more than half a metre or 45
 * degrees in a step all start afresh from the raw aim; the next step
 * smooths again. */
static void test_smooth_resets(void)
{
   video_xr_smooth_t s;
   video_xr_pose_t raw, out;
   memset(&s, 0, sizeof(s));
   snap_pose(&raw, 0.0, 0.0, 0.0, 0.0);
   video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_OFF, &raw, 0.0,
         &out);
   snap_pose(&raw, 0.01, 0.0, 0.0, 3.0);
   video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_OFF, &raw,
         1.0 / 90.0, &out);
   CHECK(!s.on && pose_same(&out, &raw));
   video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_HIGH, &raw,
         1.0 / 90.0, &out);
   CHECK(s.on && pose_same(&out, &raw));
   snap_pose(&raw, 0.02, 0.0, 0.0, 4.0);
   video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_HIGH, &raw,
         1.0 / 90.0, &out);
   CHECK(!pose_same(&out, &raw));
   video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_HIGH, &raw, 0.0,
         &out);
   CHECK(pose_same(&out, &raw));
   snap_pose(&raw, 0.62, 0.0, 0.0, 4.0);
   video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_HIGH, &raw,
         1.0 / 90.0, &out);
   CHECK(pose_same(&out, &raw));
   snap_pose(&raw, 0.62, 0.0, 0.0, 54.0);
   video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_HIGH, &raw,
         1.0 / 90.0, &out);
   CHECK(pose_same(&out, &raw));
   /* 0.4 m and 40 degrees in a step: smoothed. */
   snap_pose(&raw, 0.22, 0.0, 0.0, 14.0);
   video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_HIGH, &raw,
         1.0 / 90.0, &out);
   CHECK(!pose_same(&out, &raw));
   s.on = false;
   video_xr_smooth_step(&s, VIDEO_OPENXR_EDIT_SMOOTHING_HIGH, &raw,
         1.0 / 90.0, &out);
   CHECK(pose_same(&out, &raw));
}

/* The 3DS layout: screen 1, 1.6 x 0.96 at 1.8 m; screen 2, 1.28 x 0.96,
 * held by a hand at the origin from its spot 1.6 cm under screen 1,
 * inside the click's 1-2 cm fade. */
static void snap_3ds(video_xr_quad_set_t *set, video_xr_grab_t *g,
      video_xr_pose_t *start)
{
   memset(set, 0, sizeof(*set));
   video_xr_pose_identity(&set->quads[0].pose);
   set->quads[0].pose.position.z = -1.8f;
   set->quads[0].width           = 1.6f;
   set->quads[0].height          = 0.96f;
   set->quads[0].kind            = VIDEO_XR_QUAD_SCREEN;
   set->num_quads                = 1;
   memset(g, 0, sizeof(*g));
   video_xr_pose_identity(&g->rel);
   g->turn           = g->rel.orientation;
   g->rel.position.y = -0.976f;
   g->rel.position.z = -1.8f;
   g->base.y         = -0.976f;
   g->base.z         = -1.8f;
   g->width          = 1.28f;
   g->height         = 0.96f;
   g->slot           = 1;
   g->state          = VIDEO_XR_GRAB_HELD;
   *start            = g->rel;
}

/* Grabbing a screen does not move it: a 3DS layout's screen 2, taken
 * and held still, stays put and engages nothing, where a whole pull
 * would hinge it and lift it 0.4 cm toward screen 1's bottom edge; the
 * XR thread's call shape agrees. */
static void test_snap_no_hop(void)
{
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, pose, start, away;
   video_screen_xr_t s;
   unsigned snapped = 1;
   snap_3ds(&set, &g, &start);
   video_xr_pose_identity(&aim);
   CHECK(!video_xr_grab_pose(&g, &aim, &set, 2u, &start, &pose, &s,
            &snapped));
   CHECK(snapped == 0 && pose_same(&pose, &start));
   CHECK(NEAR(s.x, 0.0) && NEAR(s.y, 0.0) && NEAR(s.z, 0.0));
   video_xr_grab_pose(&g, &aim, &set, 2u, &start, &pose, NULL, NULL);
   CHECK(pose_same(&pose, &start));
   pose = start;
   CHECK(video_xr_snap(&set, 1, 2u, 1.28f, 0.96f, 0.0f, &pose) == 0
         && !memcmp(&pose, &start, sizeof(pose)));
   /* Had it been taken 1 m away, the pull would be whole. */
   away             = start;
   away.position.x  = 1.0f;
   video_xr_grab_pose(&g, &aim, &set, 2u, &away, &pose, NULL, NULL);
   CHECK(vec_is(&pose.position, 0.0, -0.972, -1.8));
}

/* The fade-in: 0 where the screen was taken, rising evenly with travel
 * or turn, whichever is further on, to 1 at 5 cm or 10 degrees; carried
 * sideways in 0.5 mm steps, the 3DS screen 2 rises toward screen 1's
 * edge as the fade grows, half way at 2.5 cm, no step moving it more
 * than twice the hand's, and is whole by 5 cm. */
static void test_snap_fade_in(void)
{
   unsigned i;
   video_xr_quad_set_t set;
   video_xr_grab_t g;
   video_xr_pose_t aim, pose, start, last;
   snap_3ds(&set, &g, &start);
   pose = start;
   CHECK(video_xr_snap_fade(&start, &pose) == 0.0f);
   pose.position.x = 0.025f;
   CHECK(NEAR(video_xr_snap_fade(&start, &pose), 0.5));
   pose.position.x = 0.05f;
   CHECK(NEAR(video_xr_snap_fade(&start, &pose), 1.0));
   pose.position.x = 0.2f;
   CHECK(video_xr_snap_fade(&start, &pose) == 1.0f);
   /* 1 cm along and turned 5 degrees about its up: the turn counts. */
   pose.position.x    = 0.01f;
   pose.orientation.y = (float)sin(5.0 * M_PI / 360.0);
   pose.orientation.w = (float)cos(5.0 * M_PI / 360.0);
   CHECK(NEAR(video_xr_snap_fade(&start, &pose), 0.5));

   video_xr_pose_identity(&aim);
   last = start;
   for (i = 0; i <= 160; i++)
   {
      aim.position.x = 0.0005f * i;
      video_xr_grab_pose(&g, &aim, &set, 2u, &start, &pose, NULL, NULL);
      CHECK(fabs(pose.position.x - last.position.x) <= 0.00101
            && fabs(pose.position.y - last.position.y) <= 0.00101);
      if (i == 50)
         CHECK(NEAR(pose.position.y, -0.974));
      last = pose;
   }
   CHECK(vec_is(&pose.position, 0.08, -0.972, -1.8));
}

/* An outline round an item: centred on it, facing as it does, its rim's
 * inner edge with filtering's half texel 5 mm outside the item's; about
 * 4 cm thick at the sides and 2 cm at the top and bottom of a
 * 1.6 x 0.9 m screen, as before. */
static void test_outline(void)
{
   video_xr_quad_t q, o;
   const double f = (VIDEO_XR_OUTLINE_EDGE + 0.5)
      / (double)VIDEO_XR_OUTLINE_DIM;
   memset(&q, 0, sizeof(q));
   video_xr_pose_identity(&q.pose);
   q.pose.position.x = 0.3f;
   q.pose.position.z = -1.8f;
   q.pose.orientation.y = 0.38268f;
   q.pose.orientation.w = 0.92388f;
   q.width  = 1.6f;
   q.height = 0.9f;
   q.slot   = 3;
   q.kind   = VIDEO_XR_QUAD_SCREEN;
   video_xr_outline(&q, &o);
   CHECK(NEAR(o.width * (1.0 - 2.0 * f), 1.61));
   CHECK(NEAR(o.height * (1.0 - 2.0 * f), 0.91));
   CHECK(fabs(o.width * VIDEO_XR_OUTLINE_EDGE / VIDEO_XR_OUTLINE_DIM - 0.04)
         < 0.005);
   CHECK(fabs(o.height * VIDEO_XR_OUTLINE_EDGE / VIDEO_XR_OUTLINE_DIM
            - 0.0225) < 0.005);
   CHECK(pose_same(&o.pose, &q.pose));
   CHECK(o.slot == 3 && o.kind == VIDEO_XR_QUAD_SCREEN);
   /* A small item: still 5 mm clear. */
   q.width  = 0.1f;
   q.height = 0.05f;
   video_xr_outline(&q, &o);
   CHECK(NEAR(o.width * (1.0 - 2.0 * f), 0.11));
   CHECK(NEAR(o.height * (1.0 - 2.0 * f), 0.06));
}

/* An outline hiding an edge's rim: VIDEO_XR_OUTLINE_CROP texels cut
 * on its x at that side, the rest where it was, so its other sides
 * stay; a top or bottom turned a quarter about its face first, its
 * sides swapped; turned to the head, left and right swap, so the same
 * side of the item goes; opposite edges both go. 1 x 0.5 m: a cut is
 * 3.125 cm across, 1.5625 cm turned. */
static void test_outline_crop(void)
{
   unsigned x, w;
   video_xr_quad_t o, c;
   video_xr_vec3_t ax, unit_x;
   const unsigned cut = VIDEO_XR_OUTLINE_DIM - VIDEO_XR_OUTLINE_CROP;
   const unsigned both = VIDEO_XR_OUTLINE_DIM - 2 * VIDEO_XR_OUTLINE_CROP;
   CHECK(VIDEO_XR_OUTLINE_CROP > VIDEO_XR_OUTLINE_EDGE);
   memset(&o, 0, sizeof(o));
   video_xr_pose_identity(&o.pose);
   o.pose.position.z = -1.8f;
   o.width           = 1.0f;
   o.height          = 0.5f;
   unit_x.x          = 1.0f;
   unit_x.y          = 0.0f;
   unit_x.z          = 0.0f;
   c = o;
   video_xr_outline_crop(&c, 0, false, &x, &w);
   CHECK(x == 0 && w == VIDEO_XR_OUTLINE_DIM && pose_same(&c.pose, &o.pose)
         && c.width == 1.0f && c.height == 0.5f);
   /* Right: its high texels; its left side stays at x -0.5. */
   c = o;
   video_xr_outline_crop(&c, 1u << 2, false, &x, &w);
   CHECK(x == 0 && w == cut && fabs(c.width - 0.96875) < 1e-6
         && fabs(c.height - 0.5) < 1e-6
         && fabs(c.pose.position.x + 0.015625) < 1e-6
         && turn_apart(&c.pose.orientation, &o.pose.orientation) < 1e-3);
   c = o;
   video_xr_outline_crop(&c, 1u << 3, false, &x, &w);
   CHECK(x == VIDEO_XR_OUTLINE_CROP && w == cut
         && fabs(c.pose.position.x - 0.015625) < 1e-6);
   /* Right, turned to the head: its low texels, the same side in the
    * room; left, its high ones. */
   c = o;
   video_xr_turn_back(&c.pose);
   video_xr_outline_crop(&c, 1u << 2, true, &x, &w);
   CHECK(x == VIDEO_XR_OUTLINE_CROP && w == cut
         && fabs(c.width - 0.96875) < 1e-6
         && fabs(c.pose.position.x + 0.015625) < 1e-6);
   c = o;
   video_xr_turn_back(&c.pose);
   video_xr_outline_crop(&c, 1u << 3, true, &x, &w);
   CHECK(x == 0 && w == cut && fabs(c.width - 0.96875) < 1e-6
         && fabs(c.pose.position.x - 0.015625) < 1e-6);
   /* Top: its x up the item, its high texels; its bottom stays at
    * y -0.25. */
   c = o;
   video_xr_outline_crop(&c, 1u << 0, false, &x, &w);
   video_xr_rotate(&c.pose.orientation, &unit_x, &ax);
   CHECK(x == 0 && w == cut && vec_is(&ax, 0.0, 1.0, 0.0)
         && fabs(c.width - 0.484375) < 1e-6 && fabs(c.height - 1.0) < 1e-6
         && fabs(c.pose.position.y + 0.0078125) < 1e-6
         && fabs(c.pose.position.x) < 1e-6);
   c = o;
   video_xr_outline_crop(&c, 1u << 1, false, &x, &w);
   CHECK(x == VIDEO_XR_OUTLINE_CROP
         && fabs(c.pose.position.y - 0.0078125) < 1e-6);
   /* Top, turned to the head: still its high texels; bottom, its low
    * ones. */
   c = o;
   video_xr_turn_back(&c.pose);
   video_xr_outline_crop(&c, 1u << 0, true, &x, &w);
   video_xr_rotate(&c.pose.orientation, &unit_x, &ax);
   CHECK(x == 0 && vec_is(&ax, 0.0, 1.0, 0.0)
         && fabs(c.pose.position.y + 0.0078125) < 1e-6);
   c = o;
   video_xr_turn_back(&c.pose);
   video_xr_outline_crop(&c, 1u << 1, true, &x, &w);
   video_xr_rotate(&c.pose.orientation, &unit_x, &ax);
   CHECK(x == VIDEO_XR_OUTLINE_CROP && w == cut && vec_is(&ax, 0.0, 1.0, 0.0)
         && fabs(c.pose.position.y - 0.0078125) < 1e-6);
   /* Opposite edges: both sides, the middle kept. */
   c = o;
   video_xr_outline_crop(&c, 3u, false, &x, &w);
   CHECK(x == VIDEO_XR_OUTLINE_CROP && w == both
         && fabs(c.width - 0.46875) < 1e-6
         && fabs(c.pose.position.y) < 1e-6);
   c = o;
   video_xr_outline_crop(&c, 12u, false, &x, &w);
   CHECK(x == VIDEO_XR_OUTLINE_CROP && w == both
         && fabs(c.width - 0.9375) < 1e-6
         && fabs(c.pose.position.x) < 1e-6);
}

/* At full width an outline cut keeps its pose and size and shows its
 * cut's cell: none, the whole square; from behind, a side is the room's
 * same side, the other cell; a top or bottom wins over a side. */
static void test_outline_cell(void)
{
   unsigned k;
   for (k = 0; k < 2; k++)
   {
      CHECK(video_xr_outline_cell(0, false, k) == VIDEO_XR_OUTLINE_X(k));
      CHECK(video_xr_outline_cell(0, true, k) == VIDEO_XR_OUTLINE_X(k));
      CHECK(video_xr_outline_cell(1u, false, k) == VIDEO_XR_CUT_X(0, k));
      CHECK(video_xr_outline_cell(2u, false, k) == VIDEO_XR_CUT_X(1, k));
      CHECK(video_xr_outline_cell(4u, false, k) == VIDEO_XR_CUT_X(2, k));
      CHECK(video_xr_outline_cell(8u, false, k) == VIDEO_XR_CUT_X(3, k));
      CHECK(video_xr_outline_cell(3u, false, k) == VIDEO_XR_CUT_X(4, k));
      CHECK(video_xr_outline_cell(12u, false, k) == VIDEO_XR_CUT_X(5, k));
      CHECK(video_xr_outline_cell(1u, true, k) == VIDEO_XR_CUT_X(0, k));
      CHECK(video_xr_outline_cell(3u, true, k) == VIDEO_XR_CUT_X(4, k));
      CHECK(video_xr_outline_cell(4u, true, k) == VIDEO_XR_CUT_X(3, k));
      CHECK(video_xr_outline_cell(8u, true, k) == VIDEO_XR_CUT_X(2, k));
      CHECK(video_xr_outline_cell(12u, true, k) == VIDEO_XR_CUT_X(5, k));
      CHECK(video_xr_outline_cell(2u | 4u, false, k) == VIDEO_XR_CUT_X(1, k));
   }
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
   test_layout();
   test_settings_identity();
   test_grab();
   test_grab_stick();
   test_grab_turn();
   test_grab_turn_pitched();
   test_grab_turn_lowered();
   test_grab_turn_tilt_ray();
   test_carry_heading();
   test_carry_full_turn();
   test_grab_tilt();
   test_grab_tilt_evenly();
   test_grab_turn_and_tilt();
   test_grab_turn_lock();
   test_grab_tilt_turned();
   test_tilt_full_turn();
   test_carry_vertical();
   test_carry_flip();
   test_carry_still();
   test_menu_handle();
   test_handle_first();
   test_handle_sticky();
   test_snap_lines();
   test_snap_pulling();
   test_snap_zones();
   test_snap_targets();
   test_snap_continuous();
   test_snap_ranked();
   test_snap_whole_line();
   test_snap_turn_tie();
   test_snap_turned();
   test_hinge_3ds();
   test_hinge_square_fold();
   test_hinge_side();
   test_hinge_overlap();
   test_hinge_range();
   test_hinge_flush_corner();
   test_hinge_pivot();
   test_hinge_ranked();
   test_hinge_turned_away();
   test_hinge_outranked();
   test_hinge_continuous();
   test_hinge_roll_band();
   test_hinge_whole();
   test_hinge_swing();
   test_hinge_other_axis();
   test_hinge_step_put();
   test_hinge_put_still();
   test_gap_click();
   test_hinge_line();
   test_hinge_place();
   test_gap_free();
   test_gap_fade_out();
   test_gap_stop();
   test_gap_stop_one();
   test_gap_stop_arm();
   test_gap_stop_arm_turned();
   test_gap_stop_turned();
   test_gap_swing();
   test_gap_nearer();
   test_gap_nearer_sweep();
   test_gap_nearer_stacked();
   test_gap_nearer_corner();
   test_gap_commit();
   test_gap_limits();
   test_gap_sweeps();
   test_grab_snap();
   test_turn_snap();
   test_tilt_snap();
   test_step_turn();
   test_step_tilt();
   test_step_flat();
   test_step_snap_first();
   test_step_lid();
   test_turn_lid();
   test_near_hinge();
   test_rest_hinges();
   test_rest_hinge();
   test_grab_kept();
   test_grab_through();
   test_grab_through_rest();
   test_rest_gap_one();
   test_rest_hinge_sides();
   test_rest_hinge_size();
   test_hinge_settle();
   test_hinge_settle_taken();
   test_hinge_fold_rest();
   test_grab_kept_turn();
   test_smooth_steady();
   test_smooth_tremor();
   test_smooth_sweep();
   test_smooth_rates();
   test_smooth_resets();
   test_snap_no_hop();
   test_snap_fade_in();
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
   test_beam();
   test_cursor_pixels();
   test_tag_pixels();
   test_cut_pixels();
   test_back_pixels();
   test_hinge_pixels();
   test_back_pick();
   test_tag_pose();
   test_outline();
   test_outline_crop();
   test_outline_cell();
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
