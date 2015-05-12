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

#include "menu_displaylist.h"
#include "menu_entries.h"
#include "menu_setting.h"
#include "menu_navigation.h"
#include <file/file_list.h>
#include <file/file_path.h>
#include <file/file_extract.h>
#include <file/dir_list.h>

int menu_entries_setting_set_flags(rarch_setting_t *setting)
{
   if (!setting)
      return 0;

   if (setting->flags & SD_FLAG_IS_DRIVER)
      return MENU_SETTING_DRIVER;

   switch (setting->type)
   {
      case ST_ACTION:
         return MENU_SETTING_ACTION;
      case ST_PATH:
         return MENU_FILE_PATH;
      case ST_GROUP:
         return MENU_SETTING_GROUP;
      case ST_SUB_GROUP:
         return MENU_SETTING_SUBGROUP;
      default:
         break;
   }

   return 0;
}

/**
 * menu_entries_init:
 * @menu                     : Menu handle.
 *
 * Creates and initializes menu entries.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool menu_entries_init(menu_handle_t *menu)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   menu_displaylist_info_t info = {0};
   if (!menu || !menu_list)
      return false;

   info.list  = menu_list->selection_buf;
   info.type  = MENU_SETTINGS;
   info.flags = SL_FLAG_MAIN_MENU;
   strlcpy(info.label, "Main Menu", sizeof(info.label));

   menu_displaylist_push_list(&info, DISPLAYLIST_MAIN_MENU);

   return true;
}
