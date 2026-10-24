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

RETRO_BEGIN_DECLS

/* Main-thread owner. No file access occurs until the task handler runs. */
task_subtitle_request_t *task_subtitle_load(const char *path);
/* -1 failed, 0 pending, 1 ready, 2 already taken. */
int task_subtitle_status(const task_subtitle_request_t *request);
rsubtitle_srt_t *task_subtitle_take(task_subtitle_request_t *request);
/* Cancels pending work and releases the caller's reference; never joins. */
void task_subtitle_release(task_subtitle_request_t *request);

RETRO_END_DECLS

#endif
