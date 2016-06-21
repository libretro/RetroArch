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
#include <string/stdstring.h>

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../../msg_hash.h"

#ifndef BIND_ACTION_CANCEL
#define BIND_ACTION_CANCEL(cbs, name) \
   cbs->action_cancel = name; \
   cbs->action_cancel_ident = #name;
#endif

static int action_cancel_pop_default(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_entry_go_back();
}

static int action_cancel_core_content(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_ADD_CONTENT_LIST), 0);
   return 0;
}

static int menu_cbs_init_bind_cancel_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t hash, const char *elem0, const char *menu_label)
{
   uint32_t elem0_hash      = msg_hash_calculate(elem0);

   if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST)))
   {
      BIND_ACTION_CANCEL(cbs, action_cancel_core_content);
      return 0;
   }

   return -1;
}

static int menu_cbs_init_bind_cancel_compare_type(
      menu_file_list_cbs_t *cbs, unsigned type,
      uint32_t label_hash)
{
   return -1;
}

int menu_cbs_init_bind_cancel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1, const char *menu_label,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;


   BIND_ACTION_CANCEL(cbs, action_cancel_pop_default);

   if (menu_cbs_init_bind_cancel_compare_label(cbs, label, label_hash, elem0, menu_label) == 0)
      return 0;

   if (menu_cbs_init_bind_cancel_compare_type(
            cbs, type, label_hash) == 0)
      return 0;

   return -1;
}
