/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include "menu_generic.h"

#include "../menu.h"
#include "../menu_displaylist.h"
#include "../menu_hash.h"
#include "../menu_list.h"

bool generic_menu_init_list(void *data)
{
   menu_displaylist_info_t info = {0};
   menu_handle_t   *menu  = (menu_handle_t*)data;
   menu_list_t *menu_list = menu_list_get_ptr();

   if (!menu || !menu_list)
      return false;

   info.list  = menu_list->selection_buf;
   info.type  = MENU_SETTINGS;
   info.flags = SL_FLAG_MAIN_MENU | SL_FLAG_MAIN_MENU_SETTINGS;
   strlcpy(info.label, menu_hash_to_str(MENU_VALUE_MAIN_MENU), sizeof(info.label));

   menu_list_push(menu_list->menu_stack,
         info.path, info.label, info.type, info.flags, 0);
   menu_displaylist_push_list(&info, DISPLAYLIST_MAIN_MENU);

   return true;
}
