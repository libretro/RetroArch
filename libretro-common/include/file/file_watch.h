/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_watch.h).
 * ---------------------------------------------------------------------------------------
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

#ifndef __LIBRETRO_SDK_FILE_WATCH_H
#define __LIBRETRO_SDK_FILE_WATCH_H

#include <boolean.h>
#include <lists/string_list.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Events a watch can report.  Platforms may only be able to
 * distinguish some of these. */
enum file_watch_event
{
   FILE_WATCH_EVENT_MODIFIED          = (1 << 0),
   FILE_WATCH_EVENT_WRITE_FILE_CLOSED = (1 << 1),
   FILE_WATCH_EVENT_FILE_MOVED        = (1 << 2),
   FILE_WATCH_EVENT_FILE_DELETED      = (1 << 3)
};

typedef struct file_watch file_watch_t;

/**
 * file_watch_supported:
 *
 * @return true if this platform can watch files for changes.
 **/
bool file_watch_supported(void);

/**
 * file_watch_new:
 * @paths      : list of file paths to watch.
 * @event_mask : bitmask of file_watch_event values to report.
 *
 * Starts watching the given files.  The list is copied; the caller
 * keeps ownership of @paths.
 *
 * @return a watch handle, or NULL if watching is unsupported on this
 * platform or setup failed.
 **/
file_watch_t *file_watch_new(struct string_list *paths, int event_mask);

/**
 * file_watch_poll:
 * @watch      : watch handle from file_watch_new.
 *
 * Non-blocking; intended to be called from a per-frame loop.
 *
 * @return true if any watched file reported a requested event since
 * the last poll.
 **/
bool file_watch_poll(file_watch_t *watch);

/**
 * file_watch_free:
 * @watch      : watch handle from file_watch_new, may be NULL.
 *
 * Stops watching and releases the handle.
 **/
void file_watch_free(file_watch_t *watch);

RETRO_END_DECLS

#endif
