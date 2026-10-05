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

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_THREADS
#include <features/features_cpu.h>
#include <rthreads/rthreads.h>
#endif

#include "cloud_sync_driver.h"
#include "../list_special.h"
#include "../retroarch.h"
#include "../verbosity.h"

static cloud_sync_driver_t cloud_sync_null = {
   NULL,  /* sync_begin */
   NULL,  /* sync_end */
   NULL,  /* read */
   NULL,  /* update */
   NULL,  /* free */
   "null", /* ident */
   0      /* flags */
};

const cloud_sync_driver_t *cloud_sync_drivers[] = {
   &cloud_sync_webdav,
#ifdef HAVE_SSL
   &cloud_sync_google_drive,
#endif
#ifdef HAVE_S3
   &cloud_sync_s3,
#endif
#ifdef HAVE_ICLOUD
   &cloud_sync_icloud,
#endif
#ifdef HAVE_ICLOUD_DRIVE
   &cloud_sync_icloud_drive,
#endif
#ifdef HAVE_SMBCLIENT
   &cloud_sync_smb,
#endif
#ifdef HAVE_NFSCLIENT
   &cloud_sync_nfs,
#endif
   &cloud_sync_null,
   NULL
};

static cloud_sync_driver_state_t cloud_sync_driver_st = {0};

cloud_sync_driver_state_t *cloud_sync_state_get_ptr(void)
{
   return &cloud_sync_driver_st;
}

/**
 * config_get_cloud_sync_driver_options:
 *
 * Get an enumerated list of all cloud sync driver names, separated by '|'.
 *
 * @return string listing of all cloud sync driver names, separated by '|'.
 **/
const char* config_get_cloud_sync_driver_options(void)
{
   return char_list_new_special(STRING_LIST_CLOUD_SYNC_DRIVERS, NULL);
}

void cloud_sync_find_driver(const char *cloud_sync_driver,
      const char *prefix, bool verbosity_enabled)
{
   cloud_sync_driver_state_t
      *cloud_sync_st              = &cloud_sync_driver_st;
   int i                        = (int)driver_find_index(
         "cloud_sync_driver", cloud_sync_driver);

   if (i >= 0)
      cloud_sync_st->driver       = (const cloud_sync_driver_t*)
         cloud_sync_drivers[i];
   else
   {
      if (verbosity_enabled && cloud_sync_driver[0])
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
               cloud_sync_driver);

         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; cloud_sync_drivers[d]; d++)
            RARCH_LOG_OUTPUT("\t%s\n", cloud_sync_drivers[d]->ident);

         RARCH_WARN("Going to default to null...\n");
      }

      i = (int)driver_find_index("cloud_sync_driver", "null");
      cloud_sync_st->driver = (const cloud_sync_driver_t*)cloud_sync_drivers[i];
   }
}

#ifdef HAVE_THREADS
/* A blocking driver's calls go to one worker thread per sync, which
 * runs them in the order they were made; the caller gets true at once
 * and its handler runs from cloud_sync_poll() on its own thread, so a
 * slow or absent server never holds up the thread driving the sync -
 * the frame loop itself when Threaded Tasks is off. The worker leaves
 * once the sync ends (END done, or BEGIN failed) and nothing is
 * queued. */
enum cloud_sync_op_type
{
   CLOUD_SYNC_OP_BEGIN = 0,
   CLOUD_SYNC_OP_END,
   CLOUD_SYNC_OP_READ,
   CLOUD_SYNC_OP_UPDATE,
   CLOUD_SYNC_OP_FREE
};

#define CLOUD_SYNC_OP_FLG_DONE    (1 << 0) /* the driver called back */
#define CLOUD_SYNC_OP_FLG_SUCCESS (1 << 1)
#define CLOUD_SYNC_OP_FLG_PATH    (1 << 2) /* ... with a path */

/* One allocation: the strings follow the struct. */
typedef struct cloud_sync_op
{
   struct cloud_sync_op         *next;
   const cloud_sync_driver_t    *driver;
   cloud_sync_complete_handler_t cb;
   void                         *user_data;
   RFILE                        *file;  /* UPDATE's file; then the result's */
   char                         *path;  /* NULL for BEGIN and END */
   char                         *local; /* READ's local file */
   unsigned                      flags;
   int                           type;
} cloud_sync_op_t;

static struct
{
   slock_t         *lock;
   scond_t         *cond;
   cloud_sync_op_t *queue;      /* for the worker, oldest first */
   cloud_sync_op_t *queue_tail;
   cloud_sync_op_t *done;       /* for cloud_sync_poll, oldest first */
   cloud_sync_op_t *done_tail;
   bool             worker;     /* a worker thread is running */
   bool             ended;      /* the sync it serves is over */
} cloud_sync_async;

static void cloud_sync_op_capture(void *user_data, const char *path,
      bool success, RFILE *file)
{
   cloud_sync_op_t *op = (cloud_sync_op_t*)user_data;
   op->flags |= CLOUD_SYNC_OP_FLG_DONE;
   if (success)
      op->flags |= CLOUD_SYNC_OP_FLG_SUCCESS;
   if (path)
      op->flags |= CLOUD_SYNC_OP_FLG_PATH;
   op->file   = file;
}

static void cloud_sync_op_run(cloud_sync_op_t *op)
{
   const cloud_sync_driver_t *driver = op->driver;
   switch (op->type)
   {
      case CLOUD_SYNC_OP_BEGIN:
         driver->cloud_sync_begin(cloud_sync_op_capture, op);
         break;
      case CLOUD_SYNC_OP_END:
         driver->cloud_sync_end(cloud_sync_op_capture, op);
         break;
      case CLOUD_SYNC_OP_READ:
         driver->cloud_sync_read(op->path, op->local,
               cloud_sync_op_capture, op);
         break;
      case CLOUD_SYNC_OP_UPDATE:
         driver->cloud_sync_update(op->path, op->file,
               cloud_sync_op_capture, op);
         break;
      case CLOUD_SYNC_OP_FREE:
         driver->cloud_sync_free(op->path, cloud_sync_op_capture, op);
         break;
   }
   /* A call that never called back failed; an upload's file goes back
    * to the handler, which closes it. */
   if (!(op->flags & CLOUD_SYNC_OP_FLG_DONE))
   {
      op->flags = CLOUD_SYNC_OP_FLG_DONE;
      if (op->type != CLOUD_SYNC_OP_UPDATE)
         op->file = NULL;
   }
}

static void cloud_sync_worker(void *data)
{
   (void)data;
   for (;;)
   {
      cloud_sync_op_t *op;

      slock_lock(cloud_sync_async.lock);
      while (!cloud_sync_async.queue && !cloud_sync_async.ended)
         scond_wait(cloud_sync_async.cond, cloud_sync_async.lock);
      if (!(op = cloud_sync_async.queue))
      {
         cloud_sync_async.worker = false;
         /* cloud_sync_deinit() may be waiting for this */
         scond_broadcast(cloud_sync_async.cond);
         slock_unlock(cloud_sync_async.lock);
         return;
      }
      if (!(cloud_sync_async.queue = op->next))
         cloud_sync_async.queue_tail = NULL;
      slock_unlock(cloud_sync_async.lock);

      op->next = NULL;
      cloud_sync_op_run(op);

      slock_lock(cloud_sync_async.lock);
      if (cloud_sync_async.done_tail)
         cloud_sync_async.done_tail->next = op;
      else
         cloud_sync_async.done = op;
      cloud_sync_async.done_tail = op;
      if (     op->type == CLOUD_SYNC_OP_END
            || (     op->type == CLOUD_SYNC_OP_BEGIN
                 && !(op->flags & CLOUD_SYNC_OP_FLG_SUCCESS)))
         cloud_sync_async.ended = true;
      slock_unlock(cloud_sync_async.lock);
   }
}

/* Queues the call for the worker, starting one if none runs. False
 * when it could not be queued: nothing was called and @file is still
 * the caller's. */
static bool cloud_sync_op_push(const cloud_sync_driver_t *driver,
      int type, const char *path, const char *local, RFILE *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   size_t           plen  = path  ? strlen(path)  + 1 : 0;
   size_t           llen  = local ? strlen(local) + 1 : 0;
   cloud_sync_op_t *op;
   bool             spawn = false;

   if (!cloud_sync_async.lock)
   {
      if (!(cloud_sync_async.lock = slock_new()))
         return false;
      if (!(cloud_sync_async.cond = scond_new()))
      {
         slock_free(cloud_sync_async.lock);
         cloud_sync_async.lock = NULL;
         return false;
      }
   }
   if (!(op = (cloud_sync_op_t*)malloc(sizeof(*op) + plen + llen)))
      return false;
   op->next      = NULL;
   op->driver    = driver;
   op->cb        = cb;
   op->user_data = user_data;
   op->file      = file;
   op->path      = plen ? (char*)(op + 1)    : NULL;
   op->local     = llen ? (char*)(op + 1) + plen : NULL;
   op->flags     = 0;
   op->type      = type;
   if (plen)
      memcpy(op->path, path, plen);
   if (llen)
      memcpy(op->local, local, llen);

   slock_lock(cloud_sync_async.lock);
   if (type == CLOUD_SYNC_OP_BEGIN)
      cloud_sync_async.ended = false;
   if (cloud_sync_async.queue_tail)
      cloud_sync_async.queue_tail->next = op;
   else
      cloud_sync_async.queue = op;
   cloud_sync_async.queue_tail = op;
   if (!cloud_sync_async.worker)
      spawn = cloud_sync_async.worker = true;
   else
      scond_signal(cloud_sync_async.cond);
   slock_unlock(cloud_sync_async.lock);

   if (spawn)
   {
      /* no worker ran, so the queue held only this call */
      sthread_t *thread = sthread_create(cloud_sync_worker, NULL);
      if (!thread)
      {
         slock_lock(cloud_sync_async.lock);
         cloud_sync_async.queue      = NULL;
         cloud_sync_async.queue_tail = NULL;
         cloud_sync_async.worker     = false;
         slock_unlock(cloud_sync_async.lock);
         free(op);
         return false;
      }
      sthread_detach(thread);
   }
   return true;
}

void cloud_sync_poll(void)
{
   cloud_sync_op_t *op;

   /* the lock is made on this thread, before any worker exists */
   if (!cloud_sync_async.lock)
      return;
   slock_lock(cloud_sync_async.lock);
   op                         = cloud_sync_async.done;
   cloud_sync_async.done      = NULL;
   cloud_sync_async.done_tail = NULL;
   slock_unlock(cloud_sync_async.lock);

   while (op)
   {
      cloud_sync_op_t *next = op->next;
      op->cb(op->user_data,
            (op->flags & CLOUD_SYNC_OP_FLG_PATH) ? op->path : NULL,
            (op->flags & CLOUD_SYNC_OP_FLG_SUCCESS) != 0, op->file);
      free(op);
      op = next;
   }
}

static void cloud_sync_op_list_free(cloud_sync_op_t *op)
{
   while (op)
   {
      cloud_sync_op_t *next = op->next;
      if (op->file)
         filestream_close(op->file);
      free(op);
      op = next;
   }
}

void cloud_sync_deinit(unsigned timeout_ms)
{
   cloud_sync_op_t *dropped;
   retro_time_t     deadline;

   if (!cloud_sync_async.lock)
      return;
   deadline = cpu_features_get_time_usec() + (retro_time_t)timeout_ms * 1000;

   slock_lock(cloud_sync_async.lock);
   /* calls not started are dropped; the worker leaves after its own */
   dropped                     = cloud_sync_async.queue;
   cloud_sync_async.queue      = NULL;
   cloud_sync_async.queue_tail = NULL;
   cloud_sync_async.ended      = true;
   scond_broadcast(cloud_sync_async.cond);
   while (cloud_sync_async.worker)
   {
      retro_time_t left = deadline - cpu_features_get_time_usec();
      if (left <= 0)
         break;
      scond_wait_timeout(cloud_sync_async.cond, cloud_sync_async.lock,
            (int64_t)left);
   }
   if (cloud_sync_async.done_tail)
   {
      cloud_sync_async.done_tail->next = dropped;
      dropped = cloud_sync_async.done;
   }
   cloud_sync_async.done      = NULL;
   cloud_sync_async.done_tail = NULL;
   slock_unlock(cloud_sync_async.lock);

   cloud_sync_op_list_free(dropped);
}

#define CLOUD_SYNC_BLOCKING(driver) \
   ((driver)->flags & CLOUD_SYNC_DRIVER_FLG_BLOCKING)
#else
void cloud_sync_poll(void) { }
void cloud_sync_deinit(unsigned timeout_ms) { (void)timeout_ms; }
#endif

bool cloud_sync_begin(cloud_sync_complete_handler_t cb, void *user_data)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;
   if (!driver || !driver->cloud_sync_begin)
      return false;
#ifdef HAVE_THREADS
   if (CLOUD_SYNC_BLOCKING(driver))
      return cloud_sync_op_push(driver, CLOUD_SYNC_OP_BEGIN,
            NULL, NULL, NULL, cb, user_data);
#endif
   return driver->cloud_sync_begin(cb, user_data);
}

bool cloud_sync_end(cloud_sync_complete_handler_t cb,
      void *user_data)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;
   if (!driver || !driver->cloud_sync_end)
      return false;
#ifdef HAVE_THREADS
   if (CLOUD_SYNC_BLOCKING(driver))
      return cloud_sync_op_push(driver, CLOUD_SYNC_OP_END,
            NULL, NULL, NULL, cb, user_data);
#endif
   return driver->cloud_sync_end(cb, user_data);
}

bool cloud_sync_read(const char *path, const char *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;
   if (!driver || !driver->cloud_sync_read)
      return false;
#ifdef HAVE_THREADS
   if (CLOUD_SYNC_BLOCKING(driver))
      return cloud_sync_op_push(driver, CLOUD_SYNC_OP_READ,
            path, file, NULL, cb, user_data);
#endif
   return driver->cloud_sync_read(path, file, cb, user_data);
}

bool cloud_sync_update(const char *path, RFILE *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;
   if (!driver || !driver->cloud_sync_update)
      return false;
#ifdef HAVE_THREADS
   if (CLOUD_SYNC_BLOCKING(driver))
      return cloud_sync_op_push(driver, CLOUD_SYNC_OP_UPDATE,
            path, NULL, file, cb, user_data);
#endif
   return driver->cloud_sync_update(path, file, cb, user_data);
}

bool cloud_sync_free(const char *path,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;
   if (!driver || !driver->cloud_sync_free)
      return false;
#ifdef HAVE_THREADS
   if (CLOUD_SYNC_BLOCKING(driver))
      return cloud_sync_op_push(driver, CLOUD_SYNC_OP_FREE,
            path, NULL, NULL, cb, user_data);
#endif
   return driver->cloud_sync_free(path, cb, user_data);
}
