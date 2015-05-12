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
#include "menu_list.h"
#include "menu_setting.h"

static int action_cancel_lookup_setting(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_setting_set(type, label, MENU_ACTION_CANCEL, false);
}

static int action_cancel_pop_default(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;

   menu_apply_deferred_settings();
   menu_list_pop_stack(menu_list);
   return 0;
}

void menu_entries_cbs_init_bind_cancel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (!cbs)
      return;

   cbs->action_cancel = action_cancel_lookup_setting;

   /* TODO - add some stuff here. */
   cbs->action_cancel = action_cancel_pop_default;
}
