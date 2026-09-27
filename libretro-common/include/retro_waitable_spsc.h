/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_waitable_spsc.h).
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

#ifndef __LIBRETRO_SDK_WAITABLE_SPSC_H
#define __LIBRETRO_SDK_WAITABLE_SPSC_H

/* retro_waitable_spsc.h - an SPSC queue whose two ends can sleep.
 *
 * retro_spsc moves the bytes and retro_eventcount parks a thread; this
 * is the two of them wired together, because every subsystem that
 * wants both otherwise grows its own mutex, condition variable and
 * generation counter to say what the queue already knows.
 *
 * It is a composition and not a change to retro_spsc: a queue whose
 * ends never sleep should not carry two eventcounts, so the plain
 * queue stays plain and this sits on top.
 *
 *   producer                      consumer
 *   --------                      --------
 *   wait_writable(n)              wait_readable(n)
 *   write(n)        -- data -->   read(n)
 *                   <-- space --
 *
 * Usage
 * -----
 *   retro_waitable_spsc_t q;
 *   retro_waitable_spsc_init(&q, 65536);
 *
 *   producer:
 *      while (!retro_waitable_spsc_wait_writable(&q, len, 5000))
 *         if (giving_up()) break;
 *      retro_waitable_spsc_write(&q, buf, len);
 *
 *   consumer:
 *      if (retro_waitable_spsc_wait_readable(&q, len, 5000))
 *         retro_waitable_spsc_read(&q, buf, len);
 *
 *   shutdown:
 *      retro_waitable_spsc_cancel(&q);
 *
 * Rules
 * -----
 * 1. One producer, one consumer, as retro_spsc requires.  The queue is
 *    what the two ends agree on; the eventcounts only decide when a
 *    thread sleeps, so a spurious wake costs a re-check and nothing
 *    else.
 *
 * 2. A waiter is told what it asked for became available, not that it
 *    still is.  Only the thread on that end can consume it, so for the
 *    producer and consumer themselves the answer holds; treat it as a
 *    hint in any other context.
 *
 * 3. Every write notifies and every read notifies.  With nobody parked
 *    that is a handful of nanoseconds, so the common case costs
 *    nothing worth batching for.  With somebody parked it is a wake,
 *    and waking a consumer for each of sixteen small writes is sixteen
 *    wakes: batch the writes, or use the _quiet forms and notify once
 *    when the batch is done.  What the right batch is belongs to the
 *    caller, so this makes no guess.
 *
 * 4. A waiter sleeps until its bytes arrive or its bound expires, and
 *    nothing else it can see changes in between, so teardown needs
 *    retro_waitable_spsc_cancel() rather than a nudge: releasing the
 *    sleep alone only sends it round the loop again.
 */

#include <retro_common_api.h>
#include <retro_spsc.h>
#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>
#include <boolean.h>
#include <stdint.h>

RETRO_BEGIN_DECLS

typedef struct retro_waitable_spsc
{
   retro_spsc_t       queue;
   retro_eventcount_t readable;   /* bytes arrived   */
   retro_eventcount_t writable;   /* space freed     */
   retro_atomic_int_t cancelled;  /* teardown        */
} retro_waitable_spsc_t;

/**
 * retro_waitable_spsc_init:
 * @q            : object to initialise.
 * @min_capacity : as retro_spsc_init.
 *
 * @return true on success.  On failure @q is safe to pass to
 * retro_waitable_spsc_free().
 */
bool retro_waitable_spsc_init(retro_waitable_spsc_t *q,
      size_t min_capacity);

/**
 * retro_waitable_spsc_free:
 * @q : object to release.
 *
 * No thread may be inside any other call on @q.
 */
void retro_waitable_spsc_free(retro_waitable_spsc_t *q);

/**
 * retro_waitable_spsc_write:
 * @q     : queue to write to.
 * @data  : bytes to copy in.
 * @bytes : how many.
 *
 * Writes what fits and tells the consumer.
 *
 * @return bytes written, which may be fewer than asked for.
 */
size_t retro_waitable_spsc_write(retro_waitable_spsc_t *q,
      const void *data, size_t bytes);

/**
 * retro_waitable_spsc_read:
 * @q     : queue to read from.
 * @data  : where to copy to.
 * @bytes : how many.
 *
 * Reads what is there and tells the producer space was freed.
 *
 * @return bytes read, which may be fewer than asked for.
 */
size_t retro_waitable_spsc_read(retro_waitable_spsc_t *q,
      void *data, size_t bytes);

/**
 * retro_waitable_spsc_write_quiet:
 * retro_waitable_spsc_read_quiet:
 *
 * As above but without the notification, for a caller batching several
 * transfers into one wake.  Pair with the matching notify below, and
 * do not leave a batch unannounced.
 */
size_t retro_waitable_spsc_write_quiet(retro_waitable_spsc_t *q,
      const void *data, size_t bytes);
size_t retro_waitable_spsc_read_quiet(retro_waitable_spsc_t *q,
      void *data, size_t bytes);

void retro_waitable_spsc_notify_readable(retro_waitable_spsc_t *q);
void retro_waitable_spsc_notify_writable(retro_waitable_spsc_t *q);

/**
 * retro_waitable_spsc_wait_readable:
 * @q          : queue to wait on.
 * @bytes      : how many the caller wants to read.
 * @timeout_us : bound in microseconds; zero or less polls.
 *
 * Sleeps until at least @bytes can be read, or the bound expires.
 *
 * @return true if the bytes are there.
 */
bool retro_waitable_spsc_wait_readable(retro_waitable_spsc_t *q,
      size_t bytes, int64_t timeout_us);

/**
 * retro_waitable_spsc_wait_writable:
 * @q          : queue to wait on.
 * @bytes      : how many the caller wants to write.
 * @timeout_us : bound in microseconds; zero or less polls.
 *
 * Sleeps until at least @bytes can be written, or the bound expires.
 *
 * @return true if the space is there.
 */
bool retro_waitable_spsc_wait_writable(retro_waitable_spsc_t *q,
      size_t bytes, int64_t timeout_us);

/**
 * retro_waitable_spsc_cancel:
 * @q : queue to shut down.
 *
 * Releases both ends and makes every wait from here on return false at
 * once, whatever the queue holds.  For teardown, and not reversible;
 * the bytes already in the queue are left alone and can still be read
 * with the _quiet form.
 */
void retro_waitable_spsc_cancel(retro_waitable_spsc_t *q);

/**
 * retro_waitable_spsc_cancelled:
 * @q : queue to ask about.
 *
 * @return true once retro_waitable_spsc_cancel() has been called.
 */
bool retro_waitable_spsc_cancelled(const retro_waitable_spsc_t *q);

RETRO_END_DECLS

#endif
