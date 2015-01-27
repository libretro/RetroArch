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
#include "../database_info.h"
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

int menu_database_print_info(const char *path,
    const char *query)
{
#ifdef HAVE_LIBRETRODB
   size_t i;
   database_info_list_t *db_info = NULL;

   if (!(db_info = database_info_list_new(path, query)))
      return -1;

   for (i = 0; i < db_info->count; i++)
   {
      database_info_t *db_info_entry = (database_info_t*)&db_info->list[i];

      if (!db_info_entry)
         continue;

      if (db_info_entry->description)
         RARCH_LOG("Description: %s\n", db_info_entry->description);
      if (db_info_entry->publisher)
         RARCH_LOG("Publisher: %s\n", db_info_entry->publisher);
      if (db_info_entry->developer)
         RARCH_LOG("Developer: %s\n", db_info_entry->developer);
      if (db_info_entry->origin)
         RARCH_LOG("Origin: %s\n", db_info_entry->origin);
      if (db_info_entry->franchise)
         RARCH_LOG("Franchise: %s\n", db_info_entry->franchise);
      if (db_info_entry->max_users)
         RARCH_LOG("Max Users: %d\n", db_info_entry->max_users);
      if (db_info_entry->edge_magazine_rating)
         RARCH_LOG("Edge Magazine Rating: %d/10\n", db_info_entry->edge_magazine_rating);
      if (db_info_entry->edge_magazine_issue)
         RARCH_LOG("Edge Magazine Issue: %d\n", db_info_entry->edge_magazine_issue);
      if (db_info_entry->edge_magazine_review)
         RARCH_LOG("Edge Magazine Review: %s\n", db_info_entry->edge_magazine_review);
      if (db_info_entry->releasemonth)
         RARCH_LOG("Releasedate Month: %d\n", db_info_entry->releasemonth);
      if (db_info_entry->releaseyear)
         RARCH_LOG("Releasedate Year: %d\n", db_info_entry->releaseyear);
      if (db_info_entry->bbfc_rating)
         RARCH_LOG("BBFC Rating: %s\n", db_info_entry->bbfc_rating);
      if (db_info_entry->esrb_rating)
         RARCH_LOG("ESRB Rating: %s\n", db_info_entry->esrb_rating);
      if (db_info_entry->elspa_rating)
         RARCH_LOG("ELSPA Rating: %s\n", db_info_entry->elspa_rating);
      if (db_info_entry->pegi_rating)
         RARCH_LOG("PEGI Rating: %s\n", db_info_entry->pegi_rating);
      if (db_info_entry->cero_rating)
         RARCH_LOG("CERO Rating: %s\n", db_info_entry->cero_rating);
      RARCH_LOG("\n\n");
   }

   if (db_info)
      database_info_list_free(db_info);
   db_info = NULL;
#endif
   return 0;
}
