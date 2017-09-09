/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <stdio.h>
#include <stdint.h>

#include <compat/strl.h>
#include <retro_endianness.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "libretro-db/libretrodb.h"

#include "list_special.h"
#include "database_info.h"
#include "msg_hash.h"
#include "verbosity.h"

#define DB_CURSOR_ROM_NAME                      0x16bbcf13U
#define DB_CURSOR_NAME                          0x7c9b0c46U
#define DB_CURSOR_DESCRIPTION                   0x91b0c789U
#define DB_CURSOR_GENRE                         0x0f802156U
#define DB_CURSOR_PUBLISHER                     0x5e099013U
#define DB_CURSOR_DEVELOPER                     0x1783d2abU
#define DB_CURSOR_ORIGIN                        0x1315e3edU
#define DB_CURSOR_FRANCHISE                     0xc3a526b8U
#define DB_CURSOR_BBFC_RATING                   0xede26836U
#define DB_CURSOR_ESRB_RATING                   0x4c3fa255U
#define DB_CURSOR_ELSPA_RATING                  0xd9cab41eU
#define DB_CURSOR_CERO_RATING                   0x084a1772U
#define DB_CURSOR_PEGI_RATING                   0x431b736eU
#define DB_CURSOR_CHECKSUM_CRC32                0x0b88671dU
#define DB_CURSOR_CHECKSUM_SHA1                 0x7c9de632U
#define DB_CURSOR_CHECKSUM_MD5                  0x0b888fabU
#define DB_CURSOR_ENHANCEMENT_HW                0xab612029U
#define DB_CURSOR_EDGE_MAGAZINE_REVIEW          0xd3573eabU
#define DB_CURSOR_EDGE_MAGAZINE_RATING          0xd30dc4feU
#define DB_CURSOR_EDGE_MAGAZINE_ISSUE           0xa0f30d42U
#define DB_CURSOR_FAMITSU_MAGAZINE_RATING       0x0a50ca62U
#define DB_CURSOR_TGDB_RATING                   0x8d61458aU
#define DB_CURSOR_MAX_USERS                     0x1084ff77U
#define DB_CURSOR_RELEASEDATE_MONTH             0x790ad76cU
#define DB_CURSOR_RELEASEDATE_YEAR              0x7fd06ed7U
#define DB_CURSOR_RUMBLE_SUPPORTED              0x1a4dc3ecU
#define DB_CURSOR_COOP_SUPPORTED                0x7c953ff6U
#define DB_CURSOR_ANALOG_SUPPORTED              0xf220fc17U
#define DB_CURSOR_SIZE                          0x7c9dede0U
#define DB_CURSOR_SERIAL                        0x1b843ec5U

static void database_info_build_query_add_quote(char *s, size_t len)
{
   strlcat(s, "\"", len);
}

static void database_info_build_query_add_bracket_open(char *s, size_t len)
{
   strlcat(s, "{'", len);
}

static void database_info_build_query_add_bracket_close(char *s, size_t len)
{
   strlcat(s, "}", len);
}

static void database_info_build_query_add_colon(char *s, size_t len)
{
   strlcat(s, "':", len);
}

static void database_info_build_query_add_glob_open(char *s, size_t len)
{
   strlcat(s, "glob('*", len);
}

static void database_info_build_query_add_glob_close(char *s, size_t len)
{
   strlcat(s, "*')", len);
}

int database_info_build_query_enum(char *s, size_t len,
      enum database_query_type type,
      const char *path)
{
   bool add_quotes = true;
   bool add_glob   = false;

   database_info_build_query_add_bracket_open(s, len);

   switch (type)
   {
      case DATABASE_QUERY_ENTRY:
         strlcat(s, "name", len);
         break;
      case DATABASE_QUERY_ENTRY_PUBLISHER:
         strlcat(s, "publisher", len);
         break;
      case DATABASE_QUERY_ENTRY_DEVELOPER:
         strlcat(s, "developer", len);
         add_glob = true;
         add_quotes = false;
         break;
      case DATABASE_QUERY_ENTRY_ORIGIN:
         strlcat(s, "origin", len);
         break;
      case DATABASE_QUERY_ENTRY_FRANCHISE:
         strlcat(s, "franchise", len);
         break;
      case DATABASE_QUERY_ENTRY_RATING:
         strlcat(s, "esrb_rating", len);
         break;
      case DATABASE_QUERY_ENTRY_BBFC_RATING:
         strlcat(s, "bbfc_rating", len);
         break;
      case DATABASE_QUERY_ENTRY_ELSPA_RATING:
         strlcat(s, "elspa_rating", len);
         break;
      case DATABASE_QUERY_ENTRY_ESRB_RATING:
         strlcat(s, "esrb_rating", len);
         break;
      case DATABASE_QUERY_ENTRY_PEGI_RATING:
         strlcat(s, "pegi_rating", len);
         break;
      case DATABASE_QUERY_ENTRY_CERO_RATING:
         strlcat(s, "cero_rating", len);
         break;
      case DATABASE_QUERY_ENTRY_ENHANCEMENT_HW:
         strlcat(s, "enhancement_hw", len);
         break;
      case DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_RATING:
         strlcat(s, "edge_rating", len);
         add_quotes = false;
         break;
      case DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_ISSUE:
         strlcat(s, "edge_issue", len);
         add_quotes = false;
         break;
      case DATABASE_QUERY_ENTRY_FAMITSU_MAGAZINE_RATING:
         strlcat(s, "famitsu_rating", len);
         add_quotes = false;
         break;
      case DATABASE_QUERY_ENTRY_RELEASEDATE_MONTH:
         strlcat(s, "releasemonth", len);
         add_quotes = false;
         break;
      case DATABASE_QUERY_ENTRY_RELEASEDATE_YEAR:
         strlcat(s, "releaseyear", len);
         add_quotes = false;
         break;
      case DATABASE_QUERY_ENTRY_MAX_USERS:
         strlcat(s, "users", len);
         add_quotes = false;
         break;
      case DATABASE_QUERY_NONE:
         RARCH_LOG("Unknown type: %d\n", type);
         break;
   }

   database_info_build_query_add_colon(s, len);
   if (add_glob)
      database_info_build_query_add_glob_open(s, len);
   if (add_quotes)
      database_info_build_query_add_quote(s, len);
   strlcat(s, path, len);
   if (add_glob)
      database_info_build_query_add_glob_close(s, len);
   if (add_quotes)
      database_info_build_query_add_quote(s, len);
   database_info_build_query_add_bracket_close(s, len);

#if 0
   RARCH_LOG("query: %s\n", s);
#endif

   return 0;
}

/*
 * NOTE: Allocates memory, it is the caller's responsibility to free the
 * memory after it is no longer required.
 */
char *bin_to_hex_alloc(const uint8_t *data, size_t len)
{
   size_t i;
   char *ret = (char*)malloc(len * 2 + 1);

   if (len && !ret)
      return NULL;

   for (i = 0; i < len; i++)
      snprintf(ret+i * 2, 3, "%02X", data[i]);
   return ret;
}


static int database_cursor_iterate(libretrodb_cursor_t *cur,
      database_info_t *db_info)
{
   unsigned i;
   struct rmsgpack_dom_value item;
   const char* str                = NULL;

   if (libretrodb_cursor_read_item(cur, &item) != 0)
      return -1;

   if (item.type != RDT_MAP)
   {
      rmsgpack_dom_value_free(&item);
      return 1;
   }

   db_info->analog_supported       = -1;
   db_info->rumble_supported       = -1;
   db_info->coop_supported         = -1;

   for (i = 0; i < item.val.map.len; i++)
   {
      uint32_t                 value = 0;
      struct rmsgpack_dom_value *key = &item.val.map.items[i].key;
      struct rmsgpack_dom_value *val = &item.val.map.items[i].value;
      const char *val_string         = NULL;

      if (!key || !val)
         continue;

      val_string = val->val.string.buff;
      str        = key->val.string.buff;
      value      = msg_hash_calculate(str);

      switch (value)
      {
         case DB_CURSOR_SERIAL:
            if (!string_is_empty(val_string))
               db_info->serial = strdup(val_string);
            break;
         case DB_CURSOR_ROM_NAME:
            if (!string_is_empty(val_string))
               db_info->rom_name = strdup(val_string);
            break;
         case DB_CURSOR_NAME:
            if (!string_is_empty(val_string))
               db_info->name = strdup(val_string);
            break;
         case DB_CURSOR_DESCRIPTION:
            if (!string_is_empty(val_string))
               db_info->description = strdup(val_string);
            break;
         case DB_CURSOR_GENRE:
            if (!string_is_empty(val_string))
               db_info->genre = strdup(val_string);
            break;
         case DB_CURSOR_PUBLISHER:
            if (!string_is_empty(val_string))
               db_info->publisher = strdup(val_string);
            break;
         case DB_CURSOR_DEVELOPER:
            if (!string_is_empty(val_string))
               db_info->developer = string_split(val_string, "|");
            break;
         case DB_CURSOR_ORIGIN:
            if (!string_is_empty(val_string))
               db_info->origin = strdup(val_string);
            break;
         case DB_CURSOR_FRANCHISE:
            if (!string_is_empty(val_string))
               db_info->franchise = strdup(val_string);
            break;
         case DB_CURSOR_BBFC_RATING:
            if (!string_is_empty(val_string))
               db_info->bbfc_rating = strdup(val_string);
            break;
         case DB_CURSOR_ESRB_RATING:
            if (!string_is_empty(val_string))
               db_info->esrb_rating = strdup(val_string);
            break;
         case DB_CURSOR_ELSPA_RATING:
            if (!string_is_empty(val_string))
               db_info->elspa_rating = strdup(val_string);
            break;
         case DB_CURSOR_CERO_RATING:
            if (!string_is_empty(val_string))
               db_info->cero_rating          = strdup(val_string);
            break;
         case DB_CURSOR_PEGI_RATING:
            if (!string_is_empty(val_string))
               db_info->pegi_rating          = strdup(val_string);
            break;
         case DB_CURSOR_ENHANCEMENT_HW:
            if (!string_is_empty(val_string))
               db_info->enhancement_hw       = strdup(val_string);
            break;
         case DB_CURSOR_EDGE_MAGAZINE_REVIEW:
            if (!string_is_empty(val_string))
               db_info->edge_magazine_review = strdup(val_string);
            break;
         case DB_CURSOR_EDGE_MAGAZINE_RATING:
            db_info->edge_magazine_rating    = (unsigned)val->val.uint_;
            break;
         case DB_CURSOR_EDGE_MAGAZINE_ISSUE:
            db_info->edge_magazine_issue     = (unsigned)val->val.uint_;
            break;
         case DB_CURSOR_FAMITSU_MAGAZINE_RATING:
            db_info->famitsu_magazine_rating = (unsigned)val->val.uint_;
            break;
         case DB_CURSOR_TGDB_RATING:
            db_info->tgdb_rating             = (unsigned)val->val.uint_;
            break;
         case DB_CURSOR_MAX_USERS:
            db_info->max_users               = (unsigned)val->val.uint_;
            break;
         case DB_CURSOR_RELEASEDATE_MONTH:
            db_info->releasemonth            = (unsigned)val->val.uint_;
            break;
         case DB_CURSOR_RELEASEDATE_YEAR:
            db_info->releaseyear             = (unsigned)val->val.uint_;
            break;
         case DB_CURSOR_RUMBLE_SUPPORTED:
            db_info->rumble_supported        = (int)val->val.uint_;
            break;
         case DB_CURSOR_COOP_SUPPORTED:
            db_info->coop_supported          = (int)val->val.uint_;
            break;
         case DB_CURSOR_ANALOG_SUPPORTED:
            db_info->analog_supported        = (int)val->val.uint_;
            break;
         case DB_CURSOR_SIZE:
            db_info->size                    = (unsigned)val->val.uint_;
            break;
         case DB_CURSOR_CHECKSUM_CRC32:
            db_info->crc32 = swap_if_little32(*(uint32_t*)val->val.binary.buff);
            break;
         case DB_CURSOR_CHECKSUM_SHA1:
            db_info->sha1 = bin_to_hex_alloc((uint8_t*)val->val.binary.buff, val->val.binary.len);
            break;
         case DB_CURSOR_CHECKSUM_MD5:
            db_info->md5 = bin_to_hex_alloc((uint8_t*)val->val.binary.buff, val->val.binary.len);
            break;
         default:
            RARCH_LOG("Unknown key: %s\n", str);
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
   libretrodb_close(db);

   return -1;
}

static int database_cursor_close(libretrodb_t *db, libretrodb_cursor_t *cur)
{
   libretrodb_cursor_close(cur);
   libretrodb_close(db);

   return 0;
}

database_info_handle_t *database_info_dir_init(const char *dir,
      enum database_type type, retro_task_t *task)
{
   unsigned i;
   database_info_handle_t     *db  = (database_info_handle_t*)
      calloc(1, sizeof(*db));

   if (!db)
      return NULL;

   db->list           = dir_list_new_special(dir, DIR_LIST_RECURSIVE, NULL);

   if (!db->list)
      goto error;

   db->list_ptr       = 0;
   db->status         = DATABASE_STATUS_ITERATE;
   db->type           = type;

   if (db->list->size > 0)
   {
      for (i = 0; i < db->list->size; i++)
      {
         const char *path = db->list->elems[i].data;

         if (task)
            task_set_progress(task, (i / (float)db->list->size) * 100);

         if (path_is_compressed_file(path) && !path_contains_compressed_file(path))
         {
            struct string_list *archive_list = path_is_compressed_file(path) ?
                  file_archive_get_file_list(path, NULL) : NULL;

            if (archive_list && archive_list->size > 0)
            {
               unsigned i;

               for (i = 0; i < archive_list->size; i++)
               {
                  char *new_path   = (char*)malloc(
                        PATH_MAX_LENGTH * sizeof(char));
                  size_t path_size = PATH_MAX_LENGTH * sizeof(char);
                  size_t path_len  = strlen(path);

                  new_path[0] = '\0';

                  strlcpy(new_path, path, path_size);

                  if (path_len + strlen(archive_list->elems[i].data)
                         + 1 < PATH_MAX_LENGTH)
                  {
                     new_path[path_len] = '#';
                     strlcpy(new_path + path_len + 1,
                           archive_list->elems[i].data,
                           path_size - path_len);
                  }

                  string_list_append(db->list, new_path,
                        archive_list->elems[i].attr);

                  free(new_path);
               }

               string_list_free(archive_list);
            }
         }
      }
   }

   return db;

error:
   if (db)
      free(db);
   return NULL;
}

database_info_handle_t *database_info_file_init(const char *path,
      enum database_type type, retro_task_t *task)
{
   union string_list_elem_attr attr;
   database_info_handle_t      *db  = (database_info_handle_t*)
      calloc(1, sizeof(*db));

   if (!db)
      return NULL;

   attr.i             = 0;

   db->list           = string_list_new();

   if (!db->list)
      goto error;

   string_list_append(db->list, path, attr);

   if (path_is_compressed_file(path))
   {
      struct string_list *archive_list =path_is_compressed_file(path) ?
            file_archive_get_file_list(path, NULL) : NULL;

      if (archive_list && archive_list->size > 0)
      {
         unsigned i;

         for (i = 0; i < archive_list->size; i++)
         {
            char *new_path   = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
            size_t path_size = PATH_MAX_LENGTH * sizeof(char);
            size_t path_len  = strlen(path);

            if (task)
               task_set_progress(task,
                     (i / (float)archive_list->size) * 100);

            new_path[0] = '\0';

            strlcpy(new_path, path, path_size);

            if (path_len + strlen(archive_list->elems[i].data)
                   + 1 < PATH_MAX_LENGTH)
            {
               new_path[path_len] = '#';
               strlcpy(new_path + path_len + 1,
                     archive_list->elems[i].data,
                     path_size - path_len);
            }

            string_list_append(db->list, new_path,
                                  archive_list->elems[i].attr);
            free(new_path);
         }

         string_list_free(archive_list);
      }
   }

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
   int ret                                  = 0;
   unsigned k                               = 0;
   database_info_t *database_info           = NULL;
   database_info_list_t *database_info_list = NULL;
   libretrodb_t *db                         = libretrodb_new();
   libretrodb_cursor_t *cur                 = libretrodb_cursor_new();

   if (!db || !cur)
      goto end;

   if ((database_cursor_open(db, cur, rdb_path, query) != 0))
      goto end;

   database_info_list = (database_info_list_t*)
      calloc(1, sizeof(*database_info_list));

   if (!database_info_list)
      goto end;

   while (ret != -1)
   {
      database_info_t db_info = {0};
      ret = database_cursor_iterate(cur, &db_info);

      if (ret == 0)
      {
         database_info_t *db_ptr  = NULL;
         database_info_t *new_ptr = (database_info_t*)
            realloc(database_info, (k+1) * sizeof(database_info_t));

         if (!new_ptr)
         {
            database_info_list_free(database_info_list);
            free(database_info);
            free(database_info_list);
            database_info_list = NULL;
            goto end;
         }

         database_info = new_ptr;
         db_ptr        = &database_info[k];

         memcpy(db_ptr, &db_info, sizeof(*db_ptr));

         k++;
      }
   }

   database_info_list->list  = database_info;
   database_info_list->count = k;

end:
   if (db)
   {
      database_cursor_close(db, cur);
      libretrodb_free(db);
   }
   if (cur)
      libretrodb_cursor_free(cur);

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

      if (info->name)
         free(info->name);
      if (info->rom_name)
         free(info->rom_name);
      if (info->serial)
         free(info->serial);
      if (info->genre)
         free(info->genre);
      if (info->description)
         free(info->description);
      if (info->publisher)
         free(info->publisher);
      if (info->developer)
         string_list_free(info->developer);
      info->developer = NULL;
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
      if (info->sha1)
         free(info->sha1);
      if (info->md5)
         free(info->md5);
   }

   free(database_info_list->list);
}
