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
   data_transfer_t *dt;
   RFILE *file;                        /* plain-file source        */
#ifdef HAVE_COMPRESSION
   file_archive_entry_source_t *src;   /* archive-entry source     */
#endif
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
   void *ud;
   uint8_t all_ok;
};

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

static void content_prefetch_handler(retro_task_t *task)
{
   struct content_prefetch_state *st =
         (struct content_prefetch_state*)task->state;
   struct content_prefetch_item *it;

   if (st->cursor >= st->count)
   {
      if (st->done)
         st->done(st->ud, st->all_ok ? true : false);
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   it = &st->items[st->cursor];

   if (!it->opened)
   {
      if (!content_prefetch_item_open(it))
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
         st->deposit(st->ud, it->path, out, len);
      else
         st->all_ok = 0;
      it->finished = 1;
      content_prefetch_item_close(it);
      st->cursor++;
   }
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
      free(st->items[i].path);
   }
   free(st->items);
   free(st);
}

bool task_push_content_prefetch(const char **paths, size_t count,
      content_prefetch_deposit_t deposit, content_prefetch_done_t done,
      void *ud)
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
   st->deposit = deposit;
   st->done    = done;
   st->ud      = ud;
   st->all_ok  = 1;

   t->state    = st;
   t->handler  = content_prefetch_handler;
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
