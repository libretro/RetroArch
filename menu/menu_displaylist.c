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
#include "menu_display.h"
#include "menu_entries.h"

int menu_displaylist_push(file_list_t *list, file_list_t *menu_list)
{
   menu_handle_t *menu    = menu_driver_get_ptr();
   driver_t       *driver = driver_get_ptr();
   int                ret = menu_entries_deferred_push(list, menu_list);

   menu->need_refresh = false;

   if (ret == 0)
   {
      const ui_companion_driver_t *ui = ui_companion_get_ptr();

      if (ui)
         ui->notify_list_loaded(driver->ui_companion_data, list, menu_list);
   }

   return ret;
}
