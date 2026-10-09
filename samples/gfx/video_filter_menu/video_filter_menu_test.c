/* The menu's video filter rows get the video driver the frames they
 * make: a chosen filter's scale and pixel format, and the core's or
 * the filter's format as Video Filter Enable turns it off and on.
 *
 * Links the shipping RetroArch objects with only main() replaced and
 * boots the frontend on a 240x160 RGB565 core (built next to this
 * binary by build.sh, with the Normal2x and Normal4x plugins and
 * widen32, a test filter giving XRGB8888 at twice the size). The rows are driven
 * through the menu's own entry points. A copy of the live video
 * driver's table is put in place before each step: a driver set up
 * again finds the real table, so the copy being gone says the driver
 * was rebuilt.
 *
 * With the null driver (the default), which cannot take a new frame
 * format in place:
 *  - choosing Normal4x sets the driver up again for its scale; Enable
 *    off and on with it is instant, its output keeping the core's
 *    format;
 *  - choosing widen32 sets the driver up again for 32-bit frames, and
 *    so does each turn of Enable, Normal2x after it (frames of the same
 *    size in the core's format), widen32 again, and removing it;
 *  - with a driver that says it takes a new format in place (a stand-in
 *    set_frame_format on the poke table), each change is told to it
 *    with the format and scale the frames now have, nothing is set up
 *    again, and a change the frames do not make tells it nothing.
 *
 * HARNESS_VIDEO_DRIVER=gl, glcore, vulkan or (on macOS) metal, under a
 * display (Xvfb on Linux), runs the same steps on that driver, which
 * takes them in place: nothing is set up again, and after each step
 * the picture read back from the screen is the one the driver draws
 * once set up again for it.
 * HARNESS_THREADED=1 runs it under the threaded video wrapper, and
 * HARNESS_SHADER=1 with a two-pass GLSL preset on gl (twopass.glslp).
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/gfx/video_filter_menu/build.sh
 *   samples/gfx/video_filter_menu/video_filter_menu_test
 *   HARNESS_VIDEO_DRIVER=vulkan xvfb-run -a \
 *      samples/gfx/video_filter_menu/video_filter_menu_test
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
#include "../../../gfx/video_shader_parse.h"
#ifdef HAVE_THREADS
#include "../../../gfx/video_thread_wrapper.h"
#endif
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
/* A driver that takes new frame formats in place */
static bool real_driver;

/* ------------------------------------------------------------------ */

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

#ifdef __APPLE__
/* threaded_video's harness_cocoa.m: the harness runs inside Cocoa's
 * application, which needs its run loop turned each frame */
void harness_cocoa_pump(void);
void harness_cocoa_exit_status(int status);
#endif

static void pump(unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      runloop_iterate();
      task_queue_check();
#ifdef __APPLE__
      harness_cocoa_pump();
#endif
   }
}

static void menu_open(bool open)
{
   if (menu_is_up() != open)
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

/* Remove Video Filter, as its menu entry's OK does */
static void remove_filter(void)
{
   config_get_ptr()->paths.path_softfilter_plugin[0] = '\0';
   video_driver_filter_free();
   video_driver_filter_apply();
}

/* ------------------------------------------------------------------ */
/* The picture on the screen                                          */
/* ------------------------------------------------------------------ */

static uint8_t *read_screen(size_t *len)
{
   struct video_viewport vp;
   video_driver_state_t *video_st = video_state_get_ptr();
   uint8_t *buf;

   *len = 0;
#ifdef HAVE_THREADS
   if (video_st->thread_wrapper_active)
      video_thread_wait_idle();
#endif
   memset(&vp, 0, sizeof(vp));
   video_driver_get_viewport_info(&vp);
   *len = (size_t)VIDEO_SCALE_W(vp.dims) * VIDEO_SCALE_H(vp.dims) * 3;
   if (!*len || !video_st->current_video->read_viewport)
      return NULL;
   if (!(buf = (uint8_t*)malloc(*len)))
      return NULL;
   if (!video_st->current_video->read_viewport(video_st->data, buf, false))
   {
      free(buf);
      return NULL;
   }
   return buf;
}

/* The core's frames drawn as the driver now takes them, against the
 * same drawn by the driver set up again for them. */
static void check_screen(const char *step)
{
   char what[256];
   size_t len_a, len_b;
   uint8_t *a, *b;

   menu_open(false);
   pump(6);
   a = read_screen(&len_a);
   command_event(CMD_EVENT_REINIT, NULL);
   pump(6);
   b = read_screen(&len_b);
   menu_open(true);

   snprintf(what, sizeof(what), "%s: the screen could not be read", step);
   CHECK(a && b, what);
   if (a && b)
   {
      size_t i, diff = 0;
      snprintf(what, sizeof(what),
            "%s: the viewport changed size across the reinit", step);
      CHECK(len_a == len_b, what);
      for (i = 0; i < len_a && i < len_b; i++)
         if (a[i] != b[i])
            diff++;
      snprintf(what, sizeof(what),
            "%s: %lu of %lu bytes on the screen differ from the driver "
            "set up again", step, (unsigned long)diff,
            (unsigned long)len_a);
      CHECK(diff == 0, what);
   }
   free(a);
   free(b);
}

/* A step: the driver is set up again where it cannot take the change,
 * kept where it can, and its picture matches a fresh setup. */
static void step_done(const char *step, bool needs_setup)
{
   char what[256];
   if (real_driver)
   {
      snprintf(what, sizeof(what), "%s: the driver was set up again", step);
      CHECK(!driver_rebuilt(), what);
      check_screen(step);
   }
   else if (needs_setup)
   {
      snprintf(what, sizeof(what),
            "%s: the driver was not set up again for its frames", step);
      CHECK(driver_rebuilt(), what);
      menu_open(false);
      pump(4);
      menu_open(true);
   }
   else
   {
      snprintf(what, sizeof(what),
            "%s: the driver was set up again for frames it takes as is",
            step);
      CHECK(!driver_rebuilt(), what);
   }
   hook_install();
}

/* ------------------------------------------------------------------ */
/* Lanes                                                              */
/* ------------------------------------------------------------------ */

static void lane_steps(void)
{
   settings_t *settings = config_get_ptr();
   unsigned had         = failures;

   hook_install();
   choose_filter("Normal4x.filt");
   CHECK(video_state_get_ptr()->state_filter && !filter_out_rgb32()
         && video_state_get_ptr()->state_scale >= 4,
         "Normal4x: the filter is not loaded");
   step_done("Normal4x chosen", true);

   toggle_enable();
   CHECK(!settings->bools.video_filter_enable, "Normal4x: Enable did not turn off");
   step_done("Normal4x, Enable off", false);
   toggle_enable();
   CHECK(settings->bools.video_filter_enable, "Normal4x: Enable did not turn on");
   step_done("Normal4x, Enable on", false);

   choose_filter("widen32.filt");
   CHECK(video_state_get_ptr()->state_filter && filter_out_rgb32(),
         "widen32: the filter is not loaded with 32-bit output");
   step_done("widen32 chosen", true);

   toggle_enable();
   CHECK(!settings->bools.video_filter_enable, "widen32: Enable did not turn off");
   step_done("widen32, Enable off", true);
   toggle_enable();
   CHECK(settings->bools.video_filter_enable, "widen32: Enable did not turn on");
   step_done("widen32, Enable on", true);

   /* The same size, the other format, both ways */
   choose_filter("Normal2x.filt");
   CHECK(video_state_get_ptr()->state_filter && !filter_out_rgb32(),
         "Normal2x: the filter is not loaded");
   step_done("Normal2x chosen after widen32", true);
   choose_filter("widen32.filt");
   step_done("widen32 chosen after Normal2x", true);

   remove_filter();
   CHECK(!video_state_get_ptr()->state_filter, "the filter was not removed");
   step_done("widen32 removed", true);

   if (failures == had)
      fprintf(stderr, "[pass] filter steps on %s%s: %s\n",
            settings->arrays.video_driver,
            video_state_get_ptr()->thread_wrapper_active ? " (threaded)" : "",
            real_driver
            ? "taken in place, the screen as after a fresh setup"
            : "set up again for a new format or a larger scale only");
}

/* The stand-in for a driver that takes frame formats in place */
static video_poke_interface_t fake_poke;
static unsigned fake_calls;
static bool     fake_rgb32;
static unsigned fake_scale;

static bool fake_set_frame_format(void *data, bool rgb32, unsigned input_scale)
{
   (void)data;
   fake_calls++;
   fake_rgb32 = rgb32;
   fake_scale = input_scale;
   return true;
}

static void fake_expect(const char *step, unsigned calls, bool rgb32,
      unsigned scale)
{
   char what[256];
   snprintf(what, sizeof(what),
         "%s: told %u times (want %u), last 32-bit %d scale %u "
         "(want %d, %u)", step, fake_calls, calls, fake_rgb32,
         fake_scale, rgb32, scale);
   CHECK(     fake_calls == calls
         && (!calls || (fake_rgb32 == rgb32 && fake_scale == scale)), what);
   snprintf(what, sizeof(what), "%s: the driver was set up again", step);
   CHECK(!driver_rebuilt(), what);
}

static void lane_told_in_place(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   unsigned had                   = failures;

   /* From the core's own frames, at scale 1 (240x160 in 256) */
   if (video_st->state_filter)
      remove_filter();
   if (!settings->bools.video_filter_enable)
      toggle_enable();
   command_event(CMD_EVENT_REINIT, NULL);
   menu_open(true);

   if (video_st->poke)
      fake_poke               = *video_st->poke;
   fake_poke.set_frame_format = fake_set_frame_format;
   video_st->poke             = &fake_poke;
   fake_calls                 = 0;
   hook_install();

   choose_filter("widen32.filt");       /* 480x320 in 512 */
   fake_expect("widen32 chosen", 1, true, 2);
   toggle_enable();
   fake_expect("widen32, Enable off", 2, false, 2);
   toggle_enable();
   fake_expect("widen32, Enable on", 3, true, 2);
   remove_filter();
   fake_expect("widen32 removed", 4, false, 2);
   choose_filter("Normal4x.filt");      /* 960x640 in 1024 */
   fake_expect("Normal4x chosen", 5, false, 4);
   toggle_enable();
   fake_expect("Normal4x, Enable off", 5, false, 4);
   toggle_enable();
   fake_expect("Normal4x, Enable on", 5, false, 4);

   menu_open(false);
   pump(4);
   menu_open(true);

   if (failures == had)
      fprintf(stderr, "[pass] a driver taking frame formats in place is "
            "told each change once, with what the frames now are\n");
}

/* ------------------------------------------------------------------ */

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
   const char *drv     = getenv("HARNESS_VIDEO_DRIVER");
   FILE *cfg;
   (void)argc;

   if (!drv || !*drv)
      drv = "null";
   real_driver = !string_is_equal(drv, "null");

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
   fprintf(cfg, "video_driver = \"%s\"\n", drv);
   fprintf(cfg, "audio_driver = \"null\"\n");
   fprintf(cfg, "input_driver = \"null\"\n");
   fprintf(cfg, "input_joypad_driver = \"null\"\n");
   fprintf(cfg, "menu_driver = \"rgui\"\n");
   fprintf(cfg, "video_threaded = \"%s\"\n",
         string_is_equal(getenv("HARNESS_THREADED"), "1") ? "true" : "false");
   fprintf(cfg, "video_vsync = \"false\"\n");
   fprintf(cfg, "video_fullscreen = \"false\"\n");
   fprintf(cfg, "video_smooth = \"false\"\n");
   fprintf(cfg, "video_shader_enable = \"%s\"\n",
         string_is_equal(getenv("HARNESS_SHADER"), "1") ? "true" : "false");
   fprintf(cfg, "video_font_enable = \"false\"\n");
   fprintf(cfg, "video_filter_enable = \"true\"\n");
   fprintf(cfg, "video_filter_dir = \"%s\"\n", filter_dir);
   fprintf(cfg, "menu_pause_libretro = \"true\"\n");
   fprintf(cfg, "menu_enable_widgets = \"false\"\n");
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
   if (!string_is_equal(config_get_ptr()->arrays.video_driver, drv))
   {
      fprintf(stderr, "FAIL: video driver %s did not come up (%s)\n",
            drv, config_get_ptr()->arrays.video_driver);
      return 1;
   }
   /* gl: the steps with a two-pass preset, the render chain's passes
    * made again around the frame textures */
   if (string_is_equal(getenv("HARNESS_SHADER"), "1"))
   {
      char preset[600];
      snprintf(preset, sizeof(preset), "%.*s/twopass.glslp", dirlen, base);
      if (!video_shader_apply_shader(config_get_ptr(),
               RARCH_SHADER_GLSL, preset, false))
      {
         fprintf(stderr, "FAIL: the preset %s did not load\n", preset);
         return 1;
      }
      pump(2);
   }
   menu_open(true);
   if (!menu_is_up())
   {
      fprintf(stderr, "FAIL: the menu did not open\n");
      return 1;
   }

   lane_steps();
   if (!real_driver)
      lane_told_in_place();

#ifdef __APPLE__
   /* main_exit() terminates the Cocoa application; the status goes
    * with it */
   harness_cocoa_exit_status(failures ? 1 : 0);
#endif
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
