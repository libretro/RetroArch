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

/* The Stereo 3D & Screens list with Headset Output off and on.
 *
 * Links the shipping objects with only main() replaced (the
 * playlist_nav pattern). A headset gives each eye its own image, so
 * the list leaves Stereo Mode out while Headset Output is on, and
 * pressing Headset Output rebuilds the open list with the cursor still
 * on it. Headset Refresh Rate sits under Headset Output, and with no
 * headset lists the fixed rates.
 *
 * The video driver stays null: the list reads only the driver's name,
 * and a press's reinit runs under "null", so no Vulkan device or
 * OpenXR runtime is made.
 *
 * Requires a completed non-Qt build with OpenXR:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/stereo_headset/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <compat/strl.h>
#include <lists/file_list.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <features/features_cpu.h>
#include <retro_miscellaneous.h>

#include "../../../msg_hash_lbl_str.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../menu/menu_cbs.h"
#include "../../../menu/menu_setting.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../menu/menu_displaylist.h"
#include "../../../input/input_driver.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../gfx/video_driver.h"
#include "../../../frontend/frontend_driver.h"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

#ifdef HAVE_OPENXR
/* The drivers with a headset path, and one without. A build has
 * Vulkan's, the desktop GL drivers', or both. */
static const char *const headset_drivers[] = {
#ifdef HAVE_VULKAN
   "vulkan",
#endif
#ifdef HAVE_OPENXR_GL
   "glcore", "gl",
#endif
   NULL };
/* The first of them, where one will do. */
#define HEADSET_DRIVER (headset_drivers[0])
#ifdef HAVE_OPENXR_GL
#define NO_PATH_DRIVER "gl1"
#else
#define NO_PATH_DRIVER "glcore"
#endif

/* The driver the off, on and toggle lanes run under. */
static const char *lane_driver;

static file_list_t *selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

/* Index of the row whose label is @label, or -1. */
static int row(const char *label)
{
   file_list_t *buf = selection_buf();
   size_t i;
   if (!buf)
      return -1;
   for (i = 0; i < buf->size; i++)
      if (buf->list[i].label && string_is_equal(buf->list[i].label, label))
         return (int)i;
   return -1;
}

static void run_frame(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), MENU_ACTION_NOOP,
         cpu_features_get_time_usec());
}

/* What a hotplug does to the open Controllers or Port N Controls list. */
static void rebuild_as_hotplug(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                   | MENU_ST_FLAG_PREVENT_POPULATE;
   run_frame();
}

/* The type of the row under the cursor, or 0. */
static unsigned cursor_type(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *buf           = selection_buf();
   if (!buf || menu_st->selection_ptr >= buf->size)
      return 0;
   return buf->list[menu_st->selection_ptr].type;
}

static void set_driver(const char *ident)
{
   settings_t *settings = config_get_ptr();
   strlcpy(settings->arrays.video_driver, ident,
         sizeof(settings->arrays.video_driver));
}

static void set_headset(bool on)
{
   settings_t *settings = config_get_ptr();
   configuration_set_bool(settings, settings->bools.video_openxr_enable, on);
}

/* What OK on "Stereo 3D & Screens" in Video settings does; the pushed
 * list is built on the next frame. */
static void open_stereo_list(void)
{
   generic_action_ok_displaylist_push("", NULL, "", 0, 0, 0,
         ACTION_OK_DL_VIDEO_STEREO_SETTINGS_LIST);
   run_frame();
}

/* A press on the Headset Output row, with the driver name null while
 * its reinit runs. */
static void press_headset_output(unsigned action)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *buf           = selection_buf();
   int i                      = row(MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR);

   CHECK(i >= 0, "fixture: no Headset Output row to press");
   if (i < 0)
      return;
   menu_st->selection_ptr = (size_t)i;
   set_driver("null");
   menu_setting_set(buf->list[i].type, action, false);
   set_driver(lane_driver);
}

static bool cursor_on_headset_output(void)
{
   return (int)menu_state_get_ptr()->selection_ptr
      == row(MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR);
}

static void lane_off(void)
{
   set_headset(false);
   set_driver(lane_driver);
   open_stereo_list();

   CHECK(row(MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR) >= 0,
         "fixture: Headset Output is not listed under %s", lane_driver);
   CHECK(row(MENU_ENUM_LABEL_VIDEO_STEREO_MODE_STR) >= 0,
         "Stereo Mode is not listed with Headset Output off");
   CHECK(row(MENU_ENUM_LABEL_VIDEO_STEREO_SWAP_EYES_STR) >= 0,
         "Swap Eyes is not listed with Headset Output off");
   CHECK(row(MENU_ENUM_LABEL_VIDEO_SCREEN_LAYOUT_STR) >= 0,
         "Screen Layout is not listed with Headset Output off");
}

static void lane_on(void)
{
   set_headset(true);
   set_driver(lane_driver);
   open_stereo_list();

   CHECK(row(MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR) >= 0,
         "fixture: Headset Output is not listed under %s", lane_driver);
   CHECK(row(MENU_ENUM_LABEL_VIDEO_STEREO_MODE_STR) < 0,
         "Stereo Mode is listed with Headset Output on");
   CHECK(row(MENU_ENUM_LABEL_VIDEO_STEREO_SWAP_EYES_STR) >= 0,
         "Swap Eyes is not listed with Headset Output on");
   CHECK(row(MENU_ENUM_LABEL_VIDEO_SCREEN_LAYOUT_STR) >= 0,
         "Screen Layout is not listed with Headset Output on");
}

/* Under a driver without a headset path the setting has no row to
 * turn it off from, and nothing to hide. */
static void lane_on_other_driver(void)
{
   set_headset(true);
   set_driver(NO_PATH_DRIVER);
   open_stereo_list();

   CHECK(row(MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR) < 0,
         "fixture: Headset Output is listed under " NO_PATH_DRIVER);
   CHECK(row(MENU_ENUM_LABEL_VIDEO_STEREO_MODE_STR) >= 0,
         "Stereo Mode is not listed under " NO_PATH_DRIVER
         " with the setting on");
}

/* The headset's rows follow the driver's headset path: all of them, in
 * order, under one that has it; none under one that has not. */
static void lane_headset_rows(void)
{
   static const char *const rows[] = {
      MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR,
      MENU_ENUM_LABEL_VIDEO_OPENXR_REFRESH_RATE_STR,
      MENU_ENUM_LABEL_VIDEO_OPENXR_DISTANCE_STR,
      MENU_ENUM_LABEL_VIDEO_OPENXR_WIDTH_STR,
      MENU_ENUM_LABEL_VIDEO_OPENXR_PLACEMENT_STR,
      MENU_ENUM_LABEL_VIDEO_OPENXR_CONTROLLERS_STR,
      MENU_ENUM_LABEL_VIDEO_OPENXR_LASER_STR };
   unsigned i, d;

#ifdef HAVE_VULKAN
   CHECK(video_driver_has_headset_path("vulkan"),
         "vulkan has no headset path");
#else
   CHECK(!video_driver_has_headset_path("vulkan"),
         "vulkan has a headset path without HAVE_VULKAN");
#endif
#ifdef HAVE_OPENXR_GL
   CHECK(video_driver_has_headset_path("glcore"), "glcore headset path");
   CHECK(video_driver_has_headset_path("gl"), "gl headset path");
#else
   CHECK(     !video_driver_has_headset_path("gl")
         && !video_driver_has_headset_path("glcore"),
         "a GL driver has a headset path without HAVE_OPENXR_GL");
#endif
   CHECK(!video_driver_has_headset_path("null"), "null headset path");
   CHECK(!video_driver_has_headset_path("gl1"), "gl1 headset path");

   set_headset(false);
   for (d = 0; headset_drivers[d]; d++)
   {
      int last = -1;
      set_driver(headset_drivers[d]);
      open_stereo_list();
      for (i = 0; i < ARRAY_SIZE(rows); i++)
      {
         int r = row(rows[i]);
         CHECK(r > last, "%s: %s is at row %d, after row %d",
               headset_drivers[d], rows[i], r, last);
         last = r;
      }
   }

   set_driver(NO_PATH_DRIVER);
   open_stereo_list();
   for (i = 0; i < ARRAY_SIZE(rows); i++)
      CHECK(row(rows[i]) < 0, "%s is listed under " NO_PATH_DRIVER,
            rows[i]);
   set_driver(HEADSET_DRIVER);
}

/* Fixed in Room by default; each value shows its own name. */
static void lane_placement(void)
{
   static const char *const shown[] = {
      "Fixed in Room", "Follow Head", "Lazy Follow" };
   char s[64];
   unsigned i;
   settings_t *settings     = config_get_ptr();
   rarch_setting_t *setting = menu_setting_find_enum(
         MENU_ENUM_LABEL_VIDEO_OPENXR_PLACEMENT);

   CHECK(setting != NULL, "fixture: no Screen Placement setting");
   if (!setting)
      return;
   CHECK(settings->uints.video_openxr_placement
         == VIDEO_OPENXR_PLACEMENT_FIXED,
         "Screen Placement is %u by default, want Fixed in Room",
         settings->uints.video_openxr_placement);
   for (i = 0; i < ARRAY_SIZE(shown); i++)
   {
      configuration_set_uint(settings,
            settings->uints.video_openxr_placement, i);
      setting->actions->repr(setting, s, sizeof(s));
      CHECK(string_is_equal(s, shown[i]),
            "placement %u shows \"%s\", want \"%s\"", i, s, shown[i]);
   }
   configuration_set_uint(settings, settings->uints.video_openxr_placement,
         VIDEO_OPENXR_PLACEMENT_FIXED);
}

static void lane_toggle(void)
{
   set_headset(false);
   set_driver(lane_driver);
   open_stereo_list();
   CHECK(row(MENU_ENUM_LABEL_VIDEO_STEREO_MODE_STR) >= 0,
         "fixture: Stereo Mode is not listed before the press");

   press_headset_output(MENU_ACTION_OK);
   run_frame();
   CHECK(config_get_ptr()->bools.video_openxr_enable,
         "fixture: pressing Headset Output did not turn it on");
   CHECK(row(MENU_ENUM_LABEL_VIDEO_STEREO_MODE_STR) < 0,
         "Stereo Mode is still listed after turning Headset Output on");
   CHECK(row(MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR) >= 0,
         "the list lost Headset Output after the press");
   CHECK(cursor_on_headset_output(),
         "the cursor left Headset Output when it was turned on");

   press_headset_output(MENU_ACTION_OK);
   run_frame();
   CHECK(!config_get_ptr()->bools.video_openxr_enable,
         "fixture: pressing Headset Output again did not turn it off");
   CHECK(row(MENU_ENUM_LABEL_VIDEO_STEREO_MODE_STR) >= 0,
         "Stereo Mode is not back after turning Headset Output off");
   CHECK(cursor_on_headset_output(),
         "the cursor left Headset Output when it was turned off");

   /* Start puts it back to off, its default. */
   press_headset_output(MENU_ACTION_RIGHT);
   run_frame();
   CHECK(config_get_ptr()->bools.video_openxr_enable,
         "fixture: right on Headset Output did not turn it on");
   CHECK(cursor_on_headset_output(),
         "the cursor left Headset Output when right turned it on");
   press_headset_output(MENU_ACTION_START);
   run_frame();
   CHECK(!config_get_ptr()->bools.video_openxr_enable,
         "fixture: Start on Headset Output did not turn it off");
   CHECK(cursor_on_headset_output(),
         "the cursor left Headset Output when Start turned it off");
}

/* Right under Headset Output. With no headset to list its rates it
 * steps through Auto, Headset's Choice and 72, 90, 120 and 144 Hz,
 * round, and its dropdown lists the same. */
static void lane_refresh_rate(void)
{
   static const unsigned want[]     = { 0, 1, 72, 90, 120, 144, 0 };
   static const char *const shown[] = { "Auto", "Headset's Choice",
      "72 Hz", "90 Hz", "120 Hz", "144 Hz", "Auto" };
   char s[64];
   unsigned i;
   int enable, rate;
   file_list_t *buf;
   settings_t *settings     = config_get_ptr();
   rarch_setting_t *setting = menu_setting_find_enum(
         MENU_ENUM_LABEL_VIDEO_OPENXR_REFRESH_RATE);

   set_headset(true);
   set_driver(HEADSET_DRIVER);
   open_stereo_list();
   enable = row(MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR);
   rate   = row(MENU_ENUM_LABEL_VIDEO_OPENXR_REFRESH_RATE_STR);
   CHECK(rate >= 0 && rate == enable + 1,
         "Headset Refresh Rate is not right under Headset Output "
         "(rows %d and %d)", enable, rate);
   CHECK(setting != NULL, "fixture: no Headset Refresh Rate setting");
   if (!setting)
      return;

   configuration_set_uint(settings,
         settings->uints.video_openxr_refresh_rate, 0);
   for (i = 0; i < sizeof(want) / sizeof(want[0]); i++)
   {
      CHECK(settings->uints.video_openxr_refresh_rate == want[i],
            "right step %u: %u, want %u", i,
            settings->uints.video_openxr_refresh_rate, want[i]);
      setting->actions->repr(setting, s, sizeof(s));
      CHECK(string_is_equal(s, shown[i]),
            "right step %u shows \"%s\", want \"%s\"", i, s, shown[i]);
      setting->actions->right(setting, 0, true);
   }

   configuration_set_uint(settings,
         settings->uints.video_openxr_refresh_rate, 0);
   setting->actions->left(setting, 0, true);
   CHECK(settings->uints.video_openxr_refresh_rate == 144,
         "left from Auto: %u, want 144",
         settings->uints.video_openxr_refresh_rate);
   configuration_set_uint(settings,
         settings->uints.video_openxr_refresh_rate, 100);
   setting->actions->right(setting, 0, true);
   CHECK(settings->uints.video_openxr_refresh_rate == 0,
         "right from an unlisted 100 Hz: %u, want Auto",
         settings->uints.video_openxr_refresh_rate);

   configuration_set_uint(settings,
         settings->uints.video_openxr_refresh_rate, 120);
   generic_action_ok_displaylist_push(NULL, NULL, NULL, 0, 0, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_HEADSET_REFRESH_RATE);
   run_frame();
   buf = selection_buf();
   CHECK(buf && buf->size == 6, "the dropdown has %u rows, want 6",
         buf ? (unsigned)buf->size : 0);
   if (buf && buf->size == 6)
   {
      CHECK(     string_is_equal(buf->list[0].path, "Auto")
            && string_is_equal(buf->list[5].path, "144 Hz"),
            "the dropdown runs \"%s\" to \"%s\"",
            buf->list[0].path, buf->list[5].path);
      CHECK(menu_state_get_ptr()->selection_ptr == 4,
            "the dropdown selects row %u, want 4 (120 Hz)",
            (unsigned)menu_state_get_ptr()->selection_ptr);
   }

   /* A menu driver passes the row's position as the press's index,
    * not the row's value. */
   for (i = 0; i < 6; i++)
   {
      static const unsigned stored[] = { 0, 1, 72, 90, 120, 144 };
      menu_file_list_cbs_t *cbs;
      file_list_t *list;

      configuration_set_uint(settings,
            settings->uints.video_openxr_refresh_rate, 999);
      generic_action_ok_displaylist_push(NULL, NULL, NULL, 0, 0, 0,
            ACTION_OK_DL_DROPDOWN_BOX_LIST_HEADSET_REFRESH_RATE);
      run_frame();
      list = selection_buf();
      cbs  = (list && list->size == 6)
         ? (menu_file_list_cbs_t*)list->list[i].actiondata : NULL;
      CHECK(cbs && cbs->action_ok, "fixture: dropdown row %u has no press",
            i);
      if (!cbs || !cbs->action_ok)
         continue;
      cbs->action_ok(list->list[i].path, list->list[i].label,
            list->list[i].type, i, list->list[i].entry_idx);
      CHECK(settings->uints.video_openxr_refresh_rate == stored[i],
            "pressing dropdown row %u stores %u, want %u", i,
            settings->uints.video_openxr_refresh_rate, stored[i]);
   }
   configuration_set_uint(settings,
         settings->uints.video_openxr_refresh_rate, 0);
}

/* A controller connecting or going above the cursor's controller in
 * Controllers leaves the cursor on it; with its own row gone, the
 * cursor stays in the list. */
static void lane_controller_rows(void)
{
   file_list_t *buf;
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned pad_b             = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 1;

   input_config_set_device_name(1, "Pad B");
   generic_action_ok_displaylist_push("", NULL, "", 0, 0, 0,
         ACTION_OK_DL_INPUT_CONTROLLERS_LIST);
   run_frame();
   menu_st->selection_ptr = 0;
   CHECK(cursor_type() == pad_b, "Pad B is not the first row");

   input_config_set_device_name(0, "Pad A");
   rebuild_as_hotplug();
   CHECK(cursor_type() == pad_b && menu_st->selection_ptr == 1,
         "a controller connecting above took the cursor (cursor %u)",
         (unsigned)menu_st->selection_ptr);

   menu_st->selection_ptr = 1;
   input_config_clear_device_name(0);
   rebuild_as_hotplug();
   CHECK(cursor_type() == pad_b && menu_st->selection_ptr == 0,
         "a controller going above took the cursor (cursor %u)",
         (unsigned)menu_st->selection_ptr);

   input_config_set_device_name(0, "Pad A");
   rebuild_as_hotplug();
   input_config_clear_device_name(1);
   rebuild_as_hotplug();
   buf = selection_buf();
   CHECK(buf && menu_st->selection_ptr < buf->size,
         "the cursor is past the list once its controller went "
         "(cursor %u)", (unsigned)menu_st->selection_ptr);
   input_config_clear_device_name(0);
}

/* Device Index: a port reading the controller another port reads
 * names the port that uses it. */
static void lane_device_index_used_by(void)
{
   char s[128];
   unsigned was;
   settings_t *settings     = config_get_ptr();
   rarch_setting_t *setting = menu_setting_find_enum(
         (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_DEVICE_INDEX + 1));

   CHECK(setting != NULL, "fixture: no Port 2 Device Index setting");
   if (!setting)
      return;
   was = settings->uints.input_joypad_index[1];
   settings->uints.input_joypad_index[1] = 0;
   setting->actions->repr(setting, s, sizeof(s));
   CHECK(strstr(s, " (used by Port 1)") != NULL,
         "Port 2's Device Index on Port 1's controller reads \"%s\"", s);
   settings->uints.input_joypad_index[1] = was;
}

/* Max Users 4 and pads A, B and C in slots 0 to 2: A and B Player 1, B
 * sharing from port 4, C Auto on port 2. */
static unsigned t_max_users_before;
static void t_three_pads(void)
{
   settings_t *settings = config_get_ptr();
   t_max_users_before   = settings->uints.input_max_users;
   configuration_set_uint(settings, settings->uints.input_max_users, 4);
   input_config_set_device_name(0, "Pad A");
   input_config_set_device_name(1, "Pad B");
   input_config_set_device_name(2, "Pad C");
   input_config_plays_as_set(0, 1);
   input_autoconfigure_reallocate_ports(0);
   input_config_plays_as_set(1, 1);
   input_autoconfigure_reallocate_ports(1);
   input_autoconfigure_reallocate_ports(2);
}

/* "null" is no joypad driver: the first that starts stands in and
 * reports the host's own pads at init. They go before any lane. */
static void t_drop_host_pads(void)
{
   unsigned s;
   task_queue_wait(NULL, NULL);
   for (s = 0; s < MAX_USERS; s++)
      if (input_config_get_device_name(s))
         input_autoconfigure_disconnect(s, input_config_get_device_name(s));
   task_queue_wait(NULL, NULL);
}

/* The pads gone, their entries let go and the ports as they were. */
static void t_forget_pads(void)
{
   unsigned s;
   settings_t *settings = config_get_ptr();
   for (s = 0; s < 4; s++)
   {
      if (input_config_get_device_name(s))
         input_config_plays_as_set(s, 0);
      input_config_clear_device_name(s);
      input_config_plays_as_release(s);
      settings->uints.input_joypad_index[s] = s;
   }
   configuration_set_uint(settings, settings->uints.input_max_users,
         t_max_users_before);
}

/* RetroPad Binds > Port @n Controls, built on the next frame. */
static void open_port_controls(unsigned n)
{
   char label[32];
   snprintf(label, sizeof(label), "%u_input_binds_list", n);
   generic_action_ok_displaylist_push("", NULL, label, 0, 0, 0,
         ACTION_OK_DL_USER_BINDS_LIST);
   run_frame();
}

/* Row @i's sublabel into @s, "" with none. */
static void row_sublabel(size_t i, char *s, size_t len)
{
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *buf          = selection_buf();
   *s = '\0';
   if (buf && i < buf->size)
      cbs = (menu_file_list_cbs_t*)buf->list[i].actiondata;
   if (cbs && cbs->action_sublabel)
      cbs->action_sublabel(buf, buf->list[i].type, (unsigned)i,
            buf->list[i].label, buf->list[i].path, s, len);
}

static bool is_plays_as_row(unsigned type)
{
   return type >= MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN
       && type <  MENU_SETTINGS_INPUT_PLAYS_AS_END;
}

/* Index of the first row of type @type, or -1. */
static int row_of_type(unsigned type)
{
   size_t i;
   file_list_t *buf = selection_buf();
   if (!buf)
      return -1;
   for (i = 0; i < buf->size; i++)
      if (buf->list[i].type == type)
         return (int)i;
   return -1;
}

/* Right, or Left, on row @i, as the menu presses it. */
static void press_lr(int i, bool right)
{
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *buf          = selection_buf();
   if (buf && i >= 0 && (size_t)i < buf->size)
      cbs = (menu_file_list_cbs_t*)buf->list[i].actiondata;
   CHECK(cbs && (right ? cbs->action_right : cbs->action_left),
         "fixture: no Left/Right on row %d", i);
   if (!cbs || !(right ? cbs->action_right : cbs->action_left))
      return;
   menu_state_get_ptr()->selection_ptr = (size_t)i;
   if (right)
      cbs->action_right(buf->list[i].type, buf->list[i].label, false);
   else
      cbs->action_left(buf->list[i].type, buf->list[i].label, false);
}

/* @action on the row under the cursor, then a frame. */
static void press(enum menu_action action)
{
   menu_driver_iterate(menu_state_get_ptr(), disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), action, cpu_features_get_time_usec());
   run_frame();
}

/* A list made into a file list of its own, as XMB and Ozone make their
 * playlist tabs when a scan ends. */
static void build_other_list(void)
{
   file_list_t other;
   menu_displaylist_info_t info;
   const char *tmp = getenv("TMPDIR");
   memset(&other, 0, sizeof(other));
   menu_displaylist_info_init(&info);
   info.list = &other;
   info.path = strdup((tmp && *tmp) ? tmp : ".");
   menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL, &info,
         config_get_ptr());
   menu_displaylist_info_free(&info);
   file_list_deinitialize(&other);
}

static size_t stack_depth(void)
{
   menu_list_t *menu_list = menu_state_get_ptr()->entries.list;
   file_list_t *stack     = menu_list ? MENU_LIST_GET(menu_list, 0) : NULL;
   return stack ? stack->size : 0;
}

/* Port N Controls lists the controllers playing as the player port N
 * feeds, in port order: its own "This port", each other by the page
 * its binds are on. It opens on its first row, and Cancel out of a
 * dropdown comes back to the row it came from. A controller joining
 * above keeps the cursor on its row, or on the bind it is on, as does
 * a change that keeps the row; a row moved off the player leaves the
 * cursor on Add Controller, and Controllers no row to find. */
static void lane_port_controls_rows(void)
{
   char s[128];
   int bind, di;
   size_t depth;
   unsigned port3;
   file_list_t *buf;
   settings_t *settings       = config_get_ptr();
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned pad_a             = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN;
   unsigned pad_b             = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 1;
   unsigned pad_c             = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 2;
   unsigned pad_d             = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 3;

   t_three_pads();

   /* from RetroPad Binds, Port 3 Controls left empty, then opened
    * again once D plays as Player 3: on D's row */
   port3 = settings->uints.input_joypad_index[2];
   generic_action_ok_displaylist_push("", NULL, "", 0, 0, 0,
         ACTION_OK_DL_INPUT_RETROPAD_BINDS_LIST);
   run_frame();
   open_port_controls(3);
   buf = selection_buf();
   CHECK(buf && buf->size && !is_plays_as_row(buf->list[0].type),
         "fixture: Port 3 Controls lists a controller");
   press(MENU_ACTION_CANCEL);
   input_config_set_device_name(3, "Pad D");
   input_config_plays_as_set(3, 3);
   input_autoconfigure_reallocate_ports(3);
   open_port_controls(3);
   CHECK(menu_st->selection_ptr == 0 && cursor_type() == pad_d,
         "Port 3 Controls opened again with D on row %u",
         (unsigned)menu_st->selection_ptr);
   input_config_plays_as_set(3, 0);
   input_config_clear_device_name(3);
   input_config_plays_as_release(3);
   settings->uints.input_joypad_index[2] = port3;

   open_port_controls(1);
   CHECK(menu_st->selection_ptr == 0,
         "Port 1 Controls opened on row %u",
         (unsigned)menu_st->selection_ptr);
   buf = selection_buf();
   CHECK(buf && buf->size > 2
         && buf->list[0].type == pad_a && buf->list[1].type == pad_b
         && !is_plays_as_row(buf->list[2].type),
         "Port 1 Controls does not start with rows for A and B");
   row_sublabel(0, s, sizeof(s));
   CHECK(string_is_equal(s, "This port"),
         "A's sublabel on Port 1 Controls is \"%s\"", s);
   row_sublabel(1, s, sizeof(s));
   CHECK(string_is_equal(s, "Shared \xC2\xB7 binds in Port 4 Controls"),
         "B's sublabel on Port 1 Controls is \"%s\"", s);

   /* OK into B's Plays As and Device Index, each Cancelled */
   menu_st->selection_ptr = 1;
   depth = stack_depth();
   press(MENU_ACTION_OK);
   CHECK(stack_depth() == depth + 1, "fixture: OK on B opened nothing");
   press(MENU_ACTION_CANCEL);
   CHECK(menu_st->selection_ptr == 1 && cursor_type() == pad_b,
         "Cancel out of B's Plays As left the cursor on row %u",
         (unsigned)menu_st->selection_ptr);
   di = row("input_player1_joypad_index");
   CHECK(di == 4, "fixture: Device Index is row %d, not 4", di);
   if (di >= 0)
   {
      menu_st->selection_ptr = (size_t)di;
      press(MENU_ACTION_OK);
      CHECK(stack_depth() == depth + 1,
            "fixture: OK on Device Index opened nothing");
      press(MENU_ACTION_CANCEL);
      CHECK((int)menu_st->selection_ptr == di,
            "Cancel out of Device Index left the cursor on row %u",
            (unsigned)menu_st->selection_ptr);
   }

   open_port_controls(4);
   CHECK(menu_st->selection_ptr == 0,
         "Port 4 Controls opened from Port 1's on row %u",
         (unsigned)menu_st->selection_ptr);
   buf = selection_buf();
   CHECK(buf && buf->size > 2
         && buf->list[0].type == pad_a && buf->list[1].type == pad_b
         && !is_plays_as_row(buf->list[2].type),
         "Port 4 Controls, B's, does not list Player 1's A and B");
   row_sublabel(0, s, sizeof(s));
   CHECK(string_is_equal(s, "Shared \xC2\xB7 binds in Port 1 Controls"),
         "A's sublabel on Port 4 Controls is \"%s\"", s);
   row_sublabel(1, s, sizeof(s));
   CHECK(string_is_equal(s, "This port"),
         "B's sublabel on Port 4 Controls is \"%s\"", s);

   open_port_controls(2);
   CHECK(menu_st->selection_ptr == 0,
         "Port 2 Controls opened from Port 4's on row %u",
         (unsigned)menu_st->selection_ptr);
   buf = selection_buf();
   CHECK(buf && buf->size > 1 && buf->list[0].type == pad_c
         && !is_plays_as_row(buf->list[1].type),
         "Port 2 Controls does not list C alone");

   /* D joins Player 1 from port 3, above B's row */
   open_port_controls(1);
   CHECK(menu_st->selection_ptr == 0,
         "Port 1 Controls opened from Port 2's on row %u",
         (unsigned)menu_st->selection_ptr);
   menu_st->selection_ptr = 1;
   input_config_set_device_name(3, "Pad D");
   input_config_plays_as_set(3, 1);
   input_autoconfigure_reallocate_ports(3);
   rebuild_as_hotplug();
   CHECK(cursor_type() == pad_b && menu_st->selection_ptr == 2,
         "a controller joining above B took the cursor (cursor %u)",
         (unsigned)menu_st->selection_ptr);

   /* the cursor on the B button's bind while D, its row and its link,
    * goes and comes back */
   bind = row("p1_b");
   CHECK(bind > 2, "fixture: no B button bind after the controller rows");
   menu_st->selection_ptr = (size_t)bind;
   build_other_list();
   input_config_clear_device_name(3);
   rebuild_as_hotplug();
   CHECK((int)menu_st->selection_ptr == row("p1_b")
         && (int)menu_st->selection_ptr == bind - 2,
         "a controller going above the binds moved the cursor off "
         "the B button (cursor %u)", (unsigned)menu_st->selection_ptr);
   input_config_set_device_name(3, "Pad D");
   rebuild_as_hotplug();
   CHECK((int)menu_st->selection_ptr == row("p1_b")
         && (int)menu_st->selection_ptr == bind,
         "a controller joining above the binds moved the cursor off "
         "the B button (cursor %u)", (unsigned)menu_st->selection_ptr);

   /* Right takes D to Player 2: the cursor goes past B, so a held
    * Right stops */
   press_lr(row_of_type(pad_d), true);
   run_frame();
   CHECK(row_of_type(pad_d) < 0,
         "D on Player 2 is still on Port 1 Controls");
   CHECK(menu_st->selection_ptr == 2
         && row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR) == 2,
         "D leaving Port 1 Controls left the cursor on row %u, not on "
         "Add Controller after A's and B's",
         (unsigned)menu_st->selection_ptr);
   CHECK(menu_st->driver_data && !menu_st->driver_data->plays_as_row,
         "Port 1 Controls left plays_as_row %u for Controllers",
         menu_st->driver_data ? menu_st->driver_data->plays_as_row : 0);

   /* Right takes C, on Auto on its own port, to Player 1: its page
    * follows it to Player 1's rows, and the cursor follows C down them */
   open_port_controls(input_config_slot_user(2) + 1);
   CHECK(row_of_type(pad_c) == 0, "fixture: C is not row 0 of Port %u "
         "Controls (%d)", input_config_slot_user(2) + 1, row_of_type(pad_c));
   press_lr(row_of_type(pad_c), true);
   run_frame();
   CHECK(row_of_type(pad_c) > 0,
         "fixture: C's row did not move (%d)", row_of_type(pad_c));
   CHECK(cursor_type() == pad_c,
         "C on Player 1 left the cursor on row %u (C on row %d)",
         (unsigned)menu_st->selection_ptr, row_of_type(pad_c));
   t_forget_pads();
}

/* OK on row @i as the menu presses it; what it pushes is built on the
 * next frame. */
static void press_ok(int i)
{
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *buf          = selection_buf();
   if (buf && i >= 0 && (size_t)i < buf->size)
      cbs = (menu_file_list_cbs_t*)buf->list[i].actiondata;
   CHECK(cbs && cbs->action_ok, "fixture: row %d has no OK", i);
   if (!cbs || !cbs->action_ok)
      return;
   menu_state_get_ptr()->selection_ptr = (size_t)i;
   cbs->action_ok(buf->list[i].path, buf->list[i].label,
         buf->list[i].type, (size_t)i, buf->list[i].entry_idx);
   run_frame();
}

/* After @how, Port 1 Controls lists B alone (@on_b) or A and B, A's
 * or B's row "This port", with the cursor on Device Index (@cursor). */
static void check_port1_rows(bool on_b, bool cursor, const char *how)
{
   char s[128];
   int di = row("input_player1_joypad_index");
   unsigned pad_a = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN;
   unsigned pad_b = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 1;

   CHECK(config_get_ptr()->uints.input_joypad_index[0] == (on_b ? 1u : 0u),
         "fixture: %s left Port 1 on slot %u", how,
         config_get_ptr()->uints.input_joypad_index[0]);
   row_sublabel(0, s, sizeof(s));
   if (on_b)
      CHECK(row_of_type(pad_a) < 0 && row_of_type(pad_b) == 0
            && string_is_equal(s, "This port"),
            "%s to B: Port 1 Controls does not list B alone as "
            "\"This port\" (row 0 \"%s\")", how, s);
   else
      CHECK(row_of_type(pad_a) == 0 && row_of_type(pad_b) == 1
            && string_is_equal(s, "This port"),
            "%s to A: Port 1 Controls does not list A as \"This port\" "
            "and B (row 0 \"%s\")", how, s);
   CHECK(!cursor || (di >= 0
            && (int)menu_state_get_ptr()->selection_ptr == di),
         "%s left the cursor on row %u, not on Device Index (row %d)",
         how, (unsigned)menu_state_get_ptr()->selection_ptr, di);
}

/* Port N Controls is made again when its Device Index changes, by
 * Left/Right, Start or the dropdown: its controller rows follow which
 * controller the port reads. A dropdown pick leaves the cursor at its
 * index, as the page record goes with the dropdown's build. */
static void lane_port_controls_device_index(void)
{
   const char *di = "input_player1_joypad_index";

   t_three_pads();
   open_port_controls(1);
   CHECK(row(di) == 4, "fixture: Device Index is row %d, not 4", row(di));
   if (row(di) < 0)
   {
      t_forget_pads();
      return;
   }
   press_lr(row(di), true);
   run_frame();
   check_port1_rows(true, true, "Right");
   press_lr(row(di), false);
   run_frame();
   check_port1_rows(false, true, "Left");
   press_lr(row(di), true);
   run_frame();
   menu_state_get_ptr()->selection_ptr = (size_t)row(di);
   press(MENU_ACTION_START);
   check_port1_rows(false, true, "Start");

   menu_state_get_ptr()->selection_ptr = (size_t)row(di);
   press(MENU_ACTION_OK);
   press_ok(1);
   check_port1_rows(true, false, "A dropdown pick");
   t_forget_pads();
}

/* How many rows link to another port's Port N Controls: RetroPad
 * Binds' entries in Settings, Quick Menu > Controls' (@remaps). */
static int port_links(bool remaps)
{
   size_t i;
   int n            = 0;
   file_list_t *buf = selection_buf();
   if (!buf)
      return -1;
   for (i = 0; i < buf->size; i++)
      if (remaps
            ? (   buf->list[i].type >= MENU_SETTINGS_REMAPPING_PORT_BEGIN
               && buf->list[i].type <  MENU_SETTINGS_REMAPPING_PORT_END)
            : string_ends_with(buf->list[i].label, "_input_binds_list"))
         n++;
   return n;
}

/* Once a lane's controllers are forgotten, no Plays As entry is left
 * for the next. */
static void check_no_entries(void)
{
   unsigned e;
   for (e = 0; e < MAX_USERS; e++)
      CHECK(!input_config_plays_as_entry_name(e),
            "fixture: entry %u (\"%s\") is left", e,
            input_config_plays_as_entry_name(e));
}

/* D connects playing as Player 1 from port 3, and the open list is
 * made again as the hotplug makes it. */
static void connect_pad_d(void)
{
   input_config_set_device_name(3, "Pad D");
   input_config_plays_as_set(3, 1);
   input_autoconfigure_reallocate_ports(3);
   rebuild_as_hotplug();
}

/* Port N Controls links each controller playing as player N from
 * another port to that port's page, where its binds are: after Add
 * Controller, named for the controller and the page, opening what
 * RetroPad Binds' own entry opens. A controller on its own port, the
 * page's own and a sharer's page have none, and a sharer's row still
 * opens its Plays As. A sharer joining above keeps the cursor on Add
 * Controller, or on the bind it is on; one leaving with the cursor on
 * its link leaves it on the last link. */
static void lane_port_controls_links(void)
{
   char s[256];
   int link, add, bind;
   size_t depth;
   file_list_t *buf;
   const char *path           = NULL;
   const char *label          = NULL;
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned pad_b             = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 1;

   t_three_pads();
   open_port_controls(1);
   buf  = selection_buf();
   link = row("4_input_binds_list");
   add  = row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR);
   CHECK(link == 3 && add == 2 && port_links(false) == 1,
         "Port 1 Controls does not link to B's Port 4 Controls alone, "
         "after Add Controller (row %d, %d links)", link,
         port_links(false));
   if (!buf || link < 0)
   {
      t_forget_pads();
      return;
   }
   CHECK(string_is_equal(buf->list[link].path, "Pad B (Port 4 Controls)")
         && buf->list[link].type == MENU_SETTING_ACTION,
         "B's link reads \"%s\"", buf->list[link].path);
   row_sublabel(link, s, sizeof(s));
   CHECK(string_is_equal(s,
            msg_hash_to_str(MENU_ENUM_SUBLABEL_INPUT_USER_BINDS)),
         "B's link's sublabel is \"%s\"", s);

   depth = stack_depth();
   press_ok(link);
   menu_entries_get_last_stack(&path, &label, NULL, NULL, NULL);
   menu_entries_get_title(s, sizeof(s));
   CHECK(stack_depth() == depth + 1
         && string_is_equal(label,
               MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST_STR)
         && string_is_equal(path, "4_input_binds_list")
         && string_is_equal(s, "Port 4 Controls")
         && row("input_player4_joypad_index") >= 0,
         "OK on B's link did not open Port 4 Controls (\"%s\")", s);
   CHECK(port_links(false) == 0,
         "Port 4 Controls, B's own, links to %d pages", port_links(false));
   press(MENU_ACTION_CANCEL);
   CHECK((int)menu_st->selection_ptr == link
         && row("4_input_binds_list") == link,
         "Cancel out of Port 4 Controls left the cursor on row %u",
         (unsigned)menu_st->selection_ptr);

   press_ok(row_of_type(pad_b));
   menu_entries_get_last_stack(NULL, &label, NULL, NULL, NULL);
   CHECK(string_is_equal(label,
            MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_PLAYS_AS_STR),
         "OK on B's row opened \"%s\", not its Plays As", label);
   press(MENU_ACTION_CANCEL);

   open_port_controls(2);
   CHECK(port_links(false) == 0,
         "Port 2 Controls, C's alone, links to %d pages", port_links(false));

   /* D joins Player 1 from port 3: its link comes before B's */
   open_port_controls(1);
   menu_st->selection_ptr = (size_t)add;
   connect_pad_d();
   buf = selection_buf();
   CHECK(row("3_input_binds_list") == 4 && row("4_input_binds_list") == 5
         && string_is_equal(buf->list[4].path, "Pad D (Port 3 Controls)"),
         "with D sharing from port 3, Port 1 Controls' links are not D's "
         "then B's (rows %d, %d)", row("3_input_binds_list"),
         row("4_input_binds_list"));
   CHECK((int)menu_st->selection_ptr == add + 1
         && row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR) == add + 1,
         "a sharer joining above Add Controller took the cursor (cursor %u)",
         (unsigned)menu_st->selection_ptr);
   open_port_controls(3);
   CHECK(port_links(false) == 0,
         "Port 3 Controls, D's, links to %d pages", port_links(false));
   open_port_controls(4);
   CHECK(port_links(false) == 0,
         "Port 4 Controls, B's, links to %d pages", port_links(false));

   /* B goes with the cursor on its link, the last */
   open_port_controls(1);
   menu_st->selection_ptr = (size_t)row("4_input_binds_list");
   input_config_clear_device_name(1);
   rebuild_as_hotplug();
   CHECK(row("4_input_binds_list") < 0
         && (int)menu_st->selection_ptr == row("3_input_binds_list"),
         "B going with the cursor on its link left it on row %u, not on "
         "D's link (row %d)", (unsigned)menu_st->selection_ptr,
         row("3_input_binds_list"));
   input_config_set_device_name(1, "Pad B");
   rebuild_as_hotplug();
   bind = row("p1_b");
   menu_st->selection_ptr = (size_t)bind;
   input_config_clear_device_name(3);
   rebuild_as_hotplug();
   CHECK((int)menu_st->selection_ptr == row("p1_b")
         && (int)menu_st->selection_ptr == bind - 2,
         "a sharer going, its row and its link, moved the cursor off the "
         "B button (cursor %u)", (unsigned)menu_st->selection_ptr);
   input_config_set_device_name(3, "Pad D");
   t_forget_pads();
   check_no_entries();
}

/* Quick Menu > Controls > Port @port + 1 Controls, built on the next
 * frame. */
static void open_remap_port(unsigned port)
{
   char label[16];
   snprintf(label, sizeof(label), "%u", port);
   generic_action_ok_displaylist_push("", NULL, label, 0, 0, 0,
         ACTION_OK_DL_REMAPPINGS_PORT_LIST);
   run_frame();
}

/* Quick Menu > Controls > Port N Controls starts with the same links,
 * to the pages the sharers' remaps are on, opening what Controls' own
 * entry opens. A sharer's connect or disconnect makes the list again
 * with the cursor on its row, a remap or the last; one leaving with
 * the cursor on its link leaves it on the last link. */
static void lane_remap_port_links(void)
{
   char s[256];
   unsigned e, remap, last;
   size_t depth;
   file_list_t *buf;
   const char *path           = NULL;
   const char *label          = NULL;
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned to_port4          = MENU_SETTINGS_REMAPPING_PORT_BEGIN + 3;

   t_three_pads();
   open_remap_port(0);
   buf = selection_buf();
   CHECK(buf && buf->size > 1 && buf->list[0].type == to_port4
         && string_is_equal(buf->list[0].path, "Pad B (Port 4 Controls)")
         && string_is_equal(buf->list[0].label, "3")
         && port_links(true) == 1,
         "Port 1 Controls in the Quick Menu does not start with a link to "
         "B's Port 4 Controls alone (%d links)", port_links(true));
   if (!buf || !buf->size || buf->list[0].type != to_port4)
   {
      t_forget_pads();
      return;
   }
   row_sublabel(0, s, sizeof(s));
   CHECK(string_is_equal(s,
            msg_hash_to_str(MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS)),
         "B's link's sublabel is \"%s\"", s);

   depth = stack_depth();
   press_ok(0);
   menu_entries_get_last_stack(&path, &label, NULL, NULL, NULL);
   menu_entries_get_title(s, sizeof(s));
   CHECK(stack_depth() == depth + 1
         && string_is_equal(label,
               MENU_ENUM_LABEL_DEFERRED_REMAPPINGS_PORT_LIST_STR)
         && string_is_equal(path, "3")
         && string_is_equal(s, "Port 4 Controls")
         && row("p4_b") >= 0,
         "OK on B's link did not open Port 4 Controls (\"%s\")", s);
   CHECK(port_links(true) == 0,
         "Port 4 Controls, B's own, links to %d pages", port_links(true));
   press(MENU_ACTION_CANCEL);
   CHECK(menu_st->selection_ptr == 0 && cursor_type() == to_port4,
         "Cancel out of Port 4 Controls left the cursor on row %u",
         (unsigned)menu_st->selection_ptr);

   open_remap_port(1);
   CHECK(port_links(true) == 0,
         "Port 2 Controls, C's alone, links to %d pages", port_links(true));

   /* D connects sharing Player 1 from port 3, the cursor on the B
    * button's remap, then disconnects */
   open_remap_port(0);
   menu_st->selection_ptr = (size_t)row("p1_b");
   remap = cursor_type();
   connect_pad_d();
   buf = selection_buf();
   CHECK(port_links(true) == 2 && buf->list[0].type == to_port4 - 1
         && string_is_equal(buf->list[0].path, "Pad D (Port 3 Controls)"),
         "with D sharing from port 3, the Quick Menu's Port 1 Controls "
         "does not link to D's page first");
   CHECK(remap && cursor_type() == remap,
         "D connecting moved the cursor off the B button's remap (row %u)",
         (unsigned)menu_st->selection_ptr);
   open_remap_port(2);
   CHECK(port_links(true) == 0,
         "Port 3 Controls, D's, links to %d pages", port_links(true));
   open_remap_port(3);
   CHECK(port_links(true) == 0,
         "Port 4 Controls, B's, links to %d pages", port_links(true));
   open_remap_port(0);
   menu_st->selection_ptr = (size_t)row("p1_b");
   input_autoconfigure_disconnect(3, "Pad D");
   task_queue_wait(NULL, NULL);
   run_frame();
   CHECK(port_links(true) == 1 && selection_buf()->list[0].type == to_port4,
         "D's disconnect left the Quick Menu's Port 1 Controls with %d links",
         port_links(true));
   CHECK(cursor_type() == remap,
         "D disconnecting moved the cursor off the B button's remap (row %u)",
         (unsigned)menu_st->selection_ptr);

   /* and again with the cursor on the last row */
   buf = selection_buf();
   menu_st->selection_ptr = buf->size - 1;
   last = cursor_type();
   connect_pad_d();
   CHECK(port_links(true) == 2 && cursor_type() == last,
         "D connecting moved the cursor off the last row (row %u)",
         (unsigned)menu_st->selection_ptr);
   menu_st->selection_ptr = selection_buf()->size - 1;
   input_autoconfigure_disconnect(3, "Pad D");
   task_queue_wait(NULL, NULL);
   run_frame();
   CHECK(port_links(true) == 1 && cursor_type() == last,
         "D disconnecting moved the cursor off the last row (row %u)",
         (unsigned)menu_st->selection_ptr);

   /* B disconnects with the cursor on its link, the last */
   connect_pad_d();
   menu_st->selection_ptr = 1;
   CHECK(cursor_type() == to_port4, "fixture: B's link is not row 1");
   input_autoconfigure_disconnect(1, "Pad B");
   task_queue_wait(NULL, NULL);
   run_frame();
   CHECK(port_links(true) == 1 && menu_st->selection_ptr == 0
         && cursor_type() == to_port4 - 1,
         "B disconnecting with the cursor on its link left it on row %u, "
         "not on D's link", (unsigned)menu_st->selection_ptr);
   /* D's entry, kept by its disconnect */
   for (e = 0; e < MAX_USERS; e++)
      if (     input_config_plays_as_entry_name(e)
            && !input_config_plays_as_entry_taken(e))
         input_config_plays_as_set_entry(e, 0);
   t_forget_pads();
   check_no_entries();
}

/* Port N Controls links the controllers playing as player N, whatever
 * port N's own plays as: A Player 1, C and B Player 4, then C Player 1
 * leaves B playing as Player 4 from port 3 and C sharing Player 1 from
 * port 4. Port 4 Controls links to B's page, B's has none, and Port 1
 * Controls links to C's. */
static void lane_port_links_by_port(void)
{
   file_list_t *buf;
   int link;
   settings_t *settings = config_get_ptr();

   t_max_users_before   = settings->uints.input_max_users;
   configuration_set_uint(settings, settings->uints.input_max_users, 4);
   input_config_set_device_name(0, "Pad A");
   input_config_set_device_name(1, "Pad B");
   input_config_set_device_name(2, "Pad C");
   input_config_plays_as_set(0, 1);
   input_autoconfigure_reallocate_ports(0);
   input_config_plays_as_set(2, 4);
   input_autoconfigure_reallocate_ports(2);
   input_config_plays_as_set(1, 4);
   input_autoconfigure_reallocate_ports(1);
   input_config_plays_as_set(2, 1);
   input_autoconfigure_reallocate_ports(2);
   CHECK(input_config_slot_user(1) == 2 && input_config_user_player(2) == 3
         && input_config_slot_user(2) == 3 && input_config_user_player(3) == 0,
         "fixture: B is not on port 3 as Player 4 (port %u), or C not on "
         "port 4 as Player 1 (port %u)", input_config_slot_user(1) + 1,
         input_config_slot_user(2) + 1);

   open_port_controls(4);
   buf  = selection_buf();
   link = row("3_input_binds_list");
   CHECK(buf && link >= 0 && port_links(false) == 1
         && string_is_equal(buf->list[link].path, "Pad B (Port 3 Controls)"),
         "Port 4 Controls does not link to B's Port 3 Controls alone "
         "(%d links)", port_links(false));
   open_remap_port(3);
   buf = selection_buf();
   CHECK(buf && buf->size && port_links(true) == 1
         && buf->list[0].type == MENU_SETTINGS_REMAPPING_PORT_BEGIN + 2
         && string_is_equal(buf->list[0].path, "Pad B (Port 3 Controls)"),
         "the Quick Menu's Port 4 Controls does not start with a link to "
         "B's Port 3 Controls alone (%d links)", port_links(true));
   open_port_controls(3);
   CHECK(port_links(false) == 0,
         "Port 3 Controls, B's, links to %d pages", port_links(false));
   open_remap_port(2);
   CHECK(port_links(true) == 0,
         "the Quick Menu's Port 3 Controls, B's, links to %d pages",
         port_links(true));
   open_port_controls(1);
   CHECK(row("4_input_binds_list") >= 0 && port_links(false) == 1,
         "Port 1 Controls does not link to C's Port 4 Controls alone");
   t_forget_pads();
   check_no_entries();
}

/* Add Controller lists the controllers not playing as the page's
 * player, and Cancel comes back to it; picking one moves it there,
 * back on the page with the cursor on its row, not on the row now at
 * Add Controller's; with none left, the list says so. */
static void lane_port_controls_add(void)
{
   char title[64];
   int add;
   size_t depth;
   file_list_t *buf;
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned pad_b             = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 1;
   unsigned pad_c             = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 2;

   t_three_pads();
   open_port_controls(1);
   add = row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR);
   CHECK(add == 2, "Add Controller is not right after A and B (row %d)",
         add);
   depth = stack_depth();
   press_ok(add);
   CHECK(stack_depth() == depth + 1,
         "fixture: OK on Add Controller opened nothing");
   press(MENU_ACTION_CANCEL);
   CHECK((int)menu_st->selection_ptr == add
         && row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR) == add,
         "Cancel out of Add Controller left the cursor on row %u",
         (unsigned)menu_st->selection_ptr);

   press_ok(add);
   menu_entries_get_title(title, sizeof(title));
   CHECK(string_is_equal(title, "Add Controller"),
         "the Add Controller list's title is \"%s\"", title);
   buf = selection_buf();
   CHECK(buf && buf->size == 1
         && buf->list[0].type == MENU_SETTING_DROPDOWN_ITEM_INPUT_PLAYS_AS_ADD
         && string_is_equal(buf->list[0].path, "Pad C (Player 2)")
         && buf->list[0].entry_idx == 2,
         "Add Controller on Port 1 does not list Pad C alone");

   press_ok(0);
   CHECK(input_config_plays_as_player(2) == 1,
         "picking Pad C did not set it to Player 1");
   CHECK(row_of_type(pad_b) == add,
         "fixture: B is not on Add Controller's old row (%d)",
         row_of_type(pad_b));
   CHECK(cursor_type() == pad_c && menu_st->selection_ptr == 1
         && row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR) == 3,
         "back on Port 1 Controls the rows are not A, C, B with the "
         "cursor on C (cursor %u)", (unsigned)menu_st->selection_ptr);

   depth = stack_depth();
   press_ok(row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR));
   buf = selection_buf();
   CHECK(stack_depth() == depth + 1 && buf && buf->size == 1
         && buf->list[0].type == FILE_TYPE_NONE
         && string_is_equal(buf->list[0].label,
               MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY_STR),
         "with every controller Player 1's, Add Controller does not show "
         "the empty-list entry");
   press(MENU_ACTION_CANCEL);
   t_forget_pads();
}

/* Add Controller names where each controller plays now: twins on
 * other players differ, a sharer shows the player it joins, and one
 * with no port is left out. The one picked leaves the page it was on. */
static void lane_port_controls_add_items(void)
{
   file_list_t *buf;
   settings_t *settings = config_get_ptr();
   unsigned pad_b       = MENU_SETTINGS_INPUT_PLAYS_AS_BEGIN + 1;
   unsigned port5       = settings->uints.input_joypad_index[4];

   /* and D, C's twin, Auto on port 3; E, with every port taken */
   t_three_pads();
   input_config_set_device_name(3, "Pad C");
   input_autoconfigure_reallocate_ports(3);
   input_config_set_device_name(4, "Pad E");
   input_autoconfigure_reallocate_ports(4);
   CHECK(input_config_slot_user(3) == 2
         && input_config_slot_user(4) == MAX_USERS,
         "fixture: D is not on port 3, or E has a port");

   open_port_controls(1);
   press_ok(row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR));
   buf = selection_buf();
   CHECK(buf && buf->size == 2
         && string_is_equal(buf->list[0].path, "Pad C (Player 2)")
         && string_is_equal(buf->list[1].path, "Pad C (Player 3)"),
         "Port 1's Add Controller does not tell C and D apart by player "
         "alone (%u items)", buf ? (unsigned)buf->size : 0);
   press(MENU_ACTION_CANCEL);

   open_port_controls(2);
   press_ok(row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR));
   buf = selection_buf();
   CHECK(buf && buf->size == 3
         && string_is_equal(buf->list[0].path, "Pad A (Player 1)")
         && string_is_equal(buf->list[1].path, "Pad C (Player 3)")
         && string_is_equal(buf->list[2].path, "Pad B (Player 1)")
         && buf->list[2].entry_idx == 1,
         "Port 2's Add Controller does not list A, D and the sharer B "
         "by player alone (%u items)", buf ? (unsigned)buf->size : 0);
   press_ok(2);
   CHECK(cursor_type() == pad_b,
         "back on Port 2 Controls the cursor is not on B");
   open_port_controls(1);
   CHECK(row_of_type(pad_b) < 0,
         "B, moved to Player 2, is still on Port 1 Controls");

   input_config_clear_device_name(4);
   input_config_plays_as_release(4);
   settings->uints.input_joypad_index[4] = port5;
   t_forget_pads();
}

/* A pick needing a seventeenth entry is refused with the notice, back
 * on Add Controller with nothing changed. */
static void lane_port_controls_add_full(void)
{
   int add;
   unsigned e;
   char name[32];
   const char *msg;
   struct menu_state *menu_st  = menu_state_get_ptr();
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   /* A's and B's entries, then fourteen of controllers that are gone */
   t_three_pads();
   for (e = 2; e < MAX_USERS; e++)
   {
      snprintf(name, sizeof(name), "Gone %u", e);
      input_config_set_device_name(4, name);
      input_config_plays_as_set(4, 1);
      input_config_clear_device_name(4);
      input_config_plays_as_release(4);
   }
   CHECK(!input_config_plays_as_set(2, 1),
         "fixture: C took a seventeenth entry");

   open_port_controls(1);
   add = row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR);
   press_ok(add);
   msg_queue_clear(&runloop_st->msg_queue);
   press_ok(0);
   msg = msg_queue_pull(&runloop_st->msg_queue);
   CHECK(input_config_plays_as_player(2) == 0
         && msg && string_is_equal(msg,
               msg_hash_to_str(MSG_INPUT_PLAYS_AS_FULL)),
         "picking C with every entry taken gave no notice (\"%s\")",
         msg ? msg : "");
   CHECK(add >= 0 && (int)menu_st->selection_ptr == add
         && row(MENU_ENUM_LABEL_INPUT_PLAYS_AS_ADD_STR) == add,
         "a refused pick left the cursor on row %u",
         (unsigned)menu_st->selection_ptr);

   for (e = 0; e < MAX_USERS; e++)
      if (     input_config_plays_as_entry_name(e)
            && !input_config_plays_as_entry_taken(e))
         input_config_plays_as_set_entry(e, 0);
   t_forget_pads();
}

/* Ends the bind in progress as its timeout would. */
static void end_bind(void)
{
   menu_state_get_ptr()->input_binds.timer_timeout.timeout_end = 0;
   run_frame();
}

/* A hotkey's bind prompt says whose controllers it hears; a RetroPad
 * bind's does not. */
static void lane_bind_prompt(void)
{
   static const char line[]   = "Listening to Player 1's controllers";
   struct menu_state *menu_st = menu_state_get_ptr();
   rarch_setting_t *hotkey    = menu_setting_find_enum(
         (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN
            + RARCH_ENABLE_HOTKEY));
   rarch_setting_t *pad       = menu_setting_find("p1_b");

   CHECK(hotkey && pad && menu_st->driver_data,
         "fixture: no Hotkey Enable or Port 1 B bind");
   if (!hotkey || !pad || !menu_st->driver_data)
      return;
   menu_st->driver_data->menu_state_msg[0] = '\0';
   hotkey->actions->ok(hotkey, 0, false);
   run_frame();
   CHECK(strstr(menu_st->driver_data->menu_state_msg, line) != NULL,
         "Hotkey Enable's prompt does not say \"%s\": \"%s\"", line,
         menu_st->driver_data->menu_state_msg);
   end_bind();

   menu_st->driver_data->menu_state_msg[0] = '\0';
   pad->actions->ok(pad, 0, false);
   run_frame();
   CHECK(strstr(menu_st->driver_data->menu_state_msg, "Press")
         && !strstr(menu_st->driver_data->menu_state_msg, "Listening to"),
         "Port 1 B's prompt is \"%s\"", menu_st->driver_data->menu_state_msg);
   end_bind();
}
#endif

int main(int argc, char *argv[])
{
#ifdef HAVE_OPENXR
   char fixture_dir[512];
   char cmd[700];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc  = 0;
   const char *tmp = getenv("TMPDIR");

   (void)argc;
   (void)argv;

   if (!tmp || !*tmp)
      tmp = "/tmp";
   snprintf(fixture_dir, sizeof(fixture_dir),
         "%s/stereo_headset_%ld", tmp, (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
      return 1;

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   /* Same prelude as rarch_main(); see playlist_nav. */
   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   runloop_msg_queue_init();
   {
      FILE *cfg;
      snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg",
            fixture_dir);
      if ((cfg = fopen(cfg_path, "wb")))
      {
         fprintf(cfg, "video_driver = \"null\"\n");
         fprintf(cfg, "audio_driver = \"null\"\n");
         fprintf(cfg, "input_driver = \"null\"\n");
         fprintf(cfg, "input_joypad_driver = \"null\"\n");
         fprintf(cfg, "menu_driver = \"rgui\"\n");
         fprintf(cfg, "video_threaded = \"false\"\n");
         fprintf(cfg, "video_openxr_enable = \"false\"\n");
         fclose(cfg);
      }
   }

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   t_drop_host_pads();
   retroarch_menu_running();

   if (!selection_buf())
   {
      fprintf(stderr, "FAIL: the menu came up without an entry list\n");
      return 1;
   }

   {
      unsigned d;
      for (d = 0; headset_drivers[d]; d++)
      {
         lane_driver = headset_drivers[d];
         lane_off();
         lane_on();
         lane_toggle();
      }
      lane_driver = HEADSET_DRIVER;
   }
   lane_on_other_driver();
   lane_headset_rows();
   lane_placement();
   lane_refresh_rate();
   lane_controller_rows();
   lane_device_index_used_by();
   lane_port_controls_rows();
   lane_port_controls_device_index();
   lane_port_controls_links();
   lane_remap_port_links();
   lane_port_links_by_port();
   lane_port_controls_add();
   lane_port_controls_add_items();
   lane_port_controls_add_full();
   lane_bind_prompt();

   set_driver("null");
   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }

   if (failures)
   {
      fprintf(stderr, "stereo_headset_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("stereo_headset_test: Stereo Mode follows Headset Output; "
         "Headset Refresh Rate lists its rates\n");
#else
   (void)argc;
   (void)argv;
   printf("stereo_headset_test: skipped (no HAVE_OPENXR)\n");
#endif
   return 0;
}
