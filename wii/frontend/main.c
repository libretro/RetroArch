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
#include "../../console/retroarch_console_input.h"

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

static const uint64_t wii_nav_buttons[9] = {
   WII_GC_UP | WII_GC_LSTICK_UP | WII_GC_RSTICK_UP | WII_CLASSIC_UP | WII_CLASSIC_LSTICK_UP | WII_CLASSIC_RSTICK_UP | WII_WIIMOTE_UP | WII_NUNCHUK_UP,
   WII_GC_DOWN | WII_GC_LSTICK_DOWN | WII_GC_RSTICK_DOWN | WII_CLASSIC_DOWN | WII_CLASSIC_LSTICK_DOWN | WII_CLASSIC_RSTICK_DOWN | WII_WIIMOTE_DOWN | WII_NUNCHUK_DOWN,
   WII_GC_LEFT | WII_GC_LSTICK_LEFT | WII_GC_RSTICK_LEFT | WII_CLASSIC_LEFT | WII_CLASSIC_LSTICK_LEFT | WII_CLASSIC_RSTICK_LEFT | WII_WIIMOTE_LEFT | WII_NUNCHUK_LEFT,
   WII_GC_RIGHT | WII_GC_LSTICK_RIGHT | WII_GC_RSTICK_RIGHT | WII_CLASSIC_RIGHT | WII_CLASSIC_LSTICK_RIGHT | WII_CLASSIC_RSTICK_RIGHT | WII_WIIMOTE_RIGHT | WII_NUNCHUK_RIGHT,
   WII_GC_A | WII_CLASSIC_A | WII_WIIMOTE_A | WII_WIIMOTE_2,
   WII_GC_B | WII_CLASSIC_B | WII_WIIMOTE_B | WII_WIIMOTE_1,
   WII_GC_START | WII_CLASSIC_PLUS | WII_WIIMOTE_PLUS,
   WII_GC_Z_TRIGGER | WII_CLASSIC_MINUS | WII_WIIMOTE_MINUS,
   WII_WIIMOTE_HOME | WII_CLASSIC_HOME,
};

enum
{
   WII_DEVICE_NAV_UP = 0,
   WII_DEVICE_NAV_DOWN,
   WII_DEVICE_NAV_LEFT,
   WII_DEVICE_NAV_RIGHT,
   WII_DEVICE_NAV_A,
   WII_DEVICE_NAV_B,
   WII_DEVICE_NAV_START,
   WII_DEVICE_NAV_SELECT,
   WII_DEVICE_NAV_EXIT,
};

static bool folder_cb(const char *directory, rgui_file_enum_cb_t file_cb,
      void *userdata, void *ctx)
{
   (void)userdata;

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
      uint64_t input_poll = wii_input_update(0);

      for (unsigned i = 0; i < sizeof(wii_nav_buttons) / sizeof(wii_nav_buttons[0]); i++)
      {
         input_state |= input_poll & wii_nav_buttons[i] ? (1 << i) : 0;
      }

      uint16_t trigger_state = input_state & ~old_input_state;

      if (!first)
      {
         if (trigger_state & (1 << WII_DEVICE_NAV_EXIT))
         {
            if (can_quit)
               return false;
         }
         else
            can_quit = true;
      }

      rgui_action_t action = RGUI_ACTION_NOOP;
      if (trigger_state & (1 << WII_DEVICE_NAV_B))
         action = RGUI_ACTION_CANCEL;
      else if (trigger_state & (1 << WII_DEVICE_NAV_A))
         action = RGUI_ACTION_OK;
      else if (trigger_state & (1 << WII_DEVICE_NAV_UP))
         action = RGUI_ACTION_UP;
      else if (trigger_state & (1 << WII_DEVICE_NAV_DOWN))
         action = RGUI_ACTION_DOWN;
      else if (trigger_state & (1 << WII_DEVICE_NAV_LEFT))
         action = RGUI_ACTION_LEFT;
      else if (trigger_state & (1 << WII_DEVICE_NAV_RIGHT))
         action = RGUI_ACTION_RIGHT;
      else if (trigger_state & (1 << WII_DEVICE_NAV_START))
         action = RGUI_ACTION_START;
      else if (trigger_state & (1 << WII_DEVICE_NAV_SELECT) && !first) // don't catch start+select+l+r when exiting
         action = RGUI_ACTION_SETTINGS;

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
      first = false;
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
   log_fp = fopen("/retroarch-log.txt", "w");
#endif

   config_set_defaults();

   g_settings.audio.rate_control = true;
   g_settings.audio.rate_control_delta = 0.004;
   g_console.block_config_read = true;

   wii_video_init();
   input_wii.init();
   rarch_input_set_controls_default(&input_wii);

   rgui_handle_t *rgui = rgui_init("",
         menu_framebuf, RGUI_WIDTH * sizeof(uint16_t),
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

