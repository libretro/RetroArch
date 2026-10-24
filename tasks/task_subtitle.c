/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (task_subtitle.c).
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
#include <retro_atomic.h>
#include <queues/task_queue.h>
#include <streams/file_stream.h>
#include "task_subtitle.h"
#include "tasks_internal.h"

struct task_subtitle_request
{
   char *path, *input;
   RFILE *file;
   rsubtitle_srt_t *track;
   size_t size, done;
   retro_atomic_int_t refs, cancelled, status;
};

static void subtitle_task_close(task_subtitle_request_t *request)
{
   if (request->file)
      filestream_close(request->file);
   request->file = NULL;
   free(request->input);
   request->input = NULL;
}

static void subtitle_task_unref(task_subtitle_request_t *request)
{
   if (retro_atomic_fetch_sub_int(&request->refs, 1) != 1)
      return;
   rsubtitle_srt_free(request->track);
   free(request->path);
   free(request);
}

static void subtitle_task_finish(retro_task_t *task, int status)
{
   task_subtitle_request_t *request = (task_subtitle_request_t*)task->state;
   subtitle_task_close(request);
   if (status < 0)
   {
      rsubtitle_srt_free(request->track);
      request->track = NULL;
   }
   retro_atomic_store_release_int(&request->status, status);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void subtitle_task_handler(retro_task_t *task)
{
   task_subtitle_request_t *request = (task_subtitle_request_t*)task->state;
   if (retro_atomic_load_acquire_int(&request->cancelled)
         || (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
   {
      subtitle_task_finish(task, -1);
      return;
   }
   if (request->track)
   {
      nbio_budget_t budget;
      int status = 0;
      task_nbio_slice_open(&budget);
      while (!status && task_nbio_slice_within_budget(&budget, 0, 0))
      {
         if (retro_atomic_load_acquire_int(&request->cancelled))
            status = -1;
         else
            status = rsubtitle_srt_iterate(request->track, 4096);
      }
      task_nbio_slice_close(&budget);
      if (status)
         subtitle_task_finish(task, status);
      return;
   }
   if (!request->file)
   {
      int64_t size;
      request->file = filestream_open(request->path,
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      size = request->file ? filestream_get_size(request->file) : -1;
      if (size <= 0 || (uint64_t)size > RSUBTITLE_SRT_MAX_INPUT)
      {
         subtitle_task_finish(task, -1);
         return;
      }
      request->size = (size_t)size;
      request->input = (char*)malloc(request->size + 1);
      if (!request->input)
         subtitle_task_finish(task, -1);
      return;
   }
   {
      size_t bytes = request->size - request->done;
      int64_t got;
      if (bytes > 65536)
         bytes = 65536;
      got = filestream_read(request->file, request->input + request->done, bytes);
      if (got <= 0 || (uint64_t)got > bytes)
      {
         subtitle_task_finish(task, -1);
         return;
      }
      request->done += (size_t)got;
      if (request->done == request->size)
      {
         char *input = request->input;
         request->input = NULL;
         subtitle_task_close(request);
         request->track = rsubtitle_srt_begin(input, request->size);
         if (!request->track)
            subtitle_task_finish(task, -1);
      }
   }
}

static void subtitle_task_cleanup(retro_task_t *task)
{
   task_subtitle_request_t *request = (task_subtitle_request_t*)task->state;
   if (!retro_atomic_load_acquire_int(&request->status))
   {
      subtitle_task_close(request);
      rsubtitle_srt_free(request->track);
      request->track = NULL;
      retro_atomic_store_release_int(&request->status, -1);
   }
   subtitle_task_unref(request);
}

task_subtitle_request_t *task_subtitle_load(const char *path)
{
   task_subtitle_request_t *request;
   retro_task_t *task;
   size_t length;
   if (!path || !*path)
      return NULL;
   length = strlen(path);
   if (length == (size_t)-1)
      return NULL;
   request = (task_subtitle_request_t*)calloc(1, sizeof(*request));
   if (!request)
      return NULL;
   request->path = (char*)malloc(length + 1);
   if (!request->path)
   {
      free(request);
      return NULL;
   }
   memcpy(request->path, path, length + 1);
   task = task_init();
   if (!task)
   {
      free(request->path);
      free(request);
      return NULL;
   }
   retro_atomic_int_init(&request->refs, 2);
   retro_atomic_int_init(&request->cancelled, 0);
   retro_atomic_int_init(&request->status, 0);
   task->state = request;
   task->handler = subtitle_task_handler;
   task->cleanup = subtitle_task_cleanup;
   task_set_flags(task, RETRO_TASK_FLG_MUTE, true);
   if (!task_queue_push(task))
   {
      free(request->path);
      free(request);
      free(task);
      return NULL;
   }
   return request;
}

int task_subtitle_status(const task_subtitle_request_t *request)
{
   return request ? retro_atomic_load_acquire_int(&request->status) : -1;
}

rsubtitle_srt_t *task_subtitle_take(task_subtitle_request_t *request)
{
   rsubtitle_srt_t *track;
   if (task_subtitle_status(request) != 1)
      return NULL;
   track = request->track;
   request->track = NULL;
   retro_atomic_store_release_int(&request->status, 2);
   return track;
}

void task_subtitle_release(task_subtitle_request_t *request)
{
   if (!request)
      return;
   retro_atomic_store_release_int(&request->cancelled, 1);
   subtitle_task_unref(request);
}
