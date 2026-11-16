/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_waitaddr.h).
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

#ifndef __LIBRETRO_SDK_WAITADDR_H
#define __LIBRETRO_SDK_WAITADDR_H

/* retro_waitaddr.h - park a thread on the value of one or more 32-bit
 * words, and wake it by address.
 *
 * retro_eventcount.h parks on an object the waiter and the notifier
 * both hold.  This parks on bare addresses - a thread sleeps while the
 * words at up to RETRO_WAITADDR_MAX addresses all still hold the values
 * it last saw, and whoever changes one of them wakes it by naming that
 * address - which is what code written around a futex, WaitOnAddress or
 * C++20's atomic wait needs, with no object to hand around:
 *
 *   waiter:    while (retro_atomic_load_acquire_int(&word) == seen)
 *              {
 *                 retro_waitaddr_t w;
 *                 w.addr = &word;
 *                 w.old  = (uint32_t)seen;
 *                 retro_waitaddr_wait(&w, 1, -1);
 *              }
 *
 *   notifier:  retro_atomic_store_release_int(&word, next);
 *              retro_waitaddr_notify_one(&word);
 *
 * A notify with nobody parked on an address that shares its bucket is
 * one sequentially-consistent read-modify-write: no lock, no syscall,
 * no search.  A wait may return early and spuriously, so the waiter
 * loops, as above.
 *
 * Nothing here takes a lock (on a backend whose retro_eventcount needs
 * a condition variable to sleep, that is the only one).  A waiter is a
 * record out of a pool that only grows, published in the bucket of each
 * address it names; a notifier that finds records there claims the ones
 * that name its address and wakes them through each record's own
 * retro_eventcount.  notify_one wakes one thread that was waiting on
 * the address when it was called, if there was one.
 *
 * The words are read as retro_atomic_int_t: an address handed in must
 * be that of four bytes it is valid to load that way.
 *
 * Needs the pointer atomics (RETRO_ATOMIC_HAS_PTR) and HAVE_THREADS.
 */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define RETRO_WAITADDR_MAX 8

typedef struct retro_waitaddr
{
   const void *addr; /* of a 32-bit word                     */
   uint32_t    old;  /* the value the waiter last saw there  */
} retro_waitaddr_t;

/**
 * retro_waitaddr_wait:
 * @list       : the words to wait on and the value each was seen with.
 * @count      : how many, 1 to RETRO_WAITADDR_MAX.
 * @timeout_us : how long to block for, in microseconds; negative for
 *               no bound.
 * Blocks while every word still holds its old value, until one of the
 * addresses is notified or the bound expires.  Returns at once when a
 * word already differs.  May return early and spuriously.
 * @return false if the bound expired, true otherwise.
 */
bool retro_waitaddr_wait(const retro_waitaddr_t *list, unsigned count,
      int64_t timeout_us);

/**
 * retro_waitaddr_notify_one:
 * @addr : the address of a word that was changed.
 * Wakes one thread waiting on @addr, if any.  Call after the change is
 * published, never before.
 */
void retro_waitaddr_notify_one(const void *addr);

/**
 * retro_waitaddr_notify_all:
 * @addr : the address of a word that was changed.
 * Wakes every thread waiting on @addr.
 */
void retro_waitaddr_notify_all(const void *addr);

/**
 * retro_waitaddr_free_all:
 * Releases the records and what their eventcounts hold.  No thread may
 * be inside a wait or a notify; for a module about to be unloaded.
 * Waiting works again afterwards.
 */
void retro_waitaddr_free_all(void);

RETRO_END_DECLS

#endif
