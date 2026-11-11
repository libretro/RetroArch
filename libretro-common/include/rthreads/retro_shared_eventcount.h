/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_shared_eventcount.h).
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_SHARED_EVENTCOUNT_H
#define __LIBRETRO_SDK_SHARED_EVENTCOUNT_H

/* retro_shared_eventcount.h - an eventcount between processes.
 *
 * retro_eventcount.h's protocol, with its state in memory that two or
 * more processes map: a process parks a thread until another process
 * (or a thread of its own) publishes, with no lock on the publisher's
 * path and no polling.  It is what two processes that share a ring
 * through a mapping use in place of a timer.
 *
 * The state, retro_shared_eventcount_t, lives in the shared memory and
 * starts zeroed (or through retro_shared_eventcount_init by whoever
 * makes the mapping).  Each process attaches to it once, giving the
 * same name in every process, and uses its own handle from then on:
 *
 *   creator:  retro_shared_eventcount_init(&map->ready);
 *   each:     retro_shared_eventcount_attach(&ready, &map->ready,
 *                                            "my_mapping_ready");
 *
 * The waiting side is retro_eventcount's loop:
 *
 *   for (;;)
 *   {
 *      int key;
 *      if (have_work())
 *         break;
 *      key = retro_shared_eventcount_prepare_wait(&ready);
 *      if (have_work())
 *      {
 *         retro_shared_eventcount_cancel_wait(&ready);
 *         break;
 *      }
 *      retro_shared_eventcount_commit_wait(&ready, key);
 *   }
 *
 * and the publishing side, after publishing, in any attached process:
 *
 *   retro_shared_eventcount_notify(&ready);
 *
 * which is one sequentially-consistent read-modify-write and one load
 * when nobody is parked.  A wait may return with nothing published, as
 * a condition variable's may, which is why the waiting shape is a loop.
 * The rules of retro_eventcount.h hold as written.
 *
 * Backends
 * --------
 *   Linux / Android   futex on the shared word (FUTEX_WAIT / FUTEX_WAKE,
 *                     not the _PRIVATE forms); the name is not used
 *   Windows (desktop) a named semaphore, one per object, which each
 *                     process opens under the name; a notify releases
 *                     one permit per thread parked or about to park
 *   anything else     attach fails: the platform has no wait between
 *                     processes here, and the caller keeps its own
 *                     fallback
 *
 * Both sides must be the same build, or at least agree on the layout of
 * retro_shared_eventcount_t, and the atomics must be lock-free
 * (RETRO_ATOMIC_LOCK_FREE): attach fails where they are not.
 */

#include <retro_common_api.h>
#include <retro_atomic.h>
#include <boolean.h>
#include <stdint.h>

RETRO_BEGIN_DECLS

/* The part in shared memory.  Zeroed is the initial state. */
typedef struct retro_shared_eventcount
{
   retro_atomic_int_t epoch;   /* bumped once per notify              */
   retro_atomic_int_t waiters; /* threads inside a prepare/commit window */
} retro_shared_eventcount_t;

/* One process's handle onto it. */
typedef struct retro_shared_eventcount_handle
{
   retro_shared_eventcount_t *shared;
   void                      *os; /* the Windows semaphore; NULL elsewhere */
} retro_shared_eventcount_handle_t;

/**
 * retro_shared_eventcount_init:
 * @shared : the state, in shared memory.
 *
 * Puts @shared in its initial state; the same as zeroing it.  Call
 * before any process attaches.
 */
void retro_shared_eventcount_init(retro_shared_eventcount_t *shared);

/**
 * retro_shared_eventcount_attach:
 * @handle : this process's handle, filled in.
 * @shared : the state, in memory this process maps.
 * @name   : the object's name, the same in every process and unique
 *           among the objects of the session (Windows names a kernel
 *           object with it; ASCII, no backslash).
 *
 * @return true if this process can wait on and notify @shared.  False
 * where the platform has no wait between processes, or the object
 * could not be made; @handle is then left safe to detach.
 */
bool retro_shared_eventcount_attach(retro_shared_eventcount_handle_t *handle,
      retro_shared_eventcount_t *shared, const char *name);

/**
 * retro_shared_eventcount_detach:
 * @handle : the handle.
 *
 * Releases what attach took in this process.  No thread of this
 * process may be inside a call on @handle.  The shared state is left
 * as it is for the other processes.
 */
void retro_shared_eventcount_detach(retro_shared_eventcount_handle_t *handle);

/**
 * retro_shared_eventcount_notify:
 * @handle : the handle.
 *
 * Releases every thread of every process parked on the object, and
 * makes any wait whose prepare has already run return.  Call after the
 * work is published.
 */
void retro_shared_eventcount_notify(retro_shared_eventcount_handle_t *handle);

/**
 * retro_shared_eventcount_prepare_wait:
 * @handle : the handle.
 *
 * Opens a wait window.  Re-check the condition after it: a notify from
 * here on either shows in that check or makes the commit return.
 *
 * @return the key for commit_wait.
 */
int retro_shared_eventcount_prepare_wait(
      retro_shared_eventcount_handle_t *handle);

/**
 * retro_shared_eventcount_cancel_wait:
 * @handle : the handle.
 *
 * Closes the window without sleeping, when the re-check found work.
 */
void retro_shared_eventcount_cancel_wait(
      retro_shared_eventcount_handle_t *handle);

/**
 * retro_shared_eventcount_commit_wait:
 * @handle : the handle.
 * @key    : prepare_wait's key.
 *
 * Sleeps until a notify after the prepare, unless one has come already,
 * and closes the window.
 */
void retro_shared_eventcount_commit_wait(
      retro_shared_eventcount_handle_t *handle, int key);

/**
 * retro_shared_eventcount_commit_wait_timeout:
 * @handle  : the handle.
 * @key     : prepare_wait's key.
 * @timeout_us : longest sleep in microseconds; 0 does not sleep.
 *
 * As commit_wait, sleeping at most @timeout_us (to the platform's
 * granularity: a millisecond on Windows, rounded up).
 *
 * @return true if a notify came after the prepare, false if the time
 * ran out first.
 */
bool retro_shared_eventcount_commit_wait_timeout(
      retro_shared_eventcount_handle_t *handle, int key, int64_t timeout_us);

RETRO_END_DECLS

#endif
