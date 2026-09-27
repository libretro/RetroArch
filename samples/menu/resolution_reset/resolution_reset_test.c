/* Start on the Resolution entry.
 *
 * Links the shipping RetroArch objects with only main() replaced (the
 * playlist_nav pattern). The null video driver's poke table is
 * swapped for one that reports a 1920x1080 output and records what
 * set_video_mode is handed, then the callback bound to
 * MENU_ENUM_LABEL_SCREEN_RESOLUTION's Start runs as the menu would
 * run it.
 *
 * The claim: resetting the resolution keeps the window state the
 * frontend is in - windowed stays windowed, and full screen (from the
 * setting or from the forced-fullscreen flag) stays full screen - and
 * the mode carries the output size.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/resolution_reset/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <streams/file_stream.h>

#include "../../../config.def.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_cbs.h"
#include "../../../gfx/video_driver.h"
#include "../../../frontend/frontend_driver.h"

#define OUT_W 1920
#define OUT_H 1080

static unsigned failures = 0;
static unsigned mode_calls;
static unsigned mode_dims;
static bool     mode_fullscreen;

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

static void harness_set_video_mode(void *data, unsigned dims,
      bool fullscreen)
{
   mode_calls++;
   mode_dims       = dims;
   mode_fullscreen = fullscreen;
}

static void harness_get_video_output_size(void *data,
      unsigned *dims, char *s, size_t len)
{
   *dims   = VIDEO_SCALE_PACK(OUT_W, OUT_H);
   if (len)
      *s   = '\0';
}

static video_poke_interface_t harness_poke;

static void harness_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &harness_poke;
}

/* Presses Start on the Resolution entry; returns the fullscreen flag
 * the mode was set with. */
static bool press_start(const char *lane)
{
   menu_file_list_cbs_t cbs;

   memset(&cbs, 0, sizeof(cbs));
   cbs.enum_idx = MENU_ENUM_LABEL_SCREEN_RESOLUTION;
   menu_cbs_init_bind_start(&cbs, "", "", 0, 0);

   mode_calls = 0;
   mode_dims  = 0;
   CHECK(cbs.action_start != NULL, "%s: no Start callback bound", lane);
   if (cbs.action_start)
      cbs.action_start("", "", 0, 0, 0);

   CHECK(mode_calls == 1, "%s: set_video_mode called %u times, want 1",
         lane, mode_calls);
   CHECK(mode_dims == VIDEO_SCALE_PACK(OUT_W, OUT_H),
         "%s: mode size %ux%u, want %ux%u", lane,
         VIDEO_SCALE_W(mode_dims), VIDEO_SCALE_H(mode_dims), OUT_W, OUT_H);
   return mode_fullscreen;
}

int main(int argc, char *argv[])
{
   char fixture_dir[512];
   char cmd[700];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc = 0;
   settings_t *settings = NULL;

   (void)argc;
   (void)argv;

   harness_poke.set_video_mode        = harness_set_video_mode;
   harness_poke.get_video_output_size = harness_get_video_output_size;
   /* Every init, the reinit Start performs included, takes this. */
   video_null.poke_interface          = harness_get_poke_interface;

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/resolution_reset_%ld", (long)getpid());
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
         fprintf(cfg, "video_threaded = \"false\"\n");
         fprintf(cfg, "video_fullscreen = \"false\"\n");
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

   /* Lane 1: windowed stays windowed. */
   CHECK(!press_start("windowed"),
         "windowed: Start on Resolution took the window full screen");

   /* Lane 2: the fullscreen setting stays full screen. */
   configuration_set_bool(settings, settings->bools.video_fullscreen, true);
   CHECK(press_start("fullscreen setting"),
         "fullscreen setting: Start on Resolution left full screen");

   /* Lane 3: forced full screen (e.g. from the command line) with the
    * setting off stays full screen. */
   configuration_set_bool(settings, settings->bools.video_fullscreen, false);
   video_driver_modify_disp_flags(VIDEO_FLAG_FORCE_FULLSCREEN, 0);
   CHECK(press_start("forced fullscreen"),
         "forced fullscreen: Start on Resolution left full screen");
   video_driver_modify_disp_flags(0, VIDEO_FLAG_FORCE_FULLSCREEN);

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("resolution_reset: all lanes passed\n");
   return 0;
}
