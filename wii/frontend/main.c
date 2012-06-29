/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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
#include "rgui.h"
#include "../../driver.h"
#include "../../general.h"
#include "../../libretro.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <fat.h>

#ifdef HAVE_FILE_LOGGER
FILE * log_fp;
#endif

static uint16_t menu_framebuf[RGUI_WIDTH * RGUI_HEIGHT];

static bool folder_cb(const char *directory, rgui_file_enum_cb_t file_cb,
      void *userdata, void *ctx)
{
   (void)userdata;

   DIR *dir = opendir(directory);
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

      file_cb(ctx,
            entry->d_name, S_ISDIR(st.st_mode) ?
            RGUI_FILE_DIRECTORY : RGUI_FILE_PLAIN);
   }

   closedir(dir);
   return true;
}

static bool get_rom_path(rgui_handle_t *rgui)
{
   uint16_t old_input_state = 0;
   bool can_quit = false;

   rgui_iterate(rgui, RGUI_ACTION_REFRESH);

   for (;;)
   {
      uint16_t input_state = 0;
      input_wii.poll(NULL);

      if (input_wii.key_pressed(NULL, RARCH_QUIT_KEY))
      {
         if (can_quit)
            return false;
      }
      else
         can_quit = true;

      for (unsigned i = 0; i < RARCH_FIRST_META_KEY; i++)
      {
         input_state |= input_wii.input_state(NULL, NULL, false,
               RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
      }

      uint16_t trigger_state = input_state & ~old_input_state;

      rgui_action_t action = RGUI_ACTION_NOOP;
      if (trigger_state & (1 << RETRO_DEVICE_ID_JOYPAD_B))
         action = RGUI_ACTION_CANCEL;
      else if (trigger_state & (1 << RETRO_DEVICE_ID_JOYPAD_A))
         action = RGUI_ACTION_OK;
      else if (trigger_state & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
         action = RGUI_ACTION_UP;
      else if (trigger_state & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
         action = RGUI_ACTION_DOWN;

      const char *ret = rgui_iterate(rgui, action);
      video_wii.frame(NULL, menu_framebuf,
            RGUI_WIDTH, RGUI_HEIGHT,
            RGUI_WIDTH * sizeof(uint16_t), NULL);

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

int main(void)
{
#ifdef HW_RVL
   L2Enhance();
#endif

   fatInitDefault();

#ifdef HAVE_FILE_LOGGER
   g_extern.verbose = true;
   log_fp = fopen("sd:/retroarch-log.txt", "w");
#endif

   config_set_defaults();

   g_settings.audio.rate_control = true;
   g_settings.audio.rate_control_delta = 0.004;
   g_console.block_config_read = true;

   wii_video_init();
   input_wii.init();

   rgui_handle_t *rgui = rgui_init("sd:/",
         menu_framebuf, RGUI_WIDTH * sizeof(uint16_t),
         _binary_console_font_bmp_start, folder_cb, NULL);

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

   if(g_console.emulator_initialized)
      rarch_main_deinit();

   wii_video_deinit();
   input_wii.free(NULL);

#ifdef HAVE_FILE_LOGGER
   fclose(log_fp);
#endif

   rgui_free(rgui);
   return ret;
}

