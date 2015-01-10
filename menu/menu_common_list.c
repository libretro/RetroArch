/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "menu.h"
#include "menu_common_list.h"
#include "menu_list.h"
#include "menu_entries_cbs.h"
#include <file/file_path.h>

void menu_common_list_clear(void *data)
{
}

void menu_common_list_set_selection(void *data)
{
}

void menu_common_list_insert(void *data,
      const char *path, const char *label,
      unsigned type, size_t idx)
{
   file_list_t *list = (file_list_t*)data;

   if (!list)
      return;

   list->list[idx].actiondata = (menu_file_list_cbs_t*)
      calloc(1, sizeof(menu_file_list_cbs_t));

   if (!list->list[idx].actiondata)
   {
      RARCH_ERR("Action data could not be allocated.\n");
      return;
   }

   menu_entries_cbs_init(list, path, label, type, idx);
}

void menu_common_list_delete(void *data, size_t idx,
      size_t list_size)
{
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *list = (file_list_t*)data;

   if (!list)
      return;

   cbs = (menu_file_list_cbs_t*)list->list[idx].actiondata;

   if (cbs)
   {
      cbs->action_start         = NULL;
      cbs->action_ok            = NULL;
      cbs->action_cancel        = NULL;
      cbs->action_toggle        = NULL;
      cbs->action_deferred_push = NULL;
      free(list->list[idx].actiondata);
   }
   list->list[idx].actiondata = NULL;
}
