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

#include "../../boolean.h"
#include "../../driver.h"
#include "../../general.h"
#include "../../libretro_private.h"
#include "../../gfx/gx/sdk_defines.h"

#include "../../file.h"

#if defined(HW_RVL) && !defined(IS_SALAMANDER)
#include "../../wii/mem2_manager.h"
#include <ogc/mutex.h>
#include <ogc/cond.h>
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

static bool exit_spawn = false;
static bool exitspawn_start_game = false;

#ifndef IS_SALAMANDER

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
static mutex_t gx_device_cond_mutex;
static cond_t gx_device_cond;

static void *gx_devthread(void *a)
{
   struct timespec timeout = {0};

   timeout.tv_sec = 1;
   timeout.tv_nsec = 0;

   while (1)
   {
      LWP_MutexLock(gx_device_mutex);
      unsigned i;
      for (i = 0; i < GX_DEVICE_END; i++)
      {
         if (gx_devices[i].mounted && 
               !gx_devices[i].interface->isInserted())
         {
            gx_devices[i].mounted = false;
            char n[8];
            snprintf(n, sizeof(n), "%s:", gx_devices[i].name);
            fatUnmount(n);
         }
      }
      LWP_MutexUnlock(gx_device_mutex);
      LWP_MutexLock(gx_device_cond_mutex);
      LWP_CondTimedWait(gx_device_cond, gx_device_cond_mutex, &timeout);
      LWP_MutexUnlock(gx_device_cond_mutex);
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

static void frontend_gx_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   g_extern.log_file = fopen("/retroarch-log.txt", "w");
#endif
#endif

#ifdef HW_DOL
   chdir("carda:/retroarch");
#endif
   getcwd(g_defaults.core_dir, MAXPATHLEN);
   char *last_slash = strrchr(g_defaults.core_dir, '/');
   if (last_slash)
      *last_slash = 0;
   char *device_end = strchr(g_defaults.core_dir, '/');
   if (device_end)
      snprintf(g_defaults.port_dir, sizeof(g_defaults.port_dir),
            "%.*s/retroarch", device_end - g_defaults.core_dir,
            g_defaults.core_dir);
   else
      fill_pathname_join(g_defaults.port_dir, g_defaults.port_dir,
            "retroarch", sizeof(g_defaults.port_dir));
   fill_pathname_join(g_defaults.overlay_dir, g_defaults.core_dir,
         "overlays", sizeof(g_defaults.overlay_dir));
   fill_pathname_join(g_defaults.config_path, g_defaults.port_dir,
         "retroarch.cfg", sizeof(g_defaults.config_path));
   fill_pathname_join(g_defaults.system_dir, g_defaults.port_dir,
         "system", sizeof(g_defaults.system_dir));
   fill_pathname_join(g_defaults.sram_dir, g_defaults.port_dir,
         "savefiles", sizeof(g_defaults.sram_dir));
   fill_pathname_join(g_defaults.savestate_dir, g_defaults.port_dir,
         "savefiles", sizeof(g_defaults.savestate_dir));
   fill_pathname_join(g_defaults.playlist_dir, g_defaults.port_dir,
         "playlists", sizeof(g_defaults.playlist_dir));

#ifdef IS_SALAMANDER
   if (*argc > 2 && argv[1] != NULL && argv[2] != NULL)
      fill_pathname_join(gx_rom_path, argv[1], argv[2], sizeof(gx_rom_path));
   else
      gx_rom_path[0] = '\0';
#else
#ifdef HW_RVL
   /* needed on Wii; loaders follow a dumb standard where the path and 
    * filename are separate in the argument list */
   if (*argc > 2 && argv[1] != NULL && argv[2] != NULL)
   {
      static char path[PATH_MAX];
      *path = '\0';
      struct rarch_main_wrap *args = (struct rarch_main_wrap*)params_data;

      if (args)
      {
         fill_pathname_join(path, argv[1], argv[2], sizeof(path));

         args->touched        = true;
         args->no_content     = false;
         args->verbose        = false;
         args->config_path    = NULL;
         args->sram_path      = NULL;
         args->state_path     = NULL;
         args->content_path   = path;
         args->libretro_path  = NULL;
      }
   }
#endif
#endif
}

extern void __exception_setreload(int t);

static void frontend_gx_init(void *data)
{
   (void)data;
#ifdef HW_RVL
   IOS_ReloadIOS(IOS_GetVersion());
   L2Enhance();
#ifndef IS_SALAMANDER
   gx_init_mem2();
#endif
#endif

#if defined(DEBUG) && defined(IS_SALAMANDER)
   VIDEO_Init();
   GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
   void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
   console_init(xfb, 20, 20, rmode->fbWidth,
         rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
   VIDEO_Configure(rmode);
   VIDEO_SetNextFramebuffer(xfb);
   VIDEO_SetBlack(FALSE);
   VIDEO_Flush();
   VIDEO_WaitVSync();
   VIDEO_WaitVSync();
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
   OSThread gx_device_thread;
   gx_devices[GX_DEVICE_SD].interface = &__io_wiisd;
   gx_devices[GX_DEVICE_SD].name = "sd";
   gx_devices[GX_DEVICE_SD].mounted = fatMountSimple(
         gx_devices[GX_DEVICE_SD].name,
         gx_devices[GX_DEVICE_SD].interface);
   gx_devices[GX_DEVICE_USB].interface = &__io_usbstorage;
   gx_devices[GX_DEVICE_USB].name = "usb";
   gx_devices[GX_DEVICE_USB].mounted = fatMountSimple(
            gx_devices[GX_DEVICE_USB].name,
            gx_devices[GX_DEVICE_USB].interface);

   OSInitMutex(&gx_device_cond_mutex);
   OSInitCond(&gx_device_cond);
   OSInitMutex(&gx_device_mutex);
   OSCreateThread(&gx_device_thread, gx_devthread, 0, NULL, NULL, 0, 66, 0);
#endif
}

static void frontend_gx_exec(const char *path, bool should_load_game);

static void frontend_gx_exitspawn(char *core_path, size_t sizeof_core_path)
{
   bool should_load_game = false;
#if defined(IS_SALAMANDER)
   if (gx_rom_path[0] != '\0')
      should_load_game = true;
#elif defined(HW_RVL)
   should_load_game = exitspawn_start_game;

   if (!exit_spawn)
      return;

   frontend_gx_exec(core_path, should_load_game);

   /* FIXME/TODO - hack
    * direct loading failed (out of memory), try to jump to Salamander,
    * then load the correct core */
   fill_pathname_join(core_path, g_defaults.core_dir,
         "boot.dol", sizeof_core_path);
#endif
   frontend_gx_exec(core_path, should_load_game);
}

static void frontend_gx_process_args(int *argc, char *argv[])
{
#ifndef IS_SALAMANDER
   /* A big hack: sometimes Salamander doesn't save the new core 
    * it loads on first boot, so we make sure
    * g_settings.libretro is set here. */
   if (!g_settings.libretro[0] && *argc >= 1 && strrchr(argv[0], '/'))
   {
      char path[PATH_MAX];
      strlcpy(path, strrchr(argv[0], '/') + 1, sizeof(path));
      rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, path);
   }
#endif
}

static void frontend_gx_exec(const char *path, bool should_load_game)
{
#ifdef HW_RVL
   system_exec_wii(path, should_load_game);
#endif
}

static void frontend_gx_set_fork(bool exitspawn, bool start_game)
{
   exit_spawn = exitspawn;
   exitspawn_start_game = start_game;
}

static int frontend_gx_get_rating(void)
{
#ifdef HW_RVL
   return 8;
#else
   return 6;
#endif
}

const frontend_ctx_driver_t frontend_ctx_gx = {
   frontend_gx_get_environment_settings, /* get_environment_settings */
   frontend_gx_init,                /* init */
   NULL,                            /* deinit */
   frontend_gx_exitspawn,           /* exitspawn */
   frontend_gx_process_args,        /* process_args */
   NULL,                            /* process_events */
   frontend_gx_exec,                /* exec */
   frontend_gx_set_fork,            /* set_fork */
   NULL,                            /* shutdown */
   NULL,                            /* get_name */
   frontend_gx_get_rating,          /* get_rating */
   "gx",
};
