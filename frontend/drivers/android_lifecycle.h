/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ANDROID_LIFECYCLE_H
#define __ANDROID_LIFECYCLE_H

#include <stdint.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>
#include <features/features_cpu.h>
#include <rthreads/retro_eventcount.h>

/* What the NativeActivity callbacks on the Java UI thread wait for from
 * the app thread, without a lock.
 *
 * The commands themselves go through the command pipe, which the app
 * thread's looper polls. Every answer comes back through an atomic
 * here - a completion count, the acknowledged activity state, a few
 * flags - stored by the app thread and followed by a notify of the one
 * eventcount every waiter blocks on. A waiter re-tests its own
 * predicate after each wake, so one eventcount serves all of them.
 *
 * The current window is here too: the app thread publishes it, and a
 * thread that uses it across a span (the video context, setting the
 * buffer geometry) holds it with window_acquire/window_release. When
 * the window goes, window_retire() takes it away and waits out the
 * holders, so the UI thread is not told the surface is free while a
 * holder is still on it. */

/* flags */
#define ANDROID_LC_RUNNING        (1 << 0) /* app thread is up         */
#define ANDROID_LC_EXITED         (1 << 1) /* app thread has left      */
#define ANDROID_LC_DESTROYED      (1 << 2) /* destroy has finished     */
#define ANDROID_LC_PERM_RESOLVED  (1 << 3) /* storage permission known */
#define ANDROID_LC_PERM_GRANTED   (1 << 4)

typedef struct android_lifecycle
{
   retro_eventcount_t ec;
   retro_atomic_int_t flags;
   /* Activity state the app thread has acknowledged, -1 for none. */
   retro_atomic_int_t state;
   /* Ticketed commands the app thread has finished: window and input
    * queue changes. Compared wrap-safely against a ticket. */
   retro_atomic_int_t done_seq;
   retro_atomic_ptr_t window;
   retro_atomic_int_t window_users;
   /* The UI thread's alone: tickets handed out. */
   unsigned cmd_seq;
} android_lifecycle_t;

static INLINE bool android_lifecycle_init(android_lifecycle_t *lc)
{
   retro_atomic_store_relaxed_int(&lc->flags, 0);
   retro_atomic_store_relaxed_int(&lc->state, -1);
   retro_atomic_store_relaxed_int(&lc->done_seq, 0);
   retro_atomic_store_relaxed_ptr(&lc->window, NULL);
   retro_atomic_store_relaxed_int(&lc->window_users, 0);
   lc->cmd_seq = 0;
   return retro_eventcount_init(&lc->ec);
}

static INLINE void android_lifecycle_free(android_lifecycle_t *lc)
{
   retro_eventcount_free(&lc->ec);
}

/* ---- the app thread's side, and the permission flags from the UI --- */

static INLINE void android_lifecycle_set_flags(android_lifecycle_t *lc,
      int flags)
{
   (void)retro_atomic_fetch_or_int(&lc->flags, flags);
   retro_eventcount_notify(&lc->ec);
}

static INLINE int android_lifecycle_flags(android_lifecycle_t *lc)
{
   return retro_atomic_load_acquire_int(&lc->flags);
}

static INLINE void android_lifecycle_set_state(android_lifecycle_t *lc,
      int state)
{
   retro_atomic_store_release_int(&lc->state, state);
   retro_eventcount_notify(&lc->ec);
}

static INLINE int android_lifecycle_state(android_lifecycle_t *lc)
{
   return retro_atomic_load_acquire_int(&lc->state);
}

/* A ticketed command is finished. */
static INLINE void android_lifecycle_done(android_lifecycle_t *lc)
{
   (void)retro_atomic_fetch_add_int(&lc->done_seq, 1);
   retro_eventcount_notify(&lc->ec);
}

/* ---- the window ---- */

static INLINE void *android_lifecycle_window(android_lifecycle_t *lc)
{
   return retro_atomic_load_acquire_ptr(&lc->window);
}

/* App thread: a new window. */
static INLINE void android_lifecycle_window_set(android_lifecycle_t *lc,
      void *window)
{
   retro_atomic_store_release_ptr(&lc->window, window);
}

/* The window held until window_release(), or NULL - released already -
 * when there is none. */
static INLINE void *android_lifecycle_window_acquire(
      android_lifecycle_t *lc)
{
   void *w;
   (void)retro_atomic_fetch_add_seq_cst_int(&lc->window_users, 1);
   retro_atomic_thread_fence_seq_cst();
   if (!(w = retro_atomic_load_acquire_ptr(&lc->window)))
   {
      if (retro_atomic_fetch_sub_int(&lc->window_users, 1) == 1)
         retro_eventcount_notify(&lc->ec);
   }
   return w;
}

static INLINE void android_lifecycle_window_release(
      android_lifecycle_t *lc)
{
   if (retro_atomic_fetch_sub_int(&lc->window_users, 1) == 1)
      retro_eventcount_notify(&lc->ec);
}

/* App thread: there is no window any more. Returns once nobody holds
 * the old one. */
static INLINE void android_lifecycle_window_retire(android_lifecycle_t *lc)
{
   (void)retro_atomic_exchange_ptr(&lc->window, NULL);
   retro_atomic_thread_fence_seq_cst();
   for (;;)
   {
      int key;
      if (!retro_atomic_load_seq_cst_int(&lc->window_users))
         return;
      key = retro_eventcount_prepare_wait(&lc->ec);
      if (!retro_atomic_load_seq_cst_int(&lc->window_users))
      {
         retro_eventcount_cancel_wait(&lc->ec);
         return;
      }
      retro_eventcount_commit_wait(&lc->ec, key);
   }
}

/* ---- the UI thread's side ---- */

/* The ticket for a command just posted. */
static INLINE unsigned android_lifecycle_ticket(android_lifecycle_t *lc)
{
   return ++lc->cmd_seq;
}

/* The ticket that is already answered: what a wait gets when nothing
 * was posted. */
static INLINE unsigned android_lifecycle_last_ticket(
      android_lifecycle_t *lc)
{
   return lc->cmd_seq;
}

enum android_lifecycle_until
{
   ANDROID_LC_UNTIL_DONE = 0, /* done_seq has reached the argument */
   ANDROID_LC_UNTIL_STATE,    /* the state is the argument         */
   ANDROID_LC_UNTIL_FLAGS     /* any of the flags in the argument  */
};

static INLINE bool android_lifecycle_holds(android_lifecycle_t *lc,
      enum android_lifecycle_until until, int arg)
{
   switch (until)
   {
      case ANDROID_LC_UNTIL_DONE:
         return (int)((unsigned)retro_atomic_load_acquire_int(
                  &lc->done_seq) - (unsigned)arg) >= 0;
      case ANDROID_LC_UNTIL_STATE:
         return retro_atomic_load_acquire_int(&lc->state) == arg;
      case ANDROID_LC_UNTIL_FLAGS:
         return (retro_atomic_load_acquire_int(&lc->flags) & arg) != 0;
   }
   return false;
}

/* Blocks until the condition holds, the app thread has left (when
 * @stop_on_exit) or @timeout_us passes (negative: no bound). Returns
 * whether the condition holds. */
static INLINE bool android_lifecycle_wait(android_lifecycle_t *lc,
      enum android_lifecycle_until until, int arg, bool stop_on_exit,
      int64_t timeout_us)
{
   retro_time_t deadline = timeout_us >= 0
      ? cpu_features_get_time_usec() + timeout_us : 0;

   for (;;)
   {
      int key;
      if (android_lifecycle_holds(lc, until, arg))
         return true;
      if (stop_on_exit
            && (retro_atomic_load_acquire_int(&lc->flags)
               & ANDROID_LC_EXITED))
         return android_lifecycle_holds(lc, until, arg);

      key = retro_eventcount_prepare_wait(&lc->ec);
      if (     android_lifecycle_holds(lc, until, arg)
            || (stop_on_exit
               && (retro_atomic_load_acquire_int(&lc->flags)
                  & ANDROID_LC_EXITED)))
      {
         retro_eventcount_cancel_wait(&lc->ec);
         continue;
      }
      if (timeout_us < 0)
         retro_eventcount_commit_wait(&lc->ec, key);
      else
      {
         retro_time_t left = deadline - cpu_features_get_time_usec();
         if (left <= 0)
         {
            retro_eventcount_cancel_wait(&lc->ec);
            return android_lifecycle_holds(lc, until, arg);
         }
         retro_eventcount_commit_wait_timeout(&lc->ec, key, left);
      }
   }
}

#endif
