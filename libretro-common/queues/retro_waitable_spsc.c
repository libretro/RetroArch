/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_waitable_spsc.c).
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

#include <string.h>

#include <retro_waitable_spsc.h>
#include <features/features_cpu.h>

bool retro_waitable_spsc_init(retro_waitable_spsc_t *q,
      size_t min_capacity)
{
   if (!q)
      return false;

   memset(q, 0, sizeof(*q));
   retro_atomic_int_init(&q->cancelled, 0);

   if (!retro_spsc_init(&q->queue, min_capacity))
      return false;

   if (!retro_eventcount_init(&q->readable))
   {
      retro_spsc_free(&q->queue);
      return false;
   }

   if (!retro_eventcount_init(&q->writable))
   {
      retro_eventcount_free(&q->readable);
      retro_spsc_free(&q->queue);
      return false;
   }

   return true;
}

void retro_waitable_spsc_free(retro_waitable_spsc_t *q)
{
   if (!q)
      return;

   retro_eventcount_free(&q->writable);
   retro_eventcount_free(&q->readable);
   retro_spsc_free(&q->queue);
}

void retro_waitable_spsc_notify_readable(retro_waitable_spsc_t *q)
{
   retro_eventcount_notify(&q->readable);
}

void retro_waitable_spsc_notify_writable(retro_waitable_spsc_t *q)
{
   retro_eventcount_notify(&q->writable);
}

size_t retro_waitable_spsc_write_quiet(retro_waitable_spsc_t *q,
      const void *data, size_t bytes)
{
   return retro_spsc_write(&q->queue, data, bytes);
}

size_t retro_waitable_spsc_read_quiet(retro_waitable_spsc_t *q,
      void *data, size_t bytes)
{
   return retro_spsc_read(&q->queue, data, bytes);
}

size_t retro_waitable_spsc_write(retro_waitable_spsc_t *q,
      const void *data, size_t bytes)
{
   size_t n = retro_spsc_write(&q->queue, data, bytes);

   /* Skipped when nothing was written: a write that fitted nothing
    * leaves the queue exactly as the consumer last saw it, and a
    * consumer only parks on a queue with nothing to read - which a
    * queue too full to accept this write is not. There is no wake to
    * lose. */
   if (n)
      retro_eventcount_notify(&q->readable);

   return n;
}

size_t retro_waitable_spsc_read(retro_waitable_spsc_t *q,
      void *data, size_t bytes)
{
   size_t n = retro_spsc_read(&q->queue, data, bytes);

   if (n)
      retro_eventcount_notify(&q->writable);

   return n;
}

void retro_waitable_spsc_cancel(retro_waitable_spsc_t *q)
{
   retro_atomic_store_release_int(&q->cancelled, 1);
   retro_eventcount_notify(&q->readable);
   retro_eventcount_notify(&q->writable);
}

bool retro_waitable_spsc_cancelled(const retro_waitable_spsc_t *q)
{
   return retro_atomic_load_acquire_int(
         (retro_atomic_int_t*)&q->cancelled) != 0;
}

/* The two waits are the same shape: check, register, check again,
 * sleep.  The second check is what makes the registration meaningful,
 * since anything published after it either wakes the sleep or is seen
 * before it starts.
 *
 * The bound is against the clock rather than per sleep, so a run of
 * spurious wakes cannot stretch it. */
#define RETRO_WAITABLE_SPSC_WAIT(q, ec, avail_fn, bytes, timeout_us)          \
   retro_time_t deadline = 0;                                                 \
   bool         bounded  = (timeout_us) > 0;                                  \
                                                                              \
   if (avail_fn(&(q)->queue) >= (bytes))                                      \
      return true;                                                            \
                                                                              \
   if ((timeout_us) <= 0 || retro_waitable_spsc_cancelled(q))                 \
      return false;                                                           \
                                                                              \
   deadline = cpu_features_get_time_usec() + (retro_time_t)(timeout_us);      \
                                                                              \
   for (;;)                                                                   \
   {                                                                          \
      int          key;                                                       \
      retro_time_t now;                                                       \
                                                                              \
      key = retro_eventcount_prepare_wait(&(q)->ec);                          \
                                                                              \
      if (avail_fn(&(q)->queue) >= (bytes))                                   \
      {                                                                       \
         retro_eventcount_cancel_wait(&(q)->ec);                              \
         return true;                                                         \
      }                                                                       \
                                                                              \
      if (retro_waitable_spsc_cancelled(q))                                   \
      {                                                                       \
         retro_eventcount_cancel_wait(&(q)->ec);                              \
         return false;                                                        \
      }                                                                       \
                                                                              \
      now = cpu_features_get_time_usec();                                     \
      if (bounded && now >= deadline)                                         \
      {                                                                       \
         retro_eventcount_cancel_wait(&(q)->ec);                              \
         return avail_fn(&(q)->queue) >= (bytes);                             \
      }                                                                       \
                                                                              \
      retro_eventcount_commit_wait_timeout(&(q)->ec, key,                     \
            (int64_t)(deadline - now));                                       \
   }

bool retro_waitable_spsc_wait_readable(retro_waitable_spsc_t *q,
      size_t bytes, int64_t timeout_us)
{
   RETRO_WAITABLE_SPSC_WAIT(q, readable, retro_spsc_read_avail,
         bytes, timeout_us)
}

bool retro_waitable_spsc_wait_writable(retro_waitable_spsc_t *q,
      size_t bytes, int64_t timeout_us)
{
   RETRO_WAITABLE_SPSC_WAIT(q, writable, retro_spsc_write_avail,
         bytes, timeout_us)
}
