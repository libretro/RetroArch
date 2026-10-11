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

#include "../../../msg_hash_lbl_str.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../menu/menu_cbs.h"
#include "../../../menu/menu_setting.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
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
   set_driver("vulkan");
}

static bool cursor_on_headset_output(void)
{
   return (int)menu_state_get_ptr()->selection_ptr
      == row(MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR);
}

static void lane_off(void)
{
   set_headset(false);
   set_driver("vulkan");
   open_stereo_list();

   CHECK(row(MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR) >= 0,
         "fixture: Headset Output is not listed under vulkan");
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
   set_driver("vulkan");
   open_stereo_list();

   CHECK(row(MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR) >= 0,
         "fixture: Headset Output is not listed under vulkan");
   CHECK(row(MENU_ENUM_LABEL_VIDEO_STEREO_MODE_STR) < 0,
         "Stereo Mode is listed with Headset Output on");
   CHECK(row(MENU_ENUM_LABEL_VIDEO_STEREO_SWAP_EYES_STR) >= 0,
         "Swap Eyes is not listed with Headset Output on");
   CHECK(row(MENU_ENUM_LABEL_VIDEO_SCREEN_LAYOUT_STR) >= 0,
         "Screen Layout is not listed with Headset Output on");
}

/* Headset output is the Vulkan driver's: under another driver the
 * setting has no row to turn it off from, and nothing to hide. */
static void lane_on_other_driver(void)
{
   set_headset(true);
   set_driver("glcore");
   open_stereo_list();

   CHECK(row(MENU_ENUM_LABEL_VIDEO_OPENXR_ENABLE_STR) < 0,
         "fixture: Headset Output is listed under glcore");
   CHECK(row(MENU_ENUM_LABEL_VIDEO_STEREO_MODE_STR) >= 0,
         "Stereo Mode is not listed under glcore with the setting on");
}

static void lane_toggle(void)
{
   set_headset(false);
   set_driver("vulkan");
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
   set_driver("vulkan");
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
   retroarch_menu_running();

   if (!selection_buf())
   {
      fprintf(stderr, "FAIL: the menu came up without an entry list\n");
      return 1;
   }

   lane_off();
   lane_on();
   lane_on_other_driver();
   lane_toggle();
   lane_refresh_rate();

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
