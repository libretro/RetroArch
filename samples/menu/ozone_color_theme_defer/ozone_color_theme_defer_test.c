/* Ozone deferred color-theme write.
 *
 * This links the shipping RetroArch objects - only main() is
 * replaced, nothing is stubbed - and drives the real seam between
 * ozone's frame path and its main-thread render:
 *
 *   frame (menu_driver_frame -> ozone_frame): with "use preferred
 *   system color theme" on and the configured theme differing from
 *   the system's, the frame path re-themes and hands the value off
 *   for persistence rather than writing settings itself;
 *
 *   render (the driver's render vtable slot, main thread): performs
 *   the configuration_set_string and clears the hand-off.
 *
 * The regression this pins: an earlier mechanical snapshot
 * conversion aimed the configuration_set_uint macro's write target
 * at the per-frame snapshot copy, so the value was discarded with
 * the frame - the setting never changed, and because the next
 * frame's snapshot still carried the stale theme, the "change theme
 * on the fly" block re-fired every single frame. On that code lane 2
 * fails (the setting never becomes the system theme) and lane 3
 * fails (the block keeps re-marking the configuration as modified).
 *
 * Requires a completed non-Qt build, same as playlist_nav:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/ozone_color_theme_defer/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <boolean.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../../config.def.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../menu/menu_driver.h"
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

/* A valid theme identifier that is not the system theme
 * (ozone_get_system_theme() returns DEFAULT_OZONE_COLOR_THEME off
 * Switch, and the harness only runs off Switch). */
#define WRONG_THEME (string_is_equal(DEFAULT_OZONE_COLOR_THEME, "basic_black") \
      ? "basic_white" : "basic_black")

static void one_frame_and_render(void)
{
   video_frame_info_t vinfo;
   struct menu_state *menu_st = menu_state_get_ptr();

   /* The real assembler, so the frame sees exactly the snapshot the
    * shipping frame path would - including the theme field the
    * detection block compares. */
   video_driver_build_info(&vinfo);

   menu_driver_frame(true, &vinfo);

   /* The render vtable slot is what the main thread runs every
    * iteration while the menu is alive; it hosts the deferred
    * write. */
   if (menu_st->driver_ctx && menu_st->driver_ctx->render)
      menu_st->driver_ctx->render(menu_st->userdata, vinfo.dims, false);
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

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/ozone_theme_%ld", (long)getpid());
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
         fprintf(cfg, "menu_driver = \"ozone\"\n");
         fprintf(cfg, "video_threaded = \"false\"\n");
         /* Preference off at init: the on-the-fly block reacts to
          * CHANGES, so the harness flips the preference after init
          * and lets the frame path discover it. */
         fprintf(cfg, "menu_use_preferred_system_color_theme = \"false\"\n");
         fprintf(cfg, "ozone_menu_color_theme = \"%s\"\n",
               WRONG_THEME);
         fclose(cfg);
      }
   }

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }

   settings = config_get_ptr();

   /* --menu loads the dummy core but the menu is raised by the first
    * runloop iteration, not by init; the harness raises it the same
    * way the frontend does. */
   retroarch_menu_running();

   {
      struct menu_state *menu_st = menu_state_get_ptr();
      CHECK(   menu_st->driver_ctx && menu_st->driver_ctx->ident
            && string_is_equal(menu_st->driver_ctx->ident, "ozone"),
            "fixture: menu driver is %s, want ozone",
            (menu_st->driver_ctx && menu_st->driver_ctx->ident)
               ? menu_st->driver_ctx->ident : "(none)");
      CHECK(menu_st->userdata != NULL, "fixture: no menu userdata");
      CHECK((menu_st->flags & MENU_ST_FLAG_ALIVE) != 0,
            "fixture: menu not ALIVE, the snapshot assembler would skip"
            " the menu fields");
      {
         video_frame_info_t probe;
         video_driver_build_info(&probe);
         CHECK(string_is_equal(probe.menu.ozone_color_theme, WRONG_THEME),
               "fixture: snapshot theme %s, want %s",
               probe.menu.ozone_color_theme, WRONG_THEME);
      }
   }

   /* Lane 1: the fixture holds - configured theme in place, and one
    * settled frame under the preference-off baseline. If this fires,
    * the fixture is wrong, not the code. */
   CHECK(string_is_equal(settings->arrays.menu_ozone_color_theme, WRONG_THEME),
         "fixture: expected configured theme %s before any frame, got %s",
         WRONG_THEME, settings->arrays.menu_ozone_color_theme);
   one_frame_and_render();
   CHECK(string_is_equal(settings->arrays.menu_ozone_color_theme, WRONG_THEME),
         "fixture: theme moved with the preference still off");

   /* The change the block exists to catch: the person turns the
    * preference on. The harness is the main thread, where settings
    * writes belong. */
   configuration_set_bool(settings,
         settings->bools.menu_use_preferred_system_color_theme, true);

   /* Lane 2: one frame detects the flip and hands the system theme
    * off; the render that follows persists it. */
   settings->flags &= ~SETTINGS_FLG_MODIFIED;
   one_frame_and_render();
   CHECK(string_is_equal(settings->arrays.menu_ozone_color_theme,
            DEFAULT_OZONE_COLOR_THEME),
         "system theme not persisted: setting is %s, want %s",
         settings->arrays.menu_ozone_color_theme,
         DEFAULT_OZONE_COLOR_THEME);
   CHECK((settings->flags & SETTINGS_FLG_MODIFIED) != 0,
         "persisting the theme must mark the configuration modified");

   /* Lane 3: with the setting now agreeing with the system, the
    * block must quiesce - no re-fire, no fresh modified mark. On the
    * broken code the stale snapshot keeps the block firing every
    * frame and this stays set. */
   settings->flags &= ~SETTINGS_FLG_MODIFIED;
   one_frame_and_render();
   one_frame_and_render();
   CHECK((settings->flags & SETTINGS_FLG_MODIFIED) == 0,
         "theme block re-fired after the setting caught up");
   CHECK(string_is_equal(settings->arrays.menu_ozone_color_theme,
            DEFAULT_OZONE_COLOR_THEME),
         "setting drifted after quiescing");

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("ozone_color_theme_defer: all lanes passed\n");
   return 0;
}
