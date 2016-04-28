/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <file/file_path.h>
#include <compat/strl.h>

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../menu_entry.h"
#include "../menu_setting.h"

#include "../../runloop.h"
#include "../../tasks/tasks_internal.h"

#ifndef BIND_ACTION_SCAN
#define BIND_ACTION_SCAN(cbs, name) \
   cbs->action_scan = name; \
   cbs->action_scan_ident = #name;
#endif

#ifdef HAVE_LIBRETRODB
static void handle_dbscan_finished(void *task_data, void *user_data, const char *err)
{
   menu_ctx_environment_t menu_environ;
   menu_environ.type = MENU_ENVIRON_RESET_HORIZONTAL_LIST;
   menu_environ.data = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
}

int action_scan_file(const char *path,
      const char *label, unsigned type, size_t idx)
{
   char fullpath[PATH_MAX_LENGTH] = {0};
   const char *menu_label         = NULL;
   const char *menu_path          = NULL;
   menu_handle_t *menu            = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_entries_get_last_stack(&menu_path, &menu_label, NULL, NULL);

   fill_pathname_join(fullpath, menu_path, path, sizeof(fullpath));

   rarch_task_push_dbscan(fullpath, false, handle_dbscan_finished);

   return 0;
}

int action_scan_directory(const char *path,
      const char *label, unsigned type, size_t idx)
{
   char fullpath[PATH_MAX_LENGTH] = {0};
   const char *menu_label         = NULL;
   const char *menu_path          = NULL;
   menu_handle_t *menu            = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_entries_get_last_stack(&menu_path, &menu_label, NULL, NULL);

   strlcpy(fullpath, menu_path, sizeof(fullpath));

   if (path)
      fill_pathname_join(fullpath, fullpath, path, sizeof(fullpath));

   rarch_task_push_dbscan(fullpath, true, handle_dbscan_finished);

   return 0;
}
#endif

int action_switch_thumbnail(const char *path,
      const char *label, unsigned type, size_t idx)
{
   settings_t *settings = config_get_ptr();

   if (!settings)
      return -1;
   if (settings->menu.thumbnails == 0)
      return 0;

   settings->menu.thumbnails++;

   if (settings->menu.thumbnails > 3)
      settings->menu.thumbnails = 1;

   menu_driver_ctl(RARCH_MENU_CTL_UPDATE_THUMBNAIL_PATH, NULL);
   menu_driver_ctl(RARCH_MENU_CTL_UPDATE_THUMBNAIL_IMAGE, NULL);

   return 0;
}

static int menu_cbs_init_bind_scan_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type)
{
   switch (type)
   {
#ifdef HAVE_LIBRETRODB
      case MENU_FILE_DIRECTORY:
         BIND_ACTION_SCAN(cbs, action_scan_directory);
         return 0;
      case MENU_FILE_CARCHIVE:
      case MENU_FILE_PLAIN:
         BIND_ACTION_SCAN(cbs, action_scan_file);
         return 0;
#endif
      case MENU_FILE_RPL_ENTRY:
         BIND_ACTION_SCAN(cbs, action_switch_thumbnail);
         break;

      case MENU_FILE_NONE:
      default:
         break;
   }

   return -1;
}

int menu_cbs_init_bind_scan(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   BIND_ACTION_SCAN(cbs, NULL);

   menu_cbs_init_bind_scan_compare_type(cbs, type);

   return -1;
}
