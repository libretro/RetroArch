/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2012 - Michael Lelli
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#undef main

#include <stdbool.h>
#include "../../driver.h"
#include "../../general.h"
#include "../../libretro.h"

#include "../../console/rgui/rgui.h"

#include "../../console/rarch_console_exec.h"
#include "../../console/rarch_console_input.h"
#include "../../console/rarch_console_main_wrap.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#ifdef HW_RVL
#include <sdcard/wiisd_io.h>
#endif
#include <sdcard/gcsd.h>
#include <fat.h>

#ifdef HAVE_FILE_LOGGER
FILE * log_fp;
#endif

uint32_t menu_framebuf[320 * 240];

char app_dir[PATH_MAX];
struct retro_system_info wii_core_info;

static const struct retro_keybind _wii_nav_binds[] = {
   { 0, 0, 0, GX_GC_UP | GX_GC_LSTICK_UP | GX_GC_RSTICK_UP | GX_CLASSIC_UP | GX_CLASSIC_LSTICK_UP | GX_CLASSIC_RSTICK_UP | GX_WIIMOTE_UP | GX_NUNCHUK_UP, 0 },
   { 0, 0, 0, GX_GC_DOWN | GX_GC_LSTICK_DOWN | GX_GC_RSTICK_DOWN | GX_CLASSIC_DOWN | GX_CLASSIC_LSTICK_DOWN | GX_CLASSIC_RSTICK_DOWN | GX_WIIMOTE_DOWN | GX_NUNCHUK_DOWN, 0 },
   { 0, 0, 0, GX_GC_LEFT | GX_GC_LSTICK_LEFT | GX_GC_RSTICK_LEFT | GX_CLASSIC_LEFT | GX_CLASSIC_LSTICK_LEFT | GX_CLASSIC_RSTICK_LEFT | GX_WIIMOTE_LEFT | GX_NUNCHUK_LEFT, 0 },
   { 0, 0, 0, GX_GC_RIGHT | GX_GC_LSTICK_RIGHT | GX_GC_RSTICK_RIGHT | GX_CLASSIC_RIGHT | GX_CLASSIC_LSTICK_RIGHT | GX_CLASSIC_RSTICK_RIGHT | GX_WIIMOTE_RIGHT | GX_NUNCHUK_RIGHT, 0 },
   { 0, 0, 0, GX_GC_A | GX_CLASSIC_A | GX_WIIMOTE_A | GX_WIIMOTE_2, 0 },
   { 0, 0, 0, GX_GC_B | GX_CLASSIC_B | GX_WIIMOTE_B | GX_WIIMOTE_1, 0 },
   { 0, 0, 0, GX_GC_START | GX_CLASSIC_PLUS | GX_WIIMOTE_PLUS, 0 },
   { 0, 0, 0, GX_GC_Z_TRIGGER | GX_CLASSIC_MINUS | GX_WIIMOTE_MINUS, 0 },
   { 0, 0, 0, GX_WIIMOTE_HOME | GX_CLASSIC_HOME, 0 },
};

static const struct retro_keybind *wii_nav_binds[] = {
   _wii_nav_binds
};

enum
{
   GX_DEVICE_NAV_UP = 0,
   GX_DEVICE_NAV_DOWN,
   GX_DEVICE_NAV_LEFT,
   GX_DEVICE_NAV_RIGHT,
   GX_DEVICE_NAV_A,
   GX_DEVICE_NAV_B,
   GX_DEVICE_NAV_START,
   GX_DEVICE_NAV_SELECT,
   GX_DEVICE_NAV_EXIT,
   GX_DEVICE_NAV_LAST
};

static bool folder_cb(const char *directory, rgui_file_enum_cb_t file_cb,
      void *userdata, void *ctx)
{
   bool core_chooser = (userdata) ? *(rgui_file_type_t *)userdata == RGUI_SETTINGS_CORE : false;

   if (!*directory)
   {
#ifdef HW_RVL
      file_cb(ctx, "sd:", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "usb:", RGUI_FILE_DEVICE, 0);
#endif
      file_cb(ctx, "carda:", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "cardb:", RGUI_FILE_DEVICE, 0);
      return true;
   }

   char _dir[PATH_MAX];
   snprintf(_dir, sizeof(_dir), "%s/", directory);
   DIR *dir = opendir(_dir);
   if (!dir)
      return false;

   struct dirent *entry;
   while ((entry = readdir(dir)))
   {
      char stat_path[PATH_MAX];
      snprintf(stat_path, sizeof(stat_path), "%s/%s", directory, entry->d_name);
      struct stat st;
      if (stat(stat_path, &st) < 0)
         continue;

      if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode))
         continue;

      if (core_chooser && (strstr(entry->d_name, ".dol") != entry->d_name + strlen(entry->d_name) - 4 ||
         strcasecmp(entry->d_name, "boot.dol") == 0))
         continue;

      file_cb(ctx,
            entry->d_name, S_ISDIR(st.st_mode) ?
            RGUI_FILE_DIRECTORY : RGUI_FILE_PLAIN, 0);
   }

   closedir(dir);
   return true;
}

static bool get_rom_path(rgui_handle_t *rgui)
{
   uint16_t old_input_state = 0;
   bool can_quit = false;
   bool first = true;

   for (;;)
   {
      uint16_t input_state = 0;

      input_wii.poll(NULL);

      for (unsigned i = 0; i < GX_DEVICE_NAV_LAST; i++)
      {
         input_state |= input_wii.input_state(NULL, wii_nav_binds, 0,
               RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
      }

      uint16_t trigger_state = input_state & ~old_input_state;
      rgui_action_t action = RGUI_ACTION_NOOP;

      // don't run anything first frame, only capture held inputs for old_input_state
      if (!first)
      {
         if (trigger_state & (1 << GX_DEVICE_NAV_EXIT))
         {
            if (can_quit)
               return false;
         }
         else
            can_quit = true;

         if (trigger_state & (1 << GX_DEVICE_NAV_B))
            action = RGUI_ACTION_CANCEL;
         else if (trigger_state & (1 << GX_DEVICE_NAV_A))
            action = RGUI_ACTION_OK;
         else if (trigger_state & (1 << GX_DEVICE_NAV_UP))
            action = RGUI_ACTION_UP;
         else if (trigger_state & (1 << GX_DEVICE_NAV_DOWN))
            action = RGUI_ACTION_DOWN;
         else if (trigger_state & (1 << GX_DEVICE_NAV_LEFT))
            action = RGUI_ACTION_LEFT;
         else if (trigger_state & (1 << GX_DEVICE_NAV_RIGHT))
            action = RGUI_ACTION_RIGHT;
         else if (trigger_state & (1 << GX_DEVICE_NAV_START))
            action = RGUI_ACTION_START;
         else if (trigger_state & (1 << GX_DEVICE_NAV_SELECT))
            action = RGUI_ACTION_SETTINGS;
      }
      else
      {
         first = false;
      }

      const char *ret = rgui_iterate(rgui, action);
      video_wii.frame(NULL, NULL,
            0, 0,
            0, NULL);

      if (ret)
      {
         g_console.initialize_rarch_enable = true;
         strlcpy(g_console.rom_path, ret, sizeof(g_console.rom_path));
         if (rarch_startup(NULL))
            return true;
      }

      old_input_state = input_state;
      rarch_sleep(10);
   }
}

int rarch_main(int argc, char **argv);

extern uint8_t _binary_console_font_bmp_start[];

static void get_environment_settings(void)
{
   getcwd(default_paths.port_dir, MAXPATHLEN);
   snprintf(default_paths.core_dir, sizeof(default_paths.core_dir), default_paths.port_dir);
   snprintf(default_paths.config_file, sizeof(default_paths.config_file), "%sretroarch.cfg", default_paths.port_dir);
   snprintf(default_paths.system_dir, sizeof(default_paths.system_dir), "%s/system", default_paths.core_dir);
   snprintf(default_paths.savestate_dir, sizeof(default_paths.savestate_dir), "%s/savestates", default_paths.core_dir);
   snprintf(default_paths.filesystem_root_dir, sizeof(default_paths.filesystem_root_dir), "/");
   snprintf(default_paths.filebrowser_startup_dir, sizeof(default_paths.filebrowser_startup_dir), default_paths.filesystem_root_dir);
   snprintf(default_paths.sram_dir, sizeof(default_paths.sram_dir), "%s/sram", default_paths.core_dir);
   snprintf(default_paths.input_presets_dir, sizeof(default_paths.input_presets_dir), "%s/presets/input", default_paths.core_dir);
   strlcpy(default_paths.executable_extension, ".dol", sizeof(default_paths.executable_extension));
}

int main(void)
{
#ifdef HW_RVL
   L2Enhance();
#endif

   fatInitDefault();
   getcwd(app_dir, sizeof(app_dir));

   get_environment_settings();

#ifdef HAVE_LOGGER
   g_extern.verbose = true;
   logger_init();
#endif
#ifdef HAVE_FILE_LOGGER
   g_extern.verbose = true;
   log_fp = fopen("/retroarch-log.txt", "w");
#endif

   config_set_defaults();
   input_wii.init();

   retro_get_system_info(&wii_core_info);
   RARCH_LOG("Core: %s\n", wii_core_info.library_name);

   video_wii.start();

   gx_video_t *gx = (gx_video_t*)driver.video_data;
   gx->menu_data = menu_framebuf;

   char tmp_path[PATH_MAX];
   const char *extension = default_paths.executable_extension;
   snprintf(tmp_path, sizeof(tmp_path), default_paths.core_dir);
   const char *path_prefix = tmp_path; 

   char full_path[1024];
   snprintf(full_path, sizeof(full_path), "%sCORE%s", path_prefix, extension);

   bool find_libretro_file = rarch_configure_libretro_core(full_path, path_prefix, path_prefix, 
   default_paths.config_file, extension);

   rarch_settings_set_default(&input_wii);
   rarch_config_load(default_paths.config_file, path_prefix, extension, find_libretro_file);
   init_libretro_sym();

   input_wii.post_init();

   rgui_handle_t *rgui = rgui_init("",
         menu_framebuf, RGUI_WIDTH * sizeof(uint32_t),
         _binary_console_font_bmp_start, folder_cb, NULL);
   rgui_iterate(rgui, RGUI_ACTION_REFRESH);

   int ret = 0;

   while (get_rom_path(rgui) && ret == 0)
   {
      bool repeat = false;

      input_wii.poll(NULL);

      do{
         repeat = rarch_main_iterate();
      }while(repeat && !g_console.frame_advance_enable);

      audio_stop_func();
   }

   if(path_file_exists(default_paths.config_file))
      rarch_config_save(default_paths.config_file);

   if(g_console.emulator_initialized)
      rarch_main_deinit();

   video_wii.stop();
   input_wii.free(NULL);

#ifdef HAVE_FILE_LOGGER
   fclose(log_fp);
#endif
#ifdef HAVE_LOGGER
   logger_shutdown();
#endif

   rgui_free(rgui);

   if(g_console.return_to_launcher)
      rarch_console_exec(g_console.launch_app_on_exit);

   return ret;
}

