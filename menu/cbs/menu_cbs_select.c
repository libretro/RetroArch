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

#include "../menu.h"
#include "../menu_entry.h"
#include "../menu_cbs.h"
#include "../menu_setting.h"

#include "../../runloop_data.h"

static int action_select_default(const char *path, const char *label, unsigned type,
      size_t idx)
{
   menu_entry_t entry;
   int ret                   = 0;
   enum menu_action action   = MENU_ACTION_NOOP;
   menu_file_list_cbs_t *cbs = NULL;
   menu_list_t    *menu_list = menu_list_get_ptr();
   rarch_setting_t *setting  = menu_setting_find(
         menu_list->selection_buf->list[idx].label);

   menu_entry_get(&entry, idx, NULL, false);

   cbs = menu_list_get_actiondata_at_offset(menu_list->selection_buf, idx);

   if ((cbs && cbs->action_ok) || menu_setting_is_of_general_type(setting))
       action = MENU_ACTION_OK;
   else { if (cbs && cbs->action_start)
         action = MENU_ACTION_START;
      if (cbs && cbs->action_right)
         action = MENU_ACTION_RIGHT;
   }
    
   if (action != MENU_ACTION_NOOP)
       ret = menu_entry_action(&entry, idx, action);

   rarch_main_data_iterate();
    
   return ret;
}

static int action_select_directory(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return 0;
}

static int action_select_core_setting(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return core_setting_right(type, label, true);
}

static int action_select_cheat(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return action_right_cheat(type, label, true);
}

static int action_select_input_desc(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return action_right_input_desc(type, label, true);
}

static int menu_cbs_init_bind_select_compare_type(
      menu_file_list_cbs_t *cbs, unsigned type)
{
   if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
      cbs->action_select = action_select_cheat;
   else if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
      cbs->action_select = action_select_input_desc;
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START))
      cbs->action_select = action_select_core_setting;
   else
   {
      switch (type)
      {
         case MENU_FILE_PATH:
         case MENU_FILE_DIRECTORY:
         case MENU_FILE_USE_DIRECTORY:
            cbs->action_select = action_select_directory;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

int menu_cbs_init_bind_select(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   cbs->action_select = action_select_default;

   if (menu_cbs_init_bind_select_compare_type(cbs, type) == 0)
      return 0;

   return -1;
}
