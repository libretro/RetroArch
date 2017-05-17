/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include "../menu_driver.h"
#include "../menu_cbs.h"

#ifndef BIND_ACTION_DOWN
#define BIND_ACTION_DOWN(cbs, name) \
   cbs->action_down = name; \
   cbs->action_down_ident = #name;
#endif

static int action_bind_down_generic(unsigned type, const char *label)
{
   size_t scroll_accel    = 0;
   unsigned scroll_speed  = 0;
   if (!menu_driver_ctl(MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL, &scroll_accel))
      return -1;

   scroll_speed = (unsigned)((MAX(scroll_accel, 2) - 2) / 4 + 1);

   if (menu_entries_get_size() <= 0)
      return 0;

   menu_driver_ctl(MENU_NAVIGATION_CTL_INCREMENT, &scroll_speed);

   return 0;
}

int menu_cbs_init_bind_down(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   BIND_ACTION_DOWN(cbs, action_bind_down_generic);

   return -1;
}
