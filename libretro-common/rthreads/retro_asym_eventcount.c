/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_asym_eventcount.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* See the header for the design. This file is the two protocols and the
 * park.
 *
 * The park is a condition variable everywhere. That is deliberate for a
 * first version: it is the cold side, a condvar is portable to every
 * backend rthreads has, and the correctness argument is short --
 * commit_wait re-checks the epoch under the lock and notify broadcasts
 * under the same lock, so a wake cannot fall between the check and the
 * sleep. A futex park on the epoch word would save a lock on the wake,
 * and can replace this once the primitive has been on real SMP hardware.
 */

#include <stdlib.h>
#include <string.h>

#include <rthreads/rthreads.h>
#include <rthreads/retro_procbarrier.h>
#include <rthreads/retro_asym_eventcount.h>

bool retro_asym_eventcount_init(retro_asym_eventcount_t *ec)
{
   if (!ec)
      return false;
   memset(ec, 0, sizeof(*ec));

   ec->lock = slock_new();
   if (!ec->lock)
      return false;
   ec->cond = scond_new();
   if (!ec->cond)
   {
      slock_free(ec->lock);
      ec->lock = NULL;
      return false;
   }

   retro_atomic_store_relaxed_int(&ec->epoch, 0);
   retro_atomic_store_relaxed_int(&ec->waiters, 0);

   /* Decide the protocol once. A platform with no process-wide barrier
    * runs the symmetric pair for this object's whole life; alternating
    * would leave one side ordered and the other not. */
   ec->asymmetric = (retro_procbarrier_init(0) != RETRO_PROCBARRIER_NONE);
   return true;
}

void retro_asym_eventcount_free(retro_asym_eventcount_t *ec)
{
   if (!ec)
      return;
   if (ec->cond)
      scond_free(ec->cond);
   if (ec->lock)
      slock_free(ec->lock);
   ec->cond = NULL;
   ec->lock = NULL;
}

int retro_asym_eventcount_is_asymmetric(const retro_asym_eventcount_t *ec)
{
   return ec ? ec->asymmetric : 0;
}

/* ------------------------------------------------------------------ */
/* Producer                                                            */
/* ------------------------------------------------------------------ */

void retro_asym_eventcount_notify(retro_asym_eventcount_t *ec)
{
   if (ec->asymmetric)
   {
      /* Release, so the data published before this call is visible to a
       * waiter that acquires the new epoch. Plain otherwise: no lock
       * prefix, no fence. The single-producer condition in the header is
       * what makes the non-atomic increment sound. */
      int e = retro_atomic_load_relaxed_int(&ec->epoch);
      retro_atomic_store_release_int(&ec->epoch, (int)((unsigned)e + 1u));

      /* Relaxed. The barrier in prepare_wait is what orders this against
       * a waiter's registration; a fence here would put the cost back
       * where this primitive exists to remove it. */
      if (retro_atomic_load_relaxed_int(&ec->waiters) == 0)
         return;
   }
   else
   {
      /* No barrier available: the symmetric pair, exactly as
       * retro_eventcount does it. */
      retro_atomic_fetch_add_seq_cst_int(&ec->epoch, 1);
      if (retro_atomic_load_seq_cst_int(&ec->waiters) == 0)
         return;
   }

   /* Someone is inside a prepare/commit window. Broadcast under the
    * lock, which is what pairs with commit_wait's check-under-lock. */
   slock_lock(ec->lock);
   scond_broadcast(ec->cond);
   slock_unlock(ec->lock);
}

/* ------------------------------------------------------------------ */
/* Consumer                                                            */
/* ------------------------------------------------------------------ */

int retro_asym_eventcount_prepare_wait(retro_asym_eventcount_t *ec)
{
   if (ec->asymmetric)
   {
      /* Register with a plain increment: the barrier below is what
       * publishes it, and the barrier is also a full fence for this
       * thread on every tier, so the store cannot linger in this core's
       * buffer past it. */
      retro_atomic_fetch_add_int(&ec->waiters, 1);

      /* The whole design in one call. Every other running thread passes
       * through a full fence before this returns, so a producer store to
       * the epoch that executed before now is visible to the load below,
       * and a producer that stores after now will find our registration
       * when it loads waiters. */
      retro_procbarrier();

      /* Belt and braces on the caller's own core, in case a tier ever
       * fails to serialise the calling thread. Cold path; costs nothing
       * that matters. */
      retro_atomic_thread_fence_seq_cst();

      return retro_atomic_load_acquire_int(&ec->epoch);
   }

   /* Symmetric: mirror of the notify pair. */
   retro_atomic_fetch_add_seq_cst_int(&ec->waiters, 1);
   return retro_atomic_load_seq_cst_int(&ec->epoch);
}

void retro_asym_eventcount_cancel_wait(retro_asym_eventcount_t *ec)
{
   retro_atomic_fetch_sub_int(&ec->waiters, 1);
}

void retro_asym_eventcount_commit_wait(retro_asym_eventcount_t *ec, int key)
{
   /* The lock covers the re-check and the sleep together. A notify that
    * bumped the epoch before this acquires the lock is seen by the
    * re-check; one that bumps after it must take the lock to broadcast,
    * and so cannot slip between the check and the wait. */
   slock_lock(ec->lock);
   while (retro_atomic_load_acquire_int(&ec->epoch) == key)
      scond_wait(ec->cond, ec->lock);
   retro_atomic_fetch_sub_int(&ec->waiters, 1);
   slock_unlock(ec->lock);
}

bool retro_asym_eventcount_commit_wait_timeout(retro_asym_eventcount_t *ec,
      int key, int64_t timeout_us)
{
   bool woken = true;
   slock_lock(ec->lock);
   if (retro_atomic_load_acquire_int(&ec->epoch) == key)
      woken = scond_wait_timeout(ec->cond, ec->lock, timeout_us);
   retro_atomic_fetch_sub_int(&ec->waiters, 1);
   slock_unlock(ec->lock);
   return woken;
}
