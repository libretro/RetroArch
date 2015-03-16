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
#include "menu_entries.h"
#include "menu_navigation.h"
#include "menu_entries_cbs.h"

static int action_bind_up_or_down_generic(unsigned type, const char *label,
      unsigned action)
{
   unsigned scroll_speed  = 0;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;

   scroll_speed = (max(menu->navigation.scroll.acceleration, 2) - 2) / 4 + 1;

   if (menu_list_get_size(menu->menu_list) <= 0)
      return 0;

   switch (action)
   {
      case MENU_ACTION_UP:
         menu_navigation_decrement(&menu->navigation, scroll_speed);
         break;
      case MENU_ACTION_DOWN:
         menu_navigation_increment(&menu->navigation, scroll_speed);
         break;
   }

   return 0;
}

void menu_entries_cbs_init_bind_up_or_down(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   cbs->action_up_or_down = action_bind_up_or_down_generic;
}
