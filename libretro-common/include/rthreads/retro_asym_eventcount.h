/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_asym_eventcount.h).
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

#ifndef __LIBRETRO_SDK_ASYM_EVENTCOUNT_H
#define __LIBRETRO_SDK_ASYM_EVENTCOUNT_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
#include <retro_atomic.h>

RETRO_BEGIN_DECLS

/*
 * retro_asym_eventcount: an eventcount whose notify costs a plain store.
 *
 * Same shape and same contract as retro_eventcount -- notify on one side,
 * prepare / commit / cancel on the other, the epoch as the handshake --
 * with one difference in where the ordering is paid for.
 *
 * retro_eventcount's notify is a sequentially-consistent read-modify-write
 * followed by a sequentially-consistent load: on x86 a locked instruction
 * on every publish, so that a notify and a registering waiter can never
 * each miss the other. That is the right price for a general primitive
 * with any number of producers.
 *
 * This one moves the price to the waiter. notify is a release store and a
 * relaxed load -- three ordinary moves on x86, no lock prefix, no fence.
 * prepare_wait, after registering, calls retro_procbarrier(), which makes
 * every other running thread pass through a full fence. So either the
 * producer's store to the epoch was executed before the barrier and the
 * waiter's re-check sees it, or it was executed after, in which case the
 * waiter's registration is already visible and the producer's load finds
 * it and wakes. The lost-wakeup window is closed from the cold side.
 *
 * Measured on the producer: 6.2 ns for a fetch_add-based notify against
 * 2.6 ns for this, per publish, with the consumer awake. That is the
 * whole point, and it only pays off when the consumer is awake far more
 * often than it parks -- a render thread fed by a core thread, say.
 *
 * TWO CONDITIONS, BOTH ENFORCED BY THE CALLER
 *
 * Single producer. A plain store cannot serve concurrent notifiers; two
 * of them would lose each other's increments. If more than one thread
 * can notify, use retro_eventcount.
 *
 * Never call retro_procbarrier on the hot path. It is a syscall at best
 * and a signal per running thread at worst. This primitive calls it in
 * prepare_wait and nowhere else, which is the cold side by construction.
 * If the consumer parks at a high rate, the barrier's cost -- which
 * grows with core count -- can exceed what the producer saved. Spin
 * before parking; retro_eventcount_spin_iters() gives a sane budget.
 *
 * WHEN THERE IS NO BARRIER
 *
 * retro_procbarrier() returns 0 on platforms with nothing -- the
 * multi-core consoles -- and this primitive then behaves exactly as
 * retro_eventcount does: notify becomes the sequentially-consistent pair.
 * The decision is made once at init and stays for the object's life, so
 * a caller never has to know which mode it is in; it is only slower.
 * retro_asym_eventcount_is_asymmetric() reports it, for logs.
 */

typedef struct retro_asym_eventcount
{
   struct slock       *lock;
   struct scond       *cond;
   /* Together on one line, for the reason retro_eventcount.h gives at
    * the same two fields: both sides read both cursors, so separating
    * them buys a second line per operation and measures as no change
    * where the operations punctuate real work. */
   retro_atomic_int_t  epoch;      /* bumped once per notify              */
   retro_atomic_int_t  waiters;    /* threads inside a prepare/commit     */
   int                 asymmetric; /* 1: barrier-backed; 0: seq_cst pair  */
} retro_asym_eventcount_t;

bool retro_asym_eventcount_init(retro_asym_eventcount_t *ec);
void retro_asym_eventcount_free(retro_asym_eventcount_t *ec);

/* Producer side. A release store and a relaxed load when asymmetric.
 * Wakes parked waiters if there are any. */
void retro_asym_eventcount_notify(retro_asym_eventcount_t *ec);

/* Consumer side, three phases as in retro_eventcount:
 *   key = prepare_wait(ec);    register, barrier, snapshot the epoch
 *   if (work available) { cancel_wait(ec); consume; }
 *   else commit_wait(ec, key); park until the epoch moves past key
 * The predicate check between prepare and commit is the caller's; the
 * barrier in prepare is what makes it safe. */
int  retro_asym_eventcount_prepare_wait(retro_asym_eventcount_t *ec);
void retro_asym_eventcount_cancel_wait(retro_asym_eventcount_t *ec);
void retro_asym_eventcount_commit_wait(retro_asym_eventcount_t *ec, int key);
bool retro_asym_eventcount_commit_wait_timeout(retro_asym_eventcount_t *ec,
      int key, int64_t timeout_us);

/* Whether init resolved a process-wide barrier. 0 means the object is
 * running the symmetric protocol. */
int  retro_asym_eventcount_is_asymmetric(const retro_asym_eventcount_t *ec);

RETRO_END_DECLS

#endif
