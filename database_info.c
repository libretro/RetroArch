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
#include "hash.h"
#include "file_ops.h"
#include "file_extract.h"
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

#ifdef HAVE_ZLIB
static bool zlib_compare_crc32(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   RARCH_LOG("CRC32: 0x%x\n", crc32);

   return true;
}
#endif

int database_info_write_rdl(const char *dir)
{
   size_t i;
   const char *exts = "";
   struct string_list *str_list = NULL;

   if (g_extern.core_info)
      exts = core_info_list_get_all_extensions(g_extern.core_info);
   
   str_list = dir_list_new(dir, exts, false);

   if (!str_list)
      return -1;

   for (i = 0; i < str_list->size; i++)
   {
      char parent_dir[PATH_MAX_LENGTH];
      const char *name = str_list->elems[i].data;
      if (!name)
         continue;

      path_parent_dir(parent_dir);

#ifdef HAVE_ZLIB
      if (!strcmp(path_get_extension(name), "zip"))
      {
         RARCH_LOG("[ZIP]: name: %s\n", name);

         if (!zlib_parse_file(name, NULL, zlib_compare_crc32,
                  (void*)parent_dir))
            RARCH_LOG("Could not process ZIP file.\n");
      }
      else
#endif
      {
         ssize_t ret;
         uint32_t crc, target_crc = 0;
         uint8_t *ret_buf = NULL;

         (void)target_crc;

         RARCH_LOG("name: %s\n", name);
         ret = read_file(name, (void**)&ret_buf);

         if (ret <= 0)
            continue;

         crc = crc32_calculate(ret_buf, ret);

         RARCH_LOG("CRC32: 0x%x .\n", (unsigned)crc);

         if (ret_buf)
            free(ret_buf);
      }
   }

   string_list_free(str_list);

   return 0;
}

database_info_list_t *database_info_list_new(const char *rdb_path, const char *query)
{
   libretrodb_t db;
   libretrodb_cursor_t cur;
   struct rmsgpack_dom_value item;
   size_t j;
   unsigned k = 0;
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

      database_info = (database_info_t*)realloc(database_info, (k+1) * sizeof(database_info_t));

      if (!database_info)
         goto error;

      db_info = &database_info[k];

      if (!db_info)
         continue;

      db_info->name                   = NULL;
      db_info->description            = NULL;
      db_info->publisher              = NULL;
      db_info->developer              = NULL;
      db_info->origin                 = NULL;
      db_info->franchise              = NULL;
      db_info->bbfc_rating            = NULL;
      db_info->elspa_rating           = NULL;
      db_info->esrb_rating            = NULL;
      db_info->pegi_rating            = NULL;
      db_info->cero_rating            = NULL;
      db_info->edge_magazine_review   = NULL;
      db_info->enhancement_hw         = NULL;
      db_info->crc32                  = NULL;
      db_info->sha1                   = NULL;
      db_info->md5                    = NULL;
      db_info->famitsu_magazine_rating= 0;
      db_info->edge_magazine_rating   = 0;
      db_info->edge_magazine_issue    = 0;
      db_info->max_users              = 0;
      db_info->releasemonth           = 0;
      db_info->releaseyear            = 0;
      db_info->analog_supported       = -1;
      db_info->rumble_supported       = -1;

      for (j = 0; j < item.map.len; j++)
      {
         struct rmsgpack_dom_value *key = &item.map.items[j].key;
         struct rmsgpack_dom_value *val = &item.map.items[j].value;

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
         {
            size_t i;
            char crc32[PATH_MAX_LENGTH];

            for (i = 0; i < val->binary.len; i++)
            {
               snprintf(crc32+i*2, sizeof(crc32)-i*2, "%02X", (unsigned char)val->binary.buff[i]);
            }
            db_info->crc32 = strdup(crc32);
         }

         if (!strcmp(key->string.buff, "sha1"))
         {
            size_t i;
            char sha1[PATH_MAX_LENGTH];

            for (i = 0; i < val->binary.len; i++)
            {
               snprintf(sha1+i*2, sizeof(sha1)-i*2, "%02X", (unsigned char)val->binary.buff[i]);
            }
            db_info->sha1 = strdup(sha1);
         }

         if (!strcmp(key->string.buff, "md5"))
         {
            size_t i;
            char md5[PATH_MAX_LENGTH];

            for (i = 0; i < val->binary.len; i++)
            {
               snprintf(md5+i*2, sizeof(md5)-i*2, "%02X", (unsigned char)val->binary.buff[i]);
            }
            db_info->md5 = strdup(md5);
         }
      }
      k++;
   }

   database_info_list->list  = database_info;
   database_info_list->count = k;

   return database_info_list;

error:
   libretrodb_cursor_close(&cur);
   libretrodb_close(&db);
   database_info_list_free(database_info_list);
   return NULL;
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
