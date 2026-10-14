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
   video_windows_plan(&map, screens, true, true, 0, &plan);
   CHECK(plan.main_mask == 1u && plan.separate_mask == 6u);
   video_windows_plan(&map, screens, true, true, 2u, &plan);
   CHECK(plan.main_mask == 3u && plan.separate_mask == 4u);
   screens[2].mode = VIDEO_WINDOW_HIDDEN;
   video_windows_plan(&map, screens, true, true, 4u, &plan);
   CHECK(plan.hidden_mask == 4u && plan.separate_mask == 2u);
   video_windows_plan(&map, screens, false, true, 0, &plan);
   CHECK(plan.main_mask == 7u && plan.hidden_mask == 0u);
   video_windows_plan(&map, screens, true, false, 0, &plan);
   CHECK(plan.main_mask == 3u && plan.separate_mask == 0u && plan.hidden_mask == 4u);
   screens[0].mode = VIDEO_WINDOW_HIDDEN;
   screens[1].mode = VIDEO_WINDOW_ATTACHED;
   video_windows_plan(&map, screens, true, true, 0, &plan);
   CHECK(plan.main_mask == 3u && plan.hidden_mask == 4u);
   screens[1].mode = 99;
   video_windows_plan(&map, screens, true, true, 0, &plan);
   CHECK(plan.main_mask == 3u);
   mono_map(&map, 0);
   video_windows_plan(&map, screens, true, true, 0, &plan);
   CHECK(plan.main_mask == 0u && plan.separate_mask == 0u && plan.hidden_mask == 0u);
   mono_map(&map, RETRO_VIDEO_VIEWS_MAX);
   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
      screens[i].mode = VIDEO_WINDOW_SEPARATE;
   video_windows_plan(&map, screens, true, true, 0, &plan);
   CHECK(plan.main_mask == 1u && plan.separate_mask == 254u);
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

int main(void)
{
   test_windows_policy();
   test_selected_layout();
   if (failures)
      return 1;
   puts("video_windows: ok");
   return 0;
}
