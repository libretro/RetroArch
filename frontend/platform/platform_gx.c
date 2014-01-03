/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
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
#include "../../libretro_private.h"

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
extern void system_exec_wii(const char *path, bool should_load_game);
#endif
#include <sdcard/gcsd.h>
#include <fat.h>

#ifdef IS_SALAMANDER
char config_path[512];
char libretro_path[512];

static void find_and_set_first_file(void)
{
   //Last fallback - we'll need to start the first executable file 
   // we can find in the RetroArch cores directory

   char first_file[512] = {0};
   find_first_libretro_core(first_file, sizeof(first_file),
   default_paths.core_dir, "dol");

   if(first_file[0])
      strlcpy(libretro_path, first_file, sizeof(libretro_path));
   else
      RARCH_ERR("Failed last fallback - RetroArch Salamander will exit.\n");
}

static void salamander_init_settings(void)
{
   char tmp_str[512] = {0};
   bool config_file_exists;

   if (path_file_exists(config_path))
      config_file_exists = true;

   //try to find CORE executable
   char core_executable[1024];
   fill_pathname_join(core_executable, default_paths.core_dir, "CORE.dol", sizeof(core_executable));

   if(path_file_exists(core_executable))
   {
      //Start CORE executable
      strlcpy(libretro_path, core_executable, sizeof(libretro_path));
      RARCH_LOG("Start [%s].\n", libretro_path);
   }
   else
   {
      if(config_file_exists)
      {
         config_file_t * conf = config_file_new(config_path);
         if (!conf) // stupid libfat bug or something; somtimes it says the file is there when it doesn't
            config_file_exists = false;
         else
         {
            config_get_array(conf, "libretro_path", tmp_str, sizeof(tmp_str));
            config_file_free(conf);
            strlcpy(libretro_path, tmp_str, sizeof(libretro_path));
         }
      }

      if(!config_file_exists || !strcmp(libretro_path, ""))
         find_and_set_first_file();
      else
      {
         RARCH_LOG("Start [%s] found in retroarch.cfg.\n", libretro_path);
      }

      if (!config_file_exists)
      {
         config_file_t *new_conf = config_file_new(NULL);
         config_set_string(new_conf, "libretro_path", libretro_path);
         config_file_write(new_conf, config_path);
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

#ifdef IS_SALAMANDER
extern char gx_rom_path[PATH_MAX];
#endif

static void get_environment_settings(int argc, char *argv[], void *args)
{
#ifndef IS_SALAMANDER
   g_extern.verbose = true;

#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   g_extern.log_file = fopen("/retroarch-log.txt", "w");
#endif
#endif

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
      fill_pathname_join(default_paths.port_dir, default_paths.port_dir, "retroarch", sizeof(default_paths.port_dir));
#ifdef IS_SALAMANDER
   fill_pathname_join(config_path, default_paths.port_dir, "retroarch.cfg", sizeof(config_path));
#else
   fill_pathname_join(g_extern.config_path, default_paths.port_dir, "retroarch.cfg", sizeof(g_extern.config_path));
#endif
   fill_pathname_join(default_paths.system_dir, default_paths.port_dir, "system", sizeof(default_paths.system_dir));
   fill_pathname_join(default_paths.sram_dir, default_paths.port_dir, "savefiles", sizeof(default_paths.sram_dir));
   fill_pathname_join(default_paths.savestate_dir, default_paths.port_dir, "savefiles", sizeof(default_paths.savestate_dir));

#ifdef IS_SALAMANDER
   if (argc > 2 && argv[1] != NULL && argv[2] != NULL)
      fill_pathname_join(gx_rom_path, argv[1], argv[2], sizeof(gx_rom_path));
   else
      gx_rom_path[0] = '\0';
#endif
}

extern void __exception_setreload(int t);

static void system_init(void *data)
{
   (void)data;
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
   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
   dotab_stdout.write_r = gx_logger_net;
#elif defined(HAVE_FILE_LOGGER) && !defined(IS_SALAMANDER)
   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
   dotab_stdout.write_r = gx_logger_file;
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

static void system_exec(const char *path, bool should_load_game);

static void system_exitspawn(void)
{
#if defined(IS_SALAMANDER)
   system_exec(libretro_path, gx_rom_path[0] != '\0' ? true : false);
#elif defined(HW_RVL)
   bool should_load_game = false;
   if (g_extern.lifecycle_state & (1ULL << MODE_EXITSPAWN_START_GAME))
      should_load_game = true;

   system_exec(g_settings.libretro, should_load_game);
   // direct loading failed (out of memory), try to jump to salamander then load the correct core
   char boot_dol[PATH_MAX];
   fill_pathname_join(boot_dol, default_paths.core_dir, "boot.dol", sizeof(boot_dol));
   system_exec(boot_dol, should_load_game);
#endif
}

static void system_deinit(void *data)
{
   (void)data;
#ifndef IS_SALAMANDER
   // we never init GX/VIDEO subsystems in salamander
   GX_DrawDone();
   GX_AbortFrame();
   GX_Flush();
   VIDEO_SetBlack(true);
   VIDEO_Flush();
   VIDEO_WaitVSync();
#endif
}

static int system_process_args(int argc, char *argv[], void *args)
{
   int ret = 0;

#ifndef IS_SALAMANDER
   // a big hack: sometimes salamander doesn't save the new core it loads on first boot,
   // so we make sure g_settings.libretro is set here
   if (!g_settings.libretro[0] && argc >= 1 && strrchr(argv[0], '/'))
   {
      char path[PATH_MAX];
      strlcpy(path, strrchr(argv[0], '/') + 1, sizeof(path));
      rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, path);
   }

   if (argc > 2 && argv[1] != NULL && argv[2] != NULL)
   {
      fill_pathname_join(g_extern.fullpath, argv[1], argv[2], sizeof(g_extern.fullpath));
      ret = 1;
   }
#endif

   return ret;
}

static void system_exec(const char *path, bool should_load_game)
{
#ifdef HW_RVL
   system_exec_wii(path, should_load_game);
#endif
}

const frontend_ctx_driver_t frontend_ctx_gx = {
   get_environment_settings,        /* get_environment_settings */
   system_init,                     /* init */
   system_deinit,                   /* deinit */
   system_exitspawn,                /* exitspawn */
   system_process_args,             /* process_args */
   NULL,                            /* process_events */
   system_exec,                     /* exec */
   NULL,                            /* shutdown */
   "gx",
};
