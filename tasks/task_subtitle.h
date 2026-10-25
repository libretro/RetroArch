/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (task_subtitle.h).
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

#ifndef TASK_SUBTITLE_H
#define TASK_SUBTITLE_H

#include <formats/rsubtitle_srt.h>

typedef struct task_subtitle_request task_subtitle_request_t;

typedef struct task_subtitle_catalog task_subtitle_catalog_t;

enum task_subtitle_source
{
   TASK_SUBTITLE_SIDECAR = 0,
   TASK_SUBTITLE_MP4,
   TASK_SUBTITLE_WEBM
};

typedef struct task_subtitle_entry
{
   const char *path;
   enum task_subtitle_source source;
   int track;
} task_subtitle_entry_t;

RETRO_BEGIN_DECLS

/* Main-thread owner. No file access occurs until the task handler runs. */
task_subtitle_request_t *task_subtitle_load(const char *path);
/* Container track index; decoding and file access stay on the worker. */
task_subtitle_request_t *task_subtitle_load_embedded(const char *path, int track);
task_subtitle_request_t *task_subtitle_load_entry(const task_subtitle_entry_t *entry);
/* -1 failed, 0 pending, 1 ready, 2 already taken. */
int task_subtitle_status(const task_subtitle_request_t *request);
rsubtitle_srt_t *task_subtitle_take(task_subtitle_request_t *request);
/* Cancels pending work and releases the caller's reference; never joins. */
void task_subtitle_release(task_subtitle_request_t *request);

/* Bounded, asynchronous same-movie sidecar discovery. */
task_subtitle_catalog_t *task_subtitle_discover(const char *movie_path);
int task_subtitle_catalog_status(const task_subtitle_catalog_t *catalog);
size_t task_subtitle_catalog_count(const task_subtitle_catalog_t *catalog);
/* Borrowed until release; available only after discovery completes. */
const char *task_subtitle_catalog_path(const task_subtitle_catalog_t *catalog,
      size_t index);
const task_subtitle_entry_t *task_subtitle_catalog_entry(
      const task_subtitle_catalog_t *catalog, size_t index);
void task_subtitle_catalog_release(task_subtitle_catalog_t *catalog);

RETRO_END_DECLS

#endif
