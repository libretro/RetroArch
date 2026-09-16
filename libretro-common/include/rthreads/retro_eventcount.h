/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_eventcount.h).
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

#ifndef __LIBRETRO_SDK_EVENTCOUNT_H
#define __LIBRETRO_SDK_EVENTCOUNT_H

/* retro_eventcount.h - park a consumer without putting a lock on the
 * producer's fast path.
 *
 * A lock-free queue answers "is there work?" without a lock, but says
 * nothing about what a consumer should do when the answer is no.
 * Spinning burns a core; a condition variable puts a lock acquisition on
 * every publish whether or not anyone is asleep.  An eventcount is the
 * third answer: the consumer registers its intent to sleep, re-checks
 * its own condition, and only then blocks, and the producer pays one
 * sequentially-consistent read-modify-write and one sequentially-
 * consistent load when nobody is parked - no lock, no syscall.
 *
 * The pairing this exists for is retro_spsc plus retro_atomic: the queue
 * moves the data, a retired-work counter published by the consumer
 * answers "has it caught up", and this answers "wake me when it has not".
 *
 * Usage
 * -----
 * The consumer owns the loop and its own predicate, so the predicate is
 * never a callback and never an indirect call in a spin:
 *
 *   for (;;)
 *   {
 *      int key;
 *
 *      if (have_work())
 *         break;
 *
 *      key = retro_eventcount_prepare_wait(&ec);
 *
 *      if (have_work())
 *      {
 *         retro_eventcount_cancel_wait(&ec);
 *         break;
 *      }
 *
 *      retro_eventcount_commit_wait(&ec, key);
 *   }
 *
 * and the producer, after publishing:
 *
 *   retro_eventcount_notify(&ec);
 *
 * commit_wait may return without the condition having changed, exactly
 * as a condition variable may, which is why the shape above is a loop.
 *
 * Rules
 * -----
 * 1. Every retro_eventcount_prepare_wait() must be answered by exactly
 *    one retro_eventcount_commit_wait() or retro_eventcount_cancel_wait()
 *    on the same thread.
 *
 * 2. No lock is held across that window on any backend, so the window
 *    is yours: evaluating a predicate that touches other objects is
 *    fine.  Keep it short anyway.  A notifier that sees a registered
 *    waiter does the wake work whether or not the waiter goes on to
 *    sleep, so a long window buys a notifier pointless syscalls.
 *
 * 3. Any number of threads may notify.  Any number may wait; a notify
 *    releases all of them.  That holds on every backend, including the
 *    ones whose atomics are not lock-free, where the bookkeeping is
 *    kept under a mutex because their read-modify-writes are not
 *    indivisible.
 *
 * 4. The window between prepare_wait and its answer must not span 2^32
 *    notifications, or the key could match a different epoch than the
 *    one it named.  Keeping the window short, which rule 2 asks for
 *    anyway, is several orders of magnitude more than enough.
 *
 * 5. A consumer that spins before parking must gate the spin on
 *    RETRO_ATOMIC_LOCK_FREE, not merely on this header existing.  On a
 *    backend where the atomics are real but not lock-free -- the PS2 EE
 *    masks interrupts around a read-modify-write and reschedules only
 *    out of an interrupt -- a spinning thread never lets the thread it
 *    waits for run.  Parking through this object is fine there; spinning
 *    first is not.
 *
 * Backends
 * --------
 *   Linux / Android      futex(FUTEX_WAIT_PRIVATE), no lock at all
 *   Windows              a waiter list of stack blocks, slept on with the
 *                        best primitive ntdll offers, resolved at runtime:
 *                        NtWaitForAlertByThreadId on 8 and newer,
 *                        NtWaitForKeyedEvent back to XP, and a per-thread
 *                        auto-reset event on anything older, including 9x.
 *                        No mutex on any tier.  If none of the three can
 *                        be had - no ntdll entry points and no TLS index
 *                        left for the event - the object falls back to
 *                        the scond backend below rather than failing.
 *   everything else      rthreads scond, with the lock taken only across
 *                        the sleep itself
 *
 * The scond backend is not a degraded mode; it is correct and it is what
 * macOS, the BSDs and the console ports use.  What it costs is one lock
 * acquisition per notify while a consumer is parked, which is the case
 * where a syscall was going to happen anyway.
 *
 * Where the atomics themselves are not lock-free, the handshake that
 * lets notify skip the lock cannot be relied on, so that build takes the
 * lock on every notify instead.  retro_eventcount_backend_name() reports
 * which of these is live.
 */

#include <retro_common_api.h>
#include <retro_atomic.h>
#include <boolean.h>
#include <stdint.h>

RETRO_BEGIN_DECLS

/* Spelled as bare struct pointers rather than slock_t / scond_t so this
 * header needs no include of rthreads.h and repeats no typedef: a
 * translation unit that includes both is then legal C89. */
typedef struct retro_eventcount
{
   struct slock       *lock;    /* NULL unless the backend needs a condvar   */
   struct scond       *cond;    /* NULL unless the backend needs a condvar   */
#if defined(_WIN32) && !defined(_XBOX) && defined(RETRO_ATOMIC_HAS_PTR)
   /* Win32 keeps its own waiter list: the blocks live on the waiters'
    * stacks and the low bit of the head is the list's spin lock.  No
    * caller mutex is involved, which is the whole point of it -- a
    * condition variable would re-acquire one on every wake. */
   retro_atomic_ptr_t  waitlist;
#endif
   retro_atomic_int_t  epoch;   /* bumped once per notify                    */
   retro_atomic_int_t  waiters; /* threads inside a prepare/commit window    */
} retro_eventcount_t;

/**
 * retro_eventcount_init:
 * @ec : object to initialise.
 *
 * Brings @ec up in the unsignalled state with no waiters.  Allocates
 * only on the backends that need a mutex and a condition variable.
 *
 * @return true on success.  On failure @ec is left safe to pass to
 * retro_eventcount_free().
 */
bool retro_eventcount_init(retro_eventcount_t *ec);

/**
 * retro_eventcount_free:
 * @ec : object to release.
 *
 * Releases whatever init allocated.  No thread may be inside any other
 * call on @ec.  Safe on a zeroed object and on one init failed for.
 */
void retro_eventcount_free(retro_eventcount_t *ec);

/**
 * retro_eventcount_notify:
 * @ec : object to signal.
 *
 * Releases every thread currently waiting on @ec, and causes any
 * prepare_wait that has already run to return without blocking.  Call
 * after the work is published, never before.
 *
 * With nobody parked this is one sequentially-consistent
 * read-modify-write and one sequentially-consistent load: no lock, no
 * syscall.
 */
void retro_eventcount_notify(retro_eventcount_t *ec);

/**
 * retro_eventcount_prepare_wait:
 * @ec : object to wait on.
 *
 * Opens a wait window and returns the key that names the current state
 * of @ec.  Re-check your predicate after this returns: a notify that
 * lands from here on is guaranteed either to be visible to that check
 * or to make the matching commit_wait return immediately.
 *
 * @return the key to hand to retro_eventcount_commit_wait().
 */
int retro_eventcount_prepare_wait(retro_eventcount_t *ec);

/**
 * retro_eventcount_cancel_wait:
 * @ec : object whose wait window to close.
 *
 * Closes the window opened by retro_eventcount_prepare_wait() without
 * blocking.  Use when the re-check found work.
 */
void retro_eventcount_cancel_wait(retro_eventcount_t *ec);

/**
 * retro_eventcount_commit_wait:
 * @ec  : object to block on.
 * @key : value returned by the matching retro_eventcount_prepare_wait().
 *
 * Blocks until @ec is notified, and closes the wait window.  May return
 * early and spuriously, so callers loop.
 */
void retro_eventcount_commit_wait(retro_eventcount_t *ec, int key);

/**
 * retro_eventcount_commit_wait_timeout:
 * @ec         : object to block on.
 * @key        : value returned by the matching prepare_wait().
 * @timeout_us : how long to block for, in microseconds.  Zero or less
 *               polls.  A bound long enough to overflow the backend's
 *               own unit is clamped, never wrapped.
 *
 * As retro_eventcount_commit_wait(), bounded in time.
 *
 * @return true if @ec was notified, false if the bound expired.  A
 * spurious early return reports true, so the caller's loop still owns
 * the decision.
 */
bool retro_eventcount_commit_wait_timeout(retro_eventcount_t *ec,
      int key, int64_t timeout_us);

/**
 * retro_eventcount_spin_iters:
 *
 * @return how many times a waiter spins on its flag word before it
 * commits to the kernel, as this build resolved it.  Zero on a
 * uniprocessor, where the spin is skipped.  For logs and benchmarks;
 * on Windows it is only meaningful after the first
 * retro_eventcount_init().
 */
unsigned retro_eventcount_spin_iters(void);

/**
 * retro_eventcount_backend_name:
 *
 * @return a string literal naming the live parking backend, for logs
 * and CI.  On Windows this reflects the runtime probe, so it is only
 * meaningful after the first retro_eventcount_init().
 */
const char *retro_eventcount_backend_name(void);

RETRO_END_DECLS

#endif
