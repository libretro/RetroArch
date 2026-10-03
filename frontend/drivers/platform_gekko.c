/*  RetroArch - A frontend for libretro.
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

/* The GameCube and Wii on os/gekko: storage mounted as C library
 * devices, the directory layout under the one RetroArch started from,
 * and leaving through the loader.
 *
 * Wii: the front SD slot is "sd:", the first USB drive "usb:".
 * GameCube: an SD adapter in serial port 2 is "sd:", in the memory
 * card slots "carda:" and "cardb:". */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gekko/disk.h>
#ifdef HW_RVL
#include <gekko/usb.h>
#endif

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif
#include <string/stdstring.h>

#include "../frontend_driver.h"

#include "../../defaults.h"
#include "../../msg_hash.h"
#include "../../retroarch_types.h"
#include "../../verbosity.h"

#if !defined(IS_SALAMANDER)
#include "../../paths.h"
#include "../../menu/menu_entries.h"
#endif

struct volume
{
   const char *name;
   uint8_t     mounted;
};

#ifdef HW_RVL
enum { VOL_SD = 0, VOL_USB, VOL_COUNT };
static struct volume volumes[VOL_COUNT] = { { "sd", 0 }, { "usb", 0 } };
#else
enum { VOL_SD = 0, VOL_CARDA, VOL_CARDB, VOL_COUNT };
static struct volume volumes[VOL_COUNT] = {
   { "sd", 0 }, { "carda", 0 }, { "cardb", 0 } };
/* The EXI channel each GameCube volume sits on. */
static const unsigned volume_channel[VOL_COUNT] = { 2, 0, 1 };
#endif

static void mount(unsigned i, gk_blockdev_t *dev)
{
   int ret;
   if (!dev)
      return;
   if ((ret = gk_fat_mount(volumes[i].name, dev)))
      RARCH_WARN("[Gekko] %s: no FAT volume (%d).\n", volumes[i].name, ret);
   else
      volumes[i].mounted = 1;
}

static void frontend_gekko_init(void *data)
{
   (void)data;
#ifdef HW_RVL
   mount(VOL_SD,  gk_sd_open());
   mount(VOL_USB, gk_usbstorage_open());
#else
   {
      unsigned i;
      for (i = 0; i < VOL_COUNT; i++)
         mount(i, gk_sdgecko_open(volume_channel[i]));
   }
#endif
}

static void frontend_gekko_deinit(void *data)
{
   unsigned i;
   (void)data;
   for (i = 0; i < VOL_COUNT; i++)
      if (volumes[i].mounted)
      {
         gk_fat_unmount(volumes[i].name);
         volumes[i].mounted = 0;
      }
}

/* Where RetroArch lives: the directory of the program the loader
 * started, else retroarch on the first volume mounted. */
static void base_dir(int argc, char *argv[], char *s, size_t len)
{
   unsigned i;
   if (argc >= 1 && argv && argv[0] && strstr(argv[0], ":/"))
   {
      char *slash;
      strlcpy(s, argv[0], len);
      if ((slash = strrchr(s, '/')))
         *slash = '\0';
      return;
   }
   for (i = 0; i < VOL_COUNT; i++)
      if (volumes[i].mounted)
      {
         snprintf(s, len, "%s:/retroarch", volumes[i].name);
         return;
      }
   strlcpy(s, "sd:/retroarch", len);
}

static void frontend_gekko_get_env(int *argc, char *argv[],
      void *args, void *params_data)
{
   char *slash;
#ifndef IS_SALAMANDER
   struct rarch_main_wrap *params = (struct rarch_main_wrap*)params_data;
#endif
   (void)args;

   base_dir(*argc, argv, g_defaults.dirs[DEFAULT_DIR_CORE],
         sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   chdir(g_defaults.dirs[DEFAULT_DIR_CORE]);

#ifndef IS_SALAMANDER
   /* Some loaders pass nothing at all; RetroArch's argument parsing
    * wants something to look at. */
   if (*argc <= 0 || !argv)
   {
      if (params)
      {
         params->content_path  = NULL;
         params->sram_path     = NULL;
         params->state_path    = NULL;
         params->config_path   = NULL;
         params->libretro_path = NULL;
         params->flags        &= ~(RARCH_MAIN_WRAP_FLAG_VERBOSE
                                 | RARCH_MAIN_WRAP_FLAG_NO_CONTENT);
         params->flags        |=   RARCH_MAIN_WRAP_FLAG_TOUCHED;
      }
   }
   else if (*argc > 2 && argv[1] && *argv[1] && argv[2] && *argv[2]
         && params)
   {
      /* Loaders that start content pass its directory and file name
       * as two arguments. */
      static char path[PATH_MAX_LENGTH];
      fill_pathname_join(path, argv[1], argv[2], sizeof(path));
      params->content_path  = path;
      params->sram_path     = NULL;
      params->state_path    = NULL;
      params->config_path   = NULL;
      params->libretro_path = NULL;
      params->flags        &= ~(RARCH_MAIN_WRAP_FLAG_VERBOSE
                              | RARCH_MAIN_WRAP_FLAG_NO_CONTENT);
      params->flags        |=   RARCH_MAIN_WRAP_FLAG_TOUCHED;
   }
#endif

   /* The port directory: retroarch at the root of the volume the
    * program is on. */
   strlcpy(g_defaults.dirs[DEFAULT_DIR_PORT],
         g_defaults.dirs[DEFAULT_DIR_CORE],
         sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   if ((slash = strchr(g_defaults.dirs[DEFAULT_DIR_PORT], '/')))
      *slash = '\0';
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PORT],
         g_defaults.dirs[DEFAULT_DIR_PORT], "retroarch",
         sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
         g_defaults.dirs[DEFAULT_DIR_CORE], "info",
         sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG],
         g_defaults.dirs[DEFAULT_DIR_CORE], "autoconfig",
         sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY],
         g_defaults.dirs[DEFAULT_DIR_CORE], "overlays",
         sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY],
         g_defaults.dirs[DEFAULT_DIR_CORE], "overlays/keyboards",
         sizeof(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY]));
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
   /* REMAP lives under MENU_CONFIG, so that comes first. */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
         g_defaults.dirs[DEFAULT_DIR_PORT], "config",
         sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP],
         g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], "remaps",
         sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
#endif
}

static void frontend_gekko_process_args(int *argc, char *argv[])
{
#ifndef IS_SALAMANDER
   /* The core is the one linked into the program the loader ran. */
   if (*argc >= 1 && argv && argv[0])
   {
      const char *last_slash = strrchr(argv[0], '/');
      if (last_slash && path_is_valid(last_slash + 1))
         path_set(RARCH_PATH_CORE, last_slash + 1);
   }
#endif
}

static enum frontend_architecture frontend_gekko_get_arch(void)
{
   return FRONTEND_ARCH_PPC;
}

static int frontend_gekko_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list            = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content
      ? MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR
      : MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
   unsigned i;
   for (i = 0; i < VOL_COUNT; i++)
   {
      char root[16];
      if (!volumes[i].mounted)
         continue;
      snprintf(root, sizeof(root), "%s:/", volumes[i].name);
      menu_entries_append(list, root,
            msg_hash_to_str(MSG_EXTERNAL_APPLICATION_DIR),
            enum_idx, FILE_TYPE_DIRECTORY, 0, 0, NULL);
   }
#endif
   return 0;
}

static void frontend_gekko_shutdown(bool unused)
{
   (void)unused;
#ifndef IS_SALAMANDER
   /* exit() goes back to the loader, or the system menu. */
   exit(0);
#endif
}

frontend_ctx_driver_t frontend_ctx_gx = {
   frontend_gekko_get_env,          /* get_env */
   frontend_gekko_init,             /* init */
   frontend_gekko_deinit,           /* deinit */
   NULL,                            /* exitspawn */
   frontend_gekko_process_args,     /* process_args */
   NULL,                            /* exec */
   NULL,                            /* set_fork */
   frontend_gekko_shutdown,         /* shutdown */
   NULL,                            /* get_name */
   NULL,                            /* get_os */
   NULL,                            /* load_content */
   frontend_gekko_get_arch,         /* get_architecture */
   NULL,                            /* get_powerstate */
   frontend_gekko_parse_drive_list, /* parse_drive_list */
   NULL,                            /* install_signal_handler */
   NULL,                            /* get_sighandler_state */
   NULL,                            /* set_sighandler_state */
   NULL,                            /* destroy_signal_handler_state */
   NULL,                            /* attach_console */
   NULL,                            /* detach_console */
   NULL,                            /* get_lakka_version */
   NULL,                            /* set_screen_brightness */
   NULL,                            /* set_sustained_performance_mode */
   NULL,                            /* get_cpu_model_name */
   NULL,                            /* get_user_language */
   NULL,                            /* is_narrator_running */
   NULL,                            /* accessibility_speak */
   NULL,                            /* set_gamemode */
   NULL,                            /* get_display_type */
   "gx",                            /* ident */
   NULL                             /* get_video_driver */
};
