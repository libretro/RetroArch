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

int database_info_build_query_enum(char *s, size_t len,
      enum database_query_type type,
      const char *path)
{
   size_t pos = 0;

   switch (type)
   {
      case DATABASE_QUERY_ENTRY:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'n';
         s[3]       = 'a';
         s[4]       = 'm';
         s[5]       = 'e';
         s[6]       = '\'';
         s[7]       = ':';
         s[8]       = '"';
         s[9]       = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_PUBLISHER:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'p';
         s[3]       = 'u';
         s[4]       = 'b';
         s[5]       = 'l';
         s[6]       = 'i';
         s[7]       = 's';
         s[8]       = 'h';
         s[9]       = 'e';
         s[10]      = 'r';
         s[11]      = '\'';
         s[12]      = ':';
         s[13]      = '"';
         s[14]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_DEVELOPER:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'd';
         s[3]       = 'e';
         s[4]       = 'v';
         s[5]       = 'e';
         s[6]       = 'l';
         s[7]       = 'o';
         s[8]       = 'p';
         s[9]       = 'e';
         s[10]      = 'r';
         s[11]      = '\'';
         s[12]      = ':';
         s[13]      = 'g';
         s[14]      = 'l';
         s[15]      = 'o';
         s[16]      = 'b';
         s[17]      = '(';
         s[18]      = '\'';
         s[19]      = '*';
         s[20]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '*';
         s[pos+1]   = '\'';
         s[pos+2]   = ')';
         s[pos+3]   = '}';
         s[pos+4]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_ORIGIN:
         s[0]       = '{';
	      s[1]       = '\'';
         s[2]       = 'o';
         s[3]       = 'r';
         s[4]       = 'i';
         s[5]       = 'g';
         s[6]       = 'i';
         s[7]       = 'n';
         s[8]       = '\'';
         s[9]       = ':';
         s[10]      = '"';
         s[11]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_FRANCHISE:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'f';
         s[3]       = 'r';
         s[4]       = 'a';
         s[5]       = 'n';
         s[6]       = 'c';
         s[7]       = 'h';
         s[8]       = 'i';
         s[9]       = 's';
         s[10]      = 'e';
         s[11]      = '\'';
         s[12]      = ':';
         s[13]      = '"';
         s[14]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_RATING:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'e';
         s[3]       = 's';
         s[4]       = 'r';
         s[5]       = 'b';
         s[6]       = '_';
         s[7]       = 'r';
         s[8]       = 'a';
         s[9]       = 't';
         s[10]      = 'i';
         s[11]      = 'n';
         s[12]      = 'g';
         s[13]      = '\'';
         s[14]      = ':';
         s[15]      = '"';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_BBFC_RATING:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'b';
         s[3]       = 'b';
         s[4]       = 'f';
         s[5]       = 'c';
         s[6]       = '_';
         s[7]       = 'r';
         s[8]       = 'a';
         s[9]       = 't';
         s[10]      = 'i';
         s[11]      = 'n';
         s[12]      = 'g';
         s[13]      = '\'';
         s[14]      = ':';
         s[15]      = '"';
         s[16]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_ELSPA_RATING:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'e';
         s[3]       = 'l';
         s[4]       = 's';
         s[5]       = 'p';
         s[6]       = 'a';
         s[7]       = '_';
         s[8]       = 'r';
         s[9]       = 'a';
         s[10]      = 't';
         s[11]      = 'i';
         s[12]      = 'n';
         s[13]      = 'g';
         s[14]      = '\'';
         s[15]      = ':';
         s[16]      = '"';
         s[17]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_ESRB_RATING:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'e';
         s[3]       = 's';
         s[4]       = 'r';
         s[5]       = 'b';
         s[6]       = '_';
         s[7]       = 'r';
         s[8]       = 'a';
         s[9 ]      = 't';
         s[10]      = 'i';
         s[11]      = 'n';
         s[12]      = 'g';
         s[13]      = '\'';
         s[14]      = ':';
         s[15]      = '"';
         s[16]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_PEGI_RATING:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'p';
         s[3]       = 'e';
         s[4]       = 'g';
         s[5]       = 'i';
         s[6]       = '_';
         s[7]       = 'r';
         s[8]       = 'a';
         s[9]       = 't';
         s[10]      = 'i';
         s[11]      = 'n';
         s[12]      = 'g';
         s[13]      = '\'';
         s[14]      = ':';
         s[15]      = '"';
         s[16]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_CERO_RATING:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'c';
         s[3]       = 'e';
         s[4]       = 'r';
         s[5]       = 'o';
         s[6]       = '_';
         s[7]       = 'r';
         s[8]       = 'a';
         s[9]       = 't';
         s[10]      = 'i';
         s[11]      = 'n';
         s[12]      = 'g';
         s[13]      = '\'';
         s[14]      = ':';
         s[15]      = '"';
         s[16]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_ENHANCEMENT_HW:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'e';
         s[3]       = 'n';
         s[4]       = 'h';
         s[5]       = 'a';
         s[6]       = 'n';
         s[7]       = 'c';
         s[8]       = 'e';
         s[9]       = 'm';
         s[10]      = 'e';
         s[11]      = 'n';
         s[12]      = 't';
         s[13]      = '_';
         s[14]      = 'h';
         s[15]      = 'w';
         s[16]      = '\'';
         s[17]      = ':';
         s[18]      = '"';
         s[19]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_RATING:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'e';
         s[3]       = 'd';
         s[4]       = 'g';
         s[5]       = 'e';
         s[6]       = '_';
         s[7]       = 'r';
         s[8]       = 'a';
         s[9]       = 't';
         s[10]      = 'i';
         s[11]      = 'n';
         s[12]      = 'g';
         s[13]      = '\'';
         s[14]      = ':';
         s[15]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '}';
         s[pos+1]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_ISSUE:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'e';
         s[3]       = 'd';
         s[4]       = 'g';
         s[5]       = 'e';
         s[6]       = '_';
         s[7]       = 'i';
         s[8]       = 's';
         s[9]       = 's';
         s[10]      = 'u';
         s[11]      = 'e';
         s[12]      = '\'';
         s[13]      = ':';
         s[14]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '}';
         s[pos+1]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_FAMITSU_MAGAZINE_RATING:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'f';
         s[3]       = 'a';
         s[4]       = 'm';
         s[5]       = 'i';
         s[6]       = 't';
         s[7]       = 's';
         s[8]       = 'u';
         s[9]       = '_';
         s[10]      = 'r';
         s[11]      = 'a';
         s[12]      = 't';
         s[13]      = 'i';
         s[14]      = 'n';
         s[15]      = 'g';
         s[16]      = '\'';
         s[17]      = ':';
         s[18]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '}';
         s[pos+1]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_RELEASEDATE_MONTH:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'r';
         s[3]       = 'e';
         s[4]       = 'l';
         s[5]       = 'e';
         s[6]       = 'a';
         s[7]       = 's';
         s[8]       = 'e';
         s[9]       = 'm';
         s[10]      = 'o';
         s[11]      = 'n';
         s[12]      = 't';
         s[13]      = 'h';
         s[14]      = '\'';
         s[15]      = ':';
         s[16]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '}';
         s[pos+1]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_RELEASEDATE_YEAR:
	 s[0]       = '{';
	 s[1]       = '\'';
         s[2]       = 'r';
         s[3]       = 'e';
         s[4]       = 'l';
         s[5]       = 'e';
         s[6]       = 'a';
         s[7]       = 's';
         s[8]       = 'e';
         s[9]       = 'y';
         s[10]      = 'e';
         s[11]      = 'a';
         s[12]      = 'r';
         s[13]      = '\'';
         s[14]      = ':';
         s[15]      = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '}';
         s[pos+1]   = '\0';
         break;
      case DATABASE_QUERY_ENTRY_MAX_USERS:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = 'u';
         s[3]       = 's';
         s[4]       = 'e';
         s[5]       = 'r';
         s[6]       = 's';
         s[7]       = '\'';
         s[8]       = ':';
         s[9]       = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '}';
         s[pos+1]   = '\0';
         break;
      case DATABASE_QUERY_NONE:
         s[0]       = '{';
         s[1]       = '\'';
         s[2]       = '\'';
         s[3]       = ':';
         s[4]       = '\'';
         s[5]       = ':';
         s[6]       = '"';
         s[7]       = '\0';
         pos        = strlcat(s, path, len);
         s[pos  ]   = '"';
         s[pos+1]   = '}';
         s[pos+2]   = '\0';
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
      else if (string_is_equal(str, "category"))
      {
         if (!string_is_empty(val_string))
            db_info->category = strdup(val_string);
      }
      else if (string_is_equal(str, "language"))
      {
         if (!string_is_empty(val_string))
            db_info->language = strdup(val_string);
      }
      else if (string_is_equal(str, "region"))
      {
         if (!string_is_empty(val_string))
            db_info->region = strdup(val_string);
      }
      else if (string_is_equal(str, "score"))
      {
         if (!string_is_empty(val_string))
            db_info->score = strdup(val_string);
      }
      else if (string_is_equal(str, "media"))
      {
         if (!string_is_empty(val_string))
            db_info->media = strdup(val_string);
      }
      else if (string_is_equal(str, "controls"))
      {
         if (!string_is_empty(val_string))
            db_info->controls = strdup(val_string);
      }
      else if (string_is_equal(str, "artstyle"))
      {
         if (!string_is_empty(val_string))
            db_info->artstyle = strdup(val_string);
      }
      else if (string_is_equal(str, "gameplay"))
      {
         if (!string_is_empty(val_string))
            db_info->gameplay = strdup(val_string);
      }
      else if (string_is_equal(str, "narrative"))
      {
         if (!string_is_empty(val_string))
            db_info->narrative = strdup(val_string);
      }
      else if (string_is_equal(str, "pacing"))
      {
         if (!string_is_empty(val_string))
            db_info->pacing = strdup(val_string);
      }
      else if (string_is_equal(str, "perspective"))
      {
         if (!string_is_empty(val_string))
            db_info->perspective = strdup(val_string);
      }
      else if (string_is_equal(str, "setting"))
      {
         if (!string_is_empty(val_string))
            db_info->setting = strdup(val_string);
      }
      else if (string_is_equal(str, "visual"))
      {
         if (!string_is_empty(val_string))
            db_info->visual = strdup(val_string);
      }
      else if (string_is_equal(str, "vehicular"))
      {
         if (!string_is_empty(val_string))
            db_info->vehicular = strdup(val_string);
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
      else if (string_ends_with_size(str, "_rating",
               strlen(str), STRLEN_CONST("_rating")))
      {
         if (string_is_equal(str, "bbfc_rating"))
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
         else if (string_is_equal(str, "edge_rating"))
            db_info->edge_magazine_rating    = (unsigned)val->val.uint_;
         else if (string_is_equal(str, "famitsu_rating"))
            db_info->famitsu_magazine_rating = (unsigned)val->val.uint_;
         else if (string_is_equal(str, "tgdb_rating"))
            db_info->tgdb_rating             = (unsigned)val->val.uint_;
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
      else if (string_is_equal(str, "edge_issue"))
         db_info->edge_magazine_issue     = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "users"))
         db_info->max_users               = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "releasemonth"))
         db_info->releasemonth            = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "releaseyear"))
         db_info->releaseyear             = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "rumble"))
         db_info->rumble_supported        = (int)val->val.uint_;
      else if (string_is_equal(str, "achievements"))
         db_info->achievements            = (int)val->val.uint_;
      else if (string_is_equal(str, "console_exclusive"))
         db_info->console_exclusive       = (int)val->val.uint_;
      else if (string_is_equal(str, "platform_exclusive"))
         db_info->platform_exclusive      = (int)val->val.uint_;
      else if (string_is_equal(str, "coop"))
         db_info->coop_supported          = (int)val->val.uint_;
      else if (string_is_equal(str, "analog"))
         db_info->analog_supported        = (int)val->val.uint_;
      else if (string_is_equal(str, "size"))
         db_info->size                    = (unsigned)val->val.uint_;
      else if (string_is_equal(str, "crc"))
      {
         switch (val->val.binary.len)
         {
            case 1:
               db_info->crc32 = *(uint8_t*)val->val.binary.buff;
               break;
            case 2:
               db_info->crc32 = swap_if_little16(*(uint16_t*)val->val.binary.buff);
               break;
            case 4:
               db_info->crc32 = swap_if_little32(*(uint32_t*)val->val.binary.buff);
               break;
            default:
               db_info->crc32 = 0;
               break;
         }
      }
      else if (string_is_equal(str, "sha1"))
         db_info->sha1 = bin_to_hex_alloc(
               (uint8_t*)val->val.binary.buff, val->val.binary.len);
      else if (string_is_equal(str, "md5"))
         db_info->md5 = bin_to_hex_alloc(
               (uint8_t*)val->val.binary.buff, val->val.binary.len);
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
      malloc(sizeof(*db));

   if (!db)
      return NULL;

   core_info_get_list(&core_info_list);

   if (!(list = dir_list_new(dir, core_info_list ? core_info_list->all_ext : NULL,
         false, show_hidden_files,
         false, true)))
   {
      free(db);
      return NULL;
   }

   dir_list_prioritize(list);

   db->status             = DATABASE_STATUS_ITERATE;
   db->type               = type;
   db->list_ptr           = 0;
   db->list               = list;

   return db;
}

database_info_handle_t *database_info_file_init(const char *path,
      enum database_type type, retro_task_t *task)
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
   db->list_ptr           = 0;
   db->list               = list;

   return db;
}

void database_info_free(database_info_handle_t *db)
{
   if (db)
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
