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
/* The key cloud sync keeps its server manifest under.  Drivers that
 * treat ordinary files specially (such as backing up the copy an
 * upload replaces) leave this one alone: it is rewritten every sync. */
#define CLOUD_SYNC_SERVER_MANIFEST "manifest.server"

typedef void (*cloud_sync_complete_handler_t)(void *user_data, const char *path, bool success, RFILE *file);

typedef struct cloud_sync_driver
{
   bool (*cloud_sync_begin)(cloud_sync_complete_handler_t cb, void *user_data);
   bool (*cloud_sync_end)(cloud_sync_complete_handler_t cb, void *user_data);

   bool (*cloud_sync_read)(const char *path, const char *file, cloud_sync_complete_handler_t cb, void *user_data);
   bool (*cloud_sync_update)(const char *path, RFILE *file, cloud_sync_complete_handler_t cb, void *user_data);
   bool (*cloud_sync_free)(const char *path, cloud_sync_complete_handler_t cb, void *user_data);

   const char *ident;
   unsigned flags;
} cloud_sync_driver_t;

/* The driver's calls block on the network until they are done and
 * call back before returning (SMB, NFS). With threads they are run on
 * a worker, and their results are handed back by cloud_sync_poll(). */
#define CLOUD_SYNC_DRIVER_FLG_BLOCKING (1 << 0)

typedef struct
{
   const cloud_sync_driver_t *driver;
} cloud_sync_driver_state_t;

cloud_sync_driver_state_t *cloud_sync_state_get_ptr(void);

extern cloud_sync_driver_t cloud_sync_webdav;
#ifdef HAVE_SSL
extern cloud_sync_driver_t cloud_sync_google_drive;
#endif
#ifdef HAVE_S3
extern cloud_sync_driver_t cloud_sync_s3;
#endif
#ifdef HAVE_ICLOUD
extern cloud_sync_driver_t cloud_sync_icloud;
#endif
#ifdef HAVE_ICLOUD_DRIVE
extern cloud_sync_driver_t cloud_sync_icloud_drive;
#endif
#ifdef HAVE_SMBCLIENT
extern cloud_sync_driver_t cloud_sync_smb;
#endif
#ifdef HAVE_NFSCLIENT
extern cloud_sync_driver_t cloud_sync_nfs;
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

/**
 * cloud_sync_poll:
 *
 * Runs the completion handlers of calls a blocking driver finished on
 * its worker, on the calling thread, in the order the calls were made.
 * The caller of cloud_sync_begin() and friends calls this each time it
 * runs; it returns at once when nothing has finished.
 **/
void cloud_sync_poll(void);

/**
 * cloud_sync_deinit:
 * @timeout_ms : longest wait for a call already running on the worker
 *
 * On exit, once the task queue is drained: drops the calls a blocking
 * driver has not started, without running their handlers, and waits
 * up to @timeout_ms for the one under way so no driver code runs while
 * the frontend is torn down. A call still blocked past the bound is
 * left to finish on its own.
 **/
void cloud_sync_deinit(unsigned timeout_ms);

RETRO_END_DECLS

#endif
