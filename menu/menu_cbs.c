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

#include <compat/strl.h>
#include <string/stdstring.h>
#include <string/string_list.h>

#include "menu_driver.h"
#include "menu_hash.h"
#include "menu_cbs.h"
#include "../verbosity.h"

#if 0
#define DEBUG_LOG
#endif

static void menu_cbs_init_log(const char *entry_label, const char *bind_label, const char *label)
{
#ifdef DEBUG_LOG
   if (!string_is_empty(label))
      RARCH_LOG("[%s]\t\t\tFound %s bind : [%s]\n", entry_label, bind_label, label);
#endif
}

void menu_cbs_init(void *data,
      menu_file_list_cbs_t *cbs,
      const char *path, const char *label,
      unsigned type, size_t idx)
{
   menu_ctx_bind_t bind_info;
   char elem0[PATH_MAX_LENGTH];
   char elem1[PATH_MAX_LENGTH];
   const char *repr_label       = NULL;
   struct string_list *str_list = NULL;
   const char *menu_label       = NULL;
   int ret                      = 0;
   uint32_t label_hash          = 0;
   uint32_t menu_label_hash     = 0;
   file_list_t *list            = (file_list_t*)data;
   if (!list)
      return;

   elem0[0] = '\0';
   elem1[0] = '\0';

   menu_entries_get_last_stack(NULL, &menu_label, NULL, NULL);

   if (label)
      str_list = string_split(label, "|");

   if (str_list && str_list->size > 0)
      strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
   if (str_list && str_list->size > 1)
      strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));

   if (!label || !menu_label)
      goto error;

   label_hash      = menu_hash_calculate(label);
   menu_label_hash = menu_hash_calculate(menu_label);

#ifdef DEBUG_LOG
   RARCH_LOG("\n");
#endif

   repr_label = (!string_is_empty(label)) ? label : path;

   ret = menu_cbs_init_bind_ok(cbs, path, label, type, idx, elem0, elem1, menu_label, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "OK", cbs->action_ok_ident);

   ret = menu_cbs_init_bind_cancel(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "CANCEL", cbs->action_cancel_ident);

   ret = menu_cbs_init_bind_scan(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "SCAN", cbs->action_scan_ident);

   ret = menu_cbs_init_bind_start(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "START", cbs->action_start_ident);

   ret = menu_cbs_init_bind_select(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "SELECT", cbs->action_select_ident);

   ret = menu_cbs_init_bind_info(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "INFO", cbs->action_info_ident);

   ret = menu_cbs_init_bind_content_list_switch(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "CONTENT SWITCH", cbs->action_content_list_switch_ident);

   ret = menu_cbs_init_bind_up(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "UP", cbs->action_up_ident);

   ret = menu_cbs_init_bind_down(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "DOWN", cbs->action_down_ident);

   ret = menu_cbs_init_bind_left(cbs, path, label, type, idx, elem0, elem1, menu_label, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "LEFT", cbs->action_left_ident);

   ret = menu_cbs_init_bind_right(cbs, path, label, type, idx, elem0, elem1, menu_label, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "RIGHT", cbs->action_right_ident);

   ret = menu_cbs_init_bind_deferred_push(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "DEFERRED PUSH", cbs->action_deferred_push_ident);

   ret = menu_cbs_init_bind_refresh(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "REFRESH", cbs->action_refresh_ident);

   ret = menu_cbs_init_bind_get_string_representation(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "GET VALUE", cbs->action_get_value_ident);

   ret = menu_cbs_init_bind_title(cbs, path, label, type, idx, elem0, elem1, label_hash, menu_label_hash);

   menu_cbs_init_log(repr_label, "GET TITLE", cbs->action_get_title_ident);

   bind_info.cbs             = cbs;
   bind_info.path            = path;
   bind_info.label           = label;
   bind_info.type            = type;
   bind_info.idx             = idx;
   bind_info.elem0           = elem0;
   bind_info.elem1           = elem1;
   bind_info.label_hash      = label_hash;
   bind_info.menu_label_hash = menu_label_hash;

   menu_driver_ctl(RARCH_MENU_CTL_BIND_INIT, &bind_info);

   ret = bind_info.retcode;

   (void)ret;

error:
   string_list_free(str_list);
   str_list = NULL;
}

int menu_cbs_exit(void)
{
   return -1;
}
