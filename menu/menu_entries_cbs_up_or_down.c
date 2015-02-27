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

#include <file/file_path.h>
#include "menu.h"
#include "menu_entries_cbs.h"
#include "menu_setting.h"
#include "menu_input.h"
#include "menu_entries.h"
#include "menu_shader.h"
#include "menu_navigation.h"

#include "../file_ext.h"
#include "../file_extract.h"
#include "../file_ops.h"
#include "../config.def.h"
#include "../cheats.h"
#include "../retroarch.h"
#include "../performance.h"

#ifdef HAVE_NETWORKING
#include "../net_http.h"
#endif

#ifdef HAVE_LIBRETRODB
#include "../database_info.h"
#endif

#include "menu_database.h"

#include "../input/input_autodetect.h"
#include "../input/input_remapping.h"

#include "../gfx/video_viewport.h"

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
         if (menu->navigation.selection_ptr >= scroll_speed)
               menu_navigation_set(&menu->navigation,
                     menu->navigation.selection_ptr - scroll_speed, true);
         else
         {
            if (g_settings.menu.navigation.wraparound.vertical_enable)
               menu_navigation_set(&menu->navigation, 
                     menu_list_get_size(menu->menu_list) - 1, true);
            else
               menu_navigation_set(&menu->navigation, 0, true);
         }
         break;
      case MENU_ACTION_DOWN:
         if (menu->navigation.selection_ptr + scroll_speed < (menu_list_get_size(menu->menu_list)))
            menu_navigation_set(&menu->navigation,
                  menu->navigation.selection_ptr + scroll_speed, true);
         else
         {
            if (g_settings.menu.navigation.wraparound.vertical_enable)
               menu_navigation_clear(&menu->navigation, false);
            else
               menu_navigation_set(&menu->navigation,
                     menu_list_get_size(menu->menu_list) - 1, true);
         }
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
