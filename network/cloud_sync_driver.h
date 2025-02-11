/*  RetroArch - A frontend for libretro.
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

#ifndef __CLOUD_SYNC_DRIVER__H
#define __CLOUD_SYNC_DRIVER__H

#include <boolean.h>
#include <stddef.h>
#include <streams/file_stream.h>

RETRO_BEGIN_DECLS

/*
 * For a read, `success' indicates whether we successfully communicated with the
 * server. We may ask to read a file that doesn't exist; in that case, `success'
 * is true and `file' is NULL. `file' is expected to be close()'d by the handler
 * if non-NULL.
 */
typedef void (*cloud_sync_complete_handler_t)(void *user_data, const char *path, bool success, RFILE *file);

typedef struct cloud_sync_driver
{
   bool (*cloud_sync_begin)(cloud_sync_complete_handler_t cb, void *user_data);
   bool (*cloud_sync_end)(cloud_sync_complete_handler_t cb, void *user_data);

   bool (*cloud_sync_read)(const char *path, const char *file, cloud_sync_complete_handler_t cb, void *user_data);
   bool (*cloud_sync_update)(const char *path, RFILE *file, cloud_sync_complete_handler_t cb, void *user_data);
   bool (*cloud_sync_free)(const char *path, cloud_sync_complete_handler_t cb, void *user_data);

   const char *ident;
} cloud_sync_driver_t;

typedef struct
{
   const cloud_sync_driver_t *driver;
} cloud_sync_driver_state_t;

cloud_sync_driver_state_t *cloud_sync_state_get_ptr(void);

extern cloud_sync_driver_t cloud_sync_webdav;
#ifdef HAVE_ICLOUD
extern cloud_sync_driver_t cloud_sync_icloud;
#endif

extern const cloud_sync_driver_t *cloud_sync_drivers[];

/**
 * config_get_cloud_sync_driver_options:
 *
 * Get an enumerated list of all cloud_sync driver names, separated by '|'.
 *
 * Returns: string listing of all cloud_sync driver names, separated by '|'.
 **/
const char* config_get_cloud_sync_driver_options(void);

void cloud_sync_find_driver(const char *drv, const char *prefix,
      bool verbosity_enabled);

bool cloud_sync_begin(cloud_sync_complete_handler_t cb, void *user_data);
bool cloud_sync_end(cloud_sync_complete_handler_t cb, void *user_data);

bool cloud_sync_read(const char *path, const char *file, cloud_sync_complete_handler_t cb, void *user_data);
bool cloud_sync_update(const char *path, RFILE *file, cloud_sync_complete_handler_t cb, void *user_data);
bool cloud_sync_free(const char *path, cloud_sync_complete_handler_t cb, void *user_data);

RETRO_END_DECLS

#endif
