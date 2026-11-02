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
   video_windows_plan(&map, screens, true, true, 0, NULL, &plan);
   CHECK(plan.main_mask == 1u && plan.separate_mask == 6u);
   video_windows_plan(&map, screens, true, true, 2u, NULL, &plan);
   CHECK(plan.main_mask == 3u && plan.separate_mask == 4u);
   screens[2].mode = VIDEO_WINDOW_HIDDEN;
   video_windows_plan(&map, screens, true, true, 4u, NULL, &plan);
   CHECK(plan.hidden_mask == 4u && plan.separate_mask == 2u);
   video_windows_plan(&map, screens, false, true, 0, NULL, &plan);
   CHECK(plan.main_mask == 7u && plan.hidden_mask == 0u);
   video_windows_plan(&map, screens, true, false, 0, NULL, &plan);
   CHECK(plan.main_mask == 3u && plan.separate_mask == 0u && plan.hidden_mask == 4u);
   screens[0].mode = VIDEO_WINDOW_HIDDEN;
   screens[1].mode = VIDEO_WINDOW_ATTACHED;
   video_windows_plan(&map, screens, true, true, 0, NULL, &plan);
   CHECK(plan.main_mask == 3u && plan.hidden_mask == 4u);
   screens[1].mode = 99;
   video_windows_plan(&map, screens, true, true, 0, NULL, &plan);
   CHECK(plan.main_mask == 3u);
   mono_map(&map, 0);
   video_windows_plan(&map, screens, true, true, 0, NULL, &plan);
   CHECK(plan.main_mask == 0u && plan.separate_mask == 0u && plan.hidden_mask == 0u);
   mono_map(&map, RETRO_VIDEO_VIEWS_MAX);
   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
      screens[i].mode = VIDEO_WINDOW_SEPARATE;
   video_windows_plan(&map, screens, true, true, 0, NULL, &plan);
   CHECK(plan.main_mask == 1u && plan.separate_mask == 254u);
}

static void test_windows_attached(void)
{
   video_views_map_t map;
   video_window_settings_t screens[RETRO_VIDEO_VIEWS_MAX];
   video_windows_route_t route;
   video_windows_plan_t plan;
   unsigned i;
   memset(screens, 0, sizeof(screens));
   memset(&route, 0, sizeof(route));
   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
      screens[i].mode = VIDEO_WINDOW_SEPARATE;
   mono_map(&map, 3);
   /* No route: screen 0 stays, whatever its mode */
   video_windows_plan(&map, screens, true, true, 0, NULL, &plan);
   CHECK(plan.main_mask == 1u && plan.separate_mask == 6u);
   screens[0].mode = VIDEO_WINDOW_HIDDEN;
   video_windows_plan(&map, screens, true, true, 0, NULL, &plan);
   CHECK(plan.main_mask == 1u && plan.hidden_mask == 0u);
   /* A route puts screen 0 out, whatever its mode */
   screens[0].mode = VIDEO_WINDOW_ATTACHED;
   route.own       = 1;
   route.display   = 2;
   route.screens   = 1u;
   video_windows_plan(&map, screens, true, true, 0, &route, &plan);
   CHECK(plan.main_mask == 6u && plan.separate_mask == 1u);
   /* and a Separate screen it leaves out stays */
   route.screens = 5u;
   video_windows_plan(&map, screens, true, true, 0, &route, &plan);
   CHECK(plan.main_mask == 2u && plan.separate_mask == 5u);
   /* A close of screen 0's window puts it back */
   video_windows_plan(&map, screens, true, true, 1u, &route, &plan);
   CHECK(plan.main_mask == 3u && plan.separate_mask == 4u);
   /* Hidden stays hidden */
   screens[1].mode = VIDEO_WINDOW_HIDDEN;
   video_windows_plan(&map, screens, true, true, 0, &route, &plan);
   CHECK(plan.main_mask == 0u && plan.hidden_mask == 2u);
   CHECK(plan.separate_mask == 5u);
   screens[1].mode = VIDEO_WINDOW_SEPARATE;
   /* Off, or no window support: every screen in the main window */
   video_windows_plan(&map, screens, false, true, 0, &route, &plan);
   CHECK(plan.main_mask == 7u);
   video_windows_plan(&map, screens, true, false, 0, &route, &plan);
   CHECK(plan.main_mask == 7u && plan.separate_mask == 0u);
   /* No settings: all attached */
   video_windows_plan(&map, NULL, true, true, 0, NULL, &plan);
   CHECK(plan.main_mask == 7u && plan.separate_mask == 0u && plan.hidden_mask == 0u);
}

static void monitor_display(video_window_display_t *d, unsigned ordinal,
      const char *name, bool own)
{
   memset(d, 0, sizeof(*d));
   d->ordinal  = ordinal;
   d->valid    = true;
   d->on       = true;
   d->physical = true;
   d->monitor  = true;
   d->own      = own;
   strcpy(d->name, name);
}

static void test_resolve(void)
{
   video_window_display_t d[3];
   video_window_settings_t s[RETRO_VIDEO_VIEWS_MAX];
   static char keys[RETRO_VIDEO_VIEWS_MAX][VIDEO_WINDOW_DISPLAY_NAME_SIZE];
   video_windows_route_t r;
   video_views_map_t map;
   video_windows_plan_t plan;
   /* ISO C will not add const under a pointer to array by itself */
#define KEYS ((const char (*)[VIDEO_WINDOW_DISPLAY_NAME_SIZE])keys)
   memset(s, 0, sizeof(s));
   memset(keys, 0, sizeof(keys));
   /* RetroArch on DP-1; HDMI-A-1; a second DP-1 */
   monitor_display(&d[0], 1, "DP-1", true);
   monitor_display(&d[1], 2, "HDMI-A-1", false);
   monitor_display(&d[2], 3, "DP-1", false);

   /* All Auto: Game Screen 1 in the main window, the others on the
    * first monitor RetroArch is not on */
   video_windows_resolve(d, 3, KEYS, s, &r);
   CHECK(r.monitors && !r.own && !r.display && !r.ui && !r.move);
   CHECK(r.main_screens == 1u && r.screens == 0xfeu);
   CHECK(r.target[1] == 2 && r.target[7] == 2);
   /* A size without a position set, as the menu sets it from Auto,
    * keeps Auto's monitor */
   s[1].dims = VIDEO_SCALE_PACK(320, 240);
   video_windows_resolve(d, 3, KEYS, s, &r);
   CHECK(r.target[1] == 2);
   s[1].pos = VIDEO_POS_PACK(-100, 50);
   video_windows_resolve(d, 3, KEYS, s, &r);
   CHECK(r.target[1] == 2);
   /* Saved bounds say where an Auto window goes, the origin too */
   s[1].pos_set = true;
   video_windows_resolve(d, 3, KEYS, s, &r);
   CHECK(r.target[1] == 0 && r.target[2] == 2);
   s[1].pos = 0;
   video_windows_resolve(d, 3, KEYS, s, &r);
   CHECK(r.target[1] == 0);
   /* A position without a size is not bounds */
   s[1].dims = 0;
   video_windows_resolve(d, 3, KEYS, s, &r);
   CHECK(r.target[1] == 2);
   s[1].pos_set = false;

   /* By name: a duplicate by its key, RetroArch's own monitor too */
   strcpy(keys[1], "DP-1 2");
   strcpy(keys[2], "DP-1");
   video_windows_resolve(d, 3, KEYS, s, &r);
   CHECK(r.target[1] == 3 && r.target[2] == 1);
   CHECK(r.screens & 6u);

   /* Game Screen 1 on a monitor, 2 in the main window, 3 Off */
   strcpy(keys[0], "HDMI-A-1");
   strcpy(keys[1], VIDEO_WINDOW_DISPLAY_MAIN);
   strcpy(keys[2], VIDEO_WINDOW_DISPLAY_OFF);
   video_windows_resolve(d, 3, KEYS, s, &r);
   CHECK((r.screens & 1u) && r.target[0] == 2);
   CHECK(r.main_screens == 2u);
   CHECK(!(r.screens & 4u) && !(r.main_screens & 4u) && !r.target[2]);
   /* The plan, from modes as the frame derives them */
   mono_map(&map, 3);
   s[0].mode = VIDEO_WINDOW_ATTACHED;
   s[1].mode = VIDEO_WINDOW_ATTACHED;
   s[2].mode = VIDEO_WINDOW_HIDDEN;
   video_windows_plan(&map, s, true, true, 0, &r, &plan);
   CHECK(plan.separate_mask == 1u && plan.main_mask == 2u
         && plan.hidden_mask == 4u);
   /* A close puts Game Screen 1 back in the main window */
   video_windows_plan(&map, s, true, true, 1u, &r, &plan);
   CHECK(plan.separate_mask == 0u && plan.main_mask == 3u);

   /* A name not listed is Auto */
   strcpy(keys[0], "DP-9");
   strcpy(keys[1], "DP-9");
   video_windows_resolve(d, 3, KEYS, s, &r);
   CHECK((r.main_screens & 1u) && !(r.screens & 1u));
   CHECK((r.screens & 2u) && r.target[1] == 2);
   /* Only RetroArch's monitor: Auto there; none listed: the platform's */
   video_windows_resolve(d, 1, KEYS, s, &r);
   CHECK(r.target[1] == 1);
   video_windows_resolve(NULL, 0, KEYS, s, &r);
   CHECK((r.screens & 2u) && r.target[1] == 0 && (r.main_screens & 1u));
   /* No settings at all: Auto everywhere */
   video_windows_resolve(d, 3, NULL, NULL, &r);
   CHECK(r.main_screens == 1u && r.target[1] == 2);
#undef KEYS
}

static void test_spare_window(void)
{
   /* Past the content's screens */
   CHECK(video_windows_spare_window(1u, 2) == 2);
   CHECK(video_windows_spare_window(3u, 2) == 2);
   CHECK(video_windows_spare_window(1u, 1) == 1);
   CHECK(video_windows_spare_window(1u, 0) == 1);
   /* Eight screens: one whose screen has no window */
   CHECK(video_windows_spare_window(0xfbu, 8) == 2);
   CHECK(video_windows_spare_window(0xffu, 8) == 0);
}

static void test_display_own(void)
{
   char keys[RETRO_VIDEO_VIEWS_MAX][VIDEO_WINDOW_DISPLAY_NAME_SIZE];
   video_windows_route_t r;
   video_window_display_t d;
   memset(keys, 0, sizeof(keys));
   memset(&d, 0, sizeof(d));
   d.ordinal = 1;
   d.valid   = true;
   d.monitor = true;
   strcpy(d.name, "DP-1");
   CHECK(!video_windows_display_own(&d, 1, "", 0));
   CHECK(video_windows_display_own(&d, 1, "", 1));
   CHECK(video_windows_display_own(&d, 1, "DP-1", 0));
   CHECK(!video_windows_display_own(&d, 1, VIDEO_WINDOW_DISPLAY_MAIN, 1));
   CHECK(!video_windows_display_own(&d, 1, VIDEO_WINDOW_DISPLAY_OFF, 0));
   /* Game Screen 1 on a monitor not listed is in the main window, as
    * video_windows_resolve() has it; the others get their own */
   CHECK(!video_windows_display_own(&d, 1, "DP-2", 0));
   CHECK(!video_windows_display_own(NULL, 0, "DP-1", 0));
   CHECK(video_windows_display_own(&d, 1, "DP-2", 1));
   strcpy(keys[0], "DP-2");
   video_windows_resolve(&d, 1,
         (const char (*)[VIDEO_WINDOW_DISPLAY_NAME_SIZE])keys, NULL, &r);
   CHECK((r.main_screens & 1u) && !(r.screens & 1u));
   strcpy(keys[0], "DP-1");
   video_windows_resolve(&d, 1,
         (const char (*)[VIDEO_WINDOW_DISPLAY_NAME_SIZE])keys, NULL, &r);
   CHECK(r.screens & 1u);
   memset(&d, 0, sizeof(d));
   CHECK(video_windows_offer_main(NULL, 0));
   d.monitor = true;
   CHECK(video_windows_offer_main(&d, 1));
   d.monitor = false;
   CHECK(!video_windows_offer_main(&d, 1));
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

/* The Thor: the top panel, the bottom one and a DP monitor */
static void test_thor(video_window_display_t *d, unsigned own)
{
   test_display(&d[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN, own == 1);
   test_display(&d[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN, own == 2);
   test_display(&d[2], 3, VIDEO_WINDOW_CONNECTION_UNKNOWN, own == 3);
   strcpy(d[0].name, "Built-in Screen");
   strcpy(d[1].name, "Screen-2");
   strcpy(d[2].name, "DP Screen");
}

static void test_display_key(void)
{
   video_window_display_t d[3];
   char key[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
   char small[6];
   unsigned i;
   memset(d, 0, sizeof(d));
   strcpy(d[0].name, "Built-in Screen");
   strcpy(d[1].name, "Screen-2");
   video_windows_display_key(d, 0, key, sizeof(key));
   CHECK(!strcmp(key, "Built-in Screen"));
   video_windows_display_key(d, 1, key, sizeof(key));
   CHECK(!strcmp(key, "Screen-2"));
   /* A repeated name counts in list order */
   strcpy(d[0].name, "HDMI");
   strcpy(d[1].name, "HDMI");
   strcpy(d[2].name, "HDMI");
   video_windows_display_key(d, 0, key, sizeof(key));
   CHECK(!strcmp(key, "HDMI"));
   video_windows_display_key(d, 1, key, sizeof(key));
   CHECK(!strcmp(key, "HDMI 2"));
   video_windows_display_key(d, 2, key, sizeof(key));
   CHECK(!strcmp(key, "HDMI 3"));
   /* A long name gives way to its count */
   for (i = 0; i < VIDEO_WINDOW_DISPLAY_NAME_SIZE - 1; i++)
      d[0].name[i] = d[1].name[i] = 'a';
   d[0].name[i] = d[1].name[i] = '\0';
   video_windows_display_key(d, 1, key, sizeof(key));
   CHECK(strlen(key) == VIDEO_WINDOW_DISPLAY_NAME_SIZE - 1);
   CHECK(!strcmp(key + VIDEO_WINDOW_DISPLAY_NAME_SIZE - 3, " 2"));
   video_windows_display_key(d, 0, small, sizeof(small));
   CHECK(!strcmp(small, "aaaaa"));
   video_windows_display_key(d, 1, small, sizeof(small));
   CHECK(!strcmp(small, "aaa 2"));
   /* Sizes 0 to 3: nothing past the size, ended inside it, no room for
    * a count */
   strcpy(d[0].name, "HDMI");
   strcpy(d[1].name, "HDMI");
   for (i = 0; i < 4; i++)
   {
      memset(small, '#', sizeof(small));
      video_windows_display_key(d, 1, small, i);
      CHECK(small[i] == '#');
      if (i)
      {
         CHECK(strlen(small) == i - 1);
         CHECK(!strncmp(small, "HDMI", i - 1));
      }
   }
   /* A 63-character name, the longest a key holds, keys whole */
   for (i = 0; i < VIDEO_WINDOW_DISPLAY_NAME_SIZE - 1; i++)
      d[0].name[i] = 'b';
   d[0].name[i] = '\0';
   strcpy(d[1].name, "Screen-2");
   video_windows_display_key(d, 0, key, sizeof(key));
   CHECK(strlen(key) == VIDEO_WINDOW_DISPLAY_NAME_SIZE - 1);
   CHECK(!strcmp(key, d[0].name));
}

static void test_display_key_unnamed(void)
{
   video_window_display_t d[3];
   char key[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
   memset(d, 0, sizeof(d));
   d[0].ordinal = 1;
   d[1].ordinal = 2;
   d[2].ordinal = 3;
   strcpy(d[2].name, "Monitor 2");
   /* No name: one by its ordinal, never "" (Auto) or " 2" */
   video_windows_display_key(d, 0, key, sizeof(key));
   CHECK(!strcmp(key, "Monitor 1"));
   video_windows_display_key(d, 1, key, sizeof(key));
   CHECK(!strcmp(key, "Monitor 2"));
   /* A name that is another's fallback is a duplicate of it */
   video_windows_display_key(d, 2, key, sizeof(key));
   CHECK(!strcmp(key, "Monitor 2 2"));
}

/* Eight records sit on the display list's JNI frame: a field that
 * grows them is a choice, not an accident */
static void test_display_record(void)
{
   CHECK(sizeof(video_window_display_t) == 124);
}

/* Auto falls back on the display marked default */
static void test_display_default(void)
{
   video_window_display_t d[2];
   video_windows_place_in_t in;
   video_windows_host_t host;
   video_windows_route_t r;
   test_display(&d[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN, true);
   test_display(&d[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN, false);
   d[1].physical   = false;
   d[1].is_default = true;
   memset(&in, 0, sizeof(in));
   memset(&host, 0, sizeof(host));
   in.displays  = d;
   in.count     = 2;
   in.menu_open = true;
   video_windows_host_reset(&host);
   video_windows_place(&host, &in, &r);
   CHECK(r.menu == 2);
   d[1].is_default = false;
   video_windows_host_reset(&host);
   video_windows_place(&host, &in, &r);
   CHECK(r.menu == 1);
}

#define PLACE_MENU VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU)
#define PLACE_NOTE VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_NOTIFICATIONS)

static video_window_display_t place_d[3];
static char place_screen[RETRO_VIDEO_VIEWS_MAX][VIDEO_WINDOW_DISPLAY_NAME_SIZE];
static char place_menu[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
static video_windows_place_in_t place_in;
static video_windows_host_t place_host;
static video_windows_route_t place_r;

/* The Thor with RetroArch on display @own, the top panel the default
 * one, @count displays listed: Auto everywhere, Menu Follows Game Screen 1,
 * no content, the menu open, a host that decides afresh */
static void place_reset(unsigned own, unsigned count)
{
   test_thor(place_d, own);
   place_d[0].is_default = true;
   memset(place_screen, 0, sizeof(place_screen));
   place_menu[0] = '\0';
   memset(&place_in, 0, sizeof(place_in));
   place_in.displays       = place_d;
   place_in.count          = count;
   place_in.screen_display =
      (const char (*)[VIDEO_WINDOW_DISPLAY_NAME_SIZE])place_screen;
   place_in.menu_display   = place_menu;
   place_in.menu_open      = true;
   video_windows_host_reset(&place_host);
}

/* Content with @num_screens screens, 0 for none; the menu closes with
 * it and is open without */
static void place_game(unsigned num_screens, bool mapless)
{
   place_in.num_screens = num_screens;
   place_in.mapless     = mapless;
   place_in.menu_open   = !num_screens;
}

/* RetroArch now on display @own, 0 for none */
static void place_own(unsigned own)
{
   unsigned i;
   for (i = 0; i < place_in.count; i++)
      place_d[i].own = place_d[i].ordinal == own;
}

static void place_at(retro_time_t now)
{
   place_in.now = now;
   video_windows_place(&place_host, &place_in, &place_r);
}

static bool placed(unsigned display, unsigned screens,
      unsigned main_screens, unsigned ui, unsigned move)
{
   if (     place_r.display == display && place_r.screens == screens
         && place_r.main_screens == main_screens && place_r.ui == ui
         && place_r.move == move)
      return true;
   printf("placed display %u screens %u main %u ui %u move %u\n",
         place_r.display, place_r.screens, place_r.main_screens,
         place_r.ui, place_r.move);
   return false;
}

/* Where the screens, the menu and the notifications go, with RetroArch
 * already on the display placement decides for it */
static void test_place_where(void)
{
   /* place: targets */
   place_reset(1, 3);
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 0));
   CHECK(place_r.own == 1 && place_r.menu == 1 && !place_r.target[0]);
   place_game(2, false);
   place_at(0);
   CHECK(placed(2, 2u, 1u, 0u, 0));
   CHECK(place_r.target[0] == 1 && place_r.target[1] == 2);
   CHECK(!place_r.target[2] && !place_r.logo);
   /* Off: not shown */
   strcpy(place_screen[1], VIDEO_WINDOW_DISPLAY_OFF);
   place_at(0);
   CHECK(placed(0, 0u, 1u, 0u, 0) && !place_r.target[1]);
   /* A name; one not listed is Auto */
   strcpy(place_screen[1], "DP Screen");
   place_at(0);
   CHECK(placed(3, 2u, 1u, 0u, 0));
   strcpy(place_screen[1], "Nowhere");
   place_at(0);
   CHECK(placed(2, 2u, 1u, 0u, 0));
   /* Names shared: the second listed is " 2", cut to fit when long */
   {
      char key[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
      strcpy(place_d[1].name, "HDMI");
      strcpy(place_d[2].name, "HDMI");
      strcpy(place_screen[1], "HDMI 2");
      place_at(0);
      CHECK(place_r.target[1] == 3);
      strcpy(place_screen[1], "HDMI");
      place_at(0);
      CHECK(place_r.target[1] == 2);
      memset(place_d[1].name, 'x', VIDEO_WINDOW_DISPLAY_NAME_SIZE - 1);
      place_d[1].name[VIDEO_WINDOW_DISPLAY_NAME_SIZE - 1] = '\0';
      memcpy(place_d[2].name, place_d[1].name, sizeof(place_d[2].name));
      video_windows_display_key(place_d, 2, key, sizeof(key));
      strcpy(place_screen[1], key);
      place_at(0);
      CHECK(place_r.target[1] == 3);
      strcpy(place_screen[1], place_d[1].name);
      place_at(0);
      CHECK(place_r.target[1] == 2);
   }
   /* Both on RetroArch's display: no Presentation */
   place_reset(2, 3);
   place_game(2, false);
   strcpy(place_screen[0], "Screen-2");
   place_at(0);
   CHECK(placed(0, 0u, 3u, 0u, 0));
   /* At most RETRO_VIDEO_VIEWS_MAX screens */
   place_reset(1, 3);
   place_game(99, false);
   place_at(0);
   CHECK(placed(2, 2u, 0xfdu, 0u, 0));
   /* Nothing routed without a list or an own display */
   place_in.displays = NULL;
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 0) && !place_r.own && !place_r.menu);
   place_in.displays = place_d;
   place_in.count    = 0;
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 0) && !place_r.own);
   place_in.count    = 3;
   place_own(0);
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 0) && !place_r.own);
   /* A game without a map shows even when Off, in the main window */
   place_reset(1, 3);
   place_game(1, true);
   strcpy(place_screen[0], VIDEO_WINDOW_DISPLAY_OFF);
   place_at(0);
   CHECK(placed(0, 0u, 1u, 0u, 0) && place_r.target[0] == 1);
   /* and only there, with RetroArch moved off Game Screen 1's display: the
    * open menu has that display */
   place_reset(2, 3);
   place_game(1, true);
   strcpy(place_screen[0], "Screen-2");
   place_at(0);
   CHECK(placed(0, 0u, 1u, 0u, 0));
   place_own(1);
   place_at(0);
   CHECK(place_r.target[0] == 2 && placed(0, 0u, 1u, PLACE_MENU, 0));
   place_in.menu_open = true;
   place_at(0);
   CHECK(placed(2, 0u, 1u, PLACE_MENU, 0) && !place_r.logo);

   /* place: Auto */
   /* A DIRECT or TRANSITIVE monitor takes Game Screen 1, and Game Screen 2 the
    * default display */
   place_reset(3, 3);
   place_d[2].connection = VIDEO_WINDOW_CONNECTION_DIRECT;
   place_game(2, false);
   place_at(0);
   CHECK(placed(1, 2u, 1u, 0u, 0));
   CHECK(place_r.target[0] == 3 && place_r.target[1] == 1);
   CHECK(place_r.menu == 3);
   place_d[2].connection = VIDEO_WINDOW_CONNECTION_TRANSITIVE;
   place_at(0);
   CHECK(placed(1, 2u, 1u, 0u, 0));
   /* not without product info */
   place_d[2].physical = false;
   place_own(1);
   place_at(0);
   CHECK(placed(2, 2u, 1u, 0u, 0) && place_r.target[0] == 1);
   /* No default display listed: RetroArch's own stands in */
   place_reset(2, 3);
   place_d[0].is_default = false;
   place_game(2, false);
   place_at(0);
   CHECK(place_r.target[0] == 2 && place_r.target[1] == 1);
   CHECK(placed(1, 2u, 1u, 0u, 0));
   /* Game Screens 3 and up: the default display */
   place_reset(1, 3);
   place_game(3, false);
   place_at(0);
   CHECK(placed(2, 2u, 5u, 0u, 0) && place_r.target[2] == 1);
   /* Private, off or invalid: not Auto's */
   place_d[1].is_private = true;
   place_at(0);
   CHECK(place_r.target[1] == 3);
   place_d[1].is_private = false;
   place_d[1].on         = false;
   place_at(0);
   CHECK(place_r.target[1] == 3);
   place_d[1].on         = true;
   place_d[1].valid      = false;
   place_at(0);
   CHECK(place_r.target[1] == 3);
   place_d[1].valid      = true;
   /* A refused display is still Auto's */
   place_own(2);
   place_d[1].refused = true;
   place_at(0);
   CHECK(place_r.target[1] == 2);
   CHECK(placed(1, 5u, 2u, PLACE_MENU | PLACE_NOTE, 0));
   /* Menu Display Auto is Game Screen 1's Auto choice */
   place_reset(1, 3);
   place_in.menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   place_at(0);
   CHECK(place_r.menu == 1);
   strcpy(place_menu, VIDEO_WINDOW_DISPLAY_OFF);
   place_at(0);
   CHECK(place_r.menu == 1);
   strcpy(place_menu, "Nowhere");
   place_at(0);
   CHECK(place_r.menu == 1);
   place_d[2].connection = VIDEO_WINDOW_CONNECTION_DIRECT;
   place_own(3);
   place_menu[0] = '\0';
   place_at(0);
   CHECK(place_r.menu == 3 && placed(0, 0u, 0u, 0u, 0));
   strcpy(place_menu, "Screen-2");
   place_own(2);
   place_at(0);
   CHECK(place_r.menu == 2 && placed(0, 0u, 0u, 0u, 0));

   /* place: menu follows an Off screen */
   place_reset(2, 3);
   place_in.menu_follows = 1;
   strcpy(place_screen[1], VIDEO_WINDOW_DISPLAY_OFF);
   place_at(0);
   CHECK(place_r.menu == 2 && placed(0, 0u, 0u, 0u, 0));
   /* the default display while a monitor has Game Screen 1 */
   place_reset(1, 3);
   place_d[2].connection = VIDEO_WINDOW_CONNECTION_DIRECT;
   place_in.menu_follows = 1;
   strcpy(place_screen[1], VIDEO_WINDOW_DISPLAY_OFF);
   place_at(0);
   CHECK(place_r.menu == 1 && placed(0, 0u, 0u, 0u, 0));
   /* or a screen the content does not have */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   strcpy(place_screen[1], "DP Screen");
   place_game(1, false);
   place_at(0);
   CHECK(place_r.menu == 3 && !place_r.target[1]);
   CHECK(placed(0, 0u, 1u, PLACE_MENU, 0));
   place_in.menu_follows = 5;
   place_at(0);
   CHECK(place_r.menu == 1 && placed(0, 0u, 1u, 0u, 0));
   /* Out of range: Separate */
   place_in.menu_follows = 99;
   strcpy(place_menu, "Screen-2");
   place_at(0);
   CHECK(place_r.menu == 2);

   /* place: in use */
   /* A display is in use while it shows a screen or the open menu */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_game(1, false);
   place_at(0);
   CHECK(placed(0, 0u, 1u, PLACE_MENU, 0));
   place_in.menu_open = true;
   place_at(0);
   CHECK(placed(2, 0u, 1u, PLACE_MENU, 0));
   /* Notifications never put one in use */
   place_in.menu_open     = false;
   place_in.notifications = VIDEO_NOTIFICATIONS_WITH_MENU;
   place_at(0);
   CHECK(placed(0, 0u, 1u, PLACE_MENU, 0));

   /* place: three displays */
   /* One Presentation, the lowest screen's; a third display's screens
    * join the main window */
   place_reset(1, 3);
   place_game(3, false);
   strcpy(place_screen[1], "DP Screen");
   strcpy(place_screen[2], "Screen-2");
   place_at(0);
   CHECK(placed(3, 2u, 5u, 0u, 0));
   /* and so does the menu's */
   place_in.menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   strcpy(place_menu, "Screen-2");
   place_in.menu_open    = true;
   place_at(0);
   CHECK(placed(3, 2u, 5u, 0u, 0));
   /* A refused display never has it: the next one does */
   place_own(3);
   place_d[2].refused = true;
   place_at(0);
   CHECK(placed(1, 1u, 6u, PLACE_NOTE, 0));

   /* place: logo */
   /* The menu on a refusing display, the screens elsewhere: the main
    * window has nothing to show until the menu opens there */
   place_reset(1, 3);
   place_d[0].refused    = true;
   place_in.menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   strcpy(place_menu, "Built-in Screen");
   strcpy(place_screen[0], "Screen-2");
   place_game(2, false);
   place_at(0);
   CHECK(placed(2, 3u, 0u, PLACE_NOTE, 0) && place_r.logo);
   place_in.menu_open = true;
   place_at(0);
   CHECK(placed(2, 3u, 0u, PLACE_NOTE, 0) && !place_r.logo);
   /* Not with no game */
   place_game(0, false);
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 0) && !place_r.logo);
   /* A game whose screens are all Off */
   place_reset(1, 3);
   strcpy(place_screen[0], VIDEO_WINDOW_DISPLAY_OFF);
   strcpy(place_screen[1], VIDEO_WINDOW_DISPLAY_OFF);
   place_game(2, false);
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 0) && place_r.logo);

   /* place: notifications */
   /* With the Menu: a closed menu's display has no window for a
    * mapless game; open, the menu's window has them */
   place_reset(1, 3);
   place_in.menu_follows  = 1;
   place_in.notifications = VIDEO_NOTIFICATIONS_WITH_MENU;
   place_game(1, true);
   place_at(0);
   CHECK(placed(0, 0u, 1u, PLACE_MENU, 0));
   place_in.menu_open = true;
   place_at(0);
   CHECK(placed(2, 0u, 1u, PLACE_MENU | PLACE_NOTE, 0));
   /* With the Game: a game without a map has them in the main window */
   place_in.notifications = VIDEO_NOTIFICATIONS_WITH_GAME;
   place_at(0);
   CHECK(placed(2, 0u, 1u, PLACE_MENU, 0));
   /* With the Menu, the menu in the main window: there too */
   place_in.notifications = VIDEO_NOTIFICATIONS_WITH_MENU;
   place_in.menu_follows  = 0;
   place_game(2, false);
   place_in.menu_open     = true;
   place_at(0);
   CHECK(placed(2, 2u, 1u, 0u, 0));
   /* Closed, in Game Screen 2's window on the menu's display */
   place_in.menu_follows = 1;
   place_in.menu_open    = false;
   place_at(0);
   CHECK(placed(2, 2u, 1u, PLACE_MENU | PLACE_NOTE, 0));
   /* No window there, the game's: a one-screen game */
   place_game(1, false);
   place_at(0);
   CHECK(placed(0, 0u, 1u, PLACE_MENU, 0));
   /* or a menu display with no screen */
   place_game(2, false);
   place_in.menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   strcpy(place_menu, "DP Screen");
   place_at(0);
   CHECK(placed(2, 2u, 1u, 0u, 0));
   /* With the Menu, the menu closed on the refusing display RetroArch
    * has: the main window, not the game's */
   place_reset(1, 3);
   place_d[0].refused     = true;
   place_in.notifications = VIDEO_NOTIFICATIONS_WITH_MENU;
   place_in.menu_follows  = VIDEO_MENU_FOLLOWS_SEPARATE;
   strcpy(place_menu, "Built-in Screen");
   strcpy(place_screen[0], "Screen-2");
   place_game(2, false);
   place_at(0);
   CHECK(place_r.menu == 1 && placed(2, 3u, 0u, 0u, 0));
   /* Game Screen 2 on it, an open menu drawn off its display: they stay
    * with the menu, in the main window */
   strcpy(place_menu, "Screen-2");
   strcpy(place_screen[0], "DP Screen");
   strcpy(place_screen[1], "Built-in Screen");
   place_in.menu_open = true;
   place_at(0);
   CHECK(place_r.menu == 2 && placed(3, 1u, 2u, 0u, 0));
}

/* Auto's monitor: the platform's type where it gives one, else the
 * connection, else a product name off the default display. */
static void test_place_external(void)
{
   /* Typed external: Game Screen 1 and the menu on the monitor, Game
    * Screen 2 on the default display */
   place_reset(1, 3);
   place_d[0].type = VIDEO_WINDOW_TYPE_INTERNAL;
   place_d[1].type = VIDEO_WINDOW_TYPE_INTERNAL;
   place_d[2].type = VIDEO_WINDOW_TYPE_EXTERNAL;
   place_game(2, false);
   place_at(0);
   CHECK(place_r.target[0] == 3 && place_r.target[1] == 1);
   CHECK(place_r.menu == 3);
   /* Typed internal: never, whatever its connection and name */
   place_reset(1, 3);
   place_d[2].type       = VIDEO_WINDOW_TYPE_INTERNAL;
   place_d[2].connection = VIDEO_WINDOW_CONNECTION_DIRECT;
   strcpy(place_d[2].model, "LG HDR 4K");
   place_game(2, false);
   place_at(0);
   CHECK(place_r.target[0] == 1 && place_r.target[1] == 2);
   /* Untyped on an unknown connection: the named display is the
    * monitor, the unnamed panel below it is not */
   place_reset(1, 3);
   strcpy(place_d[2].model, "LG HDR 4K");
   place_game(2, false);
   place_at(0);
   CHECK(place_r.target[0] == 3 && place_r.target[1] == 1);
   /* A one-screen phone and a named monitor: Game Screen 2 stays off
    * the monitor */
   place_reset(1, 2);
   strcpy(place_d[1].model, "LG HDR 4K");
   place_game(2, false);
   place_at(0);
   CHECK(place_r.target[0] == 2 && place_r.target[1] == 1);
   /* A named default display is never the monitor */
   place_reset(1, 2);
   strcpy(place_d[0].model, "AMOLED");
   strcpy(place_d[1].model, "LG HDR 4K");
   place_game(2, false);
   place_at(0);
   CHECK(place_r.target[0] == 2 && place_r.target[1] == 1);
   /* Nor one without product info, typed and named or not */
   place_reset(1, 3);
   place_d[1].physical = false;
   place_d[1].type     = VIDEO_WINDOW_TYPE_EXTERNAL;
   strcpy(place_d[1].model, "Cast");
   place_d[2].type     = VIDEO_WINDOW_TYPE_EXTERNAL;
   place_game(2, false);
   place_at(0);
   CHECK(place_r.target[0] == 3 && place_r.target[1] == 1);
}

/* When RetroArch moves, and the order its windows come and go in */
static void test_place_moves(void)
{
   unsigned seq;
   /* place: main window's display */
   /* Cocoon starts RetroArch on the bottom panel: it moves to the menu's
    * display, the top one, and the bottom one is free */
   place_reset(2, 3);
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 1));
   place_at(100000);
   CHECK(placed(0, 0u, 0u, 0u, 0) && place_host.moving == 1);
   place_own(1);
   place_at(200000);
   CHECK(placed(0, 0u, 0u, 0u, 0) && !place_host.moving);
   /* A setting changing while a move is under way: deferred, decided
    * once it arrives */
   place_reset(2, 3);
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 1));
   place_in.menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   strcpy(place_menu, "DP Screen");
   place_at(100000);
   CHECK(placed(3, 0u, 0u, PLACE_MENU | PLACE_NOTE, 0));
   CHECK(place_host.moving == 1);
   place_own(1);
   place_at(200000);
   CHECK(placed(0, 0u, 0u, 0u, 3));
   /* Menu Follows alone decides again, and so does Menu Display */
   place_reset(1, 3);
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 0));
   place_in.menu_follows = 1;
   place_at(100000);
   CHECK(placed(0, 0u, 0u, 0u, 2));
   place_reset(1, 3);
   place_in.menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 0));
   strcpy(place_menu, "DP Screen");
   place_at(100000);
   CHECK(placed(0, 0u, 0u, 0u, 3));
   /* A display this game may use that refuses: RetroArch goes there,
    * once its own display's Presentation shows */
   place_reset(1, 3);
   strcpy(place_screen[1], "DP Screen");
   place_game(2, false);
   place_at(0);
   CHECK(placed(3, 2u, 1u, 0u, 0));
   place_d[2].refused = true;
   place_at(100000);
   CHECK(placed(1, 1u, 2u, PLACE_MENU | PLACE_NOTE, 0));
   CHECK(place_host.make == 1 && place_host.host == 3);
   place_in.shown = 1;
   place_at(200000);
   CHECK(placed(1, 1u, 2u, PLACE_MENU | PLACE_NOTE, 3));
   place_own(3);
   place_at(300000);
   CHECK(placed(1, 1u, 2u, PLACE_MENU | PLACE_NOTE, 0));
   CHECK(!place_host.moving && !place_host.make);
   /* A refusal clearing decides nothing */
   place_d[2].refused = false;
   place_at(400000);
   CHECK(placed(1, 1u, 2u, PLACE_MENU | PLACE_NOTE, 0));
   /* Nor when it clears as RetroArch arrives */
   place_reset(1, 3);
   strcpy(place_screen[1], "DP Screen");
   place_game(2, false);
   place_at(0);
   place_d[2].refused = true;
   place_at(100000);
   place_in.shown = 1;
   place_at(200000);
   CHECK(place_r.move == 3);
   place_own(3);
   place_d[2].refused = false;
   place_at(300000);
   CHECK(placed(1, 1u, 2u, PLACE_MENU | PLACE_NOTE, 0));
   /* A names change forgets it: back to Game Screen 1's display */
   strcpy(place_d[1].name, "Screen-3");
   place_at(400000);
   CHECK(place_r.move == 1);
   /* Two displays that refuse: Game Screen 2's before the menu's */
   place_reset(1, 3);
   place_d[1].refused    = true;
   place_d[2].refused    = true;
   place_in.menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   strcpy(place_menu, "Screen-2");
   strcpy(place_screen[1], "DP Screen");
   place_game(2, false);
   place_at(0);
   place_in.shown = 1;
   place_at(100000);
   CHECK(place_r.move == 3);
   /* Game Screen 1's before Game Screen 2's */
   place_reset(1, 3);
   place_d[1].refused = true;
   place_d[2].refused = true;
   strcpy(place_screen[0], "Screen-2");
   strcpy(place_screen[1], "DP Screen");
   place_game(2, false);
   place_at(0);
   CHECK(placed(0, 0u, 3u, 0u, 2));

   /* place: one-screen game, menu on a refusing display */
   /* The game wins: the menu opens over it */
   place_reset(2, 3);
   place_d[0].refused    = true;
   place_in.menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   strcpy(place_menu, "Built-in Screen");
   strcpy(place_screen[0], "Screen-2");
   place_game(1, true);
   place_at(0);
   CHECK(placed(0, 0u, 1u, 0u, 0) && !place_r.logo);
   place_in.menu_open = true;
   place_at(100000);
   CHECK(placed(0, 0u, 1u, 0u, 0) && !place_r.logo);
   /* One that takes a Presentation: the menu there while open, the
    * display free while it is closed */
   place_d[0].refused = false;
   place_in.menu_open = false;
   place_at(200000);
   CHECK(placed(0, 0u, 1u, PLACE_MENU, 0));
   place_in.menu_open = true;
   place_at(300000);
   CHECK(placed(1, 0u, 1u, PLACE_MENU, 0));
   /* Its map arriving, the screen count the same: the menu's refusing
    * display, once Game Screen 1's Presentation shows */
   place_reset(2, 3);
   place_d[0].refused    = true;
   place_in.menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   strcpy(place_menu, "Built-in Screen");
   strcpy(place_screen[0], "Screen-2");
   place_game(1, true);
   place_at(0);
   CHECK(placed(0, 0u, 1u, 0u, 0));
   place_in.mapless = false;
   place_at(100000);
   CHECK(placed(2, 1u, 0u, PLACE_NOTE, 0));
   place_in.shown = 2;
   place_at(200000);
   CHECK(placed(2, 1u, 0u, PLACE_NOTE, 1));

   /* place: make before break at start */
   /* Long after RetroArch last moved, a game still waits for its
    * Presentation; Android putting RetroArch elsewhere drops the wait */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 2));
   place_own(2);
   place_at(100000);
   place_game(2, false);
   place_at(3000000);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 0) && place_host.make == 2);
   place_own(3);
   place_in.shown = 2;
   place_at(3100000);
   CHECK(!place_r.move && !place_host.make);
   /* Menu Follows Game Screen 2, on the bottom panel: with no game,
    * RetroArch there */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 2));
   place_own(2);
   place_at(100000);
   CHECK(placed(0, 0u, 0u, 0u, 0));
   /* A two-screen game: Game Screen 2's Presentation on the bottom panel
    * first, the main window showing Game Screen 1, then the move */
   place_game(2, false);
   place_at(200000);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 0) && place_host.make == 2);
   place_at(300000);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 0));
   place_in.shown = 2;
   place_at(400000);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 1));
   place_at(500000);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 0));
   place_own(1);
   place_at(600000);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 0));
   CHECK(!place_host.moving && !place_host.make);

   /* place: keep at close */
   /* RetroArch moves under the Presentation, which stays, not drawn,
    * until the main window has drawn there */
   place_game(0, false);
   place_at(700000);
   CHECK(placed(2, 0u, 0u, 0u, 2) && place_r.keep == 2);
   place_at(800000);
   CHECK(placed(2, 0u, 0u, 0u, 0) && place_r.keep == 2);
   place_own(2);
   place_at(900000);
   CHECK(placed(2, 0u, 0u, 0u, 0) && place_r.keep == 2);
   place_at(1000000);
   CHECK(place_r.keep == 2);
   place_at(1100000);
   CHECK(place_r.keep == 2);
   place_at(1200000);
   CHECK(placed(0, 0u, 0u, 0u, 0) && !place_r.keep);
   /* A swap: the move comes first, under the Presentation; the display
    * left gets one after the keep */
   place_reset(1, 3);
   place_game(2, false);
   place_at(0);
   CHECK(placed(2, 2u, 1u, 0u, 0));
   place_in.shown = 2;
   strcpy(place_screen[0], "Screen-2");
   strcpy(place_screen[1], "Built-in Screen");
   place_at(100000);
   CHECK(placed(2, 0u, 3u, 0u, 2) && place_r.keep == 2);
   place_own(2);
   place_at(200000);
   place_at(300000);
   place_at(400000);
   CHECK(place_r.keep == 2);
   place_at(500000);
   CHECK(placed(1, 2u, 1u, 0u, 0) && !place_r.keep);
   /* Kept, not drawn: the menu that follows Game Screen 2 is not in it */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_game(2, false);
   place_at(0);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 0));
   place_in.shown = 2;
   strcpy(place_screen[0], "Screen-2");
   strcpy(place_screen[1], "Built-in Screen");
   place_at(100000);
   CHECK(placed(2, 0u, 3u, 0u, 2) && place_r.keep == 2);
   /* A list change ends a keep */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_game(2, false);
   place_at(0);
   place_in.shown = 2;
   place_game(0, false);
   place_at(100000);
   CHECK(place_r.keep == 2 && place_r.move == 2);
   place_own(2);
   place_at(200000);
   CHECK(place_r.keep == 2);
   strcpy(place_d[2].name, "HDMI Screen");
   place_at(300000);
   CHECK(placed(0, 0u, 0u, 0u, 0) && !place_r.keep);
   /* So does RetroArch being put elsewhere */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_game(2, false);
   place_at(0);
   place_in.shown = 2;
   place_game(0, false);
   place_at(100000);
   place_own(2);
   place_at(200000);
   CHECK(place_r.keep == 2);
   place_own(3);
   place_at(300000);
   CHECK(!place_r.keep);
   /* And the move under the Presentation failing, though a setting
    * changed meanwhile moves RetroArch on at once */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_game(2, false);
   place_at(0);
   place_in.shown = 2;
   place_game(0, false);
   place_at(100000);
   CHECK(place_r.keep == 2 && place_host.moving == 2);
   place_in.menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   strcpy(place_menu, "DP Screen");
   place_in.failed_move = place_host.move_seq;
   place_at(200000);
   place_in.failed_move = 0;
   CHECK(place_r.move == 3 && !place_r.keep);

   /* place: menu open and close never move */
   place_reset(1, 3);
   place_d[0].refused    = true;
   place_in.menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   strcpy(place_menu, "Built-in Screen");
   strcpy(place_screen[0], "Screen-2");
   place_game(2, false);
   place_at(0);
   CHECK(placed(2, 3u, 0u, PLACE_NOTE, 0) && place_r.logo);
   place_in.menu_open = true;
   place_at(100000);
   CHECK(placed(2, 3u, 0u, PLACE_NOTE, 0) && !place_r.logo);
   place_in.menu_open = false;
   place_at(200000);
   CHECK(placed(2, 3u, 0u, PLACE_NOTE, 0) && place_r.logo);
   /* Nor does RetroArch's own display changing */
   place_own(2);
   place_at(300000);
   CHECK(placed(0, 0u, 3u, 0u, 0) && place_host.host == 1);
   /* Nor Notifications */
   place_in.notifications = VIDEO_NOTIFICATIONS_WITH_MENU;
   place_at(400000);
   CHECK(placed(0, 0u, 3u, 0u, 0) && place_host.host == 1);

   /* place: failures */
   /* A move that does not show in time gives its display up: what it
    * would show stays in the main window, until the list changes */
   place_reset(2, 3);
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 1));
   place_at(VIDEO_WINDOWS_MOVE_USEC - 1);
   CHECK(placed(0, 0u, 0u, 0u, 0) && place_host.moving == 1);
   place_at(VIDEO_WINDOWS_MOVE_USEC);
   CHECK(placed(0, 0u, 0u, 0u, 0));
   CHECK(place_host.unable == 1u && !place_host.moving);
   CHECK(place_host.host == 2);
   strcpy(place_screen[2], "DP Screen");
   place_at(VIDEO_WINDOWS_MOVE_USEC + 100000);
   CHECK(placed(0, 0u, 0u, 0u, 0));
   place_in.count = 2;
   place_at(VIDEO_WINDOWS_MOVE_USEC + 200000);
   CHECK(placed(0, 0u, 0u, 0u, 1) && !place_host.unable);
   /* One the platform could not start: given up at once */
   place_in.failed_move = place_r.move_seq;
   place_at(VIDEO_WINDOWS_MOVE_USEC + 300000);
   CHECK(placed(0, 0u, 0u, 0u, 0) && place_host.unable == 1u);
   place_in.failed_move = 0;
   /* A display unplugged and back that a stalled app thread hears as
    * one list: its new id makes it a new list */
   place_reset(2, 3);
   place_at(0);
   place_at(VIDEO_WINDOWS_MOVE_USEC);
   CHECK(place_host.unable == 1u);
   place_d[0].id = 5;
   place_at(VIDEO_WINDOWS_MOVE_USEC + 100000);
   CHECK(placed(0, 0u, 0u, 0u, 1) && !place_host.unable);
   /* A failure names its move: a late one for a move given up on ends
    * no later move */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 2));
   seq = place_r.move_seq;
   CHECK(seq && seq == place_host.move_seq);
   place_in.count = 2;
   place_at(100000);
   CHECK(placed(0, 0u, 0u, 0u, 2) && place_r.move_seq != seq);
   place_in.failed_move = seq;
   place_at(200000);
   place_in.failed_move = 0;
   CHECK(place_host.moving == 2 && !place_host.unable);
   place_in.failed_move = place_host.move_seq;
   place_at(300000);
   place_in.failed_move = 0;
   CHECK(!place_host.moving && place_host.unable == 2u);
   /* Numbers wrap inside a Java int and outlast a reset, so a failure
    * from before the reset ends no move after it */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_host.move_seq   = 0x7fffffffu;
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 2) && place_r.move_seq == 1);
   video_windows_host_reset(&place_host);
   CHECK(place_host.move_seq == 1);
   place_at(100000);
   CHECK(placed(0, 0u, 0u, 0u, 2) && place_r.move_seq == 2);
   place_in.failed_move = 1;
   place_at(200000);
   place_in.failed_move = 0;
   CHECK(place_host.moving == 2 && !place_host.unable);
   /* A Presentation that never shows: the move after the time */
   place_reset(2, 3);
   place_in.menu_follows = 1;
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 0));
   place_game(2, false);
   place_at(100000);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 0) && place_host.make == 2);
   place_at(100000 + VIDEO_WINDOWS_MOVE_USEC);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 1) && !place_host.make);
   /* A display going away: Android moves RetroArch, placement decides
    * again */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 2));
   place_own(2);
   place_at(100000);
   place_in.count = 1;
   place_own(1);
   place_at(200000);
   CHECK(placed(0, 0u, 0u, 0u, 0) && place_r.menu == 1);
   /* A list change while a move is under way: asked again at once */
   place_reset(2, 3);
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 1));
   strcpy(place_d[2].name, "HDMI Screen");
   place_at(100000);
   CHECK(placed(0, 0u, 0u, 0u, 1));
   /* A move that lands after its time: the display RetroArch is on is
    * not unable, and arriving decides nothing */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_at(0);
   CHECK(placed(0, 0u, 0u, 0u, 2));
   place_at(VIDEO_WINDOWS_MOVE_USEC);
   CHECK(place_host.unable == 2u && !place_host.moving);
   place_own(2);
   place_at(VIDEO_WINDOWS_MOVE_USEC + 100000);
   CHECK(placed(0, 0u, 0u, 0u, 0));
   CHECK(!place_host.unable && place_host.host == 1);
   /* So a two-screen game's start makes Game Screen 2's Presentation there
    * before the move */
   place_game(2, false);
   place_at(VIDEO_WINDOWS_MOVE_USEC + 200000);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 0) && place_host.make == 2);
   place_in.shown = 2;
   place_at(VIDEO_WINDOWS_MOVE_USEC + 300000);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 1));

   /* place: map change mid-game */
   /* Routed again once the start's time is up, RetroArch staying: it
    * moved onto Game Screen 2's refusing display, and the game dropping
    * Game Screen 2 leaves it there with Game Screen 1 on the Presentation */
   place_reset(1, 3);
   strcpy(place_screen[1], "DP Screen");
   place_d[2].refused = true;
   place_game(2, false);
   place_at(0);
   CHECK(placed(1, 1u, 2u, PLACE_MENU | PLACE_NOTE, 0));
   place_in.shown = 1;
   place_at(100000);
   CHECK(place_r.move == 3);
   place_own(3);
   place_at(200000);
   place_game(1, false);
   place_at(VIDEO_WINDOWS_MOVE_USEC);
   CHECK(placed(1, 1u, 0u, PLACE_MENU | PLACE_NOTE, 0) && place_r.logo);
   CHECK(place_host.host == 3 && !place_host.make);
   /* The map cleared: the frame in the main window */
   place_game(1, true);
   place_at(VIDEO_WINDOWS_MOVE_USEC + 100000);
   CHECK(placed(0, 0u, 1u, PLACE_MENU, 0) && !place_r.logo);
   /* Content close still moves */
   place_in.shown = 0;
   place_game(0, false);
   place_at(VIDEO_WINDOWS_MOVE_USEC + 200000);
   CHECK(placed(0, 0u, 0u, 0u, 1));
   /* A first map a frame after a start without one is the start's */
   place_reset(1, 3);
   strcpy(place_screen[1], "DP Screen");
   place_d[2].refused = true;
   place_game(1, true);
   place_at(1000000);
   CHECK(placed(0, 0u, 1u, 0u, 0));
   place_game(2, false);
   place_at(1016667);
   CHECK(placed(1, 1u, 2u, PLACE_MENU | PLACE_NOTE, 0));
   CHECK(place_host.make == 1 && place_host.host == 3);
   /* Not one that comes after the move's time */
   place_reset(1, 3);
   strcpy(place_screen[1], "DP Screen");
   place_d[2].refused = true;
   place_game(1, true);
   place_at(1000000);
   place_game(2, false);
   place_at(1000000 + VIDEO_WINDOWS_MOVE_USEC);
   CHECK(placed(0, 0u, 3u, 0u, 0) && !place_host.make);
   /* Any map change soon after a start is the start's: a second screen
    * a few frames in gets a two-screen start's make before break, and
    * dropping it again decides again */
   place_reset(1, 3);
   strcpy(place_screen[1], "DP Screen");
   place_d[2].refused = true;
   place_game(1, false);
   place_at(1000000);
   CHECK(placed(0, 0u, 1u, 0u, 0));
   place_game(2, false);
   place_at(1050000);
   CHECK(placed(1, 1u, 2u, PLACE_MENU | PLACE_NOTE, 0));
   CHECK(place_host.make == 1 && place_host.host == 3);
   place_game(1, false);
   place_at(1100000);
   CHECK(placed(0, 0u, 1u, 0u, 0));
   CHECK(!place_host.make && place_host.host == 1);
   /* A start's make, Game Screen 2 dropped as its Presentation shows: no
    * move onto a display the game no longer uses */
   place_reset(1, 3);
   strcpy(place_screen[1], "DP Screen");
   place_d[2].refused = true;
   place_game(2, false);
   place_at(1000000);
   CHECK(place_host.make == 1 && place_host.host == 3);
   place_in.shown = 1;
   place_game(1, false);
   place_at(1050000);
   CHECK(placed(0, 0u, 1u, 0u, 0));
   CHECK(!place_host.make && place_host.host == 1);
   /* A start's move under way: the change is decided on arrival, and
    * RetroArch may move again */
   place_reset(1, 3);
   strcpy(place_screen[1], "DP Screen");
   place_d[2].refused = true;
   place_game(2, false);
   place_at(1000000);
   place_in.shown = 1;
   place_at(1050000);
   CHECK(place_r.move == 3);
   place_game(1, false);
   place_at(1100000);
   CHECK(!place_r.move && place_host.moving == 3);
   place_own(3);
   place_at(1150000);
   CHECK(place_r.move == 1 && place_r.keep == 1);
   /* A make asked late in the start, its map gone once the start's time
    * is up: no move, though the Presentation shows or never does */
   place_reset(1, 3);
   strcpy(place_screen[1], "DP Screen");
   place_d[2].refused = true;
   place_game(1, false);
   place_at(0);
   place_game(2, false);
   place_at(1900000);
   CHECK(place_host.make == 1 && place_host.host == 3);
   place_game(1, false);
   place_at(2050000);
   CHECK(placed(0, 0u, 1u, 0u, 0) && !place_host.make);
   place_in.shown = 1;
   place_at(2100000);
   CHECK(placed(0, 0u, 1u, 0u, 0));
   place_in.shown = 0;
   place_at(1900000 + VIDEO_WINDOWS_MOVE_USEC);
   CHECK(placed(0, 0u, 1u, 0u, 0) && !place_host.moving);
   /* One a setting asked mid-game survives a map change: the move once
    * the Presentation shows */
   place_reset(1, 3);
   strcpy(place_screen[2], "Built-in Screen");
   place_d[2].refused = true;
   place_game(2, false);
   place_at(0);
   CHECK(placed(2, 2u, 1u, 0u, 0));
   strcpy(place_screen[1], "DP Screen");
   place_at(3000000);
   CHECK(place_host.make == 1 && place_host.host == 3);
   place_game(3, false);
   place_at(3050000);
   CHECK(place_host.make == 1 && !place_r.move);
   place_in.shown = 1;
   place_at(3100000);
   CHECK(place_r.move == 3);
   /* Content replaced is a close and a start, decided once the new one
    * is up: the old one closing and the new map coming move nothing */
   place_reset(1, 3);
   strcpy(place_screen[1], "DP Screen");
   place_d[2].refused = true;
   place_game(1, false);
   place_at(0);
   CHECK(placed(0, 0u, 1u, 0u, 0));
   place_in.switching = true;
   place_game(1, true);
   place_at(100000);
   CHECK(placed(0, 0u, 1u, 0u, 0));
   place_game(2, false);
   place_at(116667);
   CHECK(placed(0, 0u, 3u, 0u, 0) && !place_host.make);
   place_in.switching = false;
   place_at(133333);
   CHECK(placed(1, 1u, 2u, PLACE_MENU | PLACE_NOTE, 0));
   CHECK(place_host.make == 1 && place_host.host == 3);
   /* One whose load fails ends on the dummy core: a close */
   place_reset(1, 3);
   place_in.menu_follows = 1;
   place_game(2, false);
   place_at(0);
   CHECK(placed(2, 2u, 1u, PLACE_MENU, 0));
   place_in.shown     = 2;
   place_in.switching = true;
   place_game(0, false);
   place_at(100000);
   CHECK(!place_r.move && place_host.host == 1);
   place_in.switching = false;
   place_at(116667);
   CHECK(placed(2, 0u, 0u, 0u, 2) && place_r.keep == 2);
   /* A start without a map: its first map soon after is the start's */
   place_reset(1, 3);
   strcpy(place_screen[1], "DP Screen");
   place_d[2].refused = true;
   place_game(1, false);
   place_at(0);
   place_in.switching = true;
   place_game(1, true);
   place_at(5000000);
   place_in.switching = false;
   place_at(5016667);
   CHECK(placed(0, 0u, 1u, 0u, 0));
   place_game(2, false);
   place_at(5033333);
   CHECK(placed(1, 1u, 2u, PLACE_MENU | PLACE_NOTE, 0));
   CHECK(place_host.make == 1);
}

/* A display listed without an ordinal is never Auto's, even listed
 * after the panel it would replace */
static void test_place_no_ordinal(void)
{
   place_reset(1, 3);
   test_display(&place_d[2], 0, VIDEO_WINDOW_CONNECTION_UNKNOWN, false);
   strcpy(place_d[2].name, "No Ordinal");
   place_in.menu_follows = 1;
   place_at(0);
   CHECK(place_r.menu == 2);
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

/* Desktop: a live window whose monitor, size or screens changed is made
 * again, the others are left */
static void test_windows_steps_follow(void)
{
   static video_windows_frame_t frame;
   video_windows_held_t held;
   unsigned retire;
   unsigned create;
   memset(&held, 0, sizeof(held));
   memset(&frame, 0, sizeof(frame));
   frame.monitors            = true;
   frame.window_screens[1]   = 2u;
   frame.window_screens[2]   = 4u;
   frame.settings[1].mode    = VIDEO_WINDOW_SEPARATE;
   frame.settings[2].mode    = VIDEO_WINDOW_SEPARATE;
   frame.settings[1].monitor = 2;
   frame.settings[2].monitor = 2;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0u && create == 6u);
   held.live = create;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0u && create == 0u);
   /* Game Screen 2 to another monitor: its window alone */
   frame.settings[1].monitor = 1;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 2u && create == 2u);
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0u && create == 0u);
   /* A size, fullscreen, a position, a position set */
   frame.settings[2].dims = VIDEO_SCALE_PACK(640, 480);
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 4u && create == 4u);
   frame.settings[2].fullscreen = true;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 4u && create == 4u);
   frame.settings[2].pos = VIDEO_POS_PACK(10, 20);
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 4u && create == 4u);
   frame.settings[2].pos_set = true;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 4u && create == 4u);
   /* An id that now shows another screen, as Game Screen 1's spare id
    * can */
   frame.window_screens[2] = 1u;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 4u && create == 4u);
   /* A kept window is left */
   frame.window_keep         = 4u;
   frame.settings[2].monitor = 1;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0u);
   frame.window_keep         = 0u;
   /* Android (no monitors): a changed window is left as it is */
   frame.monitors            = false;
   frame.window_screens[2]   = 4u;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0u && create == 0u);
   /* One taken over from another driver is kept as it is first seen */
   memset(&held, 0, sizeof(held));
   held.live      = 6u;
   frame.monitors = true;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0u && create == 0u);
   frame.settings[1].monitor = 2;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 2u && create == 2u);
}

/* A window lives while it shows screens or a UI layer, or is kept */
static void test_windows_steps_ui(void)
{
   video_windows_frame_t frame;
   video_windows_held_t held;
   unsigned retire, create;
   memset(&frame, 0, sizeof(frame));
   memset(&held, 0, sizeof(held));
   frame.display = 2;
   frame.window_ui[VIDEO_WINDOWS_PRESENTATION] =
      VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU);
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0 && create == 2u);
   held.live       = 2u;
   held.display[1] = 2;
   frame.window_ui[VIDEO_WINDOWS_PRESENTATION] = 0;
   frame.window_keep = 1u << VIDEO_WINDOWS_PRESENTATION;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0 && create == 0);
   frame.window_keep = 0;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 2u && create == 0);
   /* A keep never makes a window: not one that is down, nor one up
    * on another display */
   held.live         = 0;
   frame.window_keep = 1u << VIDEO_WINDOWS_PRESENTATION;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0 && create == 0);
   held.live       = 2u;
   held.display[1] = 3;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 2u && create == 0);

   /* Taken from the last driver: held while kept, let go when the keep
    * ends, held for screens on its display, made again on another */
   memset(&frame, 0, sizeof(frame));
   memset(&held, 0, sizeof(held));
   held.live         = 2u;
   held.display[1]   = 2;
   frame.display     = 2;
   frame.restore     = 5;
   frame.window_keep = 1u << VIDEO_WINDOWS_PRESENTATION;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0 && create == 0);
   frame.display     = 0;
   frame.window_keep = 0;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 2u);
   memset(&held, 0, sizeof(held));
   held.live         = 2u;
   held.display[1]   = 2;
   frame.display     = 2;
   frame.window_screens[VIDEO_WINDOWS_PRESENTATION] = 2u;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 0 && create == 0);
   frame.display     = 3;
   video_windows_steps(&held, &frame, &retire, &create);
   CHECK(retire == 2u && create == 2u);
}

static unsigned widgets_asked;
static bool widgets_visible(bool visible)
{
   widgets_asked++;
   return visible;
}

static void test_window_ui_drawn(void)
{
   const unsigned menu  = VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU);
   const unsigned notes = VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_NOTIFICATIONS);
   const char *none     = NULL;
   const char *empty    = "";
   const char *text     = "Saved state";
   /* Nothing away, nothing drawn away */
   CHECK(VIDEO_WINDOW_UI_DRAWN(0u, true, text, true, true) == 0);
   /* A closed menu has nothing to draw; a message is not the menu's */
   CHECK(VIDEO_WINDOW_UI_DRAWN(menu, false, text, true, true) == 0);
   CHECK(VIDEO_WINDOW_UI_DRAWN(menu, true, none, false, false) == menu);
   CHECK(VIDEO_WINDOW_UI_DRAWN(notes, true, none, false, false) == 0);
   CHECK(VIDEO_WINDOW_UI_DRAWN(notes, false, empty, false, false) == 0);
   CHECK(VIDEO_WINDOW_UI_DRAWN(notes, false, text, false, false) == notes);
   CHECK(VIDEO_WINDOW_UI_DRAWN(notes, false, none, true, false) == notes);
   CHECK(VIDEO_WINDOW_UI_DRAWN(notes, false, none, false, true) == notes);
   CHECK(VIDEO_WINDOW_UI_DRAWN(menu | notes, true, none, false, false)
         == menu);
   CHECK(VIDEO_WINDOW_UI_DRAWN(menu | notes, false, none, false, true)
         == notes);
   /* The widgets are asked only when nothing else answered */
   widgets_asked = 0;
   CHECK(VIDEO_WINDOW_UI_DRAWN(menu, true, none, false,
            widgets_visible(true)) == menu);
   CHECK(VIDEO_WINDOW_UI_DRAWN(notes, false, text, false,
            widgets_visible(true)) == notes);
   CHECK(VIDEO_WINDOW_UI_DRAWN(notes, false, none, true,
            widgets_visible(true)) == notes);
   CHECK(widgets_asked == 0);
   CHECK(VIDEO_WINDOW_UI_DRAWN(notes, false, none, false,
            widgets_visible(false)) == 0);
   CHECK(widgets_asked == 1);
}

int main(void)
{
   test_windows_policy();
   test_windows_attached();
   test_resolve();
   test_spare_window();
   test_display_own();
   test_display_key();
   test_display_key_unnamed();
   test_display_record();
   test_display_default();
   test_place_where();
   test_place_external();
   test_place_moves();
   test_place_no_ordinal();
   test_selected_layout();
   test_second_output_touch();
   test_window_events();
   test_window_place();
   test_windows_steps();
   test_windows_steps_follow();
   test_windows_steps_ui();
   test_window_ui_drawn();
   if (failures)
      return 1;
   puts("video_windows: ok");
   return 0;
}
