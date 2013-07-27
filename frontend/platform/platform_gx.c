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

   if (path_file_exists(default_paths.config_path))
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
         if (!conf) // stupid libfat bug or something; somtimes it says the file is there when it doesn't
            config_file_exists = false;
         else
         {
            config_get_array(conf, "libretro_path", tmp_str, sizeof(tmp_str));
            config_file_free(conf);
            snprintf(default_paths.libretro_path, sizeof(default_paths.libretro_path), tmp_str);
         }
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
   snprintf(default_paths.sram_dir, sizeof(default_paths.savestate_dir), "%s/savefiles", default_paths.port_dir);
   snprintf(default_paths.savestate_dir, sizeof(default_paths.savestate_dir), "%s/savestates", default_paths.port_dir);
   strlcpy(default_paths.filesystem_root_dir, "/", sizeof(default_paths.filesystem_root_dir));
   snprintf(default_paths.filebrowser_startup_dir, sizeof(default_paths.filebrowser_startup_dir), default_paths.filesystem_root_dir);
   snprintf(default_paths.input_presets_dir, sizeof(default_paths.input_presets_dir), "%s/input", default_paths.port_dir);

#ifdef IS_SALAMANDER
   if (argc > 2 && argv[1] != NULL && argv[2] != NULL)
      snprintf(gx_rom_path, sizeof(gx_rom_path),
            "%s%s", argv[1], argv[2]);
   else
      gx_rom_path[0] = '\0';
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

static void system_exitspawn(void)
{
#if defined(IS_SALAMANDER)
   rarch_console_exec(default_paths.libretro_path, gx_rom_path[0] != '\0' ? true : false);
#elif defined(HW_RVL)
   bool should_load_game = false;
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_EXITSPAWN_START_GAME))
      should_load_game = true;

   rarch_console_exec(g_settings.libretro, should_load_game);
   // direct loading failed (out of memory), try to jump to salamander then load the correct core
   char boot_dol[PATH_MAX];
   snprintf(boot_dol, sizeof(boot_dol), "%s/boot.dol", default_paths.core_dir);
   rarch_console_exec(boot_dol, should_load_game);
#endif
}

static void system_deinit(void)
{
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

static int system_process_args(int argc, char *argv[])
{
   int ret = 0;

#ifndef IS_SALAMANDER
   // a big hack: sometimes salamander doesn't save the new core it loads on first boot,
   // so we make sure g_settings.libretro is set here
   if (!g_settings.libretro[0] && argc >= 1 && strrchr(argv[0], '/'))
      strlcpy(g_settings.libretro, strrchr(argv[0], '/') + 1, sizeof(g_settings.libretro));

   if (argc > 2 && argv[1] != NULL && argv[2] != NULL)
   {
      snprintf(g_extern.fullpath, sizeof(g_extern.fullpath),
            "%s%s", argv[1], argv[2]);
      ret = 1;
   }
#endif

   return ret;
}

#include <stdio.h>

#include <string.h>
#include <fat.h>
#include <gctypes.h>
#include <ogc/cache.h>
#include <ogc/lwp_threads.h>
#include <ogc/system.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>

#define EXECUTE_ADDR ((uint8_t *) 0x91800000)
#define BOOTER_ADDR ((uint8_t *) 0x93000000)
#define ARGS_ADDR ((uint8_t *) 0x93200000)

extern uint8_t _binary_wii_app_booter_app_booter_bin_start[];
extern uint8_t _binary_wii_app_booter_app_booter_bin_end[];
#define booter_start _binary_wii_app_booter_app_booter_bin_start
#define booter_end _binary_wii_app_booter_app_booter_bin_end

#include "../../retroarch_logger.h"
#include "../../file.h"

#ifdef IS_SALAMANDER
char gx_rom_path[PATH_MAX];
#endif

#ifdef HW_RVL
static void dol_copy_argv_path(const char *dolpath, const char *argpath)
{
   char tmp[PATH_MAX];
   size_t len, t_len;
   struct __argv *argv = (struct __argv *) ARGS_ADDR;
   memset(ARGS_ADDR, 0, sizeof(struct __argv));
   char *cmdline = (char *) ARGS_ADDR + sizeof(struct __argv);
   argv->argvMagic = ARGV_MAGIC;
   argv->commandLine = cmdline;
   len = 0;

   // a device-less fullpath
   if (dolpath[0] == '/')
   {
      char *dev = strchr(__system_argv->argv[0], ':');
      t_len = dev - __system_argv->argv[0] + 1;
      memcpy(cmdline, __system_argv->argv[0], t_len);
      len += t_len;
   }
   // a relative path
   else if (strstr(dolpath, "sd:/") != dolpath && strstr(dolpath, "usb:/") != dolpath &&
       strstr(dolpath, "carda:/") != dolpath && strstr(dolpath, "cardb:/") != dolpath)
   {
      fill_pathname_parent_dir(tmp, __system_argv->argv[0], sizeof(tmp));
      t_len = strlen(tmp);
      memcpy(cmdline, tmp, t_len);
      len += t_len;
   }

   t_len = strlen(dolpath);
   memcpy(cmdline + len, dolpath, t_len);
   len += t_len;
   cmdline[len++] = 0;

   // file must be split into two parts, the path and the actual filename
   // done to be compatible with loaders
   if (argpath && strrchr(argpath, '/') != NULL)
   {
      // basedir
      fill_pathname_parent_dir(tmp, argpath, sizeof(tmp));
      t_len = strlen(tmp);
      memcpy(cmdline + len, tmp, t_len);
      len += t_len;
      cmdline[len++] = 0;

      // filename
      char *name = strrchr(argpath, '/') + 1;
      t_len = strlen(name);
      memcpy(cmdline + len, name, t_len);
      len += t_len;
      cmdline[len++] = 0;
   }

   cmdline[len++] = 0;
   argv->length = len;
   DCFlushRange(ARGS_ADDR, sizeof(struct __argv) + argv->length);
}

// WARNING: after we move any data into EXECUTE_ADDR, we can no longer use any
// heap memory and are restricted to the stack only
static void system_exec(const char *path, bool should_load_game)
{
   char game_path[PATH_MAX];

   RARCH_LOG("Attempt to load executable: [%s] %d.\n", path, sizeof(game_path));

   // copy heap info into stack so it survives us moving the .dol into MEM2
   if (should_load_game)
   {
#ifdef IS_SALAMANDER
      strlcpy(game_path, gx_rom_path, sizeof(game_path));
#else
      strlcpy(game_path, g_extern.fullpath, sizeof(game_path));
#endif
   }

   FILE * fp = fopen(path, "rb");
   if (fp == NULL)
   {
      RARCH_ERR("Could not open DOL file %s.\n", path);
      return;
   }

   fseek(fp, 0, SEEK_END);
   size_t size = ftell(fp);
   fseek(fp, 0, SEEK_SET);

   // try to allocate a buffer for it. if we can't, fail
   void *dol = malloc(size);
   if (!dol)
   {
      RARCH_ERR("Could not execute DOL file %s.\n", path);
      fclose(fp);
      return;
   }

   fread(dol, 1, size, fp);
   fclose(fp);

   fatUnmount("carda:");
   fatUnmount("cardb:");
   fatUnmount("sd:");
   fatUnmount("usb:");
   __io_wiisd.shutdown();
   __io_usbstorage.shutdown();

   // luckily for us, newlib's memmove doesn't allocate a seperate buffer for
   // copying in situations of overlap, so it's safe to do this
   memmove(EXECUTE_ADDR, dol, size);
   DCFlushRange(EXECUTE_ADDR, size);

   dol_copy_argv_path(path, should_load_game ? game_path : NULL);

   size_t booter_size = booter_end - booter_start;
   memcpy(BOOTER_ADDR, booter_start, booter_size);
   DCFlushRange(BOOTER_ADDR, booter_size);

   RARCH_LOG("jumping to %08x\n", (unsigned) BOOTER_ADDR);
   SYS_ResetSystem(SYS_SHUTDOWN,0,0);
   __lwp_thread_stopmultitasking((void (*)(void)) BOOTER_ADDR);
}
#endif

const frontend_ctx_driver_t frontend_ctx_gx = {
   get_environment_settings,
   system_init,
   system_deinit,
   system_exitspawn,
   system_process_args,
#ifdef HW_RVL
   system_exec,
#else
   NULL,
#endif
   "gx",
};
