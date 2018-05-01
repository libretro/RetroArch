/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2016 - Ali Bouhlel
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <wiiu/types.h>
#include <file/file_path.h>

#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#include <string/stdstring.h>

#include "file_path_special.h"

#include "../frontend_driver.h"
#include "../../defaults.h"
#include "../../paths.h"
#include "../../verbosity.h"

#include "hbl.h"
#include "wiiu_dbg.h"
#include "tasks/tasks_internal.h"

#ifndef IS_SALAMANDER
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#endif

#define WIIU_SD_PATH "sd:/"
#define WIIU_USB_PATH "usb:/"

/**
 * The Wii U frontend driver.
 *
 * If you're looking for main() and friends, they've been moved to
 * wiiu/main.c
 */

static enum frontend_fork wiiu_fork_mode = FRONTEND_FORK_NONE;
static const char *elf_path_cst = WIIU_SD_PATH "retroarch/retroarch.elf";

static void frontend_wiiu_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   unsigned i;
   (void)args;

   fill_pathname_basedir(g_defaults.dirs[DEFAULT_DIR_PORT], elf_path_cst, sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], g_defaults.dirs[DEFAULT_DIR_PORT],
         "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], g_defaults.dirs[DEFAULT_DIR_PORT],
         "media", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], g_defaults.dirs[DEFAULT_DIR_PORT],
         "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], g_defaults.dirs[DEFAULT_DIR_CORE],
         "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], g_defaults.dirs[DEFAULT_DIR_CORE],
         "savestates", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], g_defaults.dirs[DEFAULT_DIR_CORE],
         "savefiles", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], g_defaults.dirs[DEFAULT_DIR_CORE],
         "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], g_defaults.dirs[DEFAULT_DIR_CORE],
         "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], g_defaults.dirs[DEFAULT_DIR_PORT],
         "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], g_defaults.dirs[DEFAULT_DIR_PORT],
         "config/remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], g_defaults.dirs[DEFAULT_DIR_PORT],
         "filters", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], g_defaults.dirs[DEFAULT_DIR_PORT],
         "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], g_defaults.dirs[DEFAULT_DIR_PORT],
         "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
   fill_pathname_join(g_defaults.path.config, g_defaults.dirs[DEFAULT_DIR_PORT],
         file_path_str(FILE_PATH_MAIN_CONFIG), sizeof(g_defaults.path.config));

   for (i = 0; i < DEFAULT_DIR_LAST; i++)
   {
      const char *dir_path = g_defaults.dirs[i];
      if (!string_is_empty(dir_path))
         path_mkdir(dir_path);
   }
}

static void frontend_wiiu_deinit(void *data)
{
   (void)data;
}

static void frontend_wiiu_shutdown(bool unused)
{
   (void)unused;
}

static void frontend_wiiu_init(void *data)
{
   (void)data;
   DEBUG_LINE();
   verbosity_enable();
   DEBUG_LINE();
}


static int frontend_wiiu_get_rating(void)
{
   return 10;
}

enum frontend_architecture frontend_wiiu_get_architecture(void)
{
   return FRONTEND_ARCH_PPC;
}

static int frontend_wiiu_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t *)data;
   enum msg_hash_enums enum_idx = load_content ?
      MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
      MSG_UNKNOWN;

   if (!list)
      return -1;

   menu_entries_append_enum(list, WIIU_SD_PATH,
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);

   menu_entries_append_enum(list, WIIU_USB_PATH,
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#endif
   return 0;
}


static void frontend_wiiu_exec(const char *path, bool should_load_game)
{

   struct
   {
      u32 magic;
      u32 argc;
      char * argv[3];
      char args[];
   }*param     = getApplicationEndAddr();
   int len     = 0;
   param->argc = 0;

   if(!path || !*path)
   {
      RARCH_LOG("No executable path provided, cannot Restart\n");
   }

   DEBUG_STR(path);

   strcpy(param->args + len, elf_path_cst);
   param->argv[param->argc] = param->args + len;
   len += strlen(param->args + len) + 1;
   param->argc++;

   RARCH_LOG("Attempt to load core: [%s].\n", path);
#ifndef IS_SALAMANDER
   if (should_load_game && !path_is_empty(RARCH_PATH_CONTENT))
   {
      strcpy(param->args + len, path_get(RARCH_PATH_CONTENT));
      param->argv[param->argc] = param->args + len;
      len += strlen(param->args + len) + 1;
      param->argc++;

      RARCH_LOG("content path: [%s].\n", path_get(RARCH_PATH_CONTENT));
   }
#endif
   param->argv[param->argc] = NULL;

   {
      if (HBL_loadToMemory(path, (u32)param->args - (u32)param + len) < 0)
         RARCH_LOG("Failed to load core\n");
      else
      {
         param->magic = ARGV_MAGIC;
         ARGV_PTR = param;
         DEBUG_VAR(param->argc);
         DEBUG_VAR(param->argv);

      }
   }
}

#ifndef IS_SALAMANDER
static bool frontend_wiiu_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         RARCH_LOG("FRONTEND_FORK_CORE\n");
         wiiu_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         RARCH_LOG("FRONTEND_FORK_CORE_WITH_ARGS\n");
         wiiu_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         RARCH_LOG("FRONTEND_FORK_RESTART\n");
         /* NOTE: We don't implement Salamander, so just turn
          * this into FRONTEND_FORK_CORE. */
         wiiu_fork_mode  = FRONTEND_FORK_CORE;
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}
#endif

static void frontend_wiiu_exitspawn(char *s, size_t len)
{
   bool should_load_game = false;
#ifndef IS_SALAMANDER
   if (wiiu_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (wiiu_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_game = true;
         break;
      default:
         break;
   }
#endif
   frontend_wiiu_exec(s, should_load_game);
}


frontend_ctx_driver_t frontend_ctx_wiiu =
{
   frontend_wiiu_get_environment_settings,
   frontend_wiiu_init,
   frontend_wiiu_deinit,
   frontend_wiiu_exitspawn,
   NULL,                         /* process_args */
   frontend_wiiu_exec,
#ifdef IS_SALAMANDER
   NULL,                         /* set_fork */
#else
   frontend_wiiu_set_fork,
#endif
   frontend_wiiu_shutdown,
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_wiiu_get_rating,
   NULL,                         /* load_content */
   frontend_wiiu_get_architecture,
   NULL,                         /* get_powerstate */
   frontend_wiiu_parse_drive_list,
   NULL,                         /* get_mem_total */
   NULL,                         /* get_mem_free */
   NULL,                         /* install_signal_handler */
   NULL,                         /* get_signal_handler_state */
   NULL,                         /* set_signal_handler_state */
   NULL,                         /* destroy_signal_handler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   NULL,                         /* watch_path_for_changes */
   NULL,                         /* check_for_path_changes */
   "wiiu",
   NULL,                         /* get_video_driver */
};
