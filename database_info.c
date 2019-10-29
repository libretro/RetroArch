/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include <lists/string_list.h>
#include <lists/dir_list.h>
#include <string/stdstring.h>

#include "libretro-db/libretrodb.h"

#include "core_info.h"
#include "database_info.h"
#include "verbosity.h"

int database_info_build_query_enum(char *s, size_t len,
      enum database_query_type type,
      const char *path)
{
   bool add_quotes = true;
   bool add_glob   = false;

   strlcpy(s, "{'", len);

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
         add_glob   = true;
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

   strlcat(s, "':", len);
   if (add_glob)
      strlcat(s, "glob('*", len);
   if (add_quotes)
      strlcat(s, "\"", len);
   strlcat(s, path, len);
   if (add_glob)
      strlcat(s, "*')", len);
   if (add_quotes)
      strlcat(s, "\"", len);

   strlcat(s, "}", len);

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
      struct rmsgpack_dom_value *key = &item.val.map.items[i].key;
      struct rmsgpack_dom_value *val = &item.val.map.items[i].value;
      const char *val_string         = NULL;

      if (!key || !val)
         continue;

      val_string                     = val->val.string.buff;
      str                            = key->val.string.buff;

      if (string_is_equal(str, "publisher"))
      {
         if (!string_is_empty(val_string))
            db_info->publisher = strdup(val_string);
      }
      else if (string_is_equal(str, "developer"))
      {
         if (!string_is_empty(val_string))
            db_info->developer = string_split(val_string, "|");
      }
      else if (string_is_equal(str, "serial"))
      {
         if (!string_is_empty(val_string))
            db_info->serial = strdup(val_string);
      }
      else if (string_is_equal(str, "rom_name"))
      {
         if (!string_is_empty(val_string))
            db_info->rom_name = strdup(val_string);
      }
      else if (string_is_equal(str, "name"))
      {
         if (!string_is_empty(val_string))
            db_info->name = strdup(val_string);
      }
      else if (string_is_equal(str, "description"))
      {
         if (!string_is_empty(val_string))
            db_info->description = strdup(val_string);
      }
      else if (string_is_equal(str, "genre"))
      {
         if (!string_is_empty(val_string))
            db_info->genre = strdup(val_string);
      }
      else if (string_is_equal(str, "origin"))
      {
         if (!string_is_empty(val_string))
            db_info->origin = strdup(val_string);
      }
      else if (string_is_equal(str, "franchise"))
      {
         if (!string_is_empty(val_string))
            db_info->franchise = strdup(val_string);
      }
      else if (string_is_equal(str, "bbfc_rating"))
      {
         if (!string_is_empty(val_string))
            db_info->bbfc_rating = strdup(val_string);
      }
      else if (string_is_equal(str, "esrb_rating"))
      {
         if (!string_is_empty(val_string))
            db_info->esrb_rating = strdup(val_string);
      }
      else if (string_is_equal(str, "elspa_rating"))
      {
         if (!string_is_empty(val_string))
            db_info->elspa_rating = strdup(val_string);
      }
      else if (string_is_equal(str, "cero_rating"))
      {
         if (!string_is_empty(val_string))
            db_info->cero_rating          = strdup(val_string);
      }
      else if (string_is_equal(str, "pegi_rating"))
      {
         if (!string_is_empty(val_string))
            db_info->pegi_rating          = strdup(val_string);
      }
      else if (string_is_equal(str, "enhancement_hw"))
      {
         if (!string_is_empty(val_string))
            db_info->enhancement_hw       = strdup(val_string);
      }
      else if (string_is_equal(str, "edge_review"))
      {
         if (!string_is_empty(val_string))
            db_info->edge_magazine_review = strdup(val_string);
      }
      else if (string_is_equal(str, "edge_rating"))
         db_info->edge_magazine_rating    = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "edge_issue"))
         db_info->edge_magazine_issue     = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "famitsu_rating"))
         db_info->famitsu_magazine_rating = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "tgdb_rating"))
         db_info->tgdb_rating             = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "users"))
         db_info->max_users               = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "releasemonth"))
         db_info->releasemonth            = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "releaseyear"))
         db_info->releaseyear             = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "rumble"))
         db_info->rumble_supported        = (int)val->val.uint_;
      else if (string_is_equal(str, "coop"))
         db_info->coop_supported          = (int)val->val.uint_;
      else if (string_is_equal(str, "analog"))
         db_info->analog_supported        = (int)val->val.uint_;
      else if (string_is_equal(str, "size"))
         db_info->size                    = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "crc"))
         db_info->crc32 = swap_if_little32(
               *(uint32_t*)val->val.binary.buff);
      else if (string_is_equal(str, "sha1"))
         db_info->sha1 = bin_to_hex_alloc(
               (uint8_t*)val->val.binary.buff, val->val.binary.len);
      else if (string_is_equal(str, "md5"))
         db_info->md5 = bin_to_hex_alloc(
               (uint8_t*)val->val.binary.buff, val->val.binary.len);
      else
      {
         RARCH_LOG("Unknown key: %s\n", str);
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

static bool type_is_prioritized(const char *path)
{
   const char *ext = path_get_extension(path);
   if (string_is_equal_noncase(ext, "cue"))
      return true;
   if (string_is_equal_noncase(ext, "gdi"))
      return true;
   return false;
}

static int dir_entry_compare(const void *left, const void *right)
{
   const struct string_list_elem *le = (const struct string_list_elem*)left;
   const struct string_list_elem *re = (const struct string_list_elem*)right;
   bool                            l = type_is_prioritized(le->data);
   bool                            r = type_is_prioritized(re->data);

   return (int) r - (int) l;
}

static void dir_list_prioritize(struct string_list *list)
{
   qsort(list->elems, list->size, sizeof(*list->elems), dir_entry_compare);
}

database_info_handle_t *database_info_dir_init(const char *dir,
      enum database_type type, retro_task_t *task,
      bool show_hidden_files)
{
   core_info_list_t *core_info_list = NULL;
   struct string_list       *list   = NULL;
   database_info_handle_t     *db   = (database_info_handle_t*)
      calloc(1, sizeof(*db));

   if (!db)
      return NULL;

   core_info_get_list(&core_info_list);

   list = dir_list_new(dir, core_info_list ? core_info_list->all_ext : NULL,
         false, show_hidden_files,
         false, true);

   if (!list)
   {
      free(db);
      return NULL;
   }

   dir_list_prioritize(list);

   db->list           = list;
   db->list_ptr       = 0;
   db->status         = DATABASE_STATUS_ITERATE;
   db->type           = type;

   return db;
}

database_info_handle_t *database_info_file_init(const char *path,
      enum database_type type, retro_task_t *task)
{
   union string_list_elem_attr attr;
   struct string_list        *list  = NULL;
   database_info_handle_t      *db  = (database_info_handle_t*)
      calloc(1, sizeof(*db));

   if (!db)
      return NULL;

   attr.i             = 0;

   list               = string_list_new();

   if (!list)
   {
      free(db);
      return NULL;
   }

   string_list_append(list, path, attr);

   db->list_ptr       = 0;
   db->list           = list;
   db->status         = DATABASE_STATUS_ITERATE;
   db->type           = type;

   return db;
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
      malloc(sizeof(*database_info_list));

   if (!database_info_list)
      goto end;

   database_info_list->count  = 0;
   database_info_list->list   = NULL;

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
            if (db_info.bbfc_rating)
               free(db_info.bbfc_rating);
            if (db_info.cero_rating)
               free(db_info.cero_rating);
            if (db_info.description)
               free(db_info.description);
            if (db_info.edge_magazine_review)
               free(db_info.edge_magazine_review);
            if (db_info.elspa_rating)
               free(db_info.elspa_rating);
            if (db_info.enhancement_hw)
               free(db_info.enhancement_hw);
            if (db_info.esrb_rating)
               free(db_info.esrb_rating);
            if (db_info.franchise)
               free(db_info.franchise);
            if (db_info.genre)
               free(db_info.genre);
            if (db_info.name)
               free(db_info.name);
            if (db_info.origin)
               free(db_info.origin);
            if (db_info.pegi_rating)
               free(db_info.pegi_rating);
            if (db_info.publisher)
               free(db_info.publisher);
            if (db_info.rom_name)
               free(db_info.rom_name);
            if (db_info.serial)
               free(db_info.serial);
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
