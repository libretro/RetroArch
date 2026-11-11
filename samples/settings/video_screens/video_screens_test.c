/* Exercises indexed XR settings and the screens' own windows through
 * shipping configuration and video objects. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include <compat/strl.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <time/rtime.h>
#include <features/features_cpu.h>
#include <string/stdstring.h>
#include <retro_timers.h>

#include "../../../configuration.h"
#include "../../../verbosity.h"
#include "../../../gfx/video_windows.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../paths.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../menu/menu_setting.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_displaylist.h"
#include "../../../menu/menu_cbs.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/video_thread_wrapper.h"
#ifdef HAVE_GFX_WIDGETS
#include "../../../gfx/gfx_display.h"
#include "../../../gfx/gfx_widgets.h"
#endif
#ifdef HAVE_VULKAN
#include "../../../gfx/common/vulkan_common.h"
#endif
#include "../../../input/input_driver.h"
#include "../../../input/input_defines.h"
#include "../../../input/input_keymaps.h"
#include "../../../input/input_remapping.h"
#include "../../../tasks/tasks_internal.h"
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#endif

#ifdef HAVE_X11
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>
#include <X11/Xatom.h>
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include "../../../gfx/common/x11_common.h"
#include "../../../gfx/common/x11_monitors.h"
#include "../../../gfx/common/dbus_common.h"
#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
#define NATIVE_GLX
#include <GL/glx.h>
#endif
#endif

#ifdef HAVE_WAYLAND
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "../../../gfx/common/wayland_common.h"
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#define NATIVE_WIN32
#include "../../../gfx/common/win32_common.h"
#include "../../../ui/ui_companion_driver.h"
#include <queues/task_queue.h>
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
#define NATIVE_WGL
#endif
#endif

#if defined(HAVE_SDL3) || defined(HAVE_SDL2)
#define NATIVE_SDL
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#ifndef HAVE_LIBDECOR_H
#define NATIVE_NO_LIBDECOR
#endif
#ifdef HAVE_SDL3
#include "../../../gfx/common/sdl3_common.h"
#else
#include "../../../gfx/common/sdl2_common.h"
#include "SDL_syswm.h"
#endif
/* SDL 2's SDL_config.h has its own build's HAVE_LIBDECOR_H */
#ifdef NATIVE_NO_LIBDECOR
#undef HAVE_LIBDECOR_H
#endif
#endif

#if defined(HAVE_X11) || defined(HAVE_WAYLAND) || defined(NATIVE_WIN32) \
   || defined(NATIVE_SDL)
#define NATIVE_WINDOWS
#if defined(HAVE_EGL) && !defined(HAVE_DYNAMIC_EGL)
#define NATIVE_EGL
#include <EGL/egl.h>
#endif
#endif

static unsigned failures;
static char fixture_path[2048];
static char saved_path[2048];
static char override_path[2048];
static char generated_path[2048];

#define CHECK(condition) \
   do { \
      if (!(condition)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
         failures++; \
      } \
   } while (0)

static void write_config(const char *path, const char *text)
{
   CHECK(filestream_write_file(path, text, strlen(text)));
}

static void load_fixture(const char *text)
{
   write_config(fixture_path, text);
   path_clear(RARCH_PATH_CONFIG_OVERRIDE);
   path_clear(RARCH_PATH_CONFIG_APPEND);
   path_set(RARCH_PATH_CONFIG, fixture_path);
   config_load();
}

static void test_window_config(void)
{
   static const char *fields[] = {"mode", "x", "y", "width", "height", "pos_set", "monitor", "fullscreen"};
   static rarch_system_info_t system_info;
   settings_t *settings = config_get_ptr();
   config_file_t *conf;
   unsigned i, j, minimal;
   int value;
   bool flag;
   char text[64];
   memset(&system_info, 0, sizeof(system_info));
   system_info.info.library_name = "video_screens_test";
   load_fixture("");
   CHECK(!settings->bools.video_windows_enable);
   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      CHECK(!settings->arrays.video_screen_display[i][0]);
      CHECK(settings->uints.video_screen_window_dims[i] == 0);
      CHECK(settings->uints.video_screen_window_pos[i] == 0);
      CHECK(!settings->bools.video_screen_window_fullscreen[i]);
      CHECK(!settings->bools.video_screen_window_pos_set[i]);
   }
   settings->bools.config_save_minimal = true;
   CHECK(config_save_file(saved_path));
   conf = config_file_new_from_path_to_string(saved_path);
   CHECK(conf != NULL);
   if (conf)
   {
      char key[64];
      CHECK(config_get_entry(conf, "video_windows_enable") == NULL);
      for (i = 1; i <= RETRO_VIDEO_VIEWS_MAX; i++)
         for (j = 0; j < sizeof(fields) / sizeof(fields[0]); j++)
         {
            snprintf(key, sizeof(key), "video_screen%u_window_%s", i, fields[j]);
            CHECK(config_get_entry(conf, key) == NULL);
         }
      config_file_free(conf);
   }
   for (minimal = 0; minimal < 2; minimal++)
   {
      load_fixture("video_windows_enable = \"true\"\n"
            "video_screen1_window_mode = \"2\"\n"
            "video_screen2_window_mode = \"2\"\n"
            "video_screen2_window_x = \"-1200\"\n"
            "video_screen2_window_y = \"-30\"\n"
            "video_screen2_window_width = \"640\"\n"
            "video_screen2_window_height = \"480\"\n"
            "video_screen2_window_pos_set = \"true\"\n"
            "video_screen2_window_monitor = \"3\"\n"
            "video_screen2_window_fullscreen = \"true\"\n"
            "video_screen8_window_mode = \"0\"\n");
      CHECK(settings->bools.video_windows_enable);
      CHECK(string_is_equal(settings->arrays.video_screen_display[0],
               VIDEO_WINDOW_DISPLAY_OFF));
      CHECK(string_is_equal(settings->arrays.video_screen_display[1],
               VIDEO_WINDOW_DISPLAY_OFF));
      CHECK(settings->uints.video_screen_window_pos[1] == VIDEO_POS_PACK(-1200, -30));
      settings->bools.config_save_minimal = minimal != 0;
      write_config(saved_path, "video_screen2_window_monitor = \"3\"\n");
      CHECK(config_save_file(saved_path));
      conf = config_file_new_from_path_to_string(saved_path);
      CHECK(conf != NULL);
      if (conf)
      {
         CHECK(config_get_entry(conf, "video_screen2_window_monitor") == NULL);
         config_file_free(conf);
      }
      path_set(RARCH_PATH_CONFIG, saved_path);
      config_load();
      CHECK(settings->uints.video_screen_window_pos[1] == VIDEO_POS_PACK(-1200, -30));
      CHECK(settings->uints.video_screen_window_dims[1] == VIDEO_SCALE_PACK(640, 480));
      CHECK(settings->bools.video_screen_window_fullscreen[1]);
      CHECK(settings->bools.video_screen_window_pos_set[1]);
      CHECK(string_is_equal(settings->arrays.video_screen_display[1],
               VIDEO_WINDOW_DISPLAY_OFF));
      CHECK(string_is_equal(settings->arrays.video_screen_display[7],
               VIDEO_WINDOW_DISPLAY_MAIN));
      write_config(override_path, "video_windows_enable = \"false\"\n"
            "video_screen2_display = \"\"\n"
            "video_screen2_window_x = \"0\"\n"
            "video_screen2_window_y = \"0\"\n"
            "video_screen2_window_width = \"800\"\n"
            "video_screen2_window_height = \"600\"\n"
            "video_screen2_window_pos_set = \"false\"\n"
            "video_screen2_window_monitor = \"0\"\n"
            "video_screen2_window_fullscreen = \"false\"\n");
      CHECK(config_load_override_file(override_path));
      CHECK(!settings->bools.video_windows_enable);
      CHECK(!settings->arrays.video_screen_display[1][0]);
      CHECK(settings->uints.video_screen_window_pos[1] == VIDEO_POS_PACK(0, 0));
      CHECK(settings->uints.video_screen_window_dims[1] == VIDEO_SCALE_PACK(800, 600));
      CHECK(!settings->bools.video_screen_window_pos_set[1]);
      path_set(RARCH_PATH_BASENAME, "window_game.rom");
      CHECK(config_save_overrides(OVERRIDE_GAME, &system_info, false, NULL) == 1);
      strlcpy(generated_path, path_get(RARCH_PATH_CONFIG_OVERRIDE), sizeof(generated_path));
      conf = config_file_new_from_path_to_string(generated_path);
      CHECK(conf != NULL);
      if (conf)
      {
         CHECK(config_get_int(conf, "video_screen2_window_x", &value) && value == 0);
         CHECK(config_get_int(conf, "video_screen2_window_width", &value) && value == 800);
         CHECK(config_get_entry(conf, "video_screen2_window_monitor") == NULL);
         CHECK(config_get_bool(conf, "video_screen2_window_pos_set", &flag) && !flag);
         CHECK(config_get_array(conf, "video_screen2_display", text, sizeof(text))
               && !text[0]);
         CHECK(config_get_entry(conf, "video_screen2_window_mode") == NULL);
         CHECK(config_get_bool(conf, "video_windows_enable", &flag) && !flag);
         config_file_free(conf);
      }
      CHECK(config_unload_override());
      CHECK(settings->bools.video_windows_enable);
      CHECK(string_is_equal(settings->arrays.video_screen_display[1],
               VIDEO_WINDOW_DISPLAY_OFF));
      CHECK(settings->uints.video_screen_window_pos[1] == VIDEO_POS_PACK(-1200, -30));
      CHECK(settings->bools.video_screen_window_pos_set[1]);
      CHECK(config_load_override_file(generated_path));
      CHECK(settings->uints.video_screen_window_pos[1] == VIDEO_POS_PACK(0, 0));
      CHECK(!settings->bools.video_screen_window_fullscreen[1]);
      CHECK(!settings->bools.video_screen_window_pos_set[1]);
      CHECK(config_unload_override());
      path_clear(RARCH_PATH_BASENAME);
   }
   load_fixture("video_screen1_window_mode = \"1\"\n"
         "video_screen2_window_mode = \"99\"\n"
         "video_screen2_window_x = \"-32769\"\n"
         "video_screen2_window_y = \"32768\"\n"
         "video_screen2_window_width = \"65536\"\n"
         "video_screen2_window_height = \"1\"\n"
         "video_screen3_window_x = \"-32768\"\n"
         "video_screen3_window_y = \"32767\"\n"
         "video_screen3_window_width = \"65535\"\n"
         "video_screen3_window_height = \"65535\"\n");
   CHECK(!settings->arrays.video_screen_display[0][0]);
   CHECK(!settings->arrays.video_screen_display[1][0]);
   CHECK(settings->uints.video_screen_window_pos[1] == 0);
   CHECK(settings->uints.video_screen_window_dims[1] == 0);
   CHECK(settings->uints.video_screen_window_pos[2] == VIDEO_POS_PACK(-32768, 32767));
   CHECK(settings->uints.video_screen_window_dims[2] == VIDEO_SCALE_PACK(65535, 65535));
}

static void test_xr_config_roundtrip(void)
{
   settings_t *settings = config_get_ptr();
   unsigned i;
   load_fixture(
         "video_screen1_xr_x = \"1.5\"\n"
         "video_screen1_xr_y = \"-2\"\n"
         "video_screen1_xr_z = \"3\"\n"
         "video_screen1_xr_yaw = \"90\"\n"
         "video_screen1_xr_pitch = \"-45\"\n"
         "video_screen1_xr_roll = \"180\"\n"
         "video_screen1_xr_scale = \"2\"\n"
         "video_screen1_xr_visible = \"false\"\n"
         "video_screen2_xr_x = \"0.25\"\n"
         "video_screen8_xr_y = \"4\"\n"
         "video_screen8_xr_yaw = \"-30\"\n");
   CHECK(settings->floats.video_screen_xr_x[0] == 1.5f);
   CHECK(settings->floats.video_screen_xr_x[1] == 0.25f);
   CHECK(settings->floats.video_screen_xr_scale[7] == 1.0f);
   CHECK(settings->bools.video_screen_xr_visible[7]);
   settings->bools.config_save_minimal = false;
   CHECK(config_save_file(saved_path));
   path_set(RARCH_PATH_CONFIG, saved_path);
   config_load();
   CHECK(settings->floats.video_screen_xr_x[0] == 1.5f);
   CHECK(settings->floats.video_screen_xr_y[0] == -2.0f);
   CHECK(settings->floats.video_screen_xr_z[0] == 3.0f);
   CHECK(settings->floats.video_screen_xr_yaw[0] == 90.0f);
   CHECK(settings->floats.video_screen_xr_pitch[0] == -45.0f);
   CHECK(settings->floats.video_screen_xr_roll[0] == 180.0f);
   CHECK(settings->floats.video_screen_xr_scale[0] == 2.0f);
   CHECK(!settings->bools.video_screen_xr_visible[0]);
   CHECK(settings->floats.video_screen_xr_y[7] == 4.0f);
   CHECK(settings->floats.video_screen_xr_yaw[7] == -30.0f);
   config_reset_video_screen_xr(settings, 0, 1);
   CHECK(settings->floats.video_screen_xr_x[1] == 0.25f);
   CHECK(settings->floats.video_screen_xr_x[0] == 0.0f);
   CHECK(settings->floats.video_screen_xr_scale[7] == 1.0f);
   CHECK(settings->bools.video_screen_xr_visible[7]);
   settings->floats.video_screen_xr_x[0] = 3.0f;
   config_reset_video_screen_xr(settings, 1, 1);
   CHECK(settings->floats.video_screen_xr_x[1] == 0.0f);
   CHECK(settings->floats.video_screen_xr_x[0] == 3.0f);
   config_reset_video_screen_xr(settings, RETRO_VIDEO_VIEWS_MAX, UINT_MAX);
   CHECK(settings->floats.video_screen_xr_y[7] == 4.0f);
   config_reset_video_screen_xr(settings, 7, UINT_MAX);
   CHECK(settings->floats.video_screen_xr_y[7] == 0.0f);
   CHECK(settings->floats.video_screen_xr_x[0] == 3.0f);
   config_reset_video_screen_xr(settings, 0, RETRO_VIDEO_VIEWS_MAX);
   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      CHECK(settings->floats.video_screen_xr_x[i] == 0.0f);
      CHECK(settings->floats.video_screen_xr_y[i] == 0.0f);
      CHECK(settings->floats.video_screen_xr_z[i] == 0.0f);
      CHECK(settings->floats.video_screen_xr_yaw[i] == 0.0f);
      CHECK(settings->floats.video_screen_xr_pitch[i] == 0.0f);
      CHECK(settings->floats.video_screen_xr_roll[i] == 0.0f);
      CHECK(settings->floats.video_screen_xr_scale[i] == 1.0f);
      CHECK(settings->bools.video_screen_xr_visible[i]);
   }

   write_config(override_path, "video_screen2_xr_x = \"0.5\"\n");
   path_set(RARCH_PATH_CONFIG_APPEND, override_path);
   config_load();
   CHECK(settings->floats.video_screen_xr_x[1] == 0.5f);
   write_config(fixture_path, "video_screen2_xr_x = \"0.75\"\n");
   CHECK(config_load_override_file(fixture_path));
   CHECK(settings->floats.video_screen_xr_x[1] == 0.75f);
   CHECK(config_unload_override());
   CHECK(settings->floats.video_screen_xr_x[1] == 0.5f);
   path_clear(RARCH_PATH_CONFIG_APPEND);
}

static void test_xr_minimal_save(void)
{
   static const char *fields[] = {"x", "y", "z", "yaw", "pitch", "roll", "scale", "visible"};
   settings_t *settings = config_get_ptr();
   config_file_t *conf;
   unsigned i, j;
   char key[64];
   float value = 0.0f;
   bool visible = true;
   load_fixture("config_save_minimal = \"true\"\n");
   CHECK(config_save_file(saved_path));
   conf = config_file_new_from_path_to_string(saved_path);
   CHECK(conf != NULL);
   if (conf)
   {
      for (i = 1; i <= RETRO_VIDEO_VIEWS_MAX; i++)
         for (j = 0; j < sizeof(fields) / sizeof(fields[0]); j++)
         {
            snprintf(key, sizeof(key), "video_screen%u_xr_%s", i, fields[j]);
            CHECK(config_get_entry(conf, key) == NULL);
         }
      config_file_free(conf);
   }
   settings->floats.video_screen_xr_x[1] = 0.25f;
   settings->bools.video_screen_xr_visible[7] = false;
   CHECK(config_save_file(saved_path));
   conf = config_file_new_from_path_to_string(saved_path);
   CHECK(conf != NULL);
   if (conf)
   {
      CHECK(config_get_float(conf, "video_screen2_xr_x", &value));
      CHECK(value == 0.25f);
      CHECK(config_get_bool(conf, "video_screen8_xr_visible", &visible));
      CHECK(!visible);
      CHECK(config_get_entry(conf, "video_screen8_xr_scale") == NULL);
      config_file_free(conf);
   }
   path_set(RARCH_PATH_CONFIG, saved_path);
   config_load();
   CHECK(settings->floats.video_screen_xr_x[1] == 0.25f);
   CHECK(settings->floats.video_screen_xr_scale[7] == 1.0f);
   CHECK(!settings->bools.video_screen_xr_visible[7]);
}

static void test_xr_override_unload(void)
{
   settings_t *settings = config_get_ptr();
   static rarch_system_info_t system_info;
   config_file_t *conf;
   float value = -1.0f;
   bool visible = false;
   unsigned minimal;
   memset(&system_info, 0, sizeof(system_info));
   system_info.info.library_name = "video_screens_test";
   path_set(RARCH_PATH_BASENAME, "screen_game.rom");
   for (minimal = 0; minimal < 2; minimal++)
   {
      load_fixture(minimal
            ? "config_save_minimal = \"true\"\nvideo_screen1_xr_x = \"1.5\"\nvideo_screen8_xr_scale = \"2\"\nvideo_screen8_xr_visible = \"false\"\n"
            : "config_save_minimal = \"false\"\nvideo_screen1_xr_x = \"1.5\"\nvideo_screen8_xr_scale = \"2\"\nvideo_screen8_xr_visible = \"false\"\n");
      write_config(override_path,
            "video_screen1_xr_x = \"0\"\n"
            "video_screen2_xr_y = \"-3\"\n"
            "video_screen8_xr_scale = \"1\"\n"
            "video_screen8_xr_visible = \"true\"\n");
      CHECK(config_load_override_file(override_path));
      CHECK(settings->floats.video_screen_xr_x[0] == 0.0f);
      CHECK(settings->floats.video_screen_xr_y[1] == -3.0f);
      CHECK(settings->floats.video_screen_xr_scale[7] == 1.0f);
      CHECK(settings->bools.video_screen_xr_visible[7]);
      CHECK(config_save_overrides(OVERRIDE_GAME, &system_info, false, NULL) == 1);
      strlcpy(generated_path, path_get(RARCH_PATH_CONFIG_OVERRIDE), sizeof(generated_path));
      conf = config_file_new_from_path_to_string(generated_path);
      CHECK(conf != NULL);
      if (conf)
      {
         CHECK(config_get_float(conf, "video_screen1_xr_x", &value));
         CHECK(value == 0.0f);
         CHECK(config_get_float(conf, "video_screen8_xr_scale", &value));
         CHECK(value == 1.0f);
         CHECK(config_get_bool(conf, "video_screen8_xr_visible", &visible));
         CHECK(visible);
         CHECK(config_get_entry(conf, "video_screen1_xr_scale") == NULL);
         config_file_free(conf);
      }
      CHECK(config_unload_override());
      CHECK(settings->floats.video_screen_xr_x[0] == 1.5f);
      CHECK(settings->floats.video_screen_xr_y[1] == 0.0f);
      CHECK(settings->floats.video_screen_xr_scale[7] == 2.0f);
      CHECK(!settings->bools.video_screen_xr_visible[7]);
      CHECK(config_load_override_file(generated_path));
      CHECK(settings->floats.video_screen_xr_x[0] == 0.0f);
      CHECK(settings->floats.video_screen_xr_y[1] == -3.0f);
      CHECK(settings->floats.video_screen_xr_scale[7] == 1.0f);
      CHECK(settings->bools.video_screen_xr_visible[7]);
      CHECK(config_unload_override());
      CHECK(settings->floats.video_screen_xr_x[0] == 1.5f);
      CHECK(settings->floats.video_screen_xr_y[1] == 0.0f);
      CHECK(settings->floats.video_screen_xr_scale[7] == 2.0f);
      CHECK(!settings->bools.video_screen_xr_visible[7]);
   }
   path_clear(RARCH_PATH_BASENAME);
}

static void test_xr_config_invalid(void)
{
   settings_t *settings = config_get_ptr();
   load_fixture(
         "video_screen1_xr_x = \"100\"\n"
         "video_screen1_xr_y = \"-100\"\n"
         "video_screen1_xr_z = \"nan\"\n"
         "video_screen1_xr_yaw = \"999\"\n"
         "video_screen1_xr_pitch = \"-999\"\n"
         "video_screen1_xr_roll = \"inf\"\n"
         "video_screen1_xr_scale = \"-1\"\n"
         "video_screen1_xr_visible = \"false\"\n"
         "video_screen2_xr_x = \"nan\"\n"
         "video_screen2_xr_y = \"inf\"\n"
         "video_screen2_xr_z = \"-inf\"\n"
         "video_screen2_xr_yaw = \"nan\"\n"
         "video_screen2_xr_pitch = \"inf\"\n"
         "video_screen2_xr_roll = \"-inf\"\n"
         "video_screen2_xr_scale = \"nan\"\n"
         "video_screen8_xr_scale = \"999\"\n"
         "video_screen9_xr_x = \"5\"\n");
   CHECK(settings->floats.video_screen_xr_x[0] == 10.0f);
   CHECK(settings->floats.video_screen_xr_y[0] == -10.0f);
   CHECK(settings->floats.video_screen_xr_z[0] == 0.0f);
   CHECK(settings->floats.video_screen_xr_yaw[0] == 180.0f);
   CHECK(settings->floats.video_screen_xr_pitch[0] == -180.0f);
   CHECK(settings->floats.video_screen_xr_roll[0] == 0.0f);
   CHECK(settings->floats.video_screen_xr_scale[0] == 0.1f);
   CHECK(!settings->bools.video_screen_xr_visible[0]);
   CHECK(settings->floats.video_screen_xr_x[1] == 0.0f);
   CHECK(settings->floats.video_screen_xr_y[1] == 0.0f);
   CHECK(settings->floats.video_screen_xr_z[1] == 0.0f);
   CHECK(settings->floats.video_screen_xr_yaw[1] == 0.0f);
   CHECK(settings->floats.video_screen_xr_pitch[1] == 0.0f);
   CHECK(settings->floats.video_screen_xr_roll[1] == 0.0f);
   CHECK(settings->floats.video_screen_xr_scale[1] == 1.0f);
   CHECK(settings->floats.video_screen_xr_scale[7] == 10.0f);
   CHECK(settings->floats.video_screen_xr_x[7] == 0.0f);
}

static void test_xr_menu_place_config(void)
{
   static rarch_system_info_t system_info;
   settings_t *settings = config_get_ptr();
   config_file_t *conf;
   float value = 0.0f;
   bool placed = true;

   load_fixture(
         "video_openxr_menu_xr_x = \"0.5\"\n"
         "video_openxr_menu_xr_z = \"-0.25\"\n"
         "video_openxr_menu_xr_yaw = \"30\"\n"
         "video_openxr_menu_xr_pitch = \"-15\"\n"
         "video_openxr_menu_xr_roll = \"45\"\n"
         "video_openxr_menu_xr_scale = \"0.75\"\n"
         "video_openxr_menu_placed = \"true\"\n");
   CHECK(settings->floats.video_openxr_menu_xr_x == 0.5f);
   CHECK(settings->floats.video_openxr_menu_xr_z == -0.25f);
   CHECK(settings->floats.video_openxr_menu_xr_yaw == 30.0f);
   CHECK(settings->floats.video_openxr_menu_xr_scale == 0.75f);
   CHECK(settings->bools.video_openxr_menu_placed);
   settings->bools.config_save_minimal = false;
   CHECK(config_save_file(saved_path));
   path_set(RARCH_PATH_CONFIG, saved_path);
   config_load();
   CHECK(settings->floats.video_openxr_menu_xr_x == 0.5f);
   CHECK(settings->floats.video_openxr_menu_xr_z == -0.25f);
   CHECK(settings->floats.video_openxr_menu_xr_yaw == 30.0f);
   CHECK(settings->floats.video_openxr_menu_xr_pitch == -15.0f);
   CHECK(settings->floats.video_openxr_menu_xr_roll == 45.0f);
   CHECK(settings->floats.video_openxr_menu_xr_scale == 0.75f);
   CHECK(settings->bools.video_openxr_menu_placed);

   /* Minimal: only what differs from the defaults. */
   load_fixture("config_save_minimal = \"true\"\nvideo_openxr_menu_xr_y = \"1.5\"\n");
   CHECK(!settings->bools.video_openxr_menu_placed);
   CHECK(config_save_file(saved_path));
   conf = config_file_new_from_path_to_string(saved_path);
   CHECK(conf != NULL);
   if (conf)
   {
      CHECK(config_get_float(conf, "video_openxr_menu_xr_y", &value));
      CHECK(value == 1.5f);
      CHECK(config_get_entry(conf, "video_openxr_menu_xr_x") == NULL);
      CHECK(config_get_entry(conf, "video_openxr_menu_xr_scale") == NULL);
      CHECK(config_get_entry(conf, "video_openxr_menu_placed") == NULL);
      config_file_free(conf);
   }

   /* Out of range clamps, non-finite takes the default. */
   load_fixture(
         "video_openxr_menu_xr_x = \"100\"\n"
         "video_openxr_menu_xr_roll = \"nan\"\n"
         "video_openxr_menu_xr_scale = \"0\"\n");
   CHECK(settings->floats.video_openxr_menu_xr_x == 10.0f);
   CHECK(settings->floats.video_openxr_menu_xr_roll == 0.0f);
   CHECK(settings->floats.video_openxr_menu_xr_scale == 0.1f);

   /* A game override's place, and the base's back on unload. */
   memset(&system_info, 0, sizeof(system_info));
   system_info.info.library_name = "video_screens_test";
   path_set(RARCH_PATH_BASENAME, "menu_game.rom");
   load_fixture("config_save_minimal = \"true\"\n"
         "video_openxr_menu_placed = \"true\"\n"
         "video_openxr_menu_xr_x = \"1\"\n");
   write_config(override_path,
         "video_openxr_menu_placed = \"false\"\n"
         "video_openxr_menu_xr_x = \"0\"\n"
         "video_openxr_menu_xr_pitch = \"-20\"\n");
   settings->floats.video_openxr_menu_xr_y = 2.0f;
   CHECK(config_load_override_file(override_path));
   CHECK(!settings->bools.video_openxr_menu_placed);
   CHECK(settings->floats.video_openxr_menu_xr_pitch == -20.0f);
   /* In neither file: the default. */
   CHECK(settings->floats.video_openxr_menu_xr_y == 0.0f);
   CHECK(config_save_overrides(OVERRIDE_GAME, &system_info, false, NULL) == 1);
   strlcpy(generated_path, path_get(RARCH_PATH_CONFIG_OVERRIDE), sizeof(generated_path));
   conf = config_file_new_from_path_to_string(generated_path);
   CHECK(conf != NULL);
   if (conf)
   {
      CHECK(config_get_bool(conf, "video_openxr_menu_placed", &placed));
      CHECK(!placed);
      CHECK(config_get_float(conf, "video_openxr_menu_xr_pitch", &value));
      CHECK(value == -20.0f);
      CHECK(config_get_entry(conf, "video_openxr_menu_xr_scale") == NULL);
      config_file_free(conf);
   }
   CHECK(config_unload_override());
   CHECK(settings->bools.video_openxr_menu_placed);
   CHECK(settings->floats.video_openxr_menu_xr_x == 1.0f);
   CHECK(settings->floats.video_openxr_menu_xr_pitch == 0.0f);
   path_clear(RARCH_PATH_BASENAME);

   config_reset_video_menu_xr(settings);
   CHECK(!settings->bools.video_openxr_menu_placed);
   CHECK(settings->floats.video_openxr_menu_xr_x == 0.0f);
   CHECK(settings->floats.video_openxr_menu_xr_scale == 1.0f);
}

/* A presented map fixture, as consumed by the shipping menu. */
static void set_menu_map(unsigned screens)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   unsigned i;
   memset(&video_st->views_core, 0, sizeof(video_st->views_core));
   for (i = 0; i < screens; i++)
   {
      struct retro_video_view *view = &video_st->views_core.views[i];
      view->screen = i;
      view->width = 100;
      view->height = 100;
   }
   video_st->views_core.num_views = screens;
   video_st->views_core.num_screens = screens;
   video_st->views_presented = screens != 0;
   video_st->views_frame_dims = VIDEO_SCALE_PACK(100, 100);
}

#ifdef HAVE_OPENXR
static bool menu_has_setting(const char *name)
{
   settings_t *settings = config_get_ptr();
   file_list_t *list = (file_list_t*)calloc(1, sizeof(*list));
   bool found = false;
   size_t i;
   CHECK(list != NULL);
   if (!list)
      return false;
   menu_displaylist_build_list(list, settings,
         DISPLAYLIST_VIDEO_HEADSET_SETTINGS_LIST, false);
   for (i = 0; i < list->size; i++)
      if (list->list[i].label && !strcmp(list->list[i].label, name))
         found = true;
   file_list_free(list);
   return found;
}

/* Whether the headset list shows name right after prev. */
static bool menu_setting_follows(const char *name, const char *prev)
{
   settings_t *settings = config_get_ptr();
   file_list_t *list = (file_list_t*)calloc(1, sizeof(*list));
   bool follows = false;
   size_t i;
   CHECK(list != NULL);
   if (!list)
      return false;
   menu_displaylist_build_list(list, settings,
         DISPLAYLIST_VIDEO_HEADSET_SETTINGS_LIST, false);
   for (i = 1; i < list->size; i++)
      if (     list->list[i].label && list->list[i - 1].label
            && !strcmp(list->list[i].label, name)
            && !strcmp(list->list[i - 1].label, prev))
         follows = true;
   file_list_free(list);
   return follows;
}

#endif

static void menu_action(const char *name, unsigned action)
{
   rarch_setting_t *setting = menu_setting_find(name);
   CHECK(setting != NULL);
   if (setting)
      CHECK(menu_action_handle_setting(setting, 0, action, false) == 0);
}

static void menu_value(const char *name, const char *value)
{
   rarch_setting_t *setting = menu_setting_find(name);
   char text[64];
   CHECK(setting != NULL);
   if (!setting)
      return;
   text[0] = '\0';
   CHECK(setting->actions->repr != NULL);
   if (setting->actions->repr)
      setting->actions->repr(setting, text, sizeof(text));
   CHECK(!strcmp(text, value));
}

static void test_xr_screen_menu(void)
{
   settings_t *settings = config_get_ptr();
   load_fixture("video_driver = \"glcore\"\n");
   set_menu_map(2);
#ifdef HAVE_OPENXR
   CHECK(menu_has_setting("video_screen_selected"));
#endif
   menu_action("video_screen_selected", MENU_ACTION_RIGHT);
   menu_value("video_screen_selected", "2");
   CHECK(menu_state_get_ptr()->video_screen_selected == 1u);
   settings->flags &= ~SETTINGS_FLG_MODIFIED;
   menu_action("video_screen_xr_x", MENU_ACTION_RIGHT);
   CHECK(settings->floats.video_screen_xr_x[0] == 0.0f);
   CHECK(settings->floats.video_screen_xr_x[1] == 0.1f);
   CHECK(settings->flags & SETTINGS_FLG_MODIFIED);
   menu_value("video_screen_xr_x", "0.1 m");
   settings->floats.video_screen_xr_y[1] = -1.5f;
   menu_value("video_screen_xr_y", "-1.5 m");
   menu_action("video_screen_xr_z", MENU_ACTION_LEFT);
   CHECK(settings->floats.video_screen_xr_z[1] == -0.1f);
   menu_value("video_screen_xr_z", "-0.1 m");
   menu_action("video_screen_xr_pitch", MENU_ACTION_LEFT);
   CHECK(settings->floats.video_screen_xr_pitch[1] == -1.0f);
   menu_value("video_screen_xr_pitch", "-1 deg");
   menu_action("video_screen_xr_roll", MENU_ACTION_RIGHT);
   CHECK(settings->floats.video_screen_xr_roll[1] == 1.0f);
   menu_value("video_screen_xr_roll", "1 deg");
   menu_action("video_screen_xr_yaw", MENU_ACTION_RIGHT);
   CHECK(settings->floats.video_screen_xr_yaw[1] == 1.0f);
   menu_value("video_screen_xr_yaw", "1 deg");
   menu_action("video_screen_xr_scale", MENU_ACTION_RIGHT);
   menu_value("video_screen_xr_scale", "1.1");
   menu_action("video_screen_xr_visible", MENU_ACTION_RIGHT);
   CHECK(!settings->bools.video_screen_xr_visible[1]);
   settings->bools.video_screen_xr_visible[0] = false;
#ifdef HAVE_OPENXR
   CHECK(menu_has_setting("video_screen_selected"));
   CHECK(menu_has_setting("video_screen_xr_reset"));
   CHECK(menu_has_setting("video_screen_xr_reset_all"));
#endif
   settings->floats.video_screen_xr_scale[7] = 3.0f;
   settings->flags &= ~SETTINGS_FLG_MODIFIED;
   menu_action("video_screen_xr_reset", MENU_ACTION_OK);
   CHECK(settings->flags & SETTINGS_FLG_MODIFIED);
   CHECK(settings->floats.video_screen_xr_x[0] == 0.0f);
   CHECK(settings->floats.video_screen_xr_x[1] == 0.0f);
   CHECK(menu_state_get_ptr()->video_screen_selected == 1u);
   menu_value("video_screen_selected", "2");
   CHECK(settings->floats.video_screen_xr_y[1] == 0.0f);
   CHECK(settings->floats.video_screen_xr_scale[1] == 1.0f);
   CHECK(settings->bools.video_screen_xr_visible[1]);
   CHECK(!settings->bools.video_screen_xr_visible[0]);
   CHECK(settings->floats.video_screen_xr_scale[7] == 3.0f);
   settings->flags &= ~SETTINGS_FLG_MODIFIED;
   menu_action("video_screen_xr_reset_all", MENU_ACTION_OK);
   CHECK(settings->flags & SETTINGS_FLG_MODIFIED);
   CHECK(settings->bools.video_screen_xr_visible[0]);
   CHECK(settings->floats.video_screen_xr_scale[7] == 1.0f);
   settings->bools.menu_navigation_wraparound_enable = false;
   settings->floats.video_screen_xr_x[1] = 10.0f;
   menu_action("video_screen_xr_x", MENU_ACTION_RIGHT);
   CHECK(settings->floats.video_screen_xr_x[1] == 10.0f);
   settings->floats.video_screen_xr_yaw[1] = -180.0f;
   menu_action("video_screen_xr_yaw", MENU_ACTION_LEFT);
   CHECK(settings->floats.video_screen_xr_yaw[1] == -180.0f);
   settings->floats.video_screen_xr_scale[1] = 0.1f;
   menu_action("video_screen_xr_scale", MENU_ACTION_LEFT);
   CHECK(settings->floats.video_screen_xr_scale[1] == 0.1f);
   menu_action("video_screen_xr_x", MENU_ACTION_START);
   CHECK(settings->floats.video_screen_xr_x[1] == 0.0f);
   CHECK(menu_state_get_ptr()->video_screen_selected == 1u);
}

static void test_xr_menu_map_change(void)
{
   settings_t *settings = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   rarch_setting_t *x;
   load_fixture("video_driver = \"glcore\"\n");
   set_menu_map(2);
   settings->floats.video_screen_xr_x[1] = 4.0f;
   video_st->views_core.views[0].screen = 1;
   video_st->views_core.views[1].screen = 0;
   menu_action("video_screen_xr_x", MENU_ACTION_RIGHT);
   CHECK(settings->floats.video_screen_xr_x[1] == 4.1f);
   CHECK(settings->floats.video_screen_xr_x[0] == 0.0f);
   x = menu_setting_find("video_screen_xr_x");
   CHECK(x != NULL);
   set_menu_map(1);
   if (x)
      CHECK(menu_action_handle_setting(x, 0, MENU_ACTION_RIGHT, false) == 0);
   menu_value("video_screen_selected", "1");
   CHECK(menu_state_get_ptr()->video_screen_selected == 0u);
   CHECK(settings->floats.video_screen_xr_x[0] == 0.1f);
   CHECK(settings->floats.video_screen_xr_x[1] == 4.1f);
   set_menu_map(0);
   {
      rarch_setting_t *setting = menu_setting_find("video_screen_xr_x");
      CHECK(setting != NULL);
      /* Without a map the whole frame is screen 1. */
      if (setting)
         CHECK(menu_action_handle_setting(setting, 0, MENU_ACTION_RIGHT, false) == 0);
      CHECK(menu_state_get_ptr()->video_screen_selected == 0u);
      CHECK(fabs(settings->floats.video_screen_xr_x[0] - 0.2f) < 1e-6);
      CHECK(settings->floats.video_screen_xr_x[1] == 4.1f);
   }
   menu_value("video_screen_selected", "1");
   menu_action("video_screen_selected", MENU_ACTION_RIGHT);
   menu_value("video_screen_selected", "1");
#ifdef HAVE_OPENXR
   CHECK(menu_has_setting("video_screen_selected"));
   CHECK(menu_has_setting("video_screen_xr_x"));
   CHECK(menu_has_setting("video_screen_xr_visible"));
   CHECK(menu_has_setting("video_screen_xr_reset"));
   CHECK(menu_has_setting("video_openxr_distance"));
   CHECK(menu_has_setting("video_screen_xr_reset_all"));
#endif
}

/* Reset All Screens puts the menu back in front of screen 1. */
static void test_xr_menu_reset_all(void)
{
   settings_t *settings = config_get_ptr();
   load_fixture("video_driver = \"glcore\"\n"
         "video_openxr_menu_placed = \"true\"\n"
         "video_openxr_menu_xr_x = \"2\"\n");
   set_menu_map(1);
   CHECK(settings->bools.video_openxr_menu_placed);
   menu_action("video_screen_xr_reset_all", MENU_ACTION_OK);
   CHECK(!settings->bools.video_openxr_menu_placed);
   CHECK(settings->floats.video_openxr_menu_xr_x == 0.0f);
}

/* Screen 2 moved, turned, scaled and hidden, as before each reset. */
static void xr_edit_screen2_moved(settings_t *settings)
{
   settings->floats.video_screen_xr_x[1]      = 1.5f;
   settings->floats.video_screen_xr_y[1]      = -0.5f;
   settings->floats.video_screen_xr_z[1]      = 2.0f;
   settings->floats.video_screen_xr_yaw[1]    = 30.0f;
   settings->floats.video_screen_xr_pitch[1]  = -10.0f;
   settings->floats.video_screen_xr_roll[1]   = 5.0f;
   settings->floats.video_screen_xr_scale[1]  = 2.5f;
   settings->bools.video_screen_xr_visible[1] = false;
   settings->flags                           &= ~SETTINGS_FLG_MODIFIED;
}

/* Edit mode's writes are the menu rows' writes: the same settings, the
 * same saved keys, clamped as a loaded value is; visibility stays the
 * menu's. Its reset is Reset Screen's, or the menu's own. */
static void test_xr_edit_writes(void)
{
   settings_t *settings = config_get_ptr();
   video_screen_xr_t values, got;
   config_file_t *conf;
   float row_x, row_yaw, row_scale;
   float value = 0.0f;

   load_fixture("video_driver = \"glcore\"\nconfig_save_minimal = \"true\"\n");
   set_menu_map(2);
   menu_state_get_ptr()->video_screen_selected = 1;
   menu_action("video_screen_xr_x", MENU_ACTION_RIGHT);
   menu_action("video_screen_xr_yaw", MENU_ACTION_LEFT);
   menu_action("video_screen_xr_scale", MENU_ACTION_RIGHT);
   row_x     = settings->floats.video_screen_xr_x[1];
   row_yaw   = settings->floats.video_screen_xr_yaw[1];
   row_scale = settings->floats.video_screen_xr_scale[1];

   load_fixture("video_driver = \"glcore\"\nconfig_save_minimal = \"true\"\n");
   settings->flags &= ~SETTINGS_FLG_MODIFIED;
   video_views_xr_reset(&values, 1);
   values.x       = row_x;
   values.yaw     = row_yaw;
   values.scale   = row_scale;
   values.visible = false;
   config_set_video_screen_xr(1, &values);
   CHECK(settings->flags & SETTINGS_FLG_MODIFIED);
   CHECK(settings->floats.video_screen_xr_x[1] == row_x);
   CHECK(settings->floats.video_screen_xr_yaw[1] == row_yaw);
   CHECK(settings->floats.video_screen_xr_scale[1] == row_scale);
   CHECK(settings->bools.video_screen_xr_visible[1]);
   CHECK(settings->floats.video_screen_xr_x[0] == 0.0f);
   config_get_video_screen_xr(1, &got);
   CHECK(got.x == row_x && got.yaw == row_yaw && got.scale == row_scale
         && got.visible);
   CHECK(config_save_file(saved_path));
   conf = config_file_new_from_path_to_string(saved_path);
   CHECK(conf != NULL);
   if (conf)
   {
      CHECK(config_get_float(conf, "video_screen2_xr_x", &value));
      CHECK(value == row_x);
      CHECK(config_get_float(conf, "video_screen2_xr_yaw", &value));
      CHECK(value == row_yaw);
      CHECK(config_get_float(conf, "video_screen2_xr_scale", &value));
      CHECK(value == row_scale);
      CHECK(config_get_entry(conf, "video_screen2_xr_visible") == NULL);
      config_file_free(conf);
   }

   values.x     = 12.0f;
   values.scale = 0.0f;
   config_set_video_screen_xr(1, &values);
   CHECK(settings->floats.video_screen_xr_x[1] == 10.0f);
   CHECK(settings->floats.video_screen_xr_scale[1] == 0.1f);

   /* Reset: what Reset Screen leaves, hidden screens shown again. */
   set_menu_map(2);
   menu_state_get_ptr()->video_screen_selected = 1;
   xr_edit_screen2_moved(settings);
   menu_action("video_screen_xr_reset", MENU_ACTION_OK);
   config_get_video_screen_xr(1, &values);
   xr_edit_screen2_moved(settings);
   config_set_video_screen_xr(1, NULL);
   CHECK(settings->flags & SETTINGS_FLG_MODIFIED);
   config_get_video_screen_xr(1, &got);
   CHECK(got.x == values.x && got.y == values.y && got.z == values.z
         && got.yaw == values.yaw && got.pitch == values.pitch
         && got.roll == values.roll && got.scale == values.scale
         && got.visible == values.visible);
   CHECK(got.visible && got.scale == 1.0f && got.z == 0.0f);

   video_views_xr_reset(&values, 1);
   values.x     = 0.5f;
   values.scale = 0.75f;
   settings->flags &= ~SETTINGS_FLG_MODIFIED;
   CHECK(!config_get_video_menu_xr(&got));
   config_set_video_menu_xr(&values);
   CHECK(settings->flags & SETTINGS_FLG_MODIFIED);
   CHECK(settings->bools.video_openxr_menu_placed);
   CHECK(settings->floats.video_openxr_menu_xr_x == 0.5f);
   CHECK(settings->floats.video_openxr_menu_xr_scale == 0.75f);
   CHECK(config_get_video_menu_xr(&got) && got.x == 0.5f
         && got.scale == 0.75f);
   settings->flags &= ~SETTINGS_FLG_MODIFIED;
   config_set_video_menu_xr(NULL);
   CHECK(settings->flags & SETTINGS_FLG_MODIFIED);
   CHECK(!settings->bools.video_openxr_menu_placed);
   CHECK(settings->floats.video_openxr_menu_xr_x == 0.0f);
   CHECK(settings->floats.video_openxr_menu_xr_scale == 1.0f);
   CHECK(!config_get_video_menu_xr(&got));
}

#ifdef HAVE_OPENXR
static unsigned menu_test_headset_state(void *user)
{
   return *(unsigned*)user;
}

static unsigned menu_test_headset_transition(void *user)
{
   unsigned *state = (unsigned*)user;
   unsigned current = *state;
   *state ^= VIDEO_XR_HEADSET_TRACKING;
   return current;
}

static bool menu_test_headset_space(void *user,
      enum retro_vr_reference_space *space, unsigned *width, unsigned *height)
{
   *width = 100;
   *height = 100;
   return true;
}

static bool menu_test_headset_content(void *data,
      const video_vr_content_info_t *info)
{
   return true;
}

static void test_xr_menu_tracked_screen(void)
{
   settings_t *settings = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   video_driver_t *saved_driver = video_st->current_video;
   static video_driver_t driver;
   video_xr_host_t host;
   struct retro_video_views views;
   unsigned state = VIDEO_XR_HEADSET_TRACKING;
   memset(&driver, 0, sizeof(driver));
   driver.set_vr_content_info = menu_test_headset_content;
   memset(&host, 0, sizeof(host));
   host.state = menu_test_headset_state;
   host.set_space = menu_test_headset_space;
   host.user = &state;
   video_st->current_video = &driver;
   video_driver_set_xr_host(&host);
   load_fixture("video_driver = \"glcore\"\n");
   set_menu_map(3);
   video_st->views_core.views[0].eye = RETRO_VIDEO_VIEW_EYE_LEFT;
   video_st->views_core.views[1].eye = RETRO_VIDEO_VIEW_EYE_RIGHT;
   video_st->views_core.views[1].screen = 0;
   video_st->views_core.views[2].screen = 1;
   video_st->views_core.num_screens = 2;
   memset(&views, 0, sizeof(views));
   views.views = video_st->views_core.views;
   views.num_views = 3;
   views.flags = RETRO_VIDEO_VIEWS_FLAG_TRACKED;
   CHECK(video_driver_set_views(&views));
   CHECK(video_driver_vr_content_active());
   menu_state_get_ptr()->video_screen_selected = 0;
   settings->floats.video_screen_xr_x[0] = 2.0f;
   menu_value("video_screen_selected", "2");
   menu_action("video_screen_selected", MENU_ACTION_RIGHT);
   menu_value("video_screen_selected", "2");
   menu_action("video_screen_xr_x", MENU_ACTION_RIGHT);
   CHECK(settings->floats.video_screen_xr_x[0] == 2.0f);
   CHECK(settings->floats.video_screen_xr_x[1] == 0.1f);
   CHECK(menu_has_setting("video_screen_xr_x"));
   video_st->views_core.num_views = 2;
   video_st->views_core.num_screens = 1;
   CHECK(!menu_has_setting("video_screen_selected"));
   CHECK(!menu_has_setting("video_screen_xr_x"));
   CHECK(!menu_has_setting("video_screen_xr_reset"));
   CHECK(menu_has_setting("video_openxr_distance"));
   CHECK(menu_has_setting("video_screen_xr_reset_all"));
   CHECK(settings->floats.video_screen_xr_x[0] == 2.0f);
   CHECK(settings->floats.video_screen_xr_x[1] == 0.1f);
   state = 0;
   CHECK(menu_has_setting("video_screen_selected"));
   menu_value("video_screen_selected", "1");
   {
      rarch_setting_t *selector = menu_setting_find("video_screen_selected");
      unsigned i;
      CHECK(selector != NULL);
      host.state = menu_test_headset_transition;
      video_driver_set_xr_host(&host);
      if (selector)
         for (i = 0; i < 4; i++)
         {
            state = 0;
            CHECK(menu_action_handle_setting(selector, 0,
                     MENU_ACTION_RIGHT, false) == 0);
            CHECK(menu_state_get_ptr()->video_screen_selected == 0u);
            state = VIDEO_XR_HEADSET_TRACKING;
            CHECK(menu_action_handle_setting(selector, 0,
                     MENU_ACTION_LEFT, false) == -1);
            CHECK(menu_state_get_ptr()->video_screen_selected == 0u);
         }
      CHECK(settings->floats.video_screen_xr_x[0] == 2.0f);
      CHECK(settings->floats.video_screen_xr_x[1] == 0.1f);
   }
   video_driver_vr_content_clear();
   video_driver_set_xr_host(NULL);
   video_st->current_video = saved_driver;
}

/* Edit Screens, While Editing Screens and Snap to Screens show with
 * Headset Output on; the hotkey exists; without a session the row is
 * refused and the menu stays. */
static void test_xr_edit_rows(void)
{
   settings_t *settings = config_get_ptr();
   const char *base     = NULL;
   unsigned i;
   load_fixture("video_driver = \"glcore\"\nvideo_openxr_enable = \"false\"\n");
   set_menu_map(1);
   CHECK(!menu_has_setting("video_screen_xr_edit"));
   CHECK(!menu_has_setting("video_openxr_edit_pause"));
   CHECK(!menu_has_setting("video_openxr_edit_snap"));
   CHECK(!menu_has_setting("video_openxr_edit_snap_angles"));
   CHECK(!menu_has_setting("video_openxr_edit_hinge_gap"));
   CHECK(!menu_has_setting("video_openxr_edit_smoothing"));
   settings->bools.video_openxr_enable = true;
   CHECK(menu_has_setting("video_screen_xr_edit"));
   CHECK(menu_has_setting("video_openxr_edit_pause"));
   menu_value("video_openxr_edit_pause", "Keep Running");
   menu_action("video_openxr_edit_pause", MENU_ACTION_RIGHT);
   CHECK(settings->bools.video_openxr_edit_pause);
   menu_value("video_openxr_edit_pause", "Pause");
   CHECK(menu_has_setting("video_openxr_edit_snap"));
   CHECK(settings->bools.video_openxr_edit_snap);
   menu_value("video_openxr_edit_snap", "ON");
   menu_action("video_openxr_edit_snap", MENU_ACTION_RIGHT);
   CHECK(!settings->bools.video_openxr_edit_snap);
   menu_value("video_openxr_edit_snap", "OFF");
   CHECK(menu_setting_follows("video_openxr_edit_snap_angles",
         "video_openxr_edit_snap"));
   CHECK(settings->uints.video_openxr_edit_snap_angles
         == VIDEO_OPENXR_EDIT_SNAP_ANGLES_15);
   menu_value("video_openxr_edit_snap_angles", "15°");
   menu_action("video_openxr_edit_snap_angles", MENU_ACTION_RIGHT);
   CHECK(settings->uints.video_openxr_edit_snap_angles
         == VIDEO_OPENXR_EDIT_SNAP_ANGLES_30);
   menu_value("video_openxr_edit_snap_angles", "30°");
   CHECK(menu_setting_follows("video_openxr_edit_hinge_gap",
         "video_openxr_edit_snap_angles"));
   CHECK(settings->uints.video_openxr_edit_hinge_gap
         == VIDEO_OPENXR_EDIT_HINGE_GAP_2);
   menu_value("video_openxr_edit_hinge_gap", "2 cm");
   menu_action("video_openxr_edit_hinge_gap", MENU_ACTION_RIGHT);
   CHECK(settings->uints.video_openxr_edit_hinge_gap
         == VIDEO_OPENXR_EDIT_HINGE_GAP_3);
   menu_value("video_openxr_edit_hinge_gap", "3 cm");
   CHECK(menu_setting_follows("video_screen_xr_edit",
         "video_openxr_enable"));
   CHECK(menu_setting_follows("video_openxr_edit_snap",
         "video_screen_xr_edit"));
   CHECK(menu_setting_follows("video_openxr_edit_smoothing",
         "video_openxr_edit_hinge_gap"));
   CHECK(menu_setting_follows("video_openxr_edit_pause",
         "video_openxr_edit_smoothing"));
   CHECK(menu_setting_follows("video_openxr_refresh_rate",
         "video_openxr_edit_pause"));
   CHECK(menu_setting_follows("video_openxr_placement",
         "video_openxr_perf_boost"));
   CHECK(menu_setting_follows("video_openxr_width",
         "video_openxr_distance"));
   CHECK(menu_setting_follows("video_openxr_menu_follow",
         "video_openxr_width"));
   CHECK(menu_setting_follows("video_screen_selected",
         "video_openxr_menu_follow"));
   /* Off unless asked for: the menu opens where it always did. */
   CHECK(!settings->bools.video_openxr_menu_follow);
   menu_value("video_openxr_menu_follow", "OFF");
   menu_action("video_openxr_menu_follow", MENU_ACTION_RIGHT);
   CHECK(settings->bools.video_openxr_menu_follow);
   menu_value("video_openxr_menu_follow", "ON");
   menu_action("video_openxr_menu_follow", MENU_ACTION_LEFT);
   CHECK(!settings->bools.video_openxr_menu_follow);
   CHECK(menu_setting_follows("video_screen_xr_visible",
         "video_screen_selected"));
   CHECK(menu_setting_follows("video_screen_xr_x",
         "video_screen_xr_visible"));
   CHECK(menu_has_setting("video_openxr_edit_smoothing"));
   CHECK(settings->uints.video_openxr_edit_smoothing
         == VIDEO_OPENXR_EDIT_SMOOTHING_MEDIUM);
   menu_value("video_openxr_edit_smoothing", "Medium");
   menu_action("video_openxr_edit_smoothing", MENU_ACTION_RIGHT);
   CHECK(settings->uints.video_openxr_edit_smoothing
         == VIDEO_OPENXR_EDIT_SMOOTHING_HIGH);
   menu_value("video_openxr_edit_smoothing", "High");
   for (i = 0; input_config_bind_map_get_valid(i); i++)
      if (input_config_bind_map_get_retro_key(i) == RARCH_HEADSET_EDIT_SCREENS)
         base = input_config_bind_map_get_base(i);
   CHECK(base && !strcmp(base, "headset_edit_screens"));
   /* The row asks for edit mode, which the desktop is told it cannot
    * have; it is no Reset Screen. */
   runloop_msg_queue_init();
   settings->floats.video_screen_xr_x[0] = 2.0f;
   menu_action("video_screen_xr_edit", MENU_ACTION_OK);
   CHECK(!video_state_get_ptr()->headset_editing);
   {
      const char *msg = msg_queue_pull(&runloop_state_get_ptr()->msg_queue);
      CHECK(msg && !strcmp(msg,
               msg_hash_to_str(MSG_OPENXR_EDIT_UNAVAILABLE)));
   }
   CHECK(settings->floats.video_screen_xr_x[0] == 2.0f);
   runloop_msg_queue_deinit();
   settings->bools.video_openxr_enable = false;
}

/* Laser Beam is on Settings > Input > Controllers, not with the
 * headset's rows: Edit Mode Only unless the config says otherwise; the
 * menu steps through its values. */
static void test_xr_laser_beam_row(void)
{
   settings_t *settings = config_get_ptr();
   load_fixture("video_driver = \"glcore\"\n");
   CHECK(!menu_has_setting("video_openxr_laser_beam"));
   CHECK(settings->uints.video_openxr_laser_beam
         == VIDEO_OPENXR_LASER_BEAM_EDIT);
   menu_value("video_openxr_laser_beam", "Edit Mode Only");
   menu_action("video_openxr_laser_beam", MENU_ACTION_RIGHT);
   CHECK(settings->uints.video_openxr_laser_beam
         == VIDEO_OPENXR_LASER_BEAM_ALWAYS);
   menu_value("video_openxr_laser_beam", "Always");
   load_fixture("video_driver = \"glcore\"\n"
         "video_openxr_laser_beam = \"0\"\n");
   CHECK(settings->uints.video_openxr_laser_beam
         == VIDEO_OPENXR_LASER_BEAM_OFF);
   menu_value("video_openxr_laser_beam", "Off");
}

static void test_xr_edit_smoothing_row(void)
{
   settings_t *settings = config_get_ptr();
   load_fixture("video_driver = \"glcore\"\n"
         "video_openxr_enable = \"true\"\n"
         "video_openxr_edit_smoothing = \"0\"\n");
   CHECK(settings->uints.video_openxr_edit_smoothing
         == VIDEO_OPENXR_EDIT_SMOOTHING_OFF);
   menu_value("video_openxr_edit_smoothing", "Off");
   load_fixture("video_driver = \"glcore\"\n"
         "video_openxr_enable = \"true\"\n"
         "video_openxr_edit_smoothing = \"1\"\n");
   menu_value("video_openxr_edit_smoothing", "Low");
}

static void test_xr_edit_snap_angles_row(void)
{
   settings_t *settings = config_get_ptr();
   load_fixture("video_driver = \"glcore\"\n"
         "video_openxr_enable = \"true\"\n"
         "video_openxr_edit_snap_angles = \"0\"\n");
   CHECK(settings->uints.video_openxr_edit_snap_angles
         == VIDEO_OPENXR_EDIT_SNAP_ANGLES_OFF);
   menu_value("video_openxr_edit_snap_angles", "Off");
   load_fixture("video_driver = \"glcore\"\n"
         "video_openxr_enable = \"true\"\n"
         "video_openxr_edit_snap_angles = \"3\"\n");
   menu_value("video_openxr_edit_snap_angles", "45°");
   load_fixture("video_driver = \"glcore\"\n"
         "video_openxr_enable = \"true\"\n"
         "video_openxr_edit_snap_angles = \"4\"\n");
   menu_value("video_openxr_edit_snap_angles", "90°");
}

/* Each Hinge Gap shows its own value, and survives a save and load. */
static void test_xr_edit_hinge_gap_row(void)
{
   static const char *const shown[] = { "0 cm", "1 cm", "2 cm", "3 cm",
      "5 cm", "10 cm" };
   char fixture[128];
   settings_t *settings = config_get_ptr();
   unsigned i;
   for (i = 0; i < 6; i++)
   {
      snprintf(fixture, sizeof(fixture), "video_driver = \"glcore\"\n"
            "video_openxr_enable = \"true\"\n"
            "video_openxr_edit_hinge_gap = \"%u\"\n", i);
      load_fixture(fixture);
      CHECK(settings->uints.video_openxr_edit_hinge_gap == i);
      menu_value("video_openxr_edit_hinge_gap", shown[i]);
      settings->bools.config_save_minimal = false;
      CHECK(config_save_file(saved_path));
      path_set(RARCH_PATH_CONFIG, saved_path);
      config_load();
      CHECK(settings->uints.video_openxr_edit_hinge_gap == i);
   }
}
#endif

#ifdef VIDEO_SCREENS_RUNTIME_TEST
int rarch_main(int argc, char *argv[], void *data);

int video_screens_test_iterate(void)
{
   static unsigned applied;
   const char *path = getenv("RA_SCREEN_TEST_SCRIPT");
   int result = runloop_iterate();
   RFILE *file = path ? filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE) : NULL;
   if (file)
   {
      char text[128];
      unsigned serial, screen, visible, i;
      float x, scale;
      if (filestream_gets(file, text, sizeof(text))
            && sscanf(text, "%u %u %f %f %u",
               &serial, &screen, &x, &scale, &visible) == 5
            && serial > applied && screen <= RETRO_VIDEO_VIEWS_MAX)
      {
         settings_t *settings = config_get_ptr();
         const char *ack = getenv("RA_SCREEN_TEST_ACK");
         char value[32];
         for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
            if (screen == i || screen == RETRO_VIDEO_VIEWS_MAX)
            {
               settings->floats.video_screen_xr_x[i] = x;
               settings->floats.video_screen_xr_scale[i] = scale;
               settings->bools.video_screen_xr_visible[i] = visible != 0;
            }
         applied = serial;
         snprintf(value, sizeof(value), "%u %u", applied,
               (unsigned)video_driver_get_frame_count());
         if (ack)
            filestream_write_file(ack, value, strlen(value));
      }
      filestream_close(file);
   }
   return result;
}
#endif

/* A driver that draws views itself, and the screens' own windows while
 * window_test_flags says so: frame() keeps the windows it was handed. */
static uint32_t window_test_flags;
static unsigned window_test_frames;
static const video_windows_frame_t *window_test_handed;
static video_windows_frame_t window_test_seen;
static video_driver_t window_test_driver;
static video_poke_interface_t window_test_poke;
static video_driver_t *window_test_saved_video;
static const video_poke_interface_t *window_test_saved_poke;
static void *window_test_saved_data;
static uint32_t window_test_saved_flags;
static unsigned window_test_saved_dims;
static uint32_t window_test_saved_runloop;
static enum rarch_core_type window_test_saved_core;

static bool window_test_frame(void *data, const void *frame, unsigned dims,
      uint64_t count, unsigned pitch, const char *msg,
      video_frame_info_t *info)
{
   (void)data;
   (void)frame;
   (void)dims;
   (void)count;
   (void)pitch;
   (void)msg;
   window_test_frames++;
   window_test_handed = info->windows;
   if (info->windows)
      window_test_seen = *info->windows;
   return true;
}

static void window_test_viewport(void *data, struct video_viewport *vp)
{
   (void)data;
   vp->pos       = VIDEO_POS_PACK(0, 0);
   vp->dims      = VIDEO_SCALE_PACK(800, 480);
   vp->full_dims = VIDEO_SCALE_PACK(800, 480);
}

static uint32_t window_test_get_flags(void *data)
{
   (void)data;
   return window_test_flags;
}

/* Two 4x2 screens side by side in an 8x2 frame */
static void window_test_views(void)
{
   static struct retro_video_view views[2];
   struct retro_video_views map;
   memset(views, 0, sizeof(views));
   views[0].width  = views[1].width  = 4;
   views[0].height = views[1].height = 2;
   views[1].x      = 4;
   views[1].screen = 1;
   memset(&map, 0, sizeof(map));
   map.views     = views;
   map.num_views = 2;
   CHECK(video_driver_set_views(&map));
}

/* window_test_views() drawn by the test driver into an 800x480
 * window, the second screen Separate */
static void window_test_begin(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   memset(&window_test_driver, 0, sizeof(window_test_driver));
   window_test_driver.frame         = window_test_frame;
   window_test_driver.viewport_info = window_test_viewport;
   window_test_driver.ident         = "window_test";
   memset(&window_test_poke, 0, sizeof(window_test_poke));
   window_test_poke.get_flags       = window_test_get_flags;
   window_test_saved_video  = video_st->current_video;
   window_test_saved_poke   = video_st->poke;
   window_test_saved_data   = video_st->data;
   window_test_saved_flags  = video_st->main_flags;
   window_test_saved_dims   = video_driver_get_output_dims();
   /* As before a driver's init: the title mailbox the frames use */
   video_driver_lock_new();
   video_st->current_video  = &window_test_driver;
   video_st->poke           = &window_test_poke;
   video_st->data           = &window_test_driver;
   video_st->main_flags    |= VIDEO_FLAG_ACTIVE;
   video_driver_set_output_dims(VIDEO_SCALE_PACK(800, 480));
   /* A core runs, and placement decides afresh */
   window_test_saved_runloop = runloop_state_get_ptr()->flags;
   window_test_saved_core    = runloop_state_get_ptr()->current_core_type;
   runloop_state_get_ptr()->flags            |= RUNLOOP_FLAG_CORE_RUNNING;
   runloop_state_get_ptr()->current_core_type = CORE_TYPE_PLAIN;
   video_windows_host_reset(&video_st->windows_host);
   settings->bools.video_windows_enable          = true;
   settings->arrays.video_screen_display[1][0]   = '\0';
   window_test_flags = (1u << GFX_CTX_FLAGS_VIDEO_VIEWS)
                     | (1u << GFX_CTX_FLAGS_VIDEO_WINDOWS);
   window_test_views();
}

static void window_test_run(void)
{
   static uint32_t pixels[8 * 2];
   window_test_frames = 0;
   window_test_handed = NULL;
   video_driver_frame(pixels, 8, 2, 8 * sizeof(pixels[0]));
   CHECK(window_test_frames == 1);
}

static void window_test_end(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   video_driver_clear_views();
   video_st->current_video = window_test_saved_video;
   video_st->poke          = window_test_saved_poke;
   video_st->data          = window_test_saved_data;
   video_st->main_flags    = window_test_saved_flags;
   video_driver_set_output_dims(window_test_saved_dims);
   config_get_ptr()->bools.video_windows_enable = false;
   runloop_state_get_ptr()->flags             = window_test_saved_runloop;
   runloop_state_get_ptr()->current_core_type = window_test_saved_core;
}

static unsigned window_test_placements(void)
{
   const video_views_layout_t *layout = video_driver_get_views_layout();
   return layout ? layout->num_placements : 0;
}

/* video_screenN_display: saved, overridden, and Auto when absent */
static void test_window_display_config(void)
{
   static rarch_system_info_t system_info;
   settings_t *settings = config_get_ptr();
   config_file_t *conf;
   char value[64];
   unsigned i;
   memset(&system_info, 0, sizeof(system_info));
   system_info.info.library_name = "video_screens_test";
   load_fixture("");
   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
      CHECK(!settings->arrays.video_screen_display[i][0]);

   load_fixture("video_screen1_window_mode = \"1\"\n"
         "video_screen1_display = \"DP Screen\"\n"
         "video_screen2_display = \"Screen-2 2\"\n");
   CHECK(string_is_equal(settings->arrays.video_screen_display[0], "DP Screen"));
   CHECK(string_is_equal(settings->arrays.video_screen_display[1], "Screen-2 2"));
   CHECK(!settings->arrays.video_screen_display[2][0]);

   /* A minimal save keeps only names that are set */
   settings->bools.config_save_minimal = true;
   CHECK(config_save_file(saved_path));
   conf = config_file_new_from_path_to_string(saved_path);
   CHECK(conf != NULL);
   if (conf)
   {
      CHECK(config_get_array(conf, "video_screen1_display", value, sizeof(value))
            && string_is_equal(value, "DP Screen"));
      CHECK(config_get_array(conf, "video_screen2_display", value, sizeof(value))
            && string_is_equal(value, "Screen-2 2"));
      CHECK(config_get_entry(conf, "video_screen3_display") == NULL);
      config_file_free(conf);
   }
   settings->bools.config_save_minimal = false;
   CHECK(config_save_file(saved_path));
   conf = config_file_new_from_path_to_string(saved_path);
   CHECK(conf != NULL);
   if (conf)
   {
      CHECK(config_get_entry(conf, "video_screen8_display") != NULL);
      config_file_free(conf);
   }

   /* An override sets and clears names; only the changed ones are
    * written */
   write_config(override_path, "video_screen1_display = \"\"\n"
         "video_screen3_display = \"Screen-2\"\n");
   CHECK(config_load_override_file(override_path));
   CHECK(!settings->arrays.video_screen_display[0][0]);
   CHECK(string_is_equal(settings->arrays.video_screen_display[1], "Screen-2 2"));
   CHECK(string_is_equal(settings->arrays.video_screen_display[2], "Screen-2"));
   path_set(RARCH_PATH_BASENAME, "window_names.rom");
   CHECK(config_save_overrides(OVERRIDE_GAME, &system_info, false, NULL) == 1);
   strlcpy(generated_path, path_get(RARCH_PATH_CONFIG_OVERRIDE), sizeof(generated_path));
   conf = config_file_new_from_path_to_string(generated_path);
   CHECK(conf != NULL);
   if (conf)
   {
      CHECK(config_get_array(conf, "video_screen1_display", value, sizeof(value))
            && !value[0]);
      CHECK(config_get_array(conf, "video_screen3_display", value, sizeof(value))
            && string_is_equal(value, "Screen-2"));
      CHECK(config_get_entry(conf, "video_screen2_display") == NULL);
      config_file_free(conf);
   }
   CHECK(config_unload_override());
   CHECK(string_is_equal(settings->arrays.video_screen_display[0], "DP Screen"));
   CHECK(!settings->arrays.video_screen_display[2][0]);
   path_clear(RARCH_PATH_BASENAME);

   /* A load starts from Auto */
   load_fixture("");
   CHECK(!settings->arrays.video_screen_display[0][0]);
}

/* video_screenN_window_mode, saved before a display could be Off: read
 * on load, never written again */
static void test_window_migration(void)
{
   static rarch_system_info_t system_info;
   settings_t *settings = config_get_ptr();
   config_file_t *conf;
   char value[64];
   unsigned minimal;
   memset(&system_info, 0, sizeof(system_info));
   system_info.info.library_name = "video_screens_test";

   /* migration: old Hidden becomes Off */
   load_fixture("video_screen1_window_mode = \"2\"\n"
         "video_screen3_window_mode = \"2\"\n"
         "video_screen3_display = \"\"\n");
   CHECK(string_is_equal(settings->arrays.video_screen_display[0],
            VIDEO_WINDOW_DISPLAY_OFF));
   CHECK(!settings->arrays.video_screen_display[1][0]);
   CHECK(string_is_equal(settings->arrays.video_screen_display[2],
            VIDEO_WINDOW_DISPLAY_OFF));

   /* migration: old With RetroArch becomes Main Window past Screen 1;
    * Screen 1's, Separate and no mode stay Auto */
   load_fixture("video_screen1_window_mode = \"0\"\n"
         "video_screen2_window_mode = \"0\"\n"
         "video_screen3_window_mode = \"1\"\n"
         "video_screen4_window_mode = \"2\"\n");
   CHECK(!settings->arrays.video_screen_display[0][0]);
   CHECK(string_is_equal(settings->arrays.video_screen_display[1],
            VIDEO_WINDOW_DISPLAY_MAIN));
   CHECK(!settings->arrays.video_screen_display[2][0]);
   CHECK(string_is_equal(settings->arrays.video_screen_display[3],
            VIDEO_WINDOW_DISPLAY_OFF));
   CHECK(!settings->arrays.video_screen_display[4][0]);
   load_fixture("video_screen2_window_mode = \"0\"\n"
         "video_screen2_display = \"DP-1\"\n");
   CHECK(string_is_equal(settings->arrays.video_screen_display[1], "DP-1"));

   /* migration: a display key wins over the old mode */
   load_fixture("video_screen1_window_mode = \"2\"\n"
         "video_screen1_display = \"Screen-2\"\n"
         "video_screen2_window_mode = \"0\"\n"
         "video_screen2_display = \"off\"\n");
   CHECK(string_is_equal(settings->arrays.video_screen_display[0],
            "Screen-2"));
   CHECK(string_is_equal(settings->arrays.video_screen_display[1],
            VIDEO_WINDOW_DISPLAY_OFF));

   /* An old override's Hidden over a base saved since */
   load_fixture("video_screen2_display = \"\"\n");
   write_config(override_path, "video_screen2_window_mode = \"2\"\n");
   CHECK(config_load_override_file(override_path));
   CHECK(string_is_equal(settings->arrays.video_screen_display[1],
            VIDEO_WINDOW_DISPLAY_OFF));
   path_set(RARCH_PATH_BASENAME, "window_migration.rom");
   CHECK(config_save_overrides(OVERRIDE_GAME, &system_info, false, NULL) == 1);
   strlcpy(generated_path, path_get(RARCH_PATH_CONFIG_OVERRIDE),
         sizeof(generated_path));
   conf = config_file_new_from_path_to_string(generated_path);
   CHECK(conf != NULL);
   if (conf)
   {
      CHECK(config_get_array(conf, "video_screen2_display", value, sizeof(value))
            && string_is_equal(value, VIDEO_WINDOW_DISPLAY_OFF));
      CHECK(config_get_entry(conf, "video_screen2_window_mode") == NULL);
      config_file_free(conf);
   }
   CHECK(config_unload_override());
   CHECK(!settings->arrays.video_screen_display[1][0]);
   path_clear(RARCH_PATH_BASENAME);

   /* migration: the old mode key is not written */
   for (minimal = 0; minimal < 2; minimal++)
   {
      write_config(saved_path, "video_screen2_window_mode = \"2\"\n");
      load_fixture("video_screen2_window_mode = \"2\"\n");
      settings->bools.config_save_minimal = minimal != 0;
      CHECK(config_save_file(saved_path));
      conf = config_file_new_from_path_to_string(saved_path);
      CHECK(conf != NULL);
      if (conf)
      {
         CHECK(config_get_entry(conf, "video_screen2_window_mode") == NULL);
         CHECK(config_get_array(conf, "video_screen2_display", value,
                  sizeof(value))
               && string_is_equal(value, VIDEO_WINDOW_DISPLAY_OFF));
         config_file_free(conf);
      }
   }
   settings->bools.config_save_minimal = false;
   load_fixture("");
}

/* The menu's keys: defaults, saved, overridden, and kept in range */
static void test_window_menu_config(void)
{
   static rarch_system_info_t system_info;
   settings_t *settings = config_get_ptr();
   config_file_t *conf;
   char value[64];
   int number;
   memset(&system_info, 0, sizeof(system_info));
   system_info.info.library_name = "video_screens_test";
   CHECK(sizeof(settings->arrays.video_menu_display)
         == VIDEO_WINDOW_DISPLAY_NAME_SIZE);
   load_fixture("");
   CHECK(settings->uints.video_menu_follows == 0);
   CHECK(settings->uints.video_notifications_window
         == VIDEO_NOTIFICATIONS_WITH_GAME);
   CHECK(!settings->arrays.video_menu_display[0]);
   settings->bools.config_save_minimal = true;
   CHECK(config_save_file(saved_path));
   conf = config_file_new_from_path_to_string(saved_path);
   CHECK(conf != NULL);
   if (conf)
   {
      CHECK(config_get_entry(conf, "video_menu_follows") == NULL);
      CHECK(config_get_entry(conf, "video_menu_display") == NULL);
      CHECK(config_get_entry(conf, "video_notifications_window") == NULL);
      config_file_free(conf);
   }

   load_fixture("video_menu_follows = \"8\"\n"
         "video_menu_display = \"Screen-2\"\n"
         "video_notifications_window = \"1\"\n");
   CHECK(settings->uints.video_menu_follows == VIDEO_MENU_FOLLOWS_SEPARATE);
   CHECK(string_is_equal(settings->arrays.video_menu_display, "Screen-2"));
   CHECK(settings->uints.video_notifications_window
         == VIDEO_NOTIFICATIONS_WITH_MENU);
   CHECK(config_save_file(saved_path));
   conf = config_file_new_from_path_to_string(saved_path);
   CHECK(conf != NULL);
   if (conf)
   {
      CHECK(config_get_int(conf, "video_menu_follows", &number)
            && number == VIDEO_MENU_FOLLOWS_SEPARATE);
      CHECK(config_get_array(conf, "video_menu_display", value, sizeof(value))
            && string_is_equal(value, "Screen-2"));
      CHECK(config_get_int(conf, "video_notifications_window", &number)
            && number == VIDEO_NOTIFICATIONS_WITH_MENU);
      config_file_free(conf);
   }
   settings->bools.config_save_minimal = false;

   /* An override changes two; only those are written */
   write_config(override_path, "video_menu_follows = \"1\"\n"
         "video_menu_display = \"\"\n");
   CHECK(config_load_override_file(override_path));
   CHECK(settings->uints.video_menu_follows == 1);
   CHECK(!settings->arrays.video_menu_display[0]);
   CHECK(settings->uints.video_notifications_window
         == VIDEO_NOTIFICATIONS_WITH_MENU);
   path_set(RARCH_PATH_BASENAME, "window_menu.rom");
   CHECK(config_save_overrides(OVERRIDE_GAME, &system_info, false, NULL) == 1);
   strlcpy(generated_path, path_get(RARCH_PATH_CONFIG_OVERRIDE),
         sizeof(generated_path));
   conf = config_file_new_from_path_to_string(generated_path);
   CHECK(conf != NULL);
   if (conf)
   {
      CHECK(config_get_int(conf, "video_menu_follows", &number)
            && number == 1);
      CHECK(config_get_array(conf, "video_menu_display", value, sizeof(value))
            && !value[0]);
      CHECK(config_get_entry(conf, "video_notifications_window") == NULL);
      config_file_free(conf);
   }
   CHECK(config_unload_override());
   CHECK(settings->uints.video_menu_follows == VIDEO_MENU_FOLLOWS_SEPARATE);
   CHECK(string_is_equal(settings->arrays.video_menu_display, "Screen-2"));
   path_clear(RARCH_PATH_BASENAME);

   /* Out of range: the defaults */
   load_fixture("video_menu_follows = \"9\"\n"
         "video_notifications_window = \"2\"\n");
   CHECK(settings->uints.video_menu_follows == 0);
   CHECK(settings->uints.video_notifications_window
         == VIDEO_NOTIFICATIONS_WITH_GAME);
   load_fixture("");
}

/* A screen's own window, laid out from a frame's snapshot */
static void test_window_layout(void)
{
   video_windows_frame_t frame;
   video_views_layout_t layout;
   unsigned i;
   memset(&frame, 0, sizeof(frame));
   /* Three 100x100 screens stacked, screen 2 a stereo pair with its
    * right eye first */
   for (i = 0; i < 3; i++)
   {
      frame.core.views[i].screen = i;
      frame.core.views[i].y      = i * 100;
      frame.core.views[i].width  = frame.core.views[i].height = 100;
   }
   frame.core.views[3]     = frame.core.views[1];
   frame.core.views[3].eye = RETRO_VIDEO_VIEW_EYE_LEFT;
   frame.core.views[3].x   = 100;
   frame.core.views[1].eye = RETRO_VIDEO_VIEW_EYE_RIGHT;
   frame.core.num_views    = 4;
   frame.core.num_screens  = 3;
   frame.core_dims         = VIDEO_SCALE_PACK(200, 300);
   video_driver_window_layout(&frame, 2, VIDEO_SCALE_PACK(300, 200), &layout);
   CHECK(layout.num_placements == 1 && layout.placements[0].view == 2);
   CHECK(layout.placements[0].dst.dims == VIDEO_SCALE_PACK(200, 200));
   CHECK(layout.placements[0].dst.pos == VIDEO_POS_PACK(50, 0));
   /* One window for screens 1 and 2, stacked */
   video_driver_window_layout_screens(&frame, 6u,
         VIDEO_SCALE_PACK(300, 200), &layout);
   CHECK(layout.num_placements == 2);
   CHECK(layout.placements[0].view == 3 && layout.placements[1].view == 2);
   CHECK(layout.placements[0].dst.pos == VIDEO_POS_PACK(100, 0));
   CHECK(layout.placements[1].dst.pos == VIDEO_POS_PACK(100, 100));
   CHECK(layout.placements[1].dst.dims == VIDEO_SCALE_PACK(100, 100));
   frame.stretch = true;
   video_driver_window_layout(&frame, 2, VIDEO_SCALE_PACK(300, 200), &layout);
   CHECK(layout.placements[0].dst.dims == VIDEO_SCALE_PACK(300, 200));
   frame.stretch     = false;
   frame.stereo_mode = VIDEO_STEREO_MODE_SBS_FULL;
   video_driver_window_layout(&frame, 1, VIDEO_SCALE_PACK(400, 200), &layout);
   CHECK(layout.num_placements == 2);
   CHECK(layout.placements[0].view == 3 && layout.placements[1].view == 1);
   frame.swap_eyes = true;
   video_driver_window_layout(&frame, 1, VIDEO_SCALE_PACK(400, 200), &layout);
   CHECK(layout.placements[0].view == 1 && layout.placements[1].view == 3);
   video_driver_window_layout(&frame, 5, VIDEO_SCALE_PACK(400, 200), &layout);
   CHECK(layout.num_placements == 0);
}

static void window_test_display(video_window_display_t *d,
      unsigned ordinal, unsigned connection, bool own, const char *name)
{
   memset(d, 0, sizeof(*d));
   d->ordinal    = ordinal;
   d->connection = connection;
   d->own        = own;
   d->valid      = true;
   d->on         = true;
   d->physical   = true;
   /* Listed first, as Android lists its default display */
   d->is_default = ordinal == 1;
   strlcpy(d->name, name, sizeof(d->name));
}

/* The frame's snapshot of the screens' windows, and the main window's
 * layout without the screens that have windows of their own */
static void test_window_frame(void)
{
   video_window_display_t monitors[2];
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   unsigned restore, retry;
   window_test_begin();
   window_test_flags = 1u << GFX_CTX_FLAGS_VIDEO_VIEWS;
   window_test_run();
   CHECK(window_test_placements() == 2);
   CHECK(window_test_handed == NULL);

   window_test_flags |= 1u << GFX_CTX_FLAGS_VIDEO_WINDOWS;
   window_test_run();
   CHECK(window_test_placements() == 1
         && video_driver_get_views_layout()->placements[0].view == 0);
   CHECK(window_test_handed != NULL);
   CHECK(window_test_seen.plan.main_mask == 1u);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.settings[1].mode == VIDEO_WINDOW_SEPARATE);
   CHECK(window_test_seen.core.num_views == 2);
   CHECK(window_test_seen.core.views[1].x == 4);
   CHECK(window_test_seen.core_dims == VIDEO_SCALE_PACK(8, 2));
   restore = window_test_seen.restore;

   /* Off: in no window */
   strlcpy(settings->arrays.video_screen_display[1], VIDEO_WINDOW_DISPLAY_OFF,
         sizeof(settings->arrays.video_screen_display[1]));
   settings->uints.video_screen_window_pos[1]  = VIDEO_POS_PACK(-20, 30);
   window_test_run();
   CHECK(window_test_placements() == 1);
   CHECK(window_test_seen.plan.hidden_mask == 2u);
   CHECK(window_test_seen.settings[1].mode == VIDEO_WINDOW_HIDDEN);
   CHECK(window_test_seen.settings[0].mode == VIDEO_WINDOW_ATTACHED);
   CHECK(window_test_seen.settings[1].pos == VIDEO_POS_PACK(-20, 30));
   /* Game Screen 1 Off too: the desktop resolves without a list */
   strlcpy(settings->arrays.video_screen_display[0], VIDEO_WINDOW_DISPLAY_OFF,
         sizeof(settings->arrays.video_screen_display[0]));
   window_test_run();
   CHECK(window_test_seen.plan.hidden_mask & 1u);
   settings->arrays.video_screen_display[0][0] = '\0';
   /* The layout's settings; a stereo mode places screen 1 once an eye */
   settings->bools.video_scale_integer         = true;
   settings->bools.video_force_aspect          = false;
   settings->uints.video_stereo_mode           = VIDEO_STEREO_MODE_TOP_BOTTOM;
   settings->bools.video_stereo_swap_eyes      = true;
   window_test_run();
   CHECK(window_test_placements() == 2);
   CHECK(window_test_seen.scale_integer);
   CHECK(window_test_seen.stretch);
   CHECK(window_test_seen.stereo_mode == VIDEO_STEREO_MODE_TOP_BOTTOM);
   CHECK(window_test_seen.swap_eyes);
   settings->bools.video_scale_integer         = false;
   settings->bools.video_force_aspect          = true;
   settings->uints.video_stereo_mode           = VIDEO_STEREO_MODE_2D;
   settings->bools.video_stereo_swap_eyes      = false;

   /* A close attaches a screen over a new list, until the windows are
    * restored */
   settings->arrays.video_screen_display[1][0] = '\0';
   video_st->windows_closed_attached           = 2u;
   window_test_run();
   CHECK(window_test_placements() == 2);
   CHECK(window_test_seen.plan.main_mask == 3u);
   CHECK(window_test_seen.restore == restore);
   retry = window_test_seen.retry;
   window_test_display(&monitors[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "DP-1");
   window_test_display(&monitors[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "HDMI-A-1");
   monitors[0].monitor = monitors[1].monitor = true;
   video_driver_publish_window_displays(monitors, 2);
   window_test_run();
   CHECK(window_test_seen.monitors);
   CHECK(window_test_seen.retry == retry + 1);
   CHECK(window_test_seen.restore == restore);
   CHECK(window_test_seen.plan.main_mask == 3u);
   CHECK(video_st->windows_closed_attached == 2u);
   video_driver_restore_windows();
   CHECK(video_st->windows_closed_attached == 0u);
   window_test_run();
   CHECK(window_test_placements() == 1);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.restore == restore + 1);
   CHECK(window_test_seen.retry == retry + 2);
   /* A failure only until the next list */
   video_st->windows_forced_attached = 2u;
   window_test_run();
   CHECK(window_test_seen.plan.main_mask == 3u);
   video_driver_publish_window_displays(NULL, 0);
   window_test_run();
   CHECK(video_st->windows_forced_attached == 0u);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.restore == restore + 1);

   settings->bools.video_windows_enable = false;
   window_test_run();
   CHECK(window_test_placements() == 2);
   CHECK(window_test_handed == NULL);
   /* Content unloads (video_driver_clear_views()): none stay closed */
   video_st->windows_forced_attached = 2u;
   video_st->windows_closed_attached = 2u;
   restore                           = video_st->windows_restore;
   window_test_end();
   CHECK(video_st->windows_forced_attached == 0u);
   CHECK(video_st->windows_closed_attached == 0u);
   CHECK(video_st->windows_restore == restore + 1);
}

/* Desktop monitors: each screen's window where its Display puts it,
 * with no Presentation, move or UI routing */
static void test_window_monitors_frame(void)
{
   video_window_display_t displays[2];
   settings_t *settings = config_get_ptr();
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "DP-1");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "HDMI-A-1");
   displays[0].monitor = displays[1].monitor = true;
   window_test_begin();
   video_driver_publish_window_displays(displays, 2);
   /* Auto: Game Screen 2 on the monitor RetroArch is not on */
   window_test_run();
   CHECK(window_test_seen.monitors);
   CHECK(window_test_seen.display == 0);
   CHECK(window_test_seen.plan.main_mask == 1u);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.window_screens[1] == 2u);
   CHECK(window_test_seen.window_ui[1] == 0u);
   CHECK(window_test_seen.settings[1].mode == VIDEO_WINDOW_SEPARATE);
   CHECK(window_test_seen.settings[1].monitor == 2);
   /* A size alone keeps Auto's monitor; bounds with a position set, the
    * origin too, go where they are (the platform's pick) */
   settings->uints.video_screen_window_pos[1]  = VIDEO_POS_PACK(0, 0);
   settings->uints.video_screen_window_dims[1] = VIDEO_SCALE_PACK(640, 480);
   window_test_run();
   CHECK(!window_test_seen.settings[1].pos_set);
   CHECK(window_test_seen.settings[1].monitor == 2);
   settings->bools.video_screen_window_pos_set[1] = true;
   window_test_run();
   CHECK(window_test_seen.settings[1].pos_set);
   CHECK(window_test_seen.settings[1].monitor == 0);
   settings->bools.video_screen_window_pos_set[1] = false;
   settings->uints.video_screen_window_dims[1]    = 0;
   /* RetroArch's own monitor, by name */
   strlcpy(settings->arrays.video_screen_display[1], "DP-1",
         sizeof(settings->arrays.video_screen_display[1]));
   window_test_run();
   CHECK(window_test_seen.settings[1].monitor == 1);
   /* Main Window */
   strlcpy(settings->arrays.video_screen_display[1],
         VIDEO_WINDOW_DISPLAY_MAIN,
         sizeof(settings->arrays.video_screen_display[1]));
   window_test_run();
   CHECK(window_test_seen.plan.main_mask == 3u);
   CHECK(window_test_seen.plan.separate_mask == 0u);
   CHECK(window_test_placements() == 2);
   /* Game Screen 1 on a monitor: a window past the content's screens */
   strlcpy(settings->arrays.video_screen_display[0], "HDMI-A-1",
         sizeof(settings->arrays.video_screen_display[0]));
   window_test_run();
   CHECK(window_test_seen.plan.separate_mask == 1u);
   CHECK(window_test_seen.plan.main_mask == 2u);
   CHECK(window_test_seen.window_screens[2] == 1u);
   CHECK(window_test_seen.window_screens[1] == 0u);
   CHECK(window_test_seen.settings[2].mode == VIDEO_WINDOW_SEPARATE);
   CHECK(window_test_seen.settings[2].monitor == 2);
   /* Off */
   strlcpy(settings->arrays.video_screen_display[0],
         VIDEO_WINDOW_DISPLAY_OFF,
         sizeof(settings->arrays.video_screen_display[0]));
   window_test_run();
   CHECK(window_test_seen.plan.hidden_mask == 1u);
   CHECK(window_test_seen.settings[0].mode == VIDEO_WINDOW_HIDDEN);
   /* A monitor that is gone: Auto, the main window for Game Screen 1 */
   strlcpy(settings->arrays.video_screen_display[0], "DP-9",
         sizeof(settings->arrays.video_screen_display[0]));
   window_test_run();
   CHECK(window_test_seen.plan.main_mask == 3u);
   /* The listed ordinals, and by window id those that show a screen
    * whose Display names one */
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "RA-A");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "RA-B");
   displays[0].monitor = displays[1].monitor = true;
   video_driver_publish_window_displays(displays, 2);
   settings->arrays.video_screen_display[0][0] = '\0';
   strlcpy(settings->arrays.video_screen_display[1], "RA-B",
         sizeof(settings->arrays.video_screen_display[1]));
   window_test_run();
   CHECK(window_test_seen.monitors_present == 3u);
   CHECK(window_test_seen.window_named == 2u);
   video_driver_publish_window_displays(displays, 1);
   window_test_run();
   CHECK(window_test_seen.monitors_present == 1u);
   CHECK(window_test_seen.window_named == 0u);
   /* Game Screen 1's spare id */
   video_driver_publish_window_displays(displays, 2);
   strlcpy(settings->arrays.video_screen_display[0], "RA-B",
         sizeof(settings->arrays.video_screen_display[0]));
   strlcpy(settings->arrays.video_screen_display[1],
         VIDEO_WINDOW_DISPLAY_MAIN,
         sizeof(settings->arrays.video_screen_display[1]));
   window_test_run();
   CHECK(window_test_seen.window_screens[2] == 1u);
   CHECK(window_test_seen.window_named == 4u);
   settings->arrays.video_screen_display[0][0] = '\0';
   settings->arrays.video_screen_display[1][0] = '\0';
   video_driver_publish_window_displays(NULL, 0);
   window_test_run();
   window_test_end();
}

/* Window @window drawn from the last snapshot, as its renderer does */
static void window_test_draw_window(unsigned window, unsigned generation)
{
   video_views_layout_t layout;
   video_driver_window_layout_screens(&window_test_seen,
         window_test_seen.window_screens[window],
         VIDEO_SCALE_PACK(640, 480), &layout);
   video_driver_publish_window_layout(window, generation,
         window_test_seen.display, &layout, &window_test_seen.core,
         window_test_seen.core_dims, VIDEO_SCALE_PACK(640, 480));
}

static bool window_test_has_focus;

static bool window_test_focus(void *data)
{
   (void)data;
   return window_test_has_focus;
}

/* The fullscreen hotkey: the screen whose own window has the keyboard
 * focus gets its Fullscreen flipped, as its row does; otherwise, with
 * RetroArch in the background, and on a display's Presentation, the
 * main window's is the one toggled */
static void test_window_fullscreen_hotkey(void)
{
   video_window_display_t displays[2];
   unsigned gen, edit;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   bool *fullscreen               =
      settings->bools.video_screen_window_fullscreen;
   bool main_fullscreen           = settings->bools.video_fullscreen;
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "DP-1");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "HDMI-A-1");
   displays[0].monitor = displays[1].monitor = true;
   fullscreen[0]       = fullscreen[1]       = false;
   window_test_begin();
   window_test_driver.focus = window_test_focus;
   window_test_has_focus    = true;
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(window_test_seen.window_screens[1] == 2u);
   gen = video_driver_window_generation();
   /* Drawn without the focus */
   window_test_draw_window(1, gen);
   CHECK(!video_driver_window_fullscreen_toggle());
   /* Focused: Game Screen 2's, and its window follows the edit */
   video_driver_publish_window_focus(1, gen, true);
   window_test_draw_window(1, gen);
   edit             = video_st->windows_edit[1];
   settings->flags &= ~SETTINGS_FLG_MODIFIED;
   CHECK(video_driver_window_fullscreen_toggle());
   CHECK(fullscreen[1] && !fullscreen[0]);
   CHECK(settings->bools.video_fullscreen == main_fullscreen);
   CHECK(settings->flags & SETTINGS_FLG_MODIFIED);
   CHECK(video_st->windows_edit[1] == edit + 1);
   window_test_run();
   CHECK(window_test_seen.settings[1].fullscreen);
   CHECK(video_driver_window_fullscreen_toggle());
   CHECK(!fullscreen[1]);
   /* A pad hotkey with RetroArch in the background, the loss not yet
    * published */
   window_test_has_focus = false;
   CHECK(!video_driver_window_fullscreen_toggle());
   CHECK(!fullscreen[1]);
   window_test_has_focus = true;
   /* Lost, not drawn since: minimised or hidden, or a repeated frame */
   video_driver_publish_window_focus(1, gen, false);
   CHECK(!video_driver_window_fullscreen_toggle());
   CHECK(!fullscreen[1]);
   /* Let go of while focused: neither it nor the next window in its id
    * has the focus */
   video_driver_publish_window_focus(1, gen, true);
   video_driver_publish_window_layout(1, 0, 0, NULL, NULL, 0, 0);
   CHECK(!video_driver_window_fullscreen_toggle());
   gen = video_driver_window_generation();
   window_test_draw_window(1, gen);
   CHECK(!video_driver_window_fullscreen_toggle());
   CHECK(!fullscreen[1]);
   /* Game Screen 1's own window, by its spare id */
   strlcpy(settings->arrays.video_screen_display[0], "HDMI-A-1",
         sizeof(settings->arrays.video_screen_display[0]));
   strlcpy(settings->arrays.video_screen_display[1],
         VIDEO_WINDOW_DISPLAY_MAIN,
         sizeof(settings->arrays.video_screen_display[1]));
   window_test_run();
   CHECK(window_test_seen.window_screens[2] == 1u);
   gen = video_driver_window_generation();
   /* ... focused, not drawn since */
   window_test_draw_window(2, gen);
   video_driver_publish_window_focus(2, gen, true);
   CHECK(video_driver_window_fullscreen_toggle());
   CHECK(fullscreen[0] && !fullscreen[1]);
   fullscreen[0] = false;
   /* Screen windows off */
   settings->bools.video_windows_enable = false;
   window_test_run();
   CHECK(!video_driver_window_fullscreen_toggle());
   settings->bools.video_windows_enable = true;
   video_driver_publish_window_layout(2, 0, 0, NULL, NULL, 0, 0);
   settings->arrays.video_screen_display[0][0] = '\0';
   settings->arrays.video_screen_display[1][0] = '\0';
   /* A display's Presentation, which has no windowed mode */
   displays[0].monitor = displays[1].monitor = false;
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(window_test_seen.window_screens[VIDEO_WINDOWS_PRESENTATION]
         == 2u);
   gen = video_driver_window_generation();
   video_driver_publish_window_focus(VIDEO_WINDOWS_PRESENTATION, gen,
         true);
   window_test_draw_window(VIDEO_WINDOWS_PRESENTATION, gen);
   CHECK(!video_driver_window_fullscreen_toggle());
   CHECK(!fullscreen[1]);
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 0, 0,
         NULL, NULL, 0, 0);
   video_driver_publish_window_displays(NULL, 0);
   window_test_run();
   input_driver_hold_clear();
   window_test_end();
}

/* A platform's display list routes the Separate screens into one
 * window for the display; a window the renderer could not make goes
 * back to the main one until the next list */
static void test_window_displays(void)
{
   video_window_display_t displays[3];
   video_window_display_t many[VIDEO_WINDOW_DISPLAYS_MAX + 1];
   video_window_display_t got[VIDEO_WINDOW_DISPLAYS_MAX + 1];
   video_views_layout_t layout;
   video_windows_frame_t before;
   unsigned restore, retry;
   unsigned i;
   /* The Thor: its own panel, then Screen-2 */
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Screen-2");
   window_test_display(&displays[2], 3, VIDEO_WINDOW_CONNECTION_DIRECT,
         false, "HDMI");
   displays[1].dims = VIDEO_SCALE_PACK(1240, 1080);
   window_test_begin();
   /* No list: a window a screen, as on the desktop */
   window_test_run();
   CHECK(window_test_seen.display == 0);
   CHECK(window_test_seen.window_screens[1] == 2u);
   restore = window_test_seen.restore;
   retry   = window_test_seen.retry;

   /* A new list retries, and restores nothing */
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.window_screens[0] == 0u);
   CHECK(window_test_seen.window_screens[1] == 2u);
   CHECK(window_test_seen.retry == retry + 1);
   CHECK(window_test_seen.restore == restore);
   CHECK(window_test_placements() == 1);
   video_driver_window_layout_screens(&window_test_seen, 2u,
         VIDEO_SCALE_PACK(400, 200), &layout);
   CHECK(layout.num_placements == 1 && layout.placements[0].view == 1);
   video_driver_window_layout_screens(&window_test_seen, 3u,
         VIDEO_SCALE_PACK(400, 200), &layout);
   CHECK(layout.num_placements == 2);
   CHECK(video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX)
         == 2);
   CHECK(got[1].ordinal == 2 && !strcmp(got[1].name, "Screen-2"));
   CHECK(got[1].dims == VIDEO_SCALE_PACK(1240, 1080));
   memset(got, 0, sizeof(got));
   CHECK(video_driver_get_window_displays(got, 1) == 1);
   CHECK(got[0].ordinal == 1 && !strcmp(got[0].name, "Built-in Screen"));
   CHECK(got[1].ordinal == 0);
   /* A list is taken once */
   window_test_run();
   CHECK(window_test_seen.retry == retry + 1);

   /* A failure from a frame laid out before the last retry is not
    * held against the window the retry asked for */
   video_driver_attach_windows(2u, window_test_seen.retry - 1);
   window_test_run();
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.window_screens[1] == 2u);

   /* ... nor a close from before the last restore */
   before         = window_test_seen;
   before.restore = window_test_seen.restore - 1;
   video_driver_close_windows(&before, 2u);
   window_test_run();
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(video_state_get_ptr()->windows_closed_attached == 0u);

   /* Closed from its title bar: attached over a new list, until a
    * restore */
   video_driver_close_windows(&window_test_seen, 2u);
   window_test_run();
   CHECK(window_test_seen.plan.main_mask == 3u);
   CHECK(window_test_seen.window_screens[1] == 0u);
   CHECK(video_state_get_ptr()->windows_closed_attached == 2u);
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(window_test_seen.plan.main_mask == 3u);
   video_driver_restore_windows();
   window_test_run();
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.window_screens[1] == 2u);

   /* ... or an edit of its rows: a close from a frame before the edit,
    * sent before it or after, is dropped */
   before = window_test_seen;
   video_driver_close_windows(&before, 2u);
   video_driver_edit_window(1);
   window_test_run();
   CHECK(video_state_get_ptr()->windows_closed_attached == 0u);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   before = window_test_seen;
   video_driver_edit_window(1);
   video_driver_close_windows(&before, 2u);
   window_test_run();
   CHECK(video_state_get_ptr()->windows_closed_attached == 0u);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   /* One from a frame that carries the edit holds */
   video_driver_close_windows(&window_test_seen, 2u);
   window_test_run();
   CHECK(video_state_get_ptr()->windows_closed_attached == 2u);
   CHECK(window_test_seen.plan.main_mask == 3u);
   /* An edit of Game Screen 1 leaves Game Screen 2's close */
   video_driver_restore_windows();
   window_test_run();
   video_driver_close_windows(&window_test_seen, 3u);
   video_driver_edit_window(0);
   window_test_run();
   CHECK(video_state_get_ptr()->windows_closed_attached == 2u);
   CHECK(window_test_seen.plan.main_mask == 3u);
   video_driver_restore_windows();
   window_test_run();
   CHECK(window_test_seen.plan.separate_mask == 2u);

   /* The renderer could not make it: attached until the next list */
   video_driver_attach_windows(2u, window_test_seen.retry);
   window_test_run();
   CHECK(window_test_seen.plan.main_mask == 3u);
   CHECK(window_test_seen.window_screens[1] == 0u);
   CHECK(window_test_placements() == 2);
   window_test_run();
   CHECK(window_test_seen.plan.main_mask == 3u);
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(window_test_seen.plan.separate_mask == 2u);

   /* An external monitor takes Game Screen 1, so RetroArch is to move there;
    * first Game Screen 2's Presentation on the display it leaves */
   video_driver_publish_window_displays(displays, 3);
   window_test_run();
   CHECK(window_test_seen.display == 1);
   CHECK(window_test_seen.plan.main_mask == 1u);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.window_screens[VIDEO_WINDOWS_PRESENTATION]
         == 2u);
   CHECK(video_state_get_ptr()->windows_host.make == 1);
   CHECK(video_state_get_ptr()->windows_host.host == 3);
   CHECK(window_test_placements() == 1
         && video_driver_get_views_layout()->placements[0].view == 0);

   /* Nowhere to go: the screen stays attached */
   displays[1].on = false;
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(window_test_seen.display == 0);
   CHECK(window_test_seen.plan.main_mask == 3u);
   CHECK(window_test_seen.window_screens[1] == 0u);

   /* A longer list keeps its first VIDEO_WINDOW_DISPLAYS_MAX */
   for (i = 0; i <= VIDEO_WINDOW_DISPLAYS_MAX; i++)
      window_test_display(&many[i], i + 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
            false, "Screen");
   video_driver_publish_window_displays(many, VIDEO_WINDOW_DISPLAYS_MAX + 1);
   CHECK(video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX + 1)
         == VIDEO_WINDOW_DISPLAYS_MAX);
   CHECK(got[VIDEO_WINDOW_DISPLAYS_MAX - 1].ordinal
         == VIDEO_WINDOW_DISPLAYS_MAX);

   /* An empty list stops routing */
   video_driver_publish_window_displays(NULL, 0);
   window_test_run();
   CHECK(window_test_seen.display == 0);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.window_screens[1] == 2u);
   CHECK(video_driver_get_window_displays(got, VIDEO_WINDOW_DISPLAYS_MAX)
         == 0);
   window_test_end();
}

/* The touch's screen position, as a touch driver reports it */
static uint32_t menu_test_screen_pos;
/* A pointer's back, held */
static bool menu_test_back;

static int16_t menu_test_input_state(void *data,
      const input_device_driver_t *joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   (void)data;
   (void)joypad;
   (void)joypad_info;
   (void)binds;
   (void)idx;
   if (port || device != RARCH_DEVICE_POINTER_SCREEN)
      return 0;
   if (id == RETRO_DEVICE_ID_POINTER_X)
      return VIDEO_POS_X(menu_test_screen_pos);
   if (id == RETRO_DEVICE_ID_POINTER_Y)
      return VIDEO_POS_Y(menu_test_screen_pos);
   if (id == RARCH_DEVICE_ID_POINTER_BACK)
      return menu_test_back;
   return 0;
}

static void menu_test_set_texture(void *data)
{
   (void)data;
}

/* Where menu_event() puts a touch at window (x, y) */
static uint32_t menu_test_touch(struct video_viewport *vp, int x, int y)
{
   static input_bits_t none;
   menu_input_pointer_t pointer;
   uint32_t pos;
   CHECK(video_driver_translate_coord_viewport(vp, x, y, &pos,
            &menu_test_screen_pos, false));
   input_driver_poll_devices();
   menu_event(config_get_ptr(), &none, &none, false);
   menu_input_get_pointer_state(&pointer);
   return pointer.pos;
}

static bool menu_test_near(uint32_t pos, int x, int y)
{
   return abs(VIDEO_POS_X(pos) - x) <= 1 && abs(VIDEO_POS_Y(pos) - y) <= 1;
}

/* The menu's touches on the 800x480 main window: where they are on it,
 * and for RGUI through its 320x240 framebuffer, fitted 4:3 at x 80 */
static void window_test_menu_touch(struct video_viewport *vp)
{
   static input_driver_t driver;
   static menu_ctx_driver_t rgui_ctx;
   static menu_handle_t rgui;
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   input_driver_state_t *input_st = input_state_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   gfx_display_t *p_disp          = disp_get_ptr();
   input_driver_t *saved_driver   = input_st->current_driver;
   menu_handle_t *saved_menu      = menu_st->driver_data;
   unsigned saved_fb              = p_disp->framebuf_dims;
   float saved_aspect             = VIDEO_DRIVER_ASPECT_RATIO(video_st);
   bool saved_pointer             = settings->bools.menu_pointer_enable;
   bool saved_mouse               = settings->bools.menu_mouse_enable;
   bool saved_overlay             = settings->bools.input_overlay_enable;
   memset(&driver, 0, sizeof(driver));
   driver.input_state             = menu_test_input_state;
   driver.ident                   = "menu_test";
   input_st->current_driver       = &driver;
   settings->bools.menu_pointer_enable  = true;
   settings->bools.menu_mouse_enable    = false;
   settings->bools.input_overlay_enable = false;
   p_disp->framebuf_dims          = VIDEO_SCALE_PACK(800, 480);
   CHECK(menu_test_near(menu_test_touch(vp, 600, 120), 600, 120));
   memset(&rgui_ctx, 0, sizeof(rgui_ctx));
   rgui_ctx.set_texture           = menu_test_set_texture;
   memset(&rgui, 0, sizeof(rgui));
   rgui.driver_ctx                = &rgui_ctx;
   menu_st->driver_data           = &rgui;
   p_disp->framebuf_dims          = VIDEO_SCALE_PACK(320, 240);
   video_driver_aspect_ratio_put(&video_st->aspect_ratio_bits, 4.0f / 3.0f);
   video_driver_publish_vp_params();
   CHECK(menu_test_near(menu_test_touch(vp, 560, 360), 240, 180));
   video_driver_aspect_ratio_put(&video_st->aspect_ratio_bits, saved_aspect);
   video_driver_publish_vp_params();
   menu_st->driver_data           = saved_menu;
   p_disp->framebuf_dims          = saved_fb;
   input_st->current_driver       = saved_driver;
   settings->bools.menu_pointer_enable  = saved_pointer;
   settings->bools.menu_mouse_enable    = saved_mouse;
   settings->bools.input_overlay_enable = saved_overlay;
}

/* What menu_event() last returned in menu_test_window_touch() */
static unsigned menu_test_action;

/* Where menu_event() puts @count touches (0 or 1) at (x, y) of the
 * Presentation, generation @gen, or of the main window */
static uint32_t menu_test_window_touch(unsigned gen, bool presentation,
      unsigned count, int x, int y, bool *pressed)
{
   static input_bits_t none;
   input_pointer_frame_t frame;
   input_video_window_t window;
   menu_input_pointer_t pointer;
   uint32_t touch = VIDEO_POS_PACK(x, y);
   memset(&frame, 0, sizeof(frame));
   window.window     = presentation ? VIDEO_WINDOWS_PRESENTATION : 0;
   window.generation = presentation ? gen : 0;
   input_driver_publish_window_pointers(&frame, NULL, 1,
         INPUT_POINTERS_WINDOWS_SECOND);
   input_driver_publish_window_touches(&touch, &window, count,
         count ? 1 : 0, count ? 1 : 0);
   input_driver_poll_devices();
   menu_test_action = menu_event(config_get_ptr(), &none, &none, false);
   menu_input_get_pointer_state(&pointer);
   *pressed = (pointer.flags & MENU_INP_PTR_FLG_PRESSED) != 0;
   return pointer.pos;
}

/* menu_event() with no touch, the mouse's @buttons and a pointer's back
 * held or not (the driver's to answer); whether the menu's cancel is
 * inhibited */
static bool menu_test_cancel(uint16_t buttons, bool back)
{
   static input_bits_t none;
   input_pointer_frame_t frame;
   uint32_t none_pos = 0;
   memset(&frame, 0, sizeof(frame));
   frame.buttons  = buttons;
   menu_test_back = back;
   input_driver_publish_window_pointers(&frame, NULL, 1,
         INPUT_POINTERS_WINDOWS_SECOND
         | INPUT_POINTERS_POINTER_IDS_BY_DRIVER);
   input_driver_publish_window_touches(&none_pos, NULL, 0, 0, 0);
   input_driver_poll_devices();
   menu_event(config_get_ptr(), &none, &none, false);
   return menu_state_get_ptr()->input_state.cancel_inhibit;
}

/* The menu in the Presentation: its touches over that window at
 * the menu layer's size, none of the main window's; RGUI through its
 * framebuffer as it is fitted to the layer */
static void test_window_menu_touch_window(void)
{
   static input_driver_t driver;
   static menu_ctx_driver_t rgui_ctx;
   static menu_handle_t rgui;
   static menu_file_list_cbs_t resume_cbs;
   static struct item_file resume_item;
   static file_list_t resume_rows;
   static file_list_t no_rows;
   static file_list_t *resume_selection[1];
   static file_list_t *resume_stack[1];
   static menu_list_t resume_list;
   video_views_layout_t ui_only;
   video_views_map_t no_map;
   input_pointer_frame_t frame;
   uint32_t none_pos              = 0;
   unsigned dims                  = 0;
   unsigned gen;
   unsigned i;
   unsigned j;
   bool pressed                   = false;
   float saved_aspect;
   video_window_ui_target_t saved_ui;
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   input_driver_state_t *input_st = input_state_get_ptr();
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   gfx_display_t *p_disp          = disp_get_ptr();
   input_driver_t *saved_driver   = input_st->current_driver;
   menu_handle_t *saved_menu      = menu_st->driver_data;
   menu_list_t *saved_list        = menu_st->entries.list;
   size_t saved_selection         = menu_st->selection_ptr;
   unsigned saved_fb              = p_disp->framebuf_dims;
   uint16_t saved_menu_flags      = menu_st->flags;
   retro_keyboard_event_t saved_key_event = runloop_st->key_event;
   retro_keyboard_event_t saved_frontend_key_event =
      runloop_st->frontend_key_event;
   bool saved_pointer             = settings->bools.menu_pointer_enable;
   bool saved_mouse               = settings->bools.menu_mouse_enable;
   bool saved_overlay             = settings->bools.input_overlay_enable;

   window_test_begin();
   saved_aspect = VIDEO_DRIVER_ASPECT_RATIO(video_st);
   saved_ui     = video_st->windows_ui[VIDEO_WINDOW_UI_MENU];
   gen          = video_driver_window_generation();
   memset(&driver, 0, sizeof(driver));
   driver.input_state             = menu_test_input_state;
   driver.ident                   = "menu_window_test";
   input_st->current_driver       = &driver;
   memset(&rgui, 0, sizeof(rgui));
   menu_st->driver_data           = &rgui;
   settings->bools.menu_pointer_enable  = true;
   settings->bools.menu_mouse_enable    = false;
   settings->bools.input_overlay_enable = false;

   /* The Presentation has drawn at 640x360; the menu lays out there */
   memset(&ui_only, 0, sizeof(ui_only));
   memset(&no_map, 0, sizeof(no_map));
   ui_only.canvas_dims = VIDEO_SCALE_PACK(640, 360);
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, gen, 4,
         &ui_only, &no_map, 0, VIDEO_SCALE_PACK(640, 360));
   CHECK(video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, &dims) == 4);
   CHECK(dims == VIDEO_SCALE_PACK(640, 360));
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window =
      VIDEO_WINDOWS_PRESENTATION;
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].dims   =
      VIDEO_SCALE_PACK(640, 360);
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait   = false;
   CHECK(video_driver_get_ui_layer_window(VIDEO_WINDOW_UI_MENU)
         == VIDEO_WINDOWS_PRESENTATION);
   CHECK(video_driver_get_ui_layer_dims(VIDEO_WINDOW_UI_MENU)
         == VIDEO_SCALE_PACK(640, 360));

   /* The menu there, then a touch there acts where it is */
   menu_test_window_touch(gen, true, 0, 0, 0, &pressed);
   CHECK(menu_test_near(menu_test_window_touch(gen, true, 1, 480, 90,
               &pressed), 480, 90));
   CHECK(pressed);
   /* The main window's touches never reach it; it keeps its place */
   CHECK(menu_test_near(menu_test_window_touch(gen, false, 1, 400, 240,
               &pressed), 480, 90));
   CHECK(!pressed);
   CHECK(menu_test_near(menu_test_window_touch(gen, true, 0, 0, 0,
               &pressed), 480, 90));
   CHECK(!pressed);

   /* A press held as the menu changes window is dropped, not a tap in
    * the other window's layout. menu_input_post_iterate(), which this
    * does not run, lifts the inhibit once select is let go. */
   menu_test_window_touch(gen, true, 1, 480, 90, &pressed);
   CHECK(pressed);
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window = 0;
   menu_test_window_touch(gen, true, 1, 480, 90, &pressed);
   CHECK(!pressed);
   CHECK(menu_st->input_state.select_inhibit);
   CHECK(!(menu_st->input_pointer_hw_state.flags
            & MENU_INP_PTR_FLG_PRESS_SELECT));
   menu_st->input_state.select_inhibit = false;
   /* A finger already down where it goes is not the menu's until it
    * lifts; the next one is */
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window =
      VIDEO_WINDOWS_PRESENTATION;
   menu_test_window_touch(gen, false, 1, 400, 240, &pressed);
   CHECK(!pressed);
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window = 0;
   menu_test_window_touch(gen, false, 1, 400, 240, &pressed);
   CHECK(!pressed);
   menu_test_window_touch(gen, false, 1, 420, 250, &pressed);
   CHECK(!pressed);
   CHECK(!(menu_st->input_state.pointer.flags
            & (MENU_INP_PTR_FLG_ACTIVE | MENU_INP_PTR_FLG_DRAGGED)));
   menu_test_window_touch(gen, false, 0, 0, 0, &pressed);
   CHECK(!pressed);
   CHECK(!(menu_st->input_pointer_hw_state.flags
            & MENU_INP_PTR_FLG_PRESS_SELECT));
   menu_st->input_state.select_inhibit = false;
   menu_test_window_touch(gen, false, 1, 400, 240, &pressed);
   CHECK(pressed);
   menu_test_window_touch(gen, false, 0, 0, 0, &pressed);
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window =
      VIDEO_WINDOWS_PRESENTATION;
   menu_test_window_touch(gen, true, 0, 0, 0, &pressed);
   CHECK(!pressed);

   /* A press already down as the menu opens is dropped until it lifts,
    * in the Presentation and in the main window, and the release of one
    * held as it closed is no tap. Opening it again while it is up (a
    * load that failed) leaves a press be. */
   for (i = 0; i < 2; i++)
   {
      bool presentation = (i == 0);
      int x             = presentation ? 480 : 400;
      int y             = presentation ?  90 : 240;
      video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window =
         presentation ? VIDEO_WINDOWS_PRESENTATION : 0;
      menu_test_window_touch(gen, presentation, 0, 0, 0, &pressed);
      menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
      retroarch_menu_running();
      menu_test_window_touch(gen, presentation, 1, x, y, &pressed);
      CHECK(!pressed);
      CHECK(menu_st->input_state.select_inhibit);
      menu_test_window_touch(gen, presentation, 1, x + 20, y + 10, &pressed);
      CHECK(!pressed);
      CHECK(!(menu_st->input_state.pointer.flags
               & (MENU_INP_PTR_FLG_ACTIVE | MENU_INP_PTR_FLG_DRAGGED)));
      menu_test_window_touch(gen, presentation, 0, 0, 0, &pressed);
      CHECK(!pressed);
      CHECK(!(menu_st->input_pointer_hw_state.flags
               & MENU_INP_PTR_FLG_PRESS_SELECT));
      menu_st->input_state.select_inhibit = false;
      menu_test_window_touch(gen, presentation, 1, x, y, &pressed);
      CHECK(pressed);
      retroarch_menu_running();
      menu_test_window_touch(gen, presentation, 1, x, y, &pressed);
      CHECK(pressed);
      CHECK(!menu_st->input_state.select_inhibit);
      menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
      retroarch_menu_running();
      menu_test_window_touch(gen, presentation, 0, 0, 0, &pressed);
      CHECK(!pressed);
      CHECK(menu_st->input_state.select_inhibit);
      CHECK(!(menu_st->input_pointer_hw_state.flags
               & MENU_INP_PTR_FLG_PRESS_SELECT));
      menu_st->input_state.select_inhibit = false;
   }

   /* So is a cancel down as the menu opens, or down already where it
    * goes: a pointer's back in either window, the mouse's right button
    * in the main one. menu_input_post_iterate() would cancel on its
    * press, unless inhibited, and lift the inhibit once it is up. */
   for (i = 0; i < 2; i++)
   {
      video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window =
         i ? 0 : VIDEO_WINDOWS_PRESENTATION;
      menu_test_cancel(0, false);
      menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
      retroarch_menu_running();
      CHECK(menu_test_cancel(0, true));
      CHECK((menu_st->input_pointer_hw_state.flags
               & MENU_INP_PTR_FLG_PRESS_CANCEL) != 0);
      menu_test_cancel(0, false);
      CHECK(!(menu_st->input_pointer_hw_state.flags
               & MENU_INP_PTR_FLG_PRESS_CANCEL));
      menu_st->input_state.cancel_inhibit = false;
   }
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window =
      VIDEO_WINDOWS_PRESENTATION;
   menu_test_cancel(0, false);
   CHECK(!menu_test_cancel(0, true));
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window = 0;
   CHECK(menu_test_cancel(0, true));
   menu_test_cancel(0, false);
   menu_st->input_state.cancel_inhibit = false;
   settings->bools.menu_mouse_enable = true;
   menu_test_cancel(0, false);
   menu_test_cancel(0, false);
   menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
   retroarch_menu_running();
   CHECK(menu_test_cancel(INPUT_POINTER_RIGHT, false));
   CHECK(menu_st->input_state.pointer.type == MENU_POINTER_MOUSE);
   CHECK((menu_st->input_pointer_hw_state.flags
            & MENU_INP_PTR_FLG_PRESS_CANCEL) != 0);
   menu_test_cancel(0, false);
   CHECK(!(menu_st->input_pointer_hw_state.flags
            & MENU_INP_PTR_FLG_PRESS_CANCEL));
   menu_st->input_state.cancel_inhibit = false;
   settings->bools.menu_mouse_enable = false;

   /* Nor does such a press act on Resume, which acts on OK's release:
    * held through an open in either window (90 frames, a long press at
    * 60 Hz), held as the menu leaves the Presentation, or down where it
    * goes. The next tap does. */
   resume_cbs.enum_idx            = MENU_ENUM_LABEL_RESUME_CONTENT;
   resume_item.actiondata         = &resume_cbs;
   resume_rows.list               = &resume_item;
   resume_rows.capacity           = 1;
   resume_rows.size               = 1;
   resume_selection[0]            = &resume_rows;
   resume_stack[0]                = &no_rows;
   resume_list.selection_buf      = resume_selection;
   resume_list.selection_buf_size = 1;
   resume_list.menu_stack         = resume_stack;
   resume_list.menu_stack_size    = 1;
   menu_st->entries.list          = &resume_list;
   menu_st->selection_ptr         = 0;
   for (i = 0; i < 4; i++)
   {
      bool in_presentation  = (i == 0 || i == 2);
      bool tap_presentation = (i == 0);
      int x                 = in_presentation ? 480 : 400;
      int y                 = in_presentation ?  90 : 240;
      video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window =
         (i == 1) ? 0 : VIDEO_WINDOWS_PRESENTATION;
      menu_test_window_touch(gen, i != 1, 0, 0, 0, &pressed);
      if (i < 2)
      {
         menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
         retroarch_menu_running();
      }
      else
      {
         menu_test_window_touch(gen, in_presentation, 1, x, y, &pressed);
         menu_test_window_touch(gen, in_presentation, 1, x, y, &pressed);
         CHECK(menu_test_action == MENU_ACTION_NOOP);
         video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window = 0;
      }
      for (j = 0; j < 90; j++)
      {
         menu_test_window_touch(gen, in_presentation, 1, x, y, &pressed);
         CHECK(menu_test_action == MENU_ACTION_NOOP);
      }
      for (j = 0; j < 3; j++)
      {
         menu_test_window_touch(gen, in_presentation, 0, 0, 0, &pressed);
         CHECK(menu_test_action == MENU_ACTION_NOOP);
         menu_st->input_state.select_inhibit = false;
      }
      x = tap_presentation ? 480 : 400;
      y = tap_presentation ?  90 : 240;
      menu_test_window_touch(gen, tap_presentation, 1, x, y, &pressed);
      CHECK(pressed);
      menu_test_window_touch(gen, tap_presentation, 0, 0, 0, &pressed);
      CHECK(menu_test_action == MENU_ACTION_NOOP);
      menu_test_window_touch(gen, tap_presentation, 0, 0, 0, &pressed);
      CHECK(menu_test_action == MENU_ACTION_OK);
   }
   menu_st->entries.list          = saved_list;
   menu_st->selection_ptr         = saved_selection;

   menu_st->flags                 = saved_menu_flags;
   runloop_st->key_event          = saved_key_event;
   runloop_st->frontend_key_event = saved_frontend_key_event;
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window =
      VIDEO_WINDOWS_PRESENTATION;
   menu_test_window_touch(gen, true, 0, 0, 0, &pressed);

   /* RGUI: its 320x240 framebuffer fitted 4:3 into 640x360, at x 80 */
   memset(&rgui_ctx, 0, sizeof(rgui_ctx));
   rgui_ctx.set_texture           = menu_test_set_texture;
   rgui.driver_ctx                = &rgui_ctx;
   p_disp->framebuf_dims          = VIDEO_SCALE_PACK(320, 240);
   video_driver_aspect_ratio_put(&video_st->aspect_ratio_bits, 4.0f / 3.0f);
   video_driver_publish_vp_params();
   CHECK(menu_test_near(menu_test_window_touch(gen, true, 1, 320, 180,
               &pressed), 160, 120));

   video_driver_aspect_ratio_put(&video_st->aspect_ratio_bits, saved_aspect);
   video_driver_publish_vp_params();
   memset(&frame, 0, sizeof(frame));
   input_driver_publish_window_pointers(&frame, NULL, 0, 0);
   input_driver_publish_window_touches(&none_pos, NULL, 0, 0, 0);
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, gen, 0,
         NULL, NULL, 0, 0);
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU] = saved_ui;
   menu_st->driver_data           = saved_menu;
   p_disp->framebuf_dims          = saved_fb;
   input_st->current_driver       = saved_driver;
   settings->bools.menu_pointer_enable  = saved_pointer;
   settings->bools.menu_mouse_enable    = saved_mouse;
   settings->bools.input_overlay_enable = saved_overlay;
   window_test_end();
}

/* The menu going into another window is no mouse move: the view keeps
 * the mouse where it last was */
static void test_window_menu_mouse_kept(void)
{
   static input_driver_t driver;
   input_pointer_frame_t frame;
   uint32_t none_pos              = 0;
   uint32_t at;
   video_driver_state_t *video_st = video_state_get_ptr();
   input_driver_state_t *input_st = input_state_get_ptr();
   input_driver_t *saved_driver   = input_st->current_driver;
   video_window_ui_target_t saved = video_st->windows_ui[VIDEO_WINDOW_UI_MENU];
   memset(&driver, 0, sizeof(driver));
   driver.input_state       = menu_test_input_state;
   driver.ident             = "menu_mouse_test";
   input_st->current_driver = &driver;
   memset(&frame, 0, sizeof(frame));
   frame.pos = VIDEO_POS_PACK(200, 100);
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window = 0;
   input_driver_publish_window_pointers(&frame, NULL, 1,
         INPUT_POINTERS_WINDOWS_SECOND
         | INPUT_POINTERS_POINTER_IDS_BY_DRIVER);
   input_driver_publish_window_touches(&none_pos, NULL, 0, 0, 0);
   input_driver_poll_devices();
   at = input_driver_pointer_view()->mouse_pos;
   CHECK(at == VIDEO_POS_PACK(200, 100));
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU].window =
      VIDEO_WINDOWS_PRESENTATION;
   frame.pos = VIDEO_POS_PACK(300, 150);
   input_driver_publish_window_pointers(&frame, NULL, 1,
         INPUT_POINTERS_WINDOWS_SECOND
         | INPUT_POINTERS_POINTER_IDS_BY_DRIVER);
   input_driver_publish_window_touches(&none_pos, NULL, 0, 0, 0);
   input_driver_poll_devices();
   CHECK(input_driver_pointer_view()->mouse_pos == at);
   video_st->windows_ui[VIDEO_WINDOW_UI_MENU] = saved;
   input_st->current_driver = saved_driver;
   input_driver_publish_window_pointers(&frame, NULL, 0, 0);
   input_driver_publish_window_touches(&none_pos, NULL, 0, 0, 0);
   input_driver_poll_devices();
}

/* Screens by name, as on the Thor; a main window with no screen keeps
 * the menu's touches and shows the idle logo */
static void test_window_display_names(void)
{
   const uint32_t oob = VIDEO_POS_PACK(-0x8000, -0x8000);
   video_window_display_t displays[3];
   struct video_viewport vp;
   uint32_t pos, screen_pos, main_screen;
   settings_t *settings       = config_get_ptr();
   struct menu_state *menu_st = menu_state_get_ptr();
   uint16_t saved_menu        = menu_st->flags;
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Screen-2");
   window_test_display(&displays[2], 3, VIDEO_WINDOW_CONNECTION_DIRECT,
         false, "DP Screen");
   CHECK(sizeof(settings->arrays.video_screen_display[0])
         == VIDEO_WINDOW_DISPLAY_NAME_SIZE);
   window_test_begin();
   menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
   video_driver_publish_window_displays(displays, 3);
   /* By name, where Auto would give Game Screen 1 the DP monitor */
   strlcpy(settings->arrays.video_screen_display[0], "Built-in Screen",
         sizeof(settings->arrays.video_screen_display[0]));
   strlcpy(settings->arrays.video_screen_display[1], "Screen-2",
         sizeof(settings->arrays.video_screen_display[1]));
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.plan.main_mask == 1u);
   CHECK(window_test_seen.window_screens[0] == 0u);
   CHECK(window_test_seen.window_screens[VIDEO_WINDOWS_PRESENTATION]
         == 2u);
   CHECK(!window_test_seen.logo);
   /* A touch on the main window lands on the screen it shows */
   CHECK(video_driver_get_viewport_info(&vp));
   CHECK(video_driver_translate_coord_viewport(&vp, 400, 240, &pos,
            &screen_pos, true));
   CHECK(VIDEO_POS_X(pos) < -0x2000);
   /* The menu on a display that refused, both screens on Screen-2: the
    * main window shows none, keeps the menu's touches and idles */
   displays[0].refused = true;
   video_driver_publish_window_displays(displays, 3);
   settings->uints.video_menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   strlcpy(settings->arrays.video_menu_display, "Built-in Screen",
         sizeof(settings->arrays.video_menu_display));
   strlcpy(settings->arrays.video_screen_display[0], "Screen-2",
         sizeof(settings->arrays.video_screen_display[0]));
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(window_test_seen.plan.separate_mask == 3u);
   CHECK(window_test_seen.logo);
   CHECK(!video_state_get_ptr()->windows_host.moving);
   CHECK(video_driver_get_views_layout() != NULL);
   CHECK(window_test_placements() == 0);
   CHECK(video_driver_translate_coord_viewport(&vp, 400, 240, &pos,
            &main_screen, true));
   CHECK(pos == oob);
   CHECK(main_screen == screen_pos);
   CHECK(video_driver_translate_coord_viewport(&vp, 400, 240, &pos,
            &main_screen, false));
   CHECK(pos == oob);
   CHECK(main_screen == screen_pos);
   window_test_menu_touch(&vp);
   settings->uints.video_menu_follows          = 0;
   settings->arrays.video_menu_display[0]      = '\0';
   settings->arrays.video_screen_display[0][0] = '\0';
   settings->arrays.video_screen_display[1][0] = '\0';
   menu_st->flags                              = saved_menu;
   window_test_end();
}

/* The frame path asks for each move once and never keeps one in the
 * route a redraw reuses; make before break at start, the Presentation
 * kept at close. Menu Follows Game Screen 2, the Thor's bottom panel. */
static void test_window_moves(void)
{
   video_window_display_t displays[2];
   video_views_layout_t layout;
   unsigned i;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   /* Cocoon starts RetroArch on the bottom panel */
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Screen-2");
   settings->uints.video_menu_follows = 1;
   window_test_begin();
   video_driver_publish_window_displays(displays, 2);
   /* A two-screen game: Game Screen 2's Presentation on RetroArch's display
    * first, the main window showing Game Screen 1 */
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.plan.main_mask == 1u);
   CHECK(video_st->windows_host.make == 2);
   CHECK(!video_st->windows_host.moving);
   /* It drew there: the move is asked once, and not kept for a redraw */
   video_driver_window_layout_screens(&window_test_seen, 2u,
         VIDEO_SCALE_PACK(1240, 1080), &layout);
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 1, 2,
         &layout, &window_test_seen.core, window_test_seen.core_dims,
         VIDEO_SCALE_PACK(1240, 1080));
   window_test_run();
   CHECK(video_st->windows_host.moving == 1);
   CHECK(!video_st->windows_host.make);
   CHECK(video_st->windows_route.move == 0);
   CHECK(window_test_seen.display == 2);
   window_test_run();
   CHECK(video_st->windows_host.moving == 1);
   /* The list shows RetroArch on the top panel */
   displays[0].own = true;
   displays[1].own = false;
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(!video_st->windows_host.moving);
   CHECK(window_test_seen.display == 2);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(!window_test_seen.window_keep);
   /* The game closes: RetroArch goes back under the Presentation, which
    * is kept, not drawn, until RetroArch has drawn there */
   runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_CORE_RUNNING;
   video_driver_clear_views();
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(window_test_seen.window_keep
         == 1u << VIDEO_WINDOWS_PRESENTATION);
   CHECK(window_test_seen.window_screens[VIDEO_WINDOWS_PRESENTATION]
         == 0u);
   CHECK(video_st->windows_host.moving == 2);
   CHECK(video_st->windows_route.move == 0);
   displays[0].own = false;
   displays[1].own = true;
   video_driver_publish_window_displays(displays, 2);
   for (i = 0; i < VIDEO_WINDOWS_HOLD_FRAMES; i++)
   {
      window_test_run();
      CHECK(window_test_seen.window_keep
            == 1u << VIDEO_WINDOWS_PRESENTATION);
   }
   window_test_run();
   CHECK(!window_test_seen.window_keep);
   CHECK(window_test_seen.display == 0);
   CHECK(!video_st->windows_host.moving);
   settings->uints.video_menu_follows = 0;
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 0, 0,
         NULL, NULL, 0, 0);
   window_test_end();
   /* The list the later tests expect */
   video_driver_publish_window_displays(displays, 2);
}

/* The Presentation drawn from the last snapshot on its display, as a
 * driver publishes it */
static void window_test_draw(unsigned generation)
{
   video_views_layout_t layout;
   video_driver_window_layout_screens(&window_test_seen,
         window_test_seen.window_screens[VIDEO_WINDOWS_PRESENTATION],
         VIDEO_SCALE_PACK(1240, 1080), &layout);
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION,
         generation, window_test_seen.display, &layout,
         &window_test_seen.core, window_test_seen.core_dims,
         VIDEO_SCALE_PACK(1240, 1080));
}

/* The threaded wrapper draws behind the app thread: a layout drawn
 * before the snapshot that last let the Presentation go or moved it is
 * not shown, so make before break holds. Nothing is published here
 * until the test says the video side drew. */
static void test_window_shown_lag(void)
{
   video_window_display_t displays[2];
   unsigned i;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   uint16_t saved_menu            = menu_st->flags;
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Screen-2");
   settings->uints.video_menu_follows = 1;
   menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
   window_test_begin();
   video_driver_publish_window_displays(displays, 2);
   /* No lag: drawn from the snapshot that asked, it shows at once */
   window_test_run();
   CHECK(video_st->windows_host.make == 2);
   window_test_draw(1);
   CHECK(video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL) == 2);
   window_test_run();
   CHECK(!video_st->windows_host.make);
   CHECK(video_st->windows_host.moving == 1);
   displays[0].own = true;
   displays[1].own = false;
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(!video_st->windows_host.moving);
   /* The game closes; RetroArch arrives under the kept Presentation,
    * which is then let go of */
   runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_CORE_RUNNING;
   video_driver_clear_views();
   window_test_run();
   CHECK(window_test_seen.window_keep
         == 1u << VIDEO_WINDOWS_PRESENTATION);
   displays[0].own = false;
   displays[1].own = true;
   video_driver_publish_window_displays(displays, 2);
   for (i = 0; i <= VIDEO_WINDOWS_HOLD_FRAMES; i++)
      window_test_run();
   CHECK(!window_test_seen.window_keep);
   CHECK(window_test_seen.display == 0);
   /* Not drawn yet: the newest layout still names display 2 */
   CHECK(!video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL));
   /* The game starts again at once: display 2 gets a new Presentation,
    * which must draw before RetroArch leaves */
   runloop_state_get_ptr()->flags |= RUNLOOP_FLAG_CORE_RUNNING;
   window_test_views();
   window_test_run();
   CHECK(video_st->windows_host.make == 2);
   CHECK(!video_st->windows_host.moving);
   window_test_run();
   CHECK(video_st->windows_host.make == 2);
   CHECK(!video_st->windows_host.moving);
   window_test_draw(2);
   window_test_run();
   CHECK(!video_st->windows_host.make);
   CHECK(video_st->windows_host.moving == 1);
   displays[0].own = true;
   displays[1].own = false;
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(!video_st->windows_host.moving);
   /* Without a map, the menu following Game Screen 2: open, the
    * Presentation shows it on display 2 */
   video_driver_clear_views();
   window_test_run();
   CHECK(window_test_seen.display == 0);
   menu_st->flags |= MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(window_test_seen.display == 2);
   window_test_draw(3);
   window_test_run();
   CHECK(video_driver_get_ui_layer_window(VIDEO_WINDOW_UI_MENU)
         == VIDEO_WINDOWS_PRESENTATION);
   CHECK(!video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   /* Closed and opened again: the wrapper took back the snapshot that
    * let it go, so the window lives on and draws the later one. Before
    * that draw, what it showed does not count. */
   menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(window_test_seen.display == 0);
   menu_st->flags |= MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(!video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL));
   CHECK(video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   window_test_draw(3);
   CHECK(video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL) == 2);
   window_test_run();
   CHECK(!video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   window_test_run();
   CHECK(video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL) == 2);
   /* Closed, then at once Game Screen 1 is set to display 2: RetroArch moves
    * there, and keeps no Presentation that is being let go of */
   menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(window_test_seen.display == 0);
   strlcpy(settings->arrays.video_screen_display[0], "Screen-2",
         sizeof(settings->arrays.video_screen_display[0]));
   window_test_run();
   CHECK(video_st->windows_host.moving == 2);
   CHECK(!video_st->windows_host.keep);
   CHECK(!window_test_seen.window_keep);
   settings->arrays.video_screen_display[0][0] = '\0';
   settings->uints.video_menu_follows          = 0;
   menu_st->flags                              = saved_menu;
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 0, 0,
         NULL, NULL, 0, 0);
   window_test_end();
   /* The list the later tests expect */
   displays[0].own = false;
   displays[1].own = true;
   video_driver_publish_window_displays(displays, 2);
}

static void window_test_free(void *data)
{
   (void)data;
}

#ifdef HAVE_THREADS
static retro_atomic_int_t window_test_bounds_reading;
static retro_atomic_int_t window_test_bounds_done;
#endif
/* The edit count window 2 is made for while it is dragged */
static unsigned window_test_bounds_edit;

/* Publishes window 2's bounds as a renderer does through a drag, every
 * other one a move */
static void window_test_publish_bounds(void *data)
{
   unsigned count = *(const unsigned*)data;
   unsigned i;
#ifdef HAVE_THREADS
   while (!retro_atomic_load_acquire_int(&window_test_bounds_reading))
      ;
#endif
   for (i = 1; i <= count; i++)
      video_driver_publish_window_bounds(2, 4u, window_test_bounds_edit,
            VIDEO_POS_PACK(i % 1000, 100), VIDEO_SCALE_PACK(200 + i, 300),
            (i & 1) != 0);
#ifdef HAVE_THREADS
   retro_atomic_store_release_int(&window_test_bounds_done, 1);
#endif
}

/* The bounds a renderer forwards for a window the user moved reach its
 * screen's settings, and only those */
static void test_window_bounds_saved(void)
{
   static struct retro_video_view views[3];
   static video_views_layout_t layout;
   static video_window_settings_t seen;
   struct retro_video_views map;
   video_window_display_t displays[2];
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   unsigned saved_pos[3], saved_dims[3];
   bool saved_pos_set[3];
   bool saved_save_positions      = settings->bools.video_window_save_positions;
   unsigned count                 = 2000;
   unsigned i, gen, spare, edit;
   uint32_t before, after;
   for (i = 0; i < 3; i++)
   {
      saved_pos[i]     = settings->uints.video_screen_window_pos[i];
      saved_dims[i]    = settings->uints.video_screen_window_dims[i];
      saved_pos_set[i] = settings->bools.video_screen_window_pos_set[i];
   }
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "RA-A");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "RA-B");
   displays[0].monitor = displays[1].monitor = true;
   window_test_begin();
   /* Three screens: Game Screen 3 named on RA-B, in window 2 */
   memset(views, 0, sizeof(views));
   views[0].width  = 4;
   views[0].height = views[1].height = views[2].height = 2;
   views[1].x      = 4;
   views[1].width  = views[2].width = 2;
   views[1].screen = 1;
   views[2].x      = 6;
   views[2].screen = 2;
   memset(&map, 0, sizeof(map));
   map.views     = views;
   map.num_views = 3;
   CHECK(video_driver_set_views(&map));
   settings->arrays.video_screen_display[0][0] = '\0';
   strlcpy(settings->arrays.video_screen_display[2], "RA-B",
         sizeof(settings->arrays.video_screen_display[2]));
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(window_test_seen.monitors);
   CHECK(window_test_seen.window_screens[2] == 4u);
   CHECK(window_test_seen.settings[2].monitor == 2);
   gen = video_driver_window_generation();
   video_driver_window_layout(&window_test_seen, 2,
         VIDEO_SCALE_PACK(800, 480), &layout);
   video_driver_publish_window_layout(2, gen, 2, &layout,
         &window_test_seen.core, window_test_seen.core_dims,
         VIDEO_SCALE_PACK(800, 480));
   CHECK(video_driver_translate_window_point(2, gen, 400, 240, true,
            &before));

   /* Moved: pos, size and pos_set, with no edit and the Display kept */
   edit             = window_test_seen.settings[2].edit;
   settings->flags &= ~SETTINGS_FLG_MODIFIED;
   video_driver_publish_window_bounds(2, 4u, video_st->windows_edit[2],
         VIDEO_POS_PACK(300, 200), VIDEO_SCALE_PACK(500, 400), true);
   window_test_run();
   CHECK(settings->uints.video_screen_window_pos[2]
         == VIDEO_POS_PACK(300, 200));
   CHECK(settings->uints.video_screen_window_dims[2]
         == VIDEO_SCALE_PACK(500, 400));
   CHECK(settings->bools.video_screen_window_pos_set[2]);
   CHECK(settings->flags & SETTINGS_FLG_MODIFIED);
   seen = window_test_seen.settings[2];
   CHECK(seen.dims == VIDEO_SCALE_PACK(500, 400) && seen.pos_set);
   CHECK(seen.edit == edit);
   CHECK(string_is_equal(settings->arrays.video_screen_display[2], "RA-B"));
   CHECK(seen.monitor == 2);
   /* The layout came with them, so input still maps the window */
   CHECK(video_driver_translate_window_point(2, gen, 400, 240, true,
            &after));
   CHECK(after == before);

   /* Resized only: the position stays */
   settings->uints.video_screen_window_pos[2] = VIDEO_POS_PACK(1, 1);
   video_driver_publish_window_bounds(2, 4u, video_st->windows_edit[2],
         VIDEO_POS_PACK(9, 9), VIDEO_SCALE_PACK(600, 450), false);
   window_test_run();
   CHECK(settings->uints.video_screen_window_dims[2]
         == VIDEO_SCALE_PACK(600, 450));
   CHECK(settings->uints.video_screen_window_pos[2] == VIDEO_POS_PACK(1, 1));
   /* The same again, and a move to where it is: nothing modified */
   settings->flags &= ~SETTINGS_FLG_MODIFIED;
   video_driver_publish_window_bounds(2, 4u, video_st->windows_edit[2],
         VIDEO_POS_PACK(9, 9), VIDEO_SCALE_PACK(600, 450), false);
   window_test_run();
   video_driver_publish_window_bounds(2, 4u, video_st->windows_edit[2],
         VIDEO_POS_PACK(1, 1), VIDEO_SCALE_PACK(600, 450), true);
   window_test_run();
   CHECK(!(settings->flags & SETTINGS_FLG_MODIFIED));
   /* Taken once: a frame with nothing new writes nothing */
   settings->uints.video_screen_window_dims[2] = VIDEO_SCALE_PACK(10, 10);
   window_test_run();
   CHECK(settings->uints.video_screen_window_dims[2]
         == VIDEO_SCALE_PACK(10, 10));
   /* Nor do the layouts after them, in whichever slot */
   for (i = 0; i < 3; i++)
   {
      video_driver_publish_window_layout(2, gen, 2, &layout,
            &window_test_seen.core, window_test_seen.core_dims,
            VIDEO_SCALE_PACK(800, 480));
      window_test_run();
   }
   CHECK(settings->uints.video_screen_window_dims[2]
         == VIDEO_SCALE_PACK(10, 10));

   /* Game Screen 1's spare id saves to Game Screen 1 */
   spare = video_windows_spare_window(window_test_seen.plan.separate_mask
         | 1u, window_test_seen.core.num_screens);
   CHECK(spare == 3);
   video_driver_publish_window_bounds(spare, 1u, video_st->windows_edit[0],
         VIDEO_POS_PACK(40, 50), VIDEO_SCALE_PACK(320, 240), true);
   window_test_run();
   CHECK(settings->uints.video_screen_window_pos[0] == VIDEO_POS_PACK(40, 50));
   CHECK(settings->uints.video_screen_window_dims[0]
         == VIDEO_SCALE_PACK(320, 240));
   CHECK(settings->bools.video_screen_window_pos_set[0]);
   CHECK(settings->uints.video_screen_window_dims[2]
         == VIDEO_SCALE_PACK(10, 10));

   /* Saved whatever Remember Window Position says */
   settings->bools.video_window_save_positions = false;
   video_driver_publish_window_bounds(2, 4u, video_st->windows_edit[2],
         VIDEO_POS_PACK(310, 210), VIDEO_SCALE_PACK(510, 410), true);
   window_test_run();
   CHECK(settings->uints.video_screen_window_pos[2]
         == VIDEO_POS_PACK(310, 210));
   CHECK(settings->uints.video_screen_window_dims[2]
         == VIDEO_SCALE_PACK(510, 410));
   settings->bools.video_window_save_positions = saved_save_positions;

   /* Wayland: a size alone never sets the position */
   settings->bools.video_screen_window_pos_set[2] = false;
   video_driver_publish_window_bounds(2, 4u, video_st->windows_edit[2],
         0, VIDEO_SCALE_PACK(520, 420), false);
   window_test_run();
   CHECK(settings->uints.video_screen_window_dims[2]
         == VIDEO_SCALE_PACK(520, 420));
   CHECK(!settings->bools.video_screen_window_pos_set[2]);
   CHECK(!window_test_seen.settings[2].pos_set);

   /* No window, no screens or no size: nothing */
   video_driver_publish_window_bounds(0, 4u, video_st->windows_edit[2],
         0, VIDEO_SCALE_PACK(1, 1), true);
   video_driver_publish_window_bounds(RETRO_VIDEO_VIEWS_MAX, 4u,
         video_st->windows_edit[2], 0, VIDEO_SCALE_PACK(1, 1), true);
   video_driver_publish_window_bounds(2, 0u, 0, 0, VIDEO_SCALE_PACK(1, 1),
         true);
   video_driver_publish_window_bounds(2, 4u, video_st->windows_edit[2],
         0, 0, true);
   window_test_run();
   CHECK(settings->uints.video_screen_window_dims[2]
         == VIDEO_SCALE_PACK(520, 420));
   CHECK(!settings->bools.video_screen_window_pos_set[2]);

   /* A quit's config save takes what no frame has */
   video_driver_publish_window_bounds(2, 4u, video_st->windows_edit[2],
         VIDEO_POS_PACK(320, 220), VIDEO_SCALE_PACK(530, 430), true);
   video_driver_take_window_bounds();
   CHECK(settings->uints.video_screen_window_pos[2]
         == VIDEO_POS_PACK(320, 220));
   CHECK(settings->uints.video_screen_window_dims[2]
         == VIDEO_SCALE_PACK(530, 430));
   CHECK(settings->bools.video_screen_window_pos_set[2]);

   /* A driver's teardown keeps what its last frames left, and the next
    * one's counts start again, and are taken */
   video_driver_publish_window_bounds(2, 4u, video_st->windows_edit[2],
         VIDEO_POS_PACK(330, 230), VIDEO_SCALE_PACK(540, 440), true);
   window_test_driver.free = window_test_free;
   video_driver_free_internal();
   CHECK(settings->uints.video_screen_window_pos[2]
         == VIDEO_POS_PACK(330, 230));
   CHECK(settings->uints.video_screen_window_dims[2]
         == VIDEO_SCALE_PACK(540, 440));
   video_st->data = &window_test_driver;
   video_st->poke = &window_test_poke;
   video_driver_publish_window_bounds(spare, 1u, video_st->windows_edit[0],
         VIDEO_POS_PACK(60, 70), VIDEO_SCALE_PACK(330, 250), true);
   window_test_run();
   CHECK(settings->uints.video_screen_window_pos[0] == VIDEO_POS_PACK(60, 70));
   CHECK(settings->uints.video_screen_window_dims[0]
         == VIDEO_SCALE_PACK(330, 250));

   /* A drag on the context's thread beside the frames: the last one
    * wins */
   window_test_bounds_edit = video_st->windows_edit[2];
#ifdef HAVE_THREADS
   {
      retro_time_t until;
      sthread_t *thread;
      retro_atomic_store_release_int(&window_test_bounds_reading, 0);
      retro_atomic_store_release_int(&window_test_bounds_done, 0);
      thread = sthread_create(window_test_publish_bounds, &count);
      CHECK(thread != NULL);
      until = cpu_features_get_time_usec() + 10000000;
      while (     thread
            && !retro_atomic_load_acquire_int(&window_test_bounds_done)
            && cpu_features_get_time_usec() < until)
      {
         window_test_run();
         retro_atomic_store_release_int(&window_test_bounds_reading, 1);
      }
      if (thread)
         sthread_join(thread);
   }
#else
   window_test_publish_bounds(&count);
#endif
   window_test_run();
   CHECK(settings->uints.video_screen_window_dims[2]
         == VIDEO_SCALE_PACK(200 + count, 300));
   CHECK(settings->uints.video_screen_window_pos[2]
         == VIDEO_POS_PACK((count - 1) % 1000, 100));
   CHECK(settings->bools.video_screen_window_pos_set[2]);

   /* The list test_window_monitors_frame() left */
   video_driver_publish_window_displays(displays, 1);
   window_test_run();
   video_driver_publish_window_layout(2, 0, 0, NULL, NULL, 0, 0);
   window_test_end();
   settings->arrays.video_screen_display[2][0] = '\0';
   for (i = 0; i < 3; i++)
   {
      settings->uints.video_screen_window_pos[i]     = saved_pos[i];
      settings->uints.video_screen_window_dims[i]    = saved_dims[i];
      settings->bools.video_screen_window_pos_set[i] = saved_pos_set[i];
   }
}

/* A menu edit crossing a save of a dragged window, with a renderer's
 * steps as gl2 and vulkan run them: the edit wins, and the window and
 * the settings end the same */
static void test_window_bounds_edit(void)
{
   static video_windows_held_t held;
   video_window_display_t displays[2];
   settings_t *settings               = config_get_ptr();
   const video_window_settings_t *win = &held.made[1];
   rarch_setting_t *x;
   unsigned retire, create;
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "RA-A");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "RA-B");
   displays[0].monitor = displays[1].monitor = true;
   load_fixture("");
   settings->uints.video_screen_window_pos[1]     = VIDEO_POS_PACK(100, 50);
   settings->uints.video_screen_window_dims[1]    = VIDEO_SCALE_PACK(640, 480);
   settings->bools.video_screen_window_pos_set[1] = true;
   window_test_begin();
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(window_test_seen.monitors);
   memset(&held, 0, sizeof(held));
   video_windows_steps(&held, &window_test_seen, &retire, &create);
   CHECK(create == 2u);
   held.live       = create;
   held.display[1] = window_test_seen.display;
   x = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_X + 1));
   CHECK(x != NULL);
   if (!x)
   {
      window_test_end();
      return;
   }

   /* A: Right on X is in the frame; the drag's save is judged before
    * that frame's steps, and taken after */
   CHECK(menu_action_handle_setting(x, 0, MENU_ACTION_RIGHT, false) == 0);
   window_test_run();
   video_driver_publish_window_bounds(1, held.made_screens[1], win->edit,
         VIDEO_POS_PACK(300, 200), VIDEO_SCALE_PACK(800, 600), true);
   video_windows_held_saved(&held, 1, VIDEO_POS_PACK(300, 200),
         VIDEO_SCALE_PACK(800, 600), true);
   video_windows_steps(&held, &window_test_seen, &retire, &create);
   CHECK(!retire && !create && held.update == 2u);
   CHECK(win->pos == VIDEO_POS_PACK(110, 50));
   CHECK(win->dims == VIDEO_SCALE_PACK(640, 480));
   window_test_run();
   CHECK(settings->uints.video_screen_window_pos[1]
         == VIDEO_POS_PACK(110, 50));
   CHECK(settings->uints.video_screen_window_dims[1]
         == VIDEO_SCALE_PACK(640, 480));
   video_windows_steps(&held, &window_test_seen, &retire, &create);
   CHECK(!retire && !create && !held.update);

   /* C: the save is published, and Right on X comes before the frame
    * that takes it */
   video_driver_publish_window_bounds(1, held.made_screens[1], win->edit,
         VIDEO_POS_PACK(400, 300), VIDEO_SCALE_PACK(820, 620), true);
   video_windows_held_saved(&held, 1, VIDEO_POS_PACK(400, 300),
         VIDEO_SCALE_PACK(820, 620), true);
   video_windows_steps(&held, &window_test_seen, &retire, &create);
   CHECK(!held.update);
   CHECK(menu_action_handle_setting(x, 0, MENU_ACTION_RIGHT, false) == 0);
   window_test_run();
   CHECK(settings->uints.video_screen_window_pos[1]
         == VIDEO_POS_PACK(120, 50));
   CHECK(settings->uints.video_screen_window_dims[1]
         == VIDEO_SCALE_PACK(640, 480));
   video_windows_steps(&held, &window_test_seen, &retire, &create);
   CHECK(!retire && !create && held.update == 2u);
   CHECK(win->pos == VIDEO_POS_PACK(120, 50));
   CHECK(win->dims == VIDEO_SCALE_PACK(640, 480));

   /* B: the save is taken first, with the edit it was made for, and the
    * row steps from it */
   video_driver_publish_window_bounds(1, held.made_screens[1], win->edit,
         VIDEO_POS_PACK(500, 400), VIDEO_SCALE_PACK(840, 640), true);
   video_windows_held_saved(&held, 1, VIDEO_POS_PACK(500, 400),
         VIDEO_SCALE_PACK(840, 640), true);
   video_windows_steps(&held, &window_test_seen, &retire, &create);
   window_test_run();
   CHECK(settings->uints.video_screen_window_pos[1]
         == VIDEO_POS_PACK(500, 400));
   CHECK(settings->uints.video_screen_window_dims[1]
         == VIDEO_SCALE_PACK(840, 640));
   video_windows_steps(&held, &window_test_seen, &retire, &create);
   CHECK(!retire && !create && !held.update);
   CHECK(menu_action_handle_setting(x, 0, MENU_ACTION_RIGHT, false) == 0);
   window_test_run();
   CHECK(settings->uints.video_screen_window_pos[1]
         == VIDEO_POS_PACK(510, 400));
   video_windows_steps(&held, &window_test_seen, &retire, &create);
   CHECK(held.update == 2u);
   CHECK(win->pos == VIDEO_POS_PACK(510, 400));
   CHECK(win->dims == VIDEO_SCALE_PACK(840, 640));

   /* A move from before an edit and a resize after it, taken together:
    * the resize alone */
   CHECK(menu_action_handle_setting(x, 0, MENU_ACTION_RIGHT, false) == 0);
   window_test_run();
   video_driver_publish_window_bounds(1, held.made_screens[1], win->edit,
         VIDEO_POS_PACK(600, 500), VIDEO_SCALE_PACK(860, 660), true);
   video_windows_held_saved(&held, 1, VIDEO_POS_PACK(600, 500),
         VIDEO_SCALE_PACK(860, 660), true);
   video_windows_steps(&held, &window_test_seen, &retire, &create);
   CHECK(held.update == 2u && win->pos == VIDEO_POS_PACK(520, 400));
   video_driver_publish_window_bounds(1, held.made_screens[1], win->edit,
         VIDEO_POS_PACK(520, 400), VIDEO_SCALE_PACK(880, 680), false);
   video_windows_held_saved(&held, 1, VIDEO_POS_PACK(520, 400),
         VIDEO_SCALE_PACK(880, 680), false);
   window_test_run();
   CHECK(settings->uints.video_screen_window_pos[1]
         == VIDEO_POS_PACK(520, 400));
   CHECK(settings->uints.video_screen_window_dims[1]
         == VIDEO_SCALE_PACK(880, 680));
   CHECK(settings->bools.video_screen_window_pos_set[1]);
   video_windows_steps(&held, &window_test_seen, &retire, &create);
   CHECK(!retire && !create && !held.update);
   CHECK(win->pos == VIDEO_POS_PACK(520, 400));
   CHECK(win->dims == VIDEO_SCALE_PACK(880, 680));

   /* The list test_window_bounds_saved() left */
   video_driver_publish_window_displays(displays, 1);
   window_test_run();
   window_test_end();
   settings->uints.video_screen_window_pos[1]     = 0;
   settings->uints.video_screen_window_dims[1]    = 0;
   settings->bools.video_screen_window_pos_set[1] = false;
}

/* A driver reinit: the teardown clears every layout, and the next
 * driver's init takes the kept Presentation and publishes it again,
 * with no screens. Still shown on display 2, so a close no placement
 * saw before the reinit keeps it there. */
static void test_window_shown_reinit(void)
{
   static video_views_layout_t layout;
   static video_views_map_t map;
   video_window_display_t displays[2];
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   uint16_t saved_menu            = menu_st->flags;
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Screen-2");
   settings->uints.video_menu_follows = 1;
   menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
   window_test_begin();
   video_driver_publish_window_displays(displays, 2);
   /* Game Screen 2 shown on display 2, RetroArch moved to display 1 */
   window_test_run();
   window_test_draw(1);
   CHECK(video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL) == 2);
   window_test_run();
   displays[0].own = true;
   displays[1].own = false;
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(!video_st->windows_host.moving);
   window_test_driver.free = window_test_free;
   video_driver_free_internal();
   CHECK(!video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL));
   video_st->data = &window_test_driver;
   video_st->poke = &window_test_poke;
   memset(&layout, 0, sizeof(layout));
   memset(&map, 0, sizeof(map));
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 1, 2,
         &layout, &map, 0, VIDEO_SCALE_PACK(1240, 1080));
   CHECK(video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL) == 2);
   /* The game closed before the reinit */
   runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_CORE_RUNNING;
   video_driver_clear_views();
   window_test_run();
   CHECK(window_test_seen.window_keep
         == 1u << VIDEO_WINDOWS_PRESENTATION);
   CHECK(window_test_seen.display == 2);
   settings->uints.video_menu_follows = 0;
   menu_st->flags                     = saved_menu;
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 0, 0,
         NULL, NULL, 0, 0);
   window_test_end();
   /* The list the later tests expect */
   displays[0].own = false;
   displays[1].own = true;
   video_driver_publish_window_displays(displays, 2);
}

/* The Presentation moved to another display while still wanted: what
 * it drew on the old display, or on the new one before the move, is
 * not shown until it draws from the snapshot that moved it */
static void test_window_shown_moved(void)
{
   video_window_display_t displays[3];
   settings_t *settings       = config_get_ptr();
   struct menu_state *menu_st = menu_state_get_ptr();
   uint16_t saved_menu        = menu_st->flags;
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Screen-2");
   window_test_display(&displays[2], 3, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Screen-3");
   menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
   window_test_begin();
   video_driver_publish_window_displays(displays, 3);
   /* Game Screen 2 on Auto: the second panel */
   window_test_run();
   CHECK(window_test_seen.display == 2);
   window_test_draw(1);
   CHECK(video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL) == 2);
   /* Moved to display 3: the newest layout is on display 2 */
   strlcpy(settings->arrays.video_screen_display[1], "Screen-3",
         sizeof(settings->arrays.video_screen_display[1]));
   window_test_run();
   CHECK(window_test_seen.display == 3);
   CHECK(window_test_seen.window_screens[VIDEO_WINDOWS_PRESENTATION]
         == 2u);
   CHECK(!video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL));
   window_test_draw(2);
   CHECK(video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL) == 3);
   /* Back to 2 and on to 3 again before it draws: the layout on 3 is
    * from before the move */
   settings->arrays.video_screen_display[1][0] = '\0';
   window_test_run();
   CHECK(window_test_seen.display == 2);
   strlcpy(settings->arrays.video_screen_display[1], "Screen-3",
         sizeof(settings->arrays.video_screen_display[1]));
   window_test_run();
   CHECK(window_test_seen.display == 3);
   CHECK(window_test_seen.window_screens[VIDEO_WINDOWS_PRESENTATION]
         == 2u);
   CHECK(!video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL));
   window_test_draw(4);
   CHECK(video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL) == 3);
   settings->arrays.video_screen_display[1][0] = '\0';
   menu_st->flags = saved_menu;
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 0, 0,
         NULL, NULL, 0, 0);
   window_test_end();
   /* The list the later tests expect */
   displays[0].own = false;
   displays[1].own = true;
   video_driver_publish_window_displays(displays, 2);
}

/* With windows on, a frame has them without content views too; each UI
 * layer's window and size, the wait for the Presentation, and the
 * accessors */
static void test_window_ui_targets(void)
{
   video_window_display_t displays[2];
   video_views_layout_t layout;
   unsigned dims;
   unsigned ui;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   uint16_t saved_menu            = menu_st->flags;
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Screen-2");
   displays[1].dims = VIDEO_SCALE_PACK(1240, 1080);
   window_test_begin();
   ui = video_driver_get_ui_dims();
   video_driver_publish_window_displays(displays, 2);
   /* A game without a map, the menu following Game Screen 2: closed, it
    * lays out for the bottom panel and nothing is shown there */
   video_driver_clear_views();
   settings->uints.video_menu_follows = 1;
   menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(window_test_handed != NULL);
   CHECK(window_test_seen.display == 0);
   CHECK(!window_test_seen.window_ui[VIDEO_WINDOWS_PRESENTATION]);
   CHECK(video_driver_get_ui_layer_window(VIDEO_WINDOW_UI_MENU)
         == VIDEO_WINDOWS_PRESENTATION);
   CHECK(video_driver_get_ui_layer_dims(VIDEO_WINDOW_UI_MENU)
         == VIDEO_SCALE_PACK(1240, 1080));
   CHECK(video_driver_get_ui_layer_window(VIDEO_WINDOW_UI_NOTIFICATIONS)
         == 0);
   CHECK(video_driver_get_ui_layer_dims(VIDEO_WINDOW_UI_NOTIFICATIONS)
         == ui);
   /* It opens: the Presentation is wanted for the menu alone, which
    * waits for it */
   menu_st->flags |= MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(window_test_seen.window_ui[VIDEO_WINDOWS_PRESENTATION]
         == VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU));
   CHECK(window_test_seen.window_screens[VIDEO_WINDOWS_PRESENTATION]
         == 0u);
   CHECK(window_test_seen.ui[VIDEO_WINDOW_UI_MENU].window
         == VIDEO_WINDOWS_PRESENTATION);
   CHECK(window_test_seen.ui[VIDEO_WINDOW_UI_MENU].wait);
   CHECK(video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   /* Its window drew there: the menu lays out at its client size, and
    * the snapshot carries what the frame was laid out with */
   video_driver_window_layout_screens(&window_test_seen, 0,
         VIDEO_SCALE_PACK(1000, 900), &layout);
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 7, 2,
         &layout, &window_test_seen.core, window_test_seen.core_dims,
         VIDEO_SCALE_PACK(1000, 900));
   CHECK(video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, &dims)
         == 2);
   CHECK(dims == VIDEO_SCALE_PACK(1000, 900));
   window_test_run();
   CHECK(!video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   CHECK(video_driver_get_ui_layer_dims(VIDEO_WINDOW_UI_MENU)
         == VIDEO_SCALE_PACK(1000, 900));
   CHECK(window_test_seen.ui[VIDEO_WINDOW_UI_MENU].wait);
   /* The Presentation goes: the menu is the main window's at once */
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 0, 0,
         NULL, NULL, 0, 0);
   window_test_run();
   CHECK(!video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL));
   CHECK(video_driver_get_ui_layer_window(VIDEO_WINDOW_UI_MENU) == 0);
   CHECK(video_driver_get_ui_layer_dims(VIDEO_WINDOW_UI_MENU) == ui);
   /* Closed and opened again, it waits for the new one; past the wait
    * the main window has it */
   menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(window_test_seen.display == 0);
   CHECK(!video_st->windows_ui_asked);
   menu_st->flags |= MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   video_st->windows_ui_asked -= VIDEO_WINDOWS_UI_WAIT_USEC;
   window_test_run();
   CHECK(video_driver_get_ui_layer_window(VIDEO_WINDOW_UI_MENU) == 0);
   CHECK(!video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   /* With the Menu, notifications go with it */
   settings->uints.video_notifications_window =
      VIDEO_NOTIFICATIONS_WITH_MENU;
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 8, 2,
         &layout, &window_test_seen.core, window_test_seen.core_dims,
         VIDEO_SCALE_PACK(1000, 900));
   window_test_run();
   CHECK(window_test_seen.window_ui[VIDEO_WINDOWS_PRESENTATION]
         == (VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU)
            | VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_NOTIFICATIONS)));
   CHECK(video_driver_get_ui_layer_window(VIDEO_WINDOW_UI_NOTIFICATIONS)
         == VIDEO_WINDOWS_PRESENTATION);
   settings->uints.video_notifications_window =
      VIDEO_NOTIFICATIONS_WITH_GAME;
   /* Windows off: the main window has everything, and placement starts
    * afresh */
   settings->bools.video_windows_enable = false;
   window_test_run();
   CHECK(window_test_handed == NULL);
   CHECK(video_driver_get_ui_layer_window(VIDEO_WINDOW_UI_MENU) == 0);
   CHECK(video_driver_get_ui_layer_dims(VIDEO_WINDOW_UI_MENU) == ui);
   CHECK(!video_st->windows_host.names);
   settings->uints.video_menu_follows = 0;
   menu_st->flags                     = saved_menu;
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 0, 0,
         NULL, NULL, 0, 0);
   window_test_end();
}

/* A Presentation made on another display waits for itself: the menu
 * stays laid out for it instead of falling back at once */
static void test_window_ui_wait_moved(void)
{
   video_window_display_t displays[3];
   video_views_layout_t layout;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   uint16_t saved_menu            = menu_st->flags;
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Screen-2");
   window_test_display(&displays[2], 3, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Screen-3");
   displays[1].dims = VIDEO_SCALE_PACK(1240, 1080);
   window_test_begin();
   video_driver_publish_window_displays(displays, 3);
   video_driver_clear_views();
   settings->uints.video_menu_follows = 1;
   menu_st->flags |= MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   video_driver_window_layout_screens(&window_test_seen, 0,
         VIDEO_SCALE_PACK(1000, 900), &layout);
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 7, 2,
         &layout, &window_test_seen.core, window_test_seen.core_dims,
         VIDEO_SCALE_PACK(1000, 900));
   window_test_run();
   CHECK(!video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   strlcpy(settings->arrays.video_screen_display[1], "Screen-3",
         sizeof(settings->arrays.video_screen_display[1]));
   window_test_run();
   CHECK(window_test_seen.display == 3);
   CHECK(video_driver_get_ui_layer_window(VIDEO_WINDOW_UI_MENU)
         == VIDEO_WINDOWS_PRESENTATION);
   CHECK(video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   settings->arrays.video_screen_display[1][0] = '\0';
   settings->uints.video_menu_follows          = 0;
   menu_st->flags                              = saved_menu;
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 0, 0,
         NULL, NULL, 0, 0);
   /* The list test_window_ui_targets() left for the later tests */
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   window_test_end();
}

static video_views_layout_t window_test_layouts[2];
static video_views_map_t window_test_map;
static unsigned window_test_generation;
#ifdef HAVE_THREADS
/* The publisher starts once the reader reads, and says when it is done:
 * the two overlap. The test may spin on them. */
static retro_atomic_int_t window_test_reading;
static retro_atomic_int_t window_test_published;
#endif

/* Publishes window 1's layouts, alternating, as a renderer does */
static void window_test_publish(void *data)
{
   unsigned count = *(const unsigned*)data;
   unsigned i;
#ifdef HAVE_THREADS
   while (!retro_atomic_load_acquire_int(&window_test_reading))
      ;
#endif
   for (i = 0; i < count; i++)
      video_driver_publish_window_layout(1, window_test_generation, 2,
            &window_test_layouts[i & 1], &window_test_map,
            VIDEO_SCALE_PACK(800, 480), VIDEO_SCALE_PACK(800, 480));
#ifdef HAVE_THREADS
   retro_atomic_store_release_int(&window_test_published, 1);
#endif
}

/* A libretro position back to the nearest pixel of a frame @size wide */
static int window_test_pixel(int16_t v, unsigned size)
{
   return (int)((((long)v + 0x8000L) * (long)(size - 1) + 0x7fffL)
         / 0xffffL);
}

static void test_window_pointer_mapping(void)
{
   static const int points[][2] = {
      {0, 0}, {400, 240}, {799, 479}, {-5, 100}, {900, 600}, {120, 30}
   };
   const uint32_t oob = VIDEO_POS_PACK(-0x8000, -0x8000);
   settings_t *settings = config_get_ptr();
   video_windows_frame_t snap;
   struct retro_video_view v[3];
   struct video_viewport vp;
   uint32_t pos, main_pos, main_screen, win_pos;
   unsigned gen, i, count = 1;
   unsigned saved_index[2];

   memset(&snap, 0, sizeof(snap));
   memset(v, 0, sizeof(v));
   /* 3DS: the upper screen's eyes side by side, the lower one centred
    * under them, in an 800x480 frame */
   v[0].width = v[1].width = 400;
   v[0].height = v[1].height = 240;
   v[1].x      = 400;
   v[0].eye    = RETRO_VIDEO_VIEW_EYE_LEFT;
   v[1].eye    = RETRO_VIDEO_VIEW_EYE_RIGHT;
   v[2].x      = 240;
   v[2].y      = 240;
   v[2].width  = 320;
   v[2].height = 240;
   v[2].screen = 1;
   CHECK(video_views_validate(v, 3, &snap.core));
   snap.core_dims = VIDEO_SCALE_PACK(800, 480);
   /* The lower screen's window, 800x480 client pixels drawn at 1600x960
    * on a 2x display, and at 800x480 on a 1x one */
   video_driver_window_layout(&snap, 1, VIDEO_SCALE_PACK(1600, 960),
         &window_test_layouts[0]);
   video_driver_window_layout(&snap, 1, VIDEO_SCALE_PACK(800, 480),
         &window_test_layouts[1]);
   CHECK(window_test_layouts[0].num_placements == 1);
   CHECK(window_test_layouts[0].placements[0].view == 2);
   window_test_map        = snap.core;
   gen                    = video_driver_window_generation();
   window_test_generation = gen;
   CHECK(gen != 0);
   CHECK(!video_driver_translate_window_point(1, gen, 400, 240, true, &pos));

#ifdef HAVE_THREADS
   {
      retro_time_t until;
      sthread_t *thread;
      retro_atomic_store_release_int(&window_test_reading, 1);
      thread = sthread_create(window_test_publish, &count);
      CHECK(thread != NULL);
      /* Seen through the mailbox alone, before the join */
      until = cpu_features_get_time_usec() + 2000000;
      while (     thread
            && !video_driver_translate_window_point(1, gen, 400, 240,
                  true, &pos)
            && cpu_features_get_time_usec() < until)
         ;
      CHECK(video_driver_translate_window_point(1, gen, 400, 240, true, &pos));
      if (thread)
         sthread_join(thread);
   }
#else
   window_test_publish(&count);
#endif
   /* The centre of the window is the centre of the lower screen */
   CHECK(video_driver_translate_window_point(1, gen, 400, 240, true, &pos));
   CHECK(window_test_pixel(VIDEO_POS_X(pos), 800) >= 240);
   CHECK(window_test_pixel(VIDEO_POS_X(pos), 800) < 560);
   CHECK(window_test_pixel(VIDEO_POS_Y(pos), 480) >= 240);
   CHECK(window_test_pixel(VIDEO_POS_Y(pos), 480) < 480);
   CHECK(window_test_pixel(VIDEO_POS_X(pos), 800) == 400);
   CHECK(window_test_pixel(VIDEO_POS_Y(pos), 480) == 360);
   win_pos = pos;
   /* The letterbox: off-screen when asked, else the nearest edge */
   CHECK(video_driver_translate_window_point(1, gen, 20, 240, true, &pos));
   CHECK(pos == oob);
   CHECK(video_driver_translate_window_point(1, gen, 20, 240, false, &pos));
   CHECK(window_test_pixel(VIDEO_POS_X(pos), 800) == 240);
   /* Client pixels: x 100 is 200 of the 1600 drawn, on the screen */
   CHECK(video_driver_translate_window_point(1, gen, 100, 240, true, &pos));
   CHECK(pos != oob);
   CHECK(window_test_pixel(VIDEO_POS_X(pos), 800) == 250);
   /* Another generation, another window, no window */
   CHECK(!video_driver_translate_window_point(1, gen + 1, 400, 240, true, &pos));
   CHECK(!video_driver_translate_window_point(1, 0, 400, 240, true, &pos));
   CHECK(!video_driver_translate_window_point(2, gen, 400, 240, true, &pos));
   CHECK(!video_driver_translate_window_point(RETRO_VIDEO_VIEWS_MAX, gen,
            400, 240, true, &pos));

   /* The main window: the viewport's translation, with views or not */
   window_test_begin();
   CHECK(video_driver_get_viewport_info(&vp));
   for (i = 0; i < 2 * ARRAY_SIZE(points); i++)
   {
      int x = points[i % ARRAY_SIZE(points)][0];
      int y = points[i % ARRAY_SIZE(points)][1];
      bool oob_asked = i >= ARRAY_SIZE(points);
      CHECK(video_driver_translate_coord_viewport(&vp, x, y,
               &main_pos, &main_screen, oob_asked));
      CHECK(video_driver_translate_window_point(0, 0, x, y, oob_asked, &pos));
      CHECK(pos == main_pos);
      CHECK(video_driver_translate_window_point(0, gen, x, y, oob_asked, &pos));
      CHECK(pos == main_pos);
   }
   window_test_run();
   CHECK(window_test_placements() == 1);
   for (i = 0; i < ARRAY_SIZE(points); i++)
   {
      CHECK(video_driver_translate_coord_viewport(&vp, points[i][0],
               points[i][1], &main_pos, &main_screen, true));
      CHECK(video_driver_translate_window_point(0, 0, points[i][0],
               points[i][1], true, &pos));
      CHECK(pos == main_pos);
   }

   /* Published devices: each placed in its own window */
   {
      input_pointer_frame_t frames[2];
      input_video_window_t windows[2];
      uint32_t touch = VIDEO_POS_PACK(400, 240);
      memset(frames, 0, sizeof(frames));
      memset(windows, 0, sizeof(windows));
      frames[0].pos       = VIDEO_POS_PACK(400, 240);
      frames[0].rel       = VIDEO_POS_PACK(3, -4);
      frames[0].buttons   = INPUT_POINTER_LEFT;
      frames[1].pos       = VIDEO_POS_PACK(400, 240);
      frames[1].rel       = VIDEO_POS_PACK(5, 6);
      frames[1].buttons   = INPUT_POINTER_RIGHT;
      windows[1].window     = 1;
      windows[1].generation = gen;
      CHECK(video_driver_translate_coord_viewport(&vp, 400, 240,
               &main_pos, &main_screen, false));
      input_driver_publish_window_pointers(frames, windows, 2, 0);
      CHECK(input_driver_device_pos(0, RETRO_DEVICE_POINTER, 0) == main_pos);
      CHECK(input_driver_device_pos(0, RARCH_DEVICE_POINTER_SCREEN, 0)
            == main_screen);
      CHECK(input_driver_device_pos(0, RARCH_DEVICE_MOUSE_SCREEN, 0)
            == VIDEO_POS_PACK(400, 240));
      CHECK(input_driver_device_pos(1, RETRO_DEVICE_POINTER, 0) == win_pos);
      CHECK(input_driver_device_state(1, RETRO_DEVICE_LIGHTGUN, 0,
               RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) == VIDEO_POS_X(win_pos));
      CHECK(input_driver_device_state(1, RETRO_DEVICE_LIGHTGUN, 0,
               RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 0);
      CHECK(input_driver_device_state(1, RETRO_DEVICE_POINTER, 0,
               RETRO_DEVICE_ID_POINTER_PRESSED) == 0);
      /* Motion and buttons are the device's, whatever the window */
      CHECK(input_driver_device_state(1, RETRO_DEVICE_MOUSE, 0,
               RETRO_DEVICE_ID_MOUSE_X) == 5);
      CHECK(input_driver_device_state(1, RETRO_DEVICE_MOUSE, 0,
               RETRO_DEVICE_ID_MOUSE_Y) == 6);
      CHECK(input_driver_device_state(1, RETRO_DEVICE_MOUSE, 0,
               RETRO_DEVICE_ID_MOUSE_RIGHT) == 1);
      CHECK(input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0,
               RETRO_DEVICE_ID_MOUSE_X) == 3);
      /* No place on the main window's screen */
      CHECK(input_driver_device_pos(1, RARCH_DEVICE_MOUSE_SCREEN, 0) == oob);
      CHECK(input_driver_device_pos(1, RARCH_DEVICE_POINTER_SCREEN, 0) == oob);

      /* By Mouse Index, port 0 reads device 1 */
      saved_index[0] = settings->uints.input_mouse_index[0];
      saved_index[1] = settings->uints.input_mouse_index[1];
      settings->uints.input_mouse_index[0] = 1;
      settings->uints.input_mouse_index[1] = 0;
      input_driver_publish_window_pointers(frames, windows, 2,
            INPUT_POINTERS_BY_MOUSE_INDEX);
      CHECK(input_driver_device_pos(0, RETRO_DEVICE_POINTER, 0) == win_pos);
      CHECK(input_driver_device_pos(1, RETRO_DEVICE_POINTER, 0) == main_pos);
      CHECK(input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0,
               RETRO_DEVICE_ID_MOUSE_X) == 5);
      CHECK(input_driver_device_state(0, RETRO_DEVICE_MOUSE, 0,
               RETRO_DEVICE_ID_MOUSE_RIGHT) == 1);
      settings->uints.input_mouse_index[0] = saved_index[0];
      settings->uints.input_mouse_index[1] = saved_index[1];

      /* The main window's call: every device in the main window */
      input_driver_publish_pointers(frames, 2, 0);
      CHECK(input_driver_device_pos(1, RETRO_DEVICE_POINTER, 0) == main_pos);

      /* A window gone: off-screen, never a place in the main window */
      windows[1].generation = gen + 1;
      input_driver_publish_window_pointers(frames, windows, 2, 0);
      CHECK(input_driver_device_pos(1, RETRO_DEVICE_POINTER, 0) == oob);
      CHECK(input_driver_device_state(1, RETRO_DEVICE_POINTER, 0,
               RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN) == 1);
      CHECK(input_driver_device_state(1, RETRO_DEVICE_LIGHTGUN, 0,
               RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) == 1);
      CHECK(input_driver_device_state(1, RETRO_DEVICE_MOUSE, 0,
               RETRO_DEVICE_ID_MOUSE_X) == 5);

      /* A touch in the screen's window, and the same in the main one */
      windows[1].generation = gen;
      input_driver_publish_window_touches(&touch, &windows[1], 1, 1, 1);
      CHECK(input_driver_device_pos(0, RETRO_DEVICE_POINTER, 0) == win_pos);
      CHECK(input_driver_device_state(0, RETRO_DEVICE_POINTER, 0,
               RETRO_DEVICE_ID_POINTER_PRESSED) == 1);
      CHECK(input_driver_device_pos(0, RARCH_DEVICE_POINTER_SCREEN, 0) == oob);
      input_driver_publish_touches(&touch, 1, 1, 1);
      CHECK(input_driver_device_pos(0, RETRO_DEVICE_POINTER, 0) == main_pos);
      input_driver_publish_pointers(frames, 0, 0);
      input_driver_publish_touches(&touch, 0, 0, 0);
   }
   window_test_end();

#ifdef HAVE_THREADS
   /* Latest wins while the main thread reads: never a torn layout */
   {
      sthread_t *thread;
      unsigned torn  = 0;
      unsigned reads = 0;
      count = 20000;
      retro_atomic_store_release_int(&window_test_reading, 0);
      retro_atomic_store_release_int(&window_test_published, 0);
      thread = sthread_create(window_test_publish, &count);
      CHECK(thread != NULL);
      while (thread && !retro_atomic_load_acquire_int(&window_test_published))
      {
         if (     video_driver_translate_window_point(1, gen, 400, 240,
                     true, &pos)
               && pos != win_pos)
            torn++;
         if (!reads++)
            retro_atomic_store_release_int(&window_test_reading, 1);
      }
      if (thread)
         sthread_join(thread);
      CHECK(torn == 0);
      CHECK(reads > 1);
   }
#endif

   /* A window retired */
   video_driver_publish_window_layout(1, gen, 0, NULL, NULL, 0, 0);
   CHECK(!video_driver_translate_window_point(1, gen, 400, 240, true, &pos));
}

/* Device @i's left button and motion, and the touches down in a list */
#define WINDOW_TEST_LEFT(i) input_driver_device_state(i, RETRO_DEVICE_MOUSE, \
      0, RETRO_DEVICE_ID_MOUSE_LEFT)
#define WINDOW_TEST_MOTION(i) input_driver_device_state(i, \
      RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X)
#define WINDOW_TEST_TOUCHES(device) input_driver_device_state(0, device, \
      0, RETRO_DEVICE_ID_POINTER_COUNT)

/* Input from a screen's window that goes is let go; the main window's
 * is kept */
static void test_window_pointer_release(void)
{
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   input_pointer_frame_t frames[2];
   input_video_window_t windows[2];
   uint32_t touches[2];
   unsigned off;

   memset(frames, 0, sizeof(frames));
   memset(windows, 0, sizeof(windows));
   frames[0].pos         = VIDEO_POS_PACK(10, 10);
   frames[0].rel         = VIDEO_POS_PACK(3, -4);
   frames[0].buttons     = INPUT_POINTER_LEFT;
   frames[1]             = frames[0];
   windows[1].window     = 1;
   windows[1].generation = video_driver_window_generation();
   touches[0]            = VIDEO_POS_PACK(10, 10);
   touches[1]            = VIDEO_POS_PACK(10, 10);

   window_test_begin();
   /* Display Off for Screen 2, then the windows off */
   for (off = 0; off < 2; off++)
   {
      settings->arrays.video_screen_display[1][0] = '\0';
      settings->bools.video_windows_enable        = true;
      window_test_run();
      CHECK(video_st->windows_wanted == 2u);
      input_driver_publish_window_pointers(frames, windows, 2,
            INPUT_POINTERS_WINDOWS_SECOND);
      input_driver_publish_window_touches(touches, windows, 2, 3u, 3u);
      CHECK(WINDOW_TEST_LEFT(1) == 1);
      CHECK(WINDOW_TEST_MOTION(1) == 3);
      CHECK(WINDOW_TEST_TOUCHES(RARCH_DEVICE_POINTER_SECOND) == 1);

      if (off)
         settings->bools.video_windows_enable = false;
      else
         strlcpy(settings->arrays.video_screen_display[1],
               VIDEO_WINDOW_DISPLAY_OFF,
               sizeof(settings->arrays.video_screen_display[1]));
      window_test_run();
      CHECK(video_st->windows_wanted == 0);
      CHECK(WINDOW_TEST_LEFT(1) == 0);
      CHECK(WINDOW_TEST_MOTION(1) == 0);
      CHECK(WINDOW_TEST_TOUCHES(RARCH_DEVICE_POINTER_SECOND) == 0);
      CHECK(WINDOW_TEST_LEFT(0) == 1);
      CHECK(WINDOW_TEST_MOTION(0) == 3);
      CHECK(WINDOW_TEST_TOUCHES(RETRO_DEVICE_POINTER) == 1);
   }
   settings->arrays.video_screen_display[1][0] = '\0';
   input_driver_publish_pointers(frames, 0, 0);
   input_driver_publish_touches(touches, 0, 0, 0);
   window_test_end();
}

/* Game Screen 2's window turned: drawn again at the swapped size, the menu
 * lays out at that size */
static void test_window_turned(void)
{
   video_window_display_t displays[2];
   video_views_layout_t layout;
   unsigned dims;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   uint16_t saved_menu            = menu_st->flags;
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Screen-2");
   displays[1].dims = VIDEO_SCALE_PACK(1240, 1080);
   window_test_begin();
   video_driver_publish_window_displays(displays, 2);
   /* The menu open in Game Screen 2's window, drawn at 1240x1080 */
   video_driver_clear_views();
   settings->uints.video_menu_follows = 1;
   menu_st->flags |= MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(window_test_seen.display == 2);
   video_driver_window_layout_screens(&window_test_seen, 0,
         VIDEO_SCALE_PACK(1240, 1080), &layout);
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 7, 2,
         &layout, &window_test_seen.core, window_test_seen.core_dims,
         VIDEO_SCALE_PACK(1240, 1080));
   window_test_run();
   CHECK(video_driver_get_ui_layer_dims(VIDEO_WINDOW_UI_MENU)
         == VIDEO_SCALE_PACK(1240, 1080));
   /* Turned: the same window drawn at 1080x1240 */
   video_driver_window_layout_screens(&window_test_seen, 0,
         VIDEO_SCALE_PACK(1080, 1240), &layout);
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 7, 2,
         &layout, &window_test_seen.core, window_test_seen.core_dims,
         VIDEO_SCALE_PACK(1080, 1240));
   CHECK(video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, &dims)
         == 2);
   CHECK(dims == VIDEO_SCALE_PACK(1080, 1240));
   window_test_run();
   CHECK(video_driver_get_ui_layer_window(VIDEO_WINDOW_UI_MENU)
         == VIDEO_WINDOWS_PRESENTATION);
   CHECK(video_driver_get_ui_layer_dims(VIDEO_WINDOW_UI_MENU)
         == VIDEO_SCALE_PACK(1080, 1240));
   CHECK(!video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   /* The next frame is laid out at it */
   window_test_run();
   CHECK(window_test_seen.ui[VIDEO_WINDOW_UI_MENU].dims
         == VIDEO_SCALE_PACK(1080, 1240));
   settings->uints.video_menu_follows = 0;
   menu_st->flags                     = saved_menu;
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 0, 0,
         NULL, NULL, 0, 0);
   window_test_end();
}

/* A touch on a turned window maps through the layout drawn at the new
 * size, from the first read after it was published */
static void test_window_turned_touch(void)
{
   const uint32_t oob = VIDEO_POS_PACK(-0x8000, -0x8000);
   video_windows_frame_t snap;
   struct retro_video_view v[3];
   video_views_layout_t wide;
   video_views_layout_t tall;
   const video_views_rect_t *dst = &tall.placements[0].dst;
   uint32_t pos;
   unsigned gen;
   int x, y;
   memset(&snap, 0, sizeof(snap));
   memset(v, 0, sizeof(v));
   /* The 3DS map of test_window_pointer_mapping(): the lower screen,
    * 320x240 at (240, 240) of an 800x480 frame */
   v[0].width = v[1].width = 400;
   v[0].height = v[1].height = 240;
   v[1].x      = 400;
   v[0].eye    = RETRO_VIDEO_VIEW_EYE_LEFT;
   v[1].eye    = RETRO_VIDEO_VIEW_EYE_RIGHT;
   v[2].x      = 240;
   v[2].y      = 240;
   v[2].width  = 320;
   v[2].height = 240;
   v[2].screen = 1;
   CHECK(video_views_validate(v, 3, &snap.core));
   snap.core_dims = VIDEO_SCALE_PACK(800, 480);
   video_driver_window_layout(&snap, 1, VIDEO_SCALE_PACK(1240, 1080),
         &wide);
   video_driver_window_layout(&snap, 1, VIDEO_SCALE_PACK(1080, 1240),
         &tall);
   CHECK(wide.num_placements == 1 && tall.num_placements == 1);
   /* Above the tall layout's screen, on the wide one's */
   x = (int)VIDEO_SCALE_W(dst->dims) / 2;
   y = VIDEO_POS_Y(dst->pos) / 2;
   CHECK(y > VIDEO_POS_Y(wide.placements[0].dst.pos));
   gen = video_driver_window_generation();
   video_driver_publish_window_layout(1, gen, 2, &wide, &snap.core,
         snap.core_dims, VIDEO_SCALE_PACK(1240, 1080));
   /* Drawn there, and seen by a frame */
   CHECK(video_driver_window_shown(1, NULL) == 2);
   CHECK(video_driver_translate_window_point(1, gen, x, y, true, &pos));
   CHECK(pos != oob);
   /* Turned, and read before any frame looks */
   video_driver_publish_window_layout(1, gen, 2, &tall, &snap.core,
         snap.core_dims, VIDEO_SCALE_PACK(1080, 1240));
   CHECK(video_driver_translate_window_point(1, gen, x, y, true, &pos));
   CHECK(pos == oob);
   x = VIDEO_POS_X(dst->pos) + (int)VIDEO_SCALE_W(dst->dims) / 2;
   y = VIDEO_POS_Y(dst->pos) + (int)VIDEO_SCALE_H(dst->dims) / 2;
   CHECK(video_driver_translate_window_point(1, gen, x, y, true, &pos));
   CHECK(window_test_pixel(VIDEO_POS_X(pos), 800) == 400);
   CHECK(window_test_pixel(VIDEO_POS_Y(pos), 480) == 360);
   video_driver_publish_window_layout(1, gen, 0, NULL, NULL, 0, 0);
   CHECK(!video_driver_translate_window_point(1, gen, x, y, true, &pos));
}

/* A display turned: the list again with its size swapped, the names
 * the same. Placement goes by name, so nothing moves, and a move
 * RetroArch gave up is not asked again. Cocoon's start on the bottom
 * panel, as in test_window_moves(). */
static void test_window_turned_display(void)
{
   video_window_display_t displays[2];
   video_views_layout_t layout;
   unsigned display, separate, main_mask, host;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Screen-2");
   displays[0].dims = VIDEO_SCALE_PACK(1920, 1080);
   displays[1].dims = VIDEO_SCALE_PACK(1240, 1080);
   settings->uints.video_menu_follows = 1;
   window_test_begin();
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   video_driver_window_layout_screens(&window_test_seen, 2u,
         VIDEO_SCALE_PACK(1240, 1080), &layout);
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 1, 2,
         &layout, &window_test_seen.core, window_test_seen.core_dims,
         VIDEO_SCALE_PACK(1240, 1080));
   window_test_run();
   CHECK(video_st->windows_host.moving == 1);
   /* The move never lands and is given up */
   video_st->windows_host.asked -= VIDEO_WINDOWS_MOVE_USEC;
   window_test_run();
   CHECK(!video_st->windows_host.moving);
   display   = window_test_seen.display;
   separate  = window_test_seen.plan.separate_mask;
   main_mask = window_test_seen.plan.main_mask;
   host      = video_st->windows_host.host;
   /* The top panel turns */
   displays[0].dims = VIDEO_SCALE_PACK(1080, 1920);
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(!video_st->windows_host.moving);
   CHECK(video_st->windows_host.host == host);
   CHECK(window_test_seen.display == display);
   CHECK(window_test_seen.plan.separate_mask == separate);
   CHECK(window_test_seen.plan.main_mask == main_mask);
   settings->uints.video_menu_follows = 0;
   video_driver_publish_window_layout(VIDEO_WINDOWS_PRESENTATION, 0, 0,
         NULL, NULL, 0, 0);
   window_test_end();
   /* The list the later tests expect */
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Screen-2");
   displays[1].dims = VIDEO_SCALE_PACK(1240, 1080);
   video_driver_publish_window_displays(displays, 2);
}

/* Game Screen 2's display turned while the menu waits for its window: the
 * menu lays out at the size listed now */
static void test_window_turned_waiting(void)
{
   video_window_display_t displays[2];
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   uint16_t saved_menu            = menu_st->flags;
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Screen-2");
   displays[1].dims = VIDEO_SCALE_PACK(1240, 1080);
   window_test_begin();
   video_driver_publish_window_displays(displays, 2);
   /* The menu opens for Game Screen 2, whose window has not drawn */
   video_driver_clear_views();
   settings->uints.video_menu_follows = 1;
   menu_st->flags &= ~MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(!video_st->windows_ui_asked);
   menu_st->flags |= MENU_ST_FLAG_ALIVE;
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(!video_driver_window_shown(VIDEO_WINDOWS_PRESENTATION, NULL));
   CHECK(video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   CHECK(video_driver_get_ui_layer_dims(VIDEO_WINDOW_UI_MENU)
         == VIDEO_SCALE_PACK(1240, 1080));
   /* Game Screen 2 turns while the menu waits; the test restamps the wait,
    * so the clock cannot run it out */
   displays[1].dims = VIDEO_SCALE_PACK(1080, 1240);
   video_driver_publish_window_displays(displays, 2);
   video_st->windows_ui_asked = 0;
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(video_st->windows_ui[VIDEO_WINDOW_UI_MENU].wait);
   CHECK(video_driver_get_ui_layer_window(VIDEO_WINDOW_UI_MENU)
         == VIDEO_WINDOWS_PRESENTATION);
   CHECK(video_driver_get_ui_layer_dims(VIDEO_WINDOW_UI_MENU)
         == VIDEO_SCALE_PACK(1080, 1240));
   /* The next frame is laid out at it */
   window_test_run();
   CHECK(window_test_seen.ui[VIDEO_WINDOW_UI_MENU].dims
         == VIDEO_SCALE_PACK(1080, 1240));
   /* The list the later tests expect, taken while windows are on */
   displays[1].dims = VIDEO_SCALE_PACK(1240, 1080);
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(!retro_atomic_load_acquire_int(
         &video_st->window_displays.published));
   /* Windows off, so the tests after start afresh */
   settings->bools.video_windows_enable = false;
   window_test_run();
   CHECK(!video_st->windows_ui_asked);
   settings->uints.video_menu_follows = 0;
   menu_st->flags                     = saved_menu;
   window_test_end();
}

#ifdef HAVE_THREADS
#define WINDOW_TEST_GENERATIONS 50000
static unsigned window_test_gens[2 * WINDOW_TEST_GENERATIONS];

static void window_test_allocate(void *data)
{
   unsigned *out = (unsigned*)data;
   unsigned i;
   for (i = 0; i < WINDOW_TEST_GENERATIONS; i++)
      out[i] = video_driver_window_generation();
}

static int window_test_compare(const void *a, const void *b)
{
   unsigned x = *(const unsigned*)a;
   unsigned y = *(const unsigned*)b;
   return (x > y) - (x < y);
}
#endif

static void test_window_generation(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   int saved;
#ifdef HAVE_THREADS
   {
      unsigned i, repeats = 0;
      sthread_t *thread = sthread_create(window_test_allocate,
            window_test_gens + WINDOW_TEST_GENERATIONS);
      CHECK(thread != NULL);
      window_test_allocate(window_test_gens);
      if (thread)
         sthread_join(thread);
      qsort(window_test_gens, 2 * WINDOW_TEST_GENERATIONS,
            sizeof(window_test_gens[0]), window_test_compare);
      for (i = 0; i < 2 * WINDOW_TEST_GENERATIONS; i++)
         if (!window_test_gens[i]
               || (i && window_test_gens[i] == window_test_gens[i - 1]))
            repeats++;
      CHECK(repeats == 0);
   }
#else
   {
      unsigned a = video_driver_window_generation();
      unsigned b = video_driver_window_generation();
      CHECK(a && b && a != b);
   }
#endif
   saved = retro_atomic_load_acquire_int(&video_st->window_generation);
   retro_atomic_store_release_int(&video_st->window_generation, INT_MAX - 1);
   CHECK(video_driver_window_generation() == 0);
   CHECK(video_driver_window_generation() == 0);
   CHECK(retro_atomic_load_acquire_int(&video_st->window_generation) > 0);
   retro_atomic_store_release_int(&video_st->window_generation, saved);
}

static video_window_common_t *window_test_kept;

static video_window_common_t *window_test_kept_at(unsigned screen)
{
   return screen == 1 ? window_test_kept : NULL;
}

/* A close and a move no poll had drained when the window was kept
 * reach the driver that takes it; focus the window system gave it
 * meanwhile does not */
static void test_window_kept_pending(void)
{
   video_window_event_t events[8];
   video_window_common_t common;
   video_window_native_t native, taken;
   unsigned i, count;
   unsigned seen = 0;
   int owner     = 0;
   memset(&common, 0, sizeof(common));
   memset(&native, 0, sizeof(native));
   memset(&taken, 0, sizeof(taken));
   common.owner              = &owner;
   common.native             = &native;
   common.request.screen     = 1;
   common.request.generation = 1;
   native.state              = &common;
   native.focused            = true;
   native.presentable        = true;
   common.pending           |= VIDEO_WINDOW_EVENT_BIT(GEOMETRY);
   video_window_mark_closed_by_user(&common);
   video_window_hold(&common, &native);
   common.pending           |= VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN);
   video_window_adopt(&common, &owner, &taken);
   window_test_kept          = &common;
   count = video_windows_drain(&owner, window_test_kept_at, events,
         ARRAY_SIZE(events));
   window_test_kept          = NULL;
   for (i = 0; i < count; i++)
      seen |= 1u << events[i].kind;
   CHECK(seen & VIDEO_WINDOW_EVENT_BIT(CLOSE));
   CHECK(seen & VIDEO_WINDOW_EVENT_BIT(GEOMETRY));
   CHECK(!(seen & VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN)));
   CHECK(taken.state == &common && !taken.focused);
}

/* The Display rows replaced the mode and monitor rows */
static void test_window_menu_settings(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   rarch_setting_t *enable;
   unsigned i, restore;
   char name[64];
   load_fixture("");
   for (i = 1; i <= RETRO_VIDEO_VIEWS_MAX; i++)
   {
      snprintf(name, sizeof(name), "video_screen%u_window_mode", i);
      CHECK(menu_setting_find(name) == NULL);
      snprintf(name, sizeof(name), "video_screen%u_window_monitor", i);
      CHECK(menu_setting_find(name) == NULL);
      snprintf(name, sizeof(name), "video_screen%u_display", i);
      CHECK(menu_setting_find(name) != NULL);
   }
   enable = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_WINDOWS_ENABLE);
   CHECK(enable != NULL);
   if (!enable)
      return;
   /* A change brings failed and closed windows back */
   video_st->windows_forced_attached = 2u;
   video_st->windows_closed_attached = 2u;
   enable->actions->change(enable);
   CHECK(video_st->windows_forced_attached == 0u);
   CHECK(video_st->windows_closed_attached == 0u);
   /* So does Restore Screen Windows */
   restore = video_st->windows_restore;
   video_st->windows_closed_attached = 2u;
   menu_action("video_screen_windows_restore", MENU_ACTION_OK);
   CHECK(video_st->windows_closed_attached == 0u);
   CHECK(video_st->windows_restore == restore + 1u);
}

static void window_test_named(video_window_display_t *d, unsigned ordinal,
      bool own, const char *name, const char *model, unsigned dims)
{
   window_test_display(d, ordinal, VIDEO_WINDOW_CONNECTION_UNKNOWN, own,
         name);
   strlcpy(d->model, model, sizeof(d->model));
   d->dims = dims;
}

/* The display row: Auto, each display by name, then Off */
static void test_window_menu_display(void)
{
   static video_window_display_t displays[3];
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   char *picked                   = settings->arrays.video_screen_display[1];
   rarch_setting_t *row;
   rarch_setting_t *first;
   char s[128];
   load_fixture("");
   row   = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_DISPLAY + 1));
   first = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_SCREEN_DISPLAY);
   CHECK(row != NULL);
   CHECK(first != NULL);
   CHECK(menu_setting_find_enum((enum msg_hash_enums)
            (MENU_ENUM_LABEL_VIDEO_SCREEN_DISPLAY + RETRO_VIDEO_VIEWS_MAX - 1)) != NULL);
   if (!row || !first)
      return;
   CHECK(string_is_equal(row->name, "video_screen2_display"));
   CHECK(string_is_equal(row->short_description, "Game Screen 2 Display"));
   CHECK(string_is_equal(first->name, "video_screen1_display"));
   CHECK(row->type == ST_STRING_OPTIONS);
   CHECK(row->value.target.string == picked);

   /* Nothing published on the desktop: Auto, Main Window and Off */
   video_driver_publish_window_displays(displays, 0);
   menu_setting_video_screen_display_values();
   CHECK(string_is_equal(row->aux.values, "|main|off"));
   row->actions->repr(row, s, sizeof(s));
   CHECK(string_is_equal(s, "Auto"));
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_MAIN));
   row->actions->repr(row, s, sizeof(s));
   CHECK(string_is_equal(s, "Main Window"));
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_OFF));
   row->actions->repr(row, s, sizeof(s));
   CHECK(string_is_equal(s, "OFF"));
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(!picked[0]);

   /* The Thor, with a monitor on DP */
   window_test_named(&displays[0], 1, true, "Built-in Screen", "",
         VIDEO_SCALE_PACK(1920, 1080));
   window_test_named(&displays[1], 2, false, "Screen-2", "AYN",
         VIDEO_SCALE_PACK(1240, 1080));
   window_test_named(&displays[2], 3, false, "DP Screen", "DELL U2720Q", 0);
   video_driver_publish_window_displays(displays, 3);
   menu_setting_video_screen_display_values();
   CHECK(string_is_equal(row->aux.values,
            "|Built-in Screen|Screen-2|DP Screen|off"));
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(string_is_equal(picked, "Built-in Screen"));
   row->actions->repr(row, s, sizeof(s));
   CHECK(string_is_equal(s, "Display: Built-in Screen (1920x1080)"));
   CHECK(row->actions->right(row, 0, false) == 0);
   row->actions->repr(row, s, sizeof(s));
   CHECK(string_is_equal(s, "Display: Screen-2 (AYN 1240x1080)"));
   /* A short buffer: cut, and nothing written past it */
   memset(s, 'x', sizeof(s) - 1);
   s[sizeof(s) - 1] = '\0';
   CHECK(row->actions->repr(row, s, 12) == strlen("Display: Screen-2"));
   CHECK(string_is_equal(s, "Display: Sc"));
   CHECK(strspn(s + 12, "x") == sizeof(s) - 13);
   CHECK(row->actions->right(row, 0, false) == 0);
   row->actions->repr(row, s, sizeof(s));
   CHECK(string_is_equal(s, "Display: DP Screen (DELL U2720Q)"));
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_OFF));
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(!picked[0]);
   CHECK(row->actions->left(row, 0, false) == 0);
   CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_OFF));
   CHECK(row->actions->left(row, 0, false) == 0);
   CHECK(string_is_equal(picked, "DP Screen"));

   /* Unplugged: shown as saved; right goes to Auto, left to Off */
   video_driver_publish_window_displays(displays, 2);
   row->actions->repr(row, s, sizeof(s));
   CHECK(string_is_equal(s, "Display: DP Screen"));
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(!picked[0]);
   strlcpy(picked, "DP Screen", 64);
   CHECK(row->actions->left(row, 0, false) == 0);
   CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_OFF));

   /* A repeated name: the second is offered and saved with " 2" */
   strlcpy(displays[2].name, "Screen-2", sizeof(displays[2].name));
   video_driver_publish_window_displays(displays, 3);
   menu_setting_video_screen_display_values();
   CHECK(string_is_equal(row->aux.values,
            "|Built-in Screen|Screen-2|Screen-2 2|off"));
   strlcpy(picked, "Screen-2", 64);
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(string_is_equal(picked, "Screen-2 2"));

   /* A change gives failed windows another try, and brings back its
    * own screen's closed window only */
   video_st->windows_forced_attached = 2u;
   video_st->windows_closed_attached = 6u;
   row->actions->change(row);
   CHECK(video_st->windows_forced_attached == 0u);
   CHECK(video_st->windows_closed_attached == 4u);
   video_st->windows_closed_attached = 0u;
   picked[0] = '\0';
   video_driver_publish_window_displays(displays, 0);
}

/* Desktop monitors: Auto, Main Window, each monitor, then Off; the
 * menu's row gets the monitors only */
static void test_window_menu_monitors(void)
{
   static video_window_display_t displays[2];
   struct menu_state *menu_st = menu_state_get_ptr();
   char *picked = config_get_ptr()->arrays.video_screen_display[1];
   rarch_setting_t *row;
   rarch_setting_t *menu;
   char s[128];
   load_fixture("");
   row  = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_DISPLAY + 1));
   menu = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_MENU_DISPLAY);
   CHECK(row != NULL);
   CHECK(menu != NULL);
   if (!row || !menu)
      return;
   CHECK(row->value.target.string == picked);
   window_test_named(&displays[0], 1, true, "DP-1", "DELL U2720Q",
         VIDEO_SCALE_PACK(2560, 1440));
   window_test_named(&displays[1], 2, false, "", "", 0);
   displays[0].monitor = displays[1].monitor = true;
   video_driver_publish_window_displays(displays, 2);
   menu_setting_video_screen_display_values();
   CHECK(string_is_equal(row->aux.values, "|main|DP-1|Monitor 2|off"));
   CHECK(string_is_equal(menu->aux.values, "|DP-1|Monitor 2"));
   picked[0] = '\0';
   menu_st->flags &= ~(MENU_ST_FLAG_PREVENT_POPULATE
         | MENU_ST_FLAG_ENTRIES_NEED_REFRESH);
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_MAIN));
   /* The rows under it follow the choice */
   CHECK(menu_st->flags & MENU_ST_FLAG_ENTRIES_NEED_REFRESH);
   CHECK(menu_st->flags & MENU_ST_FLAG_PREVENT_POPULATE);
   row->actions->repr(row, s, sizeof(s));
   CHECK(string_is_equal(s, "Main Window"));
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(string_is_equal(picked, "DP-1"));
   row->actions->repr(row, s, sizeof(s));
   CHECK(string_is_equal(s, "Display: DP-1 (DELL U2720Q 2560x1440)"));
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(string_is_equal(picked, "Monitor 2"));
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_OFF));
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(!picked[0]);
   CHECK(row->actions->left(row, 0, false) == 0);
   CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_OFF));
   strlcpy(picked, "DP-1", 64);
   CHECK(row->actions->left(row, 0, false) == 0);
   CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_MAIN));
   CHECK(row->actions->left(row, 0, false) == 0);
   CHECK(!picked[0]);
   /* The menu's row steps through the monitors alone */
   CHECK(menu->actions->right(menu, 0, false) == 0);
   CHECK(string_is_equal(menu->value.target.string, "DP-1"));
   menu->value.target.string[0] = '\0';
   /* None listed on the desktop: Auto, Main Window, Off */
   video_driver_publish_window_displays(displays, 0);
   menu_setting_video_screen_display_values();
   CHECK(string_is_equal(row->aux.values, "|main|off"));
   CHECK(string_is_equal(menu->aux.values, "|"));
   /* A monitor literally named "main" is not offered */
   strlcpy(displays[1].name, VIDEO_WINDOW_DISPLAY_MAIN,
         sizeof(displays[1].name));
   video_driver_publish_window_displays(displays, 2);
   menu_setting_video_screen_display_values();
   CHECK(string_is_equal(row->aux.values, "|main|DP-1|off"));
   CHECK(string_is_equal(menu->aux.values, "|DP-1"));
   /* Every slot listed and the one chosen gone: left is Off, right is
    * Auto, not the last monitor */
   {
      static video_window_display_t many[VIDEO_WINDOW_DISPLAYS_MAX];
      char name[16];
      unsigned i;
      for (i = 0; i < VIDEO_WINDOW_DISPLAYS_MAX; i++)
      {
         snprintf(name, sizeof(name), "DP-%u", i + 1);
         window_test_named(&many[i], i + 1, !i, name, "", 0);
         many[i].monitor = true;
      }
      video_driver_publish_window_displays(many, VIDEO_WINDOW_DISPLAYS_MAX);
      strlcpy(picked, "HDMI-9", 64);
      CHECK(row->actions->left(row, 0, false) == 0);
      CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_OFF));
      strlcpy(picked, "HDMI-9", 64);
      CHECK(row->actions->right(row, 0, false) == 0);
      CHECK(!picked[0]);
   }
   video_driver_publish_window_displays(NULL, 0);
   picked[0] = '\0';
}

/* OK on a row, as menu_action_handle_setting() presses it */
static int window_display_ok(rarch_setting_t *row)
{
   CHECK(row->actions->ok != NULL);
   if (!row->actions->ok)
      return -1;
   return row->actions->ok(row, 0, false);
}

/* The list OK opens, which the menu builds from the path OK pushes:
 * the row's enum */
static size_t window_display_dropdown(file_list_t *list,
      const rarch_setting_t *row)
{
   menu_displaylist_info_t info;
   char path[16];
   snprintf(path, sizeof(path), "%d", (int)row->enum_idx);
   memset(list, 0, sizeof(*list));
   menu_displaylist_info_init(&info);
   info.list = list;
   info.path = strdup(path);
   menu_displaylist_ctl(DISPLAYLIST_DROPDOWN_LIST, &info, config_get_ptr());
   menu_displaylist_info_free(&info);
   return list->size;
}

/* A dropdown entry pressed, bound as under the menu's dropdown, which
 * this harness has no stack for */
static void window_display_pick(file_list_t *list, size_t i)
{
   const char *dropdown      = msg_hash_to_str(
         MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST);
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         list->list[i].actiondata;
   CHECK(cbs != NULL);
   if (!cbs)
      return;
   menu_cbs_init_bind_ok(cbs, list->list[i].path, list->list[i].label,
         strlen(list->list[i].label), list->list[i].type, i,
         dropdown, strlen(dropdown));
   CHECK(cbs->action_ok != NULL);
   if (cbs->action_ok)
      cbs->action_ok(list->list[i].path, list->list[i].label,
            list->list[i].type, i, list->list[i].entry_idx);
}

/* OK on a display row: Auto, each display listed now, then Off; the
 * menu's row has no Off */
static void test_window_menu_display_dropdown(void)
{
   static video_window_display_t displays[2];
   settings_t *settings       = config_get_ptr();
   struct menu_state *menu_st = menu_state_get_ptr();
   char *picked               = settings->arrays.video_screen_display[1];
   menu_file_list_cbs_t *cbs;
   rarch_setting_t *row;
   rarch_setting_t *menu;
   file_list_t list;
   unsigned i;
   load_fixture("");
   row  = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_DISPLAY + 1));
   menu = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_MENU_DISPLAY);
   CHECK(row != NULL);
   CHECK(menu != NULL);
   if (!row || !menu)
      return;
   /* menu_entries_clear() wants a menu driver: the null one */
   for (i = 0; menu_ctx_drivers[i]; i++)
      if (string_is_equal(menu_ctx_drivers[i]->ident, "null"))
         menu_st->driver_ctx = menu_ctx_drivers[i];
   CHECK(menu_st->driver_ctx != NULL);
   if (!menu_st->driver_ctx)
      return;

   /* Nothing published on the desktop: Auto, Main Window and Off */
   video_driver_publish_window_displays(displays, 0);
   CHECK(window_display_ok(row) == 1);
   CHECK(window_display_dropdown(&list, row) == 3);
   if (list.size == 3)
   {
      CHECK(string_is_equal(list.list[0].path, "Auto"));
      CHECK(string_is_equal(list.list[1].path, "Main Window"));
      CHECK(string_is_equal(list.list[2].path, "OFF"));
      window_display_pick(&list, 1);
      CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_MAIN));
      picked[0] = '\0';
   }
   file_list_deinitialize(&list);
   /* The menu's row never offers Main Window */
   CHECK(window_display_ok(menu) == 1);
   CHECK(window_display_dropdown(&list, menu) == 1);
   file_list_deinitialize(&list);

   /* Published since the Game Screens list was built */
   window_test_named(&displays[0], 1, true, "Built-in Screen", "",
         VIDEO_SCALE_PACK(1920, 1080));
   window_test_named(&displays[1], 2, false, "Screen-2", "AYN",
         VIDEO_SCALE_PACK(1240, 1080));
   video_driver_publish_window_displays(displays, 2);
   strlcpy(picked, "Screen-2", 64);
   CHECK(window_display_ok(row) == 1);
   CHECK(window_display_dropdown(&list, row) == 4);
   if (list.size == 4)
   {
      CHECK(string_is_equal(list.list[0].path, "Auto"));
      CHECK(string_is_equal(list.list[1].path,
               "Display: Built-in Screen (1920x1080)"));
      CHECK(string_is_equal(list.list[2].path,
               "Display: Screen-2 (AYN 1240x1080)"));
      CHECK(string_is_equal(list.list[3].path, "OFF"));
      cbs = (menu_file_list_cbs_t*)list.list[2].actiondata;
      CHECK(cbs && cbs->checked);
      CHECK(string_is_equal(picked, "Screen-2"));
      window_display_pick(&list, 1);
      CHECK(string_is_equal(picked, "Built-in Screen"));
      window_display_pick(&list, 3);
      CHECK(string_is_equal(picked, VIDEO_WINDOW_DISPLAY_OFF));
      window_display_pick(&list, 0);
      CHECK(!picked[0]);
      window_display_pick(&list, 2);
      CHECK(string_is_equal(picked, "Screen-2"));
   }
   file_list_deinitialize(&list);

   /* Menu Display: the same displays, no Off */
   CHECK(window_display_ok(menu) == 1);
   CHECK(window_display_dropdown(&list, menu) == 3);
   if (list.size == 3)
   {
      CHECK(string_is_equal(list.list[0].path, "Auto"));
      CHECK(string_is_equal(list.list[2].path,
               "Display: Screen-2 (AYN 1240x1080)"));
      window_display_pick(&list, 2);
      CHECK(string_is_equal(settings->arrays.video_menu_display,
               "Screen-2"));
   }
   file_list_deinitialize(&list);
   menu_st->driver_ctx = NULL;
   picked[0] = '\0';
   settings->arrays.video_menu_display[0] = '\0';
   video_driver_publish_window_displays(displays, 0);
}

/* Menu Follows, Menu Display and Notifications */
static void test_window_menu_rows(void)
{
   static video_window_display_t displays[2];
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   char *picked                   = settings->arrays.video_menu_display;
   rarch_setting_t *follows;
   rarch_setting_t *menu;
   rarch_setting_t *notes;
   char s[128];
   load_fixture("");
   follows = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_MENU_FOLLOWS);
   menu    = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_MENU_DISPLAY);
   notes   = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_NOTIFICATIONS_WINDOW);
   CHECK(follows != NULL);
   CHECK(menu != NULL);
   CHECK(notes != NULL);
   if (!follows || !menu || !notes)
      return;

   CHECK(string_is_equal(follows->name, "video_menu_follows"));
   CHECK(string_is_equal(follows->short_description, "Menu Follows"));
   CHECK(follows->value.target.unsigned_integer
         == &settings->uints.video_menu_follows);
   CHECK(follows->max == (float)VIDEO_MENU_FOLLOWS_SEPARATE);
   follows->actions->repr(follows, s, sizeof(s));
   CHECK(string_is_equal(s, "Game Screen 1"));
   /* The list shows or hides Menu Display */
   menu_st->flags &= ~MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   CHECK(follows->actions->right(follows, 0, false) == 0);
   CHECK(settings->uints.video_menu_follows == 1);
   CHECK(menu_st->flags & MENU_ST_FLAG_ENTRIES_NEED_REFRESH);
   menu_st->flags &= ~(MENU_ST_FLAG_ENTRIES_NEED_REFRESH
         | MENU_ST_FLAG_PREVENT_POPULATE);
   follows->actions->repr(follows, s, sizeof(s));
   CHECK(string_is_equal(s, "Game Screen 2"));
   settings->uints.video_menu_follows = VIDEO_MENU_FOLLOWS_SEPARATE;
   follows->actions->repr(follows, s, sizeof(s));
   CHECK(string_is_equal(s, "Separate"));

   CHECK(string_is_equal(notes->name, "video_notifications_window"));
   CHECK(string_is_equal(notes->short_description, "Notifications"));
   CHECK(notes->max == 1.0f);
   notes->actions->repr(notes, s, sizeof(s));
   CHECK(string_is_equal(s, "With the Game"));
   settings->uints.video_notifications_window = VIDEO_NOTIFICATIONS_WITH_MENU;
   notes->actions->repr(notes, s, sizeof(s));
   CHECK(string_is_equal(s, "With the Menu"));

   CHECK(string_is_equal(menu->name, "video_menu_display"));
   CHECK(string_is_equal(menu->short_description, "Menu Display"));
   CHECK(menu->type == ST_STRING_OPTIONS);
   CHECK(menu->value.target.string == picked);

   /* Auto and each display; never Off */
   video_driver_publish_window_displays(displays, 0);
   menu_setting_video_screen_display_values();
   CHECK(string_is_equal(menu->aux.values, "|"));
   window_test_named(&displays[0], 1, true, "Built-in Screen", "",
         VIDEO_SCALE_PACK(1920, 1080));
   window_test_named(&displays[1], 2, false, "Screen-2", "AYN",
         VIDEO_SCALE_PACK(1240, 1080));
   video_driver_publish_window_displays(displays, 2);
   menu_setting_video_screen_display_values();
   CHECK(string_is_equal(menu->aux.values, "|Built-in Screen|Screen-2"));
   CHECK(menu->actions->right(menu, 0, false) == 0);
   CHECK(string_is_equal(picked, "Built-in Screen"));
   CHECK(menu->actions->right(menu, 0, false) == 0);
   menu->actions->repr(menu, s, sizeof(s));
   CHECK(string_is_equal(s, "Display: Screen-2 (AYN 1240x1080)"));
   CHECK(menu->actions->right(menu, 0, false) == 0);
   CHECK(!picked[0]);
   CHECK(menu->actions->left(menu, 0, false) == 0);
   CHECK(string_is_equal(picked, "Screen-2"));
   /* "off" here is Auto */
   strlcpy(picked, VIDEO_WINDOW_DISPLAY_OFF, 64);
   menu->actions->repr(menu, s, sizeof(s));
   CHECK(string_is_equal(s, "Auto"));
   CHECK(menu->actions->right(menu, 0, false) == 0);
   CHECK(!picked[0]);
   /* A display whose key is "off" is never offered */
   strlcpy(displays[1].name, VIDEO_WINDOW_DISPLAY_OFF,
         sizeof(displays[1].name));
   video_driver_publish_window_displays(displays, 2);
   menu_setting_video_screen_display_values();
   CHECK(string_is_equal(menu->aux.values, "|Built-in Screen"));

   /* A change to any of them gives failed windows another try, and
    * leaves closed ones closed */
   video_st->windows_closed_attached = 2u;
   video_st->windows_forced_attached = 2u;
   follows->actions->change(follows);
   CHECK(video_st->windows_forced_attached == 0u);
   video_st->windows_forced_attached = 2u;
   menu->actions->change(menu);
   CHECK(video_st->windows_forced_attached == 0u);
   video_st->windows_forced_attached = 2u;
   notes->actions->change(notes);
   CHECK(video_st->windows_forced_attached == 0u);
   CHECK(video_st->windows_closed_attached == 2u);
   video_st->windows_closed_attached = 0u;

   picked[0] = '\0';
   video_driver_publish_window_displays(displays, 0);
   load_fixture("");
}

/* Menu Follows offers the screens the Display rows list, then Separate;
 * one chosen beyond them stays until moved off */
static void test_window_menu_follows_screens(void)
{
   settings_t *settings       = config_get_ptr();
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned *follows          = &settings->uints.video_menu_follows;
   menu_file_list_cbs_t *cbs;
   rarch_setting_t *row;
   file_list_t list;
   char s[128];
   unsigned i;
   load_fixture("");
   row = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_MENU_FOLLOWS);
   CHECK(row != NULL);
   if (!row)
      return;
   for (i = 0; menu_ctx_drivers[i]; i++)
      if (string_is_equal(menu_ctx_drivers[i]->ident, "null"))
         menu_st->driver_ctx = menu_ctx_drivers[i];
   CHECK(menu_st->driver_ctx != NULL);
   if (!menu_st->driver_ctx)
      return;
   settings->bools.menu_navigation_wraparound_enable = true;

   /* No game: Game Screen 1, Game Screen 2, Separate */
   set_menu_map(0);
   CHECK(window_display_ok(row) == 1);
   CHECK(window_display_dropdown(&list, row) == 3);
   if (list.size == 3)
   {
      CHECK(string_is_equal(list.list[0].path, "Game Screen 1"));
      CHECK(string_is_equal(list.list[1].path, "Game Screen 2"));
      CHECK(string_is_equal(list.list[2].path, "Separate"));
      window_display_pick(&list, 2);
      CHECK(*follows == VIDEO_MENU_FOLLOWS_SEPARATE);
      window_display_pick(&list, 1);
      CHECK(*follows == 1);
   }
   file_list_deinitialize(&list);
   *follows = 1;
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(*follows == VIDEO_MENU_FOLLOWS_SEPARATE);
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(*follows == 0);
   CHECK(row->actions->left(row, 0, false) == 0);
   CHECK(*follows == VIDEO_MENU_FOLLOWS_SEPARATE);
   CHECK(row->actions->left(row, 0, false) == 0);
   CHECK(*follows == 1);

   /* A DS game: the same */
   set_menu_map(2);
   CHECK(window_display_dropdown(&list, row) == 3);
   file_list_deinitialize(&list);

   /* Game Screen 4, from a 4-screen core: listed and kept, gone once left */
   *follows = 3;
   row->actions->repr(row, s, sizeof(s));
   CHECK(string_is_equal(s, "Game Screen 4"));
   CHECK(window_display_dropdown(&list, row) == 4);
   CHECK(*follows == 3);
   if (list.size == 4)
   {
      cbs = (menu_file_list_cbs_t*)list.list[2].actiondata;
      CHECK(string_is_equal(list.list[2].path, "Game Screen 4"));
      CHECK(cbs && cbs->checked);
      CHECK(string_is_equal(list.list[3].path, "Separate"));
      window_display_pick(&list, 2);
      CHECK(*follows == 3);
      window_display_pick(&list, 3);
      CHECK(*follows == VIDEO_MENU_FOLLOWS_SEPARATE);
   }
   file_list_deinitialize(&list);
   *follows = 3;
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(*follows == VIDEO_MENU_FOLLOWS_SEPARATE);
   *follows = 3;
   CHECK(row->actions->left(row, 0, false) == 0);
   CHECK(*follows == 1);
   CHECK(window_display_dropdown(&list, row) == 3);
   file_list_deinitialize(&list);
   *follows = 3;
   CHECK(row->actions->start(row) == 0);
   CHECK(*follows == 0);

   /* A 4-screen core: Game Screens 1 to 4 */
   set_menu_map(4);
   CHECK(window_display_dropdown(&list, row) == 5);
   if (list.size == 5)
   {
      CHECK(string_is_equal(list.list[3].path, "Game Screen 4"));
      window_display_pick(&list, 4);
      CHECK(*follows == VIDEO_MENU_FOLLOWS_SEPARATE);
   }
   file_list_deinitialize(&list);
   *follows = 2;
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(*follows == 3);
   CHECK(row->actions->right(row, 0, false) == 0);
   CHECK(*follows == VIDEO_MENU_FOLLOWS_SEPARATE);

   /* No more than the screens there are rows for */
   video_state_get_ptr()->views_core.num_screens = RETRO_VIDEO_VIEWS_MAX + 1;
   CHECK(menu_setting_video_screens() == RETRO_VIDEO_VIEWS_MAX);

   set_menu_map(0);
   menu_st->driver_ctx = NULL;
   load_fixture("");
}

static bool window_menu_has(const file_list_t *list, const char *label)
{
   size_t i;
   for (i = 0; i < list->size; i++)
      if (string_is_equal(list->list[i].label, label))
         return true;
   return false;
}

/* Where @label is listed, or the list's size */
static size_t window_menu_index(const file_list_t *list, const char *label)
{
   size_t i;
   for (i = 0; i < list->size; i++)
      if (string_is_equal(list->list[i].label, label))
         break;
   return i;
}

/* Screen @screen + 1's page, built as its deferred push builds it */
static void window_menu_page_build(file_list_t *list, unsigned screen)
{
   static menu_ctx_driver_t page_ctx;
   struct menu_state *menu_st   = menu_state_get_ptr();
   const menu_ctx_driver_t *ctx = menu_st->driver_ctx;
   menu_displaylist_info_t info;
   char path[64];
   snprintf(path, sizeof(path), msg_hash_to_str(
            MENU_ENUM_LABEL_VIDEO_SCREEN_SETTINGS), screen + 1);
   menu_displaylist_info_init(&info);
   info.list = list;
   info.path = strdup(path);
   if (!ctx)
   {
      page_ctx.ident      = "window_menu_page";
      menu_st->driver_ctx = &page_ctx;
   }
   menu_displaylist_ctl(DISPLAYLIST_VIDEO_SCREEN_SETTINGS_LIST, &info,
         config_get_ptr());
   menu_st->driver_ctx = ctx;
   menu_displaylist_info_free(&info);
}

/* Whether @label is listed right after Game Screen @screen's rows */
static bool window_menu_after_screen(const file_list_t *list,
      const char *label, unsigned screen)
{
   char prefix[32];
   size_t i;
   snprintf(prefix, sizeof(prefix), "video_screen%u_", screen);
   for (i = 1; i < list->size; i++)
      if (     string_is_equal(list->list[i].label, label)
            && list->list[i - 1].label
            && string_starts_with(list->list[i - 1].label, prefix))
         return true;
   return false;
}

/* The switch sits in Game Screens where the driver has windows,
 * with a row a screen (Android's Display rows, the desktop's Screen
 * N) and, on the desktop, Restore Screen Windows; the menu's rows are
 * Android's */
static void test_window_menu_list(void)
{
   settings_t *settings = config_get_ptr();
   file_list_t list;
   size_t gs, hs;
   char s[256];
   load_fixture("");
   memset(&list, 0, sizeof(list));
   menu_displaylist_build_list(&list, settings,
         DISPLAYLIST_VIDEO_GAME_SCREENS_SETTINGS_LIST, false);
   CHECK(!window_menu_has(&list, "video_windows_enable"));
   CHECK(!window_menu_has(&list, "video_screen2_window_mode"));
   CHECK(!window_menu_has(&list, "video_screen_windows_restore"));
   file_list_deinitialize(&list);

   window_test_begin();
   window_test_run();
   memset(&list, 0, sizeof(list));
   menu_displaylist_build_list(&list, settings,
         DISPLAYLIST_VIDEO_GAME_SCREENS_SETTINGS_LIST, false);
   CHECK(window_menu_has(&list, "video_windows_enable"));
   CHECK(!window_menu_has(&list, "video_screen1_window_mode"));
   CHECK(!window_menu_has(&list, "video_screen2_window_mode"));
   CHECK(!window_menu_has(&list, "video_screen2_window_monitor"));
#ifdef ANDROID
   CHECK(window_menu_has(&list, "video_screen1_display"));
   CHECK(window_menu_has(&list, "video_screen2_display"));
   CHECK(!window_menu_has(&list, "video_screen3_display"));
#else
   CHECK(window_menu_has(&list, "video_screen1_settings"));
   CHECK(window_menu_has(&list, "video_screen2_settings"));
   CHECK(!window_menu_has(&list, "video_screen3_settings"));
   CHECK(!window_menu_has(&list, "video_screen1_display"));
   CHECK(!window_menu_has(&list, "video_screen2_display"));
#endif
   /* The menu's and notifications' rows are Android's */
   CHECK(!window_menu_has(&list, "video_menu_follows"));
   CHECK(!window_menu_has(&list, "video_menu_display"));
   CHECK(!window_menu_has(&list, "video_notifications_window"));
   CHECK(!window_menu_has(&list, "video_screen2_window_x"));
#ifdef ANDROID
   CHECK(!window_menu_has(&list, "video_screen_windows_restore"));
#else
   CHECK(window_menu_after_screen(&list, "video_screen_windows_restore",
            2));
#endif
   file_list_deinitialize(&list);
   /* Stereo 3D keeps only the eyes' rows; Video lists Game Screens,
    * Headset when it is listed, then Stereo 3D */
   memset(&list, 0, sizeof(list));
   menu_displaylist_build_list(&list, settings,
         DISPLAYLIST_VIDEO_STEREO_SETTINGS_LIST, false);
   CHECK(window_menu_has(&list, "video_stereo_swap_eyes"));
   CHECK(!window_menu_has(&list, "video_screen_layout"));
   CHECK(!window_menu_has(&list, "video_windows_enable"));
   CHECK(!window_menu_has(&list, "video_screen2_display"));
   file_list_deinitialize(&list);
   memset(&list, 0, sizeof(list));
   menu_displaylist_build_list(&list, settings,
         DISPLAYLIST_VIDEO_SETTINGS_LIST, false);
   gs = window_menu_index(&list, "video_game_screens_settings");
   hs = window_menu_index(&list, "video_headset_settings");
   CHECK(gs < list.size);
   CHECK(hs == list.size || hs == gs + 1);
   CHECK(window_menu_index(&list, "video_stereo_settings")
         == (hs < list.size ? hs : gs) + 1);
   file_list_deinitialize(&list);

   settings->bools.video_windows_enable = false;
   memset(&list, 0, sizeof(list));
   menu_displaylist_build_list(&list, settings,
         DISPLAYLIST_VIDEO_GAME_SCREENS_SETTINGS_LIST, false);
   CHECK(window_menu_has(&list, "video_windows_enable"));
   CHECK(!window_menu_has(&list, "video_screen2_window_mode"));
   CHECK(!window_menu_has(&list, "video_screen2_display"));
   CHECK(!window_menu_has(&list, "video_screen2_settings"));
   CHECK(!window_menu_has(&list, "video_screen_windows_restore"));
   file_list_deinitialize(&list);
   window_test_end();

   menu_cbs_sublabel_for_enum(MENU_ENUM_LABEL_VIDEO_MENU_FOLLOWS,
         0, 0, s, sizeof(s));
   CHECK(string_is_equal(s,
            msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_MENU_FOLLOWS)));
   menu_cbs_sublabel_for_enum(MENU_ENUM_LABEL_VIDEO_MENU_DISPLAY,
         0, 0, s, sizeof(s));
   CHECK(string_is_equal(s,
            msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_MENU_DISPLAY)));
   menu_cbs_sublabel_for_enum(MENU_ENUM_LABEL_VIDEO_NOTIFICATIONS_WINDOW,
         0, 0, s, sizeof(s));
   CHECK(string_is_equal(s,
            msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_NOTIFICATIONS_WINDOW)));
#ifdef ANDROID
   menu_cbs_sublabel_for_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_DISPLAY + 7), 0, 0, s, sizeof(s));
   CHECK(string_is_equal(s,
            msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_SCREEN_DISPLAY)));
#else
   menu_cbs_sublabel_for_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_DISPLAY + 7), 0, 0, s, sizeof(s));
   CHECK(string_is_equal(s, msg_hash_to_str(
            MENU_ENUM_SUBLABEL_VIDEO_SCREEN_DISPLAY_DESKTOP)));
   menu_cbs_sublabel_for_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_DISPLAY + 1), 0, 0, s, sizeof(s));
   CHECK(string_is_equal(s, msg_hash_to_str(
            MENU_ENUM_SUBLABEL_VIDEO_SCREEN_DISPLAY_DESKTOP)));
   menu_cbs_sublabel_for_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_SETTINGS + 7), 0, 0, s, sizeof(s));
   CHECK(string_is_equal(s, msg_hash_to_str(
            MENU_ENUM_SUBLABEL_VIDEO_SCREEN_SETTINGS)));
#endif
   menu_cbs_sublabel_for_enum(MENU_ENUM_LABEL_VIDEO_WINDOWS_ENABLE,
         0, 0, s, sizeof(s));
   CHECK(string_is_equal(s,
            msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_WINDOWS_ENABLE)));
   menu_cbs_sublabel_for_enum(MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOWS_RESTORE,
         0, 0, s, sizeof(s));
   CHECK(string_is_equal(s,
            msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_SCREEN_WINDOWS_RESTORE)));
}

/* A screen with its own window gets its window rows; X and Y once it
 * has a size */
static void test_window_menu_window_rows(void)
{
   settings_t *settings           = config_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   file_list_t list;
   rarch_setting_t *width, *x, *y, *fullscreen, *display;
   unsigned restore, edit, edit2;
   char s[64];

   /* The rows step a copy; only the settings are saved */
   load_fixture("video_screen2_window_x = \"-1200\"\n"
         "video_screen2_window_width = \"640\"\n"
         "video_screen2_window_height = \"480\"\n");
   CHECK(menu_setting_find_enum((enum msg_hash_enums)
            (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_X + 1)) != NULL);
   CHECK(menu_setting_find_enum((enum msg_hash_enums)
            (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_WIDTH + 1)) != NULL);
   settings->uints.video_screen_window_pos[1]  = VIDEO_POS_PACK(-1100, 20);
   settings->uints.video_screen_window_dims[1] = VIDEO_SCALE_PACK(800, 600);
   settings->bools.config_save_minimal         = false;
   CHECK(config_save_file(saved_path));
   path_set(RARCH_PATH_CONFIG, saved_path);
   config_load();
   CHECK(settings->uints.video_screen_window_pos[1]
         == VIDEO_POS_PACK(-1100, 20));
   CHECK(settings->uints.video_screen_window_dims[1]
         == VIDEO_SCALE_PACK(800, 600));

   load_fixture("");
   window_test_begin();
   window_test_run();
   /* Game Screen 1: the main window, no rows; 2: Auto, its own */
   memset(&list, 0, sizeof(list));
   window_menu_page_build(&list, 0);
   CHECK(window_menu_has(&list, "video_screen1_display"));
   CHECK(!window_menu_has(&list, "video_screen1_window_fullscreen"));
   file_list_deinitialize(&list);
   memset(&list, 0, sizeof(list));
   window_menu_page_build(&list, 1);
   CHECK(window_menu_has(&list, "video_screen2_window_fullscreen"));
   CHECK(window_menu_has(&list, "video_screen2_window_width"));
   CHECK(window_menu_has(&list, "video_screen2_window_height"));
   CHECK(!window_menu_has(&list, "video_screen2_window_x"));
   CHECK(!window_menu_has(&list, "video_screen2_window_y"));
   file_list_deinitialize(&list);

   /* Game Screen 1 on a monitor: rows while it is listed; one that is
    * not leaves it in the main window */
   {
      static video_window_display_t displays[1];
      window_test_named(&displays[0], 1, false, "DP-1", "", 0);
      displays[0].monitor = true;
      video_driver_publish_window_displays(displays, 1);
      strlcpy(settings->arrays.video_screen_display[0], "DP-1",
            sizeof(settings->arrays.video_screen_display[0]));
      memset(&list, 0, sizeof(list));
      window_menu_page_build(&list, 0);
      CHECK(window_menu_has(&list, "video_screen1_window_fullscreen"));
      CHECK(window_menu_has(&list, "video_screen1_window_width"));
      file_list_deinitialize(&list);
      strlcpy(settings->arrays.video_screen_display[0], "DP-2",
            sizeof(settings->arrays.video_screen_display[0]));
      strlcpy(settings->arrays.video_screen_display[1], "DP-2",
            sizeof(settings->arrays.video_screen_display[1]));
      memset(&list, 0, sizeof(list));
      window_menu_page_build(&list, 0);
      CHECK(window_menu_has(&list, "video_screen1_display"));
      CHECK(!window_menu_has(&list, "video_screen1_window_fullscreen"));
      CHECK(!window_menu_has(&list, "video_screen1_window_width"));
      file_list_deinitialize(&list);
      /* Game Screen 2 on it: its own window, on Auto's monitor */
      memset(&list, 0, sizeof(list));
      window_menu_page_build(&list, 1);
      CHECK(window_menu_has(&list, "video_screen2_window_fullscreen"));
      file_list_deinitialize(&list);
      settings->arrays.video_screen_display[1][0] = '\0';
      video_driver_publish_window_displays(NULL, 0);
      memset(&list, 0, sizeof(list));
      window_menu_page_build(&list, 0);
      CHECK(!window_menu_has(&list, "video_screen1_window_fullscreen"));
      file_list_deinitialize(&list);
      settings->arrays.video_screen_display[0][0] = '\0';
   }

   width = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_WIDTH + 1));
   x     = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_X + 1));
   y     = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_Y + 1));
   CHECK(width != NULL && x != NULL && y != NULL);
   if (width && x && y)
   {
      CHECK(string_is_equal(width->short_description,
               "Game Screen 2 Window Width"));
      CHECK(width->actions->read != NULL && width->actions->change != NULL);
      CHECK(x->actions->read != NULL && x->actions->change != NULL);
      width->actions->read(width);
      width->actions->repr(width, s, sizeof(s));
      CHECK(string_is_equal(s, "Auto"));
      /* A step moves the copy; the change handler stores it, as
       * menu_setting_generic() does after a step */
      menu_st->flags &= ~(MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                        | MENU_ST_FLAG_PREVENT_POPULATE);
      CHECK(width->actions->right(width, 0, false) == 0);
      width->actions->change(width);
      CHECK(settings->uints.video_screen_window_dims[1]
            == VIDEO_SCALE_PACK(640, 480));
      /* X and Y come in with a size */
      CHECK(menu_st->flags & MENU_ST_FLAG_ENTRIES_NEED_REFRESH);
      CHECK(menu_st->flags & MENU_ST_FLAG_PREVENT_POPULATE);
      menu_st->flags &= ~(MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                        | MENU_ST_FLAG_PREVENT_POPULATE);
      CHECK(width->actions->right(width, 0, false) == 0);
      width->actions->change(width);
      CHECK(VIDEO_SCALE_W(settings->uints.video_screen_window_dims[1]) == 650);
      CHECK(VIDEO_SCALE_H(settings->uints.video_screen_window_dims[1]) == 480);
      CHECK(!(menu_st->flags & MENU_ST_FLAG_ENTRIES_NEED_REFRESH));

      settings->uints.video_screen_window_pos[1] = VIDEO_POS_PACK(-20, 30);
      x->actions->read(x);
      CHECK(!settings->bools.video_screen_window_pos_set[1]);
      /* Not placed: the rows show what is applied, not the position */
      x->actions->repr(x, s, sizeof(s));
      CHECK(string_is_equal(s, "Auto"));
      y->actions->repr(y, s, sizeof(s));
      CHECK(string_is_equal(s, "Auto"));
      CHECK(x->actions->left(x, 0, false) == 0);
      x->actions->change(x);
      CHECK(VIDEO_POS_X(settings->uints.video_screen_window_pos[1]) == -30);
      CHECK(VIDEO_POS_Y(settings->uints.video_screen_window_pos[1]) == 30);
      /* A position set from the menu is the window's */
      CHECK(settings->bools.video_screen_window_pos_set[1]);
      x->actions->repr(x, s, sizeof(s));
      CHECK(string_is_equal(s, "-30"));
      y->actions->repr(y, s, sizeof(s));
      CHECK(string_is_equal(s, "30"));

      /* A row change gives a window that could not be made another
       * try, and brings back its own screen's closed one, as the Display
       * row does */
      fullscreen = menu_setting_find_enum((enum msg_hash_enums)
            (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_FULLSCREEN + 1));
      display    = menu_setting_find_enum((enum msg_hash_enums)
            (MENU_ENUM_LABEL_VIDEO_SCREEN_DISPLAY + 1));
      CHECK(fullscreen != NULL && display != NULL);
      /* Each row counts an edit of its own screen's window, which the
       * next frame carries */
      edit2                             = video_st->windows_edit[2];
      restore                           = video_st->windows_restore;
      edit                              = video_st->windows_edit[1];
      video_st->windows_forced_attached = 2u;
      video_st->windows_closed_attached = 6u;
      CHECK(width->actions->right(width, 0, false) == 0);
      width->actions->change(width);
      CHECK(video_st->windows_forced_attached == 0u);
      CHECK(video_st->windows_closed_attached == 4u);
      CHECK(video_st->windows_restore == restore);
      CHECK(video_st->windows_edit[1] == edit + 1);
      video_st->windows_closed_attached = 0u;
      window_test_run();
      CHECK(window_test_seen.settings[1].edit == video_st->windows_edit[1]);
      restore                           = video_st->windows_restore;
      edit                              = video_st->windows_edit[1];
      video_st->windows_forced_attached = 2u;
      video_st->windows_closed_attached = 6u;
      CHECK(x->actions->right(x, 0, false) == 0);
      x->actions->change(x);
      CHECK(video_st->windows_forced_attached == 0u);
      CHECK(video_st->windows_closed_attached == 4u);
      CHECK(video_st->windows_restore == restore);
      CHECK(video_st->windows_edit[1] == edit + 1);
      video_st->windows_closed_attached = 0u;
      window_test_run();
      CHECK(window_test_seen.settings[1].edit == video_st->windows_edit[1]);
      if (fullscreen)
      {
         restore                           = video_st->windows_restore;
         edit                              = video_st->windows_edit[1];
         video_st->windows_forced_attached = 2u;
         video_st->windows_closed_attached = 6u;
         menu_action_handle_setting(fullscreen, 0, MENU_ACTION_RIGHT, false);
         CHECK(settings->bools.video_screen_window_fullscreen[1]);
         CHECK(video_st->windows_forced_attached == 0u);
         CHECK(video_st->windows_closed_attached == 4u);
         CHECK(video_st->windows_restore == restore);
         video_st->windows_closed_attached = 0u;
         /* Not by 1: the menu runs a toggle's change handler twice */
         CHECK(video_st->windows_edit[1] != edit);
         window_test_run();
         CHECK(window_test_seen.settings[1].edit
               == video_st->windows_edit[1]);
         settings->bools.video_screen_window_fullscreen[1] = false;
      }
      if (display)
      {
         edit                              = video_st->windows_edit[1];
         video_st->windows_closed_attached = 6u;
         menu_action_handle_setting(display, 0, MENU_ACTION_RIGHT, false);
         CHECK(string_is_equal(settings->arrays.video_screen_display[1],
                  VIDEO_WINDOW_DISPLAY_MAIN));
         CHECK(video_st->windows_edit[1] == edit + 1);
         CHECK(video_st->windows_closed_attached == 4u);
         video_st->windows_closed_attached = 0u;
         window_test_run();
         CHECK(window_test_seen.settings[1].edit
               == video_st->windows_edit[1]);
         settings->arrays.video_screen_display[1][0] = '\0';
         menu_st->flags &= ~(MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                           | MENU_ST_FLAG_PREVENT_POPULATE);
      }
      CHECK(video_st->windows_edit[2] == edit2);
      /* Game Screen 1's spare id carries its edits */
      {
         static video_window_display_t displays[1];
         window_test_named(&displays[0], 1, false, "DP-1", "", 0);
         displays[0].monitor = true;
         video_driver_publish_window_displays(displays, 1);
         strlcpy(settings->arrays.video_screen_display[0], "DP-1",
               sizeof(settings->arrays.video_screen_display[0]));
         video_driver_edit_window(0);
         video_driver_edit_window(0);
         CHECK(video_st->windows_edit[0] != video_st->windows_edit[2]);
         window_test_run();
         CHECK(window_test_seen.window_screens[2] == 1u);
         CHECK(window_test_seen.settings[2].edit
               == video_st->windows_edit[0]);
         settings->arrays.video_screen_display[0][0] = '\0';
         video_driver_publish_window_displays(NULL, 0);
         window_test_run();
      }
      settings->uints.video_screen_window_dims[1] = VIDEO_SCALE_PACK(650, 480);
      settings->uints.video_screen_window_pos[1]  = VIDEO_POS_PACK(-30, 30);
      x->actions->read(x);

      /* With a size, X and Y are listed */
      memset(&list, 0, sizeof(list));
      window_menu_page_build(&list, 1);
      CHECK(window_menu_has(&list, "video_screen2_window_x"));
      CHECK(window_menu_has(&list, "video_screen2_window_y"));
      file_list_deinitialize(&list);

      /* Start on X or Y: placed automatically again, as the menu runs
       * it (the change handler twice) */
      CHECK(menu_action_handle_setting(x, 0, MENU_ACTION_START, false) == 0);
      CHECK(!settings->bools.video_screen_window_pos_set[1]);
      x->actions->repr(x, s, sizeof(s));
      CHECK(string_is_equal(s, "Auto"));
      y->actions->repr(y, s, sizeof(s));
      CHECK(string_is_equal(s, "Auto"));
      /* and a step places it again */
      CHECK(menu_action_handle_setting(y, 0, MENU_ACTION_RIGHT, false) == 0);
      CHECK(settings->bools.video_screen_window_pos_set[1]);
      CHECK(settings->uints.video_screen_window_pos[1]
            == VIDEO_POS_PACK(0, 40));
      y->actions->repr(y, s, sizeof(s));
      CHECK(string_is_equal(s, "40"));
      CHECK(menu_action_handle_setting(y, 0, MENU_ACTION_START, false) == 0);
      CHECK(!settings->bools.video_screen_window_pos_set[1]);
      menu_st->flags &= ~(MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                        | MENU_ST_FLAG_PREVENT_POPULATE);

      /* Back to Auto, as the row's reset does: the position goes too */
      *width->value.target.unsigned_integer = 0;
      width->actions->change(width);
      CHECK(settings->uints.video_screen_window_dims[1] == 0);
      CHECK(!settings->bools.video_screen_window_pos_set[1]);
      CHECK(menu_st->flags & MENU_ST_FLAG_ENTRIES_NEED_REFRESH);
      menu_st->flags &= ~(MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                        | MENU_ST_FLAG_PREVENT_POPULATE);
   }
   /* Main Window: no rows */
   strlcpy(settings->arrays.video_screen_display[1],
         VIDEO_WINDOW_DISPLAY_MAIN,
         sizeof(settings->arrays.video_screen_display[1]));
   memset(&list, 0, sizeof(list));
   window_menu_page_build(&list, 1);
   CHECK(window_menu_has(&list, "video_screen2_display"));
   CHECK(!window_menu_has(&list, "video_screen2_window_fullscreen"));
   CHECK(!window_menu_has(&list, "video_screen2_window_width"));
   file_list_deinitialize(&list);
   settings->arrays.video_screen_display[1][0] = '\0';
   settings->uints.video_screen_window_dims[1] = 0;
   settings->uints.video_screen_window_pos[1]  = 0;
   window_test_end();
   {
      char sub[256];
      menu_cbs_sublabel_for_enum((enum msg_hash_enums)
            (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_WIDTH + 1),
            0, 0, sub, sizeof(sub));
      CHECK(string_is_equal(sub, msg_hash_to_str(
                  MENU_ENUM_SUBLABEL_VIDEO_SCREEN_WINDOW_WIDTH)));
      menu_cbs_sublabel_for_enum((enum msg_hash_enums)
            (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_Y + 7),
            0, 0, sub, sizeof(sub));
      CHECK(string_is_equal(sub, msg_hash_to_str(
                  MENU_ENUM_SUBLABEL_VIDEO_SCREEN_WINDOW_Y)));
      menu_cbs_sublabel_for_enum((enum msg_hash_enums)
            (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_FULLSCREEN + 1),
            0, 0, sub, sizeof(sub));
      CHECK(string_is_equal(sub, msg_hash_to_str(
                  MENU_ENUM_SUBLABEL_VIDEO_SCREEN_WINDOW_FULLSCREEN)));
   }
}

/* A window dragged with its rows' list open: they step, reset and show
 * what the drag left */
static void test_window_menu_rows_moved(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   rarch_setting_t *width, *height, *x, *y;
   char s[64];
   load_fixture("");
   settings->uints.video_screen_window_pos[1]     = VIDEO_POS_PACK(100, 50);
   settings->uints.video_screen_window_dims[1]    = VIDEO_SCALE_PACK(640, 480);
   settings->bools.video_screen_window_pos_set[1] = true;
   window_test_begin();
   window_test_run();
   /* Found as the list is built */
   width  = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_WIDTH + 1));
   height = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_HEIGHT + 1));
   x      = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_X + 1));
   y      = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_Y + 1));
   CHECK(width && height && x && y);
   if (width && height && x && y)
   {
      video_driver_publish_window_bounds(1, 2u, video_st->windows_edit[1],
            VIDEO_POS_PACK(300, 200), VIDEO_SCALE_PACK(800, 600), true);
      window_test_run();
      CHECK(settings->uints.video_screen_window_pos[1]
            == VIDEO_POS_PACK(300, 200));
      x->actions->repr(x, s, sizeof(s));
      CHECK(string_is_equal(s, "300"));
      y->actions->repr(y, s, sizeof(s));
      CHECK(string_is_equal(s, "200"));
      width->actions->repr(width, s, sizeof(s));
      CHECK(string_is_equal(s, "800"));
      height->actions->repr(height, s, sizeof(s));
      CHECK(string_is_equal(s, "600"));
      CHECK(menu_action_handle_setting(x, 0, MENU_ACTION_RIGHT, false) == 0);
      CHECK(settings->uints.video_screen_window_pos[1]
            == VIDEO_POS_PACK(310, 200));
      CHECK(settings->uints.video_screen_window_dims[1]
            == VIDEO_SCALE_PACK(800, 600));
      CHECK(settings->bools.video_screen_window_pos_set[1]);

      video_driver_publish_window_bounds(1, 2u, video_st->windows_edit[1],
            VIDEO_POS_PACK(320, 220), VIDEO_SCALE_PACK(700, 500), true);
      window_test_run();
      CHECK(menu_action_handle_setting(width, 0, MENU_ACTION_RIGHT, false)
            == 0);
      CHECK(settings->uints.video_screen_window_dims[1]
            == VIDEO_SCALE_PACK(710, 500));
      CHECK(settings->uints.video_screen_window_pos[1]
            == VIDEO_POS_PACK(320, 220));
      video_driver_publish_window_bounds(1, 2u, video_st->windows_edit[1],
            VIDEO_POS_PACK(320, 220), VIDEO_SCALE_PACK(730, 530), false);
      window_test_run();
      CHECK(menu_action_handle_setting(height, 0, MENU_ACTION_LEFT, false)
            == 0);
      CHECK(settings->uints.video_screen_window_dims[1]
            == VIDEO_SCALE_PACK(730, 520));

      /* Start puts X alone back to Auto */
      video_driver_publish_window_bounds(1, 2u, video_st->windows_edit[1],
            VIDEO_POS_PACK(330, 230), VIDEO_SCALE_PACK(720, 520), true);
      window_test_run();
      CHECK(menu_action_handle_setting(x, 0, MENU_ACTION_START, false) == 0);
      CHECK(settings->uints.video_screen_window_pos[1]
            == VIDEO_POS_PACK(0, 230));
      CHECK(settings->uints.video_screen_window_dims[1]
            == VIDEO_SCALE_PACK(720, 520));
      CHECK(!settings->bools.video_screen_window_pos_set[1]);
   }
   window_test_end();
   settings->uints.video_screen_window_pos[1]     = 0;
   settings->uints.video_screen_window_dims[1]    = 0;
   settings->bools.video_screen_window_pos_set[1] = false;
   menu_state_get_ptr()->flags &= ~(MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                  | MENU_ST_FLAG_PREVENT_POPULATE);
}

/* A menu the rows' tests drive as the menu runs: a driver that is not
 * the null one, so lists can be pushed, and a stack of its own */
static menu_ctx_driver_t window_menu_ctx;
static menu_handle_t window_menu_handle;
static file_list_t window_menu_stack;
static file_list_t window_menu_shown;
static file_list_t *window_menu_stacks[1];
static file_list_t *window_menu_selections[1];
static menu_list_t window_menu_list;

/* One press as the menu iterates it: the action on the row under the
 * cursor, then the rebuild it or the frame asked for */
static void window_menu_press(unsigned action)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   size_t i                   = menu_st->selection_ptr;
   menu_entry_t entry;
   MENU_ENTRY_INITIALIZE(entry);
   entry.flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                | MENU_ENTRY_FLAG_LABEL_ENABLED;
   menu_entry_get(&entry, 0, i, NULL, false);
   generic_menu_entry_action(NULL, &entry, i, (enum menu_action)action);
}

/* The list on top of the menu's stack is @label's */
static bool window_menu_top(enum msg_hash_enums label)
{
   return window_menu_stack.size && string_is_equal(
         window_menu_stack.list[window_menu_stack.size - 1].label,
         msg_hash_to_str(label));
}

/* The cursor is on @label */
static bool window_menu_on(const char *label)
{
   size_t i = menu_state_get_ptr()->selection_ptr;
   return i < window_menu_shown.size
      && string_is_equal(window_menu_shown.list[i].label, label);
}

/* The row @label of the list shown, as a menu driver reads it */
static void window_menu_entry(const char *label, menu_entry_t *entry)
{
   MENU_ENTRY_INITIALIZE((*entry));
   entry->flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                 | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                 | MENU_ENTRY_FLAG_VALUE_ENABLED;
   menu_entry_get(entry, 0, window_menu_index(&window_menu_shown, label),
         NULL, true);
}

/* The cursor put on @label, as navigation puts it */
static void window_menu_go(const char *label)
{
   menu_state_get_ptr()->selection_ptr = window_menu_index(
         &window_menu_shown, label);
   CHECK(window_menu_on(label));
}

/* Video's list, Game Screens' parent, built as the menu builds it */
static void window_menu_open(void)
{
   struct menu_state *menu_st          = menu_state_get_ptr();
   memset(&window_menu_ctx, 0, sizeof(window_menu_ctx));
   window_menu_ctx.ident               = "window_menu_test";
   memset(&window_menu_handle, 0, sizeof(window_menu_handle));
   window_menu_stacks[0]               = &window_menu_stack;
   window_menu_selections[0]           = &window_menu_shown;
   window_menu_list.menu_stack         = window_menu_stacks;
   window_menu_list.menu_stack_size    = 1;
   window_menu_list.selection_buf      = window_menu_selections;
   window_menu_list.selection_buf_size = 1;
   menu_st->driver_ctx                 = &window_menu_ctx;
   menu_st->driver_data                = &window_menu_handle;
   menu_st->entries.list               = &window_menu_list;
   menu_entries_append(&window_menu_stack, "",
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST),
         MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST, 0, 0, 0, NULL);
   /* A refresh wants a list shown */
   file_list_append(&window_menu_shown, "", "", 0, 0, 0);
   menu_st->selection_ptr              = 0;
   menu_st->flags                     |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   window_menu_press(MENU_ACTION_NOOP);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST));
}

/* Game Screen 2's page built again as a refresh builds it, with the
 * screens in @mask at Auto size */
static void window_menu_rows_build(unsigned mask)
{
   settings_t *settings       = config_get_ptr();
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned s;
   for (s = 0; s < 2; s++)
      if (mask & (1u << s))
      {
         settings->uints.video_screen_window_dims[s]    = 0;
         settings->bools.video_screen_window_pos_set[s] = false;
      }
   menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                   | MENU_ST_FLAG_PREVENT_POPULATE;
   window_menu_press(MENU_ACTION_NOOP);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
}

/* A key window_menu_rows_drag()'s frame presses on the row under the
 * cursor, after the drag is taken */
static unsigned window_menu_rows_key = MENU_ACTION_NOOP;

/* A frame's take, Game Screen 2 dragged by @step: what it asks the
 * menu for */
static void window_menu_rows_take(unsigned step, bool *redraw,
      bool *refresh)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   gfx_display_t *p_disp          = disp_get_ptr();
   p_disp->flags                 &= ~GFX_DISP_FLAG_FB_DIRTY;
   menu_st->flags                &= ~(MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE);
   if (step)
      video_driver_publish_window_bounds(1, 2u, video_st->windows_edit[1],
            VIDEO_POS_PACK(100 + step, 50), VIDEO_SCALE_PACK(640, 480 + step),
            true);
   window_test_run();
   menu_setting_video_screen_windows_moved();
   *redraw  = (p_disp->flags & GFX_DISP_FLAG_FB_DIRTY) != 0;
   *refresh = (menu_st->flags & (MENU_ST_FLAG_ENTRIES_NEED_REFRESH
            | MENU_ST_FLAG_PREVENT_POPULATE))
      == (MENU_ST_FLAG_ENTRIES_NEED_REFRESH | MENU_ST_FLAG_PREVENT_POPULATE);
}

/* A frame with the cursor on @label of Game Screen 2's page: the take,
 * then the frame's key and any rebuild. The cursor must be on @label in
 * the page after. */
static void window_menu_rows_drag(const char *label, unsigned step,
      bool *redraw, bool *refresh)
{
   window_menu_go(label);
   window_menu_rows_take(step, redraw, refresh);
   window_menu_press(window_menu_rows_key);
   window_menu_rows_key = MENU_ACTION_NOOP;
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
   CHECK(window_menu_on(label));
}

/* A move redraws a page's rows; an Auto-size screen's first one lists
 * X and Y too, and the cursor stays on its row whatever builds the
 * page */
static void test_window_menu_rows_redraw(void)
{
   settings_t *settings          = config_get_ptr();
   struct menu_state *menu_st    = menu_state_get_ptr();
   gfx_display_t *p_disp         = disp_get_ptr();
   const menu_ctx_driver_t *saved_ctx = menu_st->driver_ctx;
   menu_handle_t *saved_menu     = menu_st->driver_data;
   menu_list_t *saved_list       = menu_st->entries.list;
   size_t saved_selection        = menu_st->selection_ptr;
   uint16_t saved_menu_flags     = menu_st->flags;
   uint8_t saved_disp_flags      = p_disp->flags;
   const char *screen_row        = "video_screen2_settings";
   file_list_t *shown            = &window_menu_shown;
   size_t display, height;
   bool redraw, refresh;
   load_fixture("");
   window_test_begin();
   window_test_run();
   /* What earlier tests' takes left */
   menu_setting_video_screen_windows_moved();

   /* Entered from Game Screens with Game Screen 2 sized: the cursor on
    * top */
   settings->uints.video_screen_window_dims[1] = VIDEO_SCALE_PACK(640, 480);
   window_menu_open();
   window_menu_go(msg_hash_to_str(
            MENU_ENUM_LABEL_VIDEO_GAME_SCREENS_SETTINGS));
   window_menu_press(MENU_ACTION_OK);
   CHECK(window_menu_top(
            MENU_ENUM_LABEL_DEFERRED_VIDEO_GAME_SCREENS_SETTINGS_LIST));
   window_menu_go(screen_row);
   window_menu_press(MENU_ACTION_OK);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
   CHECK(menu_st->selection_ptr == 0);
   CHECK(window_menu_has(shown, "video_screen2_window_x"));

   window_menu_rows_build(2u);
   display = window_menu_index(shown, "video_screen2_display");
   height  = window_menu_index(shown, "video_screen2_window_height");
   CHECK(display == 0 && display < height && height + 1 == shown->size);
   CHECK(!window_menu_has(shown, "video_screen2_window_x"));

   /* No move, nothing asked */
   window_menu_rows_drag("video_screen2_display", 0, &redraw, &refresh);
   CHECK(!redraw && !refresh);
   /* Auto size, the cursor above Height: one refresh */
   window_menu_rows_drag("video_screen2_display", 1, &redraw, &refresh);
   CHECK(redraw && refresh);
   CHECK(window_menu_has(shown, "video_screen2_window_x"));
   /* The rest of that drag, sized now: a redraw alone */
   window_menu_rows_drag("video_screen2_display", 2, &redraw, &refresh);
   CHECK(redraw && !refresh);
   /* Bounds as saved: nothing changed */
   window_menu_rows_drag("video_screen2_display", 2, &redraw, &refresh);
   CHECK(!redraw && !refresh);

   /* The cursor on Height: X and Y go in below it */
   window_menu_rows_build(2u);
   window_menu_rows_drag("video_screen2_window_height", 3, &redraw,
         &refresh);
   CHECK(redraw && refresh);
   CHECK(window_menu_has(shown, "video_screen2_window_x"));
   /* The cursor on Window Y, Game Screen 2 Off from elsewhere: a
    * refresh drops its window rows, the cursor onto Display */
   window_menu_go("video_screen2_window_y");
   strlcpy(settings->arrays.video_screen_display[1],
         VIDEO_WINDOW_DISPLAY_OFF,
         sizeof(settings->arrays.video_screen_display[1]));
   menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                   | MENU_ST_FLAG_PREVENT_POPULATE;
   window_menu_press(MENU_ACTION_NOOP);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
   CHECK(window_menu_on("video_screen2_display"));
   CHECK(!window_menu_has(shown, "video_screen2_window_fullscreen"));
   settings->arrays.video_screen_display[1][0] = '\0';
   /* Its Display list open: a redraw alone; the return lists them */
   window_menu_rows_build(2u);
   window_menu_go("video_screen2_display");
   window_menu_press(MENU_ACTION_OK);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST));
   window_menu_rows_take(5, &redraw, &refresh);
   CHECK(redraw && !refresh);
   window_menu_press(MENU_ACTION_CANCEL);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
   CHECK(window_menu_on("video_screen2_display"));
   CHECK(window_menu_has(shown, "video_screen2_window_x"));

   /* Game Screen 1 on a monitor, its rows on its own page; window 2
    * shows it */
   {
      static video_window_display_t displays[1];
      video_driver_state_t *video_st = video_state_get_ptr();
      window_test_named(&displays[0], 1, false, "DP-1", "", 0);
      displays[0].monitor = true;
      video_driver_publish_window_displays(displays, 1);
      strlcpy(settings->arrays.video_screen_display[0], "DP-1",
            sizeof(settings->arrays.video_screen_display[0]));
      window_menu_rows_build(3u);
      CHECK(!window_menu_has(shown, "video_screen1_display"));
      CHECK(window_menu_index(shown, "video_screen2_window_height") + 1
            == shown->size);
      /* Game Screen 1 sized by a move: a redraw alone, page 2
       * unchanged */
      video_driver_publish_window_bounds(2, 1u, video_st->windows_edit[0],
            VIDEO_POS_PACK(60, 40), VIDEO_SCALE_PACK(320, 240), true);
      window_menu_rows_drag("video_screen2_display", 0, &redraw, &refresh);
      CHECK(redraw && !refresh);
      CHECK(!window_menu_has(shown, "video_screen1_window_x"));
      CHECK(!window_menu_has(shown, "video_screen2_window_x"));
      /* Both sized in one take: Game Screen 2's X and Y alone go in */
      window_menu_rows_build(3u);
      video_driver_publish_window_bounds(2, 1u, video_st->windows_edit[0],
            VIDEO_POS_PACK(62, 40), VIDEO_SCALE_PACK(320, 242), true);
      window_menu_rows_drag("video_screen2_display", 6, &redraw, &refresh);
      CHECK(redraw && refresh);
      CHECK(settings->uints.video_screen_window_dims[0]
            == VIDEO_SCALE_PACK(320, 242));
      CHECK(settings->uints.video_screen_window_dims[1]
            == VIDEO_SCALE_PACK(640, 486));
      CHECK(!window_menu_has(shown, "video_screen1_window_x"));
      CHECK(window_menu_has(shown, "video_screen2_window_x"));
      /* A Display step on Game Screen 2 in the drag's frame */
      window_menu_rows_build(3u);
      video_driver_publish_window_bounds(2, 1u, video_st->windows_edit[0],
            VIDEO_POS_PACK(65, 40), VIDEO_SCALE_PACK(320, 245), true);
      window_menu_rows_key = MENU_ACTION_RIGHT;
      window_menu_rows_drag("video_screen2_display", 0, &redraw, &refresh);
      CHECK(redraw && !refresh);
      CHECK(settings->arrays.video_screen_display[1][0] != '\0');
      CHECK(!window_menu_has(shown, "video_screen1_window_x"));
      settings->arrays.video_screen_display[1][0] = '\0';
      /* The same in the next frame */
      window_menu_rows_build(3u);
      video_driver_publish_window_bounds(2, 1u, video_st->windows_edit[0],
            VIDEO_POS_PACK(66, 40), VIDEO_SCALE_PACK(320, 246), true);
      window_menu_rows_drag("video_screen2_display", 0, &redraw, &refresh);
      CHECK(redraw && !refresh);
      window_menu_rows_key = MENU_ACTION_RIGHT;
      window_menu_rows_drag("video_screen2_display", 0, &redraw, &refresh);
      CHECK(!redraw && !refresh);
      CHECK(settings->arrays.video_screen_display[1][0] != '\0');
      settings->arrays.video_screen_display[1][0] = '\0';
      /* A Size-from-Auto edit on Game Screen 2 in the drag's frame */
      window_menu_rows_build(3u);
      video_driver_publish_window_bounds(2, 1u, video_st->windows_edit[0],
            VIDEO_POS_PACK(67, 40), VIDEO_SCALE_PACK(320, 247), true);
      window_menu_rows_key = MENU_ACTION_RIGHT;
      window_menu_rows_drag("video_screen2_window_width", 0, &redraw,
            &refresh);
      CHECK(redraw && !refresh);
      CHECK(settings->uints.video_screen_window_dims[1]
            == VIDEO_SCALE_PACK(640, 480));
      CHECK(!window_menu_has(shown, "video_screen1_window_x"));
      CHECK(window_menu_has(shown, "video_screen2_window_x"));
      /* The same in the next frame */
      window_menu_rows_build(3u);
      video_driver_publish_window_bounds(2, 1u, video_st->windows_edit[0],
            VIDEO_POS_PACK(68, 40), VIDEO_SCALE_PACK(320, 248), true);
      window_menu_rows_drag("video_screen2_window_width", 0, &redraw,
            &refresh);
      CHECK(redraw && !refresh);
      window_menu_rows_key = MENU_ACTION_RIGHT;
      window_menu_rows_drag("video_screen2_window_width", 0, &redraw,
            &refresh);
      CHECK(!redraw && !refresh);
      CHECK(settings->uints.video_screen_window_dims[1]
            == VIDEO_SCALE_PACK(640, 480));
      /* OK in the drag's frame, after a repeat: it acts, on its row */
      window_menu_rows_build(3u);
      settings->bools.video_screen_window_fullscreen[1] = false;
      window_menu_rows_key = MENU_ACTION_RIGHT;
      window_menu_rows_drag("video_screen2_window_fullscreen", 0, &redraw,
            &refresh);
      CHECK(settings->bools.video_screen_window_fullscreen[1]);
      video_driver_publish_window_bounds(2, 1u, video_st->windows_edit[0],
            VIDEO_POS_PACK(69, 40), VIDEO_SCALE_PACK(320, 249), true);
      window_menu_rows_key = MENU_ACTION_OK;
      window_menu_rows_drag("video_screen2_window_fullscreen", 0, &redraw,
            &refresh);
      CHECK(redraw && !refresh);
      CHECK(!settings->bools.video_screen_window_fullscreen[1]);

      /* OK on Game Screen 2's Display in the drag's frame opens its
       * list; a pick returns to the Display row */
      window_menu_rows_build(3u);
      video_driver_publish_window_bounds(2, 1u, video_st->windows_edit[0],
            VIDEO_POS_PACK(70, 40), VIDEO_SCALE_PACK(320, 250), true);
      window_menu_go("video_screen2_display");
      window_menu_rows_take(0, &redraw, &refresh);
      CHECK(redraw && !refresh);
      window_menu_press(MENU_ACTION_OK);
      CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST));
      /* Auto, its value now */
      menu_st->selection_ptr = 0;
      window_menu_press(MENU_ACTION_OK);
      CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
      CHECK(window_menu_on("video_screen2_display"));
      CHECK(!window_menu_has(shown, "video_screen1_window_x"));
      CHECK(!settings->arrays.video_screen_display[1][0]);
      /* The same, closed with Cancel */
      window_menu_rows_build(1u);
      video_driver_publish_window_bounds(2, 1u, video_st->windows_edit[0],
            VIDEO_POS_PACK(71, 40), VIDEO_SCALE_PACK(320, 251), true);
      window_menu_go("video_screen2_display");
      window_menu_rows_take(0, &redraw, &refresh);
      CHECK(redraw && !refresh);
      window_menu_press(MENU_ACTION_OK);
      CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST));
      window_menu_press(MENU_ACTION_CANCEL);
      CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
      CHECK(window_menu_on("video_screen2_display"));
      /* Game Screen 1 first sized while that list is open: a redraw
       * alone, and the return to the Display row */
      window_menu_rows_build(1u);
      window_menu_go("video_screen2_display");
      window_menu_press(MENU_ACTION_OK);
      CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST));
      video_driver_publish_window_bounds(2, 1u, video_st->windows_edit[0],
            VIDEO_POS_PACK(72, 40), VIDEO_SCALE_PACK(320, 252), true);
      window_menu_rows_take(0, &redraw, &refresh);
      CHECK(redraw && !refresh);
      window_menu_press(MENU_ACTION_CANCEL);
      CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
      CHECK(window_menu_on("video_screen2_display"));
      CHECK(!window_menu_has(shown, "video_screen1_window_x"));
      /* Left for Game Screens and entered again, Game Screen 2 sized
       * since: the cursor on top */
      window_menu_rows_build(2u);
      window_menu_go("video_screen2_window_height");
      window_menu_press(MENU_ACTION_CANCEL);
      CHECK(window_menu_top(
               MENU_ENUM_LABEL_DEFERRED_VIDEO_GAME_SCREENS_SETTINGS_LIST));
      CHECK(window_menu_on(screen_row));
      settings->uints.video_screen_window_dims[1] = VIDEO_SCALE_PACK(640,
            480);
      window_menu_press(MENU_ACTION_OK);
      CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
      CHECK(menu_st->selection_ptr == 0);
      CHECK(window_menu_has(shown, "video_screen2_window_x"));
      video_driver_publish_window_displays(NULL, 0);
      settings->arrays.video_screen_display[0][0]    = '\0';
      settings->uints.video_screen_window_pos[0]     = 0;
      settings->uints.video_screen_window_dims[0]    = 0;
      settings->bools.video_screen_window_pos_set[0] = false;
   }

   file_list_deinitialize(&window_menu_stack);
   file_list_deinitialize(&window_menu_shown);
   memset(&window_menu_stack, 0, sizeof(window_menu_stack));
   memset(&window_menu_shown, 0, sizeof(window_menu_shown));
   window_test_end();
   menu_st->driver_ctx    = saved_ctx;
   menu_st->driver_data   = saved_menu;
   menu_st->entries.list  = saved_list;
   menu_st->selection_ptr = saved_selection;
   menu_st->flags         = saved_menu_flags;
   p_disp->flags          = saved_disp_flags;
   settings->uints.video_screen_window_pos[1]     = 0;
   settings->uints.video_screen_window_dims[1]    = 0;
   settings->bools.video_screen_window_pos_set[1] = false;
}

/* Game Screens lists a row a screen, Screen N, showing its Display; OK
 * opens the screen's page: its Display, and its window rows under short
 * labels while it has a window of its own; a page past the core's
 * screens lists none */
static void test_window_menu_screen_page(void)
{
#ifndef ANDROID
   static video_window_display_t displays[1];
   settings_t *settings               = config_get_ptr();
   struct menu_state *menu_st         = menu_state_get_ptr();
   const menu_ctx_driver_t *saved_ctx = menu_st->driver_ctx;
   menu_handle_t *saved_menu          = menu_st->driver_data;
   menu_list_t *saved_list            = menu_st->entries.list;
   size_t saved_selection             = menu_st->selection_ptr;
   uint16_t saved_menu_flags          = menu_st->flags;
   file_list_t *shown                 = &window_menu_shown;
   menu_entry_t entry;
   char s[64];
   unsigned tries;
   load_fixture("");
   window_test_begin();
   window_test_run();
   window_menu_open();
   window_menu_go(msg_hash_to_str(
            MENU_ENUM_LABEL_VIDEO_GAME_SCREENS_SETTINGS));
   window_menu_press(MENU_ACTION_OK);
   CHECK(window_menu_top(
            MENU_ENUM_LABEL_DEFERRED_VIDEO_GAME_SCREENS_SETTINGS_LIST));

   /* A row a screen, then Restore; the Display rows are on the pages */
   CHECK(window_menu_index(shown, "video_screen1_settings") + 1
         == window_menu_index(shown, "video_screen2_settings"));
   CHECK(window_menu_index(shown, "video_screen2_settings") + 1
         == window_menu_index(shown, "video_screen_windows_restore"));
   CHECK(!window_menu_has(shown, "video_screen3_settings"));
   CHECK(!window_menu_has(shown, "video_screen1_display"));
   CHECK(!window_menu_has(shown, "video_screen2_window_fullscreen"));

   /* Called Screen N, its value its Display's */
   window_menu_entry("video_screen2_settings", &entry);
   CHECK(string_is_equal(entry.path, "Screen 2"));
   CHECK(string_is_equal(entry.value,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUTO)));

   /* OK opens Screen 2's page, under that title */
   window_menu_go("video_screen2_settings");
   window_menu_press(MENU_ACTION_OK);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
   menu_entries_get_title(s, sizeof(s));
   CHECK(string_is_equal(s, "Screen 2"));
   CHECK(menu_st->selection_ptr == 0);
   CHECK(window_menu_index(shown, "video_screen2_display") == 0);
   CHECK(window_menu_index(shown, "video_screen2_window_fullscreen") == 1);
   CHECK(window_menu_index(shown, "video_screen2_window_width") == 2);
   CHECK(window_menu_index(shown, "video_screen2_window_height") == 3);
   CHECK(!window_menu_has(shown, "video_screen2_window_x"));
   CHECK(!window_menu_has(shown, "video_screen1_display"));
   CHECK(!window_menu_has(shown, "video_screen_windows_restore"));
   /* Short labels on the page */
   window_menu_entry("video_screen2_display", &entry);
   CHECK(string_is_equal(entry.rich_label, "Display"));
   window_menu_entry("video_screen2_window_fullscreen", &entry);
   CHECK(string_is_equal(entry.rich_label, "Fullscreen"));
   window_menu_entry("video_screen2_window_width", &entry);
   CHECK(string_is_equal(entry.rich_label, "Window Width"));
   window_menu_entry("video_screen2_window_height", &entry);
   CHECK(string_is_equal(entry.rich_label, "Window Height"));
   /* X and Y once it has a size */
   settings->uints.video_screen_window_dims[1] = VIDEO_SCALE_PACK(640, 480);
   menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                   | MENU_ST_FLAG_PREVENT_POPULATE;
   window_menu_press(MENU_ACTION_NOOP);
   CHECK(window_menu_index(shown, "video_screen2_window_x") == 4);
   CHECK(window_menu_index(shown, "video_screen2_window_y") == 5);
   if (     window_menu_has(shown, "video_screen2_window_x")
         && window_menu_has(shown, "video_screen2_window_y"))
   {
      window_menu_entry("video_screen2_window_x", &entry);
      CHECK(string_is_equal(entry.rich_label, "Window X"));
      window_menu_entry("video_screen2_window_y", &entry);
      CHECK(string_is_equal(entry.rich_label, "Window Y"));
   }
   settings->uints.video_screen_window_dims[1] = 0;
   menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                   | MENU_ST_FLAG_PREVENT_POPULATE;
   window_menu_press(MENU_ACTION_NOOP);
   CHECK(!window_menu_has(shown, "video_screen2_window_x"));
   /* The Display list keeps the long name */
   window_menu_go("video_screen2_display");
   window_menu_press(MENU_ACTION_OK);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST));
   menu_entries_get_title(s, sizeof(s));
   CHECK(string_is_equal(s, "Game Screen 2 Display"));
   window_menu_press(MENU_ACTION_CANCEL);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
   window_menu_press(MENU_ACTION_CANCEL);
   CHECK(window_menu_top(
            MENU_ENUM_LABEL_DEFERRED_VIDEO_GAME_SCREENS_SETTINGS_LIST));

   /* Game Screen 1 in the main window: its page is Display alone; a
    * step to a monitor lists its window rows at once */
   window_test_named(&displays[0], 1, false, "DP-1", "", 0);
   displays[0].monitor = true;
   video_driver_publish_window_displays(displays, 1);
   window_menu_go("video_screen1_settings");
   window_menu_press(MENU_ACTION_OK);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
   CHECK(shown->size == 1);
   window_menu_go("video_screen1_display");
   for (tries = 0; tries < 4
         && !string_is_equal(settings->arrays.video_screen_display[0], "DP-1");
         tries++)
      window_menu_press(MENU_ACTION_RIGHT);
   CHECK(string_is_equal(settings->arrays.video_screen_display[0], "DP-1"));
   CHECK(window_menu_on("video_screen1_display"));
   CHECK(window_menu_has(shown, "video_screen1_window_fullscreen"));
   CHECK(!window_menu_has(shown, "video_screen2_display"));
   /* Start puts it back on Auto, the main window: they go at once */
   window_menu_press(MENU_ACTION_START);
   CHECK(!settings->arrays.video_screen_display[0][0]);
   CHECK(shown->size == 1
         && window_menu_has(shown, "video_screen1_display"));

   /* Screen 3's page while the core has 4 screens; once it has 2, a
    * refresh lists none of Screen 3's rows (D9) */
   window_menu_press(MENU_ACTION_CANCEL);
   CHECK(window_menu_top(
            MENU_ENUM_LABEL_DEFERRED_VIDEO_GAME_SCREENS_SETTINGS_LIST));
   set_menu_map(4);
   menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                   | MENU_ST_FLAG_PREVENT_POPULATE;
   window_menu_press(MENU_ACTION_NOOP);
   CHECK(window_menu_has(shown, "video_screen4_settings"));
   window_menu_go("video_screen3_settings");
   window_menu_press(MENU_ACTION_OK);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
   CHECK(window_menu_has(shown, "video_screen3_display"));
   set_menu_map(2);
   menu_st->flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                   | MENU_ST_FLAG_PREVENT_POPULATE;
   window_menu_press(MENU_ACTION_NOOP);
   CHECK(window_menu_top(MENU_ENUM_LABEL_DEFERRED_VIDEO_SCREEN_SETTINGS_LIST));
   CHECK(!window_menu_has(shown, "video_screen3_display"));
   CHECK(!window_menu_has(shown, "video_screen3_window_fullscreen"));
   CHECK(shown->size == 1 && window_menu_has(shown,
            msg_hash_to_str(MENU_ENUM_LABEL_NO_SETTINGS_FOUND)));
   menu_entries_get_title(s, sizeof(s));
   CHECK(string_is_equal(s, "Screen 3"));
   set_menu_map(0);

   video_driver_publish_window_displays(NULL, 0);
   settings->arrays.video_screen_display[0][0] = '\0';
   file_list_deinitialize(&window_menu_stack);
   file_list_deinitialize(&window_menu_shown);
   memset(&window_menu_stack, 0, sizeof(window_menu_stack));
   memset(&window_menu_shown, 0, sizeof(window_menu_shown));
   window_test_end();
   menu_st->driver_ctx    = saved_ctx;
   menu_st->driver_data   = saved_menu;
   menu_st->entries.list  = saved_list;
   menu_st->selection_ptr = saved_selection;
   menu_st->flags         = saved_menu_flags;
#endif
}

/* Each of these settings is listed in one list: Video's entries, Stereo
 * 3D, Game Screens, Headset and the Screen N pages */
static void test_window_menu_one_place(void)
{
#ifndef ANDROID
   static const enum menu_displaylist_ctl_state lists[] = {
      DISPLAYLIST_VIDEO_SETTINGS_LIST,
      DISPLAYLIST_VIDEO_STEREO_SETTINGS_LIST,
      DISPLAYLIST_VIDEO_GAME_SCREENS_SETTINGS_LIST,
      DISPLAYLIST_VIDEO_HEADSET_SETTINGS_LIST
   };
   static const char *const names[] = {
      "video_stereo_settings", "video_game_screens_settings",
      "video_stereo_mode", "video_stereo_swap_eyes", "video_screen_layout",
      "video_windows_enable", "video_screen_windows_restore",
      "video_screen1_settings", "video_screen2_settings",
      "video_screen1_display", "video_screen2_display",
      "video_screen2_window_fullscreen", "video_screen2_window_width",
      "video_screen2_window_height", "video_screen2_window_x",
      "video_screen2_window_y",
#ifdef HAVE_OPENXR
      "video_headset_settings", "video_openxr_enable",
      "video_openxr_refresh_rate", "video_openxr_distance",
      "video_screen_xr_reset_all",
#endif
   };
   settings_t *settings = config_get_ptr();
   unsigned seen[ARRAY_SIZE(names)];
   file_list_t list;
   unsigned l, n, s;
#ifdef HAVE_OPENXR
   char driver[sizeof(settings->arrays.video_driver)];
#endif
   load_fixture("");
   memset(seen, 0, sizeof(seen));
   window_test_begin();
   window_test_run();
#ifdef HAVE_OPENXR
   /* After the windows' start, which may name its own driver; the lists
    * read only the name */
   strlcpy(driver, settings->arrays.video_driver, sizeof(driver));
   strlcpy(settings->arrays.video_driver, "glcore",
         sizeof(settings->arrays.video_driver));
#endif
   settings->uints.video_screen_window_dims[1] = VIDEO_SCALE_PACK(640, 480);
   for (l = 0; l < ARRAY_SIZE(lists) + 2; l++)
   {
      memset(&list, 0, sizeof(list));
      if (l < ARRAY_SIZE(lists))
         menu_displaylist_build_list(&list, settings, lists[l], false);
      else
         window_menu_page_build(&list, l - ARRAY_SIZE(lists));
      for (s = 0; s < list.size; s++)
         for (n = 0; n < ARRAY_SIZE(names); n++)
            if (string_is_equal(list.list[s].label, names[n]))
               seen[n]++;
      file_list_deinitialize(&list);
   }
   for (n = 0; n < ARRAY_SIZE(names); n++)
      if (seen[n] != 1)
      {
         fprintf(stderr, "listed %u times: %s\n", seen[n], names[n]);
         CHECK(seen[n] == 1);
      }
   settings->uints.video_screen_window_dims[1] = 0;
#ifdef HAVE_OPENXR
   strlcpy(settings->arrays.video_driver, driver,
         sizeof(settings->arrays.video_driver));
#endif
   window_test_end();
#endif
}

#ifdef HAVE_THREADS
static slock_t *snapshot_lock;
static scond_t *snapshot_ready;
static bool snapshot_done;
static const video_windows_frame_t *snapshot_windows_at;
static video_windows_frame_t snapshot_windows;
static unsigned snapshot_drawn;
static int snapshot_data;

static void *snapshot_init(const video_info_t *info)
{
   (void)info;
   return &snapshot_data;
}

static bool snapshot_frame(void *data, const void *frame, unsigned dims,
      uint64_t count, unsigned pitch, const char *msg,
      video_frame_info_t *info)
{
   (void)data;
   (void)frame;
   (void)dims;
   (void)count;
   (void)pitch;
   (void)msg;
   slock_lock(snapshot_lock);
   snapshot_windows_at = info->windows;
   if (info->windows)
      snapshot_windows = *info->windows;
   /* The serial a window layout published now carries */
   snapshot_drawn = video_state_get_ptr()->windows_drawn;
   snapshot_done = true;
   scond_signal(snapshot_ready);
   slock_unlock(snapshot_lock);
   return true;
}

static void snapshot_free(void *data)
{
   (void)data;
}
#endif

/* The threaded wrapper hands the video thread its own copy of the
 * frame's windows, never the main thread's, and stamps its serial
 * before the driver draws it */
static void test_window_frame_snapshot(void)
{
#ifdef HAVE_THREADS
   static video_frame_info_t frame;
   static video_driver_t driver;
   const video_driver_t *threaded = NULL;
   void *data                     = NULL;
   video_windows_frame_t *owned   = &video_state_get_ptr()->windows;
   video_info_t info;
   uint32_t pixel = 0;
   bool initialized;
   unsigned restore, serial;
   window_test_begin();
   window_test_run();
   window_test_end();
   CHECK(window_test_handed == owned);
   memset(&frame, 0, sizeof(frame));
   frame.windows = window_test_handed;
   restore       = owned->restore;
   /* A snapshot the main thread's frames have not drawn */
   serial        = ++owned->serial;
   memset(&driver, 0, sizeof(driver));
   memset(&info, 0, sizeof(info));
   driver.init        = snapshot_init;
   driver.frame       = snapshot_frame;
   driver.free        = snapshot_free;
   driver.ident       = "snapshot";
   info.input_scale   = 1;
   info.rgb32         = true;
   snapshot_lock      = slock_new();
   snapshot_ready     = scond_new();
   snapshot_done      = false;
   initialized        = video_init_thread(&threaded, &data, &driver, info);
   CHECK(initialized);
   if (initialized)
   {
      frame.views.num_views       = 2;
      frame.views.views[0].screen = 1;
      frame.views.views[1].screen = 0;
      slock_lock(snapshot_lock);
      CHECK(threaded->frame(data, &pixel, VIDEO_SCALE_PACK(1, 1),
               1, sizeof(pixel), NULL, &frame));
      /* rewritten for the next frame, after a settings change */
      owned->settings[1].mode   = VIDEO_WINDOW_HIDDEN;
      owned->plan.separate_mask = 0;
      owned->restore++;
      owned->serial++;
      if (!snapshot_done)
         scond_wait_timeout(snapshot_ready, snapshot_lock, 2000000);
      CHECK(snapshot_done);
      CHECK(snapshot_windows_at != NULL);
      CHECK(snapshot_windows_at != owned);
      CHECK(snapshot_windows.settings[1].mode == VIDEO_WINDOW_SEPARATE);
      CHECK(snapshot_windows.plan.separate_mask == 2u);
      CHECK(snapshot_windows.restore == restore);
      CHECK(snapshot_drawn == serial);
      slock_unlock(snapshot_lock);
      /* A frame without windows hands none on */
      frame.windows = NULL;
      slock_lock(snapshot_lock);
      snapshot_done = false;
      CHECK(threaded->frame(data, &pixel, VIDEO_SCALE_PACK(1, 1),
               2, sizeof(pixel), NULL, &frame));
      if (!snapshot_done)
         scond_wait_timeout(snapshot_ready, snapshot_lock, 2000000);
      CHECK(snapshot_done);
      CHECK(snapshot_windows_at == NULL);
      slock_unlock(snapshot_lock);
      threaded->free(data);
   }
   scond_free(snapshot_ready);
   slock_free(snapshot_lock);
#endif
}

#ifdef HAVE_THREADS
static video_frame_info_t snapshot_queued;

static bool snapshot_queue_frame(void *data, const void *frame, unsigned dims,
      uint64_t count, unsigned pitch, const char *msg,
      video_frame_info_t *info)
{
   (void)data;
   (void)frame;
   (void)dims;
   (void)count;
   (void)pitch;
   (void)msg;
   slock_lock(snapshot_lock);
   snapshot_queued = *info;
   snapshot_done = true;
   scond_signal(snapshot_ready);
   slock_unlock(snapshot_lock);
   return true;
}
#endif

static void test_screen_frame_snapshot(void)
{
   static video_frame_info_t frame;
   settings_t *settings = config_get_ptr();
   load_fixture("video_screen2_xr_x = \"0.25\"\n");
   settings->floats.video_screen_xr_scale[7] = 20.0f;
   settings->floats.video_screen_xr_y[1] = -0.3f;
   settings->floats.video_screen_xr_z[1] = 0.4f;
   settings->floats.video_screen_xr_yaw[1] = 45.0f;
   settings->floats.video_screen_xr_pitch[1] = -30.0f;
   settings->floats.video_screen_xr_roll[1] = 90.0f;
   settings->floats.video_screen_xr_x[7] = (float)HUGE_VAL;
   settings->floats.video_openxr_menu_xr_y     = 0.5f;
   settings->floats.video_openxr_menu_xr_scale = 20.0f;
   settings->bools.video_openxr_menu_placed    = true;
   memset(&frame, 0, sizeof(frame));
   video_driver_build_info(&frame);
   CHECK(!frame.headset_menu_follow);
   settings->bools.video_openxr_menu_follow    = true;
   video_driver_build_info(&frame);
   CHECK(frame.headset_menu_follow);
   settings->bools.video_openxr_menu_follow    = false;
   CHECK(frame.headset_menu.y == 0.5f);
   CHECK(frame.headset_menu.scale == 10.0f);
   CHECK(frame.headset_menu.visible);
   CHECK(frame.headset_menu_placed);
   CHECK(frame.headset_screens[1].x == 0.25f);
   CHECK(frame.headset_screens[7].scale == 10.0f);
   CHECK(frame.headset_screens[7].x == 0.0f);
   CHECK(frame.headset_screens[1].y == -0.3f);
   CHECK(frame.headset_screens[1].z == 0.4f);
   CHECK(frame.headset_screens[1].yaw == 45.0f);
   CHECK(frame.headset_screens[1].pitch == -30.0f);
   CHECK(frame.headset_screens[1].roll == 90.0f);
   CHECK(frame.headset_screens[0].visible);
#ifdef HAVE_THREADS
   {
      static video_driver_t driver;
      const video_driver_t *threaded = NULL;
      void *data = NULL;
      video_info_t info;
      uint32_t pixel = 0;
      bool initialized;
      memset(&driver, 0, sizeof(driver));
      memset(&info, 0, sizeof(info));
      driver.init = snapshot_init;
      driver.frame = snapshot_queue_frame;
      driver.free = snapshot_free;
      driver.ident = "snapshot";
      info.input_scale = 1;
      info.rgb32 = true;
      snapshot_lock = slock_new();
      snapshot_ready = scond_new();
      snapshot_done = false;
      initialized = video_init_thread(&threaded, &data, &driver, info);
      CHECK(initialized);
      if (initialized)
      {
         frame.views.num_views = 2;
         frame.views.views[0].screen = 1;
         frame.views.views[1].screen = 0;
         slock_lock(snapshot_lock);
         CHECK(threaded->frame(data, &pixel, VIDEO_SCALE_PACK(1, 1),
                  1, sizeof(pixel), NULL, &frame));
         settings->floats.video_screen_xr_x[1] = 0.75f;
         video_state_get_ptr()->views_core.num_views = 1;
         video_state_get_ptr()->views_core.views[0].screen = 0;
         frame.headset_screens[1].x = 0.9f;
         frame.views.views[0].screen = 0;
         frame.views.num_views = 1;
         if (!snapshot_done)
            scond_wait_timeout(snapshot_ready, snapshot_lock, 2000000);
         CHECK(snapshot_done);
         CHECK(snapshot_queued.headset_screens[1].x == 0.25f);
         CHECK(settings->floats.video_screen_xr_x[1] == 0.75f);
         CHECK(snapshot_queued.views.num_views == 2);
         CHECK(snapshot_queued.views.views[0].screen == 1);
         slock_unlock(snapshot_lock);
         threaded->free(data);
      }
      scond_free(snapshot_ready);
      slock_free(snapshot_lock);
   }
#endif
}

#ifdef HAVE_THREADS
static void redraw_viewport(void *data, struct video_viewport *vp)
{
   (void)data;
   (void)vp;
}

/* As gl2 and vulkan read a screenshot: the cached frame drawn again,
 * here on the video thread */
static bool redraw_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   (void)data;
   (void)buffer;
   (void)is_idle;
   window_test_run();
   return true;
}

/* A screenshot through the threaded wrapper, back on the main thread */
static void window_test_redraw(const video_driver_t *threaded, void *data)
{
   static uint8_t buffer[3];
   video_driver_state_t *video_st  = video_state_get_ptr();
   video_st->current_video         = (video_driver_t*)threaded;
   video_st->data                  = data;
   video_st->thread_wrapper_active = true;
   CHECK(threaded->read_viewport(data, buffer, false));
   video_st->current_video         = &window_test_driver;
   video_st->data                  = &window_test_driver;
   video_st->thread_wrapper_active = false;
   CHECK(window_test_handed == &video_st->windows_redraw);
}
#endif

/* A redraw of the cached frame, as a threaded screenshot or GPU
 * recording has, keeps the last frame's route */
static void test_window_redraw(void)
{
#ifdef HAVE_THREADS
   static video_driver_t driver;
   const video_driver_t *threaded = NULL;
   void *data                     = NULL;
   video_driver_state_t *video_st = video_state_get_ptr();
   const unsigned both            =
        VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU)
      | VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_NOTIFICATIONS);
   settings_t *settings           = config_get_ptr();
   video_window_display_t displays[3];
   video_window_display_t monitors[2];
   video_window_ui_target_t ui[VIDEO_WINDOW_UI_LAYERS];
   video_windows_route_t route;
   video_windows_route_t *held;
   video_info_t info;
   unsigned layer;
   window_test_display(&monitors[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "DP-1");
   window_test_display(&monitors[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "HDMI-A-1");
   monitors[0].monitor = monitors[1].monitor = true;
   window_test_display(&displays[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "Built-in Screen");
   window_test_display(&displays[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "Screen-2");
   window_test_display(&displays[2], 3, VIDEO_WINDOW_CONNECTION_DIRECT,
         false, "HDMI");
   memset(&driver, 0, sizeof(driver));
   memset(&info, 0, sizeof(info));
   driver.init          = snapshot_init;
   driver.frame         = window_test_frame;
   driver.free          = snapshot_free;
   driver.viewport_info = redraw_viewport;
   driver.read_viewport = redraw_read_viewport;
   driver.ident         = "redraw";
   info.input_scale     = 1;
   info.rgb32           = true;
   window_test_begin();
   if (video_init_thread(&threaded, &data, &driver, info))
   {
      /* The main thread's frames go straight to the test driver, as
       * without the wrapper; window_test_redraw() installs it */
      video_st->thread_wrapper_active = false;
      /* An external monitor takes Game Screen 1: Game Screen 2's Presentation
       * first on RetroArch's display; a redraw keeps the route */
      video_driver_publish_window_displays(displays, 3);
      window_test_run();
      CHECK(window_test_seen.display == 1);
      CHECK(!window_test_seen.monitors);
      window_test_redraw(threaded, data);
      CHECK(window_test_seen.display == 1);
      CHECK(window_test_seen.plan.main_mask == 1u);
      CHECK(window_test_seen.plan.separate_mask == 2u);
      CHECK(window_test_seen.window_screens[VIDEO_WINDOWS_PRESENTATION]
            == 2u);
      CHECK(window_test_seen.serial == video_st->windows.serial);

      /* With the UI, a keep and the logo routed: every UI layer is the
       * main window's, the rest as routed. The route the redraw holds. */
      held       = &video_st->windows_routes.slot[
            video_st->windows_routes.held - 1];
      route      = *held;
      memcpy(ui, video_st->windows_ui, sizeof(ui));
      held->ui   = both;
      held->keep = 1;
      held->logo = true;
      for (layer = 0; layer < VIDEO_WINDOW_UI_LAYERS; layer++)
         video_st->windows_ui[layer].window = VIDEO_WINDOWS_PRESENTATION;
      window_test_redraw(threaded, data);
      for (layer = 0; layer < VIDEO_WINDOW_UI_LAYERS; layer++)
      {
         CHECK(window_test_seen.ui[layer].window == 0);
         CHECK(!window_test_seen.ui[layer].wait);
      }
      CHECK(window_test_seen.window_ui[VIDEO_WINDOWS_PRESENTATION]
            == both);
      CHECK(window_test_seen.window_keep
            == 1u << VIDEO_WINDOWS_PRESENTATION);
      CHECK(window_test_seen.logo);
      CHECK(window_test_seen.serial == video_st->windows.serial);
      *held = route;
      memcpy(video_st->windows_ui, ui, sizeof(ui));

      /* Nowhere to go: still attached */
      displays[1].on = false;
      video_driver_publish_window_displays(displays, 2);
      window_test_run();
      window_test_redraw(threaded, data);
      CHECK(window_test_seen.display == 0);
      CHECK(window_test_seen.plan.main_mask == 3u);
      CHECK(window_test_seen.window_screens[1] == 0u);

      /* Desktop: a redraw takes the newest route handed over, never
       * the app thread's own copy */
      video_driver_publish_window_displays(monitors, 2);
      strlcpy(settings->arrays.video_screen_display[1], "HDMI-A-1",
            sizeof(settings->arrays.video_screen_display[1]));
      window_test_run();
      window_test_redraw(threaded, data);
      CHECK(window_test_seen.monitors);
      CHECK(window_test_seen.settings[1].monitor == 2);
      strlcpy(settings->arrays.video_screen_display[1], "DP-1",
            sizeof(settings->arrays.video_screen_display[1]));
      window_test_run();
      route = video_st->windows_route;
      video_st->windows_route.target[1] = 7;
      window_test_redraw(threaded, data);
      CHECK(window_test_seen.settings[1].monitor == 1);
      video_st->windows_route = route;
      settings->arrays.video_screen_display[1][0] = '\0';

      video_driver_publish_window_displays(NULL, 0);
      window_test_run();
      threaded->free(data);
   }
   else
      CHECK(false);
   window_test_end();
#endif
}

#ifdef HAVE_THREADS
/* The ids a renderer makes for the frame it was just handed, stepping
 * its windows as gl2 and vulkan do */
static unsigned redraw_made(video_windows_held_t *held, const char *from)
{
   unsigned w, retire, create;
   video_windows_steps(held, window_test_handed ? &window_test_seen : NULL,
         &retire, &create);
   held->live &= ~retire;
   for (w = 1; w < RETRO_VIDEO_VIEWS_MAX; w++)
      if (create & (1u << w))
      {
         held->live      |= 1u << w;
         held->display[w] = window_test_seen.display;
         fprintf(stderr, "redraw own moved: window %u made by the %s"
               " frame, Game Screen 2 on monitor %u\n", w, from,
               window_test_seen.settings[1].monitor);
      }
   return create;
}

static unsigned redraw_bits(unsigned bits)
{
   unsigned n = 0;
   for (; bits; bits &= bits - 1)
      n++;
   return n;
}
#endif

/* The main window dragged to the monitor an Auto window is on, under
 * the threaded wrapper: the window stays where it was made; after an
 * edit, redraws of the cached frame between the app thread's frames
 * make it again once, on the other monitor */
static void test_window_redraw_own_moved(void)
{
#ifdef HAVE_THREADS
   static video_driver_t driver;
   const video_driver_t *threaded = NULL;
   void *data                     = NULL;
   video_driver_state_t *video_st = video_state_get_ptr();
   video_window_display_t monitors[2];
   video_windows_held_t held;
   video_info_t info;
   unsigned i;
   unsigned made                  = 0;
   window_test_display(&monitors[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "DP-1");
   window_test_display(&monitors[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "HDMI-A-1");
   monitors[0].monitor = monitors[1].monitor = true;
   memset(&driver, 0, sizeof(driver));
   memset(&info, 0, sizeof(info));
   memset(&held, 0, sizeof(held));
   driver.init          = snapshot_init;
   driver.frame         = window_test_frame;
   driver.free          = snapshot_free;
   driver.viewport_info = redraw_viewport;
   driver.read_viewport = redraw_read_viewport;
   driver.ident         = "redraw";
   info.input_scale     = 1;
   info.rgb32           = true;
   window_test_begin();
   if (video_init_thread(&threaded, &data, &driver, info))
   {
      video_st->thread_wrapper_active = false;
      /* Game Screen 2 on Auto: the monitor RetroArch is not on */
      video_driver_publish_window_displays(monitors, 2);
      window_test_run();
      CHECK(redraw_bits(redraw_made(&held, "app")) == 1);
      CHECK(window_test_seen.settings[1].monitor == 2);
      window_test_redraw(threaded, data);
      CHECK(!redraw_made(&held, "redraw"));

      /* The main window dragged onto HDMI-A-1, as X11 republishes it */
      monitors[0].own = false;
      monitors[1].own = true;
      video_driver_publish_window_displays(monitors, 2);
      for (i = 0; i < 4; i++)
      {
         window_test_redraw(threaded, data);
         made += redraw_bits(redraw_made(&held, "redraw"));
         window_test_run();
         made += redraw_bits(redraw_made(&held, "app"));
      }
      fprintf(stderr, "redraw own moved, no edit: %u remake(s)\n", made);
      CHECK(made == 0);
      CHECK(window_test_seen.settings[1].monitor == 1);
      video_driver_edit_window(1);
      for (i = 0; i < 4; i++)
      {
         window_test_redraw(threaded, data);
         made += redraw_bits(redraw_made(&held, "redraw"));
         window_test_run();
         made += redraw_bits(redraw_made(&held, "app"));
      }
      fprintf(stderr, "redraw own moved: %u remake(s)\n", made);
      CHECK(made == 1);
      CHECK(window_test_seen.settings[1].monitor == 1);

      video_driver_publish_window_displays(NULL, 0);
      window_test_run();
      threaded->free(data);
   }
   else
      CHECK(false);
   window_test_end();
#endif
}

/* An edit between the app thread's frames: a redraw's route is from
 * before it, so the edit waits for the frame laid out with it */
static void test_window_redraw_edit(void)
{
#ifdef HAVE_THREADS
   static video_driver_t driver;
   const video_driver_t *threaded = NULL;
   void *data                     = NULL;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   video_window_display_t monitors[2];
   video_windows_held_t held;
   video_info_t info;
   unsigned edit;
   window_test_display(&monitors[0], 1, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         true, "DP-1");
   window_test_display(&monitors[1], 2, VIDEO_WINDOW_CONNECTION_UNKNOWN,
         false, "HDMI-A-1");
   monitors[0].monitor = monitors[1].monitor = true;
   memset(&driver, 0, sizeof(driver));
   memset(&info, 0, sizeof(info));
   memset(&held, 0, sizeof(held));
   driver.init          = snapshot_init;
   driver.frame         = window_test_frame;
   driver.free          = snapshot_free;
   driver.viewport_info = redraw_viewport;
   driver.read_viewport = redraw_read_viewport;
   driver.ident         = "redraw";
   info.input_scale     = 1;
   info.rgb32           = true;
   window_test_begin();
   if (video_init_thread(&threaded, &data, &driver, info))
   {
      video_st->thread_wrapper_active = false;
      strlcpy(settings->arrays.video_screen_display[1], "DP-1",
            sizeof(settings->arrays.video_screen_display[1]));
      video_driver_publish_window_displays(monitors, 2);
      window_test_run();
      CHECK(redraw_bits(redraw_made(&held, "app")) == 1);
      CHECK(window_test_seen.settings[1].monitor == 1);

      /* To Auto, the other monitor: only the app thread's frame knows */
      settings->arrays.video_screen_display[1][0] = '\0';
      video_driver_edit_window(1);
      edit = held.made[1].edit;
      window_test_redraw(threaded, data);
      CHECK(!redraw_made(&held, "redraw"));
      CHECK(!held.update);
      CHECK(held.made[1].edit == edit);
      window_test_run();
      CHECK(window_test_seen.settings[1].monitor == 2);
      CHECK(redraw_bits(redraw_made(&held, "app")) == 1);
      CHECK(held.made[1].monitor == 2);

      /* A placement edit: update() from that frame too, not the redraw */
      strlcpy(settings->arrays.video_screen_display[1], "HDMI-A-1",
            sizeof(settings->arrays.video_screen_display[1]));
      video_driver_edit_window(1);
      window_test_run();
      CHECK(!redraw_made(&held, "app"));
      settings->uints.video_screen_window_pos[1]     = VIDEO_POS_PACK(10, 20);
      settings->bools.video_screen_window_pos_set[1] = true;
      video_driver_edit_window(1);
      window_test_redraw(threaded, data);
      CHECK(!redraw_made(&held, "redraw"));
      CHECK(!held.update);
      window_test_run();
      CHECK(!redraw_made(&held, "app"));
      CHECK(held.update == 2u);
      CHECK(held.made[1].pos == VIDEO_POS_PACK(10, 20));

      /* Reopened by a redraw on the old route: moved by the frame with
       * the edit */
      video_driver_close_windows(&window_test_seen,
            window_test_seen.window_screens[1]);
      held.closed_edit[1] = window_test_seen.settings[1].edit;
      held.closed         = 2u;
      held.live           = 0u;
      window_test_run();
      CHECK(!redraw_made(&held, "app"));
      settings->arrays.video_screen_display[1][0] = '\0';
      settings->uints.video_screen_window_pos[1]     = 0;
      settings->bools.video_screen_window_pos_set[1] = false;
      video_driver_edit_window(1);
      window_test_redraw(threaded, data);
      CHECK(redraw_bits(redraw_made(&held, "redraw")) == 1);
      CHECK(held.made[1].monitor == 2);
      monitors[0].own = false;
      monitors[1].own = true;
      video_driver_publish_window_displays(monitors, 2);
      window_test_run();
      CHECK(window_test_seen.settings[1].monitor == 1);
      CHECK(redraw_bits(redraw_made(&held, "app")) == 1);
      CHECK(held.made[1].monitor == 1);

      video_driver_publish_window_displays(NULL, 0);
      window_test_run();
      threaded->free(data);
   }
   else
      CHECK(false);
   window_test_end();
#endif
}

/* A UI layer in another window lays out at its target's size, waiting
 * or not; one in the main window at the main window's UI size */
static void test_window_ui_layer_dims(void)
{
   static video_frame_info_t info;
   static video_windows_frame_t frame;
   memset(&info, 0, sizeof(info));
   memset(&frame, 0, sizeof(frame));
   info.dims = VIDEO_SCALE_PACK(800, 480);
   CHECK(video_frame_ui_layer_dims(&info, VIDEO_WINDOW_UI_MENU)
         == VIDEO_SCALE_PACK(800, 480));
   info.views_layout.ui_per_eye = true;
   info.views_layout.ui_dims    = VIDEO_SCALE_PACK(400, 480);
   CHECK(video_frame_ui_layer_dims(&info, VIDEO_WINDOW_UI_NOTIFICATIONS)
         == VIDEO_SCALE_PACK(400, 480));
   info.windows = &frame;
   frame.ui[VIDEO_WINDOW_UI_MENU].window = VIDEO_WINDOWS_PRESENTATION;
   frame.ui[VIDEO_WINDOW_UI_MENU].dims   = VIDEO_SCALE_PACK(1240, 1080);
   CHECK(video_frame_ui_layer_dims(&info, VIDEO_WINDOW_UI_MENU)
         == VIDEO_SCALE_PACK(1240, 1080));
   CHECK(video_frame_ui_layer_dims(&info, VIDEO_WINDOW_UI_NOTIFICATIONS)
         == VIDEO_SCALE_PACK(400, 480));
   frame.ui[VIDEO_WINDOW_UI_MENU].wait   = true;
   CHECK(video_frame_ui_layer_dims(&info, VIDEO_WINDOW_UI_MENU)
         == VIDEO_SCALE_PACK(1240, 1080));
   frame.ui[VIDEO_WINDOW_UI_MENU].window = 0;
   CHECK(video_frame_ui_layer_dims(&info, VIDEO_WINDOW_UI_MENU)
         == VIDEO_SCALE_PACK(400, 480));
   CHECK(video_frame_ui_layer_dims(&info, VIDEO_WINDOW_UI_LAYERS)
         == VIDEO_SCALE_PACK(400, 480));
}

#ifdef HAVE_GFX_WIDGETS
static gfx_display_ctx_draw_t logo_test_seen;
static unsigned logo_test_draws;

static void logo_test_draw(gfx_display_ctx_draw_t *draw, void *data,
      unsigned video_dims)
{
   (void)data;
   (void)video_dims;
   logo_test_seen = *draw;
   logo_test_draws++;
}

/* The idle logo: one quad, centred, a quarter of the shorter side; no
 * texture, nothing drawn */
static void test_window_idle_logo(void)
{
   static video_frame_info_t info;
   static gfx_display_ctx_driver_t driver;
   gfx_display_t *p_disp           = disp_get_ptr();
   gfx_display_ctx_driver_t *saved = p_disp->dispctx;
   memset(&info, 0, sizeof(info));
   memset(&driver, 0, sizeof(driver));
   driver.draw              = logo_test_draw;
   driver.handles_transform = true;
   p_disp->dispctx          = &driver;
   info.disp_userdata       = p_disp;
   info.dims                = VIDEO_SCALE_PACK(800, 480);
   logo_test_draws          = 0;
   gfx_widgets_draw_idle_logo(&info, 0);
   CHECK(logo_test_draws == 0);
   gfx_widgets_draw_idle_logo(&info, 7);
   CHECK(logo_test_draws == 1);
   CHECK(logo_test_seen.texture == 7);
   CHECK(logo_test_seen.dims == VIDEO_SCALE_PACK(120, 120));
   CHECK(logo_test_seen.pos
         == VIDEO_POS_PACK(VIDEO_PX(340.0f), VIDEO_PX(180.0f)));
   p_disp->dispctx = saved;
}
#endif
#include "native_windows.c"
#include "native_x11.c"
#include "native_wayland.c"
#include "native_win32.c"
#include "native_sdl.c"

/* A context of 'ident' with its main window up */
static const gfx_ctx_driver_t *native_context(const char *ident,
      enum gfx_ctx_api api, unsigned major, unsigned minor, void **data)
{
   const gfx_ctx_driver_t *ctx;
#if defined(HAVE_VULKAN) && defined(HAVE_WAYLAND)
   if (strcmp(ident, "vk_wayland") == 0)
      ctx = video_context_driver_init(false, config_get_ptr(), NULL,
            &gfx_ctx_vk_wayland, ident, api, major, minor, false, data);
   else
#endif
#if defined(HAVE_VULKAN) && defined(HAVE_SDL3)
   if (strcmp(ident, "vk_sdl3") == 0)
      ctx = video_context_driver_init(false, config_get_ptr(), NULL,
            &gfx_ctx_sdl3_vk, ident, api, major, minor, false, data);
   else
#endif
#if defined(NATIVE_WIN32) && defined(HAVE_VULKAN)
   if (strcmp(ident, "vk_w") == 0)
      ctx = video_context_driver_init(false, config_get_ptr(), NULL,
            &gfx_ctx_w_vk, ident, api, major, minor, false, data);
   else
#endif
#if defined(HAVE_VULKAN) && defined(HAVE_X11) && defined(HAVE_XCB)
   if (strcmp(ident, "vk_x") == 0)
      ctx = video_context_driver_init(false, config_get_ptr(), NULL,
            &gfx_ctx_vk_x, ident, api, major, minor, false, data);
   else
#endif
   ctx = video_context_driver_init_first(NULL, ident, api, major, minor,
         false, data);
   /* A failed set_video_mode has freed the context already */
   if (     !ctx
         || !ctx->set_video_mode(*data, VIDEO_SCALE_PACK(640, 480), false))
   {
      CHECK(!"context");
      return NULL;
   }
   if (api == GFX_CTX_OPENGL_API)
      ctx->swap_interval(*data, 1);
   return ctx;
}

/* The main window remade, as a fullscreen toggle that is not done in
 * place or a video reinit from the menu remakes it, with a screen's
 * window up: the driver's teardown keeps that window, and the next
 * driver's start takes it as it was, with the focus the user left
 * where it was (on the main window, then on the screen's); a context
 * up and gone in between, as a probe that fails is, leaves it kept.
 * X11's checks the window system's side too: native_x11_kept(). */
static void native_window_kept(const char *ident, enum gfx_ctx_api api,
      unsigned major, unsigned minor)
{
   unsigned round;
#if defined(NATIVE_SDL) && !defined(HAVE_SDL3)
   unsigned video_refs = 0;
   /* Video down and another subsystem up, as the sdl2 joypad driver
    * leaves SDL: the first context holds the only video reference */
   if (native_sdl_ident(ident))
   {
      SDL_InitSubSystem(SDL_INIT_EVENTS);
      for (; SDL_WasInit(SDL_INIT_VIDEO); video_refs++)
         SDL_QuitSubSystem(SDL_INIT_VIDEO);
   }
#endif
   for (round = 0; round < 2; round++)
   {
      video_window_request_t request;
      video_window_native_t window;
      void *state, *surface;
      unsigned dims;
      bool focused;
      unsigned display = 0;
      bool took        = false;
      void *data       = NULL;
      const gfx_ctx_driver_t *ctx = native_context(ident, api, major,
            minor, &data);
      if (!ctx)
         return;
#ifdef NATIVE_SDL
      if (native_sdl_ident(ident) && native_sdl_no_windows(ctx, data))
         return;
#endif
#ifdef NATIVE_WIN32
      if (native_w32_ident(ident))
         native_window_input = native_w32_pump;
#endif
      memset(&request, 0, sizeof(request));
      memset(&window, 0, sizeof(window));
      request.screen        = 1;
      request.generation    = video_driver_window_generation();
      request.settings.mode = VIDEO_WINDOW_SEPARATE;
      request.settings.dims = VIDEO_SCALE_PACK(320, 240);
      CHECK(ctx->windows->create(data, &request, &window));
      native_window_wait(ctx, data, &request, &window,
            NATIVE_EVENT(RESUME), 0);
      native_window_settle(ctx, data, &request, 100000);
      if (round)
      {
#ifdef NATIVE_WIN32
         if (native_w32_ident(ident))
            native_w32_focus(ctx, data, &request, &window, true);
#endif
#ifdef NATIVE_SDL
         if (native_sdl_ident(ident))
            native_sdl_focus(ctx, data, &request, &window, true);
#endif
#ifdef HAVE_WAYLAND
         if (native_wl_ident(ident))
         {
            native_wl_input((gfx_ctx_wayland_data_t*)data, true);
            native_wl_focus(ctx, data, &request, &window);
            native_wl_input((gfx_ctx_wayland_data_t*)data, false);
         }
#endif
      }
      state   = window.state;
      surface = window.surface;
      dims    = window.dims;
      focused = window.focused;
      fprintf(stderr, "native %s: screen window focused %d before the "
            "remake: %d\n", ident, round, focused);

      if (ctx->windows->keep)
         ctx->windows->keep(data, &window);
      else
         ctx->windows->destroy(data, &window);
      CHECK(window.state == NULL);
      ctx->destroy(data);
      data = NULL;
      if (round && (data = ctx->init(NULL)))
         ctx->destroy(data);
      data = NULL;
      if (!(ctx = native_context(ident, api, major, minor, &data)))
         return;
      took = ctx->windows->take
         && ctx->windows->take(data, request.screen, &window, &display);
      fprintf(stderr, "native %s: remade main, screen focused %d: "
            "window %s, focused %d\n", ident, round,
            took ? "kept" : "made again", window.focused);
      CHECK(took);
#if defined(NATIVE_SDL) && !defined(HAVE_SDL3)
      /* The hold handed on, not the last reference quit under both */
      if (native_sdl_ident(ident) && !SDL_WasInit(SDL_INIT_VIDEO))
      {
         CHECK(!"SDL video down after the take");
         return;
      }
#endif
      if (!took)
         CHECK(ctx->windows->create(data, &request, &window));
      else
      {
         CHECK(window.state == state && window.surface == surface);
         CHECK(window.dims == dims);
         CHECK(window.generation == request.generation);
         CHECK(window.focused == focused);
         native_window_settle(ctx, data, &request, 300000);
         CHECK(window.state == state);
      }
#ifdef NATIVE_WIN32
      if (took && round && native_w32_ident(ident))
         CHECK(GetForegroundWindow() == native_w32_window(&window));
#endif
      ctx->windows->destroy(data, &window);
      ctx->destroy(data);
#ifdef NATIVE_WIN32
      native_window_input = NULL;
#endif
   }
#if defined(NATIVE_SDL) && !defined(HAVE_SDL3)
   if (native_sdl_ident(ident))
   {
      /* SDL as the tests before left it */
      for (; video_refs; video_refs--)
         SDL_InitSubSystem(SDL_INIT_VIDEO);
      SDL_QuitSubSystem(SDL_INIT_EVENTS);
   }
#endif
}

#if (defined(HAVE_SDL3) || defined(NATIVE_WGL)) && defined(HAVE_VULKAN)
/* A window kept by a GL context is not a Vulkan one's to take: SDL 3
 * makes it for one API, and WGL has set a pixel format on it. One is
 * made in its place. */
static void native_window_kept_gl(const char *gl_ident,
      const char *vk_ident)
{
   video_window_request_t request;
   video_window_native_t window;
   const gfx_ctx_driver_t *ctx;
   unsigned display   = 0;
   bool took;
   void *data         = NULL;
   settings_t *config = config_get_ptr();
   /* SDL 3's are up only as the configured context */
   strlcpy(config->arrays.video_context_driver, gl_ident,
         sizeof(config->arrays.video_context_driver));
   ctx = native_context(gl_ident, GFX_CTX_OPENGL_API, 0, 0, &data);
   strlcpy(config->arrays.video_context_driver, vk_ident,
         sizeof(config->arrays.video_context_driver));
   if (!ctx)
      return;
   memset(&request, 0, sizeof(request));
   memset(&window, 0, sizeof(window));
   request.screen        = 1;
   request.generation    = video_driver_window_generation();
   request.settings.mode = VIDEO_WINDOW_SEPARATE;
   request.settings.dims = VIDEO_SCALE_PACK(320, 240);
   CHECK(ctx->windows->create(data, &request, &window));
   native_window_wait(ctx, data, &request, &window,
         NATIVE_EVENT(RESUME), 0);
   ctx->windows->keep(data, &window);
   ctx->destroy(data);
   data = NULL;
   if (!(ctx = native_context(vk_ident, GFX_CTX_VULKAN_API, 1, 0, &data)))
      return;
   took = ctx->windows->take(data, request.screen, &window, &display);
   fprintf(stderr, "native %s: %s's window %s\n", vk_ident, gl_ident,
         took ? "taken" : "not taken");
   CHECK(!took);
   if (took)
      ctx->windows->destroy(data, &window);
   CHECK(ctx->windows->create(data, &request, &window));
   native_window_wait(ctx, data, &request, &window,
         NATIVE_EVENT(RESUME), 0);
   ctx->windows->destroy(data, &window);
   ctx->destroy(data);
}
#endif

/* What shows on the window system's side that a window is gone;
 * a Wayland client sees nothing */
typedef struct
{
#ifdef HAVE_X11
   Display *watch;
   Window win;
#endif
#ifdef NATIVE_WIN32
   HWND hwnd;
#endif
#ifdef NATIVE_SDL
   unsigned long id;
#endif
   void *state;
} native_gone_t;

static void native_gone_watch(const char *ident,
      const video_window_native_t *window, native_gone_t *gone)
{
   memset(gone, 0, sizeof(*gone));
   gone->state = window->state;
#ifdef HAVE_X11
   if (native_x11_ident(ident) && (gone->watch = XOpenDisplay(NULL)))
   {
      gone->win = native_x11_window(window);
      XSelectInput(gone->watch, gone->win, StructureNotifyMask);
      XSync(gone->watch, False);
   }
#endif
#ifdef NATIVE_WIN32
   if (native_w32_ident(ident))
      gone->hwnd = native_w32_window(window);
#endif
#ifdef NATIVE_SDL
   if (native_sdl_ident(ident))
      gone->id = (unsigned long)SDL_GetWindowID(
            ((native_sdl_state_t*)window->state)->win);
#endif
}

/* Whether the watched window is gone; the watch ends */
static bool native_gone(native_gone_t *gone)
{
   bool destroyed = true;
#ifdef HAVE_X11
   if (gone->watch)
   {
      retro_time_t deadline = cpu_features_get_time_usec() + 2000000;
      destroyed             = false;
      while (!destroyed && cpu_features_get_time_usec() < deadline)
      {
         XEvent event;
         XSync(gone->watch, False);
         while (XPending(gone->watch))
         {
            XNextEvent(gone->watch, &event);
            if (     event.type == DestroyNotify
                  && event.xdestroywindow.window == gone->win)
               destroyed = true;
         }
         if (!destroyed)
            retro_sleep(2);
      }
      XCloseDisplay(gone->watch);
      gone->watch = NULL;
   }
#endif
#ifdef NATIVE_WIN32
   if (gone->hwnd)
      destroyed = !IsWindow(gone->hwnd);
#endif
#ifdef NATIVE_SDL
   if (gone->id)
      destroyed =    !SDL_WasInit(SDL_INIT_VIDEO)
                  || !SDL_GetWindowFromID((Uint32)gone->id);
#endif
   return destroyed;
}

/* What the frontend runs once the next driver is up */
static void native_release_kept(void)
{
#ifdef HAVE_WAYLAND
   gfx_ctx_wl_release_kept();
#endif
#ifdef NATIVE_WIN32
   win32_window_release_kept();
#endif
   video_driver_release_kept_windows();
}

/* A kept window nobody takes goes, by the release the frontend runs
 * once the next driver is up (after init, in a threaded driver's
 * CMD_INIT, after a failed init and at quit), by a create for its
 * screen, and by a take on another visual (X11) or context (Wayland) */
static void native_window_kept_gone(const char *ident,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   unsigned round;
   unsigned rounds = 2;
#ifdef HAVE_X11
   if (native_x11_ident(ident))
      rounds = 3;
#endif
#ifdef HAVE_WAYLAND
   if (native_wl_ident(ident))
      rounds = 3;
#endif
   for (round = 0; round < rounds; round++)
   {
      video_window_request_t request;
      video_window_native_t window;
      native_gone_t gone;
      bool destroyed;
      unsigned display = 0;
      bool took        = false;
      void *data       = NULL;
      const gfx_ctx_driver_t *ctx = native_context(ident, api, major,
            minor, &data);
      if (!ctx)
         return;
#ifdef NATIVE_SDL
      if (native_sdl_ident(ident) && native_sdl_no_windows(ctx, data))
         return;
#endif
#ifdef NATIVE_WIN32
      if (native_w32_ident(ident))
         native_window_input = native_w32_pump;
#endif
      memset(&request, 0, sizeof(request));
      memset(&window, 0, sizeof(window));
      request.screen        = 1;
      request.generation    = video_driver_window_generation();
      request.settings.mode = VIDEO_WINDOW_SEPARATE;
      request.settings.dims = VIDEO_SCALE_PACK(320, 240);
      CHECK(ctx->windows->create(data, &request, &window));
      native_window_wait(ctx, data, &request, &window,
            NATIVE_EVENT(RESUME), 0);
      native_gone_watch(ident, &window, &gone);
      ctx->windows->keep(data, &window);
      ctx->destroy(data);
      data = NULL;
      if (!(ctx = native_context(ident, api, major, minor, &data)))
      {
         native_gone(&gone);
         return;
      }
      if (round == 1)
      {
         request.generation = video_driver_window_generation();
         CHECK(ctx->windows->create(data, &request, &window));
      }
      else if (round == 2)
      {
#ifdef HAVE_X11
         if (native_x11_ident(ident))
            took = x11_video_window_take(data, request.screen, &window,
                  &display, ~(VisualID)0);
#endif
#ifdef HAVE_WAYLAND
         if (native_wl_ident(ident))
            took = wl_video_window_take(&gone, request.screen, &window,
                  &display);
#endif
         CHECK(!took);
      }
      else
      {
#ifdef HAVE_WAYLAND
         if (native_wl_ident(ident))
            native_wl_kept_configure(ctx, data, &request,
                  (wl_video_window_t*)gone.state);
#endif
         native_release_kept();
      }
      destroyed = native_gone(&gone);
      fprintf(stderr, "native %s: kept, %s: %s\n", ident, round == 1
            ? "a create for its screen" : round == 2 ? "a mismatched take"
            : "the frontend's release", destroyed ? "gone" : "still up");
      CHECK(destroyed);
      if (round == 1)
         native_window_wait(ctx, data, &request, &window,
               NATIVE_EVENT(RESUME), 0);
      else if (!took && ctx->windows->take(data, request.screen, &window,
               &display))
         CHECK(!"taken after it went");
      if (window.state)
         ctx->windows->destroy(data, &window);
      ctx->destroy(data);
#ifdef NATIVE_WIN32
      native_window_input = NULL;
#endif
   }
}

#ifdef HAVE_WAYLAND
typedef struct
{
   const gfx_ctx_driver_t *ctx;
   void *data;
   video_window_native_t *window;
   bool took;
} native_wl_take_t;

static void native_wl_take_run(void *arg)
{
   unsigned display      = 0;
   native_wl_take_t *job = (native_wl_take_t*)arg;
   job->took             = job->ctx->windows->take(job->data, 1,
         job->window, &display);
}

/* Video turned threaded with a window kept: it is taken on the video
 * thread, so the seat's focus on this one, the frontend's, waits in
 * the queue for that thread's poll */
static void native_wl_kept_thread(const char *ident, enum gfx_ctx_api api,
      unsigned major, unsigned minor)
{
#ifdef HAVE_THREADS
   video_window_request_t request;
   video_window_native_t window;
   native_wl_take_t job;
   sthread_t *thread;
   void *data                  = NULL;
   const gfx_ctx_driver_t *ctx = native_context(ident, api, major, minor,
         &data);
   if (!ctx)
      return;
   memset(&request, 0, sizeof(request));
   memset(&window, 0, sizeof(window));
   request.screen        = 1;
   request.generation    = video_driver_window_generation();
   request.settings.mode = VIDEO_WINDOW_SEPARATE;
   request.settings.dims = VIDEO_SCALE_PACK(320, 240);
   CHECK(ctx->windows->create(data, &request, &window));
   native_window_wait(ctx, data, &request, &window,
         NATIVE_EVENT(RESUME), 0);
   CHECK(!window.focused);
   ctx->windows->keep(data, &window);
   ctx->destroy(data);
   data = NULL;
   if (!(ctx = native_context(ident, api, major, minor, &data)))
      return;
   job.ctx    = ctx;
   job.data   = data;
   job.window = &window;
   job.took   = false;
   if ((thread = sthread_create(native_wl_take_run, &job)))
      sthread_join(thread);
   CHECK(job.took);
   if (!job.took)
   {
      ctx->destroy(data);
      return;
   }
   wl_video_window_keyboard((gfx_ctx_wayland_data_t*)data,
         (struct wl_surface*)window.surface, true);
   fprintf(stderr, "native %s: taken on another thread, focus from this "
         "one %s\n", ident, window.focused ? "handled here" : "queued");
   CHECK(!window.focused);
   native_window_drain(ctx, data, &request);
   CHECK(window.focused);
   wl_video_window_keyboard((gfx_ctx_wayland_data_t*)data,
         (struct wl_surface*)window.surface, false);
   native_window_drain(ctx, data, &request);
   ctx->windows->destroy(data, &window);
   ctx->destroy(data);
#endif
}
#endif

/* 'core': a 3.2 core GL context, as glcore asks for; else the
 * driver's default one, as gl does */
static void test_window_native_lifecycle(const char *ident, bool core)
{
   video_window_request_t request;
   video_window_native_t window;
   unsigned restart;
   unsigned previous    = 0;
   enum gfx_ctx_api api = strstr(ident, "vk")
      ? GFX_CTX_VULKAN_API : strcmp(ident, "d3d") == 0
      ? GFX_CTX_NONE : GFX_CTX_OPENGL_API;
#ifdef NATIVE_WIN32
   /* This is the main thread, which may keep the main window up for
    * the next lifetime (win32_window_keep()) */
   if (native_w32_ident(ident))
      task_queue_init(false, NULL);
#endif
#ifdef HAVE_X11
   /* Off at the first connect, so the monitor worker starts lazily.
    * This is the main thread, where key events are acted on. */
   if (native_x11_ident(ident))
   {
      config_get_ptr()->bools.video_windows_enable = false;
      task_queue_init(false, NULL);
   }
#endif
   for (restart = 0; restart < 2; restart++)
   {
      const gfx_ctx_driver_t *ctx;
      void *ctx_data = NULL;
      memset(&request, 0, sizeof(request));
      memset(&window, 0, sizeof(window));
      request.screen        = 1;
      request.generation    = video_driver_window_generation();
      request.settings.mode = VIDEO_WINDOW_SEPARATE;
      request.settings.dims = VIDEO_SCALE_PACK(320, 240);
      CHECK(request.generation > previous);
      strlcpy(config_get_ptr()->arrays.video_context_driver, ident,
            sizeof(config_get_ptr()->arrays.video_context_driver));
#if defined(HAVE_VULKAN) && defined(HAVE_X11) && defined(HAVE_XCB)
      /* Vulkan contexts are not on the GL list */
      if (strcmp(ident, "vk_x") == 0)
         ctx = video_context_driver_init(false, config_get_ptr(), NULL,
               &gfx_ctx_vk_x, ident, api, 1, 0, false, &ctx_data);
      else
#endif
#if defined(HAVE_VULKAN) && defined(HAVE_WAYLAND)
      if (strcmp(ident, "vk_wayland") == 0)
         ctx = video_context_driver_init(false, config_get_ptr(), NULL,
               &gfx_ctx_vk_wayland, ident, api, 1, 0, false, &ctx_data);
      else
#endif
#if defined(HAVE_VULKAN) && defined(HAVE_SDL3)
      if (strcmp(ident, "vk_sdl3") == 0)
         ctx = video_context_driver_init(false, config_get_ptr(), NULL,
               &gfx_ctx_sdl3_vk, ident, api, 1, 0, false, &ctx_data);
      else
#endif
#ifdef NATIVE_WIN32
#ifdef HAVE_VULKAN
      if (strcmp(ident, "vk_w") == 0)
         ctx = video_context_driver_init(false, config_get_ptr(), NULL,
               &gfx_ctx_w_vk, ident, api, 1, 0, false, &ctx_data);
      else
#endif
      /* No context: the native table a Direct3D driver uses */
      if (strcmp(ident, "d3d") == 0)
         ctx = native_w32_d3d_init(&ctx_data);
      else
#endif
      ctx = video_context_driver_init_first(NULL, ident, api,
            api == GFX_CTX_VULKAN_API ? 1 : (core ? 3 : 0),
            api == GFX_CTX_VULKAN_API ? 0 : (core ? 2 : 0), false,
            &ctx_data);
      CHECK(ctx != NULL);
      if (!ctx)
         return;
      fprintf(stderr, "native context: requested %s, selected %s\n",
            ident, ctx->ident);
      CHECK(strcmp(ctx->ident, ident) == 0);
      if (strcmp(ctx->ident, ident) != 0)
      {
         ctx->destroy(ctx_data);
         return;
      }
      /* A failed set_video_mode has freed the context already */
      if (!ctx->set_video_mode(ctx_data, VIDEO_SCALE_PACK(640, 480), false))
      {
         CHECK(!"set_video_mode");
         return;
      }
#if defined(HAVE_X11) && defined(RARCH_HAVE_X11_MONITORS)
      /* First a context whose monitor worker cannot start */
      if (     !restart && native_x11_ident(ident)
            && !(ctx = native_x11_unlisted(ctx, &ctx_data, api,
                  api == GFX_CTX_VULKAN_API ? 1 : (core ? 3 : 0),
                  api == GFX_CTX_VULKAN_API ? 0 : (core ? 2 : 0))))
         return;
      /* Then, on two monitors, the toggle on from the start */
      if (     restart && native_x11_ident(ident)
            && !(ctx = native_x11_located(ctx, &ctx_data, api,
                  api == GFX_CTX_VULKAN_API ? 1 : (core ? 3 : 0),
                  api == GFX_CTX_VULKAN_API ? 0 : (core ? 2 : 0))))
         return;
#endif
      if (api == GFX_CTX_OPENGL_API)
         ctx->swap_interval(ctx_data, 1);
#ifdef NATIVE_SDL
      if (native_sdl_ident(ident) && native_sdl_no_windows(ctx, ctx_data))
         return;
#endif
      CHECK(ctx->windows != NULL);
      if (     ctx->windows
            && !(     ctx->windows->create && ctx->windows->update
                   && ctx->windows->poll   && ctx->windows->destroy))
      {
         CHECK(!"window operations");
         ctx->destroy(ctx_data);
         return;
      }
#ifdef NATIVE_WIN32
      if (native_w32_ident(ident))
         native_window_input = native_w32_pump;
#endif
      if (ctx->windows)
      {
         CHECK(ctx->windows->create(ctx_data, &request, &window));
         CHECK(window.generation == request.generation);
         CHECK(window.surface != NULL && window.state != NULL);
#ifdef HAVE_X11
         if (native_x11_ident(ident))
         {
            CHECK(window.kind == VIDEO_WINDOW_NATIVE_X11);
            if (window.state)
               native_x11_lifecycle(ctx, ctx_data, &request, &window);
         }
#endif
#ifdef HAVE_WAYLAND
         if (native_wl_ident(ident))
         {
            CHECK(window.kind == VIDEO_WINDOW_NATIVE_WAYLAND);
            native_wl_input((gfx_ctx_wayland_data_t*)ctx_data, true);
            if (window.state)
               native_wl_lifecycle(ctx, ctx_data, &request, &window);
         }
#endif
#ifdef NATIVE_WIN32
         if (native_w32_ident(ident))
         {
            CHECK(window.kind == VIDEO_WINDOW_NATIVE_WIN32);
            if (window.state)
               native_w32_lifecycle(ctx, ctx_data, &request, &window);
         }
#endif
#ifdef NATIVE_SDL
         if (native_sdl_ident(ident))
         {
            CHECK(window.kind == NATIVE_SDL_KIND);
            if (window.state)
               native_sdl_lifecycle(ctx, ctx_data, &request, &window);
         }
#endif
         previous = window.generation;
#ifdef HAVE_X11
         /* The context takes the window down, with a close and a resize
          * queued for it that the next lifetime must not see */
         if (!restart && window.state && native_x11_ident(ident))
         {
            Window w = native_x11_window(&window);
            XResizeWindow(g_x11_dpy, w, 500, 400);
            native_x11_close(g_x11_dpy, w);
            XSync(g_x11_dpy, False);
         }
#endif
#ifdef HAVE_WAYLAND
         /* Focus as well, by the seat */
         if (!restart && window.state && native_wl_ident(ident))
         {
            native_wl_kwin(video_window_title_screen(&request),
                  "var g = w.frameGeometry;"
                  " w.frameGeometry = {x: g.x, y: g.y, width: 500,"
                  " height: 400}; w.closeWindow();");
            native_wl_flood((gfx_ctx_wayland_data_t*)ctx_data,
                  (struct wl_surface*)window.surface, 100, true, true, true);
         }
         if (native_wl_ident(ident))
            native_wl_input((gfx_ctx_wayland_data_t*)ctx_data, false);
#endif
#ifdef NATIVE_WIN32
         if (!restart && window.state && native_w32_ident(ident))
         {
            HWND w = native_w32_window(&window);
            PostMessage(w, WM_SIZE, SIZE_RESTORED, MAKELPARAM(500, 400));
            PostMessage(w, WM_CLOSE, 0, 0);
         }
#endif
#ifdef NATIVE_SDL
         if (!restart && window.state && native_sdl_ident(ident))
         {
            SDL_Window *w = (SDL_Window*)window.surface;
            native_sdl_push(w, NATIVE_SDL_RESIZE, 500, 400);
            native_sdl_push(w, NATIVE_SDL_CLOSE, 0, 0);
         }
#endif
      }
      ctx->destroy(ctx_data);
      CHECK(window.state == NULL && window.surface == NULL);
#ifdef HAVE_X11
      if (native_x11_ident(ident))
      {
         CHECK(native_x11_titled(g_x11_dpy,
               video_window_title_screen(&request)) == 0);
         native_x11_torn_down(!restart);
      }
#endif
#ifdef NATIVE_WIN32
      if (native_w32_ident(ident))
      {
         native_w32_torn_down(video_window_title_screen(&request));
         native_window_input = NULL;
      }
#endif
#ifdef NATIVE_SDL
      if (native_sdl_ident(ident))
         native_sdl_torn_down(ctx, !restart);
#endif
   }
#ifdef HAVE_X11
   if (native_x11_ident(ident))
      native_x11_kept(ident, api,
            api == GFX_CTX_VULKAN_API ? 1 : (core ? 3 : 0),
            api == GFX_CTX_VULKAN_API ? 0 : (core ? 2 : 0));
   else
#endif
   if (strcmp(ident, "d3d") != 0)
      native_window_kept(ident, api,
            api == GFX_CTX_VULKAN_API ? 1 : (core ? 3 : 0),
            api == GFX_CTX_VULKAN_API ? 0 : (core ? 2 : 0));
   if (strcmp(ident, "d3d") != 0)
      native_window_kept_gone(ident, api,
            api == GFX_CTX_VULKAN_API ? 1 : (core ? 3 : 0),
            api == GFX_CTX_VULKAN_API ? 0 : (core ? 2 : 0));
#ifdef HAVE_WAYLAND
   if (native_wl_ident(ident))
      native_wl_kept_thread(ident, api,
            api == GFX_CTX_VULKAN_API ? 1 : (core ? 3 : 0),
            api == GFX_CTX_VULKAN_API ? 0 : (core ? 2 : 0));
#endif
#if defined(HAVE_SDL3) && defined(HAVE_VULKAN)
   if (strcmp(ident, "vk_sdl3") == 0)
      native_window_kept_gl("gl_sdl3", ident);
#endif
#if defined(NATIVE_WGL) && defined(HAVE_VULKAN)
   if (strcmp(ident, "vk_w") == 0)
   {
      native_window_input = native_w32_pump;
      native_window_kept_gl("wgl", ident);
      native_window_input = NULL;
   }
#endif
#ifdef NATIVE_WIN32
   if (native_w32_ident(ident))
      native_w32_teardown(ident, api, core);
#endif
#ifdef HAVE_WAYLAND
   if (native_wl_ident(ident))
      native_wl_teardown(ident, api, core);
#endif
}

/* A controller's Plays As player saved into a game override: gone with
 * the override unloaded and back with it loaded, placed at once each
 * time as the menu's Unload and Load Override place it; set back to
 * Auto, a controller the main config gives a player is saved as Auto. */
static void test_plays_as_override(void)
{
   static rarch_system_info_t system_info;
   settings_t *settings = config_get_ptr();
   uint8_t before[MAX_USERS];
   char device[64];
   char text[192];
   config_file_t *conf;
   unsigned minimal, player, u;

   memset(&system_info, 0, sizeof(system_info));
   system_info.info.library_name = "video_screens_test";
   for (minimal = 0; minimal < 2; minimal++)
   {
      snprintf(text, sizeof(text), "%sinput_max_users = \"4\"\n"
            "input_plays_as_device_1 = \"0002:0002 B\"\n"
            "input_plays_as_player_1 = \"3\"\n",
            minimal ? "config_save_minimal = \"true\"\n" : "");
      load_fixture(text);
      CHECK(settings->bools.config_save_minimal == (minimal != 0));
      for (u = 0; u < MAX_USERS; u++)
         settings->uints.input_joypad_index[u] = u;
      input_config_set_device_name(0, "A");
      input_config_set_device_display_name(0, "A");
      input_config_set_device_vid(0, 1);
      input_config_set_device_pid(0, 1);
      input_config_set_device_name(1, "B");
      input_config_set_device_display_name(1, "B");
      input_config_set_device_vid(1, 2);
      input_config_set_device_pid(1, 2);
      /* connected: only a connect takes an entry */
      input_config_plays_as_place(0, true);
      input_config_plays_as_place(1, true);
      CHECK(input_config_plays_as_player(1) == 3);
      CHECK(settings->uints.input_joypad_index[2] == 1);
      CHECK(input_config_plays_as_set(0, 2));
      input_autoconfigure_reallocate_ports(0);
      CHECK(settings->uints.input_joypad_index[1] == 0);

      path_set(RARCH_PATH_BASENAME, "plays_as_game.rom");
      CHECK(config_save_overrides(OVERRIDE_GAME, &system_info, false, NULL) == 1);
      strlcpy(generated_path, path_get(RARCH_PATH_CONFIG_OVERRIDE),
            sizeof(generated_path));
      conf = config_file_new_from_path_to_string(generated_path);
      CHECK(conf != NULL);
      if (conf)
      {
         CHECK(config_get_array(conf, "input_plays_as_device_1", device, sizeof(device))
               && string_is_equal(device, "0001:0001 A"));
         CHECK(config_get_uint(conf, "input_plays_as_player_1", &player) && player == 2);
         CHECK(config_get_entry(conf, "input_plays_as_device_2") == NULL);
         config_file_free(conf);
      }

      /* Unload Override */
      input_config_plays_as_players(before);
      CHECK(config_unload_override());
      input_autoconfigure_plays_as_reread(before);
      CHECK(input_config_plays_as_player(0) == 0);
      CHECK(input_config_plays_as_player(1) == 3);
      CHECK(settings->uints.input_joypad_index[0] == 0);
      CHECK(settings->uints.input_joypad_index[2] == 1);

      /* Load Override */
      input_config_plays_as_players(before);
      config_unload_override();
      CHECK(config_load_override_file(generated_path));
      input_autoconfigure_plays_as_reread(before);
      CHECK(input_config_plays_as_player(0) == 2);
      CHECK(input_config_plays_as_player(1) == 3);
      CHECK(settings->uints.input_joypad_index[1] == 0);
      CHECK(settings->uints.input_joypad_index[2] == 1);

      /* Unload Override, then Start on the override info row */
      input_config_plays_as_players(before);
      CHECK(config_unload_override());
      input_autoconfigure_plays_as_reread(before);
      input_config_plays_as_players(before);
      CHECK(config_load_override(&system_info));
      input_autoconfigure_plays_as_reread(before);
      CHECK(input_config_plays_as_player(0) == 2);
      CHECK(input_config_plays_as_player(1) == 3);
      CHECK(settings->uints.input_joypad_index[1] == 0);
      CHECK(settings->uints.input_joypad_index[2] == 1);

      CHECK(input_config_plays_as_set(0, 0));
      CHECK(input_config_plays_as_set(1, 0));
      CHECK(config_save_overrides(OVERRIDE_GAME, &system_info, false, NULL) == 1);
      conf = config_file_new_from_path_to_string(generated_path);
      CHECK(conf != NULL);
      if (conf)
      {
         CHECK(config_get_array(conf, "input_plays_as_device_1", device, sizeof(device))
               && string_is_equal(device, "0002:0002 B"));
         CHECK(config_get_uint(conf, "input_plays_as_player_1", &player) && player == 0);
         CHECK(config_get_entry(conf, "input_plays_as_device_2") == NULL);
         config_file_free(conf);
      }
      CHECK(config_unload_override());
      CHECK(input_config_plays_as_player(1) == 3);
      CHECK(config_load_override_file(generated_path));
      CHECK(input_config_plays_as_player(1) == 0);
      CHECK(config_unload_override());
      path_clear(RARCH_PATH_BASENAME);
      for (u = 0; u < 2; u++)
      {
         input_config_clear_device_name(u);
         input_config_clear_device_display_name(u);
         input_config_set_device_vid(u, 0);
         input_config_set_device_pid(u, 0);
         input_config_plays_as_release(u);
      }
   }
   load_fixture("");
}

/* A pad of one model connected in @slot, or unplugged from it. */
static void plays_as_pad(unsigned slot, bool connected)
{
   input_config_set_device_vid(slot, connected ? 1 : 0);
   input_config_set_device_pid(slot, connected ? 1 : 0);
   if (connected)
   {
      input_config_set_device_name(slot, "Pad");
      input_config_set_device_display_name(slot, "Pad");
      input_config_plays_as_place(slot, true);
      return;
   }
   input_config_clear_device_name(slot);
   input_config_clear_device_display_name(slot);
   input_config_plays_as_release(slot);
}

/* Two of a model, a game override giving the first Player 5: with it
 * unplugged, Unload Override, Load Override and Start on the override
 * row leave the other on Auto, and the first plays its own back. */
static void test_plays_as_override_twin(void)
{
   static rarch_system_info_t system_info;
   settings_t *settings = config_get_ptr();
   uint8_t before[MAX_USERS];
   char path[2048];
   unsigned step, u;

   memset(&system_info, 0, sizeof(system_info));
   system_info.info.library_name = "video_screens_test";
   for (step = 0; step < 3; step++)
   {
      load_fixture("input_max_users = \"4\"\n"
            "input_plays_as_device_1 = \"0001:0001 Pad\"\n"
            "input_plays_as_player_1 = \"2\"\n");
      for (u = 0; u < MAX_USERS; u++)
         settings->uints.input_joypad_index[u] = u;
      plays_as_pad(0, true);
      plays_as_pad(1, true);
      CHECK(input_config_plays_as_player(0) == 2);
      CHECK(input_config_plays_as_entry(1) == -1);
      CHECK(input_config_plays_as_set(0, 5));
      input_autoconfigure_reallocate_ports(0);
      path_set(RARCH_PATH_BASENAME, "plays_as_twin.rom");
      CHECK(config_save_overrides(OVERRIDE_GAME, &system_info, false, NULL) == 1);
      strlcpy(path, path_get(RARCH_PATH_CONFIG_OVERRIDE), sizeof(path));

      plays_as_pad(0, false);
      input_config_plays_as_players(before);
      if (step == 0)
         CHECK(config_unload_override());
      else if (step == 1)
      {
         config_unload_override();
         CHECK(config_load_override_file(path));
      }
      else
         CHECK(config_load_override(&system_info));
      input_autoconfigure_plays_as_reread(before);
      CHECK(input_config_plays_as_entry(1) == -1);
      CHECK(input_config_plays_as_player(1) == 0);
      plays_as_pad(0, true);
      CHECK(input_config_plays_as_player(0) == (step ? 5u : 2u));

      config_unload_override();
      path_clear(RARCH_PATH_BASENAME);
      for (u = 0; u < 2; u++)
         plays_as_pad(u, false);
   }
   load_fixture("");
}

/* Two of a model, the second given Player 4 by a game override: with
 * the first unplugged, loading it the first time, Start on its row and
 * Load Override give the second its Player 4, and the first, back,
 * plays its own. */
static void test_plays_as_override_unplugged(void)
{
   static rarch_system_info_t system_info;
   settings_t *settings = config_get_ptr();
   uint8_t before[MAX_USERS];
   char path[2048];
   unsigned step, u;

   memset(&system_info, 0, sizeof(system_info));
   system_info.info.library_name = "video_screens_test";
   for (step = 0; step < 3; step++)
   {
      load_fixture("input_max_users = \"4\"\n"
            "input_plays_as_device_1 = \"0001:0001 Pad\"\n"
            "input_plays_as_player_1 = \"2\"\n");
      for (u = 0; u < MAX_USERS; u++)
         settings->uints.input_joypad_index[u] = u;
      plays_as_pad(0, true);
      plays_as_pad(1, true);
      CHECK(input_config_plays_as_set(1, 4));
      input_autoconfigure_reallocate_ports(1);
      path_set(RARCH_PATH_BASENAME, "plays_as_unplugged.rom");
      CHECK(config_save_overrides(OVERRIDE_GAME, &system_info, false, NULL) == 1);
      strlcpy(path, path_get(RARCH_PATH_CONFIG_OVERRIDE), sizeof(path));
      /* the first time: from the main config, the second on Auto */
      if (step == 0)
      {
         input_config_plays_as_players(before);
         CHECK(config_unload_override());
         input_autoconfigure_plays_as_reread(before);
         CHECK(input_config_plays_as_entry(1) == -1);
      }

      plays_as_pad(0, false);
      input_config_plays_as_players(before);
      if (step == 0)
         CHECK(config_load_override_file(path));
      else if (step == 1)
         CHECK(config_load_override(&system_info));
      else
      {
         config_unload_override();
         CHECK(config_load_override_file(path));
      }
      input_autoconfigure_plays_as_reread(before);
      CHECK(input_config_plays_as_entry(1) == 1);
      CHECK(input_config_plays_as_player(1) == 4);
      plays_as_pad(0, true);
      CHECK(input_config_plays_as_entry(0) == 0);
      CHECK(input_config_plays_as_player(0) == 2);

      config_unload_override();
      path_clear(RARCH_PATH_BASENAME);
      for (u = 0; u < 2; u++)
         plays_as_pad(u, false);
   }
   load_fixture("");
}

int main(int argc, char **argv)
{
#ifdef VIDEO_SCREENS_RUNTIME_TEST
   if (getenv("RA_SCREEN_TEST_SCRIPT"))
      return rarch_main(argc, argv, NULL);
#endif
   if (     (argc == 3 || argc == 4)
         && strcmp(argv[1], "--native-windows") == 0)
   {
      rtime_init();
      retroarch_config_init();
      retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
      frontend_driver_init_first(NULL);
      config_set_defaults(config_get_ptr());
      retro_main_log_file_init(NULL, false);
      verbosity_enable();
      test_window_native_lifecycle(argv[2],
            argc == 3 || strcmp(argv[3], "gl") != 0);
      if (failures)
         return 1;
      puts("PASS native window lifecycle");
      return 0;
   }
   if (argc != 2 && argc != 3)
      return 2;
   fill_pathname_join(fixture_path, argv[1], "fixture.cfg", sizeof(fixture_path));
   fill_pathname_join(saved_path, argv[1], "saved.cfg", sizeof(saved_path));
   fill_pathname_join(override_path, argv[1], "override.cfg", sizeof(override_path));
   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   menu_state_get_ptr()->entries.list_settings = menu_setting_new();
   CHECK(menu_state_get_ptr()->entries.list_settings != NULL);
   if (argc == 2)
   {
      test_window_config();
      test_window_display_config();
      test_window_migration();
      test_window_menu_config();
      test_xr_config_roundtrip();
      test_xr_minimal_save();
      test_xr_override_unload();
      test_xr_config_invalid();
      test_xr_menu_place_config();
   }
   test_window_layout();
   test_window_frame();
   test_window_monitors_frame();
   test_window_fullscreen_hotkey();
   test_window_bounds_saved();
   test_window_bounds_edit();
   test_window_displays();
   test_window_display_names();
   test_window_moves();
   test_window_shown_lag();
   test_window_shown_moved();
   test_window_shown_reinit();
   test_window_ui_targets();
   test_window_ui_wait_moved();
   test_window_turned();
   test_window_turned_touch();
   test_window_turned_display();
   test_window_turned_waiting();
   test_window_pointer_mapping();
   test_window_pointer_release();
   test_window_menu_touch_window();
   test_window_menu_mouse_kept();
   test_window_generation();
   test_window_kept_pending();
   test_window_ui_layer_dims();
#ifdef HAVE_GFX_WIDGETS
   test_window_idle_logo();
#endif
   test_window_frame_snapshot();
   test_window_menu_settings();
   test_window_menu_display();
   test_window_menu_monitors();
   test_window_menu_display_dropdown();
   test_window_menu_rows();
   test_window_menu_follows_screens();
   test_window_menu_list();
   test_window_menu_window_rows();
   test_window_menu_rows_moved();
   test_window_menu_rows_redraw();
   test_window_menu_screen_page();
   test_window_menu_one_place();
   test_window_redraw();
   test_window_redraw_own_moved();
   test_window_redraw_edit();
   test_screen_frame_snapshot();
   test_xr_screen_menu();
   test_xr_menu_map_change();
   test_xr_menu_reset_all();
   test_xr_edit_writes();
#ifdef HAVE_OPENXR
   test_xr_menu_tracked_screen();
   test_xr_edit_rows();
   test_xr_laser_beam_row();
   test_xr_edit_smoothing_row();
   test_xr_edit_snap_angles_row();
   test_xr_edit_hinge_gap_row();
#endif
   test_plays_as_override();
   test_plays_as_override_twin();
   test_plays_as_override_unplugged();
   menu_setting_free(menu_state_get_ptr()->entries.list_settings);
   free(menu_state_get_ptr()->entries.list_settings);
   menu_state_get_ptr()->entries.list_settings = NULL;
   if (failures)
      return 1;
   puts("PASS video_screens_test");
   return 0;
}
