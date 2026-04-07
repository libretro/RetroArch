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
#include "manual_content_scan.h"

int database_info_build_query_enum(char *s, size_t len,
      enum database_query_type type,
      const char *path)
{
   size_t _len = 0;

   switch (type)
   {
      case DATABASE_QUERY_ENTRY:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'n';
         s[++_len]  = 'a';
         s[++_len]  = 'm';
         s[++_len]  = 'e';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_PUBLISHER:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'p';
         s[++_len]  = 'u';
         s[++_len]  = 'b';
         s[++_len]  = 'l';
         s[++_len]  = 'i';
         s[++_len]  = 's';
         s[++_len]  = 'h';
         s[++_len]  = 'e';
         s[++_len]  = 'r';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_DEVELOPER:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'd';
         s[++_len]  = 'e';
         s[++_len]  = 'v';
         s[++_len]  = 'e';
         s[++_len]  = 'l';
         s[++_len]  = 'o';
         s[++_len]  = 'p';
         s[++_len]  = 'e';
         s[++_len]  = 'r';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = 'g';
         s[++_len]  = 'l';
         s[++_len]  = 'o';
         s[++_len]  = 'b';
         s[++_len]  = '(';
         s[++_len]  = '\'';
         s[++_len]  = '*';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '*';
         s[++_len]  = '\'';
         s[++_len]  = ')';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_ORIGIN:
         s[  _len]  = '{';
	      s[++_len]  = '\'';
         s[++_len]  = 'o';
         s[++_len]  = 'r';
         s[++_len]  = 'i';
         s[++_len]  = 'g';
         s[++_len]  = 'i';
         s[++_len]  = 'n';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_FRANCHISE:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'f';
         s[++_len]  = 'r';
         s[++_len]  = 'a';
         s[++_len]  = 'n';
         s[++_len]  = 'c';
         s[++_len]  = 'h';
         s[++_len]  = 'i';
         s[++_len]  = 's';
         s[++_len]  = 'e';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_RATING:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'e';
         s[++_len]  = 's';
         s[++_len]  = 'r';
         s[++_len]  = 'b';
         s[++_len]  = '_';
         s[++_len]  = 'r';
         s[++_len]  = 'a';
         s[++_len]  = 't';
         s[++_len]  = 'i';
         s[++_len]  = 'n';
         s[++_len]  = 'g';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_BBFC_RATING:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'b';
         s[++_len]  = 'b';
         s[++_len]  = 'f';
         s[++_len]  = 'c';
         s[++_len]  = '_';
         s[++_len]  = 'r';
         s[++_len]  = 'a';
         s[++_len]  = 't';
         s[++_len]  = 'i';
         s[++_len]  = 'n';
         s[++_len]  = 'g';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[++_len]  = '"';
         s[++_len]  = '}';
         s[++_len] = '\0';
         break;
      case DATABASE_QUERY_ENTRY_ELSPA_RATING:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'e';
         s[++_len]  = 'l';
         s[++_len]  = 's';
         s[++_len]  = 'p';
         s[++_len]  = 'a';
         s[++_len]  = '_';
         s[++_len]  = 'r';
         s[++_len]  = 'a';
         s[++_len]  = 't';
         s[++_len]  = 'i';
         s[++_len]  = 'n';
         s[++_len]  = 'g';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_ESRB_RATING:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'e';
         s[++_len]  = 's';
         s[++_len]  = 'r';
         s[++_len]  = 'b';
         s[++_len]  = '_';
         s[++_len]  = 'r';
         s[++_len]  = 'a';
         s[++_len]  = 't';
         s[++_len]  = 'i';
         s[++_len]  = 'n';
         s[++_len]  = 'g';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_PEGI_RATING:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'p';
         s[++_len]  = 'e';
         s[++_len]  = 'g';
         s[++_len]  = 'i';
         s[++_len]  = '_';
         s[++_len]  = 'r';
         s[++_len]  = 'a';
         s[++_len]  = 't';
         s[++_len]  = 'i';
         s[++_len]  = 'n';
         s[++_len]  = 'g';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_CERO_RATING:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'c';
         s[++_len]  = 'e';
         s[++_len]  = 'r';
         s[++_len]  = 'o';
         s[++_len]  = '_';
         s[++_len]  = 'r';
         s[++_len]  = 'a';
         s[++_len]  = 't';
         s[++_len]  = 'i';
         s[++_len]  = 'n';
         s[++_len]  = 'g';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_ENHANCEMENT_HW:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'e';
         s[++_len]  = 'n';
         s[++_len]  = 'h';
         s[++_len]  = 'a';
         s[++_len]  = 'n';
         s[++_len]  = 'c';
         s[++_len]  = 'e';
         s[++_len]  = 'm';
         s[++_len]  = 'e';
         s[++_len]  = 'n';
         s[++_len]  = 't';
         s[++_len]  = '_';
         s[++_len]  = 'h';
         s[++_len]  = 'w';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_RATING:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'e';
         s[++_len]  = 'd';
         s[++_len]  = 'g';
         s[++_len]  = 'e';
         s[++_len]  = '_';
         s[++_len]  = 'r';
         s[++_len]  = 'a';
         s[++_len]  = 't';
         s[++_len]  = 'i';
         s[++_len]  = 'n';
         s[++_len]  = 'g';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]   = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_ISSUE:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'e';
         s[++_len]  = 'd';
         s[++_len]  = 'g';
         s[++_len]  = 'e';
         s[++_len]  = '_';
         s[++_len]  = 'i';
         s[++_len]  = 's';
         s[++_len]  = 's';
         s[++_len]  = 'u';
         s[++_len]  = 'e';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_FAMITSU_MAGAZINE_RATING:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'f';
         s[++_len]  = 'a';
         s[++_len]  = 'm';
         s[++_len]  = 'i';
         s[++_len]  = 't';
         s[++_len]  = 's';
         s[++_len]  = 'u';
         s[++_len]  = '_';
         s[++_len]  = 'r';
         s[++_len]  = 'a';
         s[++_len]  = 't';
         s[++_len]  = 'i';
         s[++_len]  = 'n';
         s[++_len]  = 'g';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_RELEASEDATE_MONTH:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'r';
         s[++_len]  = 'e';
         s[++_len]  = 'l';
         s[++_len]  = 'e';
         s[++_len]  = 'a';
         s[++_len]  = 's';
         s[++_len]  = 'e';
         s[++_len]  = 'm';
         s[++_len]  = 'o';
         s[++_len]  = 'n';
         s[++_len]  = 't';
         s[++_len]  = 'h';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_RELEASEDATE_YEAR:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'r';
         s[++_len]  = 'e';
         s[++_len]  = 'l';
         s[++_len]  = 'e';
         s[++_len]  = 'a';
         s[++_len]  = 's';
         s[++_len]  = 'e';
         s[++_len]  = 'y';
         s[++_len]  = 'e';
         s[++_len]  = 'a';
         s[++_len]  = 'r';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_MAX_USERS:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'u';
         s[++_len]  = 's';
         s[++_len]  = 'e';
         s[++_len]  = 'r';
         s[++_len]  = 's';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_GENRE:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'g';
         s[++_len]  = 'e';
         s[++_len]  = 'n';
         s[++_len]  = 'r';
         s[++_len]  = 'e';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_ENTRY_REGION:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = 'r';
         s[++_len]  = 'e';
         s[++_len]  = 'g';
         s[++_len]  = 'i';
         s[++_len]  = 'o';
         s[++_len]  = 'n';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
      case DATABASE_QUERY_NONE:
         s[  _len]  = '{';
         s[++_len]  = '\'';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '\'';
         s[++_len]  = ':';
         s[++_len]  = '"';
         s[++_len]  = '\0';
         _len      += strlcpy(s + _len, path, len - _len);
         s[  _len]  = '"';
         s[++_len]  = '}';
         s[++_len]  = '\0';
         break;
   }

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

   ret[0] = '\0';
   for (i = 0; i < len; i++)
      snprintf(ret+i * 2, 3, "%02X", data[i]);
   return ret;
}

static int database_cursor_iterate(libretrodb_cursor_t *cur,
      database_info_t *db_info)
{
   size_t i;
   struct rmsgpack_dom_value item;

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
      const char *str;
      const char *val_string;
      size_t      str_len;

      if (!key || !val)
         continue;

      if (key->type != RDT_STRING)
         continue;

      str     = key->val.string.buff;
      str_len = strlen(str);

      val_string = val->val.string.buff;

      switch (str_len)
      {
         case 3:
            if (memcmp(str, "crc", 3) == 0)
            {
               if (val->type == RDT_BINARY)
               {
                  switch (val->val.binary.len)
                  {
                     case 1:
                        db_info->crc32 = *(uint8_t*)val->val.binary.buff;
                        break;
                     case 2:
                        db_info->crc32 = swap_if_little16(
                              *(uint16_t*)val->val.binary.buff);
                        break;
                     case 4:
                        db_info->crc32 = swap_if_little32(
                              *(uint32_t*)val->val.binary.buff);
                        break;
                     default:
                        db_info->crc32 = 0;
                        break;
                  }
               }
            }
            else if (memcmp(str, "md5", 3) == 0)
            {
               if (val->type == RDT_BINARY)
                  db_info->md5 = bin_to_hex_alloc(
                        (uint8_t*)val->val.binary.buff,
                        val->val.binary.len);
            }
            break;

         case 4:
            if (memcmp(str, "name", 4) == 0)
            {
               if (val_string && *val_string)
                  db_info->name = strdup(val_string);
            }
            else if (memcmp(str, "size", 4) == 0)
               db_info->size = (uint64_t)val->val.uint_;
            else if (memcmp(str, "coop", 4) == 0)
               db_info->coop_supported = (int)val->val.uint_;
            else if (memcmp(str, "sha1", 4) == 0)
            {
               if (val->type == RDT_BINARY)
                  db_info->sha1 = bin_to_hex_alloc(
                        (uint8_t*)val->val.binary.buff,
                        val->val.binary.len);
            }
            break;

         case 5:
            if (memcmp(str, "genre", 5) == 0)
            {
               if (val_string && *val_string)
                  db_info->genre = strdup(val_string);
            }
            else if (memcmp(str, "score", 5) == 0)
            {
               if (val_string && *val_string)
                  db_info->score = strdup(val_string);
            }
            else if (memcmp(str, "media", 5) == 0)
            {
               if (val_string && *val_string)
                  db_info->media = strdup(val_string);
            }
            else if (memcmp(str, "users", 5) == 0)
               db_info->max_users = (unsigned)val->val.uint_;
            break;

         case 6:
            if (memcmp(str, "serial", 6) == 0)
            {
               if (val_string && *val_string)
                  db_info->serial = strdup(val_string);
            }
            else if (memcmp(str, "region", 6) == 0)
            {
               if (val_string && *val_string)
                  db_info->region = strdup(val_string);
            }
            else if (memcmp(str, "pacing", 6) == 0)
            {
               if (val_string && *val_string)
                  db_info->pacing = strdup(val_string);
            }
            else if (memcmp(str, "visual", 6) == 0)
            {
               if (val_string && *val_string)
                  db_info->visual = strdup(val_string);
            }
            else if (memcmp(str, "origin", 6) == 0)
            {
               if (val_string && *val_string)
                  db_info->origin = strdup(val_string);
            }
            else if (memcmp(str, "rumble", 6) == 0)
               db_info->rumble_supported = (int)val->val.uint_;
            else if (memcmp(str, "analog", 6) == 0)
               db_info->analog_supported = (int)val->val.uint_;
            break;

         case 7:
            if (memcmp(str, "setting", 7) == 0)
            {
               if (val_string && *val_string)
                  db_info->setting = strdup(val_string);
            }
            break;

         case 8:
            if (memcmp(str, "category", 8) == 0)
            {
               if (val_string && *val_string)
                  db_info->category = strdup(val_string);
            }
            else if (memcmp(str, "language", 8) == 0)
            {
               if (val_string && *val_string)
                  db_info->language = strdup(val_string);
            }
            else if (memcmp(str, "controls", 8) == 0)
            {
               if (val_string && *val_string)
                  db_info->controls = strdup(val_string);
            }
            else if (memcmp(str, "artstyle", 8) == 0)
            {
               if (val_string && *val_string)
                  db_info->artstyle = strdup(val_string);
            }
            else if (memcmp(str, "gameplay", 8) == 0)
            {
               if (val_string && *val_string)
                  db_info->gameplay = strdup(val_string);
            }
            else if (memcmp(str, "rom_name", 8) == 0)
            {
               /* rom_name is not used anywhere in codebase,
                * but is frequently added to DB */
            }
            break;

         case 9:
            if (memcmp(str, "publisher", 9) == 0)
            {
               if (val_string && *val_string)
                  db_info->publisher = strdup(val_string);
            }
            else if (memcmp(str, "developer", 9) == 0)
            {
               if (val_string && *val_string)
                  db_info->developer = string_split(val_string, "|");
            }
            else if (memcmp(str, "narrative", 9) == 0)
            {
               if (val_string && *val_string)
                  db_info->narrative = strdup(val_string);
            }
            else if (memcmp(str, "vehicular", 9) == 0)
            {
               if (val_string && *val_string)
                  db_info->vehicular = strdup(val_string);
            }
            else if (memcmp(str, "franchise", 9) == 0)
            {
               if (val_string && *val_string)
                  db_info->franchise = strdup(val_string);
            }
            break;

         case 10:
            if (memcmp(str, "edge_issue", 10) == 0)
               db_info->edge_magazine_issue = (unsigned)val->val.uint_;
            break;

         case 11:
            if (memcmp(str, "description", 11) == 0)
            {
               if (val_string && *val_string)
                  db_info->description = strdup(val_string);
            }
            else if (memcmp(str, "perspective", 11) == 0)
            {
               if (val_string && *val_string)
                  db_info->perspective = strdup(val_string);
            }
            else if (memcmp(str, "bbfc_rating", 11) == 0)
            {
               if (val_string && *val_string)
                  db_info->bbfc_rating = strdup(val_string);
            }
            else if (memcmp(str, "esrb_rating", 11) == 0)
            {
               if (val_string && *val_string)
                  db_info->esrb_rating = strdup(val_string);
            }
            else if (memcmp(str, "cero_rating", 11) == 0)
            {
               if (val_string && *val_string)
                  db_info->cero_rating = strdup(val_string);
            }
            else if (memcmp(str, "pegi_rating", 11) == 0)
            {
               if (val_string && *val_string)
                  db_info->pegi_rating = strdup(val_string);
            }
            else if (memcmp(str, "edge_rating", 11) == 0)
               db_info->edge_magazine_rating = (unsigned)val->val.uint_;
            else if (memcmp(str, "tgdb_rating", 11) == 0)
               db_info->tgdb_rating = (unsigned)val->val.uint_;
            else if (memcmp(str, "edge_review", 11) == 0)
            {
               if (val_string && *val_string)
                  db_info->edge_magazine_review = strdup(val_string);
            }
            else if (memcmp(str, "releaseyear", 11) == 0)
               db_info->releaseyear = (unsigned)val->val.uint_;
            break;

         case 12:
            if (memcmp(str, "elspa_rating", 12) == 0)
            {
               if (val_string && *val_string)
                  db_info->elspa_rating = strdup(val_string);
            }
            else if (memcmp(str, "releasemonth", 12) == 0)
               db_info->releasemonth = (unsigned)val->val.uint_;
            else if (memcmp(str, "achievements", 12) == 0)
               db_info->achievements = (int)val->val.uint_;
            break;

         case 14:
            if (memcmp(str, "famitsu_rating", 14) == 0)
               db_info->famitsu_magazine_rating = (unsigned)val->val.uint_;
            else if (memcmp(str, "enhancement_hw", 14) == 0)
            {
               if (val_string && *val_string)
                  db_info->enhancement_hw = strdup(val_string);
            }
            break;

         case 17:
            if (memcmp(str, "console_exclusive", 17) == 0)
               db_info->console_exclusive = (int)val->val.uint_;
            break;

         case 18:
            if (memcmp(str, "platform_exclusive", 18) == 0)
               db_info->platform_exclusive = (int)val->val.uint_;
            break;

         default:
            break;
      }
   }

   rmsgpack_dom_value_free(&item);

   return 0;
}

static int database_cursor_open(libretrodb_t *db,
      libretrodb_cursor_t *cur, const char *path, const char *query)
{
   const char *err       = NULL;
   libretrodb_query_t *q = NULL;

   if ((libretrodb_open(path, db, false)) != 0)
      return -1;

   if (query)
      q = (libretrodb_query_t*)libretrodb_query_compile(db, query,
      strlen(query), &err);

   if (err || (libretrodb_cursor_open(db, cur, q)) != 0)
   {
      if (q)
         libretrodb_query_free(q);
      libretrodb_close(db);
      return -1;
   }

   if (q)
      libretrodb_query_free(q);

   return 0;
}

/* Types 'cue' and 'gdi' are prioritized */
static bool type_is_prioritized(const char *path)
{
   const char *ext = path_get_extension(path);
   if (ext)
   {
      char e0 = ext[0] | 0x20;
      char e1 = ext[1] | 0x20;
      char e2 = ext[2] | 0x20;
      if (ext[3] == '\0')
         return (e0 == 'c' && e1 == 'u' && e2 == 'e')
            || (e0 == 'g' && e1 == 'd' && e2 == 'i');
   }
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

database_info_handle_t *database_info_dir_init(const char *dir,
      enum database_type type, char *file_exts,
      bool show_hidden_files, bool recursive, bool include_archive,
      struct string_list **content_list)
{
   core_info_list_t *core_info_list = NULL;
   struct string_list       *list   = NULL;
   database_info_handle_t     *db   = (database_info_handle_t*)
      malloc(sizeof(*db));

   if (!db)
      return NULL;

   /* File list will include all supported files, 
    * unless extension list is given */
   if (!file_exts || !*file_exts)
      core_info_get_list(&core_info_list);

   if (!(list = dir_list_new(dir, core_info_list ? core_info_list->all_ext : file_exts,
         false, show_hidden_files,
         include_archive, recursive)))
   {
      free(db);
      return NULL;
   }

   /* dir list prioritize */
   qsort(list->elems, list->size, sizeof(*list->elems), dir_entry_compare);

   db->status             = DATABASE_STATUS_ITERATE;
   db->type               = type;
   *content_list          = list;

   return db;
}

database_info_handle_t *database_info_file_init(const char *path,
      enum database_type type, retro_task_t *task, struct string_list **content_list)
{
   union string_list_elem_attr attr;
   struct string_list        *list  = NULL;
   database_info_handle_t      *db  = (database_info_handle_t*)
      malloc(sizeof(*db));

   if (!db)
      return NULL;

   if (!(list = string_list_new()))
   {
      free(db);
      return NULL;
   }

   attr.i                 = 0;
   string_list_append(list, path, attr);

   db->status             = DATABASE_STATUS_ITERATE;
   db->type               = type;
   *content_list          = list;

   return db;
}

void database_info_free(database_info_handle_t *db)
{
/*   if (db)
      string_list_free(db->list);*/
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
            if (db_info.category)
               free(db_info.category);
            if (db_info.language)
               free(db_info.language);
            if (db_info.region)
               free(db_info.region);
            if (db_info.score)
               free(db_info.score);
            if (db_info.media)
               free(db_info.media);
            if (db_info.controls)
               free(db_info.controls);
            if (db_info.artstyle)
               free(db_info.artstyle);
            if (db_info.gameplay)
               free(db_info.gameplay);
            if (db_info.narrative)
               free(db_info.narrative);
            if (db_info.pacing)
               free(db_info.pacing);
            if (db_info.perspective)
               free(db_info.perspective);
            if (db_info.setting)
               free(db_info.setting);
            if (db_info.visual)
               free(db_info.visual);
            if (db_info.vehicular)
               free(db_info.vehicular);
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
            if (db_info.md5)
               free(db_info.md5);
            if (db_info.sha1)
               free(db_info.sha1);

            db_info.name                 = NULL;
            db_info.rom_name             = NULL;
            db_info.serial               = NULL;
            db_info.genre                = NULL;
            db_info.description          = NULL;
            db_info.publisher            = NULL;
            db_info.developer            = NULL;
            db_info.origin               = NULL;
            db_info.franchise            = NULL;
            db_info.edge_magazine_review = NULL;
            db_info.cero_rating          = NULL;
            db_info.pegi_rating          = NULL;
            db_info.enhancement_hw       = NULL;
            db_info.elspa_rating         = NULL;
            db_info.esrb_rating          = NULL;
            db_info.bbfc_rating          = NULL;
            db_info.sha1                 = NULL;
            db_info.md5                  = NULL;

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
      libretrodb_cursor_close(cur);
      libretrodb_close(db);
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
      if (info->category)
         free(info->category);
      if (info->language)
         free(info->language);
      if (info->region)
         free(info->region);
      if (info->score)
         free(info->score);
      if (info->media)
         free(info->media);
      if (info->controls)
         free(info->controls);
      if (info->artstyle)
         free(info->artstyle);
      if (info->gameplay)
         free(info->gameplay);
      if (info->narrative)
         free(info->narrative);
      if (info->pacing)
         free(info->pacing);
      if (info->perspective)
         free(info->perspective);
      if (info->setting)
         free(info->setting);
      if (info->visual)
         free(info->visual);
      if (info->vehicular)
         free(info->vehicular);
      if (info->description)
         free(info->description);
      if (info->publisher)
         free(info->publisher);
      if (info->developer)
         string_list_free(info->developer);
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

      info->name                 = NULL;
      info->rom_name             = NULL;
      info->serial               = NULL;
      info->genre                = NULL;
      info->description          = NULL;
      info->publisher            = NULL;
      info->developer            = NULL;
      info->origin               = NULL;
      info->franchise            = NULL;
      info->edge_magazine_review = NULL;
      info->cero_rating          = NULL;
      info->pegi_rating          = NULL;
      info->enhancement_hw       = NULL;
      info->elspa_rating         = NULL;
      info->esrb_rating          = NULL;
      info->bbfc_rating          = NULL;
      info->sha1                 = NULL;
      info->md5                  = NULL;
   }

   free(database_info_list->list);
}
