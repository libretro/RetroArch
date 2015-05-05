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

#include "../menu/menu_driver.h"

#include "../runloop_data.h"
#include "tasks.h"

#ifdef HAVE_LIBRETRODB
#ifdef HAVE_MENU
void rarch_main_data_db_iterate(bool is_thread, void *data)
{
   data_runloop_t         *runloop = (data_runloop_t*)data;
   menu_handle_t             *menu = menu_driver_get_ptr();
   database_info_handle_t      *db = menu ? menu->db : NULL;

   if (!db || !menu || !runloop)
      return;

   switch (db->status)
   {
      case DATABASE_STATUS_NONE:
         break;
      case DATABASE_STATUS_ITERATE:
         database_info_iterate(db);
         break;
      case DATABASE_STATUS_FREE:
         database_info_free(db);
         db = NULL;
         break;
   }
}
#endif
#endif
