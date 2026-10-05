/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - libretroadmin
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

#ifndef __VIDEO_THREAD_HW_FENCE_H
#define __VIDEO_THREAD_HW_FENCE_H

#include <boolean.h>
#include <retro_inline.h>
#include <retro_atomic.h>

/* The hardware ring's fence for one slot, handed between the video
 * thread and the core's thread without a lock.
 *
 * The video thread arms the fence after every frame it draws from the
 * slot, and the core's thread waits it before it renders into the slot
 * again. A dupe draws from a slot a second time, so the fence is armed
 * again before anyone has waited it, and the two threads can be at the
 * same slot at once. One driver fence cannot take that: a VkFence may
 * not be submitted while it is signalled or pending, nor reset while
 * the other thread submits it, and a GL sync object swapped under a
 * waiter is waited or deleted by the wrong thread.
 *
 * So a slot has three driver fences, and each is only ever touched by
 * the one thread that owns it at that moment. Ownership moves by an
 * atomic exchange:
 *
 *   arm   (video thread)  take an idle cell, signal it, exchange it
 *                         into `armed`. Whatever comes back out is the
 *                         earlier arming nobody waited: the video
 *                         thread now owns it, waits it out - work
 *                         submitted a frame ago, and the new arming
 *                         sits behind it on the queue anyway - and
 *                         returns it to idle.
 *   wait  (core's thread) exchange `armed` out. Whatever comes out is
 *                         the waiter's alone: wait it, return it to
 *                         idle, and look again in case the slot was
 *                         armed meanwhile.
 *
 * At most one cell is armed and at most one is with the waiter, so of
 * three one is always idle for the video thread to take. No lock, and
 * no driver fence is ever used by two threads at the same time. The
 * driver's signal is only called on an idle fence and its wait only on
 * an armed one, which is all the drivers' fences need.
 *
 * The core's thread arms a slot too, behind the queue work for a frame
 * it took back before the video thread claimed it. The video thread
 * draws nothing from that slot meanwhile, so a slot is still armed by
 * one thread at a time. */

#define HW_FENCE_CELLS 3

typedef struct hw_fence
{
   void              *cell[HW_FENCE_CELLS]; /* the driver's fences        */
   retro_atomic_int_t busy[HW_FENCE_CELLS]; /* armed, or with the waiter  */
   retro_atomic_int_t armed;                /* cell + 1, or 0 for none    */
   int                held;                 /* the waiter's own: the cell
                                             * + 1 it took and has not
                                             * finished waiting, or 0    */
} hw_fence_t;

typedef void (*hw_fence_signal_fn)(void *data, void *fence);
typedef bool (*hw_fence_wait_fn)(void *data, void *fence, unsigned timeout_us);

static INLINE void hw_fence_init(hw_fence_t *f)
{
   unsigned i;
   for (i = 0; i < HW_FENCE_CELLS; i++)
   {
      f->cell[i] = NULL;
      retro_atomic_int_init(&f->busy[i], 0);
   }
   retro_atomic_int_init(&f->armed, 0);
   f->held = 0;
}

/* Video thread, after a frame drawn from the slot; or the core's
 * thread, for a frame taken back (see above). `forever` is the
 * driver's unbounded timeout. */
static INLINE void hw_fence_arm(hw_fence_t *f, hw_fence_signal_fn signal,
      hw_fence_wait_fn wait, void *data, unsigned forever)
{
   int prev;
   int c;
   for (c = 0; c < HW_FENCE_CELLS; c++)
      if (!retro_atomic_load_acquire_int(&f->busy[c]))
         break;
   /* Cannot happen: one armed, one with the waiter, one left. */
   if (c == HW_FENCE_CELLS)
      return;
   retro_atomic_store_release_int(&f->busy[c], 1);
   signal(data, f->cell[c]);
   prev = retro_atomic_exchange_int(&f->armed, c + 1);
   if (prev)
   {
      wait(data, f->cell[prev - 1], forever);
      retro_atomic_store_release_int(&f->busy[prev - 1], 0);
   }
}

/* The core's thread. Returns false when timeout_us passed first; the
 * cell stays the waiter's and the next call goes on waiting it. */
static INLINE bool hw_fence_wait(hw_fence_t *f, hw_fence_wait_fn wait,
      void *data, unsigned timeout_us)
{
   for (;;)
   {
      int c = f->held;
      if (!c)
         c  = retro_atomic_exchange_int(&f->armed, 0);
      if (!c)
         return true;
      f->held = c;
      if (!wait(data, f->cell[c - 1], timeout_us))
         return false;
      f->held = 0;
      retro_atomic_store_release_int(&f->busy[c - 1], 0);
   }
}

#endif
