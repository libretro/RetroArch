/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "menu_common.h"
#include "menu_entries.h"
#include "backend/menu_backend.h"

/* TODO - return with error values. */

static int action_ok_push_content_list(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return 0;

   menu_entries_push(driver.menu->menu_stack,
         g_settings.menu_content_directory, label, MENU_FILE_DIRECTORY,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_push_history_or_path_list(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return 0;

   menu_entries_push(driver.menu->menu_stack,
         "", label, type, driver.menu->selection_ptr);
   return 0;
}

void menu_entries_cbs_init(void *data,
      const char *path, const char *label,
      unsigned type, size_t index)
{
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *list = (file_list_t*)data;

   if (!list)
      return;

   cbs = (menu_file_list_cbs_t*)list->list[index].actiondata;

   if (!cbs)
      return;

   cbs->action_ok = NULL;

   if (
         !strcmp(label, "load_content") ||
         !strcmp(label, "detect_core_list")
      )
      cbs->action_ok = action_ok_push_content_list;
   else if (!strcmp(label, "history_list") ||
         menu_common_type_is(label, type) == MENU_FILE_DIRECTORY
         )
      cbs->action_ok = action_ok_push_history_or_path_list;
}
