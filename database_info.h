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
   DATABASE_QUERY_ENTRY_MAX_USERS
};

typedef struct
{
   enum database_status status;
   enum database_type type;
   size_t list_ptr;
   struct string_list *list;
   file_archive_transfer_t state;
} database_info_handle_t;

typedef struct
{
   int analog_supported;
   int rumble_supported;
   int coop_supported;
   uint32_t crc32;
   unsigned size;
   unsigned famitsu_magazine_rating;
   unsigned edge_magazine_rating;
   unsigned edge_magazine_issue;
   unsigned max_users;
   unsigned releasemonth;
   unsigned releaseyear;
   unsigned tgdb_rating;
   char *name;
   char *rom_name;
   char *serial;
   char *description;
   char *genre;
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
} database_info_t;

typedef struct
{
   size_t count;
   database_info_t *list;
} database_info_list_t;

database_info_list_t *database_info_list_new(const char *rdb_path,
      const char *query);

void database_info_list_free(database_info_list_t *list);

database_info_handle_t *database_info_dir_init(const char *dir,
      enum database_type type, retro_task_t *task,
      bool show_hidden_files);

database_info_handle_t *database_info_file_init(const char *path,
      enum database_type type, retro_task_t *task);

void database_info_free(database_info_handle_t *handle);

int database_info_build_query_enum(
      char *query, size_t len, enum database_query_type type, const char *path);

/* NOTE: Allocates memory, it is the caller's responsibility to free the
 * memory after it is no longer required. */
char *bin_to_hex_alloc(const uint8_t *data, size_t len);

RETRO_END_DECLS

#endif /* CORE_INFO_H_ */
