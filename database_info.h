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

#ifndef DATABASE_INFO_H_
#define DATABASE_INFO_H_

#include <stdint.h>
#include <stddef.h>

#include <file/archive_file.h>
#include <retro_common_api.h>
#include <queues/task_queue.h>

RETRO_BEGIN_DECLS

enum database_status
{
   DATABASE_STATUS_NONE = 0,
   DATABASE_STATUS_ITERATE,
   DATABASE_STATUS_ITERATE_BEGIN,
   DATABASE_STATUS_ITERATE_START,
   DATABASE_STATUS_ITERATE_NEXT,
   DATABASE_STATUS_FREE
};

enum database_type
{
   DATABASE_TYPE_NONE = 0,
   DATABASE_TYPE_ITERATE,
   DATABASE_TYPE_ITERATE_ARCHIVE,
   DATABASE_TYPE_ITERATE_LUTRO,
   DATABASE_TYPE_SERIAL_LOOKUP,
   DATABASE_TYPE_SERIAL_LOOKUP_SIZEHINT,
   DATABASE_TYPE_CRC_LOOKUP
};

enum database_query_type
{
   DATABASE_QUERY_NONE = 0,
   DATABASE_QUERY_ENTRY,
   DATABASE_QUERY_ENTRY_PUBLISHER,
   DATABASE_QUERY_ENTRY_DEVELOPER,
   DATABASE_QUERY_ENTRY_ORIGIN,
   DATABASE_QUERY_ENTRY_FRANCHISE,
   DATABASE_QUERY_ENTRY_RATING,
   DATABASE_QUERY_ENTRY_BBFC_RATING,
   DATABASE_QUERY_ENTRY_ELSPA_RATING,
   DATABASE_QUERY_ENTRY_ESRB_RATING,
   DATABASE_QUERY_ENTRY_PEGI_RATING,
   DATABASE_QUERY_ENTRY_CERO_RATING,
   DATABASE_QUERY_ENTRY_ENHANCEMENT_HW,
   DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_RATING,
   DATABASE_QUERY_ENTRY_EDGE_MAGAZINE_ISSUE,
   DATABASE_QUERY_ENTRY_FAMITSU_MAGAZINE_RATING,
   DATABASE_QUERY_ENTRY_RELEASEDATE_MONTH,
   DATABASE_QUERY_ENTRY_RELEASEDATE_YEAR,
   DATABASE_QUERY_ENTRY_MAX_USERS,
   DATABASE_QUERY_ENTRY_GENRE,
   DATABASE_QUERY_ENTRY_REGION
};

typedef struct
{
   enum database_status status;
   enum database_type type;
} database_info_handle_t;

typedef struct
{
   char *name;
   char *rom_name;
   char *serial;
   char *description;
   char *genre;
   char *category;
   char *language;
   char *region;
   char *score;
   char *media;
   char *controls;
   char *artstyle;
   char *gameplay;
   char *narrative;
   char *pacing;
   char *perspective;
   char *setting;
   char *visual;
   char *vehicular;
   char *publisher;
   struct string_list *developer;
   char *origin;
   char *franchise;
   char *edge_magazine_review;
   char *bbfc_rating;
   char *elspa_rating;
   char *esrb_rating;
   char *pegi_rating;
   char *cero_rating;
   char *enhancement_hw;
   char *sha1;
   char *md5;
   void *userdata;
   int achievements;
   int console_exclusive;
   int platform_exclusive;
   int analog_supported;
   int rumble_supported;
   int coop_supported;
   uint32_t crc32;
   uint64_t size;
   unsigned famitsu_magazine_rating;
   unsigned edge_magazine_rating;
   unsigned edge_magazine_issue;
   unsigned max_users;
   unsigned releasemonth;
   unsigned releaseyear;
   unsigned tgdb_rating;
} database_info_t;

typedef struct
{
   database_info_t *list;
   size_t count;
} database_info_list_t;

/* Field selection flags for database_info_list_new_filtered.
 * Controls which fields are extracted from each record.
 * 0 = extract all fields. */
#define DB_EXTRACT_NAME     (1 << 0)
#define DB_EXTRACT_CRC      (1 << 1)
#define DB_EXTRACT_SERIAL   (1 << 2)
#define DB_EXTRACT_SIZE     (1 << 3)
#define DB_EXTRACT_MD5      (1 << 4)
#define DB_EXTRACT_SHA1     (1 << 5)

/* Preset used by the ROM scanner */
#define DB_EXTRACT_SCAN_FIELDS \
   (DB_EXTRACT_NAME | DB_EXTRACT_CRC | DB_EXTRACT_SERIAL | DB_EXTRACT_SIZE)

database_info_list_t *database_info_list_new(const char *rdb_path,
      const char *query);

/* One-slot cache of the most recent async database scan result
 * (see tasks/task_database_info.c). The cache retains ownership
 * of the returned list - callers must not free it. */
database_info_list_t *menu_dbinfo_cache_get(const char *path,
      const char *query);
bool menu_dbinfo_cache_has(const char *path, const char *query);

/* A crc -> record-offset index over one database, built in a single
 * pass.  The scanner otherwise walks a whole database per content
 * file; with an index it walks once and binary-searches thereafter.
 * Costs 8 bytes per indexed record - a 32-bit offset beside the key -
 * and lives for as long as the caller keeps it. */
typedef struct database_info_crc_index database_info_crc_index_t;

/* @max_bytes caps the entry table; the build gives up and returns
 * NULL rather than exceed it, because a partial index would miss
 * matches without saying so.  Zero means no limit. */
database_info_crc_index_t *database_info_crc_index_new(const char *rdb_path,
      size_t max_bytes);

/* Bytes the index actually holds, for a caller tracking a budget
 * across several databases. */
size_t database_info_crc_index_bytes(const database_info_crc_index_t *idx);
void database_info_crc_index_free(database_info_crc_index_t *idx);
size_t database_info_crc_index_count(const database_info_crc_index_t *idx);

/* Size range of the records the index covers, collected during the
 * same walk that built it.  False when no record carried a size. */
bool database_info_crc_index_size_range(
      const database_info_crc_index_t *idx, int64_t *min, int64_t *max);

/* Records whose crc is @crc or @archive_crc, in file order, with
 * @fields extracted - the same list database_info_list_new_filtered()
 * returns for "{crc:or(...)}".  NULL if the lookup could not be
 * served, so the caller falls back to the query path. */
/* @rdb_path is the database the caller believes @idx describes; the
 * lookup refuses if it does not match, so an index paired with the
 * wrong database degrades to the query path instead of answering with
 * another system's records.  Pass NULL to skip the check. */
database_info_list_t *database_info_list_new_crc(
      const database_info_crc_index_t *idx, const char *rdb_path,
      uint32_t crc, uint32_t archive_crc, unsigned fields);

/* The same treatment for the serial lookup disc content uses.  A
 * serial is variable-length, so the index keeps a hash and the
 * candidate records are read back and compared exactly: a collision
 * costs an extra read, never a wrong match. */
typedef struct database_info_serial_index database_info_serial_index_t;

database_info_serial_index_t *database_info_serial_index_new(
      const char *rdb_path, size_t max_bytes);

size_t database_info_serial_index_bytes(
      const database_info_serial_index_t *idx);
void database_info_serial_index_free(database_info_serial_index_t *idx);
size_t database_info_serial_index_count(
      const database_info_serial_index_t *idx);

database_info_list_t *database_info_list_new_serial(
      const database_info_serial_index_t *idx, const char *rdb_path,
      const char *serial, unsigned fields);

database_info_list_t *database_info_list_new_filtered(const char *rdb_path,
      const char *query, unsigned fields);

void database_info_list_free(database_info_list_t *list);

database_info_handle_t *database_info_dir_init(const char *dir,
      enum database_type type, char* file_exts,
      bool show_hidden_files, bool recursive, bool include_archive, 
      struct string_list **content_list);

/* As database_info_dir_init(), but over a content list the caller has
 * already built (e.g. incrementally via dir_list_iter_step() so the
 * walk could be spread across task gathers).  Applies the same
 * cue/gdi-prioritising sort the directory variant applies and borrows
 * @list without taking ownership.  Returns NULL only on allocation
 * failure. */
database_info_handle_t *database_info_dir_init_from_list(
      enum database_type type, struct string_list *list);

database_info_handle_t *database_info_file_init(const char *path,
      enum database_type type, retro_task_t *task, struct string_list **content_list);

void database_info_free(database_info_handle_t *handle);

int database_info_build_query_enum(
      char *query, size_t len, enum database_query_type type, const char *path);

/* NOTE: Allocates memory, it is the caller's responsibility to free the
 * memory after it is no longer required. */
char *bin_to_hex_alloc(const uint8_t *data, size_t len);

RETRO_END_DECLS

#endif /* CORE_INFO_H_ */
