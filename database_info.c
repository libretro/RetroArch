/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2013-2015 - Jason Fetters
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

#include "database_info.h"
#include "general.h"
#include <file/file_path.h>
#include "file_ext.h"
#include <file/dir_list.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

int database_open_cursor(libretrodb_t *db,
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

database_info_list_t *database_info_list_new(const char *rdb_path, const char *query)
{
   size_t i = 0, j;
   libretrodb_t db;
   libretrodb_cursor_t cur;
   struct rmsgpack_dom_value item;
   database_info_t *database_info = NULL;
   database_info_list_t *database_info_list = NULL;

   if ((libretrodb_open(rdb_path, &db)) != 0)
      return NULL;
   if ((database_open_cursor(&db, &cur, query) != 0))
      return NULL;

   database_info_list = (database_info_list_t*)calloc(1, sizeof(*database_info_list));
   if (!database_info_list)
      goto error;

   while (libretrodb_cursor_read_item(&cur, &item) == 0)
   {
      database_info_t *db_info = NULL;
      if (item.type != RDT_MAP)
         continue;

      database_info = (database_info_t*)realloc(database_info, (i+1) * sizeof(database_info_t));

      if (!database_info)
         goto error;

      db_info = (database_info_t*)&database_info[i];

      db_info->description = NULL;
      db_info->publisher   = NULL;
      db_info->developer   = NULL;
      db_info->origin      = NULL;
      db_info->franchise   = NULL;

      for (j = 0; j < item.map.len; j++)
      {
         struct rmsgpack_dom_value *key = &item.map.items[j].key;
         struct rmsgpack_dom_value *val = &item.map.items[j].value;

         if (!strcmp(key->string.buff, "description"))
            db_info->description = strdup(val->string.buff);

         if (!strcmp(key->string.buff, "publisher"))
            db_info->publisher = strdup(val->string.buff);

         if (!strcmp(key->string.buff, "developer"))
            db_info->developer = strdup(val->string.buff);

         if (!strcmp(key->string.buff, "origin"))
            db_info->origin = strdup(val->string.buff);

         if (!strcmp(key->string.buff, "franchise"))
            db_info->franchise = strdup(val->string.buff);
      }
i++;
   }

   database_info_list->list  = database_info;
   database_info_list->count = i;

   return database_info_list;

error:
   libretrodb_cursor_close(&cur);
   libretrodb_close(&db);
   database_info_list_free(database_info_list);
   return NULL;
}

void database_info_list_free(database_info_list_t *database_info_list)
{
   size_t i, j;

   if (!database_info_list)
      return;

   for (i = 0; i < database_info_list->count; i++)
   {
      database_info_t *info = (database_info_t*)&database_info_list->list[i];

      if (!info)
         continue;

      if (info->description)
         free(info->description);
      if (info->publisher)
         free(info->publisher);
      if (info->developer)
         free(info->developer);
      if (info->origin)
         free(info->origin);
      if (info->franchise)
         free(info->franchise);
   }

   free(database_info_list->list);
   free(database_info_list);
}
