/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
#include "../driver.h"
#include "../general.h"
#include "../libretro.h"

#include "../console/rgui/rgui.h"
#include "../gfx/fonts/bitmap.h"

#include "../console/rarch_console_exec.h"
#include "../console/rarch_console_input.h"
#include "../console/rarch_console_settings.h"

#ifdef HW_RVL
#include "../wii/mem2_manager.h"
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#ifndef _DIRENT_HAVE_D_TYPE
#include <sys/stat.h>
#endif
#include <unistd.h>
#include <dirent.h>

#ifdef HW_RVL
#include <ogc/ios.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#endif
#include <sdcard/gcsd.h>
#include <fat.h>

enum
{
   GX_DEVICE_SD = 0,
   GX_DEVICE_USB,
   GX_DEVICE_END
};

uint16_t menu_framebuf[400 * 240];
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

#ifdef HW_RVL
static struct {
   bool mounted;
   const DISC_INTERFACE *interface;
   const char *name;
} gx_devices[GX_DEVICE_END];
static mutex_t gx_device_mutex;

static void *gx_devthread(void *a)
{
   while (1)
   {
      LWP_MutexLock(gx_device_mutex);
      unsigned i;
      for (i = 0; i < GX_DEVICE_END; i++)
      {
         if (gx_devices[i].mounted && !gx_devices[i].interface->isInserted())
         {
            gx_devices[i].mounted = false;
            char n[8];
            snprintf(n, sizeof(n), "%s:", gx_devices[i].name);
            fatUnmount(n);
         }
      }
      LWP_MutexUnlock(gx_device_mutex);
      usleep(100000);
   }

   return NULL;
}

static int gx_get_device_from_path(const char *path)
{
   if (strstr(path, "sd:") == path)
      return GX_DEVICE_SD;
   if (strstr(path, "usb:") == path)
      return GX_DEVICE_USB;
   return -1;
}
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
   fwrite(ptr, 1, len, g_extern.log_file);
   return len;
}
#endif

static const struct retro_keybind _gx_nav_binds[] = {
#ifdef HW_RVL
   { 0, 0, 0, GX_GC_UP | GX_GC_LSTICK_UP | GX_GC_RSTICK_UP | GX_CLASSIC_UP | GX_CLASSIC_LSTICK_UP | GX_CLASSIC_RSTICK_UP | GX_WIIMOTE_UP | GX_NUNCHUK_UP, 0 },
   { 0, 0, 0, GX_GC_DOWN | GX_GC_LSTICK_DOWN | GX_GC_RSTICK_DOWN | GX_CLASSIC_DOWN | GX_CLASSIC_LSTICK_DOWN | GX_CLASSIC_RSTICK_DOWN | GX_WIIMOTE_DOWN | GX_NUNCHUK_DOWN, 0 },
   { 0, 0, 0, GX_GC_LEFT | GX_GC_LSTICK_LEFT | GX_GC_RSTICK_LEFT | GX_CLASSIC_LEFT | GX_CLASSIC_LSTICK_LEFT | GX_CLASSIC_RSTICK_LEFT | GX_WIIMOTE_LEFT | GX_NUNCHUK_LEFT, 0 },
   { 0, 0, 0, GX_GC_RIGHT | GX_GC_LSTICK_RIGHT | GX_GC_RSTICK_RIGHT | GX_CLASSIC_RIGHT | GX_CLASSIC_LSTICK_RIGHT | GX_CLASSIC_RSTICK_RIGHT | GX_WIIMOTE_RIGHT | GX_NUNCHUK_RIGHT, 0 },
   { 0, 0, 0, GX_GC_A | GX_CLASSIC_A | GX_WIIMOTE_A | GX_WIIMOTE_2, 0 },
   { 0, 0, 0, GX_GC_B | GX_CLASSIC_B | GX_WIIMOTE_B | GX_WIIMOTE_1, 0 },
   { 0, 0, 0, GX_GC_START | GX_CLASSIC_PLUS | GX_WIIMOTE_PLUS, 0 },
   { 0, 0, 0, GX_GC_Z_TRIGGER | GX_CLASSIC_MINUS | GX_WIIMOTE_MINUS, 0 },
   { 0, 0, 0, GX_WIIMOTE_HOME | GX_CLASSIC_HOME, 0 },
#else
   { 0, 0, 0, GX_GC_UP | GX_GC_LSTICK_UP | GX_GC_RSTICK_UP, 0 },
   { 0, 0, 0, GX_GC_DOWN | GX_GC_LSTICK_DOWN | GX_GC_RSTICK_DOWN, 0 },
   { 0, 0, 0, GX_GC_LEFT | GX_GC_LSTICK_LEFT | GX_GC_RSTICK_LEFT, 0 },
   { 0, 0, 0, GX_GC_RIGHT | GX_GC_LSTICK_RIGHT | GX_GC_RSTICK_RIGHT, 0 },
   { 0, 0, 0, GX_GC_A, 0 },
   { 0, 0, 0, GX_GC_B, 0 },
   { 0, 0, 0, GX_GC_START, 0 },
   { 0, 0, 0, GX_GC_Z_TRIGGER, 0 },
   { 0, 0, 0, GX_WIIMOTE_HOME, 0 },
#endif
   { 0, 0, 0, GX_QUIT_KEY, 0 },
};

static const struct retro_keybind *gx_nav_binds[] = {
   _gx_nav_binds
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

#ifdef HW_RVL
   LWP_MutexLock(gx_device_mutex);
   int dev = gx_get_device_from_path(directory);

   if (dev != -1 && !gx_devices[dev].mounted && gx_devices[dev].interface->isInserted())
      fatMountSimple(gx_devices[dev].name, gx_devices[dev].interface);

   LWP_MutexUnlock(gx_device_mutex);
#endif

   char exts[256];
   if (core_chooser)
      strlcpy(exts, "dol|DOL", sizeof(exts));
   else
      strlcpy(exts, rarch_console_get_rom_ext(), sizeof(exts));
   struct string_list *ext_list = string_split(exts, "|");

   char _dir[PATH_MAX];
   snprintf(_dir, sizeof(_dir), "%s/", directory);
   DIR *dir = opendir(_dir);
   if (!dir)
      return false;

   struct dirent *entry;
   while ((entry = readdir(dir)))
   {
      char stat_path[PATH_MAX];
      const char *file_ext = path_get_extension(entry->d_name);
      snprintf(stat_path, sizeof(stat_path), "%s/%s", directory, entry->d_name);
      bool is_dir;

#ifdef _DIRENT_HAVE_D_TYPE
      is_dir = (entry->d_type == DT_DIR);
      if (entry->d_type != DT_REG && !is_dir)
         continue;
#else
      struct stat st;
      if (stat(stat_path, &st) < 0)
         continue;

      is_dir = S_ISDIR(st.st_mode);
      if (!S_ISREG(st.st_mode) && !is_dir)
         continue;
#endif

      if (core_chooser && (is_dir || strcasecmp(entry->d_name, default_paths.salamander_file) == 0))
         continue;

      if (!is_dir && ext_list && !string_list_find_elem_prefix(ext_list, ".", file_ext))
         continue;

      file_cb(ctx,
            entry->d_name,
            is_dir ? RGUI_FILE_DIRECTORY : RGUI_FILE_PLAIN, 0);
   }

   closedir(dir);
   string_list_free(ext_list);
   return true;
}

static bool rmenu_iterate(void)
{
   static uint16_t old_input_state = 0;
   static bool initial_held = true;
   static bool first_held = false;

   g_extern.draw_menu = true;
   video_gx.apply_state_changes();

   g_extern.frame_count++;

   uint16_t input_state = 0;

   input_gx.poll(NULL);

   for (unsigned i = 0; i < GX_DEVICE_NAV_LAST; i++)
   {
      input_state |= input_gx.input_state(NULL, gx_nav_binds, 0,
            RETRO_DEVICE_JOYPAD, 0, i) ? (1ULL << i) : 0;
   }

   uint16_t trigger_state = input_state & ~old_input_state;
   bool do_held = (input_state & ((1ULL << GX_DEVICE_NAV_UP) | (1ULL << GX_DEVICE_NAV_DOWN) | (1ULL << GX_DEVICE_NAV_LEFT) | (1ULL << GX_DEVICE_NAV_RIGHT))) && !(input_state & ((1ULL << GX_DEVICE_NAV_MENU) | (1ULL << GX_DEVICE_NAV_QUIT)));

   if(do_held)
   {
      if(!first_held)
      {
         first_held = true;
         SET_TIMER_EXPIRATION(1, (initial_held) ? 15 : 7);
      }

      if(IS_TIMER_EXPIRED(1))
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
   if (trigger_state & (1ULL << GX_DEVICE_NAV_UP))
      action = RGUI_ACTION_UP;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_DOWN))
      action = RGUI_ACTION_DOWN;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_LEFT))
      action = RGUI_ACTION_LEFT;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_RIGHT))
      action = RGUI_ACTION_RIGHT;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_B))
      action = RGUI_ACTION_CANCEL;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_A))
      action = RGUI_ACTION_OK;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_START))
      action = RGUI_ACTION_START;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_SELECT))
      action = RGUI_ACTION_SETTINGS;

   rgui_iterate(rgui, action);

   rarch_render_cached_frame();

   old_input_state = input_state;

   if(IS_TIMER_EXPIRED(0))
   {
      bool rmenu_enable = ((trigger_state & (1ULL << GX_DEVICE_NAV_MENU)) && g_extern.main_is_init);
      bool quit_key_pressed = (trigger_state & (1ULL << GX_DEVICE_NAV_QUIT));

      switch(g_extern.console.rmenu.mode)
      {
         case MODE_EXIT:
         case MODE_INIT:
         case MODE_EMULATION:
            break;
         default:
            g_extern.console.rmenu.mode = quit_key_pressed ? MODE_EXIT : rmenu_enable ? MODE_EMULATION : MODE_MENU;
            break;
      }
   }

   if (g_extern.console.rmenu.mode != MODE_MENU)
      goto deinit;

   return true;

deinit:
   if (!(g_extern.lifecycle_state & (1ULL << RARCH_FRAMEADVANCE)))
   {
      // set a timer delay so that we don't instantly switch back to the menu when
      // press and holding QUIT in the emulation loop (lasts for 30 frame ticks)
      SET_TIMER_EXPIRATION(0, 30);
   }
   g_extern.draw_menu = false;
   g_extern.console.rmenu.state.ingame_menu.enable = false;

   return false;
}

static void menu_init(void)
{
   rgui = rgui_init("",
         menu_framebuf, RGUI_WIDTH * sizeof(uint16_t),
         NULL /* _binary_console_font_bmp_start */, bitmap_bin, folder_cb, NULL);

   g_extern.console.rmenu.mode = MODE_MENU;
   rgui_iterate(rgui, RGUI_ACTION_REFRESH);
}

static void menu_free(void)
{
   rgui_free(rgui);
}

static void get_environment_settings(void)
{
#ifdef HW_DOL
   chdir("carda:/retroarch");
#endif
   getcwd(default_paths.core_dir, MAXPATHLEN);
   char *last_slash = strrchr(default_paths.core_dir, '/');
   if (last_slash)
      *last_slash = 0;
   char *device_end = strchr(default_paths.core_dir, '/');
   if (device_end)
      snprintf(default_paths.port_dir, sizeof(default_paths.port_dir), "%.*s/retroarch", device_end - default_paths.core_dir, default_paths.core_dir);
   else
      strlcpy(default_paths.port_dir, "/retroarch", sizeof(default_paths.port_dir));
   snprintf(g_extern.config_path, sizeof(g_extern.config_path), "%s/retroarch.cfg", default_paths.port_dir);
   snprintf(default_paths.system_dir, sizeof(default_paths.system_dir), "%s/system", default_paths.port_dir);
   snprintf(default_paths.savestate_dir, sizeof(default_paths.savestate_dir), "%s/savestates", default_paths.port_dir);
   strlcpy(default_paths.filesystem_root_dir, "/", sizeof(default_paths.filesystem_root_dir));
   snprintf(default_paths.filebrowser_startup_dir, sizeof(default_paths.filebrowser_startup_dir), default_paths.filesystem_root_dir);
   snprintf(default_paths.sram_dir, sizeof(default_paths.sram_dir), "%s/sram", default_paths.port_dir);
   snprintf(default_paths.input_presets_dir, sizeof(default_paths.input_presets_dir), "%s/input", default_paths.port_dir);
   strlcpy(default_paths.executable_extension, ".dol", sizeof(default_paths.executable_extension));
   strlcpy(default_paths.salamander_file, "boot.dol", sizeof(default_paths.salamander_file));
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

   MAKE_FILE(g_extern.config_path);
}

extern void __exception_setreload(int t);

int main(int argc, char *argv[])
{
#ifdef HW_RVL
   IOS_ReloadIOS(IOS_GetVersion());
   L2Enhance();
   gx_init_mem2();
#endif

#ifndef DEBUG
   __exception_setreload(8);
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
   g_extern.log_file = fopen("/retroarch-log.txt", "w");
   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
   dotab_stdout.write_r = gx_logger_file;
#endif

#ifdef HW_RVL
   lwp_t gx_device_thread;
   gx_devices[GX_DEVICE_SD].interface = &__io_wiisd;
   gx_devices[GX_DEVICE_SD].name = "sd";
   gx_devices[GX_DEVICE_SD].mounted = fatMountSimple(gx_devices[GX_DEVICE_SD].name, gx_devices[GX_DEVICE_SD].interface);
   gx_devices[GX_DEVICE_USB].interface = &__io_usbstorage;
   gx_devices[GX_DEVICE_USB].name = "usb";
   gx_devices[GX_DEVICE_USB].mounted = fatMountSimple(gx_devices[GX_DEVICE_USB].name, gx_devices[GX_DEVICE_USB].interface);
   LWP_MutexInit(&gx_device_mutex, false);
   LWP_CreateThread(&gx_device_thread, gx_devthread, NULL, NULL, 0, 66);
#endif

   rarch_main_clear_state();
   get_environment_settings();
   make_directories();
   config_set_defaults();
   input_gx.init();

   video_gx.start();
   driver.video = &video_gx;

   gx_video_t *gx = (gx_video_t*)driver.video_data;
   gx->menu_data = (uint32_t *) menu_framebuf;

   char tmp_path[PATH_MAX];
   const char *extension = default_paths.executable_extension;
   snprintf(tmp_path, sizeof(tmp_path), "%s/", default_paths.core_dir);
   const input_driver_t *input = &input_gx;
   const char *path_prefix = tmp_path; 

   char full_path[1024];
   snprintf(full_path, sizeof(full_path), "%sCORE%s", path_prefix, extension);

   bool find_libretro_file = rarch_configure_libretro_core(full_path, path_prefix, path_prefix, 
   g_extern.config_path, extension);

   rarch_settings_set_default();
   rarch_input_set_controls_default(input);
   rarch_config_load();

   if (find_libretro_file)
   {
      RARCH_LOG("New default libretro core saved to config file: %s.\n", g_settings.libretro);
      config_save_file(g_extern.config_path);
   }

   char core_name[64];
   rarch_console_name_from_id(core_name, sizeof(core_name));
   char input_path[1024];
   snprintf(input_path, sizeof(input_path), "%s/%s.cfg", default_paths.input_presets_dir, core_name);
   config_read_keybinds(input_path);

   init_libretro_sym();

   input_gx.post_init();

   menu_init();

   if (argc > 2 && argv[1] != NULL && argv[2] != NULL)
   {
      char rom[PATH_MAX];
      g_extern.console.external_launch.support = EXTERN_LAUNCHER_CHANNEL;
      snprintf(rom, sizeof(rom), "%s%s", argv[1], argv[2]);
      g_extern.file_state.zip_extract_mode = ZIP_EXTRACT_TO_CURRENT_DIR_AND_LOAD_FIRST_FILE;
      rarch_console_load_game_wrap(rom, g_extern.file_state.zip_extract_mode, S_DELAY_1);

      rgui_iterate(rgui, RGUI_ACTION_MESSAGE);
      g_extern.draw_menu = true;
      rarch_render_cached_frame();
      g_extern.draw_menu = false;

      g_extern.console.rmenu.mode = MODE_INIT;
   }
   else
      g_extern.console.external_launch.support = EXTERN_LAUNCHER_SALAMANDER;

begin_loop:
   if(g_extern.console.rmenu.mode == MODE_EMULATION)
   {
      input_gx.poll(NULL);

      video_set_aspect_ratio_func(g_settings.video.aspect_ratio_idx);

      audio_start_func();

      while(rarch_main_iterate());

      audio_stop_func();
   }
   else if (g_extern.console.rmenu.mode == MODE_INIT)
   {
      if(g_extern.main_is_init)
         rarch_main_deinit();

      struct rarch_main_wrap args = {0};

      args.verbose = g_extern.verbose;
      args.config_path = g_extern.config_path;
      args.sram_path = g_extern.console.main_wrap.state.default_sram_dir.enable ? g_extern.console.main_wrap.paths.default_sram_dir : NULL,
         args.state_path = g_extern.console.main_wrap.state.default_savestate_dir.enable ? g_extern.console.main_wrap.paths.default_savestate_dir : NULL,
         args.rom_path = g_extern.file_state.rom_path;
      args.libretro_path = g_settings.libretro;

      int init_ret = rarch_main_init_wrap(&args);

      if (init_ret == 0)
         RARCH_LOG("rarch_main_init succeeded.\n");
      else
         RARCH_ERR("rarch_main_init failed.\n");
   }
   else if(g_extern.console.rmenu.mode == MODE_MENU)
      rmenu_iterate();
   else
      goto begin_shutdown;
   goto begin_loop;

begin_shutdown:
   config_save_file(g_extern.config_path);
   config_save_keybinds(input_path);

   if(g_extern.main_is_init)
      rarch_main_deinit();

   input_gx.free(NULL);

   video_gx.stop();
   menu_free();

#ifdef HAVE_LOGGER
   logger_shutdown();
#elif defined(HAVE_FILE_LOGGER)
   if (g_extern.log_file)
      fclose(g_extern.log_file);
   g_extern.log_file = NULL;
#endif

   if(g_extern.console.external_launch.enable)
      rarch_console_exec(g_settings.libretro);

   exit(0);
}

