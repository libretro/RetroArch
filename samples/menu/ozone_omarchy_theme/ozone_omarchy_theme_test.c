/* Ozone Omarchy theme: colours read from ozone_omarchy.cfg.
 *
 * Links the shipping RetroArch objects with only main() replaced and
 * includes ozone.c itself, so the lanes can read the theme and the
 * handle. Each iteration is one frame, one render and a task pump.
 *
 *   init       the file present at startup colours the first frame,
 *              and the theme looks for no icon set of its own
 *   iterate    a changed file is not read while the menu just runs
 *   reopen     opening the menu reads it again; a key dropped from
 *              the file goes back to its default
 *   reselect   choosing the theme again reads it again
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/ozone_omarchy_theme/build.sh
 */

#include "../../../menu/drivers/ozone.c"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <boolean.h>
#include <retro_timers.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <queues/task_queue.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../menu/menu_driver.h"
#include "../../../gfx/video_driver.h"
#include "../../../frontend/frontend_driver.h"

#ifdef _WIN32
#include <process.h>
#define harness_pid() ((long)_getpid())
#else
#include <unistd.h>
#define harness_pid() ((long)getpid())
#endif

static unsigned failures = 0;
static char palette_path[640];

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* The first colour of a 16-float quad colour is the hex's red */
#define RED_IS(quad, hex) (fabs((double)(quad)[0] - (double)HEX_R(hex)) < 0.002)

static ozone_handle_t *handle(void)
{
   return (ozone_handle_t*)menu_state_get_ptr()->userdata;
}

static void write_palette(const char *text)
{
   filestream_write_file(palette_path, text, (int64_t)strlen(text));
}

static void iterate(void)
{
   unsigned i;
   video_frame_info_t vinfo;
   struct menu_state *menu_st = menu_state_get_ptr();

   video_driver_build_info(&vinfo);
   menu_driver_frame(true, &vinfo);
   menu_st->driver_ctx->render(menu_st->userdata, vinfo.dims, false);

   /* A threaded task queue finishes on its worker */
   for (i = 0; i < 50; i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
}

static void set_theme(const char *ident)
{
   settings_t *settings = config_get_ptr();
   configuration_set_string(settings,
         settings->arrays.menu_ozone_color_theme, ident);
}

int main(int argc, char *argv[])
{
   char fixture_dir[512];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc = 0;
   ozone_handle_t *ozone;
   struct menu_state *menu_st;

   (void)argc;
   (void)argv;

   snprintf(fixture_dir, sizeof(fixture_dir),
         "ozone_omarchy_theme_%ld", harness_pid());
   path_mkdir(fixture_dir);
   fill_pathname_join_special(palette_path, fixture_dir,
         "ozone_omarchy.cfg", sizeof(palette_path));
   write_palette(
         "ozone_background = \"0xEEEEEE\"\n"
         "ozone_text = \"0x111111\"\n"
         "ozone_accent = \"0x0000FF\"\n");

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
      snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", fixture_dir);
      if ((cfg = fopen(cfg_path, "wb")))
      {
         fprintf(cfg, "video_driver = \"null\"\n");
         fprintf(cfg, "audio_driver = \"null\"\n");
         fprintf(cfg, "input_driver = \"null\"\n");
         fprintf(cfg, "input_joypad_driver = \"null\"\n");
         fprintf(cfg, "menu_driver = \"ozone\"\n");
         fprintf(cfg, "video_threaded = \"false\"\n");
         fprintf(cfg, "menu_use_preferred_system_color_theme = \"false\"\n");
         fprintf(cfg, "ozone_menu_color_theme = \"omarchy\"\n");
         fclose(cfg);
      }
   }

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   retroarch_menu_running();

   menu_st = menu_state_get_ptr();
   if (     !menu_st->driver_ctx || !menu_st->driver_ctx->ident
         || !string_is_equal(menu_st->driver_ctx->ident, "ozone")
         || !menu_st->userdata)
   {
      fprintf(stderr, "FAIL: fixture: ozone is not the running menu\n");
      return 1;
   }
   ozone = handle();

   /* init */
   iterate();
   CHECK(ozone->theme == &ozone_theme_omarchy, "init: theme is not omarchy");
   CHECK(!ozone_theme_omarchy.name, "init: theme names an icon set");
   CHECK(RED_IS(ozone_theme_omarchy.background, 0xEEEEEE),
         "init: background not from the file");
   CHECK(RED_IS(ozone->background_running, 0xEEEEEE),
         "init: running background not recoloured");
   CHECK(RED_IS(ozone->theme_dynamic.entries_icon, 0x111111),
         "init: icon colour not from the file");
   CHECK(RED_IS(ozone->theme_dynamic.selection_border, 0x0000FF),
         "init: accent not from the file");

   /* iterate */
   write_palette("ozone_background = \"0x102030\"\n");
   iterate();
   iterate();
   CHECK(RED_IS(ozone_theme_omarchy.background, 0xEEEEEE),
         "iterate: file read while the menu runs");

   /* reopen */
   menu_st->driver_ctx->toggle(menu_st->userdata, true);
   iterate();
   iterate();
   CHECK(RED_IS(ozone_theme_omarchy.background, 0x102030),
         "reopen: file not read again");
   CHECK(RED_IS(ozone->background_running, 0x102030),
         "reopen: running background not recoloured");
   CHECK(ozone_theme_omarchy.text_rgba == 0xFFFFFFFF,
         "reopen: dropped text key kept its old colour");
   CHECK(RED_IS(ozone->theme_dynamic.entries_icon, 0xFFFFFF),
         "reopen: dropped text key kept its old icon colour");
   CHECK(RED_IS(ozone->theme_dynamic.selection_border, 0x2DA3CB),
         "reopen: dropped accent key kept its old colour");

   /* reselect */
   write_palette("ozone_background = \"0x405060\"\n");
   set_theme("basic_black");
   iterate();
   CHECK(ozone->theme != &ozone_theme_omarchy, "reselect: fixture: theme did not change");
   set_theme("omarchy");
   iterate();
   iterate();
   CHECK(ozone->theme == &ozone_theme_omarchy, "reselect: theme is not omarchy");
   CHECK(RED_IS(ozone_theme_omarchy.background, 0x405060),
         "reselect: file not read again");
   CHECK(RED_IS(ozone->background_running, 0x405060),
         "reselect: running background not recoloured");

   {
      static const char *const written[] = {
         "harness.cfg", "ozone_omarchy.cfg",
         "retroarch-keychain.cfg", "retroarch-keychain.key"
      };
      char path[640];
      unsigned i;
      for (i = 0; i < ARRAY_SIZE(written); i++)
      {
         fill_pathname_join_special(path, fixture_dir, written[i],
               sizeof(path));
         filestream_delete(path);
      }
      path_rmdir(fixture_dir);
   }

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("ozone_omarchy_theme: all lanes passed\n");
   return 0;
}
