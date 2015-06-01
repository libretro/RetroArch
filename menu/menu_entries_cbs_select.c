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
   menu_entry_t entry;
   enum menu_action action   = MENU_ACTION_NOOP;
   menu_file_list_cbs_t *cbs = NULL;
   menu_list_t    *menu_list = menu_list_get_ptr();
   rarch_setting_t *setting  = menu_setting_find(
         menu_list->selection_buf->list[idx].label);

   menu_entry_get(&entry, idx, NULL, false);

   cbs = menu_list_get_actiondata_at_offset(menu_list->selection_buf, idx);

   if (setting_is_of_path_type(setting))
      return 0;

   if ((cbs && cbs->action_ok) || setting_is_of_general_type(setting))
       action = MENU_ACTION_OK;
   else
   {
      if (cbs && cbs->action_start)
         action = MENU_ACTION_START;
      if (cbs && cbs->action_right)
         action = MENU_ACTION_RIGHT;
   }
    
   if (action != MENU_ACTION_NOOP)
       ret = menu_entry_action(&entry, idx, action);

   rarch_main_data_iterate();
    
   return ret;
}

void menu_entries_cbs_init_bind_select(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   cbs->action_select = action_select_default;
}
