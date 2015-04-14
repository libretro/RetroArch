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

#ifndef DATABASE_INFO_H_
#define DATABASE_INFO_H_

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include "libretro-db/libretrodb.h"

#ifdef __cplusplus
extern "C" {
#endif

enum database_status
{
   DATABASE_STATUS_NONE = 0,
   DATABASE_STATUS_ITERATE,
   DATABASE_STATUS_FREE,
};

enum database_type
{
   DATABASE_TYPE_NONE = 0,
   DATABASE_TYPE_RDL_WRITE,
};

typedef struct
{
   enum database_status status;
   enum database_type type;
   size_t list_ptr;
   struct string_list *list;
} database_info_handle_t;

typedef struct
{
   char *name;
   char *description;
   char *publisher;
   char *developer;
   char *origin;
   char *franchise;
   char *edge_magazine_review;
   char *bbfc_rating;
   char *elspa_rating;
   char *esrb_rating;
   char *pegi_rating;
   char *cero_rating;
   char *enhancement_hw;
   char *crc32;
   char *sha1;
   char *md5;
   unsigned famitsu_magazine_rating;
   unsigned edge_magazine_rating;
   unsigned edge_magazine_issue;
   unsigned max_users;
   unsigned releasemonth;
   unsigned releaseyear;
   int analog_supported;
   int rumble_supported;
   void *userdata;
} database_info_t;

typedef struct
{
   database_info_t *list;
   size_t count;
} database_info_list_t;

database_info_list_t *database_info_list_new(const char *rdb_path,
      const char *query);

void database_info_list_free(database_info_list_t *list);

int database_open_cursor(libretrodb_t *db,
      libretrodb_cursor_t *cur, const char *query);

database_info_handle_t *database_info_init(const char *dir,
      enum database_type type);

void database_info_free(database_info_handle_t *dbl);

int database_info_iterate(database_info_handle_t *dbl);

#ifdef __cplusplus
}
#endif

#endif /* CORE_INFO_H_ */
