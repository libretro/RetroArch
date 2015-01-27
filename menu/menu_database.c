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
#include <string.h>

#ifdef HAVE_LIBRETRODB
static int menu_database_open_cursor(libretrodb_t *db,
   libretrodb_cursor_t *cur, const char *query)
{
   const char *error     = NULL;
   libretrodb_query_t *q = NULL;

   if (query) 
      q = (libretrodb_query_t*)libretrodb_query_compile(db, query,
      strlen(query), &error);
    
   if (error)
      return -1;
   if ((libretrodb_cursor_open(db, cur, q)) != 0)
      return -1;

   return 0;
}

static int menu_database_fetch_from_query(libretrodb_t *db,
   libretrodb_cursor_t *cur, file_list_t *list)
{
   unsigned i;
   struct rmsgpack_dom_value item;
    
   while (libretrodb_cursor_read_item(cur, &item) == 0)
   {
      if (item.type != RDT_MAP)
         continue;
        
      for (i = 0; i < item.map.len; i++)
      {
         struct rmsgpack_dom_value *key = &item.map.items[i].key;
         struct rmsgpack_dom_value *val = &item.map.items[i].value;
            
         if (!strcmp(key->string.buff, "description"))
         {
            menu_list_push(list, val->string.buff, db->path,
            MENU_FILE_RDB_ENTRY, 0);
            break;
         }
      }
   }
    
   return 0;
}
#endif

int menu_database_populate_query(file_list_t *list, const char *path,
    const char *query)
{
#ifdef HAVE_LIBRETRODB
   libretrodb_t db;
   libretrodb_cursor_t cur;
    
   if ((libretrodb_open(path, &db)) != 0)
      return -1;
   if ((menu_database_open_cursor(&db, &cur, query) != 0))
      return -1;
   if ((menu_database_fetch_from_query(&db, &cur, list)) != 0)
      return -1;
    
   libretrodb_cursor_close(&cur);
   libretrodb_close(&db);
#endif

   return 0;
}
