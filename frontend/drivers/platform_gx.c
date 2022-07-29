/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <unistd.h>
#include <sys/iosupport.h>

#include <gccore.h>
#include <ogcsys.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retroarch_types.h>
#include <msg_hash.h>
#include <command.h>
#include <verbosity.h>

#if defined(HW_RVL) && !defined(IS_SALAMANDER)
#include <rthreads/rthreads.h>
#include "../../memory/wii/mem2_manager.h"
#endif

#include <defines/gx_defines.h>

#include <boolean.h>

#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif
#include <string/stdstring.h>
#include <streams/file_stream.h>

#include "../frontend_driver.h"
#include "../../defaults.h"

#ifndef IS_SALAMANDER
#include "../../menu/menu_entries.h"
#endif

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

#if defined(HW_RVL) && ! defined(IS_SALAMANDER)
static enum frontend_fork gx_fork_mode = FRONTEND_FORK_NONE;
#endif

static devoptab_t dotab_stdout = {
   "stdout",   /* device name */
   0,          /* size of file structure */
   NULL,       /* device open */
   NULL,       /* device close */
   NULL,       /* device write */
   NULL,       /* device read */
   NULL,       /* device seek */
   NULL,       /* device fstat */
   NULL,       /* device stat */
   NULL,       /* device link */
   NULL,       /* device unlink */
   NULL,       /* device chdir */
   NULL,       /* device rename */
   NULL,       /* device mkdir */
   0,          /* dirStateSize */
   NULL,       /* device diropen_r */
   NULL,       /* device dirreset_r */
   NULL,       /* device dirnext_r */
   NULL,       /* device dirclose_r */
   NULL,       /* device statvfs_r */
   NULL,       /* device ftrunctate_r */
   NULL,       /* device fsync_r */
   NULL,       /* deviceData; */
};

#ifndef IS_SALAMANDER
#include "../../paths.h"

enum
{
   GX_DEVICE_SD = 0,
   GX_DEVICE_USB,
   GX_DEVICE_END
};

#ifdef HW_RVL
static struct
{
   bool mounted;
   const DISC_INTERFACE *interface;
   const char *name;
} gx_devices[GX_DEVICE_END];

static slock_t *gx_device_mutex          = NULL;
static slock_t *gx_device_cond_mutex     = NULL;
static scond_t *gx_device_cond           = NULL;
static sthread_t *gx_device_thread       = NULL;
static volatile bool gx_stop_dev_thread  = false;

static void gx_devthread(void *a)
{
   unsigned i;

   while (!gx_stop_dev_thread)
   {
      slock_lock(gx_device_mutex);

      for (i = 0; i < GX_DEVICE_END; i++)
      {
         if (gx_devices[i].mounted)
         {
            if (!gx_devices[i].interface->isInserted())
            {
               char n[8] = {0};

               gx_devices[i].mounted = false;
               snprintf(n, sizeof(n), "%s:", gx_devices[i].name);
               fatUnmount(n);
            }
         }
         else if (gx_devices[i].interface->startup() && gx_devices[i].interface->isInserted())
            gx_devices[i].mounted = fatMountSimple(gx_devices[i].name, gx_devices[i].interface);
      }

      slock_unlock(gx_device_mutex);

      slock_lock(gx_device_cond_mutex);
      scond_wait_timeout(gx_device_cond, gx_device_cond_mutex, 1000000);
      slock_unlock(gx_device_cond_mutex);
   }
}
#endif

#endif

#ifdef IS_SALAMANDER
extern char gx_rom_path[PATH_MAX_LENGTH];
#endif

static void frontend_gx_get_env(int *argc, char *argv[],
      void *args, void *params_data)
{
   char *slash;
#ifndef IS_SALAMANDER
   struct rarch_main_wrap *params = (struct rarch_main_wrap*)params_data;
#endif

#ifdef HW_DOL
   chdir("carda:/retroarch");
#endif

   getcwd(g_defaults.dirs[DEFAULT_DIR_CORE],
      sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));

#ifndef IS_SALAMANDER
#ifdef HAVE_LOGGER
   logger_init();
#endif

   /* This situation can happen on some loaders so we really need some fake
      args or else RetroArch will just crash on parsing NULL pointers. */
   if (*argc <= 0 || !argv)
   {
      if (params)
      {
         params->content_path  = NULL;
         params->sram_path     = NULL;
         params->state_path    = NULL;
         params->config_path   = NULL;
         params->libretro_path = NULL;
         params->verbose       = false;
         params->no_content    = false;
         params->touched       = true;
      }
   }
#ifdef HW_RVL
   else if (*argc > 2 &&
         !string_is_empty(argv[1]) && !string_is_empty(argv[2]))
   {
#ifdef HAVE_NETWORKING
      /* If the process was forked for netplay purposes,
         DO NOT touch the arguments. */
      if (!string_is_equal(argv[1], "-H") && !string_is_equal(argv[1], "-C"))
#endif
      {
         /* When using external loaders (Wiiflow, etc),
            getcwd doesn't return the path correctly and as a result,
            the cfg file is not found. */
         if (string_starts_with(argv[0], "usb1") ||
               string_starts_with(argv[0], "usb2"))
         {
            strcpy_literal(g_defaults.dirs[DEFAULT_DIR_CORE], "usb");
            strlcat(g_defaults.dirs[DEFAULT_DIR_CORE], argv[0] + 4,
               sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
         }

         /* Needed on Wii; loaders follow a dumb standard where the path and
            filename are separate in the argument list. */
         if (params)
         {
            static char path[PATH_MAX_LENGTH];

            fill_pathname_join(path, argv[1], argv[2], sizeof(path));

            params->content_path  = path;
            params->sram_path     = NULL;
            params->state_path    = NULL;
            params->config_path   = NULL;
            params->libretro_path = NULL;
            params->verbose       = false;
            params->no_content    = false;
            params->touched       = true;
         }
      }

      if (gx_devices[GX_DEVICE_SD].mounted)
         chdir("sd:/");
      else if (gx_devices[GX_DEVICE_USB].mounted)
         chdir("usb:/");
   }
#endif
#else
   if (*argc > 2 && argv &&
         !string_is_empty(argv[1]) && !string_is_empty(argv[2]))
      fill_pathname_join(gx_rom_path, argv[1], argv[2], sizeof(gx_rom_path));
   else
      *gx_rom_path = '\0';
#endif

   slash = strrchr(g_defaults.dirs[DEFAULT_DIR_CORE], '/');
   if (slash)
      *slash = '\0';
   strlcpy(g_defaults.dirs[DEFAULT_DIR_PORT],
      g_defaults.dirs[DEFAULT_DIR_CORE],
      sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   slash = strchr(g_defaults.dirs[DEFAULT_DIR_PORT], '/');
   if (slash)
      *slash = '\0';
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PORT],
      g_defaults.dirs[DEFAULT_DIR_PORT], "retroarch",
      sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

   /* System paths */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
      g_defaults.dirs[DEFAULT_DIR_CORE], "info",
      sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG],
      g_defaults.dirs[DEFAULT_DIR_CORE], "autoconfig",
      sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY],
      g_defaults.dirs[DEFAULT_DIR_CORE], "overlays",
      sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT],
      g_defaults.dirs[DEFAULT_DIR_CORE], "layouts",
      sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
      g_defaults.dirs[DEFAULT_DIR_CORE], "filters/video",
      sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
      g_defaults.dirs[DEFAULT_DIR_CORE], "filters/audio",
      sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS],
      g_defaults.dirs[DEFAULT_DIR_CORE], "assets",
      sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS],
      g_defaults.dirs[DEFAULT_DIR_CORE], "cheats",
      sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   /* User paths */
   fill_pathname_join(g_defaults.path_config,
      g_defaults.dirs[DEFAULT_DIR_CORE], "retroarch.cfg",
      sizeof(g_defaults.path_config));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM],
      g_defaults.dirs[DEFAULT_DIR_PORT], "system",
      sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM],
      g_defaults.dirs[DEFAULT_DIR_PORT], "savefiles",
      sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE],
      g_defaults.dirs[DEFAULT_DIR_PORT], "savestates",
      sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST],
      g_defaults.dirs[DEFAULT_DIR_PORT], "playlists",
      sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS],
      g_defaults.dirs[DEFAULT_DIR_PORT], "logs",
      sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP],
      g_defaults.dirs[DEFAULT_DIR_PORT], "remaps",
      sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
      g_defaults.dirs[DEFAULT_DIR_PORT], "config",
      sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
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

   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;

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

static void frontend_gx_exec(const char *path, bool should_load_game)
{
#ifdef HW_RVL
   system_exec_wii(path, should_load_game);
#endif
}

static void frontend_gx_exitspawn(char *s, size_t len, char *args)
{
   bool should_load_game = false;
#if defined(IS_SALAMANDER)
   if (!string_is_empty(gx_rom_path))
      should_load_game = true;
#elif defined(HW_RVL)
   char salamander_basename[PATH_MAX_LENGTH];

   if (gx_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (gx_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_game = true;
         break;
      case FRONTEND_FORK_CORE:
         /* fall-through */
      case FRONTEND_FORK_RESTART:
         {
            char new_path[PATH_MAX_LENGTH];
            char salamander_name[PATH_MAX_LENGTH];

            if (frontend_driver_get_salamander_basename(salamander_name,
                     sizeof(salamander_name)))
            {
               fill_pathname_join(new_path, g_defaults.dirs[DEFAULT_DIR_CORE],
                     salamander_name, sizeof(new_path));
               path_set(RARCH_PATH_CONTENT, new_path);
            }
         }
         break;
      case FRONTEND_FORK_NONE:
      default:
         break;
   }

   frontend_gx_exec(s, should_load_game);
   frontend_driver_get_salamander_basename(salamander_basename,
         sizeof(salamander_basename));

   /* FIXME/TODO - hack
    * direct loading failed (out of memory),
    * try to jump to Salamander,
    * then load the correct core */
   fill_pathname_join(s, g_defaults.dirs[DEFAULT_DIR_CORE],
         salamander_basename, len);
#endif
   frontend_gx_exec(s, should_load_game);
}

static void frontend_gx_process_args(int *argc, char *argv[])
{
#ifndef IS_SALAMANDER
   /* A big hack: sometimes Salamander doesn't save the new core
    * it loads on first boot, so we make sure
    * active core path is set here. */
   if (path_is_empty(RARCH_PATH_CORE) && *argc >= 1 && strrchr(argv[0], '/'))
   {
      char path[PATH_MAX_LENGTH] = {0};
      strlcpy(path, strrchr(argv[0], '/') + 1, sizeof(path));
      if (path_is_valid(path))
         path_set(RARCH_PATH_CORE, path);
   }
#endif
}

#if defined(HW_RVL) && !defined(IS_SALAMANDER)
static bool frontend_gx_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         gx_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         gx_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         gx_fork_mode  = fork_mode;
         command_event(CMD_EVENT_QUIT, NULL);
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}
#endif

static int frontend_gx_get_rating(void)
{
#ifdef HW_RVL
   return 8;
#else
   return 6;
#endif
}

static enum frontend_architecture frontend_gx_get_arch(void)
{
   return FRONTEND_ARCH_PPC;
}

static int frontend_gx_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content ?
      MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
      MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
#ifdef HW_RVL
   menu_entries_append_enum(list,
         "sd:/",
         msg_hash_to_str(MSG_EXTERNAL_APPLICATION_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "usb:/",
         msg_hash_to_str(MSG_EXTERNAL_APPLICATION_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#endif
   menu_entries_append_enum(list,
         "carda:/",
         msg_hash_to_str(MSG_EXTERNAL_APPLICATION_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "cardb:/",
         msg_hash_to_str(MSG_EXTERNAL_APPLICATION_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#endif

   return 0;
}

static void frontend_gx_shutdown(bool unused)
{
#ifndef IS_SALAMANDER
   exit(0);
#endif
}

static uint64_t frontend_gx_get_total_mem(void)
{
   uint64_t total = SYSMEM1_SIZE;
#if defined(HW_RVL) && !defined(IS_SALAMANDER)
   total += gx_mem2_total();
#endif
   return total;
}

static uint64_t frontend_gx_get_free_mem(void)
{
   uint64_t total = SYSMEM1_SIZE - (SYSMEM1_SIZE - SYS_GetArena1Size());
#if defined(HW_RVL) && !defined(IS_SALAMANDER)
   total += (gx_mem2_total() - gx_mem2_used());
#endif
   return total;
}

frontend_ctx_driver_t frontend_ctx_gx = {
   frontend_gx_get_env,             /* get_env */
   frontend_gx_init,
   frontend_gx_deinit,
   frontend_gx_exitspawn,
   frontend_gx_process_args,
   frontend_gx_exec,
#if defined(HW_RVL) && !defined(IS_SALAMANDER)
   frontend_gx_set_fork,            /* set_fork */
#else
   NULL,                            /* set_fork */
#endif
   frontend_gx_shutdown,            /* shutdown */
   NULL,                            /* get_name */
   NULL,                            /* get_os */
   frontend_gx_get_rating,          /* get_rating */
   NULL,                            /* load_content */
   frontend_gx_get_arch,            /* get_architecture */
   NULL,                            /* get_powerstate */
   frontend_gx_parse_drive_list,    /* parse_drive_list */
   frontend_gx_get_total_mem,       /* get_total_mem */
   frontend_gx_get_free_mem,        /* get_free_mem */
   NULL,                            /* install_signal_handler */
   NULL,                            /* get_sighandler_state */
   NULL,                            /* set_sighandler_state */
   NULL,                            /* destroy_signal_handler_state */
   NULL,                            /* attach_console */
   NULL,                            /* detach_console */
   NULL,                            /* get_lakka_version */
   NULL,                            /* set_screen_brightness */
   NULL,                            /* watch_path_for_changes */
   NULL,                            /* check_for_path_changes */
   NULL,                            /* set_sustained_performance_mode */
   NULL,                            /* get_cpu_model_name  */
   NULL,                            /* get_user_language   */
   NULL,                            /* is_narrator_running */
   NULL,                            /* accessibility_speak */
   NULL,                            /* set_gamemode        */
   "gx",                            /* ident               */
   NULL                             /* get_video_driver    */
};
