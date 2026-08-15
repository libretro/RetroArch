/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (task_content_prefetch.c).
 * ---------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <formats/data_transfer.h>
#ifdef HAVE_COMPRESSION
#include <file/archive_file.h>
#endif

#include <queues/task_queue.h>

#include "task_content_prefetch.h"
#include "tasks_internal.h"

/* Bytes pumped per task tick, per file.  Sized so a frame's tick
 * costs a few milliseconds of read+inflate, keeping the frontend
 * responsive while a large ROM streams in over a second or two. */
#define CONTENT_PREFETCH_TICK_BYTES (4 * 1024 * 1024)

struct content_prefetch_item
{
   char *path;
   size_t total;                       /* bytes this item will yield */
   size_t done;                        /* bytes it has yielded       */
   data_transfer_t *dt;
   RFILE *file;                        /* plain-file source        */
#ifdef HAVE_COMPRESSION
   file_archive_entry_source_t *src;   /* archive-entry source     */
#endif
   uint8_t *out;                       /* completed buffer, held   */
   size_t   out_len;                   /* until the task callback  */
   uint8_t opened;
   uint8_t finished;
};

struct content_prefetch_state
{
   struct content_prefetch_item *items;
   size_t count;
   size_t cursor;                      /* one file at a time       */
   content_prefetch_deposit_t deposit;
   content_prefetch_done_t done;
   content_prefetch_progress_t progress_cb;
   void *ud;
   size_t bytes_total;                 /* across items, once opened  */
   size_t bytes_done;
   int8_t progress;                    /* last value reported        */
   uint8_t all_ok;
};

/* Report read progress as a whole-number percentage of the bytes
 * this task will move.
 *
 * Sizes are learned per item as each is opened, not up front - an
 * archive entry's uncompressed size is only known once its header is
 * read - so the denominator grows as the run proceeds.  For the
 * single-file case the frontend actually defers (see
 * task_content_defer_menu_load), it is exact from the first tick.
 *
 * The value is pushed only when the integer percentage changes: the
 * task queue wakes a progress callback on every update, and a 4 MiB
 * tick against a large ROM would otherwise report the same number
 * many times over. */
static void content_prefetch_update_progress(retro_task_t *task,
      struct content_prefetch_state *st)
{
   int8_t pct;

   if (!st->bytes_total)
      return;

   pct = (int8_t)((st->bytes_done * 100) / st->bytes_total);

   if (pct == st->progress)
      return;

   st->progress = pct;
   task_set_progress(task, pct);

   if (st->progress_cb)
      st->progress_cb(st->ud, pct);
}

static int64_t content_prefetch_file_read(void *ud, uint8_t *dst,
      size_t n)
{
   return filestream_read((RFILE*)ud, dst, (int64_t)n);
}

#ifdef HAVE_COMPRESSION
static int64_t content_prefetch_entry_read(void *ud, uint8_t *dst,
      size_t n)
{
   return file_archive_entry_source_read(
         (file_archive_entry_source_t*)ud, dst, (int64_t)n);
}
#endif

static void content_prefetch_item_close(struct content_prefetch_item *it)
{
   if (it->dt)
   {
      data_transfer_free(it->dt);
      it->dt = NULL;
   }
   if (it->file)
   {
      filestream_close(it->file);
      it->file = NULL;
   }
#ifdef HAVE_COMPRESSION
   if (it->src)
   {
      file_archive_entry_source_close(it->src);
      it->src = NULL;
   }
#endif
}

/* Open the item's source lazily, on the tick that reaches it. */
static bool content_prefetch_item_open(struct content_prefetch_item *it)
{
   it->opened = 1;

#ifdef HAVE_COMPRESSION
   if (path_contains_compressed_file(it->path))
   {
      int64_t usize = 0;
      if (!(it->src = file_archive_entry_source_open(it->path, &usize)))
         return false;                 /* 7z etc.: skip, not fail */
      it->total = (usize > 0) ? (size_t)usize : 0;
      if (usize <= 0
            || !(it->dt = data_transfer_open_source((size_t)usize,
                  content_prefetch_entry_read, it->src)))
      {
         content_prefetch_item_close(it);
         return false;
      }
      return true;
   }
#endif
   {
      int64_t sz = path_get_size(it->path);
      if (sz <= 0)
         return false;
      it->total  = (size_t)sz;
      if (!(it->file = filestream_open(it->path,
            RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         return false;
      if (!(it->dt = data_transfer_open_source((size_t)sz,
            content_prefetch_file_read, it->file)))
      {
         content_prefetch_item_close(it);
         return false;
      }
   }
   return true;
}

/* The handler runs wherever the task queue runs it - the worker
 * thread, under the threaded queue - so it only reads and stores.
 * Deposits and done are delivered by content_prefetch_task_callback,
 * which the queue invokes from task_queue_check() on the thread
 * that pumps it. */
static void content_prefetch_handler(retro_task_t *task)
{
   struct content_prefetch_state *st =
         (struct content_prefetch_state*)task->state;
   struct content_prefetch_item *it;

   if ((task_get_flags(task) & RETRO_TASK_FLG_CANCELLED) > 0)
   {
      st->all_ok = 0;
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   if (st->cursor >= st->count)
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   it = &st->items[st->cursor];

   if (!it->opened)
   {
      if (content_prefetch_item_open(it))
         st->bytes_total += it->total;
      else
      {
         /* skipped: the load's ordinary read path covers it */
         st->all_ok = 0;
         it->finished = 1;
         content_prefetch_item_close(it);
         st->cursor++;
         return;
      }
   }

   data_transfer_iterate(it->dt, CONTENT_PREFETCH_TICK_BYTES);

   {
      /* Charge the delta rather than the absolute, so the running
       * total stays right across several items. */
      size_t avail = data_transfer_avail(it->dt);
      if (avail > it->done)
      {
         st->bytes_done += avail - it->done;
         it->done        = avail;
      }
      content_prefetch_update_progress(task, st);
   }

   if (data_transfer_failed(it->dt))
   {
      st->all_ok = 0;
      it->finished = 1;
      content_prefetch_item_close(it);
      st->cursor++;
      return;
   }
   if (data_transfer_complete(it->dt))
   {
      size_t len   = 0;
      uint8_t *out = data_transfer_source_detach(it->dt, &len);
      it->dt       = NULL;
      if (out)
      {
         it->out     = out;
         it->out_len = len;
      }
      else
         st->all_ok = 0;
      it->finished = 1;
      content_prefetch_item_close(it);
      st->cursor++;
   }
}

/* Invoked by the task queue from task_queue_check(), on the thread
 * that pumps the queue, after the handler set FINISHED: hand every
 * held buffer to the deposit callback (ownership transfers), then
 * fire done exactly once.  Cleanup runs right after this and frees
 * whatever was not handed off. */
static void content_prefetch_task_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   struct content_prefetch_state *st =
         (struct content_prefetch_state*)task->state;
   size_t i;

   if (!st)
      return;
   for (i = 0; i < st->count; i++)
   {
      struct content_prefetch_item *it = &st->items[i];
      if (it->out)
      {
         st->deposit(st->ud, it->path, it->out, it->out_len);
         it->out     = NULL;   /* ownership transferred */
         it->out_len = 0;
      }
   }
   if (st->done)
      st->done(st->ud, st->all_ok ? true : false);
}

static void content_prefetch_cleanup(retro_task_t *task)
{
   struct content_prefetch_state *st =
         (struct content_prefetch_state*)task->state;
   size_t i;
   if (!st)
      return;
   for (i = 0; i < st->count; i++)
   {
      content_prefetch_item_close(&st->items[i]);
      free(st->items[i].out);          /* not handed off, if any */
      free(st->items[i].path);
   }
   free(st->items);
   free(st);
}

bool task_push_content_prefetch(const char **paths, size_t count,
      content_prefetch_deposit_t deposit, content_prefetch_done_t done,
      void *ud)
{
   return task_push_content_prefetch_progress(paths, count, deposit,
         done, NULL, ud);
}

bool task_push_content_prefetch_progress(const char **paths,
      size_t count, content_prefetch_deposit_t deposit,
      content_prefetch_done_t done,
      content_prefetch_progress_t progress, void *ud)
{
   struct content_prefetch_state *st = NULL;
   retro_task_t *t                   = NULL;
   size_t i;

   if (!paths || !count || !deposit)
      return false;
   if (!(t = task_init()))
      return false;
   if (!(st = (struct content_prefetch_state*)calloc(1, sizeof(*st))))
      goto error;
   if (!(st->items = (struct content_prefetch_item*)
         calloc(count, sizeof(*st->items))))
      goto error;
   for (i = 0; i < count; i++)
   {
      if (!(st->items[i].path = strdup(paths[i])))
         goto error;
      st->count++;
   }
   /* -1 so the first real value - including a 0%% at the start of a
    * large read - is reported rather than matching the initial. */
   st->progress    = -1;
   st->deposit     = deposit;
   st->progress_cb = progress;
   st->done    = done;
   st->ud      = ud;
   st->all_ok  = 1;

   t->state    = st;
   t->handler  = content_prefetch_handler;
   t->callback = content_prefetch_task_callback;
   t->cleanup  = content_prefetch_cleanup;
   t->flags   |= RETRO_TASK_FLG_MUTE;
   task_queue_push(t);
   return true;

error:
   if (st)
   {
      for (i = 0; i < st->count; i++)
         free(st->items[i].path);
      free(st->items);
      free(st);
   }
   free(t);
   return false;
}
