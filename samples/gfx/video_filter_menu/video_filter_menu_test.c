/* The menu's video filter rows set the video driver up for what it
 * will be given.
 *
 * Links the shipping RetroArch objects with only main() replaced and
 * boots the frontend with the null drivers on a 240x160 RGB565 core
 * (built next to this binary by build.sh, with the Normal4x and
 * ntsc_crt filter plugins). The rows are driven through the menu's own
 * entry points. A copy of the live video driver's table is put in
 * place before each step: a driver set up again finds the real table,
 * so the copy being gone says the driver was rebuilt.
 *
 * Lanes:
 *  - choosing a filter (Normal4x, 4x the core's frame) sets the driver
 *    up again for the filter's scale;
 *  - turning Video Filter Enable off and on with that filter is
 *    instant: its output keeps the core's pixel format;
 *  - choosing ntsc_crt, which outputs XRGB8888 for an RGB565 core,
 *    sets the driver up again for 32-bit frames;
 *  - turning Video Filter Enable off and on with ntsc_crt sets the
 *    driver up again each time, as the toggle hotkey does.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/gfx/video_filter_menu/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <time/rtime.h>
#include <queues/task_queue.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#include <retro_atomic.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../command.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../verbosity.h"
#include "../../../gfx/video_driver.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_setting.h"
#include "../../../msg_hash_lbl_str.h"

static unsigned failures = 0;

#define CHECK(cond, what) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, what); \
         failures++; \
      } \
   } while (0)

static char filter_dir[512];

static video_driver_t hooked_video;

static void hook_install(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   hooked_video            = *video_st->current_video;
   video_st->current_video = &hooked_video;
}

/* The driver was set up again since hook_install() */
static bool driver_rebuilt(void)
{
   return video_state_get_ptr()->current_video != &hooked_video;
}

static bool menu_is_up(void)
{
   return (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) != 0;
}

static void pump(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      runloop_iterate();
      task_queue_check();
   }
}

/* The core runs a few frames through whatever is set up, then the
 * menu is back for the next step. */
static void run_core(void)
{
   if (menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   pump(4);
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   pump(1);
}

static bool filter_out_rgb32(void)
{
   return ((uint32_t)retro_atomic_load_relaxed_int(
            &video_state_get_ptr()->flags) & VIDEO_FLAG_STATE_OUT_RGB32)
      ? true : false;
}

/* A filter picked in the file browser, as the browser's OK sets it */
static void choose_filter(const char *name)
{
   char path[600];
   rarch_setting_t *setting = menu_setting_find(MENU_ENUM_LABEL_VIDEO_FILTER_STR);
   if (!setting)
   {
      CHECK(0, "no Video Filter row");
      return;
   }
   snprintf(path, sizeof(path), "%s/%s", filter_dir, name);
   strlcpy(setting->value.target.string, path, setting->size);
   menu_setting_generic(setting, 0, false);
}

/* Video Filter Enable flipped with the right button on its row */
static void toggle_enable(void)
{
   rarch_setting_t *setting = menu_setting_find(
         MENU_ENUM_LABEL_VIDEO_FILTER_ENABLE_STR);
   if (!setting)
   {
      CHECK(0, "no Video Filter Enable row");
      return;
   }
   menu_action_handle_setting(setting, setting->type, MENU_ACTION_RIGHT, false);
}

static void lane_same_format(void)
{
   settings_t *settings = config_get_ptr();
   unsigned had         = failures;

   hook_install();
   choose_filter("Normal4x.filt");
   CHECK(video_state_get_ptr()->state_filter,
         "Normal4x: the filter is not loaded");
   CHECK(video_state_get_ptr()->state_scale >= 4,
         "Normal4x: the filter's scale was not taken");
   CHECK(driver_rebuilt(),
         "Normal4x chosen: the driver was not set up again for the "
         "filter's scale");
   run_core();

   hook_install();
   toggle_enable();
   CHECK(!settings->bools.video_filter_enable, "Normal4x: enable did not turn off");
   CHECK(!driver_rebuilt(),
         "Normal4x, enable off: the driver was set up again for a filter "
         "that keeps the core's format");
   run_core();
   hook_install();
   toggle_enable();
   CHECK(settings->bools.video_filter_enable, "Normal4x: enable did not turn on");
   CHECK(!driver_rebuilt(),
         "Normal4x, enable on: the driver was set up again for a filter "
         "that keeps the core's format");
   run_core();

   if (failures == had)
      fprintf(stderr, "[pass] choosing Normal4x sets the driver up again; "
            "turning it off and on is instant\n");
}

static void lane_format_change(void)
{
   settings_t *settings = config_get_ptr();
   unsigned had         = failures;

   hook_install();
   choose_filter("ntsc_crt.filt");
   CHECK(video_state_get_ptr()->state_filter && filter_out_rgb32(),
         "ntsc_crt: the filter is not loaded with 32-bit output");
   CHECK(driver_rebuilt(),
         "ntsc_crt chosen: the driver was not set up again for 32-bit frames");
   run_core();

   hook_install();
   toggle_enable();
   CHECK(!settings->bools.video_filter_enable, "ntsc_crt: enable did not turn off");
   CHECK(driver_rebuilt(),
         "ntsc_crt, enable off: the driver was not set up again for the "
         "core's RGB565 frames");
   run_core();

   hook_install();
   toggle_enable();
   CHECK(settings->bools.video_filter_enable, "ntsc_crt: enable did not turn on");
   CHECK(driver_rebuilt(),
         "ntsc_crt, enable on: the driver was not set up again for the "
         "filter's 32-bit frames");
   run_core();

   if (failures == had)
      fprintf(stderr, "[pass] choosing ntsc_crt, and turning it off and "
            "on, each set the driver up again\n");
}

int main(int argc, char *argv[])
{
   char cfg_path[512];
   char dir[400];
   char core_path[512];
   char *rarch_argv[8] = {0};
   int rarch_argc      = 0;
   int dirlen;
   const char *base;
   const char *slash;
   FILE *cfg;
   (void)argc;

   {
      const char *tmp = getenv("TMPDIR");
      if (!tmp || !*tmp)
         tmp = getenv("TEMP");
      if (!tmp || !*tmp)
         tmp = "/tmp";
      snprintf(dir, sizeof(dir), "%s/video_filter_menu_%ld",
            tmp, (long)getpid());
   }
   if (!path_mkdir(dir))
      return 1;

   /* The core and the filters, built next to this binary */
   slash = strrchr(argv[0], '/');
#ifdef _WIN32
   {
      const char *bslash = strrchr(argv[0], '\\');
      if (bslash && (!slash || bslash > slash))
         slash = bslash;
   }
#endif
   dirlen = slash ? (int)(slash - argv[0]) : 1;
   base   = slash ? argv[0] : ".";
   snprintf(core_path, sizeof(core_path), "%.*s/filter_core.so",
         dirlen, base);
   snprintf(filter_dir, sizeof(filter_dir), "%.*s/filters",
         dirlen, base);

   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", dir);
   if (!(cfg = fopen(cfg_path, "wb")))
      return 1;
   fprintf(cfg, "video_driver = \"null\"\n");
   fprintf(cfg, "audio_driver = \"null\"\n");
   fprintf(cfg, "input_driver = \"null\"\n");
   fprintf(cfg, "input_joypad_driver = \"null\"\n");
   fprintf(cfg, "menu_driver = \"rgui\"\n");
   fprintf(cfg, "video_threaded = \"false\"\n");
   fprintf(cfg, "video_vsync = \"false\"\n");
   fprintf(cfg, "video_filter_enable = \"true\"\n");
   fprintf(cfg, "video_filter_dir = \"%s\"\n", filter_dir);
   fprintf(cfg, "menu_pause_libretro = \"true\"\n");
   fprintf(cfg, "config_save_on_exit = \"false\"\n");
   fprintf(cfg, "savestate_auto_save = \"false\"\n");
   fprintf(cfg, "menu_show_load_content_animation = \"false\"\n");
   fprintf(cfg, "libretro_info_path = \"%s\"\n", dir);
   fclose(cfg);

   if (getenv("HARNESS_VERBOSE"))
      verbosity_enable();

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   runloop_msg_queue_init();

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;
   rarch_argv[rarch_argc++] = (char*)"-L";
   rarch_argv[rarch_argc++] = core_path;
   if (getenv("HARNESS_VERBOSE"))
      rarch_argv[rarch_argc++] = (char*)"-v";

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   pump(5);
   if (runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY)
   {
      fprintf(stderr, "FAIL: the harness core did not start\n");
      return 1;
   }
   if (!menu_is_up())
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   pump(1);
   if (!menu_is_up())
   {
      fprintf(stderr, "FAIL: the menu did not open\n");
      return 1;
   }

   lane_same_format();
   lane_format_change();

   main_exit(NULL);

   remove(cfg_path);
   {
      char leftover[600];
      snprintf(leftover, sizeof(leftover), "%s/core_info.cache", dir);
      remove(leftover);
   }
   path_rmdir(dir);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "[pass] video_filter_menu\n");
   return 0;
}
