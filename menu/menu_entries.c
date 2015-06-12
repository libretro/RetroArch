/*  RetroArch - A frontend for libretro.
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

#include <rhash.h>

#include "menu.h"
#include "menu_entries.h"

#if 0
#define DEBUG_LOG
#endif

static void menu_entries_init_log(int ret,
      const char *bind_label, const char *label, const char *elem0, const char *elem1,
      unsigned type)
{
   switch (ret)
   {
      case 0:
#ifdef DEBUG_LOG
         RARCH_WARN("Found %s bind (label: [%s], elem0: [%s], elem1: [%s], type: [%d]).\n",
               bind_label, label, elem0, elem1, type);
#endif
         break;
      default:
#ifdef DEBUG_LOG
         RARCH_WARN("Could not find %s bind (label: [%s], elem0: [%s], elem1: [%s], type: [%d]).\n",
               bind_label, label, elem0, elem1, type);
#endif
         break;
   }
}

void menu_entries_init(void *data,
      const char *path, const char *label,
      unsigned type, size_t idx)
{
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   int ret                      = 0;
   struct string_list *str_list = NULL;
   const char *menu_label       = NULL;
   menu_file_list_cbs_t *cbs    = NULL;
   file_list_t *list            = (file_list_t*)data;
   menu_handle_t *menu          = menu_driver_get_ptr();
   uint32_t label_hash = 0, menu_label_hash = 0;
   if (!menu)
      return;

   if (!list)
      return;

   cbs = (menu_file_list_cbs_t*)menu_list_get_actiondata_at_offset(list, idx);

   if (!cbs)
      return;

   menu_list_get_last_stack(menu->menu_list,
         NULL, &menu_label, NULL, NULL);

   if (label)
      str_list = string_split(label, "|");

   if (str_list && str_list->size > 0)
      strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
   else elem0[0]='\0';
   if (str_list && str_list->size > 1)
      strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));
   else elem1[0]='\0';

   if (str_list)
   {
      string_list_free(str_list);
      str_list = NULL;
   }

   label_hash      = djb2_calculate(label);
   menu_label_hash = djb2_calculate(menu_label);

   ret = menu_entries_cbs_init_bind_ok(cbs, path, label, type, idx, elem0, elem1, menu_label, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "OK", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_cancel(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "CANCEL", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_scan(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "SCAN", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_start(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "START", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_select(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "SELECT", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_info(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "INFO", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_content_list_switch(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "CONTENT SWITCH", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_up(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "UP", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_down(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "DOWN", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_left(cbs, path, label, type, idx, elem0, elem1, menu_label, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "LEFT", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_right(cbs, path, label, type, idx, elem0, elem1, menu_label, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "RIGHT", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_deferred_push(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "DEFERRED PUSH", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_refresh(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "REFRESH", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_iterate(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "ITERATE", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_get_string_representation(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "REPRESENTATION", label, elem0, elem1, type);

   ret = menu_entries_cbs_init_bind_title(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_entries_init_log(ret, "TITLE", label, elem0, elem1, type);

   ret = menu_driver_bind_init(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);
}
