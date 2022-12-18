/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2018 - Francisco Javier Trujillo Mata - fjtrujy
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sbv_patches.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <elf-loader.h>
#include <ps2_all_drivers.h>
#include <libpwroff.h>
#include <ps2sdkapi.h>

#if defined(SCREEN_DEBUG)
#include <debug.h>
#endif

#ifndef IS_SALAMANDER
#include "../../retroarch.h"
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#endif

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "../frontend_driver.h"
#include "../../defaults.h"
#include "../../file_path_special.h"
#include "../../paths.h"
#include "../../verbosity.h"

#if defined(DEBUG)
#define DEFAULT_PARTITION "hdd0:__common:pfs"
#endif

// Disable pthread functionality
PS2_DISABLE_AUTOSTART_PTHREAD();

static enum frontend_fork ps2_fork_mode      = FRONTEND_FORK_NONE;
static char cwd[FILENAME_MAX]                = {0};
static char mountString[10]                  = {0};
static char mountPoint[50]                   = {0};
static enum HDD_MOUNT_STATUS hddMountStatus  = HDD_MOUNT_INIT_STATUS_NOT_READY;
static enum HDD_INIT_STATUS hddStatus        = HDD_INIT_STATUS_UNKNOWN;

static void create_path_names(void)
{
   char user_path[FILENAME_MAX];

   strlcpy(user_path, cwd, sizeof(user_path));
   strlcat(user_path, "retroarch", sizeof(user_path));
   fill_pathname_basedir(g_defaults.dirs[DEFAULT_DIR_PORT], cwd, sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

   /* Content in the same folder */

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], cwd,
         "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], cwd,
         "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));

   /* user data */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], user_path,
         "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], user_path,
         "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], user_path,
         "cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], user_path,
         "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], user_path,
         "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], user_path,
         "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
         "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], user_path,
         "savefiles", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], user_path,
         "savestates", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], user_path,
         "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CACHE], user_path,
         "temp", sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], user_path,
         "overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT], user_path,
         "layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], user_path,
         "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], user_path,
         "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));

   /* history and main config */
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY],
         user_path, sizeof(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]));
   fill_pathname_join(g_defaults.path_config, user_path,
         FILE_PATH_MAIN_CONFIG, sizeof(g_defaults.path_config));

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
#endif
}

static void reset_IOP()
{
   SifInitRpc(0);
#if !defined(DEBUG) || defined(BUILD_FOR_PCSX2)
   /* Comment this line if you don't wanna debug the output */
   while(!SifIopReset(NULL, 0)){};
#endif

   while(!SifIopSync()){};
   SifInitRpc(0);
   sbv_patch_enable_lmb();
   sbv_patch_disable_prefix_check();
}

/* This method returns true if it can extract needed info from path, otherwise false.
 * In case of true, it also updates mountString, mountPoint and newCWD parameters
 * It splits path by ":", and requires a minimum of 3 elements
 * Example: if path = hdd0:__common:pfs:/retroarch/ then
 * mountString = "pfs:"
 * mountPoint = "hdd0:__common"
 * newCWD = pfs:/retroarch/
 * return true
*/
bool getMountInfo(char *path, char *mountString, char *mountPoint, char *newCWD)
{
   struct string_list *str_list = string_split(path, ":");
   if (str_list->size < 3)
      return false;

   sprintf(mountPoint, "%s:%s", str_list->elems[0].data, str_list->elems[1].data);
   sprintf(mountString, "%s:", str_list->elems[2].data);
   sprintf(newCWD, "%s%s", mountString, str_list->size == 4 ? str_list->elems[3].data : "");

   return true;
}

static void init_drivers(bool extra_drivers)
{
   init_fileXio_driver();
   init_memcard_driver(true);
   init_usb_driver();
   init_cdfs_driver();
   bool only_if_booted_from_hdd = true;
#if defined(DEBUG) && !defined(BUILD_FOR_PCSX2)
   only_if_booted_from_hdd = false;
#else
   init_poweroff_driver();
#endif
   hddStatus = init_hdd_driver(false, only_if_booted_from_hdd);

#ifndef IS_SALAMANDER
   if (extra_drivers)
   {
      init_audio_driver();
      init_joystick_driver(true);
   }
#endif
}

static void mount_partition(void)
{
   char mount_path[FILENAME_MAX];
   char new_cwd[FILENAME_MAX];
   int should_mount  = 0;
   int bootDeviceID  = getBootDeviceID(cwd);

   if (hddStatus != HDD_INIT_STATUS_IRX_OK)
      return;

   /* Try to mount HDD partition, either from cwd or default one */
   if (bootDeviceID == BOOT_DEVICE_HDD || bootDeviceID == BOOT_DEVICE_HDD0)
   {
      should_mount = 1;
      strlcpy(mount_path, cwd, sizeof(mount_path));
   }
#if !defined(IS_SALAMANDER) && defined(DEBUG)
   else
   {
      /* Even if we're booting from USB, try to mount default partition */
      strlcpy(mount_path, DEFAULT_PARTITION, sizeof(mount_path));
      should_mount = 1;
   }
#endif

   if (!should_mount)
      return;

   if (getMountInfo(mount_path, mountString, mountPoint, new_cwd) != 1)
   {
      RARCH_WARN("Partition info not readed\n");
      return;
   }

   hddMountStatus = mount_hdd_partition(mountString, mountPoint);
   if (hddMountStatus != HDD_MOUNT_STATUS_OK)
   {
      RARCH_WARN("Error mount mounting partition %s, %s\n", mountString, mountPoint);
      return;
   }

   if (bootDeviceID == BOOT_DEVICE_HDD || bootDeviceID == BOOT_DEVICE_HDD0)
   {
      /* If we're booting from HDD, we must update the cwd variable and add : to the mount point */
      strlcpy(cwd, new_cwd, sizeof(cwd));
      strlcat(mountPoint, ":", sizeof(mountPoint));
   }
   else
   {
      /* We MUST put mountPoint as empty to avoid wrong results 
	 with LoadELFFromFileWithPartition */
      strlcpy(mountPoint, "", sizeof(mountPoint));
   }
}

static void deinit_drivers(bool deinit_filesystem, bool deinit_powerOff)
{
#ifndef IS_SALAMANDER
   deinit_audio_driver();
   deinit_joystick_driver(false);
#endif

   if (deinit_filesystem)
   {
      umount_hdd_partition(mountString);

      deinit_hdd_driver(false);
      deinit_usb_driver();
      deinit_memcard_driver(true);
      deinit_fileXio_driver();

      hddMountStatus  = HDD_MOUNT_INIT_STATUS_NOT_READY;
      hddStatus        = HDD_INIT_STATUS_UNKNOWN;
   }

   if (deinit_powerOff)
      deinit_poweroff_driver();
}

static void poweroffHandler(void *arg)
{
   deinit_drivers(true, false);
   poweroffShutdown();
}

static void frontend_ps2_get_env(int *argc, char *argv[],
      void *args, void *params_data)
{
   create_path_names();

#ifndef IS_SALAMANDER
   if (!string_is_empty(argv[1]))
   {
      static char path[FILENAME_MAX] = {0};
      struct rarch_main_wrap      *args =
         (struct rarch_main_wrap*)params_data;

      if (args)
      {
         strlcpy(path, argv[1], sizeof(path));

         args->flags         &= ~(RARCH_MAIN_WRAP_FLAG_VERBOSE
                                | RARCH_MAIN_WRAP_FLAG_NO_CONTENT);
         args->flags         |=   RARCH_MAIN_WRAP_FLAG_TOUCHED;
         args->config_path    = NULL;
         args->sram_path      = NULL;
         args->state_path     = NULL;
         args->content_path   = path;
         args->libretro_path  = NULL;

         RARCH_LOG("argv[0]: %s\n", argv[0]);
         RARCH_LOG("argv[1]: %s\n", argv[1]);

         RARCH_LOG("Auto-start game %s.\n", argv[1]);
      }
   }
#endif

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
#endif
}

static void common_init_drivers(bool extra_drivers) 
{
   init_drivers(true);

   poweroffSetCallback(&poweroffHandler, NULL);

   getcwd(cwd, sizeof(cwd));
#if !defined(IS_SALAMANDER) && !defined(DEBUG)
   /* If it is not Salamander, we need to go one level 
    * up for setting the CWD. */
   path_parent_dir(cwd, strlen(cwd));
#endif
   
   mount_partition();

   waitUntilDeviceIsReady(cwd);
}

static void frontend_ps2_init(void *data)
{
   reset_IOP();
#if defined(SCREEN_DEBUG)
   init_scr();
   scr_printf("\n\nStarting RetroArch...\n");
#endif
   common_init_drivers(true);
}

static void frontend_ps2_deinit(void *data)
{
   bool deinit_filesystem = false;
#ifndef IS_SALAMANDER
   if (ps2_fork_mode == FRONTEND_FORK_NONE)
      deinit_filesystem = true;
#endif
   deinit_drivers(deinit_filesystem, true);
}

static void frontend_ps2_exec(const char *path, bool should_load_game)
{
   int args = 0;
   char *argv[1];
   RARCH_LOG("Attempt to load executable: [%s], partition [%s].\n", path, mountPoint);

   /* Reload IOP drivers for saving IOP ram */
   reset_IOP();
   common_init_drivers(false);
   waitUntilDeviceIsReady(path);

#ifndef IS_SALAMANDER
   char game_path[FILENAME_MAX];
   if (should_load_game && !path_is_empty(RARCH_PATH_CONTENT))
   {
      args++;
      const char *content = path_get(RARCH_PATH_CONTENT);
      strlcpy(game_path, content, sizeof(game_path));
      argv[0] = game_path;
      RARCH_LOG("Attempt to load executable: [%s], partition [%s] with game [%s]\n", path, mountPoint, game_path);
   }
#endif
   LoadELFFromFileWithPartition(path, mountPoint, args, argv);
}

#ifndef IS_SALAMANDER
static bool frontend_ps2_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         RARCH_LOG("FRONTEND_FORK_CORE\n");
         ps2_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         RARCH_LOG("FRONTEND_FORK_CORE_WITH_ARGS\n");
         ps2_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         RARCH_LOG("FRONTEND_FORK_RESTART\n");
         /* NOTE: We don't implement Salamander, so just turn
          * this into FRONTEND_FORK_CORE. */
         ps2_fork_mode  = FRONTEND_FORK_CORE;
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}
#endif

static void frontend_ps2_exitspawn(char *s, size_t len, char *args)
{
   bool should_load_content = false;
#ifndef IS_SALAMANDER
   if (ps2_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (ps2_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_content = true;
         break;
      case FRONTEND_FORK_NONE:
      default:
         break;
   }
#endif
   frontend_ps2_exec(s, should_load_content);
}

static int frontend_ps2_get_rating(void) { return 4; }

enum frontend_architecture frontend_ps2_get_arch(void)
{
   return FRONTEND_ARCH_MIPS;
}

static int frontend_ps2_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   char hdd[10];
   file_list_t *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content
      ? MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR
      : MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;

   menu_entries_append(list,
         rootDevicePath(BOOT_DEVICE_MC0),
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         rootDevicePath(BOOT_DEVICE_MC1),
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         rootDevicePath(BOOT_DEVICE_CDFS),
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         rootDevicePath(BOOT_DEVICE_MASS),
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);

   if (hddMountStatus == HDD_MOUNT_STATUS_OK)
   {
      size_t _len = strlcpy(hdd, mountString, sizeof(hdd));
      hdd[_len  ] = '/';
      hdd[_len+1] = '\0';
      menu_entries_append(list,
            hdd,
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);
   }
   menu_entries_append(list,
         rootDevicePath(BOOT_DEVICE_HOST),
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
#if defined(DEBUG) && !defined(BUILD_FOR_PCSX2)
   menu_entries_append(list,
         "host:",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
#endif
#endif

   return 0;
}

frontend_ctx_driver_t frontend_ctx_ps2 = {
   frontend_ps2_get_env,         /* get_env */
   frontend_ps2_init,            /* init */
   frontend_ps2_deinit,          /* deinit */
   frontend_ps2_exitspawn,       /* exitspawn */
   NULL,                         /* process_args */
   frontend_ps2_exec,            /* exec */
#ifdef IS_SALAMANDER
   NULL,                         /* set_fork */
#else
   frontend_ps2_set_fork,        /* set_fork */
#endif
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_ps2_get_rating,      /* get_rating */
   NULL,                         /* load_content */
   frontend_ps2_get_arch,        /* get_architecture */
   NULL,                         /* get_powerstate */
   frontend_ps2_parse_drive_list,/* parse_drive_list */
   NULL,                         /* get_total_mem */
   NULL,                         /* get_free_mem */
   NULL,                         /* install_signal_handler */
   NULL,                         /* get_sighandler_state */
   NULL,                         /* set_sighandler_state */
   NULL,                         /* destroy_sighandler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   NULL,                         /* get_lakka_version */
   NULL,                         /* set_screen_brightness */
   NULL,                         /* watch_path_for_changes */
   NULL,                         /* check_for_path_changes */
   NULL,                         /* set_sustained_performance_mode */
   NULL,                         /* get_cpu_model_name */
   NULL,                         /* get_user_language */
   NULL,                         /* is_narrator_running */
   NULL,                         /* accessibility_speak */
   NULL,                         /* set_gamemode */
   "ps2",                        /* ident */
   NULL                          /* get_video_driver */
};
