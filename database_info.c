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

#include <stdint.h>

static INLINE uint32_t djb2(const char *str)
{
   const unsigned char *aux = (const unsigned char*)str;
   uint32_t            hash = 5381;

   while ( *aux )
      hash = ( hash << 5 ) + hash + *aux++;

   return hash;
}

int database_info_build_query(char *s, size_t len,
      const char *label, const char *path)
{
   uint32_t value  = 0;
   bool add_quotes = true;

   strlcpy(s, "{'", len);

   value = djb2(label);

   switch (value)
   {
      case 0x1c310956U: /* displaylist_parse_database_entry */
         strlcat(s, "name", len);
         break;
      case 0x125e594dU: /* deferred_cursor_manager_list_rdb_entry_publisher */
         strlcat(s, "publisher", len);
         break;
      case 0xcbd89be5U: /* deferred_cursor_manager_list_rdb_entry_developer */
         strlcat(s, "developer", len);
         break;
      case 0x4ebaa767U: /* deferred_cursor_manager_list_rdb_entry_origin */
         strlcat(s, "origin", len);
         break;
      case 0x77f9eff2U: /* deferred_cursor_manager_list_rdb_entry_franchise */
         strlcat(s, "franchise", len);
         break;
      case 0x68eba20fU: /* deferred_cursor_manager_list_rdb_entry_esrb_rating */
         strlcat(s, "esrb_rating", len);
         break;
      case 0x0a8e67f0U: /* deferred_cursor_manager_list_rdb_entry_bbfc_rating */
         strlcat(s, "bbfc_rating", len);
         break;
      case 0x8bf6ab18U: /* deferred_cursor_manager_list_rdb_entry_elspa_rating */
         strlcat(s, "elspa_rating", len);
         break;
      case 0x5fc77328U: /* deferred_cursor_manager_list_rdb_entry_pegi_rating */
         strlcat(s, "pegi_rating", len);
         break;
      case 0x9866bda3U: /* deferred_cursor_manager_list_rdb_entry_enhancement_hw */
         strlcat(s, "enhancement_hw", len);
         break;
      case 0x24f6172cU: /* deferred_cursor_manager_list_rdb_entry_cero_rating */
         strlcat(s, "cero_rating", len);
         break;
      case 0x1c7f8a43U: /* deferred_cursor_manager_list_rdb_entry_edge_magazine_rating */
         strlcat(s, "edge_rating", len);
         add_quotes = false;
         break;
      case 0xaaeebde7U: /* deferred_cursor_manager_list_rdb_entry_edge_magazine_issue */
         strlcat(s, "edge_issue", len);
         add_quotes = false;
         break;
      case 0xbf7ff5e7U: /* deferred_cursor_manager_list_rdb_entry_famitsu_magazine_rating */
         strlcat(s, "famitsu_rating", len);
         add_quotes = false;
         break;
      case 0x2b36ce66U: /* deferred_cursor_manager_list_rdb_entry_releasemonth */
         strlcat(s, "releasemonth", len);
         add_quotes = false;
         break;
      case 0x9c7c6e91U: /* deferred_cursor_manager_list_rdb_entry_releaseyear */
         strlcat(s, "releaseyear", len);
         add_quotes = false;
         break;
      case 0xbfcba816U: /* deferred_cursor_manager_list_rdb_entry_max_users */
         strlcat(s, "users", len);
         add_quotes = false;
         break;
      default:
         RARCH_LOG("Unknown value: %d\n", value);
         break;
   }

   strlcat(s, "':", len);
   if (add_quotes)
      strlcat(s, "\"", len);
   strlcat(s, path, len);
   if (add_quotes)
      strlcat(s, "\"", len);
   strlcat(s, "}", len);

#if 0
   RARCH_LOG("query: %s\n", s);
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

static int database_cursor_iterate(libretrodb_cursor_t *cur,
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
      uint32_t                 value = 0;
      struct rmsgpack_dom_value *key = &item.map.items[i].key;
      struct rmsgpack_dom_value *val = &item.map.items[i].value;

      if (!key || !val)
         continue;

      value = djb2(key->string.buff);

      switch (value)
      {
         case 0x7c9b0c46U: /* name */
            db_info->name = strdup(val->string.buff);
            break;
         case 0x91b0c789U: /* description */
            db_info->description = strdup(val->string.buff);
            break;
         case 0x5e099013U: /* publisher */
            db_info->publisher = strdup(val->string.buff);
            break;
         case 0x1783d2abU: /* developer */
            db_info->developer = strdup(val->string.buff);
            break;
         case 0x1315e3edU: /* origin */
            db_info->origin = strdup(val->string.buff);
            break;
         case 0xc3a526b8U: /* franchise */
            db_info->franchise = strdup(val->string.buff);
            break;
         case 0xede26836U: /* bbfc_rating */
            db_info->bbfc_rating = strdup(val->string.buff);
            break;
         case 0x4c3fa255U: /* esrb_rating */
            db_info->esrb_rating = strdup(val->string.buff);
            break;
         case 0xd9cab41eU: /* elspa_rating */
            db_info->elspa_rating = strdup(val->string.buff);
            break;
         case 0x084a1772U: /* cero_rating */
            db_info->cero_rating = strdup(val->string.buff);
            break;
         case 0x431b736eU: /* pegi_rating */
            db_info->pegi_rating = strdup(val->string.buff);
            break;
         case 0xab612029U: /* enhancement_hw */
            db_info->enhancement_hw = strdup(val->string.buff);
            break;
         case 0xd3573eabU: /* edge_magazine_review */
            db_info->edge_magazine_review = strdup(val->string.buff);
            break;
         case 0xd30dc4feU: /* edge_magazine_rating */
            db_info->edge_magazine_rating = val->uint_;
            break;
         case 0xa0f30d42U: /* edge_magazine_issue */
            db_info->edge_magazine_issue = val->uint_;
            break;
         case 0x0a50ca62U: /* famitsu_magazine_rating */
            db_info->famitsu_magazine_rating = val->uint_;
            break;
         case 0x1084ff77U: /* max_users */
            db_info->max_users = val->uint_;
            break;
         case 0x790ad76cU: /* releasemonth */
            db_info->releasemonth = val->uint_;
            break;
         case 0x7fd06ed7U: /* releaseyear */
            db_info->releaseyear = val->uint_;
            break;
         case 0x1a4dc3ecU: /* rumble_supported */
            db_info->rumble_supported = val->uint_;
            break;
         case 0xf220fc17U: /* analog_supported */
            db_info->analog_supported = val->uint_;
            break;
         case 0x0b88671dU: /* crc32 */
            db_info->crc32 = bin_to_hex_alloc((uint8_t*)val->binary.buff, val->binary.len);
            break;
         case 0x7c9de632U: /* sha1 */
            db_info->sha1 = bin_to_hex_alloc((uint8_t*)val->binary.buff, val->binary.len);
            break;
         case 0x0b888fabU: /* md5 */
            db_info->md5 = bin_to_hex_alloc((uint8_t*)val->binary.buff, val->binary.len);
            break;
         default:
            RARCH_LOG("Unknown value: %d\n", value);
            break;
      }
   }

   rmsgpack_dom_value_free(&item);

   return 0;
}

static int database_cursor_open(libretrodb_t *db,
      libretrodb_cursor_t *cur, const char *path, const char *query)
{
   const char *error     = NULL;
   libretrodb_query_t *q = NULL;

   if ((libretrodb_open(path, db)) != 0)
      return -1;

   if (query) 
      q = (libretrodb_query_t*)libretrodb_query_compile(db, query,
      strlen(query), &error);
    
   if (error)
      goto error;
   if ((libretrodb_cursor_open(db, cur, q)) != 0)
      goto error;

   if (q)
      libretrodb_query_free(q);

   return 0;

error:
   if (q)
      libretrodb_query_free(q);
   query = NULL;
   libretrodb_close(db);
   db    = NULL;

   return -1;
}

static int database_cursor_close(libretrodb_t *db, libretrodb_cursor_t *cur)
{
   libretrodb_cursor_close(cur);
   libretrodb_close(db);

   return 0;
}

database_info_handle_t *database_info_init(const char *dir,
      enum database_type type)
{
   database_info_handle_t     *db  = (database_info_handle_t*)
      calloc(1, sizeof(*db));

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


database_info_list_t *database_info_list_new(
      const char *rdb_path, const char *query)
{
   libretrodb_t db;
   libretrodb_cursor_t cur;
   int ret                                  = 0;
   unsigned k                               = 0;
   database_info_t *database_info           = NULL;
   database_info_list_t *database_info_list = NULL;

   if ((database_cursor_open(&db, &cur, rdb_path, query) != 0))
      return NULL;

   database_info_list = (database_info_list_t*)
      calloc(1, sizeof(*database_info_list));

   if (!database_info_list)
      goto end;

   while (ret != -1)
   {
      database_info_t db_info = {0};
      ret = database_cursor_iterate(&cur, &db_info);

      if (ret == 0)
      {
         database_info_t *db_ptr = NULL;
         database_info = (database_info_t*)
            realloc(database_info, (k+1) * sizeof(database_info_t));

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
