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

#include <stdio.h>
#include <string.h>

#include "gfx/video_windows.h"
#include "gfx/video_defines.h"

static unsigned failures;
#define CHECK(cond) do { if (!(cond)) { \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

static void mono_map(video_views_map_t *map, unsigned count)
{
   unsigned i;
   memset(map, 0, sizeof(*map));
   for (i = 0; i < count; i++)
   {
      map->views[i].screen = i;
      map->views[i].y = i * 100;
      map->views[i].width = map->views[i].height = 100;
   }
   map->num_views = map->num_screens = count;
}

static void test_windows_policy(void)
{
   video_views_map_t map;
   video_window_settings_t screens[RETRO_VIDEO_VIEWS_MAX];
   video_windows_plan_t plan;
   unsigned i;
   memset(screens, 0, sizeof(screens));
   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
      screens[i].mode = VIDEO_WINDOW_SEPARATE;
   mono_map(&map, 3);
   video_windows_plan(&map, screens, true, true, 0, 0, &plan);
   CHECK(plan.main_mask == 1u && plan.separate_mask == 6u);
   video_windows_plan(&map, screens, true, true, 2u, 0, &plan);
   CHECK(plan.main_mask == 3u && plan.separate_mask == 4u);
   screens[2].mode = VIDEO_WINDOW_HIDDEN;
   video_windows_plan(&map, screens, true, true, 4u, 0, &plan);
   CHECK(plan.hidden_mask == 4u && plan.separate_mask == 2u);
   video_windows_plan(&map, screens, false, true, 0, 0, &plan);
   CHECK(plan.main_mask == 7u && plan.hidden_mask == 0u);
   video_windows_plan(&map, screens, true, false, 0, 0, &plan);
   CHECK(plan.main_mask == 3u && plan.separate_mask == 0u && plan.hidden_mask == 4u);
   screens[0].mode = VIDEO_WINDOW_HIDDEN;
   screens[1].mode = VIDEO_WINDOW_ATTACHED;
   video_windows_plan(&map, screens, true, true, 0, 0, &plan);
   CHECK(plan.main_mask == 3u && plan.hidden_mask == 4u);
   screens[1].mode = 99;
   video_windows_plan(&map, screens, true, true, 0, 0, &plan);
   CHECK(plan.main_mask == 3u);
   mono_map(&map, 0);
   video_windows_plan(&map, screens, true, true, 0, 0, &plan);
   CHECK(plan.main_mask == 0u && plan.separate_mask == 0u && plan.hidden_mask == 0u);
   mono_map(&map, RETRO_VIDEO_VIEWS_MAX);
   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
      screens[i].mode = VIDEO_WINDOW_SEPARATE;
   video_windows_plan(&map, screens, true, true, 0, 0, &plan);
   CHECK(plan.main_mask == 1u && plan.separate_mask == 254u);
}

/* Screen @attached stays in the main window, and screen 0 takes its
 * place: the external monitor rule */
static void test_windows_attached(void)
{
   video_views_map_t map;
   video_window_settings_t screens[RETRO_VIDEO_VIEWS_MAX];
   video_windows_plan_t plan;
   unsigned i;
   memset(screens, 0, sizeof(screens));
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
      screens[i].mode = VIDEO_WINDOW_SEPARATE;
   mono_map(&map, 3);
   video_windows_plan(&map, screens, true, true, 0, 1, &plan);
   CHECK(plan.main_mask == 2u && plan.separate_mask == 5u);
   CHECK(plan.hidden_mask == 0u);
   /* A close of screen 0's window puts it back */
   video_windows_plan(&map, screens, true, true, 1u, 1, &plan);
   CHECK(plan.main_mask == 3u && plan.separate_mask == 4u);
   /* Screen 0 has the attached screen's mode */
   screens[1].mode = VIDEO_WINDOW_ATTACHED;
   video_windows_plan(&map, screens, true, true, 0, 1, &plan);
   CHECK(plan.main_mask == 3u && plan.separate_mask == 4u);
   screens[1].mode = VIDEO_WINDOW_HIDDEN;
   video_windows_plan(&map, screens, true, true, 0, 1, &plan);
   CHECK(plan.main_mask == 2u && plan.hidden_mask == 1u);
   CHECK(plan.separate_mask == 4u);
   screens[1].mode = VIDEO_WINDOW_SEPARATE;
   /* Off, or no window support: every screen in the main window */
   video_windows_plan(&map, screens, false, true, 0, 1, &plan);
   CHECK(plan.main_mask == 7u);
   video_windows_plan(&map, screens, true, false, 0, 1, &plan);
   CHECK(plan.main_mask == 7u && plan.separate_mask == 0u);
   /* A screen the map does not have: screen 0 stays */
   mono_map(&map, 1);
   video_windows_plan(&map, screens, true, true, 0, 1, &plan);
   CHECK(plan.main_mask == 1u && plan.separate_mask == 0u);
   mono_map(&map, 3);
   video_windows_plan(&map, screens, true, true, 0, 5, &plan);
   CHECK(plan.main_mask == 1u && plan.separate_mask == 6u);
   video_windows_plan(&map, screens, true, true, 0, 99, &plan);
   CHECK(plan.main_mask == 1u && plan.separate_mask == 6u);
   /* Screen 2 attached: screen 0 takes its mode */
   screens[2].mode = VIDEO_WINDOW_HIDDEN;
   video_windows_plan(&map, screens, true, true, 0, 2, &plan);
   CHECK(plan.main_mask == 4u && plan.hidden_mask == 1u);
   CHECK(plan.separate_mask == 2u);
   /* No settings: screen 0 attached too */
   video_windows_plan(&map, NULL, true, true, 0, 1, &plan);
   CHECK(plan.main_mask == 7u && plan.separate_mask == 0u && plan.hidden_mask == 0u);
}

static void test_display(video_window_display_t *d, unsigned ordinal,
      unsigned connection, bool own)
{
   memset(d, 0, sizeof(*d));
   d->ordinal    = ordinal;
   d->connection = connection;
   d->own        = own;
   d->valid      = true;
   d->on         = true;
   d->physical   = true;
}

static bool test_routed(const video_windows_route_t *r, unsigned display,
      unsigned screens, unsigned unplaced, unsigned attached)
{
   if (     r->display == display && r->screens == screens
         && r->unplaced == unplaced && r->attached == attached)
      return true;
   printf("routed display %u screens %u unplaced %u attached %u\n",
         r->display, r->screens, r->unplaced, r->attached);
   return false;
}

/* Which display the separate screens go to */
static void test_windows_route(void)
{
   video_window_display_t d[4];
   video_window_settings_t screens[RETRO_VIDEO_VIEWS_MAX];
   video_windows_route_t r;
   unsigned i;
   memset(screens, 0, sizeof(screens));
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
      screens[i].mode = VIDEO_WINDOW_SEPARATE;

   /* A built-in second panel takes screen 1 */
   test_display(&d[0], 1, VIDEO_WINDOW_CONNECTION_BUILT_IN, true);
   test_display(&d[1], 2, VIDEO_WINDOW_CONNECTION_BUILT_IN, false);
   video_windows_route(d, 2, screens, 2, &r);
   CHECK(test_routed(&r, 2, 2u, 0u, 0));

   /* The Thor: both panels' connection is unknown; a size changes
    * nothing */
   test_display(&d[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN, true);
   test_display(&d[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN, false);
   d[0].dims = VIDEO_SCALE_PACK(1920, 1080);
   d[1].dims = VIDEO_SCALE_PACK(1240, 1080);
   video_windows_route(d, 2, screens, 2, &r);
   CHECK(test_routed(&r, 2, 2u, 0u, 0));

   /* The Thor with a monitor: the DIRECT display beats Screen-2 and
    * takes screen 0, and screen 1 stays */
   test_display(&d[2], 3, VIDEO_WINDOW_CONNECTION_DIRECT, false);
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 3, 1u, 0u, 1));
   d[2].connection = VIDEO_WINDOW_CONNECTION_TRANSITIVE;
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 3, 1u, 0u, 1));
   /* ahead of a built-in one listed after it */
   test_display(&d[0], 1, VIDEO_WINDOW_CONNECTION_DIRECT, false);
   test_display(&d[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN, true);
   test_display(&d[2], 3, VIDEO_WINDOW_CONNECTION_BUILT_IN, false);
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 1, 1u, 0u, 1));
   /* Every Auto screen goes with screen 0 */
   video_windows_route(d, 3, screens, 3, &r);
   CHECK(test_routed(&r, 1, 5u, 0u, 1));

   /* No product info, as a cast or below API 31: not for Auto */
   test_display(&d[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN, true);
   test_display(&d[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN, false);
   d[1].physical = false;
   video_windows_route(d, 2, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));
   d[0].physical = false;
   test_display(&d[2], 3, VIDEO_WINDOW_CONNECTION_UNKNOWN, false);
   d[2].physical = false;
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));

   /* Private, off, invalid and the main window's own: not for Auto */
   test_display(&d[1], 2, VIDEO_WINDOW_CONNECTION_BUILT_IN, false);
   d[1].is_private = true;
   video_windows_route(d, 2, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));
   d[1].is_private = false;
   d[1].on         = false;
   video_windows_route(d, 2, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));
   d[1].on         = true;
   d[1].valid      = false;
   video_windows_route(d, 2, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));
   d[1].valid      = true;
   d[1].own        = true;
   video_windows_route(d, 2, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));
   video_windows_route(d, 0, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));
   video_windows_route(NULL, 0, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));

   /* Several: the lowest ordinal, whatever the list's order */
   test_display(&d[0], 4, VIDEO_WINDOW_CONNECTION_BUILT_IN, false);
   test_display(&d[1], 1, VIDEO_WINDOW_CONNECTION_BUILT_IN, true);
   test_display(&d[2], 3, VIDEO_WINDOW_CONNECTION_BUILT_IN, false);
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 3, 2u, 0u, 0));
   d[0].connection = d[2].connection = VIDEO_WINDOW_CONNECTION_DIRECT;
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 3, 1u, 0u, 1));
   /* No ordinal: not for Auto, even external and listed last */
   test_display(&d[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN, true);
   test_display(&d[1], 3, VIDEO_WINDOW_CONNECTION_BUILT_IN, false);
   test_display(&d[2], 0, VIDEO_WINDOW_CONNECTION_DIRECT, false);
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 3, 2u, 0u, 0));

   /* The picker: display N takes the screen that names it, and the
    * external rule is Auto's alone */
   test_display(&d[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN, true);
   test_display(&d[1], 2, VIDEO_WINDOW_CONNECTION_BUILT_IN, false);
   test_display(&d[2], 3, VIDEO_WINDOW_CONNECTION_DIRECT, false);
   screens[1].monitor = 2;
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 2, 2u, 0u, 0));
   screens[1].monitor = 3;
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 3, 2u, 0u, 0));
   /* even without product info, and below API 31 */
   d[1].physical = false;
   screens[1].monitor = 2;
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 2, 2u, 0u, 0));
   d[1].physical = true;
   /* or off */
   d[1].on = false;
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 2, 2u, 0u, 0));
   d[1].on = true;
   /* N the main window's own display, or missing: the screen stays */
   screens[1].monitor = 1;
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));
   screens[1].monitor = 7;
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));
   d[1].valid = false;
   screens[1].monitor = 2;
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));
   d[1].valid = true;
   d[1].is_private = true;
   video_windows_route(d, 3, screens, 2, &r);
   CHECK(test_routed(&r, 0, 0u, 2u, 0));
   d[1].is_private = false;

   /* One display at a time: a screen bound for another stays */
   screens[1].monitor = 2;
   screens[2].monitor = 3;
   video_windows_route(d, 3, screens, 3, &r);
   CHECK(test_routed(&r, 2, 2u, 4u, 0));
   /* Auto screen 1 on the monitor, screen 2 named to it: shared */
   screens[1].monitor = 0;
   video_windows_route(d, 3, screens, 3, &r);
   CHECK(test_routed(&r, 3, 5u, 0u, 1));
   screens[2].monitor = 0;

   /* Only Separate screens go; screen 0 only by the external rule */
   screens[1].mode = VIDEO_WINDOW_ATTACHED;
   screens[2].mode = VIDEO_WINDOW_HIDDEN;
   video_windows_route(d, 3, screens, 3, &r);
   CHECK(test_routed(&r, 0, 0u, 0u, 0));
   screens[1].mode = screens[2].mode = VIDEO_WINDOW_SEPARATE;
   video_windows_route(d, 3, screens, 1, &r);
   CHECK(test_routed(&r, 0, 0u, 0u, 0));
   video_windows_route(d, 3, NULL, 3, &r);
   CHECK(test_routed(&r, 0, 0u, 0u, 0));
   video_windows_route(d, 3, screens, 99, &r);
   CHECK(test_routed(&r, 3, 0xfdu, 0u, 1));
}

static void test_selected_layout(void)
{
   video_views_map_t map;
   video_views_layout_params_t p;
   video_views_layout_t layout;
   video_views_rect_t custom;
   uint32_t point;
   mono_map(&map, 3);
   /* Screen 1's right eye precedes its left in the packed source. */
   map.views[3] = map.views[1];
   map.views[3].eye = RETRO_VIDEO_VIEW_EYE_LEFT;
   map.views[3].x = 100;
   map.views[1].eye = RETRO_VIDEO_VIEW_EYE_RIGHT;
   map.num_views = 4;
   memset(&p, 0, sizeof(p));
   p.map = &map;
   p.dims = VIDEO_SCALE_PACK(650, 450);
   p.exclude_screens = 5u;
   p.scale_integer = true;
   video_views_layout(&p, &layout);
   CHECK(layout.num_placements == 1 && layout.placements[0].view == 3);
   CHECK(layout.placements[0].dst.dims == VIDEO_SCALE_PACK(400, 400));
   CHECK(layout.placements[0].dst.pos == VIDEO_POS_PACK(125, 25));
   CHECK(video_views_map_point(&layout, &map, VIDEO_SCALE_PACK(200, 300),
            325, 225, false, &point));
   CHECK(VIDEO_POS_X(point) > 16500 && VIDEO_POS_X(point) < 16700);
   CHECK(VIDEO_POS_Y(point) > 0 && VIDEO_POS_Y(point) < 200);
   p.scale_integer = false;
   p.stretch = true;
   video_views_layout(&p, &layout);
   CHECK(layout.placements[0].dst.dims == p.dims);
   p.stretch = false;
   p.single_aspect = 2.0f;
   video_views_layout(&p, &layout);
   CHECK(layout.placements[0].dst.dims == VIDEO_SCALE_PACK(650, 325));
   custom.pos = VIDEO_POS_PACK(-20, 30);
   custom.dims = VIDEO_SCALE_PACK(300, 200);
   p.custom_vp = &custom;
   video_views_layout(&p, &layout);
   CHECK(layout.placements[0].dst.pos == custom.pos);
   CHECK(layout.placements[0].dst.dims == custom.dims);
   p.custom_vp = NULL;
   p.single_aspect = 0.0f;
   p.stereo_mode = VIDEO_STEREO_MODE_SBS_FULL;
   video_views_layout(&p, &layout);
   CHECK(layout.num_placements == 2);
   CHECK(layout.placements[0].view == 3 && layout.placements[1].view == 1);
   p.swap_eyes = true;
   video_views_layout(&p, &layout);
   CHECK(layout.placements[0].view == 1 && layout.placements[1].view == 3);
   p.exclude_screens = 7u;
   video_views_layout(&p, &layout);
   CHECK(layout.num_placements == 0 && layout.num_areas == 0 && layout.canvas_dims == 0);
   p.stereo_mode = VIDEO_STEREO_MODE_2D;
   p.exclude_screens = 2u;
   video_views_layout(&p, &layout);
   CHECK(layout.num_placements == 2);
   CHECK(layout.placements[0].view == 0 && layout.placements[1].view == 2);
   CHECK(layout.placements[0].dst.dims == VIDEO_SCALE_PACK(225, 225));
   CHECK(layout.placements[1].dst.pos == VIDEO_POS_PACK(213, 225));
}

static void stacked_map(video_views_map_t *map, unsigned count)
{
   unsigned i;
   memset(map, 0, sizeof(*map));
   for (i = 0; i < count; i++)
   {
      map->views[i].screen = i;
      map->views[i].y      = i * 192;
      map->views[i].width  = 256;
      map->views[i].height = 192;
   }
   map->num_views = map->num_screens = count;
}

/* A touch on a second display, through the layout its output draws
 * there, lands in the touched screen's part of the packed frame */
static void test_second_output_touch(void)
{
   const uint32_t oob = VIDEO_POS_PACK(-0x8000, -0x8000);
   video_views_map_t map;
   video_views_layout_params_t p;
   video_views_layout_t layout;
   uint32_t point = 0;

   /* Two screens, the lower on a 1240x1080 lower panel */
   stacked_map(&map, 2);
   memset(&p, 0, sizeof(p));
   p.map             = &map;
   p.dims            = VIDEO_SCALE_PACK(1240, 1080);
   p.exclude_screens = ~2u;
   video_views_layout(&p, &layout);
   CHECK(layout.num_placements == 1 && layout.placements[0].view == 1);
   CHECK(layout.placements[0].dst.dims == VIDEO_SCALE_PACK(1240, 930));
   CHECK(layout.placements[0].dst.pos == VIDEO_POS_PACK(0, 75));
   CHECK(video_views_map_point(&layout, &map, VIDEO_SCALE_PACK(256, 384),
            620, 540, true, &point));
   CHECK(VIDEO_POS_X(point) > -300 && VIDEO_POS_X(point) < 300);
   CHECK(VIDEO_POS_Y(point) > 16000 && VIDEO_POS_Y(point) < 16800);
   CHECK(video_views_map_point(&layout, &map, VIDEO_SCALE_PACK(256, 384),
            0, 75, true, &point));
   CHECK(VIDEO_POS_X(point) < -32500);
   CHECK(VIDEO_POS_Y(point) > -200 && VIDEO_POS_Y(point) < 200);
   /* The letterbox: off-screen, or held to the screen */
   CHECK(video_views_map_point(&layout, &map, VIDEO_SCALE_PACK(256, 384),
            620, 10, true, &point));
   CHECK(point == oob);
   CHECK(video_views_map_point(&layout, &map, VIDEO_SCALE_PACK(256, 384),
            620, 10, false, &point));
   CHECK(VIDEO_POS_Y(point) > -200 && VIDEO_POS_Y(point) < 200);

   /* Screens 1 and 2 share the display, one above the other */
   stacked_map(&map, 3);
   p.exclude_screens = ~6u;
   video_views_layout(&p, &layout);
   CHECK(layout.num_placements == 2);
   CHECK(layout.placements[0].view == 1 && layout.placements[1].view == 2);
   CHECK(layout.placements[0].dst.pos == VIDEO_POS_PACK(260, 0));
   CHECK(layout.placements[1].dst.pos == VIDEO_POS_PACK(260, 540));
   CHECK(video_views_map_point(&layout, &map, VIDEO_SCALE_PACK(256, 576),
            620, 270, true, &point));
   CHECK(VIDEO_POS_Y(point) > -300 && VIDEO_POS_Y(point) < 300);
   CHECK(video_views_map_point(&layout, &map, VIDEO_SCALE_PACK(256, 576),
            620, 810, true, &point));
   CHECK(VIDEO_POS_Y(point) > 21500 && VIDEO_POS_Y(point) < 22200);
   CHECK(video_views_map_point(&layout, &map, VIDEO_SCALE_PACK(256, 576),
            100, 540, true, &point));
   CHECK(point == oob);
   /* held to the nearer screen */
   CHECK(video_views_map_point(&layout, &map, VIDEO_SCALE_PACK(256, 576),
            100, 810, false, &point));
   CHECK(VIDEO_POS_X(point) < -32500);
   CHECK(VIDEO_POS_Y(point) > 21500 && VIDEO_POS_Y(point) < 22200);

   /* Screen 0 on an external monitor */
   stacked_map(&map, 2);
   p.dims            = VIDEO_SCALE_PACK(1920, 1080);
   p.exclude_screens = ~1u;
   video_views_layout(&p, &layout);
   CHECK(layout.num_placements == 1 && layout.placements[0].view == 0);
   CHECK(layout.placements[0].dst.pos == VIDEO_POS_PACK(240, 0));
   CHECK(video_views_map_point(&layout, &map, VIDEO_SCALE_PACK(256, 384),
            960, 540, true, &point));
   CHECK(VIDEO_POS_Y(point) > -16800 && VIDEO_POS_Y(point) < -16000);
}

/* Native layers' bookkeeping: two windows of one owner, one of another */
static video_window_common_t test_windows[RETRO_VIDEO_VIEWS_MAX];
static video_window_native_t test_natives[RETRO_VIDEO_VIEWS_MAX];
static unsigned test_destroyed;

static video_window_common_t *test_window_at(unsigned screen)
{
   return test_windows[screen].native ? &test_windows[screen] : NULL;
}

static void test_window_destroy(void *owner, video_window_native_t *window)
{
   test_destroyed |= 1u << (unsigned)(window - test_natives);
   test_windows[window - test_natives].native = NULL;
}

static void test_window_events(void)
{
   video_window_event_t events[8];
   unsigned i, n, seen;
   int a = 0, b = 0;
   memset(test_windows, 0, sizeof(test_windows));
   memset(test_natives, 0, sizeof(test_natives));
   for (i = 1; i <= 3; i++)
   {
      test_windows[i].owner              = i == 3 ? (void*)&b : (void*)&a;
      test_windows[i].native             = &test_natives[i];
      test_windows[i].request.screen     = i;
      test_windows[i].request.generation = 10 + i;
      test_windows[i].pos                = VIDEO_POS_PACK(i, -1);
      test_natives[i].dims               = VIDEO_SCALE_PACK(100 * i, 50);
      test_natives[i].focused            = true;
      test_natives[i].presentable        = true;
   }

   /* A close takes a waiting gain and resume with it, and the focus */
   test_windows[1].pending = VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN)
      | VIDEO_WINDOW_EVENT_BIT(RESUME) | VIDEO_WINDOW_EVENT_BIT(GEOMETRY);
   video_window_mark_closed(&test_windows[1]);
   CHECK(test_windows[1].closed);
   CHECK(test_windows[1].pending == (VIDEO_WINDOW_EVENT_BIT(CLOSE)
            | VIDEO_WINDOW_EVENT_BIT(FOCUS_LOSS)
            | VIDEO_WINDOW_EVENT_BIT(GEOMETRY)));
   CHECK(!test_natives[1].focused && !test_natives[1].presentable);
   test_windows[1].pending = 0;
   video_window_mark_closed(&test_windows[1]);
   CHECK(test_windows[1].pending == 0);
   test_natives[2].focused = false;
   video_window_mark_closed(&test_windows[2]);
   CHECK(test_windows[2].pending == VIDEO_WINDOW_EVENT_BIT(CLOSE));

   /* Drained in order, tagged, and what has no room waits */
   test_windows[1].pending = VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN)
      | VIDEO_WINDOW_EVENT_BIT(RESUME) | VIDEO_WINDOW_EVENT_BIT(SUSPEND)
      | VIDEO_WINDOW_EVENT_BIT(GEOMETRY) | VIDEO_WINDOW_EVENT_BIT(CLOSE)
      | VIDEO_WINDOW_EVENT_BIT(FOCUS_LOSS);
   test_windows[3].pending = VIDEO_WINDOW_EVENT_BIT(CLOSE);
   CHECK(video_windows_drain(&a, test_window_at, NULL, 4) == 0);
   CHECK(video_windows_drain(&a, test_window_at, events, 0) == 0);
   n = video_windows_drain(&a, test_window_at, events, 4);
   CHECK(n == 4);
   CHECK(events[0].kind == VIDEO_WINDOW_EVENT_FOCUS_LOSS);
   CHECK(events[1].kind == VIDEO_WINDOW_EVENT_CLOSE);
   CHECK(events[2].kind == VIDEO_WINDOW_EVENT_GEOMETRY);
   CHECK(events[3].kind == VIDEO_WINDOW_EVENT_SUSPEND);
   CHECK(events[0].screen == 1 && events[0].generation == 11);
   CHECK(events[2].pos == VIDEO_POS_PACK(1, -1));
   CHECK(events[2].dims == VIDEO_SCALE_PACK(100, 50));
   for (i = 0, seen = 0; i < 4; i++)
   {
      if (!(n = video_windows_drain(&a, test_window_at, events, 1)))
         break;
      seen |= 1u << events[0].kind;
      CHECK(events[0].screen != 3);
   }
   CHECK(seen == (VIDEO_WINDOW_EVENT_BIT(RESUME)
            | VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN)
            | VIDEO_WINDOW_EVENT_BIT(CLOSE)));
   CHECK(test_windows[3].pending == VIDEO_WINDOW_EVENT_BIT(CLOSE));

   /* Only the owner's windows go */
   test_destroyed = 0;
   video_windows_sweep(&a, test_window_at, test_window_destroy);
   CHECK(test_destroyed == 6u);
   CHECK(test_window_at(3) != NULL);
}

static bool test_place(int x, int y, int w, int h, bool automatic,
      int ax, int ay, int aw, int ah, int wx, int wy, int ww, int wh)
{
   video_window_rect_t rect, area;
   rect.x = x;
   rect.y = y;
   rect.w = w;
   rect.h = h;
   area.x = ax;
   area.y = ay;
   area.w = aw;
   area.h = ah;
   video_window_place_rect(&rect, &area, automatic);
   if (rect.x == wx && rect.y == wy && rect.w == ww && rect.h == wh)
      return true;
   printf("placed %d,%d %dx%d, wanted %d,%d %dx%d\n",
         rect.x, rect.y, rect.w, rect.h, wx, wy, ww, wh);
   return false;
}

/* Saved bounds on the monitors there are now */
static void test_window_place(void)
{
   video_window_rect_t a, b;
   /* On it: kept */
   CHECK(test_place(100, 100, 320, 240, false, 0, 0, 1280, 720,
            100, 100, 320, 240));
   /* Automatic: centred, whatever the position */
   CHECK(test_place(100, 100, 320, 240, true, 0, 0, 1280, 720,
            480, 240, 320, 240));
   CHECK(test_place(100, 100, 320, 240, true, 1280, 0, 1280, 720,
            1760, 240, 320, 240));
   /* Partly off: moved onto it, each edge */
   CHECK(test_place(-200, -200, 320, 240, false, 0, 0, 1280, 720,
            0, 0, 320, 240));
   CHECK(test_place(1100, 600, 320, 240, false, 0, 0, 1280, 720,
            960, 480, 320, 240));
   /* Not on it at all, or only touching it: centred */
   CHECK(test_place(-30000, -30000, 320, 240, false, 0, 0, 1280, 720,
            480, 240, 320, 240));
   CHECK(test_place(1280, 0, 320, 240, false, 0, 0, 1280, 720,
            480, 240, 320, 240));
   /* An area that does not start at 0, as a work area */
   CHECK(test_place(0, 0, 320, 240, false, 1280, 30, 1280, 690,
            1760, 255, 320, 240));
   CHECK(test_place(2500, 700, 320, 240, false, 1280, 30, 1280, 690,
            2240, 480, 320, 240));
   /* Larger than it: clamped to it */
   CHECK(test_place(100, 100, 2000, 1000, false, 0, 0, 1280, 720,
            0, 0, 1280, 720));
   /* Partly on it as saved, though not once clamped: moved, not
    * centred */
   CHECK(test_place(-1500, 100, 2000, 200, false, 0, 0, 1280, 720,
            0, 100, 1280, 200));

   a.x = 0;
   a.y = 0;
   a.w = 100;
   a.h = 50;
   b   = a;
   CHECK(video_window_rect_overlap(&a, &b) == 5000);
   b.x = 90;
   b.y = -10;
   CHECK(video_window_rect_overlap(&a, &b) == 400);
   b.x = 100;
   CHECK(video_window_rect_overlap(&a, &b) == 0);
   b.x = -30000;
   b.y = -30000;
   CHECK(video_window_rect_overlap(&a, &b) == 0);
}

/* A renderer's windows across frames */
static void test_windows_steps(void)
{
   video_windows_frame_t frame;
   video_windows_held_t held;
   unsigned retire, create;
   memset(&frame, 0, sizeof(frame));
   memset(&held, 0, sizeof(held));

   /* Screen 1 on display 2, its window id 1: made */
   frame.display           = 2;
   frame.restore           = 5;
   frame.window_screens[1] = 1u << 1;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0 && create == 2u);
   held.live       = 2u;
   held.display[1] = 2;
   /* Kept while nothing changes */
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0 && create == 0);
   /* Screens 0 and 1 share it: still kept */
   frame.window_screens[1] = 3u;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0 && create == 0);
   /* Another display: let go of and made again */
   frame.display = 3;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 2u && create == 2u);
   held.display[1] = 3;
   /* No screens: let go of */
   frame.window_screens[1] = 0;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 2u && create == 0);
   /* Windows off: every live one let go of */
   held.live = 6u;
   video_windows_steps(&held, NULL, &retire, &create);
   CHECK(retire == 6u && create == 0);
   held.live = 0;

   /* A failure stays out until the restore count moves */
   frame.window_screens[1] = 2u;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(create == 2u);
   held.failed |= 2u;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0 && create == 0);
   frame.restore = 6;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(create == 2u && held.failed == 0);
   /* ... or the id has no screens */
   held.failed = 2u;
   frame.window_screens[1] = 0;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(held.failed == 0 && create == 0);
   /* ... or windows go off */
   held.failed = 2u;
   video_windows_steps(&held, NULL, &retire, &create);
   CHECK(held.failed == 0);

   /* A window a screen, as on a desktop: ids 1 and 2 */
   frame.display           = 0;
   frame.window_screens[1] = 1u << 1;
   frame.window_screens[2] = 1u << 2;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0 && create == 6u);
   held.live       = 2u;
   held.display[1] = 0;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0 && create == 4u);
}

int main(void)
{
   test_windows_policy();
   test_windows_attached();
   test_windows_route();
   test_selected_layout();
   test_second_output_touch();
   test_window_events();
   test_window_place();
   test_windows_steps();
   if (failures)
      return 1;
   puts("video_windows: ok");
   return 0;
}
