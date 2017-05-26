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

#ifndef BIND_ACTION_REFRESH
#define BIND_ACTION_REFRESH(cbs, name) \
   cbs->action_refresh = name; \
   cbs->action_refresh_ident = #name;
#endif

int action_refresh_default(file_list_t *list, file_list_t *menu_list)
{
   menu_displaylist_ctx_entry_t entry;
   if (!menu_list)
      return -1;

   entry.list  = list;
   entry.stack = menu_list;

   if (!menu_displaylist_push(&entry))
      return -1;
   return 0;
}

int menu_cbs_init_bind_refresh(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   BIND_ACTION_REFRESH(cbs, action_refresh_default);

   return -1;
}
