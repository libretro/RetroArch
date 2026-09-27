/* Menu rows that own one half of a packed settings_t word.
 *
 * The custom viewport keeps its origin and its size in one word each,
 * and the window size pair shares one the same way; the menu still
 * shows one row per axis. Links the shipping objects with only main()
 * replaced, boots the menu headless and drives those rows through
 * the paths a person takes:
 *
 *   - the value a row displays is its own axis, not the whole word;
 *   - a right press steps that axis by one and leaves its partner as
 *     it stands;
 *   - the dropdown the menu opens for a row marks the current value,
 *     and choosing an entry sets that axis alone.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/settings/packed_rows/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <lists/file_list.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <features/features_cpu.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../setting_list.h"
#include "../../../gfx/video_defines.h"
#include "../../../menu/menu_setting.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../msg_hash.h"
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

static long shown(rarch_setting_t *s)
{
   char buf[128];
   buf[0] = '\0';
   if (s && s->actions && s->actions->repr)
      s->actions->repr(s, buf, sizeof(buf));
   return strtol(buf, NULL, 10);
}

static void press_right(rarch_setting_t *s)
{
   if (s->actions && s->actions->right)
      s->actions->right(s, 0, false);
}

static file_list_t *selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

/* Index of the dropdown entry whose path is exactly @path. */
static int find_entry(const char *path)
{
   file_list_t *buf = selection_buf();
   size_t i;
   if (!buf)
      return -1;
   for (i = 0; i < buf->size; i++)
      if (buf->list[i].path && !strcmp(buf->list[i].path, path))
         return (int)i;
   return -1;
}

/* One menu frame; a pushed dropdown is built on the next one. */
static void run_frame(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), MENU_ACTION_NOOP,
         cpu_features_get_time_usec());
}

static void press_ok_at(size_t i)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_entry_t entry;
   menu_st->selection_ptr = i;
   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                | MENU_ENTRY_FLAG_LABEL_ENABLED
                | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                | MENU_ENTRY_FLAG_VALUE_ENABLED
                | MENU_ENTRY_FLAG_SUBLABEL_ENABLED;
   menu_entry_get(&entry, 0, i, NULL, true);
   menu_entry_action(&entry, i, MENU_ACTION_OK);
}

int main(int argc, char *argv[])
{
   char fixture_dir[512];
   char cmd[700];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc = 0;
   rarch_setting_t *w, *h, *x, *y, *ww;
   settings_t *settings;
   int idx;

   (void)argc;
   (void)argv;

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/packed_rows_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
      return 1;

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

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
         fprintf(cfg, "custom_viewport_width = \"960\"\n");
         fprintf(cfg, "custom_viewport_height = \"720\"\n");
         fprintf(cfg, "custom_viewport_x = \"-12\"\n");
         fprintf(cfg, "custom_viewport_y = \"34\"\n");
         fprintf(cfg, "video_windowed_position_width = \"1280\"\n");
         fprintf(cfg, "video_windowed_position_height = \"720\"\n");
         fclose(cfg);
      }
   }

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   settings = config_get_ptr();
   retroarch_menu_running();

   w  = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH);
   h  = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT);
   x  = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X);
   y  = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y);
   ww = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH);
   CHECK(w && h && x && y && ww, "fixture: a packed row was not found");
   if (!(w && h && x && y && ww))
      goto done;

   CHECK(VIDEO_SCALE_W(settings->video_vp_custom.dims) == 960
         && VIDEO_SCALE_H(settings->video_vp_custom.dims) == 720,
         "fixture: custom viewport size did not load as 960x720");

   /* Each row shows its own axis. */
   CHECK(shown(w) == 960, "width row shows %ld, not 960", shown(w));
   CHECK(shown(h) == 720, "height row shows %ld, not 720", shown(h));
   CHECK(shown(x) == -12, "x row shows %ld, not -12", shown(x));
   CHECK(shown(y) == 34,  "y row shows %ld, not 34", shown(y));
   CHECK(shown(ww) == 1280, "window width row shows %ld, not 1280",
         shown(ww));

   /* A right press moves one axis by one step. */
   press_right(w);
   CHECK(shown(w) == 961, "width after right shows %ld, not 961",
         shown(w));
   CHECK(VIDEO_SCALE_H(settings->video_vp_custom.dims) == 720,
         "width right press changed height to %u",
         VIDEO_SCALE_H(settings->video_vp_custom.dims));
   press_right(x);
   CHECK(shown(x) == -11, "x after right shows %ld, not -11", shown(x));
   CHECK(VIDEO_POS_Y(settings->video_vp_custom.pos) == 34,
         "x right press changed y to %d",
         VIDEO_POS_Y(settings->video_vp_custom.pos));

   /* The dropdown for the window width row: it marks the current
    * value, building it leaves the word as it was, and choosing an
    * entry sets the width alone. */
   CHECK(ww->actions && ww->actions->ok,
         "fixture: window width row has no ok action");
   if (ww->actions && ww->actions->ok)
   {
      struct menu_state *menu_st = menu_state_get_ptr();
      unsigned before            = settings->uints.window_position_dims;

      ww->actions->ok(ww, 0, false);
      run_frame();
      CHECK(settings->uints.window_position_dims == before,
            "building the dropdown changed the window size word");
      idx = find_entry("1280");
      CHECK(idx >= 0, "dropdown has no 1280 entry");
      CHECK(idx >= 0 && menu_st->selection_ptr == (size_t)idx,
            "dropdown marks entry %u, not the current 1280 at %d",
            (unsigned)menu_st->selection_ptr, idx);
      idx = find_entry("1288");
      CHECK(idx >= 0, "dropdown has no 1288 entry");
      if (idx >= 0)
      {
         press_ok_at((size_t)idx);
         run_frame();
         CHECK(VIDEO_SCALE_W(settings->uints.window_position_dims) == 1288,
               "choosing 1288 set width %u",
               VIDEO_SCALE_W(settings->uints.window_position_dims));
         CHECK(VIDEO_SCALE_H(settings->uints.window_position_dims) == 720,
               "choosing a width changed height to %u",
               VIDEO_SCALE_H(settings->uints.window_position_dims));
      }
   }

done:
   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }

   if (failures)
   {
      fprintf(stderr, "packed_rows_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("packed_rows_test: all packed rows edit their own half\n");
   return 0;
}
