/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdint.h>
#include <stddef.h>

#include <xtl.h>
#include <xbdm.h>
#include <xgraphics.h>

#include <file/file_path.h>
#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "../frontend_driver.h"
#include "../../defaults.h"
#include "../../file_path_special.h"
#include "../../paths.h"
#ifndef IS_SALAMANDER
#include "../../retroarch.h"
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#endif
#include "../../verbosity.h"

#include "platform_xdk.h"

static enum frontend_fork xdk_fork_mode = FRONTEND_FORK_NONE;

static void frontend_xdk_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   HRESULT ret;
#ifdef _XBOX360
   unsigned long license_mask;
   DWORD volume_device_type;
#endif
#ifndef IS_SALAMANDER
   static char path[PATH_MAX_LENGTH] = {0};
#if defined(_XBOX1)
   LAUNCH_DATA ptr;
   DWORD launch_type;
#elif defined(_XBOX360)
   DWORD dwLaunchDataSize;
#endif
#endif
#ifndef IS_SALAMANDER
   bool original_verbose       = verbosity_is_enabled();
#endif

   (void)ret;

#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   retro_main_log_file_init("/retroarch-log.txt");
#endif
#endif

#ifdef _XBOX360
   /* Detect install environment. */
   if (XContentGetLicenseMask(&license_mask, NULL) == ERROR_SUCCESS)
   {
      XContentQueryVolumeDeviceType("GAME",&volume_device_type, NULL);

      switch(volume_device_type)
      {
         case XCONTENTDEVICETYPE_HDD: /* Launched from content package on HDD */
         case XCONTENTDEVICETYPE_MU:  /* Launched from content package on USB/Memory Unit. */
         case XCONTENTDEVICETYPE_ODD: /* Launched from content package on Optial Disc Drive. */
         default:                     /* Launched from content package on unknown device. */
            break;
      }
   }
#endif

#if defined(_XBOX1)
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CORE],
         "D:", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.path.config, g_defaults.dirs[DEFAULT_DIR_CORE],
         file_path_str(FILE_PATH_MAIN_CONFIG), sizeof(g_defaults.path.config));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         "savestates",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         "savefiles",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         "system",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         "screenshots",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         "overlays",
         sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         "layouts",
         sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         "media", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
#elif defined(_XBOX360)
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CORE],
         "game:",
         sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   strlcpy(g_defaults.path.config,
         "game:\\retroarch.cfg", sizeof(g_defaults.path.config));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT],
         "game:",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_SAVESTATE],
         "game:\\savestates",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_PLAYLIST],
         "game:\\playlists",
         sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_SRAM],
         "game:\\savefiles",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_SYSTEM],
         "game:\\system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_LOGS],
         "game:\\logs",
         sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));

#ifndef IS_SALAMANDER
#if defined(_XBOX1)
   if (XGetLaunchInfo(&launch_type, &ptr) == ERROR_SUCCESS)
   {
      char *extracted_path = NULL;
      if (launch_type == LDT_FROM_DEBUGGER_CMDLINE)
         goto exit;

      extracted_path = (char*)&ptr.Data;

      if (
            !string_is_empty(extracted_path)
            && (strstr(extracted_path, "Pool") == NULL)
            /* Hack. Unknown problem */)
      {
         /* Auto-start game */
         strlcpy(path, extracted_path, sizeof(path));
      }
   }
#elif defined(_XBOX360)
   if (XGetLaunchDataSize(&dwLaunchDataSize) == ERROR_SUCCESS)
   {
      char *extracted_path                 = (char*)calloc(dwLaunchDataSize, sizeof(char));
      BYTE* pLaunchData                    = (BYTE*)calloc(dwLaunchDataSize, sizeof(BYTE));

      XGetLaunchData(pLaunchData, dwLaunchDataSize);
      memset(extracted_path, 0, dwLaunchDataSize);

      strlcpy(extracted_path, pLaunchData, dwLaunchDataSize);

      /* Auto-start game */
      if (!string_is_empty(extracted_path))
         strlcpy(path, extracted_path, sizeof(path));

      if (pLaunchData)
         free(pLaunchData);
   }
#endif
   if (!string_is_empty(path))
   {
      struct rarch_main_wrap *args = (struct rarch_main_wrap*)params_data;

      if (args)
      {
         /* Auto-start game. */
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

#ifndef IS_SALAMANDER
#ifdef _XBOX1
exit:
   if (original_verbose)
      verbosity_enable();
   else
      verbosity_disable();
#endif
#endif
}

static void frontend_xdk_init(void *data)
{
   (void)data;
#if defined(_XBOX1) && !defined(IS_SALAMANDER)
   /* Mount drives */
   xbox_io_mount("A:", "cdrom0");
   xbox_io_mount("C:", "Harddisk0\\Partition0");
   xbox_io_mount("E:", "Harddisk0\\Partition1");
   xbox_io_mount("Z:", "Harddisk0\\Partition2");
   xbox_io_mount("F:", "Harddisk0\\Partition6");
   xbox_io_mount("G:", "Harddisk0\\Partition7");
#endif
}

static void frontend_xdk_exec(const char *path, bool should_load_game)
{
#ifndef IS_SALAMANDER
   bool original_verbose       = verbosity_is_enabled();
#endif
#if defined(_XBOX1)
   LAUNCH_DATA ptr;
#elif defined(_XBOX360)
   char game_path[1024]        = {0};
#endif
   (void)should_load_game;

#ifdef IS_SALAMANDER
   if (!string_is_empty(path))
      XLaunchNewImage(path, NULL);
#else
#if defined(_XBOX1)
   memset(&ptr, 0, sizeof(ptr));

   if (should_load_game && !path_is_empty(RARCH_PATH_CONTENT))
      snprintf((char*)ptr.Data, sizeof(ptr.Data), "%s", path_get(RARCH_PATH_CONTENT));

   if (!string_is_empty(path))
      XLaunchNewImage(path, !string_is_empty((const char*)ptr.Data) ? &ptr : NULL);
#elif defined(_XBOX360)
   if (should_load_game && !path_is_empty(RARCH_PATH_CONTENT))
   {
      strlcpy(game_path, path_get(RARCH_PATH_CONTENT), sizeof(game_path));
      XSetLaunchData(game_path, MAX_LAUNCH_DATA_SIZE);
   }

   if (!string_is_empty(path))
      XLaunchNewImage(path, 0);
#endif
#endif
#ifndef IS_SALAMANDER
   if (original_verbose)
      verbosity_enable();
   else
      verbosity_disable();
#endif
}

#ifndef IS_SALAMANDER
static bool frontend_xdk_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         RARCH_LOG("FRONTEND_FORK_CORE\n");
         xdk_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         RARCH_LOG("FRONTEND_FORK_CORE_WITH_ARGS\n");
         xdk_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         RARCH_LOG("FRONTEND_FORK_SALAMANDER_RESTART\n");
         /* NOTE: We don't implement Salamander, so just turn
          * this into FRONTEND_FORK_CORE. */
         xdk_fork_mode  = FRONTEND_FORK_CORE;
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}
#endif

static void frontend_xdk_exitspawn(char *s, size_t len)
{
   bool should_load_game = false;
#ifndef IS_SALAMANDER
   if (xdk_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (xdk_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_game = true;
         break;
      case FRONTEND_FORK_NONE:
      default:
         break;
   }
#endif
   frontend_xdk_exec(s, should_load_game);
}

static int frontend_xdk_get_rating(void)
{
#if defined(_XBOX360)
   return 11;
#elif defined(_XBOX1)
   return 7;
#endif
}

enum frontend_architecture frontend_xdk_get_architecture(void)
{
#if defined(_XBOX360)
   return FRONTEND_ARCH_PPC;
#elif defined(_XBOX1)
   return FRONTEND_ARCH_X86;
#else
   return FRONTEND_ARCH_NONE;
#endif
}

static int frontend_xdk_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content ?
      MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
      MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;

#if defined(_XBOX1)
   menu_entries_append_enum(list,
         "C:",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "D:",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "E:",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "F:",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "G:",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#elif defined(_XBOX360)
   menu_entries_append_enum(list,
         "game:",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#endif
#endif

   return 0;
}

frontend_ctx_driver_t frontend_ctx_xdk = {
   frontend_xdk_get_environment_settings,
   frontend_xdk_init,
   NULL,                         /* deinit */
   frontend_xdk_exitspawn,
   NULL,                         /* process_args */
   frontend_xdk_exec,
#ifdef IS_SALAMANDER
   NULL,
#else
   frontend_xdk_set_fork,
#endif
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_xdk_get_rating,
   NULL,                         /* load_content */
   frontend_xdk_get_architecture,
   NULL,                         /* get_powerstate */
   frontend_xdk_parse_drive_list,
   NULL,                         /* get_mem_total */
   NULL,                         /* get_mem_free */
   NULL,                         /* install_signal_handler */
   NULL,                         /* get_sighandler_state */
   NULL,                         /* set_sighandler_state */
   NULL,                         /* destroy_sighandler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   NULL,                         /* watch_path_for_changes */
   NULL,                         /* check_for_path_changes */
   NULL,                         /* set_sustained_performance_mode */
   NULL,                         /* get_cpu_model_name */
   NULL,                         /* get_user_language */
   "xdk",
};
