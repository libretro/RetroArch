/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(HW_RVL) && !defined(IS_SALAMANDER)
#include <ogc/mutex.h>
#include <ogc/cond.h>
#include "../../memory/wii/mem2_manager.h"
#endif

#include <boolean.h>

#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <file/file_list.h>
#endif

#include "../../driver.h"
#include "../../general.h"
#include "../../libretro_private.h"
#include "../../defines/gx_defines.h"

#ifdef HW_RVL
#include <ogc/ios.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
extern void system_exec_wii(const char *path, bool should_load_game);
#endif
#include <sdcard/gcsd.h>
#include <fat.h>
#include <rthreads/rthreads.h>

#ifdef USBGECKO
#include <debug.h>
#endif

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

static slock_t *gx_device_mutex;
static slock_t *gx_device_cond_mutex;
static scond_t *gx_device_cond;
static sthread_t *gx_device_thread;
static volatile bool gx_stop_dev_thread;

static void gx_devthread(void *a)
{
   while (!gx_stop_dev_thread)
   {
      unsigned i;

      slock_lock(gx_device_mutex);

      for (i = 0; i < GX_DEVICE_END; i++) {
         if (gx_devices[i].mounted) {
            if (!gx_devices[i].interface->isInserted()) {
               gx_devices[i].mounted = false;
               char n[8];
               snprintf(n, sizeof(n), "%s:", gx_devices[i].name);
               fatUnmount(n);
            }
         } else if (gx_devices[i].interface->startup() && gx_devices[i].interface->isInserted()) {
            gx_devices[i].mounted = fatMountSimple(gx_devices[i].name, gx_devices[i].interface);
         }
      }

      slock_unlock(gx_device_mutex);

      slock_lock(gx_device_cond_mutex);
      scond_wait_timeout(gx_device_cond, gx_device_cond_mutex, 1000000);
      slock_unlock(gx_device_cond_mutex);
   }
}
#endif

#ifdef HAVE_LOGGER
int gx_logger_net(struct _reent *r, int fd, const char *ptr, size_t len)
{
   static char temp[4000];
   size_t l = len >= 4000 ? 3999 : len - 1;
   memcpy(temp, ptr, l);
   temp[l] = 0;
   logger_send("%s", temp);
   return len;
}
#elif defined(HAVE_FILE_LOGGER)
int gx_logger_file(struct _reent *r, int fd, const char *ptr, size_t len)
{
   fwrite(ptr, 1, len, retro_main_log_file());
   return len;
}
#endif

#endif

#ifdef IS_SALAMANDER
extern char gx_rom_path[PATH_MAX_LENGTH];
#endif

static void frontend_gx_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   retro_main_log_file_init("/retroarch-log.txt");
#endif
#endif

#ifdef HW_DOL
   chdir("carda:/retroarch");
#endif
   getcwd(g_defaults.dir.core, MAXPATHLEN);
   char *last_slash = strrchr(g_defaults.dir.core, '/');
   if (last_slash)
      *last_slash = 0;
   char *device_end = strchr(g_defaults.dir.core, '/');
   if (device_end)
      snprintf(g_defaults.dir.port, sizeof(g_defaults.dir.port),
            "%.*s/retroarch", device_end - g_defaults.dir.core,
            g_defaults.dir.core);
   else
      fill_pathname_join(g_defaults.dir.port, g_defaults.dir.port,
            "retroarch", sizeof(g_defaults.dir.port));
   fill_pathname_join(g_defaults.dir.overlay, g_defaults.dir.core,
         "overlays", sizeof(g_defaults.dir.overlay));
   fill_pathname_join(g_defaults.path.config, g_defaults.dir.port,
         "retroarch.cfg", sizeof(g_defaults.path.config));
   fill_pathname_join(g_defaults.dir.system, g_defaults.dir.port,
         "system", sizeof(g_defaults.dir.system));
   fill_pathname_join(g_defaults.dir.sram, g_defaults.dir.port,
         "savefiles", sizeof(g_defaults.dir.sram));
   fill_pathname_join(g_defaults.dir.savestate, g_defaults.dir.port,
         "savefiles", sizeof(g_defaults.dir.savestate));
   fill_pathname_join(g_defaults.dir.playlist, g_defaults.dir.port,
         "playlists", sizeof(g_defaults.dir.playlist));

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
      static char path[PATH_MAX_LENGTH];
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

#ifdef USBGECKO
   DEBUG_Init(GDBSTUB_DEVICE_USB, 1);
   _break();
#endif

#if defined(DEBUG) && defined(IS_SALAMANDER)
   VIInit();
   GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
   void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
   console_init(xfb, 20, 20, rmode->fbWidth,
         rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
   VIConfigure(rmode);
   VISetNextFramebuffer(xfb);
   VISetBlack(FALSE);
   VIFlush();
   VIWaitForRetrace();
   VIWaitForRetrace();
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

   gx_device_cond_mutex = slock_new();
   gx_device_cond       = scond_new();
   gx_device_mutex      = slock_new();
   gx_device_thread     = sthread_create(gx_devthread, NULL);
#endif
}

static void frontend_gx_deinit(void *data)
{
   (void)data;

#if defined(HW_RVL) && !defined(IS_SALAMANDER)
   slock_lock(gx_device_cond_mutex);
   gx_stop_dev_thread = true;
   slock_unlock(gx_device_cond_mutex);
   scond_signal(gx_device_cond);
   sthread_join(gx_device_thread);
#endif
}

static void frontend_gx_exec(const char *path, bool should_load_game);

static void frontend_gx_exitspawn(char *s, size_t len)
{
   bool should_load_game = false;
#if defined(IS_SALAMANDER)
   if (gx_rom_path[0] != '\0')
      should_load_game = true;
#elif defined(HW_RVL)
   should_load_game = exitspawn_start_game;

   if (!exit_spawn)
      return;

   frontend_gx_exec(s, should_load_game);

   /* FIXME/TODO - hack
    * direct loading failed (out of memory), try to jump to Salamander,
    * then load the correct core */
   fill_pathname_join(s, g_defaults.dir.core,
         "boot.dol", len);
#endif
   frontend_gx_exec(s, should_load_game);
}

static void frontend_gx_process_args(int *argc, char *argv[])
{
#ifndef IS_SALAMANDER
   settings_t *settings = config_get_ptr();

   /* A big hack: sometimes Salamander doesn't save the new core
    * it loads on first boot, so we make sure
    * settings->libretro is set here. */
   if (!settings->libretro[0] && *argc >= 1 && strrchr(argv[0], '/'))
   {
      char path[PATH_MAX_LENGTH];
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

static enum frontend_architecture frontend_gx_get_architecture(void)
{
   return FRONTEND_ARCH_PPC;
}

static int frontend_gx_parse_drive_list(void *data)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t*)data;
#ifdef HW_RVL
   menu_entries_push(list,
         "sd:/", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "usb:/", "", MENU_FILE_DIRECTORY, 0, 0);
#endif
   menu_entries_push(list,
         "carda:/", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "cardb:/", "", MENU_FILE_DIRECTORY, 0, 0);
#endif

   return 0;
}

frontend_ctx_driver_t frontend_ctx_gx = {
   frontend_gx_get_environment_settings,
   frontend_gx_init,
   frontend_gx_deinit,
   frontend_gx_exitspawn,
   frontend_gx_process_args,
   frontend_gx_exec,
   frontend_gx_set_fork,
   NULL,                            /* shutdown */
   NULL,                            /* get_name */
   NULL,                            /* get_os */
   frontend_gx_get_rating,
   NULL,                            /* load_content */
   frontend_gx_get_architecture,
   NULL,                            /* get_powerstate */
   frontend_gx_parse_drive_list,
   "gx",
};
