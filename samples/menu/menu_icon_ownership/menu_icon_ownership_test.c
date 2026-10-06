/* Menu icon ownership.
 *
 * Links the shipping RetroArch objects with only main() replaced, the
 * way ozone_color_theme_defer does, and drives the real menu drivers
 * against the null video driver with one difference: the driver's
 * load_texture and unload_texture are the harness's, so every handle
 * the frontend holds is accounted for. An unload of a handle not
 * live, or a handle still live at deinit, fails the lane.
 *
 *   ozone       A playlist whose sidebar icon file does not exist
 *               leaves node->icon at 0. The sidebar draw used to
 *               write the shared cursor texture into the node, and
 *               the context destroy then unloaded it twice: once from
 *               the node, once from the icon set. The lane draws one
 *               frame, then deinits.
 *
 *   xmb         The same fixture against the other horizontal list;
 *               the explore view's node no longer aliases the cursor.
 *
 *   materialui  An icon whose decode is still queued when the playlist
 *               icon array is freed and rebuilt (RESET_HORIZONTAL_LIST
 *               after the playlist file went) must land nowhere: the
 *               array it was aimed at is gone. A completion that still
 *               wrote through would store a handle in freed memory and
 *               leak it, which the live count at deinit catches.
 *
 * Usage: menu_icon_ownership_test ozone|xmb|materialui
 *
 * Requires a completed non-Qt build, same as the other menu harnesses:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/menu_icon_ownership/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <queues/task_queue.h>
#include <features/features_cpu.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../retroarch_types.h"
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

/* --- the accounting driver ----------------------------------------- */

#define LIVE_MAX 512

static uintptr_t live[LIVE_MAX];
static unsigned  live_count;
static unsigned  loads;
static unsigned  unloads;
static unsigned  unknown_unloads;

static uintptr_t harness_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   void *tex = malloc(16);
   (void)video_data;
   (void)data;
   (void)threaded;
   (void)filter_type;
   if (!tex)
      return 0;
   if (live_count < LIVE_MAX)
      live[live_count++] = (uintptr_t)tex;
   loads++;
   return (uintptr_t)tex;
}

static void harness_unload_texture(void *data, bool threaded, uintptr_t id)
{
   unsigned i;
   (void)data;
   (void)threaded;
   if (!id)
      return;
   unloads++;
   for (i = 0; i < live_count; i++)
   {
      if (live[i] == id)
      {
         live[i] = live[--live_count];
         free((void*)id);
         return;
      }
   }
   unknown_unloads++;
   fprintf(stderr, "unload of a handle not live: %p\n", (void*)id);
}

static const video_poke_interface_t harness_poke = {
   NULL, /* get_flags */
   harness_load_texture,
   harness_unload_texture,
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   NULL, /* set_texture_frame */
   NULL, /* set_texture_enable */
   NULL, /* set_osd_msg */
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL, /* set_hdr_subpixel_layout */
   NULL, /* supports_texture_format */
   NULL, /* load_texture_compressed */
   NULL, /* present_last */
   NULL  /* get_last_present_time */
};

static void harness_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &harness_poke;
}

/* --- the fixture --------------------------------------------------- */

/* A 1x1 RGBA PNG */
static const unsigned char png_1x1[] = {
   0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
   0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
   0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
   0x0b, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0x0f, 0x04, 0x00,
   0x09, 0xfb, 0x03, 0xfd, 0xfb, 0x5e, 0x6b, 0x2b, 0x00, 0x00, 0x00, 0x00,
   0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

static bool write_file(const char *path, const void *data, size_t len)
{
   RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   int64_t n;
   if (!f)
      return false;
   n = filestream_write(f, data, (int64_t)len);
   filestream_close(f);
   return n == (int64_t)len;
}

static bool write_text(const char *path, const char *text)
{
   return write_file(path, text, strlen(text));
}

static char fixture_dir[512];
static char icons_dir[768];
static char playlists_dir[640];
static char playlist_path[768];
static char cfg_path[640];

/* The icon directory every one of the three drivers reads its system
 * icons from on this build: <assets>/xmb/monochrome/png. */
static bool fixture_make(const char *menu_driver)
{
   char assets_dir[640];
   char path[768];
   const char *tmp = getenv("TMPDIR");

   snprintf(fixture_dir, sizeof(fixture_dir),
         "%s/menu_icon_ownership_%s_%u", tmp ? tmp : "/tmp", menu_driver,
         (unsigned)(cpu_features_get_time_usec() & 0xffffff));
   if (!path_mkdir(fixture_dir))
      return false;

   fill_pathname_join_special(assets_dir, fixture_dir, "assets",
         sizeof(assets_dir));
   fill_pathname_join_special(path, assets_dir, "xmb", sizeof(path));
   fill_pathname_join_special(icons_dir, path, "monochrome",
         sizeof(icons_dir));
   fill_pathname_join_special(path, icons_dir, "png", sizeof(path));
   strlcpy(icons_dir, path, sizeof(icons_dir));
   if (!path_mkdir(icons_dir))
      return false;

   fill_pathname_join_special(playlists_dir, fixture_dir, "playlists",
         sizeof(playlists_dir));
   if (!path_mkdir(playlists_dir))
      return false;

   /* The one icon every lane has: the cursor the sidebar falls back
    * to, and the one the alias doubled. */
   fill_pathname_join_special(path, icons_dir, "cursor.png", sizeof(path));
   if (!write_file(path, png_1x1, sizeof(png_1x1)))
      return false;

   fill_pathname_join_special(playlist_path, playlists_dir,
         "Harness System.lpl", sizeof(playlist_path));
   if (!write_text(playlist_path,
            "{\n  \"version\": \"1.5\",\n  \"items\": [\n  ]\n}\n"))
      return false;

   if (string_is_equal(menu_driver, "materialui"))
   {
      /* MaterialUI's lane needs the playlist's icon to exist, so
       * that a decode is queued for it */
      fill_pathname_join_special(path, icons_dir,
            "Harness System.png", sizeof(path));
      if (!write_file(path, png_1x1, sizeof(png_1x1)))
         return false;
   }

   fill_pathname_join_special(cfg_path, fixture_dir, "harness.cfg",
         sizeof(cfg_path));
   {
      char cfg[2048];
      size_t _len = 0;
      _len += snprintf(cfg + _len, sizeof(cfg) - _len,
            "video_driver = \"null\"\n"
            "audio_driver = \"null\"\n"
            "input_driver = \"null\"\n"
            "input_joypad_driver = \"null\"\n"
            "video_threaded = \"false\"\n"
            "threaded_data_runloop_enable = \"false\"\n"
            "menu_ignore_missing_assets = \"true\"\n"
            "content_show_playlists = \"true\"\n"
            "materialui_icons_enable = \"true\"\n"
            "materialui_playlist_icons_enable = \"true\"\n");
      /* The configuration names MaterialUI by its driver ident */
      _len += snprintf(cfg + _len, sizeof(cfg) - _len,
            "menu_driver = \"%s\"\n",
            string_is_equal(menu_driver, "materialui") ? "glui" : menu_driver);
      _len += snprintf(cfg + _len, sizeof(cfg) - _len,
            "assets_directory = \"%s\"\n", assets_dir);
      _len += snprintf(cfg + _len, sizeof(cfg) - _len,
            "playlist_directory = \"%s\"\n", playlists_dir);
      return write_text(cfg_path, cfg);
   }
}

static void fixture_remove(void)
{
   char cmd[600];
   snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", fixture_dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }
}

/* --- the lanes ----------------------------------------------------- */

static void one_frame(void)
{
   video_frame_info_t vinfo;
   struct menu_state *menu_st = menu_state_get_ptr();
   /* The null driver reports no size; the sidebar culls every entry
    * below a zero-height screen. */
   video_driver_set_output_dims(VIDEO_SCALE_PACK(1280, 720));
   video_driver_build_info(&vinfo);
   vinfo.dims = VIDEO_SCALE_PACK(1280, 720);
   menu_driver_frame(true, &vinfo);
   if (menu_st->driver_ctx && menu_st->driver_ctx->render)
      menu_st->driver_ctx->render(menu_st->userdata, vinfo.dims, false);
}

static void run_tasks(void)
{
   unsigned i;
   for (i = 0; i < 2000; i++)
      task_queue_check();
}

int main(int argc, char *argv[])
{
   const char *menu_driver = argc > 1 ? argv[1] : "ozone";
   char *rarch_argv[4];
   int rarch_argc = 0;
   struct menu_state *menu_st;

   if (     !string_is_equal(menu_driver, "ozone")
         && !string_is_equal(menu_driver, "xmb")
         && !string_is_equal(menu_driver, "materialui"))
   {
      fprintf(stderr, "usage: %s ozone|xmb|materialui\n", argv[0]);
      return 2;
   }

   /* Installed before the driver is picked: the null driver's poke
    * comes from this function from now on. */
   video_null.poke_interface = harness_get_poke_interface;

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);

   if (!fixture_make(menu_driver))
   {
      fprintf(stderr, "FAIL: could not write the fixture\n");
      return 1;
   }

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   retroarch_menu_running();

   menu_st = menu_state_get_ptr();
   CHECK(   menu_st->driver_ctx && menu_st->driver_ctx->ident
         && string_is_equal(menu_st->driver_ctx->ident,
            string_is_equal(menu_driver, "materialui")
               ? "glui" : menu_driver),
         "fixture: menu driver is %s, want %s",
         (menu_st->driver_ctx && menu_st->driver_ctx->ident)
            ? menu_st->driver_ctx->ident : "(none)", menu_driver);
   CHECK(loads > 0, "fixture: the menu loaded no texture at all");

   if (string_is_equal(menu_driver, "materialui"))
   {
      /* The icon's decode is queued and not yet run. The playlist
       * goes, and the icon array with it. */
      filestream_delete(playlist_path);
      if (menu_st->driver_ctx && menu_st->driver_ctx->environ_cb)
         menu_st->driver_ctx->environ_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST,
               NULL, menu_st->userdata);
      run_tasks();
      one_frame();
   }
   else
   {
      /* The first frame draws the sidebar with the playlist's icon
       * not there, cursor in hand. */
      run_tasks();
      one_frame();
      one_frame();
   }

   CHECK(unknown_unloads == 0,
         "%u unload(s) of a handle that was not live before deinit",
         unknown_unloads);

   retroarch_ctl(RARCH_CTL_MAIN_DEINIT, NULL);

   CHECK(unknown_unloads == 0,
         "%u unload(s) of a handle that was not live",
         unknown_unloads);
   CHECK(live_count == 0,
         "%u texture(s) still live after deinit (%u loaded, %u unloaded)",
         live_count, loads, unloads);

   fixture_remove();

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u) [%s]\n", failures, menu_driver);
      return 1;
   }
   printf("menu_icon_ownership [%s]: %u loaded, %u unloaded, all accounted for\n",
         menu_driver, loads, unloads);
   return 0;
}
