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
#include "menu_database.h"
#include "menu_list.h"
#include "menu_entries.h"
#include "../playlist.h"
#include <string.h>

int menu_database_populate_query(file_list_t *list, const char *path,
    const char *query)
{
#ifdef HAVE_LIBRETRODB
   libretrodb_t db;
   libretrodb_cursor_t cur;

   if ((libretrodb_open(path, &db)) != 0)
      return -1;
   if ((database_open_cursor(&db, &cur, query) != 0))
      return -1;
   if ((menu_entries_push_query(&db, &cur, list)) != 0)
      return -1;

   libretrodb_cursor_close(&cur);
   libretrodb_close(&db);
#endif

   return 0;
}

static void menu_database_playlist_free(menu_handle_t *menu)
{
   if (menu->db_playlist)
      content_playlist_free(menu->db_playlist);
   menu->db_playlist = NULL;
}

void menu_database_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   if (!menu)
      return;
   menu_database_playlist_free(menu);
}

bool menu_database_realloc(const char *path,
      bool force)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return false;

   if (!strcmp(menu->db_playlist_file, path) && !force)
      return true;

   menu_database_playlist_free(menu);

   menu->db_playlist = content_playlist_init(path,
         1000);

   if (!menu->db_playlist)
      return false;

   strlcpy(menu->db_playlist_file, path,
         sizeof(menu->db_playlist_file));

   return true;
}
