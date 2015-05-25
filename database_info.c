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

#include "file_ext.h"
#include "dir_list_special.h"
#include <file/file_extract.h>

#include "database_info.h"
#include "hash.h"
#include "file_ops.h"
#include "general.h"
#include "runloop.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

int database_info_build_query(
      char *query, size_t len, const char *label, const char *path)
{
   bool add_quotes = true;

   strlcpy(query, "{'", len);

   if (!strcmp(label, "displaylist_parse_database_entry"))
      strlcat(query, "name", len);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_publisher"))
      strlcat(query, "publisher", len);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_developer"))
      strlcat(query, "developer", len);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_origin"))
      strlcat(query, "origin", len);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_franchise"))
      strlcat(query, "franchise", len);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_esrb_rating"))
      strlcat(query, "esrb_rating", len);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_bbfc_rating"))
      strlcat(query, "bbfc_rating", len);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_elspa_rating"))
      strlcat(query, "elspa_rating", len);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_pegi_rating"))
      strlcat(query, "pegi_rating", len);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_enhancement_hw"))
      strlcat(query, "enhancement_hw", len);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_cero_rating"))
      strlcat(query, "cero_rating", len);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_rating"))
   {
      strlcat(query, "edge_rating", len);
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_issue"))
   {
      strlcat(query, "edge_issue", len);
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_famitsu_magazine_rating"))
   {
      strlcat(query, "famitsu_rating", len);
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releasemonth"))
   {
      strlcat(query, "releasemonth", len);
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releaseyear"))
   {
      strlcat(query, "releaseyear", len);
      add_quotes = false;
   }
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_max_users"))
   {
      strlcat(query, "users", len);
      add_quotes = false;
   }

   strlcat(query, "':", len);
   if (add_quotes)
      strlcat(query, "\"", len);
   strlcat(query, path, len);
   if (add_quotes)
      strlcat(query, "\"", len);
   strlcat(query, "}", len);

#if 0
   RARCH_LOG("query: %s\n", query);
#endif
   return 0;
}

static char *bin_to_hex_alloc(const uint8_t *data, size_t len)
{
   size_t i;
   char *ret = (char*)malloc(len * 2 + 1);

   if (len && !ret)
      return NULL;
   
   for (i = 0; i < len; i++)
      snprintf(ret+i*2, 3, "%02X", data[i]);
   return ret;
}

int database_cursor_iterate(libretrodb_cursor_t *cur,
      database_info_t *db_info)
{
   unsigned i;
   struct rmsgpack_dom_value item;

   if (libretrodb_cursor_read_item(cur, &item) != 0)
      return -1;

   if (item.type != RDT_MAP)
      return 1;

   db_info->analog_supported       = -1;
   db_info->rumble_supported       = -1;

   for (i = 0; i < item.map.len; i++)
   {
      struct rmsgpack_dom_value *key = &item.map.items[i].key;
      struct rmsgpack_dom_value *val = &item.map.items[i].value;

      if (!key || !val)
         continue;

      if (!strcmp(key->string.buff, "name"))
         db_info->name = strdup(val->string.buff);

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

      if (!strcmp(key->string.buff, "bbfc_rating"))
         db_info->bbfc_rating = strdup(val->string.buff);

      if (!strcmp(key->string.buff, "esrb_rating"))
         db_info->esrb_rating = strdup(val->string.buff);

      if (!strcmp(key->string.buff, "elspa_rating"))
         db_info->elspa_rating = strdup(val->string.buff);

      if (!strcmp(key->string.buff, "cero_rating"))
         db_info->cero_rating = strdup(val->string.buff);

      if (!strcmp(key->string.buff, "pegi_rating"))
         db_info->pegi_rating = strdup(val->string.buff);

      if (!strcmp(key->string.buff, "enhancement_hw"))
         db_info->enhancement_hw = strdup(val->string.buff);

      if (!strcmp(key->string.buff, "edge_review"))
         db_info->edge_magazine_review = strdup(val->string.buff);

      if (!strcmp(key->string.buff, "edge_rating"))
         db_info->edge_magazine_rating = val->uint_;

      if (!strcmp(key->string.buff, "edge_issue"))
         db_info->edge_magazine_issue = val->uint_;

      if (!strcmp(key->string.buff, "famitsu_rating"))
         db_info->famitsu_magazine_rating = val->uint_;

      if (!strcmp(key->string.buff, "users"))
         db_info->max_users = val->uint_;

      if (!strcmp(key->string.buff, "releasemonth"))
         db_info->releasemonth = val->uint_;

      if (!strcmp(key->string.buff, "releaseyear"))
         db_info->releaseyear = val->uint_;

      if (!strcmp(key->string.buff, "rumble"))
         db_info->rumble_supported = val->uint_;

      if (!strcmp(key->string.buff, "analog"))
         db_info->analog_supported = val->uint_;

      if (!strcmp(key->string.buff, "crc"))
         db_info->crc32 = bin_to_hex_alloc((uint8_t*)val->binary.buff, val->binary.len);
      if (!strcmp(key->string.buff, "sha1"))
         db_info->sha1 = bin_to_hex_alloc((uint8_t*)val->binary.buff, val->binary.len);
      if (!strcmp(key->string.buff, "md5"))
         db_info->md5 = bin_to_hex_alloc((uint8_t*)val->binary.buff, val->binary.len);
   }

   return 0;
}

int database_cursor_open(libretrodb_t *db,
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

int database_cursor_close(libretrodb_t *db, libretrodb_cursor_t *cur)
{
   libretrodb_cursor_close(cur);
   libretrodb_close(db);

   return 0;
}

database_info_handle_t *database_info_init(const char *dir, enum database_type type)
{
   database_info_handle_t     *db  = (database_info_handle_t*)calloc(1, sizeof(*db));

   if (!db)
      return NULL;

   db->list           = dir_list_new_special(dir, DIR_LIST_CORE_INFO);

   if (!db->list)
      goto error;

   db->list_ptr       = 0;
   db->status         = DATABASE_STATUS_ITERATE;
   db->type           = type;

   return db;

error:
   if (db)
      free(db);
   return NULL;
}

void database_info_free(database_info_handle_t *db)
{
   if (!db)
      return;

   string_list_free(db->list);
}


database_info_list_t *database_info_list_new(const char *rdb_path, const char *query)
{
   int ret = 0;
   libretrodb_t db;
   libretrodb_cursor_t cur;
   struct rmsgpack_dom_value item;
   size_t j;
   unsigned k                               = 0;
   database_info_t *database_info           = NULL;
   database_info_list_t *database_info_list = NULL;

   if ((libretrodb_open(rdb_path, &db)) != 0)
      return NULL;
   if ((database_cursor_open(&db, &cur, query) != 0))
      return NULL;

   database_info_list = (database_info_list_t*)calloc(1, sizeof(*database_info_list));
   if (!database_info_list)
      goto end;

   while (ret != -1)
   {
      database_info_t db_info = {0};
      ret = database_cursor_iterate(&cur, &db_info);

      if (ret == 0)
      {
         database_info_t *db_ptr = NULL;
         database_info = (database_info_t*)realloc(database_info, (k+1) * sizeof(database_info_t));

         if (!database_info)
         {
            database_info_list_free(database_info_list);
            database_info_list = NULL;
            goto end;
         }

         db_ptr = &database_info[k];

         if (!db_ptr)
            continue;

         memcpy(db_ptr, &db_info, sizeof(*db_ptr));

         k++;
      }
   } 

   database_info_list->list  = database_info;
   database_info_list->count = k;

end:
   database_cursor_close(&db, &cur);

   return database_info_list;
}

void database_info_list_free(database_info_list_t *database_info_list)
{
   size_t i;

   if (!database_info_list)
      return;

   for (i = 0; i < database_info_list->count; i++)
   {
      database_info_t *info = &database_info_list->list[i];

      if (!info)
         continue;

      if (info->name)
         free(info->name);
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
      if (info->edge_magazine_review)
         free(info->edge_magazine_review);

      if (info->cero_rating)
         free(info->cero_rating);
      if (info->pegi_rating)
         free(info->pegi_rating);
      if (info->enhancement_hw)
         free(info->enhancement_hw);
      if (info->elspa_rating)
         free(info->elspa_rating);
      if (info->esrb_rating)
         free(info->esrb_rating);
      if (info->bbfc_rating)
         free(info->bbfc_rating);
      if (info->crc32)
         free(info->crc32);
      if (info->sha1)
         free(info->sha1);
      if (info->md5)
         free(info->md5);
   }

   free(database_info_list->list);
   free(database_info_list);
}

void database_playlist_free(content_playlist_t *db_playlist)
{
   if (db_playlist)
      content_playlist_free(db_playlist);
}

bool database_playlist_realloc(
      content_playlist_t *db_playlist,
      const char *path)
{
   database_playlist_free(db_playlist);

   db_playlist = content_playlist_init(path, 1000);

   if (!db_playlist)
      return false;

   return true;
}
