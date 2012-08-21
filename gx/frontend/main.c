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
rgui_handle_t *rgui;

#if defined(HAVE_LOGGER) || defined(HAVE_FILE_LOGGER)
static devoptab_t dotab_stdout = {
   "stdout",   // device name
   0,          // size of file structure
   NULL,       // device open
   NULL,       // device close
   NULL,       // device write
   NULL,       // device read
   NULL,       // device seek
   NULL,       // device fstat
   NULL,       // device stat
   NULL,       // device link
   NULL,       // device unlink
   NULL,       // device chdir
   NULL,       // device rename
   NULL,       // device mkdir
   0,          // dirStateSize
   NULL,       // device diropen_r
   NULL,       // device dirreset_r
   NULL,       // device dirnext_r
   NULL,       // device dirclose_r
   NULL,       // device statvfs_r
   NULL,       // device ftrunctate_r
   NULL,       // device fsync_r
   NULL,       // deviceData;
};
#endif

#ifdef HAVE_LOGGER
int gx_logger_net(struct _reent *r, int fd, const char *ptr, size_t len)
{
   static char temp[4000];
   size_t l = len >= 4000 ? 3999 : len;
   memcpy(temp, ptr, l);
   temp[l] = 0;
   logger_send("%s", temp);
   return len;
}
#elif defined(HAVE_FILE_LOGGER)
int gx_logger_file(struct _reent *r, int fd, const char *ptr, size_t len)
{
   fwrite(ptr, 1, len, log_fp);
   return len;
}
#endif

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
   { 0, 0, 0, GX_QUIT_KEY, 0 },
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
   GX_DEVICE_NAV_MENU,
   GX_DEVICE_NAV_QUIT,
   GX_DEVICE_NAV_LAST
};

extern uint8_t _binary_console_font_bmp_start[];

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

static void menu_loop(void)
{
   gx_video_t *gx = (gx_video_t*)driver.video_data;

   uint16_t old_input_state = 0;
   bool first = true;
   bool first_held = false;
   bool initial_held = true;

   g_console.menu_enable = true;
   gx->menu_render = true;

   do
   {
      uint16_t input_state = 0;

      input_gx.poll(NULL);

      for (unsigned i = 0; i < GX_DEVICE_NAV_LAST; i++)
      {
         input_state |= input_gx.input_state(NULL, wii_nav_binds, 0,
               RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
      }

      uint16_t trigger_state = input_state & ~old_input_state;
      bool do_held = (input_state & ((1 << GX_DEVICE_NAV_UP) | (1 << GX_DEVICE_NAV_DOWN) | (1 << GX_DEVICE_NAV_LEFT) | (1 << GX_DEVICE_NAV_RIGHT))) && !(input_state & ((1 << GX_DEVICE_NAV_MENU) | (1 << GX_DEVICE_NAV_QUIT)));

      if(do_held)
      {
         if(!first_held)
         {
            first_held = true;
            SET_TIMER_EXPIRATION(gx, (initial_held) ? 15 : 7);
         }

         if(IS_TIMER_EXPIRED(gx))
         {
            first_held = false;
            trigger_state = input_state; //second input frame set as current frame
         }

         initial_held = false;
      }
      else
      {
         first_held = false;
         initial_held = true;
      }

      rgui_action_t action = RGUI_ACTION_NOOP;

      // don't run anything first frame, only capture held inputs for old_input_state
      if (!first)
      {
         if (trigger_state & (1 << GX_DEVICE_NAV_UP))
            action = RGUI_ACTION_UP;
         else if (trigger_state & (1 << GX_DEVICE_NAV_DOWN))
            action = RGUI_ACTION_DOWN;
         else if (trigger_state & (1 << GX_DEVICE_NAV_LEFT))
            action = RGUI_ACTION_LEFT;
         else if (trigger_state & (1 << GX_DEVICE_NAV_RIGHT))
            action = RGUI_ACTION_RIGHT;
         else if (trigger_state & (1 << GX_DEVICE_NAV_B))
            action = RGUI_ACTION_CANCEL;
         else if (trigger_state & (1 << GX_DEVICE_NAV_A))
            action = RGUI_ACTION_OK;
         else if (trigger_state & (1 << GX_DEVICE_NAV_START))
            action = RGUI_ACTION_START;
         else if (trigger_state & (1 << GX_DEVICE_NAV_SELECT))
            action = RGUI_ACTION_SETTINGS;
      }
      else
      {
         first = false;
      }

      rgui_iterate(rgui, action);

      rarch_render_cached_frame();

      old_input_state = input_state;

      bool goto_menu_key_pressed = (trigger_state & (1 << GX_DEVICE_NAV_MENU));
      bool quit_key_pressed = (trigger_state & (1 << GX_DEVICE_NAV_QUIT));

      if(IS_TIMER_EXPIRED(gx))
      {
         // if we want to force goto the emulation loop, skip this
         if(g_console.mode_switch != MODE_EMULATION)
         {
            if(goto_menu_key_pressed)
            {
               g_console.menu_enable = (goto_menu_key_pressed && g_console.emulator_initialized) ? false : true;
               g_console.mode_switch = g_console.menu_enable ? MODE_MENU : MODE_EMULATION;
            }
         }
      }

      if(quit_key_pressed)
      {
         g_console.menu_enable = false;
         g_console.mode_switch = MODE_EXIT;
      }

      // set a timer delay so that we don't instantly switch back to the menu when
      // press and holding QUIT in the emulation loop (lasts for 30 frame ticks)
      if(g_console.mode_switch == MODE_EMULATION)
      {
         SET_TIMER_EXPIRATION(gx, 30);
      }

   }while(g_console.menu_enable);

   gx->menu_render = false;

   g_console.ingame_menu_enable = false;
}

static void menu_init(void)
{
   rgui = rgui_init("",
         menu_framebuf, RGUI_WIDTH * sizeof(uint32_t),
         _binary_console_font_bmp_start, folder_cb, NULL);
   rgui_iterate(rgui, RGUI_ACTION_REFRESH);

   g_console.mode_switch = MODE_MENU;
}

static void menu_free(void)
{
   rgui_free(rgui);
}

int rarch_main(int argc, char **argv);

static void get_environment_settings(void)
{
   snprintf(default_paths.port_dir, sizeof(default_paths.port_dir), "/retroarch");
   getcwd(default_paths.core_dir, MAXPATHLEN);
   snprintf(default_paths.config_file, sizeof(default_paths.config_file), "%s/retroarch.cfg", default_paths.port_dir);
   snprintf(default_paths.system_dir, sizeof(default_paths.system_dir), "%s/system", default_paths.port_dir);
   snprintf(default_paths.savestate_dir, sizeof(default_paths.savestate_dir), "%s/savestates", default_paths.port_dir);
   snprintf(default_paths.filesystem_root_dir, sizeof(default_paths.filesystem_root_dir), "/");
   snprintf(default_paths.filebrowser_startup_dir, sizeof(default_paths.filebrowser_startup_dir), default_paths.filesystem_root_dir);
   snprintf(default_paths.sram_dir, sizeof(default_paths.sram_dir), "%s/sram", default_paths.port_dir);
   snprintf(default_paths.input_presets_dir, sizeof(default_paths.input_presets_dir), "%s/input", default_paths.port_dir);
   strlcpy(default_paths.executable_extension, ".dol", sizeof(default_paths.executable_extension));
   //RARCH_LOG("port_dir: %s\n", default_paths.port_dir);
   RARCH_LOG("core_dir: %s\n", default_paths.core_dir);
}

#define MAKE_FILE(x) {\
   if (!path_file_exists((x)))\
   {\
      RARCH_WARN("File \"%s\" does not exists, creating\n", (x));\
      FILE *f = fopen((x), "wb");\
      if (!f)\
      {\
         RARCH_ERR("Could not create file \"%s\"\n", (x));\
      }\
      fclose(f);\
   }\
}

#define MAKE_DIR(x) {\
   if (!path_is_directory((x)))\
   {\
      RARCH_WARN("Directory \"%s\" does not exists, creating\n", (x));\
      if (mkdir((x), 0777) != 0)\
      {\
         RARCH_ERR("Could not create directory \"%s\"\n", (x));\
      }\
   }\
}

static void make_directories(void)
{
   MAKE_DIR(default_paths.port_dir);
   MAKE_DIR(default_paths.system_dir);
   MAKE_DIR(default_paths.savestate_dir);
   MAKE_DIR(default_paths.sram_dir);
   MAKE_DIR(default_paths.input_presets_dir);

   MAKE_FILE(default_paths.config_file);
}

int main(void)
{
#ifdef HW_RVL
   L2Enhance();
#endif

   fatInitDefault();

#ifdef HAVE_LOGGER
   g_extern.verbose = true;
   logger_init();
   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
   dotab_stdout.write_r = gx_logger_net;
#elif defined(HAVE_FILE_LOGGER)
   g_extern.verbose = true;
   log_fp = fopen("/retroarch-log.txt", "w");
   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
   dotab_stdout.write_r = gx_logger_file;
#endif

   get_environment_settings();
   make_directories();
   config_set_defaults();
   input_gx.init();

   video_gx.start();
   driver.video = &video_gx;

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

   rarch_settings_set_default(&input_gx);
   rarch_config_load(default_paths.config_file, path_prefix, extension, find_libretro_file);

   char core_name[64];
   rarch_console_name_from_id(core_name, sizeof(core_name));
   char input_path[1024];
   snprintf(input_path, sizeof(input_path), "%s/%s.cfg", default_paths.input_presets_dir, core_name);
   config_read_keybinds(input_path);

   init_libretro_sym();

   input_gx.post_init();

   menu_init();

begin_loop:
   if(g_console.mode_switch == MODE_EMULATION)
   {
      bool repeat = false;

      input_gx.poll(NULL);

      video_set_aspect_ratio_func(g_console.aspect_ratio_index);

      audio_start_func();

      do{
         repeat = rarch_main_iterate();
      }while(repeat && !g_console.frame_advance_enable);

      audio_stop_func();
   }
   else if(g_console.mode_switch == MODE_MENU)
   {
      menu_loop();

      if (g_console.mode_switch != MODE_EXIT)
         rarch_startup(default_paths.config_file);
   }
   else
      goto begin_shutdown;
   goto begin_loop;

begin_shutdown:
   if(path_file_exists(default_paths.config_file))
      rarch_config_save(default_paths.config_file);

   if(g_console.emulator_initialized)
      rarch_main_deinit();

   input_gx.free(NULL);

   video_gx.stop();
   menu_free();

#ifdef HAVE_LOGGER
   logger_shutdown();
#elif defined(HAVE_FILE_LOGGER)
   fclose(log_fp);
#endif

   if(g_console.return_to_launcher)
      rarch_console_exec(g_console.launch_app_on_exit);
   config_save_keybinds(input_path);

   return 1;
}

