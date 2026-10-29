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

#include <retro_atomic.h>

/* The worker hands calls and results over on lock-free stacks. */
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_HAS_PTR)
#define CLOUD_SYNC_ASYNC
#include <features/features_cpu.h>
#include <queues/mpsc_stack.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
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

#ifdef CLOUD_SYNC_ASYNC
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
   mpsc_stack_node_t             node;  /* first: the stacks hold ops */
   const cloud_sync_driver_t    *driver;
   cloud_sync_complete_handler_t cb;
   void                         *user_data;
   RFILE                        *file;  /* UPDATE's file; then the result's */
   char                         *path;  /* NULL for BEGIN and END */
   char                         *local; /* READ's local file */
   unsigned                      flags;
   int                           gen;   /* cloud_sync_async.gen when made */
   int                           type;
} cloud_sync_op_t;

static struct
{
   retro_eventcount_t wake;    /* the worker parks here */
   retro_eventcount_t idle;    /* cloud_sync_deinit() waits here */
   mpsc_stack_t       queue;   /* calls, for the worker */
   mpsc_stack_t       done;    /* results, for cloud_sync_poll */
   cloud_sync_op_t   *ready;   /* the poller's, oldest first */
   /* Moved on by cloud_sync_deinit(): a call from before it is dropped,
    * and the result of one still running is the worker's to dispose
    * of. */
   retro_atomic_int_t gen;
   retro_atomic_int_t worker;  /* a worker thread is running */
   bool               inited;  /* made on the calling thread */
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

static void cloud_sync_op_drop(cloud_sync_op_t *op)
{
   if (op->file)
      filestream_close(op->file);
   free(op);
}

static void cloud_sync_op_list_free(mpsc_stack_node_t *node)
{
   while (node)
   {
      mpsc_stack_node_t *next = node->next;
      cloud_sync_op_drop((cloud_sync_op_t*)node);
      node = next;
   }
}

/* Where the worker decides to leave; the async test queues a call
 * there. */
#ifdef CLOUD_SYNC_TEST_HOOKS
void cloud_sync_test_worker_leaving(void);
#define CLOUD_SYNC_WORKER_LEAVING() cloud_sync_test_worker_leaving()
#else
#define CLOUD_SYNC_WORKER_LEAVING() do { } while (0)
#endif

static bool cloud_sync_worker_has_work(int gen)
{
   return !mpsc_stack_empty(&cloud_sync_async.queue)
      || retro_atomic_load_acquire_int(&cloud_sync_async.gen) != gen;
}

static void cloud_sync_worker(void *data)
{
   mpsc_stack_node_t *calls = NULL; /* taken from the queue, oldest first */
   int                gen   = retro_atomic_load_acquire_int(
         &cloud_sync_async.gen);
   bool               ended = false;
   (void)data;

   for (;;)
   {
      cloud_sync_op_t *op;

      if (!calls)
         calls = mpsc_stack_reverse(mpsc_stack_drain(&cloud_sync_async.queue));
      if (!calls)
      {
         int key;
         if (ended || retro_atomic_load_acquire_int(&cloud_sync_async.gen) != gen)
         {
            /* Leaving: a call queued since the check above either sees
             * no worker and starts one, or is taken here. */
            CLOUD_SYNC_WORKER_LEAVING();
            retro_atomic_exchange_int(&cloud_sync_async.worker, 0);
            retro_atomic_thread_fence_seq_cst();
            if (     !mpsc_stack_empty(&cloud_sync_async.queue)
                  && retro_atomic_cas_int(&cloud_sync_async.worker, 0, 1))
               continue;
            retro_eventcount_notify(&cloud_sync_async.idle);
            return;
         }
         key = retro_eventcount_prepare_wait(&cloud_sync_async.wake);
         if (cloud_sync_worker_has_work(gen))
            retro_eventcount_cancel_wait(&cloud_sync_async.wake);
         else
            retro_eventcount_commit_wait(&cloud_sync_async.wake, key);
         continue;
      }

      op    = (cloud_sync_op_t*)calls;
      calls = calls->next;
      /* Queued before cloud_sync_deinit(): dropped, not run. */
      if (op->gen != retro_atomic_load_acquire_int(&cloud_sync_async.gen))
      {
         cloud_sync_op_drop(op);
         continue;
      }
      gen = op->gen;
      cloud_sync_op_run(op);
      if (op->type == CLOUD_SYNC_OP_END)
         ended = true;
      else if (op->type == CLOUD_SYNC_OP_BEGIN)
         ended = !(op->flags & CLOUD_SYNC_OP_FLG_SUCCESS);

      /* Outlived the exit wait: nobody polls for it. One that slips
       * past this check is dropped by the poll. */
      if (op->gen != retro_atomic_load_acquire_int(&cloud_sync_async.gen))
         cloud_sync_op_drop(op);
      else
         mpsc_stack_push(&cloud_sync_async.done, &op->node);
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
   sthread_t       *thread;

   if (!cloud_sync_async.inited)
   {
      if (!retro_eventcount_init(&cloud_sync_async.wake))
         return false;
      if (!retro_eventcount_init(&cloud_sync_async.idle))
      {
         retro_eventcount_free(&cloud_sync_async.wake);
         return false;
      }
      mpsc_stack_init(&cloud_sync_async.queue);
      mpsc_stack_init(&cloud_sync_async.done);
      cloud_sync_async.inited = true;
   }
   if (!(op = (cloud_sync_op_t*)malloc(sizeof(*op) + plen + llen)))
      return false;
   op->driver    = driver;
   op->cb        = cb;
   op->user_data = user_data;
   op->file      = file;
   op->path      = plen ? (char*)(op + 1)    : NULL;
   op->local     = llen ? (char*)(op + 1) + plen : NULL;
   op->flags     = 0;
   op->gen       = retro_atomic_load_acquire_int(&cloud_sync_async.gen);
   op->type      = type;
   if (plen)
      memcpy(op->path, path, plen);
   if (llen)
      memcpy(op->local, local, llen);

   mpsc_stack_push(&cloud_sync_async.queue, &op->node);
   /* Against the leaving worker's own: one of the two sees the other. */
   retro_atomic_thread_fence_seq_cst();
   if (!retro_atomic_cas_int(&cloud_sync_async.worker, 0, 1))
   {
      retro_eventcount_notify(&cloud_sync_async.wake);
      return true;
   }
   if ((thread = sthread_create(cloud_sync_worker, NULL)))
   {
      sthread_detach(thread);
      return true;
   }
   /* No worker ran, so the queue held only this call. */
   mpsc_stack_drain(&cloud_sync_async.queue);
   retro_atomic_exchange_int(&cloud_sync_async.worker, 0);
   free(op);
   return false;
}

void cloud_sync_poll(cloud_sync_poll_budget_t within, void *budget)
{
   if (!cloud_sync_async.inited)
      return;

   /* The oldest result always runs; more only while @within allows. */
   for (;;)
   {
      cloud_sync_op_t *op;

      if (!cloud_sync_async.ready)
         cloud_sync_async.ready = (cloud_sync_op_t*)mpsc_stack_reverse(
               mpsc_stack_drain(&cloud_sync_async.done));
      if (!(op = cloud_sync_async.ready))
         return;
      cloud_sync_async.ready = (cloud_sync_op_t*)op->node.next;

      if (op->gen != retro_atomic_load_acquire_int(&cloud_sync_async.gen))
      {
         cloud_sync_op_drop(op);
         continue;
      }
      op->cb(op->user_data,
            (op->flags & CLOUD_SYNC_OP_FLG_PATH) ? op->path : NULL,
            (op->flags & CLOUD_SYNC_OP_FLG_SUCCESS) != 0, op->file);
      free(op);

      if (within && !within(budget, 0, 0))
         return;
   }
}

void cloud_sync_deinit(unsigned timeout_ms)
{
   retro_time_t deadline;

   if (!cloud_sync_async.inited)
      return;
   deadline = cpu_features_get_time_usec() + (retro_time_t)timeout_ms * 1000;

   /* Calls not started are dropped; the worker leaves after its own,
    * which it disposes of itself if it finishes past the wait. */
   retro_atomic_fetch_add_int(&cloud_sync_async.gen, 1);
   cloud_sync_op_list_free(mpsc_stack_drain(&cloud_sync_async.queue));
   retro_eventcount_notify(&cloud_sync_async.wake);
   while (retro_atomic_load_acquire_int(&cloud_sync_async.worker))
   {
      retro_time_t left;
      int          key = retro_eventcount_prepare_wait(&cloud_sync_async.idle);
      if (!retro_atomic_load_acquire_int(&cloud_sync_async.worker))
      {
         retro_eventcount_cancel_wait(&cloud_sync_async.idle);
         break;
      }
      if ((left = deadline - cpu_features_get_time_usec()) <= 0)
      {
         retro_eventcount_cancel_wait(&cloud_sync_async.idle);
         break;
      }
      retro_eventcount_commit_wait_timeout(&cloud_sync_async.idle, key,
            (int64_t)left);
   }
   cloud_sync_op_list_free((mpsc_stack_node_t*)cloud_sync_async.ready);
   cloud_sync_async.ready = NULL;
   cloud_sync_op_list_free(mpsc_stack_drain(&cloud_sync_async.done));
}

#define CLOUD_SYNC_BLOCKING(driver) \
   ((driver)->flags & CLOUD_SYNC_DRIVER_FLG_BLOCKING)
#else
void cloud_sync_poll(cloud_sync_poll_budget_t within, void *budget)
{
   (void)within;
   (void)budget;
}
void cloud_sync_deinit(unsigned timeout_ms) { (void)timeout_ms; }
#endif

void cloud_sync_capture(void)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;
   if (driver && driver->cloud_sync_capture)
      driver->cloud_sync_capture();
}

bool cloud_sync_begin(cloud_sync_complete_handler_t cb, void *user_data)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;
   if (!driver || !driver->cloud_sync_begin)
      return false;
#ifdef CLOUD_SYNC_ASYNC
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
#ifdef CLOUD_SYNC_ASYNC
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
#ifdef CLOUD_SYNC_ASYNC
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
#ifdef CLOUD_SYNC_ASYNC
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
#ifdef CLOUD_SYNC_ASYNC
   if (CLOUD_SYNC_BLOCKING(driver))
      return cloud_sync_op_push(driver, CLOUD_SYNC_OP_FREE,
            path, NULL, NULL, cb, user_data);
#endif
   return driver->cloud_sync_free(path, cb, user_data);
}
