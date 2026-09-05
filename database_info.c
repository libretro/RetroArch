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
   /* Build queries of the shape {'KEY':"PATH"} (or {'KEY':PATH}
    * for numeric/rating fields, or the DEVELOPER glob form).
    *
    * Pre-this-rewrite each case unrolled the per-character
    * writes with s[++_len]= and then a path strlcpy followed
    * by another short s[++_len]= chain.  None of those writes
    * was bounded against len; if the caller's buffer was
    * smaller than the prefix's length the writes ran off the
    * end of s, and after the path strlcpy the trailing
    * s[_len]='"';s[++_len]='}' sequence could OOB-write up to
    * 3 bytes when path filled s.  Naively replacing the chain
    * with _len += strlcpy(s+_len, lit, len-_len) does not
    * help: strlcpy returns strlen(source) regardless of
    * truncation, so on overflow _len passes len and the next
    * (len - _len) underflows size_t, producing an
    * unbounded-size strlcpy and the same OOB.
    *
    * Use strlcpy_append (libretro-common/string/stdstring.c),
    * which is bound-checked and clamps *pos to len - 1 on
    * truncation, so subsequent calls in the chain short-
    * circuit safely.  The caller checks the final return
    * value to detect any truncation in the chain. */
   size_t _len = 0;

   if (!s || len == 0)
      return -1;

   switch (type)
   {
      case DATABASE_QUERY_ENTRY:
         strlcpy_append(s, len, &_len, "{'name':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_PUBLISHER:
         strlcpy_append(s, len, &_len, "{'publisher':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_DEVELOPER:
         strlcpy_append(s, len, &_len, "{'developer':glob('*");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "*')}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_ORIGIN:
         strlcpy_append(s, len, &_len, "{'origin':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_FRANCHISE:
         strlcpy_append(s, len, &_len, "{'franchise':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_RATING:
         strlcpy_append(s, len, &_len, "{'esrb_rating':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_BBFC_RATING:
         /* Pre-rewrite this case wrote s[++_len] = '"' after
          * the path strlcpy instead of s[_len] = '"', leaving
          * the strlcpy's NUL terminator embedded in the query
          * and silently breaking BBFC searches.  Fix while
          * rewriting. */
         strlcpy_append(s, len, &_len, "{'bbfc_rating':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_ELSPA_RATING:
         strlcpy_append(s, len, &_len, "{'elspa_rating':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_ESRB_RATING:
         strlcpy_append(s, len, &_len, "{'esrb_rating':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_PEGI_RATING:
         strlcpy_append(s, len, &_len, "{'pegi_rating':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_CERO_RATING:
         strlcpy_append(s, len, &_len, "{'cero_rating':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_ENHANCEMENT_HW:
         strlcpy_append(s, len, &_len, "{'enhancement_hw':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_RATING:
         strlcpy_append(s, len, &_len, "{'edge_rating':");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_ISSUE:
         strlcpy_append(s, len, &_len, "{'edge_issue':");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_FAMITSU_MAGAZINE_RATING:
         strlcpy_append(s, len, &_len, "{'famitsu_rating':");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_RELEASEDATE_MONTH:
         strlcpy_append(s, len, &_len, "{'releasemonth':");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_RELEASEDATE_YEAR:
         strlcpy_append(s, len, &_len, "{'releaseyear':");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_MAX_USERS:
         strlcpy_append(s, len, &_len, "{'users':");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_GENRE:
         strlcpy_append(s, len, &_len, "{'genre':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_ENTRY_REGION:
         strlcpy_append(s, len, &_len, "{'region':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
         break;
      case DATABASE_QUERY_NONE:
         strlcpy_append(s, len, &_len, "{'':':\"");
         strlcpy_append(s, len, &_len, path);
         if (strlcpy_append(s, len, &_len, "\"}"))
            return -1;
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

/* Field selection flags for database_cursor_iterate_filtered.
 * 0 = extract all fields (backward compatible). */
#define DB_EXTRACT_NAME     (1 << 0)
#define DB_EXTRACT_CRC      (1 << 1)
#define DB_EXTRACT_SERIAL   (1 << 2)
#define DB_EXTRACT_SIZE     (1 << 3)
#define DB_EXTRACT_MD5      (1 << 4)
#define DB_EXTRACT_SHA1     (1 << 5)

/* Combination used by the scanner */
#define DB_EXTRACT_SCAN_FIELDS \
   (DB_EXTRACT_NAME | DB_EXTRACT_CRC | DB_EXTRACT_SERIAL | DB_EXTRACT_SIZE)

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

      if (!(str = key->val.string.buff))
         continue;
      str_len = strlen(str);

      /* Only read the string member when the value actually holds a
       * string.  string.buff, binary.buff, map.items and array.items
       * all sit at the same offset in the union, so a field stored
       * with an unexpected type used to hand the strdup()s below a
       * pointer to the wrong kind of data - the raw bytes of a binary
       * field, or the first bytes of a map's pair array - which then
       * went into the playlist label verbatim.
       *
       * The scalar types are harmless in practice (uint_/int_ sit at
       * offset 0, so buff reads the calloc'd zero and the
       * "val_string &&" tests below reject it), and the readers
       * NUL-terminate their buffers, so this is a correctness problem
       * rather than a memory-safety one - but nothing bounds the walk
       * over a map's pair array, which is not guaranteed to contain a
       * zero byte.
       *
       * The md5 and sha1 fields already gate on val->type; the string
       * fields simply never did. */
      /* Binary counts as well as string.  The readers allocate
       * len + 1 and write the terminator, so a binary field holding
       * text is a valid C string - and "serial" is stored that way in
       * every database shipped: 27056 binary and 0 string in
       * Sony - PlayStation 2, 26957 and 0 in Sony - PlayStation,
       * 1949 and 0 in Nintendo - Nintendo Entertainment System.
       *
       * Rejecting it, as this did briefly, left db_info->serial NULL,
       * and the scanner's serial match tests that pointer before it
       * compares - so every disc system stopped matching while the
       * crc-based ones carried on working.
       *
       * What actually needed excluding is a map or an array: their
       * items pointer sits at the same offset in the union and the
       * strdup() walks it looking for a zero byte that need not be
       * there. */
      val_string = (val->type == RDT_STRING || val->type == RDT_BINARY)
         ? val->val.string.buff
         : NULL;

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

/**
 * database_cursor_iterate_filtered:
 *
 * Like database_cursor_iterate, but only extracts fields specified
 * by the @fields bitmask. Skips strdup/allocation for unrequested
 * fields, eliminating ~30 unnecessary strdup+free per record when
 * only a few fields are needed (e.g. scanner needs crc+name+serial+size).
 *
 * @fields: Bitmask of DB_EXTRACT_* flags. 0 = extract all.
 */
/* Extract @fields from an already-read record.  Split out of
 * database_cursor_iterate_filtered() below so the crc-index path,
 * which reads records by offset rather than through a cursor, fills
 * database_info_t through exactly the same code.
 *
 * Returns 0 when the record was a map and was extracted, 1 otherwise.
 * Does not free @item; the caller owns it. */
static int database_info_fill_from_dom(struct rmsgpack_dom_value *item,
      database_info_t *db_info, unsigned fields)
{
   size_t i;

   if (item->type != RDT_MAP)
      return 1;

   db_info->analog_supported = -1;
   db_info->rumble_supported = -1;
   db_info->coop_supported   = -1;

   for (i = 0; i < item->val.map.len; i++)
   {
      struct rmsgpack_dom_value *key = &item->val.map.items[i].key;
      struct rmsgpack_dom_value *val = &item->val.map.items[i].value;
      const char *str;
      size_t      str_len;

      if (!key || !val || key->type != RDT_STRING)
         continue;

      if (!(str = key->val.string.buff))
         continue;
      str_len = key->val.string.len;

      switch (str_len)
      {
         case 3:
            if ((fields & DB_EXTRACT_CRC) && memcmp(str, "crc", 3) == 0)
            {
               if (val->type == RDT_BINARY)
               {
                  switch (val->val.binary.len)
                  {
                     case 1:  db_info->crc32 = *(uint8_t*)val->val.binary.buff; break;
                     case 2:  db_info->crc32 = swap_if_little16(*(uint16_t*)val->val.binary.buff); break;
                     case 4:  db_info->crc32 = swap_if_little32(*(uint32_t*)val->val.binary.buff); break;
                     default: db_info->crc32 = 0; break;
                  }
               }
            }
            else if ((fields & DB_EXTRACT_MD5) && memcmp(str, "md5", 3) == 0)
            {
               if (val->type == RDT_BINARY)
                  db_info->md5 = bin_to_hex_alloc(
                        (uint8_t*)val->val.binary.buff, val->val.binary.len);
            }
            break;

         case 4:
            if ((fields & DB_EXTRACT_NAME) && memcmp(str, "name", 4) == 0)
            {
               /* Gate on the stored type: string.buff, binary.buff and
                * map.items share an offset in the union, so a
                * mistyped field otherwise strdup()s the wrong kind of
                * data straight into the playlist label.  Matches the
                * md5/sha1 handling above. */
               if (val->type == RDT_STRING)
               {
                  const char *vs = val->val.string.buff;
                  if (vs && *vs)
                     db_info->name = strdup(vs);
               }
            }
            else if ((fields & DB_EXTRACT_SIZE) && memcmp(str, "size", 4) == 0)
            {
               if (val->type == RDT_UINT)
                  db_info->size = val->val.uint_;
               else if (val->type == RDT_INT && val->val.int_ >= 0)
                  db_info->size = (uint64_t)val->val.int_;
            }
            else if ((fields & DB_EXTRACT_SHA1) && memcmp(str, "sha1", 4) == 0)
            {
               if (val->type == RDT_BINARY)
                  db_info->sha1 = bin_to_hex_alloc(
                        (uint8_t*)val->val.binary.buff, val->val.binary.len);
            }
            break;

         case 6:
            if ((fields & DB_EXTRACT_SERIAL) && memcmp(str, "serial", 6) == 0)
            {
               /* Stored as binary in every shipped database; see the
                * note on val_string in database_info_fill_from_dom's
                * sibling above. */
               if (val->type == RDT_STRING || val->type == RDT_BINARY)
               {
                  const char *vs = val->val.string.buff;
                  if (vs && *vs)
                     db_info->serial = strdup(vs);
               }
            }
            break;

         default:
            break;
      }
   }

   return 0;
}

static int database_cursor_iterate_filtered(libretrodb_cursor_t *cur,
      database_info_t *db_info, unsigned fields)
{
   int rv;
   struct rmsgpack_dom_value item;

   /* Fall back to full extraction if no mask specified */
   if (fields == 0)
      return database_cursor_iterate(cur, db_info);

   if (libretrodb_cursor_read_item(cur, &item) != 0)
      return -1;

   rv = database_info_fill_from_dom(&item, db_info, fields);
   rmsgpack_dom_value_free(&item);
   return rv;
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
      char e0, e1, e2;

      /* ext[1] and ext[2] were read before anything established the
       * string was that long, and path_get_extension() returns a
       * pointer to the terminator for a path with no extension - a
       * three byte read past the end of the string, on every element
       * of the qsort() comparator below. */
      if (!ext[0] || !ext[1] || !ext[2] || ext[3] != '\0')
         return false;

      e0 = ext[0] | 0x20;
      e1 = ext[1] | 0x20;
      e2 = ext[2] | 0x20;

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

database_info_handle_t *database_info_dir_init_from_list(
      enum database_type type, struct string_list *list)
{
   database_info_handle_t *db = (database_info_handle_t*)
      malloc(sizeof(*db));

   if (!db)
      return NULL;

   /* dir list prioritize - same cue/gdi ordering as
    * database_info_dir_init() */
   qsort(list->elems, list->size, sizeof(*list->elems),
         dir_entry_compare);

   db->status = DATABASE_STATUS_ITERATE;
   db->type   = type;

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

/**
 * database_info_list_new_names_only:
 *
 * Fast path for loading just game names from an .rdb file.
 * Used by the Database Manager browse list which only needs names.
 * Reads each record's map header, scans for the "name" key using
 * field-level skip, extracts just the name string, skips everything
 * else. ~10x less work per record than the full extraction path.
 */
static database_info_list_t *database_info_list_new_names_only(
      const char *rdb_path)
{
   libretrodb_t *db            = libretrodb_new();
   libretrodb_cursor_t *cur    = libretrodb_cursor_new();
   database_info_list_t *list  = NULL;
   database_info_t *items      = NULL;
   size_t count                = 0;
   size_t capacity             = 0;

   if (!db || !cur)
      goto end;

   if (database_cursor_open(db, cur, rdb_path, NULL) != 0)
      goto end;

   list = (database_info_list_t*)malloc(sizeof(*list));
   if (!list)
      goto end;

   list->count = 0;
   list->list  = NULL;

   /* Initial capacity — avoids realloc churn for small databases
    * and reduces it for large ones */
   capacity = 256;
   items    = (database_info_t*)calloc(capacity, sizeof(*items));
   if (!items)
   {
      free(list);
      list = NULL;
      goto end;
   }

   for (;;)
   {
      struct rmsgpack_dom_value item;

      if (libretrodb_cursor_read_item(cur, &item) != 0)
         break;

      if (item.type == RDT_MAP)
      {
         unsigned i;
         char *found_name = NULL;

         /* Scan the DOM for the "name" field only */
         for (i = 0; i < item.val.map.len; i++)
         {
            struct rmsgpack_dom_value *k = &item.val.map.items[i].key;
            struct rmsgpack_dom_value *v = &item.val.map.items[i].value;

            if (  k->type == RDT_STRING
               && k->val.string.len == 4
               && memcmp(k->val.string.buff, "name", 4) == 0
               && v->type == RDT_STRING
               && v->val.string.buff
               && *v->val.string.buff)
            {
               found_name = strdup(v->val.string.buff);
               break;
            }
         }

         if (found_name)
         {
            /* Grow array geometrically */
            if (count >= capacity)
            {
               database_info_t *new_items;
               capacity *= 2;
               new_items = (database_info_t*)realloc(
                     items, capacity * sizeof(*items));
               if (!new_items)
               {
                  free(found_name);
                  rmsgpack_dom_value_free(&item);
                  break;
               }
               items = new_items;
               /* Zero the new portion so free() on unset
                * fields is safe */
               memset(&items[count], 0,
                     (capacity - count) * sizeof(*items));
            }

            memset(&items[count], 0, sizeof(items[count]));
            items[count].name = found_name;
            count++;
         }
      }

      rmsgpack_dom_value_free(&item);
   }

   list->list  = items;
   list->count = count;

end:
   if (db)
   {
      libretrodb_cursor_close(cur);
      libretrodb_close(db);
      libretrodb_free(db);
   }
   if (cur)
      libretrodb_cursor_free(cur);

   return list;
}


/* ------------------------------------------------------------------
 * CRC index
 *
 * The scanner asks each database the same question once per content
 * file - does it hold this crc - and every one of those is a full
 * cursor walk.  Walking once up front and keeping crc -> record
 * offset turns the repeats into a binary search.
 *
 * Entries are sorted by crc so a lookup finds the run of records
 * sharing one.  Matches are then re-sorted by offset before the list
 * is built, because the query path returns records in file order and
 * the scanner takes the first entry that matches; reproducing that
 * order is what makes the two paths interchangeable.
 * ------------------------------------------------------------------ */

/* uint32 offset rather than uint64: paired with the key this packs to
 * 8 bytes where a uint64 would pad the pair to 16, halving what an
 * index costs.  The largest database shipped today is 7.3 MB, so the
 * range is not close to being a constraint, and db_crc_collect()
 * refuses to index anything that would not fit. */
struct db_crc_entry
{
   uint32_t offset;
   uint32_t crc;
};

struct database_info_crc_index
{
   struct db_crc_entry *entries;
   size_t               count;
   char                *rdb_path;
   /* Size range of the indexed records, collected during the same
    * walk.  The scanner otherwise pays two more walks per database
    * for exactly this. */
   uint64_t             size_min;
   uint64_t             size_max;
   bool                 have_size;
};

static int db_crc_by_crc(const void *a, const void *b)
{
   uint32_t x = ((const struct db_crc_entry*)a)->crc;
   uint32_t y = ((const struct db_crc_entry*)b)->crc;
   if (x < y) return -1;
   if (x > y) return  1;
   return 0;
}

static int db_crc_by_offset(const void *a, const void *b)
{
   uint64_t x = ((const struct db_crc_entry*)a)->offset;
   uint64_t y = ((const struct db_crc_entry*)b)->offset;
   if (x < y) return -1;
   if (x > y) return  1;
   return 0;
}

struct db_crc_build
{
   struct db_crc_entry *entries;
   size_t               count;
   size_t               capacity;
   size_t               max_bytes;
   uint64_t             size_min;
   uint64_t             size_max;
   bool                 have_size;
   bool                 failed;
};

static int db_crc_collect(void *ctx, const uint8_t *key, size_t key_len,
      uint64_t offset, const uint64_t *aux)
{
   struct db_crc_build *b = (struct db_crc_build*)ctx;

   if (aux)
   {
      if (!b->have_size)
      {
         b->size_min  = *aux;
         b->size_max  = *aux;
         b->have_size = true;
      }
      else
      {
         if (*aux < b->size_min) b->size_min = *aux;
         if (*aux > b->size_max) b->size_max = *aux;
      }
   }

   /* Only fixed 4-byte crcs are indexable.  Anything else - including
    * a record that carries a size but no crc at all, which arrives
    * here with a zero-length key so its size still counts towards the
    * range above - is left to the query path. */
   if (key_len != 4)
      return 0;

   /* Offsets are held in 32 bits; a database large enough to exceed
    * that is handed to the query path rather than indexed wrongly. */
   if (offset > (uint64_t)0xFFFFFFFFu)
   {
      b->failed = true;
      return 1;
   }

   if (b->count == b->capacity)
   {
      size_t cap = b->capacity ? b->capacity * 2 : 1024;
      struct db_crc_entry *tmp;

      /* Give up rather than exceed the caller's allowance.  A partial
       * index would be worse than none: it would miss matches without
       * saying so. */
      if (b->max_bytes && cap * sizeof(*b->entries) > b->max_bytes)
      {
         b->failed = true;
         return 1;
      }

      if (!(tmp = (struct db_crc_entry*)
               realloc(b->entries, cap * sizeof(*tmp))))
      {
         b->failed = true;
         return 1;                        /* stop the scan */
      }
      b->entries  = tmp;
      b->capacity = cap;
   }

   b->entries[b->count].crc    = ((uint32_t)key[0] << 24)
                               | ((uint32_t)key[1] << 16)
                               | ((uint32_t)key[2] <<  8)
                               |  (uint32_t)key[3];
   b->entries[b->count].offset = (uint32_t)offset;
   b->count++;
   return 0;
}

database_info_crc_index_t *database_info_crc_index_new(const char *rdb_path,
      size_t max_bytes)
{
   struct db_crc_build        build;
   database_info_crc_index_t *idx = NULL;
   libretrodb_t              *db  = NULL;

   if (!rdb_path || !*rdb_path)
      return NULL;

   memset(&build, 0, sizeof(build));
   build.max_bytes = max_bytes;

   if (!(db = libretrodb_new()))
      return NULL;

   if (libretrodb_open(rdb_path, db, false) != 0)
   {
      libretrodb_free(db);
      return NULL;
   }

   if (   libretrodb_scan_field(db, "crc", "size", db_crc_collect,
             &build) != 0
       || build.failed)
      goto error;

   if (!(idx = (database_info_crc_index_t*)calloc(1, sizeof(*idx))))
      goto error;

   if (!(idx->rdb_path = strdup(rdb_path)))
   {
      free(idx);
      idx = NULL;
      goto error;
   }

   if (build.count > 1)
      qsort(build.entries, build.count, sizeof(*build.entries),
            db_crc_by_crc);

   idx->entries   = build.entries;
   idx->count     = build.count;
   idx->size_min  = build.size_min;
   idx->size_max  = build.size_max;
   idx->have_size = build.have_size;

   libretrodb_close(db);
   libretrodb_free(db);
   return idx;

error:
   free(build.entries);
   libretrodb_close(db);
   libretrodb_free(db);
   return NULL;
}

bool database_info_crc_index_size_range(
      const database_info_crc_index_t *idx, int64_t *min, int64_t *max)
{
   if (!idx || !idx->have_size || !min || !max)
      return false;
   *min = (int64_t)idx->size_min;
   *max = (int64_t)idx->size_max;
   return true;
}

size_t database_info_crc_index_bytes(const database_info_crc_index_t *idx)
{
   if (!idx)
      return 0;
   return idx->count * sizeof(*idx->entries) + sizeof(*idx);
}

void database_info_crc_index_free(database_info_crc_index_t *idx)
{
   if (!idx)
      return;
   free(idx->entries);
   free(idx->rdb_path);
   free(idx);
}

size_t database_info_crc_index_count(const database_info_crc_index_t *idx)
{
   return idx ? idx->count : 0;
}

/* Append every entry whose crc is @crc, returning the new count, or
 * (size_t)-1 if the run does not fit.  Truncating instead would hand
 * the caller a shorter list than the query path returns for the same
 * crc, which is a wrong answer rather than a slow one - databases do
 * carry placeholder keys shared by hundreds of records. */
static size_t db_crc_gather(const database_info_crc_index_t *idx,
      uint32_t crc, struct db_crc_entry *out, size_t have, size_t cap)
{
   size_t lo = 0;
   size_t hi = idx->count;

   while (lo < hi)
   {
      size_t mid = lo + ((hi - lo) / 2);
      if (idx->entries[mid].crc < crc)
         lo = mid + 1;
      else
         hi = mid;
   }

   while (lo < idx->count && idx->entries[lo].crc == crc)
   {
      if (have >= cap)
         return (size_t)-1;
      out[have++] = idx->entries[lo++];
   }

   return have;
}

database_info_list_t *database_info_list_new_crc(
      const database_info_crc_index_t *idx, const char *rdb_path,
      uint32_t crc, uint32_t archive_crc, unsigned fields)
{
   /* Working set for one lookup.  A run longer than this is handed
    * back to the query path rather than truncated - see
    * db_crc_gather().  It is not rare: at least one shipped database
    * carries a placeholder serial shared by hundreds of records, and
    * assuming a bounded run was always enough is what silently
    * shortened those results before. */
   struct db_crc_entry   hits[32];
   database_info_list_t *list  = NULL;
   database_info_t      *items = NULL;
   libretrodb_t         *db    = NULL;
   size_t                found = 0;
   size_t                i, kept = 0;

   if (!idx)
      return NULL;

   /* The caller keys its index cache by database position, and that
    * position moves when a matched database is promoted.  Refuse an
    * index that belongs to a different database rather than answering
    * with another system's records: the query path then runs, which
    * is slow but right. */
   if (rdb_path && idx->rdb_path && strcmp(rdb_path, idx->rdb_path))
      return NULL;

   found = db_crc_gather(idx, crc, hits, found,
         sizeof(hits) / sizeof(hits[0]));
   if (found != (size_t)-1 && archive_crc && archive_crc != crc)
      found = db_crc_gather(idx, archive_crc, hits, found,
            sizeof(hits) / sizeof(hits[0]));

   /* Too many records share this crc to answer from the index; let
    * the query path, which has no such bound, handle it. */
   if (found == (size_t)-1)
      return NULL;

   if (!(list = (database_info_list_t*)calloc(1, sizeof(*list))))
      return NULL;

   if (found == 0)
      return list;                  /* empty result, as a miss returns */

   /* File order, so the scanner sees what the query path showed it. */
   if (found > 1)
      qsort(hits, found, sizeof(hits[0]), db_crc_by_offset);

   if (!(items = (database_info_t*)calloc(found, sizeof(*items))))
   {
      free(list);
      return NULL;
   }

   if (!(db = libretrodb_new())
       || libretrodb_open(idx->rdb_path, db, false) != 0)
   {
      free(items);
      free(list);
      libretrodb_free(db);
      return NULL;
   }

   for (i = 0; i < found; i++)
   {
      struct rmsgpack_dom_value item;
      if (libretrodb_read_at(db, hits[i].offset, &item) != 0)
         continue;
      if (database_info_fill_from_dom(&item, &items[kept], fields) == 0)
         kept++;
      rmsgpack_dom_value_free(&item);
   }

   libretrodb_close(db);
   libretrodb_free(db);

   list->list  = items;
   list->count = kept;
   return list;
}


/* ------------------------------------------------------------------
 * Serial index
 *
 * Disc content is matched on serial rather than crc, and that lookup
 * walks the database exactly like the crc one did.  Same treatment,
 * with one difference: a serial is variable-length, so the index
 * keeps a hash rather than the bytes and the candidate records are
 * read back and compared exactly.  A collision therefore costs an
 * extra record read, never a wrong answer.
 * ------------------------------------------------------------------ */

/* See struct db_crc_entry for why the offset is 32-bit. */
struct db_serial_entry
{
   uint32_t offset;
   uint32_t hash;
};

struct database_info_serial_index
{
   struct db_serial_entry *entries;
   size_t                  count;
   char                   *rdb_path;
};

static uint32_t db_serial_hash(const uint8_t *key, size_t len)
{
   uint32_t h = 5381;
   size_t   i;
   for (i = 0; i < len; i++)
      h = ((h << 5) + h) + key[i];
   return h;
}

/* Typed rather than punning the leading field: the previous version
 * read the first eight bytes as a uint64, which only worked while the
 * offset happened to be that wide and would silently have compared
 * offset and key together once it was not. */
static int db_serial_by_offset(const void *a, const void *b)
{
   uint32_t x = ((const struct db_serial_entry*)a)->offset;
   uint32_t y = ((const struct db_serial_entry*)b)->offset;
   if (x < y) return -1;
   if (x > y) return  1;
   return 0;
}

static int db_serial_by_hash(const void *a, const void *b)
{
   uint32_t x = ((const struct db_serial_entry*)a)->hash;
   uint32_t y = ((const struct db_serial_entry*)b)->hash;
   if (x < y) return -1;
   if (x > y) return  1;
   return 0;
}

struct db_serial_build
{
   struct db_serial_entry *entries;
   size_t                  count;
   size_t                  capacity;
   size_t                  max_bytes;
   bool                    failed;
};

static int db_serial_collect(void *ctx, const uint8_t *key, size_t key_len,
      uint64_t offset, const uint64_t *aux)
{
   struct db_serial_build *b = (struct db_serial_build*)ctx;
   (void)aux;

   if (!key_len)
      return 0;

   /* A record can carry the key more than once; one entry per record
    * is enough because the lookup reads the record back anyway. */
   if (b->count && b->entries[b->count - 1].offset == (uint32_t)offset)
      return 0;

   /* See db_crc_collect(). */
   if (offset > (uint64_t)0xFFFFFFFFu)
   {
      b->failed = true;
      return 1;
   }

   if (b->count == b->capacity)
   {
      size_t cap = b->capacity ? b->capacity * 2 : 1024;
      struct db_serial_entry *tmp;

      /* See db_crc_collect(). */
      if (b->max_bytes && cap * sizeof(*b->entries) > b->max_bytes)
      {
         b->failed = true;
         return 1;
      }

      if (!(tmp = (struct db_serial_entry*)
               realloc(b->entries, cap * sizeof(*tmp))))
      {
         b->failed = true;
         return 1;
      }
      b->entries  = tmp;
      b->capacity = cap;
   }

   b->entries[b->count].hash   = db_serial_hash(key, key_len);
   b->entries[b->count].offset = (uint32_t)offset;
   b->count++;
   return 0;
}

database_info_serial_index_t *database_info_serial_index_new(
      const char *rdb_path, size_t max_bytes)
{
   struct db_serial_build        build;
   database_info_serial_index_t *idx = NULL;
   libretrodb_t                 *db  = NULL;

   if (!rdb_path || !*rdb_path)
      return NULL;

   memset(&build, 0, sizeof(build));
   build.max_bytes = max_bytes;

   if (!(db = libretrodb_new()))
      return NULL;

   if (libretrodb_open(rdb_path, db, false) != 0)
   {
      libretrodb_free(db);
      return NULL;
   }

   if (   libretrodb_scan_field(db, "serial", NULL, db_serial_collect,
             &build) != 0
       || build.failed)
      goto error;

   if (!(idx = (database_info_serial_index_t*)calloc(1, sizeof(*idx))))
      goto error;

   if (!(idx->rdb_path = strdup(rdb_path)))
   {
      free(idx);
      idx = NULL;
      goto error;
   }

   if (build.count > 1)
      qsort(build.entries, build.count, sizeof(*build.entries),
            db_serial_by_hash);

   idx->entries = build.entries;
   idx->count   = build.count;

   libretrodb_close(db);
   libretrodb_free(db);
   return idx;

error:
   free(build.entries);
   libretrodb_close(db);
   libretrodb_free(db);
   return NULL;
}

size_t database_info_serial_index_bytes(
      const database_info_serial_index_t *idx)
{
   if (!idx)
      return 0;
   return idx->count * sizeof(*idx->entries) + sizeof(*idx);
}

void database_info_serial_index_free(database_info_serial_index_t *idx)
{
   if (!idx)
      return;
   free(idx->entries);
   free(idx->rdb_path);
   free(idx);
}

size_t database_info_serial_index_count(
      const database_info_serial_index_t *idx)
{
   return idx ? idx->count : 0;
}

/* True when @item carries @serial in a "serial" field. */
static bool db_record_has_serial(struct rmsgpack_dom_value *item,
      const char *serial, size_t serial_len)
{
   size_t i;

   if (item->type != RDT_MAP)
      return false;

   for (i = 0; i < item->val.map.len; i++)
   {
      struct rmsgpack_dom_value *k = &item->val.map.items[i].key;
      struct rmsgpack_dom_value *v = &item->val.map.items[i].value;

      if (   !k || !v
          || k->type != RDT_STRING || !k->val.string.buff
          || strcmp(k->val.string.buff, "serial"))
         continue;

      if (   v->type == RDT_BINARY
          && v->val.binary.len == serial_len
          && v->val.binary.buff
          && memcmp(v->val.binary.buff, serial, serial_len) == 0)
         return true;

      if (   v->type == RDT_STRING
          && v->val.string.buff
          && strlen(v->val.string.buff) == serial_len
          && memcmp(v->val.string.buff, serial, serial_len) == 0)
         return true;
   }

   return false;
}

database_info_list_t *database_info_list_new_serial(
      const database_info_serial_index_t *idx, const char *rdb_path,
      const char *serial, unsigned fields)
{
   /* See database_info_list_new_crc(): a run too long to hold goes to
    * the query path rather than being cut short. */
   struct db_serial_entry hits[32];
   database_info_list_t  *list  = NULL;
   database_info_t       *items = NULL;
   libretrodb_t          *db    = NULL;
   size_t                 serial_len;
   uint32_t               want;
   size_t                 lo, hi, found = 0, i, kept = 0;

   if (!idx || !serial || !*serial)
      return NULL;

   /* See database_info_list_new_crc(). */
   if (rdb_path && idx->rdb_path && strcmp(rdb_path, idx->rdb_path))
      return NULL;

   serial_len = strlen(serial);
   want       = db_serial_hash((const uint8_t*)serial, serial_len);

   lo = 0;
   hi = idx->count;
   while (lo < hi)
   {
      size_t mid = lo + ((hi - lo) / 2);
      if (idx->entries[mid].hash < want)
         lo = mid + 1;
      else
         hi = mid;
   }

   while (lo < idx->count && idx->entries[lo].hash == want)
   {
      /* Same rule as the crc index: a run too long to hold is handed
       * back to the query path rather than truncated.  Placeholder
       * serials shared by hundreds of records do occur. */
      if (found >= sizeof(hits) / sizeof(hits[0]))
         return NULL;
      hits[found++] = idx->entries[lo++];
   }

   if (!(list = (database_info_list_t*)calloc(1, sizeof(*list))))
      return NULL;

   if (found == 0)
      return list;

   if (found > 1)
      qsort(hits, found, sizeof(hits[0]), db_serial_by_offset);

   if (!(items = (database_info_t*)calloc(found, sizeof(*items))))
   {
      free(list);
      return NULL;
   }

   if (!(db = libretrodb_new())
       || libretrodb_open(idx->rdb_path, db, false) != 0)
   {
      free(items);
      free(list);
      libretrodb_free(db);
      return NULL;
   }

   for (i = 0; i < found; i++)
   {
      struct rmsgpack_dom_value item;

      if (libretrodb_read_at(db, hits[i].offset, &item) != 0)
         continue;

      /* The index matched a hash; this is where it is made exact. */
      if (   db_record_has_serial(&item, serial, serial_len)
          && database_info_fill_from_dom(&item, &items[kept], fields) == 0)
         kept++;

      rmsgpack_dom_value_free(&item);
   }

   libretrodb_close(db);
   libretrodb_free(db);

   list->list  = items;
   list->count = kept;
   return list;
}

database_info_list_t *database_info_list_new(
      const char *rdb_path, const char *query)
{
   return database_info_list_new_filtered(rdb_path, query, 0);
}

database_info_list_t *database_info_list_new_filtered(
      const char *rdb_path, const char *query, unsigned fields)
{
   int ret                                  = 0;
   unsigned k                               = 0;
   unsigned capacity                        = 0;
   database_info_t *database_info           = NULL;
   database_info_list_t *database_info_list = NULL;
   libretrodb_t *db                         = libretrodb_new();
   libretrodb_cursor_t *cur                 = libretrodb_cursor_new();

   if (!db || !cur)
      goto end;

   /* Fast path: name-only extraction for Database Manager browse
    * (NULL query = unfiltered scan, only name is used by caller) */
   if (!query)
   {
      /* Free the db/cur we just allocated — the fast path
       * creates its own */
      libretrodb_free(db);
      libretrodb_cursor_free(cur);
      return database_info_list_new_names_only(rdb_path);
   }

   if ((database_cursor_open(db, cur, rdb_path, query) != 0))
      goto end;

   database_info_list = (database_info_list_t*)
      malloc(sizeof(*database_info_list));

   if (!database_info_list)
      goto end;

   database_info_list->count  = 0;
   database_info_list->list   = NULL;

   /* Pre-allocate with geometric growth instead of realloc-by-one */
   capacity      = 64;
   database_info = (database_info_t*)calloc(capacity, sizeof(*database_info));
   if (!database_info)
   {
      free(database_info_list);
      database_info_list = NULL;
      goto end;
   }

   while (ret != -1)
   {
      database_info_t db_info = {0};
      ret = database_cursor_iterate_filtered(cur, &db_info, fields);

      if (ret == 0)
      {
         /* Grow geometrically when full */
         if (k >= capacity)
         {
            database_info_t *new_ptr;
            capacity *= 2;
            new_ptr = (database_info_t*)realloc(
                  database_info, capacity * sizeof(*database_info));

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
         }

         memcpy(&database_info[k], &db_info, sizeof(db_info));

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
