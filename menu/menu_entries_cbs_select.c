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

#include "menu.h"
#include "menu_entry.h"
#include "menu_entries_cbs.h"
#include "menu_setting.h"

#include "../runloop_data.h"

static int action_select_default(const char *path, const char *label, unsigned type,
      size_t idx)
{
   int ret = 0;
   menu_entry_t entry = {{0}};

   menu_entry_get(&entry, idx, NULL, false);

   ret = menu_entry_action(&entry, idx, MENU_ACTION_SELECT);

   rarch_main_data_iterate();
    
   return ret;
}

static int action_select_directory(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return 0;
}

void menu_entries_cbs_init_bind_select(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return;

   cbs->action_select = action_select_default;

   switch (type)
   {
      case MENU_FILE_PATH:
      case MENU_FILE_DIRECTORY:
      case MENU_FILE_USE_DIRECTORY:
         cbs->action_select = action_select_directory;
         break;
   }
}
