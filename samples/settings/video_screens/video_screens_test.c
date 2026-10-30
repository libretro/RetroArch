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

#include "../../../configuration.h"
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
#include "../../../input/input_driver.h"
#include "../../../input/input_defines.h"
#include "../../../input/input_remapping.h"
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
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
   static const char *fields[] = {"mode", "x", "y", "width", "height", "monitor", "fullscreen"};
   static rarch_system_info_t system_info;
   settings_t *settings = config_get_ptr();
   config_file_t *conf;
   unsigned i, j, minimal;
   int value;
   bool flag;
   memset(&system_info, 0, sizeof(system_info));
   system_info.info.library_name = "video_screens_test";
   load_fixture("");
   CHECK(!settings->bools.video_windows_enable);
   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      CHECK(settings->uints.video_screen_window_mode[i] == (i ? VIDEO_WINDOW_SEPARATE : VIDEO_WINDOW_ATTACHED));
      CHECK(settings->uints.video_screen_window_dims[i] == 0);
      CHECK(settings->uints.video_screen_window_pos[i] == 0);
      CHECK(settings->uints.video_screen_window_monitor[i] == 0);
      CHECK(!settings->bools.video_screen_window_fullscreen[i]);
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
            "video_screen2_window_monitor = \"3\"\n"
            "video_screen2_window_fullscreen = \"true\"\n"
            "video_screen8_window_mode = \"0\"\n");
      CHECK(settings->bools.video_windows_enable);
      CHECK(settings->uints.video_screen_window_mode[0] == VIDEO_WINDOW_ATTACHED);
      CHECK(settings->uints.video_screen_window_mode[1] == VIDEO_WINDOW_HIDDEN);
      CHECK(settings->uints.video_screen_window_mode[1] == VIDEO_WINDOW_HIDDEN);
      CHECK(settings->uints.video_screen_window_pos[1] == VIDEO_POS_PACK(-1200, -30));
      settings->bools.config_save_minimal = minimal != 0;
      CHECK(config_save_file(saved_path));
      path_set(RARCH_PATH_CONFIG, saved_path);
      config_load();
      CHECK(settings->uints.video_screen_window_pos[1] == VIDEO_POS_PACK(-1200, -30));
      CHECK(settings->uints.video_screen_window_dims[1] == VIDEO_SCALE_PACK(640, 480));
      CHECK(settings->uints.video_screen_window_monitor[1] == 3);
      CHECK(settings->bools.video_screen_window_fullscreen[1]);
      CHECK(settings->uints.video_screen_window_mode[7] == VIDEO_WINDOW_ATTACHED);
      write_config(override_path, "video_windows_enable = \"false\"\n"
            "video_screen2_window_mode = \"0\"\n"
            "video_screen2_window_x = \"0\"\n"
            "video_screen2_window_y = \"0\"\n"
            "video_screen2_window_width = \"800\"\n"
            "video_screen2_window_height = \"600\"\n"
            "video_screen2_window_monitor = \"0\"\n"
            "video_screen2_window_fullscreen = \"false\"\n");
      CHECK(config_load_override_file(override_path));
      CHECK(!settings->bools.video_windows_enable);
      CHECK(settings->uints.video_screen_window_mode[1] == VIDEO_WINDOW_ATTACHED);
      CHECK(settings->uints.video_screen_window_pos[1] == VIDEO_POS_PACK(0, 0));
      CHECK(settings->uints.video_screen_window_dims[1] == VIDEO_SCALE_PACK(800, 600));
      path_set(RARCH_PATH_BASENAME, "window_game.rom");
      CHECK(config_save_overrides(OVERRIDE_GAME, &system_info, false, NULL) == 1);
      strlcpy(generated_path, path_get(RARCH_PATH_CONFIG_OVERRIDE), sizeof(generated_path));
      conf = config_file_new_from_path_to_string(generated_path);
      CHECK(conf != NULL);
      if (conf)
      {
         CHECK(config_get_int(conf, "video_screen2_window_x", &value) && value == 0);
         CHECK(config_get_int(conf, "video_screen2_window_width", &value) && value == 800);
         CHECK(config_get_int(conf, "video_screen2_window_monitor", &value) && value == 0);
         CHECK(config_get_int(conf, "video_screen2_window_mode", &value)
               && value == VIDEO_WINDOW_ATTACHED);
         CHECK(config_get_bool(conf, "video_windows_enable", &flag) && !flag);
         config_file_free(conf);
      }
      CHECK(config_unload_override());
      CHECK(settings->bools.video_windows_enable);
      CHECK(settings->uints.video_screen_window_mode[1] == VIDEO_WINDOW_HIDDEN);
      CHECK(settings->uints.video_screen_window_pos[1] == VIDEO_POS_PACK(-1200, -30));
      CHECK(config_load_override_file(generated_path));
      CHECK(settings->uints.video_screen_window_pos[1] == VIDEO_POS_PACK(0, 0));
      CHECK(!settings->bools.video_screen_window_fullscreen[1]);
      CHECK(settings->uints.video_screen_window_monitor[1] == 0);
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
   CHECK(settings->uints.video_screen_window_mode[0] == VIDEO_WINDOW_ATTACHED);
   CHECK(settings->uints.video_screen_window_mode[1] == VIDEO_WINDOW_SEPARATE);
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
         DISPLAYLIST_VIDEO_STEREO_SETTINGS_LIST, false);
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
         DISPLAYLIST_VIDEO_STEREO_SETTINGS_LIST, false);
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
   CHECK(menu_setting_follows("video_openxr_controllers",
         "video_openxr_perf_boost"));
   CHECK(menu_setting_follows("video_openxr_placement",
         "video_openxr_laser_beam"));
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

/* Laser Beam sits with the headset's rows: Edit Mode Only unless the
 * config says otherwise; the menu steps through its values. */
static void test_xr_laser_beam_row(void)
{
   settings_t *settings = config_get_ptr();
   load_fixture("video_driver = \"glcore\"\n");
   CHECK(menu_has_setting("video_openxr_laser_beam"));
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

/* Two 4x2 screens side by side in an 8x2 frame, drawn by the test
 * driver into an 800x480 window, the second screen Separate */
static void window_test_begin(void)
{
   static struct retro_video_view views[2];
   struct retro_video_views map;
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   memset(views, 0, sizeof(views));
   views[0].width  = views[1].width  = 4;
   views[0].height = views[1].height = 2;
   views[1].x      = 4;
   views[1].screen = 1;
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
   settings->bools.video_windows_enable          = true;
   settings->uints.video_screen_window_mode[1]   = VIDEO_WINDOW_SEPARATE;
   window_test_flags = (1u << GFX_CTX_FLAGS_VIDEO_VIEWS)
                     | (1u << GFX_CTX_FLAGS_VIDEO_WINDOWS);
   memset(&map, 0, sizeof(map));
   map.views     = views;
   map.num_views = 2;
   CHECK(video_driver_set_views(&map));
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
}

static unsigned window_test_placements(void)
{
   const video_views_layout_t *layout = video_driver_get_views_layout();
   return layout ? layout->num_placements : 0;
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

/* The frame's snapshot of the screens' windows, and the main window's
 * layout without the screens that have windows of their own */
static void test_window_frame(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   unsigned restore;
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

   /* Hidden: in no window */
   settings->uints.video_screen_window_mode[1] = VIDEO_WINDOW_HIDDEN;
   settings->uints.video_screen_window_pos[1]  = VIDEO_POS_PACK(-20, 30);
   window_test_run();
   CHECK(window_test_placements() == 1);
   CHECK(window_test_seen.plan.hidden_mask == 2u);
   CHECK(window_test_seen.settings[1].pos == VIDEO_POS_PACK(-20, 30));
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

   /* A close attaches a screen until the windows are restored */
   settings->uints.video_screen_window_mode[1] = VIDEO_WINDOW_SEPARATE;
   video_st->windows_forced_attached           = 2u;
   window_test_run();
   CHECK(window_test_placements() == 2);
   CHECK(window_test_seen.plan.main_mask == 3u);
   CHECK(window_test_seen.restore == restore);
   video_driver_restore_windows();
   CHECK(video_st->windows_forced_attached == 0u);
   window_test_run();
   CHECK(window_test_placements() == 1);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.restore == restore + 1);

   settings->bools.video_windows_enable = false;
   window_test_run();
   CHECK(window_test_placements() == 2);
   CHECK(window_test_handed == NULL);
   window_test_end();
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
   strlcpy(d->name, name, sizeof(d->name));
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
   unsigned restore;
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

   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(window_test_seen.display == 2);
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.window_screens[0] == 0u);
   CHECK(window_test_seen.window_screens[1] == 2u);
   CHECK(window_test_seen.restore == restore + 1);
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
   CHECK(window_test_seen.restore == restore + 1);

   /* A failure from a frame laid out before the last restore is not
    * held against the window the restore asked for */
   video_driver_attach_windows(2u, window_test_seen.restore - 1);
   window_test_run();
   CHECK(window_test_seen.plan.separate_mask == 2u);
   CHECK(window_test_seen.window_screens[1] == 2u);

   /* The renderer could not make it: attached until the next list */
   video_driver_attach_windows(2u, window_test_seen.restore);
   window_test_run();
   CHECK(window_test_seen.plan.main_mask == 3u);
   CHECK(window_test_seen.window_screens[1] == 0u);
   CHECK(window_test_placements() == 2);
   window_test_run();
   CHECK(window_test_seen.plan.main_mask == 3u);
   video_driver_publish_window_displays(displays, 2);
   window_test_run();
   CHECK(window_test_seen.plan.separate_mask == 2u);

   /* An external monitor takes screen 0; screen 1 stays, with touch */
   video_driver_publish_window_displays(displays, 3);
   window_test_run();
   CHECK(window_test_seen.display == 3);
   CHECK(window_test_seen.plan.main_mask == 2u);
   CHECK(window_test_seen.plan.separate_mask == 1u);
   CHECK(window_test_seen.window_screens[0] == 0u);
   CHECK(window_test_seen.window_screens[1] == 1u);
   CHECK(window_test_placements() == 1
         && video_driver_get_views_layout()->placements[0].view == 1);

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
      video_driver_publish_window_layout(1, window_test_generation,
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
   video_driver_publish_window_layout(1, gen, NULL, NULL, 0, 0);
   CHECK(!video_driver_translate_window_point(1, gen, 400, 240, true, &pos));
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

/* Screens 2 and up each have a window mode and a display in the menu;
 * the display picker goes through Auto and the displays published */
static void test_window_menu_settings(void)
{
   static video_window_display_t displays[2];
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   unsigned *picked               = &settings->uints.video_screen_window_monitor[1];
   rarch_setting_t *mode;
   rarch_setting_t *monitor;
   rarch_setting_t *enable;
   char s[128];
   load_fixture("");
   video_driver_publish_window_displays(displays, 0);
   CHECK(menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_MODE) == NULL);
   CHECK(menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_MONITOR) == NULL);
   CHECK(menu_setting_find_enum((enum msg_hash_enums)
            (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_MODE + RETRO_VIDEO_VIEWS_MAX - 1)) != NULL);
   mode    = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_MODE + 1));
   monitor = menu_setting_find_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_MONITOR + 1));
   enable  = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_WINDOWS_ENABLE);
   CHECK(mode != NULL);
   CHECK(monitor != NULL);
   CHECK(enable != NULL);
   if (!mode || !monitor || !enable)
      return;
   CHECK(string_is_equal(mode->name, "video_screen2_window_mode"));
   CHECK(string_is_equal(mode->short_description, "Screen 2"));
   CHECK(mode->value.target.unsigned_integer
         == &settings->uints.video_screen_window_mode[1]);
   CHECK(string_is_equal(monitor->name, "video_screen2_window_monitor"));
   CHECK(string_is_equal(monitor->short_description, "Screen 2 Display"));
   CHECK(monitor->value.target.unsigned_integer == picked);
   mode->actions->repr(mode, s, sizeof(s));
   CHECK(string_is_equal(s, "Separate"));
   settings->uints.video_screen_window_mode[1] = VIDEO_WINDOW_HIDDEN;
   mode->actions->repr(mode, s, sizeof(s));
   CHECK(string_is_equal(s, "Hidden"));
   settings->uints.video_screen_window_mode[1] = VIDEO_WINDOW_ATTACHED;
   mode->actions->repr(mode, s, sizeof(s));
   CHECK(string_is_equal(s, "Attached"));
   CHECK(mode->max == (float)VIDEO_WINDOW_HIDDEN);

   /* A change to any of them gives closed windows another try */
   video_st->windows_forced_attached = 2u;
   mode->actions->change(mode);
   CHECK(video_st->windows_forced_attached == 0u);
   video_st->windows_forced_attached = 2u;
   monitor->actions->change(monitor);
   CHECK(video_st->windows_forced_attached == 0u);
   video_st->windows_forced_attached = 2u;
   enable->actions->change(enable);
   CHECK(video_st->windows_forced_attached == 0u);

   /* Nothing published: Auto only, and a display set elsewhere kept */
   monitor->actions->repr(monitor, s, sizeof(s));
   CHECK(string_is_equal(s, "Auto"));
   CHECK(monitor->actions->right(monitor, 0, false) == 0);
   CHECK(*picked == 0);
   CHECK(monitor->actions->left(monitor, 0, false) == 0);
   CHECK(*picked == 0);
   *picked = 5;
   monitor->actions->repr(monitor, s, sizeof(s));
   CHECK(string_is_equal(s, "5"));

   /* As the Thor lists its panels */
   memset(displays, 0, sizeof(displays));
   displays[0].ordinal  = 1;
   displays[0].own      = true;
   displays[0].valid    = true;
   displays[0].on       = true;
   displays[0].physical = true;
   displays[0].dims     = VIDEO_SCALE_PACK(1920, 1080);
   strlcpy(displays[0].name, "Built-in Screen", sizeof(displays[0].name));
   displays[1].ordinal  = 2;
   displays[1].valid    = true;
   displays[1].on       = true;
   displays[1].physical = true;
   displays[1].dims     = VIDEO_SCALE_PACK(1240, 1080);
   strlcpy(displays[1].name, "Screen-2", sizeof(displays[1].name));
   video_driver_publish_window_displays(displays, 2);
   monitor->actions->repr(monitor, s, sizeof(s));
   CHECK(string_is_equal(s, "5"));
   CHECK(monitor->actions->ok(monitor, 0, false) == 1);
   CHECK(monitor->max == 5.0f);
   CHECK(monitor->actions->left(monitor, 0, false) == 0);
   CHECK(*picked == 2);
   monitor->actions->repr(monitor, s, sizeof(s));
   CHECK(string_is_equal(s, "2 (Screen-2 1240x1080)"));
   CHECK(monitor->actions->ok(monitor, 0, false) == 1);
   CHECK(monitor->max == 2.0f);
   CHECK(monitor->actions->left(monitor, 0, false) == 0);
   monitor->actions->repr(monitor, s, sizeof(s));
   CHECK(string_is_equal(s, "1 (Built-in Screen 1920x1080)"));
   CHECK(monitor->actions->left(monitor, 0, false) == 0);
   CHECK(*picked == 0);
   CHECK(monitor->actions->left(monitor, 0, false) == 0);
   CHECK(*picked == 2);
   CHECK(monitor->actions->right(monitor, 0, false) == 0);
   CHECK(*picked == 0);
   CHECK(monitor->actions->right(monitor, 0, false) == 0);
   CHECK(*picked == 1);
   *picked = 5;
   CHECK(monitor->actions->right(monitor, 0, false) == 0);
   CHECK(*picked == 0);
   /* No published display can have an ordinal past the list's size */
   *picked = VIDEO_WINDOW_DISPLAYS_MAX;
   CHECK(monitor->actions->ok(monitor, 0, false) == 1);
   CHECK(monitor->max == (float)VIDEO_WINDOW_DISPLAYS_MAX);
   *picked = 1000;
   CHECK(monitor->actions->ok(monitor, 0, false) == 1);
   CHECK(monitor->max == 2.0f);
   /* A display with no size reported */
   displays[1].dims = 0;
   video_driver_publish_window_displays(displays, 2);
   *picked = 2;
   monitor->actions->repr(monitor, s, sizeof(s));
   CHECK(string_is_equal(s, "2 (Screen-2)"));
   video_driver_publish_window_displays(displays, 0);
   monitor->max = (float)VIDEO_WINDOW_DISPLAYS_MAX;
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

/* The screens' rows sit in Stereo 3D & Screens where the driver has
 * windows: the switch, then each screen the core has from 2 on */
static void test_window_menu_list(void)
{
   settings_t *settings = config_get_ptr();
   file_list_t list;
   char s[256];
   load_fixture("");
   memset(&list, 0, sizeof(list));
   menu_displaylist_build_list(&list, settings,
         DISPLAYLIST_VIDEO_STEREO_SETTINGS_LIST, false);
   CHECK(!window_menu_has(&list, "video_windows_enable"));
   CHECK(!window_menu_has(&list, "video_screen2_window_mode"));
   file_list_deinitialize(&list);

   window_test_begin();
   window_test_run();
   memset(&list, 0, sizeof(list));
   menu_displaylist_build_list(&list, settings,
         DISPLAYLIST_VIDEO_STEREO_SETTINGS_LIST, false);
   CHECK(window_menu_has(&list, "video_windows_enable"));
   CHECK(window_menu_has(&list, "video_screen2_window_mode"));
   CHECK(window_menu_has(&list, "video_screen2_window_monitor"));
   CHECK(!window_menu_has(&list, "video_screen1_window_mode"));
   CHECK(!window_menu_has(&list, "video_screen3_window_mode"));
   CHECK(!window_menu_has(&list, "video_screen2_window_x"));
   file_list_deinitialize(&list);

   settings->bools.video_windows_enable = false;
   memset(&list, 0, sizeof(list));
   menu_displaylist_build_list(&list, settings,
         DISPLAYLIST_VIDEO_STEREO_SETTINGS_LIST, false);
   CHECK(window_menu_has(&list, "video_windows_enable"));
   CHECK(!window_menu_has(&list, "video_screen2_window_mode"));
   file_list_deinitialize(&list);
   window_test_end();

   menu_cbs_sublabel_for_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_MODE + 1), 0, 0, s, sizeof(s));
   CHECK(string_is_equal(s,
            msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_SCREEN_WINDOW_MODE)));
   menu_cbs_sublabel_for_enum((enum msg_hash_enums)
         (MENU_ENUM_LABEL_VIDEO_SCREEN_WINDOW_MONITOR + 7), 0, 0, s, sizeof(s));
   CHECK(string_is_equal(s,
            msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_SCREEN_WINDOW_MONITOR)));
   menu_cbs_sublabel_for_enum(MENU_ENUM_LABEL_VIDEO_WINDOWS_ENABLE,
         0, 0, s, sizeof(s));
   CHECK(string_is_equal(s,
            msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_WINDOWS_ENABLE)));
}

#ifdef HAVE_THREADS
static slock_t *snapshot_lock;
static scond_t *snapshot_ready;
static bool snapshot_done;
static const video_windows_frame_t *snapshot_windows_at;
static video_windows_frame_t snapshot_windows;
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
 * frame's windows, never the main thread's */
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
   unsigned restore;
   window_test_begin();
   window_test_run();
   window_test_end();
   CHECK(window_test_handed == owned);
   memset(&frame, 0, sizeof(frame));
   frame.windows = window_test_handed;
   restore       = owned->restore;
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
      if (!snapshot_done)
         scond_wait_timeout(snapshot_ready, snapshot_lock, 2000000);
      CHECK(snapshot_done);
      CHECK(snapshot_windows_at != NULL);
      CHECK(snapshot_windows_at != owned);
      CHECK(snapshot_windows.settings[1].mode == VIDEO_WINDOW_SEPARATE);
      CHECK(snapshot_windows.plan.separate_mask == 2u);
      CHECK(snapshot_windows.restore == restore);
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
   video_window_display_t displays[3];
   video_info_t info;
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
      /* An external monitor takes screen 0 */
      video_driver_publish_window_displays(displays, 3);
      window_test_run();
      CHECK(window_test_seen.display == 3);
      window_test_redraw(threaded, data);
      CHECK(window_test_seen.display == 3);
      CHECK(window_test_seen.plan.main_mask == 2u);
      CHECK(window_test_seen.plan.separate_mask == 1u);
      CHECK(window_test_seen.window_screens[1] == 1u);

      /* Nowhere to go: still attached */
      displays[1].on = false;
      video_driver_publish_window_displays(displays, 2);
      window_test_run();
      window_test_redraw(threaded, data);
      CHECK(window_test_seen.display == 0);
      CHECK(window_test_seen.plan.main_mask == 3u);
      CHECK(window_test_seen.window_screens[1] == 0u);

      video_driver_publish_window_displays(NULL, 0);
      window_test_run();
      threaded->free(data);
   }
   else
      CHECK(false);
   window_test_end();
#endif
}

int main(int argc, char **argv)
{
#ifdef VIDEO_SCREENS_RUNTIME_TEST
   if (getenv("RA_SCREEN_TEST_SCRIPT"))
      return rarch_main(argc, argv, NULL);
#endif
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
      test_xr_config_roundtrip();
      test_xr_minimal_save();
      test_xr_override_unload();
      test_xr_config_invalid();
      test_xr_menu_place_config();
   }
   test_window_layout();
   test_window_frame();
   test_window_displays();
   test_window_pointer_mapping();
   test_window_generation();
   test_window_frame_snapshot();
   test_window_menu_settings();
   test_window_menu_list();
   test_window_redraw();
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
   menu_setting_free(menu_state_get_ptr()->entries.list_settings);
   free(menu_state_get_ptr()->entries.list_settings);
   menu_state_get_ptr()->entries.list_settings = NULL;
   if (failures)
      return 1;
   puts("PASS video_screens_test");
   return 0;
}
