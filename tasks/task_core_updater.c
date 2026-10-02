/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C)      2019 - James Leaver
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <ctype.h>
#include <boolean.h>

#include <string/stdstring.h>
#include <lists/string_list.h>
#include <file/file_path.h>
#include <net/net_http.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>

#include "task_file_transfer.h"
#include "tasks_internal.h"

#include "../configuration.h"
#include "../retroarch.h"
#include "../command.h"
#include "../msg_hash.h"
#include "../verbosity.h"
#include "../core_updater_list.h"

#if defined(ANDROID)
#include "../file_path_special.h"
#include "../play_feature_delivery/play_feature_delivery.h"
#endif

#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
#include "../menu/menu_entries.h"
#include "../menu/menu_driver.h"
#endif

/* Bytes hashed per work item of the shared I/O window.  Matches the
 * read size inside intfstream_crc_step(). */
#define CORE_CRC_CHUNK      (256 * 1024)

typedef struct
{
   intfstream_t *file;
   uint32_t      accumulator;
   bool          active;
} core_crc_slice_t;

/* Get core updater list */
enum core_updater_list_status
{
   CORE_UPDATER_LIST_BEGIN = 0,
   CORE_UPDATER_LIST_WAIT,
   CORE_UPDATER_LIST_PARSE,
   CORE_UPDATER_LIST_END
};

typedef struct core_updater_list_handle
{
   core_updater_list_t* core_list;
   /* The list is built here, across ticks, and swapped into
    * core_list by the task's callback on the main thread, so
    * nothing reading core_list ever sees it half-built */
   core_updater_list_t* parse_list;
   retro_task_t *http_task;
   http_transfer_data_t *http_data;
   enum core_updater_list_status status;
   /* One reference for the task, one for the HTTP callback while a
    * transfer is pushed: a task cancelled mid-transfer retires before
    * the transfer does, and the callback still writes the payload
    * here.  The last reference frees the handle. */
   retro_atomic_int_t refs;
   bool refresh_menu;
   /* Update Installed Cores' fetch: keep installed cores only, and
    * read core info for those alone */
   bool installed_only;
   /* Set by the HTTP callback on the main thread, polled by the
    * worker: a release store after the payload (http_data,
    * http_task_success) and an acquire load before reading it. */
   retro_atomic_int_t http_task_complete;
   bool http_task_success;
   /* Captured on the main thread at push: the handler runs on the
    * threaded task queue's worker and parses the core list against
    * these, never against the live settings. */
   char dir_libretro[DIR_MAX_LENGTH];
   char path_libretro_info[PATH_MAX_LENGTH];
   char network_buildbot_url[PATH_MAX_LENGTH];
} core_updater_list_handle_t;

/* Download core */
enum core_updater_download_status
{
   CORE_UPDATER_DOWNLOAD_BEGIN = 0,
   CORE_UPDATER_DOWNLOAD_START_BACKUP,
   CORE_UPDATER_DOWNLOAD_WAIT_BACKUP,
   CORE_UPDATER_DOWNLOAD_START_TRANSFER,
   CORE_UPDATER_DOWNLOAD_WAIT_TRANSFER,
   CORE_UPDATER_DOWNLOAD_WAIT_DECOMPRESS,
   CORE_UPDATER_DOWNLOAD_START_INSTALL,
   CORE_UPDATER_DOWNLOAD_WAIT_INSTALL,
   CORE_UPDATER_DOWNLOAD_ERROR,
   CORE_UPDATER_DOWNLOAD_END
};

/* Completion of a sub-task that reports through a callback, shared
 * between the handler waiting on it and that callback.  Each holds a
 * reference, so it outlives whichever side lets go first: a task
 * cancelled mid-wait, or a sub-task retired before the handler's
 * next tick.  A waiter that lets go while the sub-task still writes
 * into a list it owns hands the list over in orphan_list, and the
 * last reference frees it with the record.  The callback copies the
 * error the sub-task finished with, if any, into 'error' before it
 * publishes 'complete'. */
typedef struct
{
   core_updater_list_t *orphan_list;
   char *error;
   retro_atomic_int_t refs;
   retro_atomic_int_t complete;
} core_updater_sub_task_done_t;

typedef struct core_updater_download_handle
{
   char *path_dir_libretro;
   char *path_dir_core_assets;
   char *remote_filename;
   char *remote_core_path;
   char *local_download_path;
   char *local_core_path;
   /* Staged install only: the core's own staging directory beside the
    * cores, and the new core extracted into it until the backup task
    * moves the old one out and it in */
   char *staging_dir;
   char *staged_core_path;
   char *display_name;
   retro_task_t *http_task;
   retro_task_t *decompress_task;
   retro_task_t *backup_task;
   core_updater_sub_task_done_t *backup_done;
   size_t auto_backup_history_size;
   core_crc_slice_t crc_slice;
   uint32_t local_crc;
   uint32_t remote_crc;
   enum core_updater_download_status status;
   /* One reference for the task, one each for the HTTP and
    * decompress callbacks while those tasks are pushed: a download
    * cancelled mid-transfer retires first, and the callbacks still
    * write here.  The last reference frees the handle. */
   retro_atomic_int_t refs;
   /* Set by the HTTP and decompress callbacks on the main thread,
    * polled by the worker: release stores, acquire loads. */
   retro_atomic_int_t http_task_complete;
   retro_atomic_int_t decompress_task_complete;
   bool crc_match;
   /* Written by the HTTP callback before it publishes
    * http_task_complete; the worker moves itself to the error state
    * on seeing it, so only the worker ever writes status. */
   bool http_task_error;
   bool auto_backup;
   /* 'Compress Automatic Core Backups'; off stages the install so the
    * replaced core can be moved into the backups instead */
   bool backup_compress;
   bool backup_enabled;
} core_updater_download_handle_t;

/* One installed core's CRC, against the file metadata it was
 * hashed at (see 'Installed core CRC cache') */
typedef struct
{
   char *name;      /* the core's listing filename */
   int64_t size;
   int64_t mtime;
   uint32_t crc;
   bool seen;       /* a core reached this run */
} core_crc_cache_entry_t;

typedef struct
{
   core_crc_cache_entry_t *entries;
   char *path;      /* NULL: no cache */
   size_t count;
   size_t cap;
   bool dirty;
} core_crc_cache_t;

/* Update installed cores */
enum update_installed_cores_status
{
   UPDATE_INSTALLED_CORES_BEGIN = 0,
   UPDATE_INSTALLED_CORES_WAIT_LIST,
   UPDATE_INSTALLED_CORES_ITERATE,
   UPDATE_INSTALLED_CORES_UPDATE_CORE,
   UPDATE_INSTALLED_CORES_WAIT_DOWNLOAD,
   UPDATE_INSTALLED_CORES_END
};

typedef struct update_installed_cores_handle
{
   char *path_dir_libretro;
   char *path_dir_core_assets;
   core_updater_list_t* core_list;
   core_crc_cache_t crc_cache;
   /* Completion of the child list fetch / core download this task
    * is waiting on, shared with the child's callback; NULL when
    * nothing is pending.  The child retro_task_t pointers are
    * deliberately *not* retained: task_queue frees a finished task
    * in the same gather pass that ran its handler, so a stored
    * pointer can dangle before this task is stepped again.  The
    * callbacks run on the main thread at retirement while this
    * handler polls from the worker: a release store in the
    * callback, an acquire load here, which also publishes the core
    * list the list task filled in. */
   core_updater_sub_task_done_t *list_done;
   core_updater_sub_task_done_t *download_done;
   size_t auto_backup_history_size;
   size_t list_size;
   size_t list_index;
   size_t installed_index;
   core_crc_slice_t crc_slice;
   unsigned num_updated;
   unsigned num_locked;
   enum update_installed_cores_status status;
   bool auto_backup;
   /* 'Compress Automatic Core Backups', captured at the push */
   bool auto_backup_compress;
   /* The buildbot listing arrived and parsed.  core_list holds only
    * the installed cores in it, so an empty list is no failure. */
   bool list_fetched;
   /* The task title currently reads 'Scanning cores' */
   bool title_scanning;
   /* Captured on the main thread when this task is pushed: its
    * handler runs on the worker and pushes the list task from
    * there, so the list task's captures come from here, not from
    * a settings read on the worker. */
   char dir_libretro[DIR_MAX_LENGTH];
   char path_libretro_info[PATH_MAX_LENGTH];
   char network_buildbot_url[PATH_MAX_LENGTH];
} update_installed_cores_handle_t;

#if defined(ANDROID)
/* Play feature delivery core install */
enum play_feature_delivery_install_task_status
{
   PLAY_FEATURE_DELIVERY_INSTALL_BEGIN = 0,
   PLAY_FEATURE_DELIVERY_INSTALL_WAIT,
   PLAY_FEATURE_DELIVERY_INSTALL_END
};

typedef struct play_feature_delivery_install_handle
{
   char *core_filename;
   char *local_core_path;
   char *backup_core_path;
   char *display_name;
   enum play_feature_delivery_install_task_status status;
   bool success;
   bool core_already_installed;
} play_feature_delivery_install_handle_t;

/* Play feature delivery switch installed cores */
enum play_feature_delivery_switch_cores_task_status
{
   PLAY_FEATURE_DELIVERY_SWITCH_CORES_BEGIN = 0,
   PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE,
   PLAY_FEATURE_DELIVERY_SWITCH_CORES_INSTALL_CORE,
   PLAY_FEATURE_DELIVERY_SWITCH_CORES_WAIT_INSTALL,
   PLAY_FEATURE_DELIVERY_SWITCH_CORES_END
};

typedef struct play_feature_delivery_switch_cores_handle
{
   char *path_dir_libretro;
   char *path_libretro_info;
   char *err_msg;
   core_updater_list_t* core_list;
   /* Completion of the install task this task is waiting on, shared
    * with its callback; NULL when nothing is pending.  The install
    * retro_task_t is deliberately not retained: the queue frees a
    * finished task in the same gather pass that retires it, so a
    * stored pointer can dangle before this task is stepped again. */
   core_updater_sub_task_done_t *install_done;
   size_t list_size;
   size_t list_index;
   size_t installed_index;
   enum play_feature_delivery_switch_cores_task_status status;
} play_feature_delivery_switch_cores_handle_t;
#endif

/*********************/
/* Utility functions */
/*********************/

/* Sliced CRC32 of a core file.
 *
 * Each CORE_CRC_CHUNK is one work item of the shared per-frame I/O
 * window (task_nbio_slice_*), so hashing a core of any size costs a
 * tick no more than the window allows, and shares that window with
 * every other budgeted task in the same gather.
 *
 * update_installed_cores hashes every installed core to find out
 * which ones changed, so a repeat run where nothing has changed still
 * reads every installed core once.  Skipping that read needs a
 * (path, size, mtime) cache, which needs an mtime from the libretro
 * VFS first; size alone is not a safe key, since a rebuilt core of
 * identical size would be silently skipped. */

static void task_core_updater_crc_reset(core_crc_slice_t *slice)
{
   if (slice->file)
   {
      intfstream_close(slice->file);
      free(slice->file);
      slice->file = NULL;
   }
   slice->accumulator = 0;
   slice->active      = false;
}

/* Advance the CRC of @core_path within @budget, an open slice of the
 * shared I/O window.
 *
 * Returns true when the CRC is complete, writing it to @crc; false
 * means the window is spent and the caller resumes next tick.  An
 * unreadable file completes with a CRC of 0, which callers treat as
 * "no local core to compare against". */
static bool task_core_updater_crc_step(core_crc_slice_t *slice,
      const char *core_path, uint32_t *crc, nbio_budget_t *budget)
{
   while (task_nbio_slice_within_budget(budget, 0, 0))
   {
      int64_t hashed;

      if (!slice->active)
      {
         slice->accumulator = 0;
         /* FREQUENT_ACCESS: mapped where the VFS can, and each
          * crc_step() then folds from the mapping without a read. */
         if (!(slice->file = intfstream_open_file(core_path,
                     RETRO_VFS_FILE_ACCESS_READ,
                     RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)))
         {
            *crc = 0;
            return true;
         }
         intfstream_rewind(slice->file);
         slice->active = true;
      }

      if ((hashed = intfstream_crc_step(slice->file,
                  &slice->accumulator, CORE_CRC_CHUNK)) > 0)
         continue;

      /* 0 is end of stream, negative is a read error; a partial
       * hash is not a usable CRC, so both yield 0 like the
       * open-failure path. */
      *crc = (hashed == 0) ? slice->accumulator : 0;
      task_core_updater_crc_reset(slice);
      return true;
   }

   return false;
}

struct core_updater_sub_task_probe
{
   retro_task_t *target;
   uint8_t flags;
   int8_t progress;
};

static bool core_updater_sub_task_finder(retro_task_t *task, void *user_data)
{
   struct core_updater_sub_task_probe *probe =
         (struct core_updater_sub_task_probe*)user_data;

   if (task != probe->target)
      return false;

   probe->flags    = task_get_flags(task);
   probe->progress = task_get_progress(task);
   return true;
}

/* The threaded queue frees a sub-task on the main thread right after
 * its callback, while this handler runs on the worker, so there the
 * sub-task is only read through find(), which holds the queue locks.
 * Callers check the callback's complete flag first; a reused address
 * can then cost one wrong progress value at most.
 *
 * The unthreaded queue runs handlers in list order and only retires
 * finished tasks after the whole pass.  A sub-task is pushed from
 * this handler's own tick, so it always sits ahead of the updater
 * and has already ticked - alive, at worst finished but not yet
 * freed - when the updater reads it; by the next pass its callback
 * has set the complete flag and the updater stops looking.  find()
 * cannot be used there: the gather detaches the running list before
 * it ticks anything, so no sub-task is findable from inside a
 * handler and progress would never be copied. */
static bool core_updater_sub_task_running(retro_task_t *sub_task,
      int8_t *progress)
{
   task_finder_data_t find_data;
   struct core_updater_sub_task_probe probe;

   if (!task_queue_is_threaded())
   {
      if (task_get_flags(sub_task) & RETRO_TASK_FLG_FINISHED)
         return false;
      *progress = task_get_progress(sub_task);
      return true;
   }

   probe.target       = sub_task;
   probe.flags        = 0;
   probe.progress     = 0;
   find_data.func     = core_updater_sub_task_finder;
   find_data.userdata = &probe;

   if (     !task_queue_find(&find_data)
         || (probe.flags & RETRO_TASK_FLG_FINISHED))
      return false;

   *progress = probe.progress;
   return true;
}

static void core_updater_sub_task_done_release(
      core_updater_sub_task_done_t *done)
{
   if (done && retro_atomic_fetch_sub_int(&done->refs, 1) == 1)
   {
      core_updater_list_free(done->orphan_list);
      free(done->error);
      free(done);
   }
}

/* One reference for the waiting handler, one for the sub-task's
 * callback, which may run before the push returns. */
static core_updater_sub_task_done_t *core_updater_sub_task_done_new(void)
{
   core_updater_sub_task_done_t *done =
         (core_updater_sub_task_done_t*)malloc(sizeof(*done));
   if (done)
   {
      done->orphan_list = NULL;
      done->error       = NULL;
      retro_atomic_int_init(&done->refs, 2);
      retro_atomic_int_init(&done->complete, 0);
   }
   return done;
}

/* Completion for the callback side of a parent waiting on it,
 * with the error the sub-task finished with (NULL or empty for
 * none) */
static void core_updater_sub_task_done_signal(
      core_updater_sub_task_done_t *done, const char *err)
{
   if (err && *err)
      done->error = strdup(err);
   retro_atomic_store_release_int(&done->complete, 1);
   core_updater_sub_task_done_release(done);
}

/* The backup task's completion, at its retirement: before the task
 * is freed, so a handler that sees 'complete' never reads it again. */
static void cb_task_core_updater_backup(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   core_updater_sub_task_done_signal(
         (core_updater_sub_task_done_t*)user_data, err);
}

/*************************/
/* Get core updater list */
/*************************/

static void free_core_updater_list_handle(
      core_updater_list_handle_t *list_handle)
{
   if (list_handle->http_data)
   {
      /* since we took ownership, we have to destroy it ourself */
      if (list_handle->http_data->data)
         free(list_handle->http_data->data);
      /* the headers list task_http.c attaches is part of that
       * ownership */
      free(list_handle->http_data->headers);

      free(list_handle->http_data);
   }

   core_updater_list_free(list_handle->parse_list);

   free(list_handle);
   list_handle = NULL;
}

static void core_updater_list_handle_release(
      core_updater_list_handle_t *list_handle)
{
   if (     list_handle
         && retro_atomic_fetch_sub_int(&list_handle->refs, 1) == 1)
      free_core_updater_list_handle(list_handle);
}

static void cb_http_task_core_updater_get_list(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   file_transfer_t *transf    = (file_transfer_t*)user_data;
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   core_updater_list_handle_t *list_handle = NULL;
   bool ret                   = data && (!err || !*err);

   if (transf)
   {
      if ((list_handle = (core_updater_list_handle_t*)transf->user_data))
      {
         task_set_data(task, NULL); /* going to pass ownership to list_handle */

         list_handle->http_data         = data;
         list_handle->http_task_success = ret;
         retro_atomic_store_release_int(&list_handle->http_task_complete, 1);
      }
   }

   /* Log any error messages */
   if (!ret)
      RARCH_ERR("[Core Updater] Download of core list \"%s\" failed: %s.\n",
            (transf ? transf->path: "unknown"),
            (err ? err : "unknown"));

   if (transf)
      free(transf);

   /* Last: the list task may have retired already */
   core_updater_list_handle_release(list_handle);
}

static void task_core_updater_get_list_handler(retro_task_t *task)
{
   uint8_t flg;
   core_updater_list_handle_t *list_handle = NULL;

   if (!task)
      goto task_finished;

   list_handle = (core_updater_list_handle_t*)task->state;
   flg         = task_get_flags(task);

   if (!list_handle || ((flg & RETRO_TASK_FLG_CANCELLED) > 0))
      goto task_finished;

   switch (list_handle->status)
   {
      case CORE_UPDATER_LIST_BEGIN:
         {
            char buildbot_url[PATH_MAX_LENGTH];
            file_transfer_t *transf = NULL;
            const char *net_buildbot_url;

            /* Get core listing URL from the push-time capture */
            net_buildbot_url = list_handle->network_buildbot_url;

            if (!net_buildbot_url || !*net_buildbot_url)
               goto task_finished;

            fill_pathname_join_special(
                  buildbot_url,
                  net_buildbot_url,
                  ".index-extended",
                  sizeof(buildbot_url));

            /* URL-encode in place using a stack buffer
             * instead of heap-allocating a copy */
            {
               char tmp_url[PATH_MAX_LENGTH];
               strlcpy(tmp_url, buildbot_url, sizeof(tmp_url));
               buildbot_url[0] = '\0';
               net_http_urlencode_full(
                     buildbot_url, tmp_url, sizeof(buildbot_url));
            }

            if (!*buildbot_url)
               goto task_finished;

            /* Configure file transfer object */
            if (!(transf = (file_transfer_t*)calloc(1,
                        sizeof(file_transfer_t))))
               goto task_finished;

            strlcpy(transf->path, buildbot_url, sizeof(transf->path));
            transf->user_data = (void*)list_handle;

            /* Push HTTP transfer task
             * > The callback's reference is taken before the
             *   push, which it may outrun; a failed push runs no
             *   callback, so it is dropped again here along with
             *   the transfer object */
            retro_atomic_fetch_add_int(&list_handle->refs, 1);
            if (!(list_handle->http_task = (retro_task_t*)
                     task_push_http_transfer_file(
                        buildbot_url, true, NULL,
                        cb_http_task_core_updater_get_list, transf)))
            {
               free(transf);
               retro_atomic_fetch_sub_int(&list_handle->refs, 1);
            }

            /* Start waiting for HTTP transfer to complete */
            list_handle->status = CORE_UPDATER_LIST_WAIT;
         }
         break;
      case CORE_UPDATER_LIST_WAIT:
         {
            int8_t progress;

            /* If HTTP task is NULL, then it either finished
             * or an error occurred - in either case,
             * just move on to the next state */
            if (!list_handle->http_task)
               retro_atomic_store_release_int(
                     &list_handle->http_task_complete, 1);

            /* Wait for task_push_http_transfer_file()
             * callback to trigger */
            if (retro_atomic_load_acquire_int(
                     &list_handle->http_task_complete))
            {
               /* Hand the response body to the private list to
                * parse in place; a failed transfer, or one with
                * nothing in it, publishes an empty list */
               if (     list_handle->http_task_success
                     && list_handle->http_data
                     && core_updater_list_parse_network_take(
                           list_handle->parse_list,
                           (char*)list_handle->http_data->data,
                           list_handle->http_data->len,
                           list_handle->installed_only
                              ? CORE_UPDATER_LIST_PARSE_INSTALLED_ONLY
                              : 0))
                  list_handle->status = CORE_UPDATER_LIST_PARSE;
               else
                  list_handle->status = CORE_UPDATER_LIST_END;

               if (list_handle->http_data)
                  list_handle->http_data->data = NULL; /* taken */

               if (!list_handle->http_task_success)
               {
                  /* Notify user of error via task title */
                  task_free_title(task);
                  task_set_title(task,
                        strdup(msg_hash_to_str(MSG_CORE_LIST_FAILED)));
               }
            }
            /* If HTTP task is running, copy current
             * progress value to *this* task */
            else if (core_updater_sub_task_running(
                     list_handle->http_task, &progress))
               task_set_progress(task, progress);
         }
         break;
      case CORE_UPDATER_LIST_PARSE:
         {
            /* One listing line per work item of the shared I/O
             * window: each costs a stat and a core info read */
            bool done;
            nbio_budget_t budget;

            task_nbio_slice_open(&budget);
            done = core_updater_list_parse_network_step(
                  list_handle->parse_list,
                  list_handle->dir_libretro,
                  list_handle->path_libretro_info,
                  list_handle->network_buildbot_url,
                  task_nbio_slice_within_budget, &budget);
            task_nbio_slice_close(&budget);

            if (done)
               list_handle->status = CORE_UPDATER_LIST_END;
         }
         break;
      case CORE_UPDATER_LIST_END:
         /* The list is published, and the menu refreshed, by the
          * task's callback: the main thread. */
         /* fall-through */
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:
   if (task)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

/* Runs at retrieval on the main thread, after the task callback.
 * The handle must stay attached to task->state until here: the
 * finders in this file dereference task->state on a task that is
 * still findable, and cb_task_core_updater_get_list() reads
 * refresh_menu through it to clear the menu refresh flags - both on
 * a task the worker has already marked FINISHED.  The task stays
 * visible to retro_task_threaded_find() until it is retired, so the
 * handle it exposes has to stay alive exactly that long, and this
 * cleanup is the first point past both users.  A transfer still in
 * flight (the task was cancelled) holds the other reference. */
static void task_core_updater_get_list_cleanup(retro_task_t *task)
{
   core_updater_list_handle_release(
         (core_updater_list_handle_t*)task->state);
   task->state = NULL;
}

static bool task_core_updater_get_list_finder(retro_task_t *task, void *user_data)
{
   core_updater_list_handle_t *list_handle = NULL;

   if (!task || !user_data)
      return false;

   if (task->handler != task_core_updater_get_list_handler)
      return false;

   if (!(list_handle = (core_updater_list_handle_t*)task->state))
      return false;

   return ((uintptr_t)user_data == (uintptr_t)list_handle->core_list);
}

/* Signals completion of the core list fetch to a parent
 * 'update installed cores' task, if there is one, through
 * the completion record the two share - never through the
 * parent's handle, which a cancelled parent has already
 * freed by the time this runs. The
 * parent must never poll task_get_flags() on the child
 * instead: a finished task is retired and freed inside the
 * same task_queue gather pass that ran its handler, so with
 * threaded tasks disabled the parent may not get to observe
 * RETRO_TASK_FLG_FINISHED before the memory is freed. */
static void cb_task_core_updater_get_list(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   core_updater_sub_task_done_t *list_done =
         (core_updater_sub_task_done_t*)user_data;
   core_updater_list_handle_t *list_handle =
         (core_updater_list_handle_t*)task->state;

   /* Publish the list built on the worker.  Only a parse that ran
    * to the end is published as is; a fetch that failed, was
    * cancelled or stopped part-way publishes an empty list.  This
    * runs before the parent is signalled, and a cancelled parent's
    * core_list stays alive in list_done until then. */
   if (list_handle)
   {
      if (list_handle->status != CORE_UPDATER_LIST_END)
         core_updater_list_reset(list_handle->parse_list);
      core_updater_list_swap(list_handle->core_list,
            list_handle->parse_list);
   }

#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
   /* The main thread, at task retrieval: menu flags are a plain
    * read-modify-write, so every writer must be here, racing
    * nothing. */
   {
      struct menu_state *menu_st = menu_state_get_ptr();
      if (list_handle && menu_st)
      {
         if (list_handle->refresh_menu)
            menu_st->flags &= ~MENU_ST_FLAG_ENTRIES_NONBLOCKING_REFRESH;
         else
            menu_st->flags &= ~MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      }
   }
#endif

   if (list_done)
      core_updater_sub_task_done_signal(list_done, err);
}

/* The push with the three paths as values: reads no live settings,
 * so the worker that pushes the list task from the update-installed
 * handler can call it with its own push-time captures. */
static void *task_push_get_core_updater_list_captured(
      core_updater_list_t* core_list, bool mute, bool refresh_menu,
      bool installed_only,
      core_updater_sub_task_done_t *list_done,
      const char *dir_libretro, const char *path_libretro_info,
      const char *network_buildbot_url)
{
   task_finder_data_t find_data;
   retro_task_t *task                      = NULL;
   core_updater_list_handle_t *list_handle = (core_updater_list_handle_t*)
         calloc(1, sizeof(core_updater_list_handle_t));

#if defined(ANDROID)
   /* Regular core updater is disabled in
    * Play Store builds */
   if (play_feature_delivery_enabled())
      goto error;
#endif

   /* Sanity check */
   if (!core_list || !list_handle)
      goto error;

   if (!(list_handle->parse_list = core_updater_list_init()))
      goto error;

   /* Configure handle */
   retro_atomic_int_init(&list_handle->refs, 1);
   list_handle->core_list          = core_list;
   list_handle->refresh_menu       = refresh_menu;
   list_handle->installed_only     = installed_only;
   list_handle->http_task          = NULL;
   retro_atomic_store_release_int(&list_handle->http_task_complete, 0);
   list_handle->http_task_success  = false;
   list_handle->http_data          = NULL;
   strlcpy(list_handle->dir_libretro, dir_libretro,
         sizeof(list_handle->dir_libretro));
   strlcpy(list_handle->path_libretro_info, path_libretro_info,
         sizeof(list_handle->path_libretro_info));
   strlcpy(list_handle->network_buildbot_url, network_buildbot_url,
         sizeof(list_handle->network_buildbot_url));
   list_handle->status             = CORE_UPDATER_LIST_BEGIN;

   /* Concurrent downloads of the buildbot core listing
    * to the same core_updater_list_t object are not
    * allowed */
   find_data.func     = task_core_updater_get_list_finder;
   find_data.userdata = (void*)core_list;

   if (task_queue_find(&find_data))
      goto error;

   /* Create task */
   if (!(task = task_init()))
      goto error;

   /* Configure task */
   task->handler          = task_core_updater_get_list_handler;
   task->cleanup          = task_core_updater_get_list_cleanup;
   task->state            = list_handle;
   task->title            = strdup(msg_hash_to_str(MSG_FETCHING_CORE_LIST));
   task->progress         = 0;
   task->progress_cb      = task_window_progress_cb;
   task->callback         = cb_task_core_updater_get_list;
   task->user_data        = (void*)list_done;
   task->flags           |=  RETRO_TASK_FLG_ALTERNATIVE_LOOK;
   if (mute)
      task->flags        |=  RETRO_TASK_FLG_MUTE;
   else
      task->flags        &= ~RETRO_TASK_FLG_MUTE;

   /* Push task */
   task_queue_push(task);

   return task;

error:

   /* Clean up task */
   if (task)
   {
      free(task);
      task = NULL;
   }

   /* Clean up handle */
   if (list_handle)
      free_core_updater_list_handle(list_handle);

   return NULL;
}

/* The live push: main-thread callers, reading the settings at the
 * moment of the push. */
static void *task_push_get_core_updater_list_internal(
      core_updater_list_t* core_list, bool mute, bool refresh_menu)
{
   settings_t *settings = config_get_ptr();
   return task_push_get_core_updater_list_captured(
         core_list, mute, refresh_menu, false, NULL,
         settings->paths.directory_libretro,
         settings->paths.path_libretro_info,
         settings->paths.network_buildbot_url);
}

void *task_push_get_core_updater_list(
      core_updater_list_t* core_list, bool mute, bool refresh_menu)
{
   return task_push_get_core_updater_list_internal(
         core_list, mute, refresh_menu);
}

/*****************/
/* Download core */
/*****************/

static void cb_task_core_updater_download(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   /* Reload core info files
    * > This must be done on the main thread
    * > Forced: a core file changed on disk */
   bool refresh                            = true;
   core_updater_sub_task_done_t *download_done =
         (core_updater_sub_task_done_t*)user_data;

   /* Signal completion to the parent 'update installed
    * cores' task, if there is one - it cannot safely poll
    * this task's flags, since the task is freed as soon as
    * it is retired, and it may itself be gone: completion
    * goes through the record the two share */
   if (download_done)
      core_updater_sub_task_done_signal(download_done, err);

   command_event(CMD_EVENT_CORE_INFO_INIT, &refresh);

#if defined(RARCH_INTERNAL) && defined(HAVE_MENU)
   /* Force reload of contentless cores icons */
   menu_contentless_cores_free();
#endif
}

static void core_updater_download_handle_release(
      core_updater_download_handle_t *download_handle);

/* Pushed only by an archive download */
#if defined(HAVE_COMPRESSION)
static void cb_decompress_task_core_updater_download(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   decompress_task_data_t *decompress_data         =
         (decompress_task_data_t*)task_data;
   core_updater_download_handle_t *download_handle =
         (core_updater_download_handle_t*)user_data;

   /* Signal that decompression task is complete; the download task
    * may have retired already, so this is the last touch */
   if (download_handle)
   {
      retro_atomic_store_release_int(
            &download_handle->decompress_task_complete, 1);
      core_updater_download_handle_release(download_handle);
   }

   /* Remove original archive file */
   if (decompress_data)
   {
      if (*decompress_data->source_file)
         if (path_is_valid(decompress_data->source_file))
            filestream_delete(decompress_data->source_file);

      if (decompress_data->source_file)
         free(decompress_data->source_file);

      free(decompress_data);
   }

   /* Log any error messages */
   if (err && *err)
      RARCH_ERR("[Core Updater] %s", err);
}
#endif

void cb_http_task_core_updater_download(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   http_transfer_data_t *data                      = (http_transfer_data_t*)task_data;
   file_transfer_t *transf                         = (file_transfer_t*)user_data;
   core_updater_download_handle_t *download_handle = NULL;
   char output_dir[DIR_MAX_LENGTH];

   /* Resolved before any bail-out: the handle's reference for this
    * callback is dropped at the end on every path */
   if (!transf)
      goto finish;
   if (!(download_handle = (core_updater_download_handle_t*)transf->user_data))
      goto finish;

   if (!data || !*transf->path)
   {
      if (!err || !*err)
         err = "Download failed.";
      goto finish;
   }

   /* The body was streamed to transf->path as it arrived, so
    * data->data is NULL by design and there is nothing to write here.
    * task_push_http_download_file() removes the partial file unless
    * the transfer finished cleanly, so a non-2xx means there is no
    * file on disk to decompress. */
   if (err && *err)
      goto finish;

   if (data->status < 200 || data->status > 299)
   {
      err = "Download failed.";
      goto finish;
   }

   /* Recomputed for the decompress call below; the directory itself
    * was created before the transfer started. */
   strlcpy(output_dir, transf->path, sizeof(output_dir));
   path_basedir_wrapper(output_dir);

#if defined(HAVE_COMPRESSION)
   /* Decompress core file, if required
    * NOTE: If core is compressed and platform
    * doesn't have compression support, then this
    * whole thing falls apart...
    * We assume that the build process is configured
    * in such a way that this cannot happen... */
   if (path_is_compressed_file(transf->path))
   {
      /* The decompress callback's reference, taken before the push,
       * which it may outrun; a failed push runs no callback */
      retro_atomic_fetch_add_int(&download_handle->refs, 1);
      if (!(download_handle->decompress_task = (retro_task_t*)task_push_decompress(
            transf->path, output_dir,
            NULL, NULL, NULL,
            cb_decompress_task_core_updater_download,
            (void*)download_handle,
            NULL, true)))
      {
         retro_atomic_fetch_sub_int(&download_handle->refs, 1);
         err = msg_hash_to_str(MSG_DECOMPRESSION_FAILED);
         goto finish;
      }
   }
#endif

finish:
   /* Log any error messages */
   if (err && *err)
   {
      RARCH_ERR("[Core Updater] Download of \"%s\" failed: %s.\n",
            (transf ? transf->path: "unknown"), err);
      /* download_handle is still NULL on the early bail-outs above
       * (no transfer, no user_data), so it cannot be dereferenced
       * unconditionally here. */
      if (download_handle)
         download_handle->http_task_error = true;
   }
   if (transf)
      free(transf);

   if (download_handle)
   {
      /* If no decompress task was queued, mark it as completed */
      if (!download_handle->decompress_task)
         retro_atomic_store_release_int(
               &download_handle->decompress_task_complete, 1);

      /* Publish last: decompress_task, http_task_error and the
       * decompress flag above are the payload the worker reads once
       * it acquires this. */
      retro_atomic_store_release_int(&download_handle->http_task_complete, 1);

      /* The download task may have retired already */
      core_updater_download_handle_release(download_handle);
   }
}

static void free_core_updater_download_handle(core_updater_download_handle_t *download_handle)
{
   /* The staging directory is this download's alone (concurrent
    * downloads of one core are refused), and the HTTP and decompress
    * callbacks have let go by now: whatever is left in it - an archive
    * or core from a download that failed or was cancelled - goes, and
    * so does the directory */
   if (download_handle->staging_dir)
   {
      if (     download_handle->staged_core_path
            && path_is_valid(download_handle->staged_core_path))
         filestream_delete(download_handle->staged_core_path);
      if (     download_handle->local_download_path
            && path_is_valid(download_handle->local_download_path))
         filestream_delete(download_handle->local_download_path);
      path_rmdir(download_handle->staging_dir);
      free(download_handle->staging_dir);
   }

   /* A task cancelled part-way through the sliced CRC still holds an
    * open intfstream; without this it leaks the handle and the fd. */
   task_core_updater_crc_reset(&download_handle->crc_slice);

   /* Cancelled mid-wait: the backup's callback still holds its own
    * reference and frees the record when it runs. */
   core_updater_sub_task_done_release(download_handle->backup_done);

   if (download_handle->path_dir_libretro)
      free(download_handle->path_dir_libretro);

   if (download_handle->path_dir_core_assets)
      free(download_handle->path_dir_core_assets);

   if (download_handle->remote_filename)
      free(download_handle->remote_filename);

   if (download_handle->remote_core_path)
      free(download_handle->remote_core_path);

   if (download_handle->local_download_path)
      free(download_handle->local_download_path);

   if (download_handle->local_core_path)
      free(download_handle->local_core_path);


   if (download_handle->staged_core_path)
      free(download_handle->staged_core_path);

   if (download_handle->display_name)
      free(download_handle->display_name);

   free(download_handle);
   download_handle = NULL;
}

static void core_updater_download_handle_release(
      core_updater_download_handle_t *download_handle)
{
   if (     download_handle
         && retro_atomic_fetch_sub_int(&download_handle->refs, 1) == 1)
      free_core_updater_download_handle(download_handle);
}

/* Staged install: the new core is downloaded and extracted into its
 * own staging directory beside the cores, on the same volume, instead of
 * over the installed core, so the backup task can then move the old
 * core into the backups as it is and the new one into place - two
 * renames, with an installed core present throughout the download
 * and the extraction.  Only where the file the extraction would
 * replace is the installed core itself (no symbolic link between
 * them, which a move would back up instead of the core).  Returns
 * false, leaving the handle as it was, where that does not hold. */
/* .<core filename>.staging: one per core, so each has exactly one
 * owner and can be removed when that download is done */
#define CORE_UPDATER_STAGING_EXT ".staging"

static bool core_updater_download_stage(
      core_updater_download_handle_t *download_handle)
{
   bool staged = false;
   char *buf   = (char*)malloc(3 * PATH_MAX_LENGTH);
   char *member;
   char *installed;
   char *staging;

   if (!buf)
      return false;
   member    = buf;
   installed = buf +     PATH_MAX_LENGTH;
   staging   = buf + 2 * PATH_MAX_LENGTH;

   strlcpy(member, download_handle->remote_filename, PATH_MAX_LENGTH);
   if (path_is_compressed_file(member))
      path_remove_extension(member);
   fill_pathname_join_special(installed,
         download_handle->path_dir_libretro, member, PATH_MAX_LENGTH);

   if (string_is_equal(installed, download_handle->local_core_path))
   {
      char *download_path = NULL;
      char *staged_core   = NULL;
      char *staging_dir   = NULL;
      size_t _len;

      /* .<member>.staging, in the libretro directory ('installed' is
       * free again as scratch) */
      installed[0] = '.';
      _len  = 1 + strlcpy(installed + 1, member, PATH_MAX_LENGTH - 1);
      if (_len >= PATH_MAX_LENGTH)
      {
         free(buf);
         return false;
      }
      strlcpy(installed + _len, CORE_UPDATER_STAGING_EXT,
            PATH_MAX_LENGTH - _len);
      fill_pathname_join_special(staging,
            download_handle->path_dir_libretro, installed, PATH_MAX_LENGTH);
      staging_dir   = strdup(staging);
      fill_pathname_join_special(installed, staging,
            download_handle->remote_filename, PATH_MAX_LENGTH);
      download_path = strdup(installed);
      fill_pathname_join_special(installed, staging, member,
            PATH_MAX_LENGTH);
      staged_core   = strdup(installed);

      if (download_path && staged_core && staging_dir)
      {
         free(download_handle->local_download_path);
         download_handle->local_download_path = download_path;
         download_handle->staged_core_path    = staged_core;
         download_handle->staging_dir         = staging_dir;
         staged                               = true;
      }
      else
      {
         free(download_path);
         free(staged_core);
         free(staging_dir);
      }
   }

   free(buf);
   return staged;
}

static void task_core_updater_download_handler(retro_task_t *task)
{
   uint8_t flg;
   core_updater_download_handle_t *download_handle = NULL;

   if (!task)
      goto task_finished;

   download_handle = (core_updater_download_handle_t*)task->state;
   flg             = task_get_flags(task);

   if (!download_handle || ((flg & RETRO_TASK_FLG_CANCELLED) > 0))
      goto task_finished;

   switch (download_handle->status)
   {
      case CORE_UPDATER_DOWNLOAD_BEGIN:
         {
            /* Get CRC of existing core, if required.
             *
             * Sliced: the handler returns after one tick's budget and
             * is re-entered next tick, staying in this state until
             * the hash completes.  A 260MB core no longer stalls a
             * single tick for the whole read. */
            if (download_handle->local_crc == 0)
            {
               const char *local_core_path =
                  download_handle->local_core_path;
               if (
                       (local_core_path && *local_core_path)
                     && (     download_handle->crc_slice.active
                           || path_is_valid(local_core_path))
                  )
               {
                  bool done;
                  uint32_t crc = 0;
                  nbio_budget_t budget;

                  task_nbio_slice_open(&budget);
                  done = task_core_updater_crc_step(
                        &download_handle->crc_slice,
                        local_core_path, &crc, &budget);
                  task_nbio_slice_close(&budget);

                  if (!done)
                     break; /* resume next tick */
                  download_handle->local_crc = crc;
               }
            }

            /* Check whether existing core and remote core
             * have the same CRC */
            download_handle->crc_match = (download_handle->local_crc != 0)
                  && (download_handle->local_crc == download_handle->remote_crc);

            /* If CRC matches, end task immediately */
            if (download_handle->crc_match)
            {
               download_handle->status = CORE_UPDATER_DOWNLOAD_END;
               break;
            }

            /* If automatic backups are enabled and core is
             * already installed, trigger a backup - otherwise,
             * initialise download */
            download_handle->backup_enabled = download_handle->auto_backup &&
                  path_is_valid(download_handle->local_core_path);

            /* With 'Compress Automatic Core Backups' off, the backup
             * is taken after the extraction instead, by moving the
             * replaced core; where the install cannot be staged it is
             * compressed beforehand as usual */
            if (     download_handle->backup_enabled
                  && !download_handle->backup_compress
                  && core_updater_download_stage(download_handle))
               download_handle->status = CORE_UPDATER_DOWNLOAD_START_TRANSFER;
            else
               download_handle->status = download_handle->backup_enabled ?
                     CORE_UPDATER_DOWNLOAD_START_BACKUP :
                           CORE_UPDATER_DOWNLOAD_START_TRANSFER;
         }
         break;
      case CORE_UPDATER_DOWNLOAD_START_BACKUP:
         {
            core_updater_sub_task_done_t *done =
                  core_updater_sub_task_done_new();

            /* Request core backup */
            if (done)
            {
               if (!(download_handle->backup_task = (retro_task_t*)
                        task_push_core_backup(
                           download_handle->local_core_path,
                           download_handle->display_name,
                           download_handle->local_crc, CORE_BACKUP_MODE_AUTO,
                           download_handle->auto_backup_history_size,
                           download_handle->path_dir_core_assets, true,
                           cb_task_core_updater_backup, done)))
                  free(done); /* no task, so no callback */
               else
                  download_handle->backup_done = done;
            }

            if (download_handle->backup_task)
            {
               size_t _len;
               char task_title[128];

               /* Update task title */
               task_free_title(task);

               _len = strlcpy(
                     task_title, msg_hash_to_str(MSG_BACKING_UP_CORE),
                     sizeof(task_title));
               strlcpy(task_title + _len, download_handle->display_name, sizeof(task_title) - _len);

               task_set_title(task, strdup(task_title));

               /* Start waiting for backup to complete */
               download_handle->status = CORE_UPDATER_DOWNLOAD_WAIT_BACKUP;
            }
            else
            {
               /* This cannot realistically happen...
                * > If it does, just log an error and initialise
                *   download */
               RARCH_ERR("[Core Updater] Failed to backup core: \"%s\".\n",
                     download_handle->local_core_path);
               download_handle->backup_enabled = false;
               download_handle->status         = CORE_UPDATER_DOWNLOAD_START_TRANSFER;
            }
         }
         break;
      case CORE_UPDATER_DOWNLOAD_WAIT_BACKUP:
         /* Completion is read from the callback's flag, never from
          * the backup task: a retired task is freed, and the flag is
          * set before that happens.  Progress is only copied while
          * the flag is clear, through the same probe the HTTP wait
          * uses. */
         if (retro_atomic_load_acquire_int(
                  &download_handle->backup_done->complete))
         {
            core_updater_sub_task_done_release(download_handle->backup_done);
            download_handle->backup_done = NULL;
            download_handle->backup_task = NULL;
            download_handle->status      = CORE_UPDATER_DOWNLOAD_START_TRANSFER;
         }
         else
         {
            int8_t progress;
            /* Backup accounts for first third of task progress */
            if (core_updater_sub_task_running(
                     download_handle->backup_task, &progress))
               task_set_progress(task,
                     (int8_t)(((float)progress * (1.0f / 3.0f)) + 0.5f));
         }
         break;
      case CORE_UPDATER_DOWNLOAD_START_TRANSFER:
         {
            size_t _len;
            size_t _tlen;
            file_transfer_t *transf = NULL;
            char task_title[128];
            char output_dir[DIR_MAX_LENGTH];
            char http_title[NAME_MAX_LENGTH];

            /* Configure file transfer object */
            if (!(transf = (file_transfer_t*)calloc(1,
                        sizeof(file_transfer_t))))
               goto task_finished;

            strlcpy(
                  transf->path, download_handle->local_download_path,
                  sizeof(transf->path));

            transf->user_data = (void*)download_handle;

            /* The body is streamed straight to transf->path as it
             * arrives, so every check that gates the write has to
             * happen before the transfer starts rather than after
             * it. */
            strlcpy(output_dir, transf->path, sizeof(output_dir));
            path_basedir_wrapper(output_dir);

            if (!path_mkdir(output_dir))
            {
               RARCH_ERR("[Core Updater] Download of \"%s\" failed: %s.\n",
                     transf->path,
                     msg_hash_to_str(MSG_FAILED_TO_CREATE_THE_DIRECTORY));
               free(transf);
               download_handle->status = CORE_UPDATER_DOWNLOAD_ERROR;
               break;
            }

#ifdef HAVE_COMPRESSION
            /* If the core file is an archive, make sure it is not
             * already being decompressed by another task -- otherwise
             * we would now be overwriting the very file that task is
             * reading. */
            if (path_is_compressed_file(transf->path)
                  && task_check_decompress(transf->path))
            {
               RARCH_ERR("[Core Updater] Download of \"%s\" failed: %s.\n",
                     transf->path,
                     msg_hash_to_str(MSG_DECOMPRESSION_ALREADY_IN_PROGRESS));
               free(transf);
               download_handle->status = CORE_UPDATER_DOWNLOAD_ERROR;
               break;
            }
#endif

            /* The transfer title, which
             * task_push_http_download_file() takes as an
             * argument. */
            _tlen = 0;
            http_title[0] = '\0';
            strlcpy_append(http_title, sizeof(http_title), &_tlen,
                  msg_hash_to_str(MSG_DOWNLOADING));
            strlcpy_append(http_title, sizeof(http_title), &_tlen, ": ");
            strlcpy_append(http_title, sizeof(http_title), &_tlen,
                  transf->path);

            /* Push HTTP transfer task
             * > The callback's reference is taken before the
             *   push, which it may outrun; a failed push runs no
             *   callback, so it is dropped again here along with
             *   the transfer object
             * > A failed push is a failed download: with no
             *   callback, nothing would ever start the extraction
             *   WAIT_DECOMPRESS waits for.  No callback also means
             *   nothing else writes http_task_error. */
            retro_atomic_fetch_add_int(&download_handle->refs, 1);
            if (!(download_handle->http_task = (retro_task_t*)
                     task_push_http_download_file(
                        download_handle->remote_core_path, transf->path,
                        true, http_title,
                        cb_http_task_core_updater_download, transf)))
            {
               free(transf);
               retro_atomic_fetch_sub_int(&download_handle->refs, 1);
               download_handle->http_task_error = true;
            }

            /* Update task title */
            task_free_title(task);

            _len = strlcpy(
                  task_title, msg_hash_to_str(MSG_DOWNLOADING_CORE),
                  sizeof(task_title));
            strlcpy(task_title + _len, download_handle->display_name, sizeof(task_title) - _len);

            task_set_title(task, strdup(task_title));

            /* Start waiting for HTTP transfer to complete */
            download_handle->status = CORE_UPDATER_DOWNLOAD_WAIT_TRANSFER;
         }
         break;
      case CORE_UPDATER_DOWNLOAD_WAIT_TRANSFER:
         {
            int8_t progress;
            int complete;

            /* If HTTP task is NULL, then it either finished
             * or an error occurred - in either case,
             * just move on to the next state */
            if (!download_handle->http_task)
               retro_atomic_store_release_int(
                     &download_handle->http_task_complete, 1);

            complete = retro_atomic_load_acquire_int(
                  &download_handle->http_task_complete);

            /* If HTTP task is running, copy current
             * progress value to *this* task */
            if (!complete && core_updater_sub_task_running(
                     download_handle->http_task, &progress))
            {
               /* > If backups are enabled, download accounts
                *   for second third of task progress
                * > Otherwise, download accounts for first half
                *   of task progress */
               if (download_handle->backup_enabled)
                  progress = (int8_t)(((float)progress * (1.0f / 3.0f)) + (100.0f / 3.0f) + 0.5f);
               else
                  progress = progress >> 1;

               task_set_progress(task, progress);
            }

            /* Wait for task_push_http_transfer_file()
             * callback to trigger */
            if (complete)
            {
               size_t _len;
               char task_title[128];

               if (download_handle->http_task_error)
               {
                  download_handle->status = CORE_UPDATER_DOWNLOAD_ERROR;
                  break;
               }

               /* Update task title */
               task_free_title(task);

               _len = strlcpy(
                     task_title, msg_hash_to_str(MSG_EXTRACTING_CORE),
                     sizeof(task_title));
               strlcpy(task_title + _len, download_handle->display_name, sizeof(task_title) - _len);

               task_set_title(task, strdup(task_title));

               /* Start waiting for file to be extracted */
               download_handle->status = CORE_UPDATER_DOWNLOAD_WAIT_DECOMPRESS;
            }
         }
         break;
      case CORE_UPDATER_DOWNLOAD_WAIT_DECOMPRESS:
         {
            int8_t progress;
            int complete = retro_atomic_load_acquire_int(
                  &download_handle->decompress_task_complete);

            /* If decompression task is NULL, then it either
             * hasn't been queued by the download task yet,
             * or an error occurred. The latter should set
             * the decompress_task_complete flag and we'll
             * continue to the next state */
            if (    !complete
                 && download_handle->decompress_task
                 && core_updater_sub_task_running(
                     download_handle->decompress_task, &progress))
            {
               /* > If backups are enabled, decompression accounts
                *   for last third of task progress
                * > Otherwise, decompression accounts for second
                *   half of task progress */
               if (download_handle->backup_enabled)
                  progress = (int8_t)(((float)progress * (1.0f / 3.0f)) + (200.0f / 3.0f) + 0.5f);
               else
                  progress = 50 + (progress >> 1);

               task_set_progress(task, progress);
            }

            /* Wait for task_push_decompress()
             * callback to trigger */
            if (complete)
               download_handle->status = download_handle->staged_core_path
                     ? CORE_UPDATER_DOWNLOAD_START_INSTALL
                     : CORE_UPDATER_DOWNLOAD_END;
         }
         break;
      case CORE_UPDATER_DOWNLOAD_START_INSTALL:
         {
            core_updater_sub_task_done_t *done = NULL;

            /* An extraction that failed left nothing staged */
            if (!path_is_valid(download_handle->staged_core_path))
            {
               download_handle->status = CORE_UPDATER_DOWNLOAD_ERROR;
               break;
            }

            /* The backup task moves the replaced core into the
             * backups, the staged one into place, and prunes the
             * backup history; the record is created before the push,
             * which the callback may outrun */
            if ((done = core_updater_sub_task_done_new()))
            {
               if (!(download_handle->backup_task = (retro_task_t*)
                        task_push_core_backup_install(
                           download_handle->local_core_path,
                           download_handle->staged_core_path,
                           download_handle->display_name,
                           download_handle->local_crc,
                           download_handle->auto_backup_history_size,
                           download_handle->path_dir_core_assets, true,
                           cb_task_core_updater_backup, done)))
                  free(done); /* no task, so no callback */
               else
                  download_handle->backup_done = done;
            }

            if (download_handle->backup_task)
            {
               size_t _len;
               char task_title[128];

               task_free_title(task);
               _len = strlcpy(task_title,
                     msg_hash_to_str(MSG_BACKING_UP_CORE),
                     sizeof(task_title));
               strlcpy(task_title + _len, download_handle->display_name,
                     sizeof(task_title) - _len);
               task_set_title(task, strdup(task_title));

               download_handle->status = CORE_UPDATER_DOWNLOAD_WAIT_INSTALL;
            }
            /* No backup task: install without the backup, as the
             * extraction would have */
            else
            {
               RARCH_ERR("[Core Updater] Failed to backup core: \"%s\".\n",
                     download_handle->local_core_path);
               download_handle->status =
                     (filestream_rename(download_handle->staged_core_path,
                           download_handle->local_core_path) == 0)
                     ? CORE_UPDATER_DOWNLOAD_END
                     : CORE_UPDATER_DOWNLOAD_ERROR;
            }
         }
         break;
      case CORE_UPDATER_DOWNLOAD_WAIT_INSTALL:
         /* As WAIT_BACKUP: completion from the callback's record,
          * never from the retired task.  The record carries an error
          * exactly when the new core could not be installed. */
         if (retro_atomic_load_acquire_int(
                  &download_handle->backup_done->complete))
         {
            bool failed = (download_handle->backup_done->error != NULL);
            core_updater_sub_task_done_release(download_handle->backup_done);
            download_handle->backup_done = NULL;
            download_handle->backup_task = NULL;
            download_handle->status      = failed
                  ? CORE_UPDATER_DOWNLOAD_ERROR
                  : CORE_UPDATER_DOWNLOAD_END;
         }
         break;
      case CORE_UPDATER_DOWNLOAD_ERROR:
         {
            size_t _len;
            char task_title[128];

            /* Set final task title */
            task_free_title(task);

            /* A staged core that was not installed goes; the installed
             * core was never touched */
            if (     download_handle->staged_core_path
                  && path_is_valid(download_handle->staged_core_path))
               filestream_delete(download_handle->staged_core_path);

            _len = strlcpy(task_title, msg_hash_to_str(MSG_CORE_INSTALL_FAILED), sizeof(task_title));
            strlcpy(task_title + _len, download_handle->display_name,
                  sizeof(task_title) - _len);

            task_set_title(task, strdup(task_title));
            task_set_progress(task, 100);
            goto task_finished;
         }
         break;
      case CORE_UPDATER_DOWNLOAD_END:
         {
            size_t _len;
            char task_title[128];

            /* Set final task title */
            task_free_title(task);

            _len = strlcpy(
                  task_title,
                  download_handle->crc_match ?
                        msg_hash_to_str(MSG_LATEST_CORE_INSTALLED) : msg_hash_to_str(MSG_CORE_INSTALLED),
                  sizeof(task_title));
            strlcpy(task_title + _len, download_handle->display_name,
                  sizeof(task_title) - _len);

            task_set_title(task, strdup(task_title));
         }
         /* fall-through */
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:
   if (task)
   {
      /* Clear the state pointer before the handle is freed.  The
       * worker runs handlers with running_lock released and the task
       * still linked into tasks_running, so a task that has finished
       * here stays visible to retro_task_threaded_find() until the
       * worker retires it.  The finders in this file dereference
       * task->state, so leaving it pointing at freed memory is a
       * use-after-free - task_core_updater_download_finder() strcmps
       * through it, which is a hard crash the moment
       * task_update_installed_cores_handler() pushes the next core. */
      task->state = NULL;
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   }

   /* Cancelled mid-transfer: the HTTP or decompress callback still
    * holds a reference and frees the handle when it runs */
   core_updater_download_handle_release(download_handle);
}

static bool task_core_updater_download_finder(retro_task_t *task, void *user_data)
{
   if (task && user_data && task->handler == task_core_updater_download_handler)
   {
      core_updater_download_handle_t *download_handle = NULL;
      if ((download_handle = (core_updater_download_handle_t*)task->state))
         return string_is_equal((const char*)user_data, download_handle->remote_filename);
   }
   return false;
}

static void *task_push_core_updater_download_internal(
      core_updater_list_t* core_list,
      const char *filename, uint32_t crc, bool mute,
      bool auto_backup, bool backup_compress,
      size_t auto_backup_history_size,
      const char *path_dir_libretro,
      const char *path_dir_core_assets,
      core_updater_sub_task_done_t *download_done)
{
   size_t _len;
   task_finder_data_t find_data;
   char task_title[128];
   char local_download_path[PATH_MAX_LENGTH];
   const core_updater_list_entry_t *list_entry     = NULL;
   retro_task_t *task                              = NULL;
   core_updater_download_handle_t *download_handle = (core_updater_download_handle_t*)
         calloc(1, sizeof(core_updater_download_handle_t));

   task_title[0]          = '\0';
   local_download_path[0] = '\0';

#if defined(ANDROID)
   /* Regular core updater is disabled in
    * Play Store builds */
   if (play_feature_delivery_enabled())
      goto error;
#endif

   /* Sanity check */
   if (   !core_list
       || (!filename || !*filename)
       || !download_handle)
      goto error;

   /* Get core updater list entry */
   if (!core_updater_list_get_filename(
         core_list, filename, &list_entry))
      goto error;

   if (   (!list_entry->remote_core_path || !*list_entry->remote_core_path)
       || (!list_entry->local_core_path || !*list_entry->local_core_path)
       || (!list_entry->display_name || !*list_entry->display_name))
      goto error;

   /* Check whether core is locked
    * > Have to set validate_path to 'false' here,
    *   since this may not run on the main thread
    * > Validation is not required anyway, since core
    *   updater list provides 'sane' core paths */
   if (core_info_get_core_lock(list_entry->local_core_path, false))
   {
      RARCH_ERR("[Core Updater] Update disabled - core is locked: \"%s\".\n",
            list_entry->local_core_path);

      /* If task is not muted, generate notification */
      if (!mute)
      {
         char msg[128];
         size_t _len = strlcpy(msg, msg_hash_to_str(MSG_CORE_UPDATE_DISABLED), sizeof(msg));
         _len += strlcpy(msg + _len, list_entry->display_name, sizeof(msg) - _len);
         runloop_msg_queue_push(msg, _len, 1, 100, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }

      goto error;
   }

   /* Get local file download path */
   if (!path_dir_libretro || !*path_dir_libretro)
      goto error;

   fill_pathname_join_special(
         local_download_path,
         path_dir_libretro,
         list_entry->remote_filename,
         sizeof(local_download_path));

   /* Configure handle */
   download_handle->auto_backup              = auto_backup;
   download_handle->backup_compress          = backup_compress;
   download_handle->auto_backup_history_size = auto_backup_history_size;
   download_handle->path_dir_libretro        = strdup(path_dir_libretro);
   download_handle->path_dir_core_assets     = (path_dir_core_assets && *path_dir_core_assets) ? strdup(path_dir_core_assets) : NULL ;
   download_handle->remote_filename          = strdup(list_entry->remote_filename);
   download_handle->remote_core_path         = strdup(list_entry->remote_core_path);
   download_handle->local_download_path      = strdup(local_download_path);
   download_handle->local_core_path          = strdup(list_entry->local_core_path);
   download_handle->display_name             = strdup(list_entry->display_name);
   download_handle->local_crc                = crc;
   download_handle->remote_crc               = list_entry->crc;
   download_handle->crc_match                = false;
   download_handle->http_task                = NULL;
   download_handle->http_task_error          = false;
   retro_atomic_store_release_int(&download_handle->http_task_complete, 0);
   download_handle->decompress_task          = NULL;
   retro_atomic_store_release_int(
         &download_handle->decompress_task_complete, 0);
   download_handle->backup_enabled           = false;
   download_handle->backup_task              = NULL;
   download_handle->backup_done              = NULL;
   download_handle->status                   = CORE_UPDATER_DOWNLOAD_BEGIN;
   retro_atomic_int_init(&download_handle->refs, 1);

   /* Concurrent downloads of the same file are not allowed */
   find_data.func     = task_core_updater_download_finder;
   find_data.userdata = (void*)download_handle->remote_filename;

   if (task_queue_find(&find_data))
      goto error;

   /* Create task */
   if (!(task = task_init()))
      goto error;

   /* Configure task */
   _len = strlcpy(
         task_title, msg_hash_to_str(MSG_UPDATING_CORE),
         sizeof(task_title));
   strlcpy(task_title + _len, download_handle->display_name,
         sizeof(task_title) - _len);

   task->handler          = task_core_updater_download_handler;
   task->state            = download_handle;
   task->title            = strdup(task_title);
   task->progress         = 0;
   task->progress_cb      = task_window_progress_cb;
   task->callback         = cb_task_core_updater_download;
   task->user_data        = (void*)download_done;
   task->flags           |=  RETRO_TASK_FLG_ALTERNATIVE_LOOK;
   if (mute)
      task->flags        |=  RETRO_TASK_FLG_MUTE;
   else
      task->flags        &= ~RETRO_TASK_FLG_MUTE;

   /* Push task */
   task_queue_push(task);

   return task;

error:

   /* Clean up task */
   if (task)
   {
      free(task);
      task = NULL;
   }

   /* Clean up handle */
   if (download_handle)
      free_core_updater_download_handle(download_handle);

   return NULL;
}

void *task_push_core_updater_download(
      core_updater_list_t* core_list,
      const char *filename, uint32_t crc, bool mute,
      bool auto_backup, size_t auto_backup_history_size,
      const char *path_dir_libretro,
      const char *path_dir_core_assets)
{
   settings_t *settings = config_get_ptr();
   return task_push_core_updater_download_internal(
         core_list, filename, crc, mute, auto_backup,
         settings->bools.core_updater_auto_backup_compress,
         auto_backup_history_size, path_dir_libretro,
         path_dir_core_assets, NULL);
}

/****************************/
/* Installed core CRC cache */
/****************************/

/* Update Installed Cores compares each installed core's CRC with the
 * buildbot's, and hashing every installed core on every run reads
 * them all in full.  The CRC is cached per core against the size and
 * modification time the libretro directory walk reports
 * (core_updater_list_entry_t local_size/local_mtime), so a core whose
 * file has not changed since it was last hashed is not read again.
 * Anything else - no metadata, no cache, a corrupt one, a changed
 * size or mtime - hashes the core as before, so the cache can only
 * save work.
 *
 * The cache is a text file beside the core backups:
 *   core_updater_crc 1
 *   <crc, 8 hex digits> <size> <mtime> <listing filename>
 * Cores not reached in a run are dropped when it is written. */

#define CORE_CRC_CACHE_FILE   "core_updater.crc"
#define CORE_CRC_CACHE_HEADER "core_updater_crc 1"

static void core_crc_cache_free(core_crc_cache_t *cache)
{
   size_t i;
   for (i = 0; i < cache->count; i++)
      free(cache->entries[i].name);
   free(cache->entries);
   free(cache->path);
   cache->entries = NULL;
   cache->path    = NULL;
   cache->count   = 0;
   cache->cap     = 0;
   cache->dirty   = false;
}

static core_crc_cache_entry_t *core_crc_cache_find(core_crc_cache_t *cache,
      const char *name)
{
   size_t i;
   for (i = 0; i < cache->count; i++)
      if (string_is_equal(cache->entries[i].name, name))
         return &cache->entries[i];
   return NULL;
}

static core_crc_cache_entry_t *core_crc_cache_add(core_crc_cache_t *cache,
      const char *name, int64_t size, int64_t mtime, uint32_t crc)
{
   core_crc_cache_entry_t *entry;

   if (cache->count == cache->cap)
   {
      size_t cap = cache->cap ? cache->cap * 2 : 32;
      core_crc_cache_entry_t *tmp = (core_crc_cache_entry_t*)
            realloc(cache->entries, cap * sizeof(*tmp));
      if (!tmp)
         return NULL;
      cache->entries = tmp;
      cache->cap     = cap;
   }

   entry = &cache->entries[cache->count];
   if (!(entry->name = strdup(name)))
      return NULL;
   entry->size  = size;
   entry->mtime = mtime;
   entry->crc   = crc;
   entry->seen  = false;
   cache->count++;
   return entry;
}

/* Parses an unsigned decimal or hex field of @s up to @end into @out;
 * returns the character after it, or NULL if there are no digits */
static const char *core_crc_cache_parse_u64(const char *s, const char *end,
      unsigned base, uint64_t *out)
{
   uint64_t v        = 0;
   const char *start = s;
   for (; s < end; s++)
   {
      unsigned d;
      if (*s >= '0' && *s <= '9')
         d = (unsigned)(*s - '0');
      else if (base == 16 && *s >= 'a' && *s <= 'f')
         d = (unsigned)(*s - 'a' + 10);
      else if (base == 16 && *s >= 'A' && *s <= 'F')
         d = (unsigned)(*s - 'A' + 10);
      else
         break;
      v = v * base + d;
   }
   if (s == start)
      return NULL;
   *out = v;
   return s;
}

static void core_crc_cache_load(core_crc_cache_t *cache)
{
   void *buf      = NULL;
   int64_t len    = 0;
   const char *p;
   const char *end;
   size_t hlen    = STRLEN_CONST(CORE_CRC_CACHE_HEADER);

   if (!cache->path)
      return;
   if (!path_is_valid(cache->path))
      return;
   if (!filestream_read_file(cache->path, &buf, &len) || !buf)
      return;

   p   = (const char*)buf;
   end = p + len;

   /* An unknown version is ignored whole, and overwritten */
   if (     (size_t)(end - p) < hlen
         || strncmp(p, CORE_CRC_CACHE_HEADER, hlen)
         || (p + hlen < end && p[hlen] != '\n' && p[hlen] != '\r'))
   {
      free(buf);
      return;
   }
   p += hlen;

   while (p < end)
   {
      uint64_t crc, size, mtime;
      const char *line_end;
      const char *name;
      size_t name_len;
      char name_buf[PATH_MAX_LENGTH];

      /* To the next line */
      while (p < end && (*p == '\n' || *p == '\r'))
         p++;
      if (p >= end)
         break;
      for (line_end = p; line_end < end && *line_end != '\n'; line_end++)
         ;

      /* Malformed lines are skipped, not trusted */
      if (     !(p = core_crc_cache_parse_u64(p, line_end, 16, &crc))
            || p >= line_end || *p++ != ' '
            || !(p = core_crc_cache_parse_u64(p, line_end, 10, &size))
            || p >= line_end || *p++ != ' '
            || !(p = core_crc_cache_parse_u64(p, line_end, 10, &mtime))
            || p >= line_end || *p++ != ' ')
      {
         p = line_end;
         continue;
      }

      name     = p;
      name_len = (size_t)(line_end - name);
      if (name_len > 0 && name[name_len - 1] == '\r')
         name_len--;
      p        = line_end;

      if (     name_len == 0
            || name_len >= sizeof(name_buf)
            || crc > 0xFFFFFFFFu
            || crc == 0)
         continue;

      memcpy(name_buf, name, name_len);
      name_buf[name_len] = '\0';

      if (!core_crc_cache_find(cache, name_buf))
         core_crc_cache_add(cache, name_buf,
               (int64_t)size, (int64_t)mtime, (uint32_t)crc);
   }

   free(buf);
}

/* The cached CRC of @name, if its file still has @size and @mtime.
 * Marks the entry as belonging to an installed core either way. */
static bool core_crc_cache_lookup(core_crc_cache_t *cache, const char *name,
      int64_t size, int64_t mtime, uint32_t *crc)
{
   core_crc_cache_entry_t *entry = core_crc_cache_find(cache, name);
   if (!entry)
      return false;
   entry->seen = true;
   if (entry->size != size || entry->mtime != mtime)
      return false;
   *crc = entry->crc;
   return true;
}

static void core_crc_cache_store(core_crc_cache_t *cache, const char *name,
      int64_t size, int64_t mtime, uint32_t crc)
{
   core_crc_cache_entry_t *entry = core_crc_cache_find(cache, name);

   if (!cache->path)
      return;
   if (!entry && !(entry = core_crc_cache_add(cache, name, size, mtime, crc)))
      return;

   entry->size  = size;
   entry->mtime = mtime;
   entry->crc   = crc;
   entry->seen  = true;
   cache->dirty = true;
}

/* Writes the entries of cores reached this run, if anything changed */
static void core_crc_cache_save(core_crc_cache_t *cache)
{
   size_t i;
   size_t cap;
   size_t _len;
   char *text;
   bool prune = false;
   char dir[PATH_MAX_LENGTH];

   if (!cache->path)
      return;
   for (i = 0; i < cache->count; i++)
      if (!cache->entries[i].seen)
         prune = true;
   if (!cache->dirty && !prune)
      return;

   cap = STRLEN_CONST(CORE_CRC_CACHE_HEADER) + 2;
   for (i = 0; i < cache->count; i++)
      cap += strlen(cache->entries[i].name) + 64;
   if (!(text = (char*)malloc(cap)))
      return;

   _len = strlcpy(text, CORE_CRC_CACHE_HEADER "\n", cap);
   for (i = 0; i < cache->count; i++)
   {
      const core_crc_cache_entry_t *entry = &cache->entries[i];
      if (!entry->seen)
         continue;
      _len += snprintf(text + _len, cap - _len,
            "%08x " STRING_REP_INT64 " " STRING_REP_INT64 " %s\n",
            (unsigned)entry->crc, entry->size, entry->mtime, entry->name);
   }

   fill_pathname_basedir(dir, cache->path, sizeof(dir));
   if (path_is_directory(dir) || path_mkdir(dir))
      filestream_write_file_atomic(cache->path, text, (int64_t)_len);

   free(text);
   cache->dirty = false;
}

/* Cache location: beside the core backups, in
 * <core assets dir, else libretro dir>/core_backups */
static char *core_crc_cache_path(const char *dir_libretro,
      const char *dir_core_assets)
{
   char backups[PATH_MAX_LENGTH];
   char path[PATH_MAX_LENGTH];
   const char *base = (dir_core_assets && *dir_core_assets)
         ? dir_core_assets : dir_libretro;

   if (!base || !*base)
      return NULL;
   fill_pathname_join_special(backups, base, "core_backups",
         sizeof(backups));
   fill_pathname_join_special(path, backups, CORE_CRC_CACHE_FILE,
         sizeof(path));
   return strdup(path);
}

/**************************/
/* Update installed cores */
/**************************/

static void free_update_installed_cores_handle(
      update_installed_cores_handle_t *update_installed_handle)
{
   if (update_installed_handle->path_dir_libretro)
      free(update_installed_handle->path_dir_libretro);

   if (update_installed_handle->path_dir_core_assets)
      free(update_installed_handle->path_dir_core_assets);

   /* A child still pending - this task was cancelled while waiting
    * on it - keeps running and retires later.  Its callback holds
    * the other reference, so letting go here leaves the record to
    * the callback.  A list fetch's callback also still publishes
    * into core_list: the list goes with the record and dies with
    * its last reference. */
   if (update_installed_handle->list_done)
   {
      update_installed_handle->list_done->orphan_list =
            update_installed_handle->core_list;
      update_installed_handle->core_list = NULL;
      core_updater_sub_task_done_release(
            update_installed_handle->list_done);
   }
   core_updater_sub_task_done_release(
         update_installed_handle->download_done);

   core_updater_list_free(update_installed_handle->core_list);

   core_crc_cache_free(&update_installed_handle->crc_cache);

   free(update_installed_handle);
   update_installed_handle = NULL;
}

/* Walks the core list and checks each installed core against the
 * buildbot CRC.  Every list entry and every CORE_CRC_CHUNK hashed is
 * one work item of the shared I/O window, so a single call covers as
 * much of the list as the window allows: entries that are not
 * installed cost a stat each, not a tick each.  Returns with the
 * status left at ITERATE or UPDATE_CORE when the window is spent,
 * WAIT_DOWNLOAD after pushing a download, or END. */
static void task_update_installed_cores_scan(retro_task_t *task,
      update_installed_cores_handle_t *handle)
{
   nbio_budget_t budget;

   task_nbio_slice_open(&budget);

   for (;;)
   {
      const core_updater_list_entry_t *list_entry = NULL;

      if (handle->status == UPDATE_INSTALLED_CORES_ITERATE)
      {
         if (handle->list_index >= handle->list_size)
         {
            handle->status = UPDATE_INSTALLED_CORES_END;
            break;
         }

         if (!task_nbio_slice_within_budget(&budget, 0, 0))
            break;

         if (     core_updater_list_get_index(handle->core_list,
                     handle->list_index, &list_entry)
               && path_is_valid(list_entry->local_core_path))
         {
            size_t _len;
            char task_title[128];

            handle->installed_index = handle->list_index;
            handle->status          = UPDATE_INSTALLED_CORES_UPDATE_CORE;
            RARCH_LOG("[Core Updater] Checking: \"%s\"...\n",
                  list_entry->local_core_path);

            _len = strlcpy(task_title, msg_hash_to_str(MSG_CHECKING_CORE),
                  sizeof(task_title));
            strlcpy(task_title + _len, list_entry->display_name,
                  sizeof(task_title) - _len);
            task_free_title(task);
            task_set_title(task, strdup(task_title));
            handle->title_scanning = false;
         }

         handle->list_index++;
         continue;
      }

      /* UPDATE_INSTALLED_CORES_UPDATE_CORE */
      {
         uint32_t local_crc = 0;
         bool cached        = false;

         if (!core_updater_list_get_index(handle->core_list,
                  handle->installed_index, &list_entry))
         {
            handle->status = UPDATE_INSTALLED_CORES_ITERATE;
            continue;
         }

         /* Lock check once per core, on reaching it; a CRC resumed
          * from a previous tick has already passed it.  validate_path
          * is false because this may run off the main thread, and the
          * list provides sane core paths. */
         if (!handle->crc_slice.active)
         {
            /* An unchanged file keeps the CRC it was last hashed to
             * (the lookup also keeps a locked core's entry) */
            if (list_entry->local_metadata)
               cached = core_crc_cache_lookup(&handle->crc_cache,
                     list_entry->remote_filename,
                     list_entry->local_size, list_entry->local_mtime,
                     &local_crc);
         }

         if (     !handle->crc_slice.active
               && core_info_get_core_lock(list_entry->local_core_path, false))
         {
            RARCH_LOG("[Core Updater] Skipping locked core: \"%s\".\n",
                  list_entry->display_name);
            handle->num_locked++;
            handle->status = UPDATE_INSTALLED_CORES_ITERATE;
            continue;
         }

         /* ITERATE established that the core exists; a core removed
          * since then fails the open and hashes as 0, which requests
          * the download the same as a mismatch. */
         if (!cached)
         {
            if (!task_core_updater_crc_step(&handle->crc_slice,
                     list_entry->local_core_path, &local_crc, &budget))
               break;

            /* Keyed on the metadata of the walk that preceded the
             * hash: a file changed since then has other metadata
             * next run, and is hashed again */
            if (local_crc != 0 && list_entry->local_metadata)
               core_crc_cache_store(&handle->crc_cache,
                     list_entry->remote_filename,
                     list_entry->local_size, list_entry->local_mtime,
                     local_crc);
         }

         if ((local_crc != 0) && (local_crc == list_entry->crc))
         {
            RARCH_LOG("[Core Updater] Core \"%s\" is already at latest version.\n",
                  list_entry->display_name);
            handle->status = UPDATE_INSTALLED_CORES_ITERATE;
            continue;
         }

         /* The record exists *before* the push, since the child
          * task can complete before it returns */
         if (!(handle->download_done = core_updater_sub_task_done_new()))
         {
            handle->status = UPDATE_INSTALLED_CORES_ITERATE;
            continue;
         }

         if (!task_push_core_updater_download_internal(
                     handle->core_list,
                     list_entry->remote_filename,
                     local_crc, true,
                     handle->auto_backup,
                     handle->auto_backup_compress,
                     handle->auto_backup_history_size,
                     handle->path_dir_libretro,
                     handle->path_dir_core_assets,
                     handle->download_done))
         {
            /* No task, so no callback: both references are ours */
            free(handle->download_done);
            handle->download_done = NULL;
            handle->status        = UPDATE_INSTALLED_CORES_ITERATE;
            continue;
         }

         {
            size_t _len;
            char task_title[128];

            _len = strlcpy(task_title, msg_hash_to_str(MSG_UPDATING_CORE),
                  sizeof(task_title));
            strlcpy(task_title + _len, list_entry->display_name,
                  sizeof(task_title) - _len);
            task_free_title(task);
            task_set_title(task, strdup(task_title));
            handle->title_scanning = false;
         }

         handle->num_updated++;
         handle->status = UPDATE_INSTALLED_CORES_WAIT_DOWNLOAD;
         RARCH_LOG("[Core Updater] Downloading: \"%s\"...\n",
               list_entry->display_name);
         break;
      }
   }

   task_nbio_slice_close(&budget);

   /* Mid-list with no core in hand: show the scan, once */
   if (     handle->status == UPDATE_INSTALLED_CORES_ITERATE
         && !handle->title_scanning)
   {
      task_free_title(task);
      task_set_title(task, strdup(msg_hash_to_str(MSG_SCANNING_CORES)));
      handle->title_scanning = true;
   }

   task_set_progress(task, (handle->list_index * 100) / handle->list_size);
}

static void task_update_installed_cores_handler(retro_task_t *task)
{
   uint8_t flg;
   update_installed_cores_handle_t *update_installed_handle = NULL;

   if (!task)
      goto task_finished;

   update_installed_handle = (update_installed_cores_handle_t*)task->state;
   flg                     = task_get_flags(task);

   if (!update_installed_handle || ((flg & RETRO_TASK_FLG_CANCELLED) > 0))
      goto task_finished;

   switch (update_installed_handle->status)
   {
      case UPDATE_INSTALLED_CORES_BEGIN:
         core_crc_cache_load(&update_installed_handle->crc_cache);

         /* Request buildbot core list
          * > The record must exist *before* the push: the
          *   child task can finish and fire its callback
          *   before this returns
          * > If push failed, go to end (the error message
          *   is shown when the final task title is set) */
         update_installed_handle->status = UPDATE_INSTALLED_CORES_END;
         if ((update_installed_handle->list_done =
                  core_updater_sub_task_done_new()))
         {
            if (!task_push_get_core_updater_list_captured(
                     update_installed_handle->core_list,
                     true, false, true, update_installed_handle->list_done,
                     update_installed_handle->dir_libretro,
                     update_installed_handle->path_libretro_info,
                     update_installed_handle->network_buildbot_url))
            {
               /* No task, so no callback: both references are ours */
               free(update_installed_handle->list_done);
               update_installed_handle->list_done = NULL;
            }
            else
               update_installed_handle->status = UPDATE_INSTALLED_CORES_WAIT_LIST;
         }
         break;
      case UPDATE_INSTALLED_CORES_WAIT_LIST:
         {
            /* Wait for cb_task_core_updater_get_list() to
             * trigger. Note that the child task must not be
             * polled via task_get_flags(): a finished task is
             * retired and freed inside the same task_queue
             * gather pass that ran its handler, and with
             * threaded tasks disabled that pass may run the
             * child *after* this one - leaving nothing to
             * observe on the next tick but freed memory, and
             * this task stuck on 'Fetching core list...'
             * forever */
            bool list_available = retro_atomic_load_acquire_int(
                  &update_installed_handle->list_done->complete) != 0;

            /* If list is available, check that it was fetched
             * and that it holds any installed cores (an error
             * message is displayed when the final task title
             * is set) */
            if (list_available)
            {
               /* The child is done with core_list, which stays
                * ours: let go of the record */
               core_updater_sub_task_done_release(
                     update_installed_handle->list_done);
               update_installed_handle->list_done = NULL;

               update_installed_handle->list_fetched =
                     core_updater_list_get_type(update_installed_handle->core_list)
                  == CORE_UPDATER_LIST_TYPE_BUILDBOT;
               update_installed_handle->list_size =
                     core_updater_list_size(update_installed_handle->core_list);
               RARCH_DBG("[Core Updater] Updater list size from buildbot: %d.\n",
                     update_installed_handle->list_size);

               if (update_installed_handle->list_size < 1)
                  update_installed_handle->status = UPDATE_INSTALLED_CORES_END;
               else
                  update_installed_handle->status = UPDATE_INSTALLED_CORES_ITERATE;
            }
         }
         break;
      case UPDATE_INSTALLED_CORES_ITERATE:
      case UPDATE_INSTALLED_CORES_UPDATE_CORE:
         task_update_installed_cores_scan(task, update_installed_handle);
         break;
      case UPDATE_INSTALLED_CORES_WAIT_DOWNLOAD:
         {
            /* Wait for cb_task_core_updater_download() to
             * trigger - same lifetime hazard as
             * UPDATE_INSTALLED_CORES_WAIT_LIST, so the child
             * task's flags are deliberately not polled */
            bool download_complete = retro_atomic_load_acquire_int(
                  &update_installed_handle->download_done->complete) != 0;

            /* If download is complete, return to
             * UPDATE_INSTALLED_CORES_ITERATE state */
            if (download_complete)
            {
               core_updater_sub_task_done_release(
                     update_installed_handle->download_done);
               update_installed_handle->download_done = NULL;
               update_installed_handle->status = UPDATE_INSTALLED_CORES_ITERATE;
            }
         }
         break;
      case UPDATE_INSTALLED_CORES_END:
         {
            /* Only a run that reached its end writes the cache: a
             * cancelled one leaves the previous file standing */
            core_crc_cache_save(&update_installed_handle->crc_cache);

            /* Set final task title */
            task_free_title(task);

            /* > Check whether core list was fetched
             *   successfully */
            if (update_installed_handle->list_fetched)
            {
               char task_title[128];
               size_t _len = strlcpy(task_title,
                     msg_hash_to_str(MSG_ALL_CORES_UPDATED),
                     sizeof(task_title));

               /* > Generate final status message based on number
                *   of cores that were updated/locked */
               if (update_installed_handle->num_updated > 0)
               {
                  if (update_installed_handle->num_locked > 0)
                     snprintf(
                           task_title         + _len,
                           sizeof(task_title) - _len,
                           " (%s%u, %s%u)",
                           msg_hash_to_str(MSG_NUM_CORES_UPDATED),
                           update_installed_handle->num_updated,
                           msg_hash_to_str(MSG_NUM_CORES_LOCKED),
                           update_installed_handle->num_locked);
                  else
                     snprintf(
                           task_title         + _len,
                           sizeof(task_title) - _len,
                           " (%s%u)",
                           msg_hash_to_str(MSG_NUM_CORES_UPDATED),
                           update_installed_handle->num_updated);
               }
               else if (update_installed_handle->num_locked > 0)
                  snprintf(
                        task_title         + _len,
                        sizeof(task_title) - _len,
                        " (%s%u)",
                        msg_hash_to_str(MSG_NUM_CORES_LOCKED),
                        update_installed_handle->num_locked);

               task_set_title(task, strdup(task_title));
            }
            else
               task_set_title(task, strdup(msg_hash_to_str(MSG_CORE_LIST_FAILED)));
         }
         /* fall-through */
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:
   if (task)
   {
      /* Clear the state pointer before the handle is freed.  The
       * worker runs handlers with running_lock released and the task
       * still linked into tasks_running, so a task that has finished
       * here stays visible to retro_task_threaded_find() until the
       * worker retires it.  The finders in this file dereference
       * task->state, so leaving it pointing at freed memory is a
       * use-after-free - task_core_updater_download_finder() strcmps
       * through it, which is a hard crash the moment
       * task_update_installed_cores_handler() pushes the next core. */
      task->state = NULL;
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   }

   if (update_installed_handle)
      free_update_installed_cores_handle(update_installed_handle);
}

static bool task_update_installed_cores_finder(retro_task_t *task, void *user_data)
{
   if (task)
      if (     task->handler == task_update_installed_cores_handler
            )
         return true;
   return false;
}

void task_push_update_installed_cores(
      bool auto_backup, size_t auto_backup_history_size,
      const char *path_dir_libretro,
      const char *path_dir_core_assets)
{
   task_finder_data_t find_data;
   retro_task_t *task                                       = NULL;
   update_installed_cores_handle_t *update_installed_handle =
         (update_installed_cores_handle_t*)
               calloc(1, sizeof(update_installed_cores_handle_t));

#if defined(ANDROID)
   /* Regular core updater is disabled in
    * Play Store builds */
   if (play_feature_delivery_enabled())
      goto error;
#endif

   /* Sanity check */
   if (  !update_installed_handle
       || (!path_dir_libretro || !*path_dir_libretro))
      goto error;

   /* Configure handle */
   update_installed_handle->auto_backup              = auto_backup;
   update_installed_handle->auto_backup_compress     =
         config_get_ptr()->bools.core_updater_auto_backup_compress;
   update_installed_handle->auto_backup_history_size = auto_backup_history_size;
   /* Captured here, on the main thread: the handler pushes the list
    * task from the worker with these. */
   {
      settings_t *settings = config_get_ptr();
      strlcpy(update_installed_handle->dir_libretro,
            settings->paths.directory_libretro,
            sizeof(update_installed_handle->dir_libretro));
      strlcpy(update_installed_handle->path_libretro_info,
            settings->paths.path_libretro_info,
            sizeof(update_installed_handle->path_libretro_info));
      strlcpy(update_installed_handle->network_buildbot_url,
            settings->paths.network_buildbot_url,
            sizeof(update_installed_handle->network_buildbot_url));
   }
   update_installed_handle->path_dir_libretro        = strdup(path_dir_libretro);
   update_installed_handle->path_dir_core_assets     = (!path_dir_core_assets || !*path_dir_core_assets) ?
         NULL : strdup(path_dir_core_assets);
   update_installed_handle->core_list                = core_updater_list_init();
   update_installed_handle->crc_cache.path           = core_crc_cache_path(
         path_dir_libretro, path_dir_core_assets);
   update_installed_handle->list_done                = NULL;
   update_installed_handle->download_done            = NULL;
   update_installed_handle->list_size                = 0;
   update_installed_handle->list_index               = 0;
   update_installed_handle->installed_index          = 0;
   update_installed_handle->num_updated              = 0;
   update_installed_handle->num_locked               = 0;
   update_installed_handle->title_scanning           = false;
   update_installed_handle->list_fetched             = false;
   update_installed_handle->status                   = UPDATE_INSTALLED_CORES_BEGIN;

   if (!update_installed_handle->core_list)
      goto error;

   /* Only one instance of this task may run at a time */
   find_data.func     = task_update_installed_cores_finder;
   find_data.userdata = NULL;

   if (task_queue_find(&find_data))
      goto error;

   /* Create task */
   if (!(task = task_init()))
      goto error;

   /* Configure task */
   task->handler          = task_update_installed_cores_handler;
   task->state            = update_installed_handle;
   task->title            = strdup(msg_hash_to_str(MSG_FETCHING_CORE_LIST));
   task->progress         = 0;
   task->progress_cb      = task_window_progress_cb;
   task->flags           |= RETRO_TASK_FLG_ALTERNATIVE_LOOK;

   /* Push task */
   task_queue_push(task);

   return;

error:

   /* Clean up task */
   if (task)
   {
      free(task);
      task = NULL;
   }

   /* Clean up handle */
   if (update_installed_handle)
      free_update_installed_cores_handle(update_installed_handle);
}

#if defined(ANDROID)
/**************************************/
/* Play feature delivery core install */
/**************************************/

static void free_play_feature_delivery_install_handle(
      play_feature_delivery_install_handle_t *pfd_install_handle)
{
   if (pfd_install_handle->core_filename)
      free(pfd_install_handle->core_filename);

   if (pfd_install_handle->local_core_path)
      free(pfd_install_handle->local_core_path);

   if (pfd_install_handle->backup_core_path)
      free(pfd_install_handle->backup_core_path);

   if (pfd_install_handle->display_name)
      free(pfd_install_handle->display_name);

   free(pfd_install_handle);
   pfd_install_handle = NULL;
}

static void task_play_feature_delivery_core_install_handler(
      retro_task_t *task)
{
   uint8_t flg;
   play_feature_delivery_install_handle_t *pfd_install_handle = NULL;

   if (!task)
      goto task_finished;

   pfd_install_handle =
      (play_feature_delivery_install_handle_t*)task->state;
   flg                = task_get_flags(task);

   if (!pfd_install_handle || ((flg & RETRO_TASK_FLG_CANCELLED) > 0))
      goto task_finished;

   switch (pfd_install_handle->status)
   {
      case PLAY_FEATURE_DELIVERY_INSTALL_BEGIN:
         {
            size_t _len;
            char backup_core_path[PATH_MAX_LENGTH];

            /* Backup file name: an existing core is moved aside
             * here for the duration of the install
             * > Note: since only one install task can run at
             *   a time, a UID is not required */
            _len = strlcpy(backup_core_path,
                  pfd_install_handle->local_core_path,
                  sizeof(backup_core_path));
            strlcpy(backup_core_path + _len,
                  FILE_PATH_BACKUP_EXTENSION,
                  sizeof(backup_core_path) - _len);

            /* Check whether core has already been
             * installed via play feature delivery */
            if (play_feature_delivery_core_installed(
                  pfd_install_handle->core_filename))
            {
               /* A backup left by an interrupted install that
                * play feature delivery went on to complete is
                * superseded, as on a successful install */
               if (path_is_valid(backup_core_path))
                  filestream_delete(backup_core_path);

               pfd_install_handle->success                = true;
               pfd_install_handle->core_already_installed = true;
               pfd_install_handle->status                 =
                     PLAY_FEATURE_DELIVERY_INSTALL_END;
               break;
            }

            /* If core is already installed via other
             * means, must remove it before attempting
             * play feature delivery transaction */
            if (path_is_valid(pfd_install_handle->local_core_path))
            {
               int ret;

               /* Have to create a backup, in case install
                * process fails
                * > If an old backup file exists (i.e. leftovers
                *   from a mid-task crash/user exit), delete it:
                *   the core it was taken from is still here */
               if (path_is_valid(backup_core_path))
                  filestream_delete(backup_core_path);

               /* Attempt to rename core file */
               ret = filestream_rename(
                     pfd_install_handle->local_core_path,
                     backup_core_path);

               /* Success - cache backup file name */
               if (!ret)
                  pfd_install_handle->backup_core_path = strdup(backup_core_path);
               /* If backup failed, all we can do is delete
                * the existing core file... */
               else if (path_is_valid(pfd_install_handle->local_core_path))
                  filestream_delete(pfd_install_handle->local_core_path);
            }
            /* No core, but the backup an interrupted install
             * (crash, quit) moved it to: that is the user's
             * core, so it becomes this install's backup - put
             * back if this install fails, dropped if it
             * succeeds - rather than being left behind */
            else if (path_is_valid(backup_core_path))
               pfd_install_handle->backup_core_path = strdup(backup_core_path);

            /* Start download */
            if (play_feature_delivery_download(
                  pfd_install_handle->core_filename))
               pfd_install_handle->status = PLAY_FEATURE_DELIVERY_INSTALL_WAIT;
            else
               pfd_install_handle->status = PLAY_FEATURE_DELIVERY_INSTALL_END;
         }
         break;
      case PLAY_FEATURE_DELIVERY_INSTALL_WAIT:
         {
            size_t _len;
            enum play_feature_delivery_install_status install_status;
            unsigned install_progress;
            char task_title[128];
            /* Get current install status */
            bool install_active = play_feature_delivery_download_status(
                  &install_status, &install_progress);

            /* In all cases, update task progress */
            task_set_progress(task, install_progress);

            /* Interpret status */
            switch (install_status)
            {
               case PLAY_FEATURE_DELIVERY_INSTALLED:
                  pfd_install_handle->success = true;
                  pfd_install_handle->status  = PLAY_FEATURE_DELIVERY_INSTALL_END;
                  break;
               case PLAY_FEATURE_DELIVERY_FAILED:
                  pfd_install_handle->status  = PLAY_FEATURE_DELIVERY_INSTALL_END;
                  break;
               case PLAY_FEATURE_DELIVERY_DOWNLOADING:
                  task_free_title(task);
                  _len = strlcpy(task_title, msg_hash_to_str(MSG_DOWNLOADING_CORE),
                        sizeof(task_title));
                  strlcpy(task_title + _len,
                        pfd_install_handle->display_name,
                        sizeof(task_title) - _len);
                  task_set_title(task, strdup(task_title));
                  break;
               case PLAY_FEATURE_DELIVERY_INSTALLING:
                  task_free_title(task);
                  _len = strlcpy(task_title, msg_hash_to_str(MSG_INSTALLING_CORE),
                        sizeof(task_title));
                  strlcpy(task_title + _len,
                        pfd_install_handle->display_name,
                        sizeof(task_title) - _len);
                  task_set_title(task, strdup(task_title));
                  break;
               default:
                  break;
            }

            /* If install is inactive, end task (regardless
             * of status) */
            if (!install_active)
               pfd_install_handle->status = PLAY_FEATURE_DELIVERY_INSTALL_END;
         }
         break;
      case PLAY_FEATURE_DELIVERY_INSTALL_END:
         {
            size_t _len;
            uint8_t _flg;
            char task_title[128];
            const char *msg_str = msg_hash_to_str(MSG_CORE_INSTALL_FAILED);

            /* Set final task title */
            task_free_title(task);

            if (pfd_install_handle->success)
               msg_str = pfd_install_handle->core_already_installed ?
                     msg_hash_to_str(MSG_LATEST_CORE_INSTALLED) :
                     msg_hash_to_str(MSG_CORE_INSTALLED);

            _len = strlcpy(task_title, msg_str, sizeof(task_title));
            strlcpy(task_title + _len,
                  pfd_install_handle->display_name,
                  sizeof(task_title) - _len);

            task_set_title(task, strdup(task_title));

            /* Check whether a core backup file was created */
            if (  (pfd_install_handle->backup_core_path && *pfd_install_handle->backup_core_path)
                && path_is_valid(pfd_install_handle->backup_core_path))
            {
               /* If install was successful, delete backup */
               if (pfd_install_handle->success)
                  filestream_delete(pfd_install_handle->backup_core_path);
               else
               {
                  /* Otherwise, attempt to restore backup */
                  int ret = filestream_rename(
                        pfd_install_handle->backup_core_path,
                        pfd_install_handle->local_core_path);

                  /* If restore failed, all we can do is attempt
                   * to delete the backup... */
                  if (ret && path_is_valid(pfd_install_handle->backup_core_path))
                     filestream_delete(pfd_install_handle->backup_core_path);
               }
            }

            /* If task is muted and install failed, set
             * error string (allows status to be checked
             * externally) */
            _flg = task_get_flags(task);

            if (     !pfd_install_handle->success
                && ((_flg & RETRO_TASK_FLG_MUTE) > 0))
               task_set_error(task, strdup(task_title));
         }
         /* fall-through */
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:
   if (task)
   {
      /* Clear the state pointer before the handle is freed.  The
       * worker runs handlers with running_lock released and the task
       * still linked into tasks_running, so a task that has finished
       * here stays visible to retro_task_threaded_find() until the
       * worker retires it.  The finders in this file dereference
       * task->state, so leaving it pointing at freed memory is a
       * use-after-free - task_core_updater_download_finder() strcmps
       * through it, which is a hard crash the moment
       * task_update_installed_cores_handler() pushes the next core. */
      task->state = NULL;
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   }

   if (pfd_install_handle)
      free_play_feature_delivery_install_handle(pfd_install_handle);
}

/* An install that has finished is no longer downloading, even while
 * it waits to be retired: the switch task pushes the next install as
 * soon as the previous one reports completion, which its callback
 * does while the task is still findable. */
static bool task_play_feature_delivery_core_install_finder(
      retro_task_t *task, void *user_data)
{
   return (   task
           && task->handler == task_play_feature_delivery_core_install_handler
           && !(task_get_flags(task) & RETRO_TASK_FLG_FINISHED));
}

static void *task_push_play_feature_delivery_core_install_internal(
      core_updater_list_t* core_list,
      const char *filename,
      bool mute,
      core_updater_sub_task_done_t *install_done)
{
   size_t _len;
   task_finder_data_t find_data;
   char task_title[128];
   const core_updater_list_entry_t *list_entry                = NULL;
   retro_task_t *task                                         = NULL;
   play_feature_delivery_install_handle_t *pfd_install_handle = (play_feature_delivery_install_handle_t*)
         calloc(1, sizeof(play_feature_delivery_install_handle_t));

   /* Sanity check */
   if (   !core_list
       ||  (!filename || !*filename)
       || !pfd_install_handle
       || !play_feature_delivery_enabled())
      goto error;

   /* Get core updater list entry */
   if (!core_updater_list_get_filename(
         core_list, filename, &list_entry))
      goto error;

   if (   (!list_entry->local_core_path || !*list_entry->local_core_path)
       || (!list_entry->display_name || !*list_entry->display_name))
      goto error;

   /* Only one core may be downloaded at a time */
   find_data.func     = task_play_feature_delivery_core_install_finder;
   find_data.userdata = NULL;

   if (task_queue_find(&find_data))
      goto error;

   /* Configure handle */
   pfd_install_handle->core_filename          = strdup(list_entry->remote_filename);
   pfd_install_handle->local_core_path        = strdup(list_entry->local_core_path);
   pfd_install_handle->backup_core_path       = NULL;
   pfd_install_handle->display_name           = strdup(list_entry->display_name);
   pfd_install_handle->success                = false;
   pfd_install_handle->core_already_installed = false;
   pfd_install_handle->status                 = PLAY_FEATURE_DELIVERY_INSTALL_BEGIN;

   /* Create task */
   if (!(task = task_init()))
      goto error;

   /* Configure task */
   _len = strlcpy(task_title, msg_hash_to_str(MSG_UPDATING_CORE),
         sizeof(task_title));
   strlcpy(task_title + _len,
         pfd_install_handle->display_name,
         sizeof(task_title) - _len);

   task->handler          = task_play_feature_delivery_core_install_handler;
   task->state            = pfd_install_handle;
   task->title            = strdup(task_title);
   task->progress         = 0;
   task->progress_cb      = task_window_progress_cb;
   task->callback         = cb_task_core_updater_download;
   task->user_data        = (void*)install_done;
   task->flags           |=  RETRO_TASK_FLG_ALTERNATIVE_LOOK;
   if (mute)
      task->flags        |=  RETRO_TASK_FLG_MUTE;
   else
      task->flags        &= ~RETRO_TASK_FLG_MUTE;

   /* Install process may involve the *deletion*
    * of an existing core file. If core is
    * already running, must therefore unload it
    * to prevent undefined behaviour */
   if (retroarch_ctl(RARCH_CTL_IS_CORE_LOADED, (void*)list_entry->local_core_path))
      command_event(CMD_EVENT_UNLOAD_CORE, NULL);

   /* Push task */
   task_queue_push(task);

   return task;

error:

   /* Clean up task */
   if (task)
   {
      free(task);
      task = NULL;
   }

   /* Clean up handle */
   if (pfd_install_handle)
      free_play_feature_delivery_install_handle(pfd_install_handle);

   return NULL;
}

void *task_push_play_feature_delivery_core_install(
      core_updater_list_t* core_list,
      const char *filename,
      bool mute)
{
   return task_push_play_feature_delivery_core_install_internal(
         core_list, filename, mute, NULL);
}

/************************************************/
/* Play feature delivery switch installed cores */
/************************************************/

static void free_play_feature_delivery_switch_cores_handle(
      play_feature_delivery_switch_cores_handle_t *pfd_switch_cores_handle)
{
   if (pfd_switch_cores_handle->path_dir_libretro)
      free(pfd_switch_cores_handle->path_dir_libretro);

   if (pfd_switch_cores_handle->path_libretro_info)
      free(pfd_switch_cores_handle->path_libretro_info);

   if (pfd_switch_cores_handle->err_msg)
      free(pfd_switch_cores_handle->err_msg);

   /* Cancelled while an install runs: its callback holds the other
    * reference and frees the record when it runs.  The install keeps
    * no reference into core_list. */
   core_updater_sub_task_done_release(
         pfd_switch_cores_handle->install_done);

   core_updater_list_free(pfd_switch_cores_handle->core_list);

   free(pfd_switch_cores_handle);
   pfd_switch_cores_handle = NULL;
}

static void task_play_feature_delivery_switch_cores_handler(
      retro_task_t *task)
{
   uint8_t flg;
   play_feature_delivery_switch_cores_handle_t *pfd_switch_cores_handle = NULL;

   if (!task)
      goto task_finished;

   pfd_switch_cores_handle =
      (play_feature_delivery_switch_cores_handle_t*)task->state;
   flg                     = task_get_flags(task);

   if (!pfd_switch_cores_handle || ((flg & RETRO_TASK_FLG_CANCELLED) > 0))
      goto task_finished;

   switch (pfd_switch_cores_handle->status)
   {
      case PLAY_FEATURE_DELIVERY_SWITCH_CORES_BEGIN:
         {
            /* Query available cores
             * Note: It should never be possible for this
             * function (or the subsequent parsing of its
             * output) to fail. We handle error conditions
             * regardless, but there is no need to perform
             * detailed checking - just report any problems
             * to the user as a generic 'failed to retrieve
             * core list' error */
            struct string_list *available_cores =
                  play_feature_delivery_available_cores();
            bool ret                        = false;

            if (!available_cores)
            {
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_END;
               break;
            }

            /* Populate core updater list */
            ret = core_updater_list_parse_pfd_data(
                  pfd_switch_cores_handle->core_list,
                  pfd_switch_cores_handle->path_dir_libretro,
                  pfd_switch_cores_handle->path_libretro_info,
                  available_cores);

            string_list_free(available_cores);

            /* Cache list size */
            if (ret)
               pfd_switch_cores_handle->list_size =
                     core_updater_list_size(pfd_switch_cores_handle->core_list);

            if (pfd_switch_cores_handle->list_size < 1)
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_END;
            else
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE;
         }
         break;
      case PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE:
         {
            const core_updater_list_entry_t *list_entry = NULL;
            bool core_installed                         = false;

            /* Check whether we have reached the end
             * of the list */
            if (pfd_switch_cores_handle->list_index >=
                  pfd_switch_cores_handle->list_size)
            {
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_END;
               break;
            }

            /* Check whether current core is installed */
            if (core_updater_list_get_index(
                  pfd_switch_cores_handle->core_list,
                  pfd_switch_cores_handle->list_index,
                  &list_entry)
                && path_is_valid(list_entry->local_core_path))
            {
               core_installed                           = true;
               pfd_switch_cores_handle->installed_index =
                     pfd_switch_cores_handle->list_index;
               pfd_switch_cores_handle->status          =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_INSTALL_CORE;
            }

            /* Update progress display */
            task_free_title(task);

            if (core_installed)
            {
               char task_title[128];
               size_t _len = strlcpy(task_title,
                     msg_hash_to_str(MSG_CHECKING_CORE),
                     sizeof(task_title));
               strlcpy(task_title + _len,
                     list_entry->display_name,
                     sizeof(task_title) - _len);
               task_set_title(task, strdup(task_title));
            }
            else
               task_set_title(task, strdup(msg_hash_to_str(MSG_SCANNING_CORES)));

            task_set_progress(task,
                  (pfd_switch_cores_handle->list_index * 100) /
                        pfd_switch_cores_handle->list_size);

            /* Increment list index */
            pfd_switch_cores_handle->list_index++;
         }
         break;
      case PLAY_FEATURE_DELIVERY_SWITCH_CORES_INSTALL_CORE:
         {
            const core_updater_list_entry_t *list_entry = NULL;

            /* Get list entry
             * > In the event of an error, just return
             *   to PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE
             *   state */
            if (!core_updater_list_get_index(
                  pfd_switch_cores_handle->core_list,
                  pfd_switch_cores_handle->installed_index,
                  &list_entry))
            {
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE;
               break;
            }

            /* Check whether core is already installed via
             * play feature delivery */
            if (play_feature_delivery_core_installed(
                  list_entry->remote_filename))
            {
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE;
               break;
            }

            /* Existing core is not installed via
             * play feature delivery
             * > Request installation/replacement
             * > The record must exist *before* the push: the
             *   install task can finish and fire its callback
             *   before this returns */
            if (     (pfd_switch_cores_handle->install_done =
                        core_updater_sub_task_done_new())
                  && !task_push_play_feature_delivery_core_install_internal(
                        pfd_switch_cores_handle->core_list,
                        list_entry->remote_filename,
                        true,
                        pfd_switch_cores_handle->install_done))
            {
               /* No task, so no callback: both references are ours */
               free(pfd_switch_cores_handle->install_done);
               pfd_switch_cores_handle->install_done = NULL;
            }

            /* Again, if an error occurred, just return to
             * PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE
             * state */
            if (!pfd_switch_cores_handle->install_done)
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE;
            else
            {
               size_t _len;
               char task_title[128];

               /* Update task title */
               task_free_title(task);

               _len = strlcpy(task_title,
                     msg_hash_to_str(MSG_UPDATING_CORE),
                     sizeof(task_title));
               strlcpy(task_title + _len,
                     list_entry->display_name,
                     sizeof(task_title) - _len);

               task_set_title(task, strdup(task_title));

               /* Wait for installation to complete */
               pfd_switch_cores_handle->status =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_WAIT_INSTALL;
            }
         }
         break;
      case PLAY_FEATURE_DELIVERY_SWITCH_CORES_WAIT_INSTALL:
         {
            /* Wait for the install task's callback to trigger.
             * The install task itself must not be polled: it is
             * retired and freed in the same gather pass that
             * marks it finished, so this task may only ever see
             * freed memory there */
            core_updater_sub_task_done_t *install_done =
                  pfd_switch_cores_handle->install_done;

            if (!retro_atomic_load_acquire_int(&install_done->complete))
               break;

            /* Check for installation errors
             * > These should be considered 'serious', and
             *   will trigger the task to end early
             * > Otherwise, return to
             *   PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE */
            if (install_done->error)
            {
               pfd_switch_cores_handle->err_msg = install_done->error;
               install_done->error              = NULL;
               pfd_switch_cores_handle->status  =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_END;
            }
            else
               pfd_switch_cores_handle->status  =
                     PLAY_FEATURE_DELIVERY_SWITCH_CORES_ITERATE;

            core_updater_sub_task_done_release(install_done);
            pfd_switch_cores_handle->install_done = NULL;
         }
         break;
      case PLAY_FEATURE_DELIVERY_SWITCH_CORES_END:
         {
            const char *task_title = msg_hash_to_str(MSG_CORE_LIST_FAILED);

            /* Set final task title */
            task_free_title(task);

            /* > Check whether core list was generated
             *   successfully */
            if (pfd_switch_cores_handle->list_size > 0)
            {
               /* Check whether any installation errors occurred */
               if (pfd_switch_cores_handle->err_msg && *pfd_switch_cores_handle->err_msg)
                  task_title = pfd_switch_cores_handle->err_msg;
               else
                  task_title = msg_hash_to_str(MSG_ALL_CORES_SWITCHED_PFD);
            }

            task_set_title(task, strdup(task_title));
         }
         /* fall-through */
      default:
         task_set_progress(task, 100);
         goto task_finished;
   }

   return;

task_finished:
   if (task)
   {
      /* Clear the state pointer before the handle is freed.  The
       * worker runs handlers with running_lock released and the task
       * still linked into tasks_running, so a task that has finished
       * here stays visible to retro_task_threaded_find() until the
       * worker retires it.  The finders in this file dereference
       * task->state, so leaving it pointing at freed memory is a
       * use-after-free - task_core_updater_download_finder() strcmps
       * through it, which is a hard crash the moment
       * task_update_installed_cores_handler() pushes the next core. */
      task->state = NULL;
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   }

   if (pfd_switch_cores_handle)
      free_play_feature_delivery_switch_cores_handle(pfd_switch_cores_handle);
}

static bool task_play_feature_delivery_switch_cores_finder(
      retro_task_t *task, void *user_data)
{
   return (task && task->handler ==
         task_play_feature_delivery_switch_cores_handler);
}

void task_push_play_feature_delivery_switch_installed_cores(
      const char *path_dir_libretro,
      const char *path_libretro_info)
{
   task_finder_data_t find_data;
   retro_task_t *task                                                   = NULL;
   play_feature_delivery_switch_cores_handle_t *pfd_switch_cores_handle =
         (play_feature_delivery_switch_cores_handle_t*)
               calloc(1, sizeof(play_feature_delivery_switch_cores_handle_t));

   /* Sanity check */
   if (    (!path_dir_libretro || !*path_dir_libretro)
       ||  (!path_libretro_info || !*path_libretro_info)
       || !pfd_switch_cores_handle
       || !play_feature_delivery_enabled())
      goto error;

   /* Only one instance of this task my run at a time */
   find_data.func     = task_play_feature_delivery_switch_cores_finder;
   find_data.userdata = NULL;

   if (task_queue_find(&find_data))
      goto error;

   /* Configure handle */
   pfd_switch_cores_handle->path_dir_libretro  = strdup(path_dir_libretro);
   pfd_switch_cores_handle->path_libretro_info = strdup(path_libretro_info);
   pfd_switch_cores_handle->err_msg            = NULL;
   pfd_switch_cores_handle->core_list          = core_updater_list_init();
   pfd_switch_cores_handle->install_done       = NULL;
   pfd_switch_cores_handle->list_size          = 0;
   pfd_switch_cores_handle->list_index         = 0;
   pfd_switch_cores_handle->installed_index    = 0;
   pfd_switch_cores_handle->status             = PLAY_FEATURE_DELIVERY_SWITCH_CORES_BEGIN;

   if (!pfd_switch_cores_handle->core_list)
      goto error;

   /* Create task */
   if (!(task = task_init()))
      goto error;

   /* Configure task */
   task->handler          = task_play_feature_delivery_switch_cores_handler;
   task->state            = pfd_switch_cores_handle;
   task->title            = strdup(msg_hash_to_str(MSG_SCANNING_CORES));
   task->progress         = 0;
   task->progress_cb      = task_window_progress_cb;
   task->flags           |=  RETRO_TASK_FLG_ALTERNATIVE_LOOK;

   /* Push task */
   task_queue_push(task);

   return;

error:
   /* Clean up task */
   if (task)
   {
      free(task);
      task = NULL;
   }

   /* Clean up handle */
   if (pfd_switch_cores_handle)
      free_play_feature_delivery_switch_cores_handle(pfd_switch_cores_handle);
}
#endif
