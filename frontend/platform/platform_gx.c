/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012-2013 - Michael Lelli
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

#include <stdbool.h>
#include "../../driver.h"
#include "../../general.h"
#include "../../libretro.h"

#include "platform_inl.h"

#include "../../console/rarch_console.h"
#include "../../file.h"

#if defined(HW_RVL) && !defined(IS_SALAMANDER)
#include "../../wii/mem2_manager.h"
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

#ifdef IS_SALAMANDER

static void find_and_set_first_file(void)
{
   //Last fallback - we'll need to start the first executable file 
   // we can find in the RetroArch cores directory

   char first_file[512] = {0};
   find_first_libretro_core(first_file, sizeof(first_file),
   default_paths.core_dir, "dol");

   if(first_file[0])
      strlcpy(default_paths.libretro_path, first_file, sizeof(default_paths.libretro_path));
   else
      RARCH_ERR("Failed last fallback - RetroArch Salamander will exit.\n");
}

static void salamander_init_settings(void)
{
   char tmp_str[512] = {0};
   bool config_file_exists;

   if(!path_file_exists(default_paths.config_path))
   {
      FILE * f;
      config_file_exists = false;
      RARCH_ERR("Config file \"%s\" doesn't exist. Creating...\n", default_paths.config_path);
      MAKE_DIR(default_paths.port_dir);
      f = fopen(default_paths.config_path, "w");
      fclose(f);
   }
   else
      config_file_exists = true;

   //try to find CORE executable
   char core_executable[1024];
   snprintf(core_executable, sizeof(core_executable), "%s/CORE.dol", default_paths.core_dir);

   if(path_file_exists(core_executable))
   {
      //Start CORE executable
      snprintf(default_paths.libretro_path, sizeof(default_paths.libretro_path), core_executable);
      RARCH_LOG("Start [%s].\n", default_paths.libretro_path);
   }
   else
   {
      if(config_file_exists)
      {
         config_file_t * conf = config_file_new(default_paths.config_path);
         config_get_array(conf, "libretro_path", tmp_str, sizeof(tmp_str));
         config_file_free(conf);
         snprintf(default_paths.libretro_path, sizeof(default_paths.libretro_path), tmp_str);
      }

      if(!config_file_exists || !strcmp(default_paths.libretro_path, ""))
         find_and_set_first_file();
      else
      {
         RARCH_LOG("Start [%s] found in retroarch.cfg.\n", default_paths.libretro_path);
      }

      if (!config_file_exists)
      {
         config_file_t *new_conf = config_file_new(NULL);
         config_set_string(new_conf, "libretro_path", default_paths.libretro_path);
         config_file_write(new_conf, default_paths.config_path);
         config_file_free(new_conf);
      }
   }
}

#else

enum
{
   GX_DEVICE_SD = 0,
   GX_DEVICE_USB,
   GX_DEVICE_END
};

rgui_handle_t *rgui;
char input_path[1024];

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

#endif

static void get_environment_settings(int argc, char *argv[])
{
   (void)argc;
   (void)argv;

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
#ifdef IS_SALAMANDER
   snprintf(default_paths.config_path, sizeof(default_paths.config_path), "%s/retroarch.cfg", default_paths.port_dir);
#else
   snprintf(g_extern.config_path, sizeof(g_extern.config_path), "%s/retroarch.cfg", default_paths.port_dir);
#endif
   snprintf(default_paths.system_dir, sizeof(default_paths.system_dir), "%s/system", default_paths.port_dir);
   snprintf(default_paths.savestate_dir, sizeof(default_paths.savestate_dir), "%s/savestates", default_paths.port_dir);
   strlcpy(default_paths.filesystem_root_dir, "/", sizeof(default_paths.filesystem_root_dir));
   snprintf(default_paths.filebrowser_startup_dir, sizeof(default_paths.filebrowser_startup_dir), default_paths.filesystem_root_dir);
   snprintf(default_paths.sram_dir, sizeof(default_paths.sram_dir), "%s/sram", default_paths.port_dir);
   snprintf(default_paths.input_presets_dir, sizeof(default_paths.input_presets_dir), "%s/input", default_paths.port_dir);

#ifndef IS_SALAMANDER
   MAKE_DIR(default_paths.port_dir);
   MAKE_DIR(default_paths.system_dir);
   MAKE_DIR(default_paths.savestate_dir);
   MAKE_DIR(default_paths.sram_dir);
   MAKE_DIR(default_paths.input_presets_dir);

   MAKE_FILE(g_extern.config_path);
#endif
}

extern void __exception_setreload(int t);

static void system_init(void)
{
#ifdef HW_RVL
   IOS_ReloadIOS(IOS_GetVersion());
   L2Enhance();
#ifndef IS_SALAMANDER
   gx_init_mem2();
#endif
#endif

#ifndef DEBUG
   __exception_setreload(8);
#endif

   fatInitDefault();

#ifdef HAVE_LOGGER
   inl_logger_init();
   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
   dotab_stdout.write_r = gx_logger_net;
#elif defined(HAVE_FILE_LOGGER)
   inl_logger_init();
#ifndef IS_SALAMANDER
   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
   dotab_stdout.write_r = gx_logger_file;
#endif
#endif

#if defined(HW_RVL) && !defined(IS_SALAMANDER)
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
}

static void system_exitspawn(void)
{
#if defined(IS_SALAMANDER)
   rarch_console_exec(default_paths.libretro_path);
#elif defined(HW_RVL)
   // try to launch the core directly first, then fallback to salamander
   rarch_console_exec(g_settings.libretro);
   rarch_console_exec(g_extern.fullpath);
#endif
}

static void system_deinit(void)
{
#if defined(HAVE_LOGGER) || defined(HAVE_FILE_LOGGER)
   inl_logger_deinit();
#endif
}

#ifndef IS_SALAMANDER

static void system_post_init(void)
{
   char core_name[64];

   get_libretro_core_name(core_name, sizeof(core_name));
   snprintf(input_path, sizeof(input_path), "%s/%s.cfg", default_paths.input_presets_dir, core_name);
   config_read_keybinds(input_path);
}

static void system_deinit_save(void)
{
   config_save_keybinds(input_path);
}

static void system_process_args(int argc, char *argv[])
{
   if (argc > 2 && argv[1] != NULL && argv[2] != NULL)
   {
      char rom[PATH_MAX];
      g_extern.lifecycle_mode_state |= (1ULL << MODE_EXTLAUNCH_CHANNEL);
      snprintf(rom, sizeof(rom), "%s%s", argv[1], argv[2]);

      strlcpy(g_extern.fullpath, rom, sizeof(g_extern.fullpath));
      g_extern.lifecycle_mode_state |= (1ULL << MODE_LOAD_GAME);

      rgui_iterate(rgui, RGUI_ACTION_MESSAGE);
      g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_DRAW);
      rarch_render_cached_frame();
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_DRAW);
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU);
      g_extern.lifecycle_mode_state |= (1ULL << MODE_INIT);
   }
   else
      g_extern.lifecycle_mode_state |= (1ULL << MODE_EXTLAUNCH_SALAMANDER);
}

#endif
