/* Rgui deferred aspect-config write.
 *
 * Links the shipping RetroArch objects with only main() replaced
 * (the playlist_nav pattern) and drives the seam between rgui's
 * frame path and its main-thread render:
 *
 *   frame (menu_driver_frame -> rgui_frame): under threaded video
 *   this is the video thread. When the aspect-ratio lock setting
 *   changes, the frame path re-lays the menu and STAGES the video
 *   configuration on the rgui handle - it must not write settings
 *   (five fields plus the custom-aspect LUT entry) or fire
 *   CMD_EVENT_VIDEO_SET_ASPECT_RATIO from there;
 *
 *   render (main thread): applies the staged configuration into the
 *   settings and fires the command, under
 *   RGUI_FLAG_ASPECT_UPDATE_PENDING.
 *
 * The claims: after a frame that detects the change, the settings
 * are still untouched (the frame staged, nothing more); after the
 * render that follows, video_aspect_ratio_idx and the custom
 * viewport carry the menu configuration; and once the pipeline
 * settles, further frame+render rounds leave the settings alone.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/rgui_aspect_defer/build.sh
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

static void one_frame(void)
{
   video_frame_info_t vinfo;
   video_driver_build_info(&vinfo);
   menu_driver_frame(true, &vinfo);
}

static void one_render(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   if (menu_st->driver_ctx && menu_st->driver_ctx->render)
      menu_st->driver_ctx->render(menu_st->userdata,
            VIDEO_SCALE_PACK(320, 240), false);
}

int main(int argc, char *argv[])
{
   char fixture_dir[512];
   char cmd[700];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc = 0;
   settings_t *settings = NULL;
   unsigned idx_before;

   (void)argc;
   (void)argv;

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/rgui_aspect_%ld", (long)getpid());
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
         /* Lock off at init; the harness flips it and lets the
          * frame path discover the change. */
         fprintf(cfg, "rgui_aspect_ratio_lock = \"0\"\n");
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

   {
      struct menu_state *menu_st = menu_state_get_ptr();
      CHECK(   menu_st->driver_ctx && menu_st->driver_ctx->ident
            && strcmp(menu_st->driver_ctx->ident, "rgui") == 0,
            "fixture: menu driver is %s, want rgui",
            (menu_st->driver_ctx && menu_st->driver_ctx->ident)
               ? menu_st->driver_ctx->ident : "(none)");
      CHECK((menu_st->flags & MENU_ST_FLAG_ALIVE) != 0,
            "fixture: menu not ALIVE");
   }

   /* Settle: one frame + render under the baseline. */
   one_frame();
   one_render();

   idx_before = settings->uints.video_aspect_ratio_idx;

   /* The change the frame path detects: the person turns the aspect
    * lock on. The harness is the main thread, where settings writes
    * belong. */
   configuration_set_uint(settings,
         settings->uints.menu_rgui_aspect_ratio_lock, 1);

   /* Lane 1: the detecting frame stages but must not write - the
    * settings are untouched until the render applies. */
   one_frame();
   CHECK(settings->uints.video_aspect_ratio_idx == idx_before,
         "the frame path wrote video_aspect_ratio_idx itself"
         " (%u -> %u)",
         idx_before, settings->uints.video_aspect_ratio_idx);

   /* Lane 2: the render applies the staged configuration - the
    * aspect index now carries the menu configuration (custom, since
    * the locked menu viewport is a custom viewport). */
   one_render();
   CHECK(settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM,
         "staged aspect config not applied by render: idx %u, want"
         " %u (custom)",
         settings->uints.video_aspect_ratio_idx,
         (unsigned)ASPECT_RATIO_CUSTOM);
   CHECK(VIDEO_SCALE_W(settings->video_vp_custom.dims)  > 0
      && VIDEO_SCALE_H(settings->video_vp_custom.dims) > 0,
         "custom viewport not populated (%ux%u)",
         VIDEO_SCALE_W(settings->video_vp_custom.dims),
         VIDEO_SCALE_H(settings->video_vp_custom.dims));

   /* Lane 3: settled - further rounds leave the settings alone. */
   {
      unsigned idx_now = settings->uints.video_aspect_ratio_idx;
      unsigned w = VIDEO_SCALE_W(settings->video_vp_custom.dims);
      unsigned h = VIDEO_SCALE_H(settings->video_vp_custom.dims);
      one_frame();
      one_render();
      one_frame();
      one_render();
      CHECK(settings->uints.video_aspect_ratio_idx == idx_now
         && VIDEO_SCALE_W(settings->video_vp_custom.dims)  == w
         && VIDEO_SCALE_H(settings->video_vp_custom.dims) == h,
            "aspect config kept moving after settling");
   }

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("rgui_aspect_defer: all lanes passed\n");
   return 0;
}
