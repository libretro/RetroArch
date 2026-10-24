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
#include <retro_dirent.h>
#include <string/stdstring.h>
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

#define SUBTITLE_CATALOG_LIMIT 64

struct task_subtitle_catalog
{
   struct RDIR *directory;
   char *folder, *stem;
   char *paths[SUBTITLE_CATALOG_LIMIT];
   size_t count, stem_length;
   retro_atomic_int_t refs, cancelled, status;
};

static void subtitle_catalog_unref(task_subtitle_catalog_t *catalog)
{
   size_t i;
   if (retro_atomic_fetch_sub_int(&catalog->refs, 1) != 1)
      return;
   for (i = 0; i < catalog->count; i++)
      free(catalog->paths[i]);
   free(catalog->folder);
   free(catalog->stem);
   free(catalog);
}

static void subtitle_catalog_finish(retro_task_t *task, int status)
{
   task_subtitle_catalog_t *catalog = (task_subtitle_catalog_t*)task->state;
   retro_closedir(catalog->directory);
   catalog->directory = NULL;
   retro_atomic_store_release_int(&catalog->status, status);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static bool subtitle_catalog_add(task_subtitle_catalog_t *catalog,
      const char *name)
{
   size_t length = strlen(name), folder_length, index, i;
   char *path;
   if (length < catalog->stem_length + 4
#if defined(_WIN32)
         || !string_starts_with_case_insensitive(name, catalog->stem)
#else
         || memcmp(name, catalog->stem, catalog->stem_length)
#endif
         || name[catalog->stem_length] != '.'
         || !string_is_equal_noncase(name + length - 4, ".srt"))
      return true;
   if (retro_dirent_is_dir(catalog->directory, NULL))
      return true;
   folder_length = strlen(catalog->folder);
   if (length > 32768 - folder_length)
      return true;
   path = (char*)malloc(folder_length + length + 1);
   if (!path)
      return false;
   memcpy(path, catalog->folder, folder_length);
   memcpy(path + folder_length, name, length + 1);
   for (index = 0; index < catalog->count; index++)
      if (strcmp(path, catalog->paths[index]) < 0)
         break;
   if (index == SUBTITLE_CATALOG_LIMIT)
   {
      free(path);
      return true;
   }
   if (catalog->count == SUBTITLE_CATALOG_LIMIT)
      free(catalog->paths[catalog->count - 1]);
   else
      catalog->count++;
   for (i = catalog->count - 1; i > index; i--)
      catalog->paths[i] = catalog->paths[i - 1];
   catalog->paths[index] = path;
   return true;
}

static void subtitle_catalog_handler(retro_task_t *task)
{
   task_subtitle_catalog_t *catalog = (task_subtitle_catalog_t*)task->state;
   nbio_budget_t budget;
   unsigned entries = 0;
   int status = 0;
   if (!catalog->directory)
   {
      if (retro_atomic_load_acquire_int(&catalog->cancelled))
      {
         subtitle_catalog_finish(task, -1);
         return;
      }
      catalog->directory = retro_opendir(*catalog->folder ? catalog->folder : ".");
      if (!catalog->directory)
      {
         subtitle_catalog_finish(task, -1);
         return;
      }
   }
   task_nbio_slice_open(&budget);
   while (entries++ < 32 && task_nbio_slice_within_budget(&budget, 0, 0))
   {
      const char *name;
      if (retro_atomic_load_acquire_int(&catalog->cancelled)
            || (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
      {
         status = -1;
         break;
      }
      if (!retro_readdir(catalog->directory))
      {
         status = 1;
         break;
      }
      name = retro_dirent_get_name(catalog->directory);
      if (name && !subtitle_catalog_add(catalog, name))
      {
         status = -1;
         break;
      }
   }
   task_nbio_slice_close(&budget);
   if (status)
      subtitle_catalog_finish(task, status);
}

static void subtitle_catalog_cleanup(retro_task_t *task)
{
   task_subtitle_catalog_t *catalog = (task_subtitle_catalog_t*)task->state;
   if (!retro_atomic_load_acquire_int(&catalog->status))
      subtitle_catalog_finish(task, -1);
   subtitle_catalog_unref(catalog);
}

task_subtitle_catalog_t *task_subtitle_discover(const char *movie_path)
{
   task_subtitle_catalog_t *catalog;
   retro_task_t *task;
   size_t length, start, end;
   if (!movie_path || !*movie_path || (length = strlen(movie_path)) > 32768)
      return NULL;
   start = length;
   while (start && movie_path[start - 1] != '/' && movie_path[start - 1] != '\\')
      start--;
   end = length;
   while (end > start && movie_path[end - 1] != '.')
      end--;
   if (end > start)
      end--;
   else
      end = length;
   if (end == start)
      return NULL;
   catalog = (task_subtitle_catalog_t*)calloc(1, sizeof(*catalog));
   if (!catalog)
      return NULL;
   retro_atomic_int_init(&catalog->refs, 1);
   retro_atomic_int_init(&catalog->cancelled, 0);
   retro_atomic_int_init(&catalog->status, 0);
   catalog->folder = (char*)malloc(start + 1);
   catalog->stem_length = end - start;
   catalog->stem = (char*)malloc(catalog->stem_length + 1);
   if (!catalog->folder || !catalog->stem)
   {
      subtitle_catalog_unref(catalog);
      return NULL;
   }
   memcpy(catalog->folder, movie_path, start);
   catalog->folder[start] = '\0';
   memcpy(catalog->stem, movie_path + start, catalog->stem_length);
   catalog->stem[catalog->stem_length] = '\0';
   task = task_init();
   if (!task)
   {
      subtitle_catalog_unref(catalog);
      return NULL;
   }
   task->state = catalog;
   task->handler = subtitle_catalog_handler;
   task->cleanup = subtitle_catalog_cleanup;
   task_set_flags(task, RETRO_TASK_FLG_MUTE, true);
   retro_atomic_fetch_add_int(&catalog->refs, 1);
   if (!task_queue_push(task))
   {
      free(task);
      subtitle_catalog_unref(catalog);
      subtitle_catalog_unref(catalog);
      return NULL;
   }
   return catalog;
}

int task_subtitle_catalog_status(const task_subtitle_catalog_t *catalog)
{
   return catalog ? retro_atomic_load_acquire_int(&catalog->status) : -1;
}

size_t task_subtitle_catalog_count(const task_subtitle_catalog_t *catalog)
{
   return task_subtitle_catalog_status(catalog) == 1 ? catalog->count : 0;
}

const char *task_subtitle_catalog_path(const task_subtitle_catalog_t *catalog,
      size_t index)
{
   return index < task_subtitle_catalog_count(catalog) ? catalog->paths[index] : NULL;
}

void task_subtitle_catalog_release(task_subtitle_catalog_t *catalog)
{
   if (!catalog)
      return;
   retro_atomic_store_release_int(&catalog->cancelled, 1);
   subtitle_catalog_unref(catalog);
}
