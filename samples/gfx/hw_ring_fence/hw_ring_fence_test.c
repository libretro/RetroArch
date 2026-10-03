/* hw_ring_fence_test.c -- the hardware ring's fence handoff
 * (gfx/video_thread_hw_fence.h), the real header, beaten on by two
 * threads.
 *
 * The video thread arms a slot's fence after every frame it draws from
 * the slot; the core's thread waits it before it renders into the slot
 * again. A dupe arms a slot that is armed already, and the two threads
 * can be at one slot at once. What a driver's fence needs, and what
 * the fake fences here check on every call:
 *
 *   - it is signalled only while idle. A VkFence submitted while it is
 *     signalled or pending is VUID-vkQueueSubmit-fence-00063/-00064;
 *     lavapipe aborts on it.
 *   - it is waited only once it has been signalled; a wait on one that
 *     never was blocks for good.
 *   - it is never inside two calls at once: a VkFence may not be reset
 *     under a thread that is submitting it, and a GL sync object
 *     swapped under a waiter is deleted by the wrong thread.
 *
 * And at the end nothing is left armed and every cell is idle again.
 * No lock anywhere: the header moves ownership by atomic exchange.
 *
 *   hw_ring_fence_test          the handoff in the tree
 *   hw_ring_fence_test_old      -DOLD_SINGLE_FENCE: one fence per slot,
 *                               signalled after every frame and waited
 *                               when "in flight", as it was. The
 *                               Makefile requires this one to fail.
 *
 * The waiter takes timeouts at random, as the Apple build's sliced
 * wait does, so the held-across-calls path runs too. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "../../../gfx/video_thread_hw_fence.h"

#define FRAMES  200000
#define FOREVER ((unsigned)-1)

typedef struct
{
   retro_atomic_int_t state; /* 0 idle, 1 signalled or pending */
   retro_atomic_int_t user;  /* a call is inside               */
} fake_fence_t;

static retro_atomic_int_t bad_resignal;  /* signalled while not idle    */
static retro_atomic_int_t bad_wait;      /* waited while never signalled */
static retro_atomic_int_t bad_shared;    /* two calls at once            */
static retro_atomic_int_t video_done;
static unsigned           waited;        /* waits that found a fence     */

static void fake_enter(fake_fence_t *f)
{
   if (retro_atomic_exchange_int(&f->user, 1))
      retro_atomic_fetch_add_int(&bad_shared, 1);
}

static void fake_leave(fake_fence_t *f)
{
   retro_atomic_store_release_int(&f->user, 0);
}

static void fake_signal(void *data, void *fence)
{
   fake_fence_t *f = (fake_fence_t*)fence;
   (void)data;
   fake_enter(f);
   if (retro_atomic_exchange_int(&f->state, 1) != 0)
      retro_atomic_fetch_add_int(&bad_resignal, 1);
   fake_leave(f);
}

static bool fake_wait(void *data, void *fence, unsigned timeout_us)
{
   fake_fence_t *f = (fake_fence_t*)fence;
   unsigned *seed  = (unsigned*)data;
   fake_enter(f);
   if (timeout_us != FOREVER && seed)
   {
      *seed = *seed * 1103515245u + 12345u;
      if ((*seed >> 16) & 1)
      {
         fake_leave(f);
         return false;
      }
   }
   /* Wait and reset. */
   if (retro_atomic_exchange_int(&f->state, 0) != 1)
      retro_atomic_fetch_add_int(&bad_wait, 1);
   /* Counted on the core's thread only: the video thread's waits on an
    * arming it took back pass no seed. */
   if (seed)
      waited++;
   fake_leave(f);
   return true;
}

#ifdef OLD_SINGLE_FENCE
static fake_fence_t       old_fence;
static retro_atomic_int_t old_in_flight;
#else
static fake_fence_t cells[HW_FENCE_CELLS];
static hw_fence_t   fence;
#endif

/* The video thread: a frame, then the arm. Every frame here is drawn
 * from the one slot, which is what a run of dupes is. */
static void video_thread(void *u)
{
   unsigned i;
   (void)u;
   for (i = 0; i < FRAMES; i++)
   {
#ifdef OLD_SINGLE_FENCE
      fake_signal(NULL, &old_fence);
      retro_atomic_store_release_int(&old_in_flight, 1);
#else
      hw_fence_arm(&fence, fake_signal, fake_wait, NULL, FOREVER);
#endif
      /* Runs of dupes of uneven length between the core's waits, on
       * one processor as on several. */
      if ((i % 7) == 0 || (i % 13) == 0)
         sthread_yield();
   }
   retro_atomic_store_release_int(&video_done, 1);
}

/* The core's thread: waits the slot, with a timeout now and then. */
static void core_wait(unsigned *seed)
{
#ifdef OLD_SINGLE_FENCE
   (void)seed;
   if (retro_atomic_load_acquire_int(&old_in_flight))
   {
      fake_wait(seed, &old_fence, FOREVER);
      retro_atomic_store_release_int(&old_in_flight, 0);
   }
#else
   while (!hw_fence_wait(&fence, fake_wait, seed, 2000))
      ;
#endif
}

int main(void)
{
   sthread_t *video;
   unsigned seed = 1;
   int bad = 0;
#ifndef OLD_SINGLE_FENCE
   unsigned i;
   hw_fence_init(&fence);
   for (i = 0; i < HW_FENCE_CELLS; i++)
      fence.cell[i] = &cells[i];
#endif

   video = sthread_create(video_thread, NULL);
   if (!video)
      return 2;
   while (!retro_atomic_load_acquire_int(&video_done))
   {
      core_wait(&seed);
      sthread_yield();
   }
   sthread_join(video);
   core_wait(&seed);

   if (retro_atomic_load_acquire_int(&bad_resignal))
   {
      printf("FAIL: a fence was signalled %d time(s) while signalled or pending\n",
            retro_atomic_load_acquire_int(&bad_resignal));
      bad = 1;
   }
   if (retro_atomic_load_acquire_int(&bad_wait))
   {
      printf("FAIL: a fence was waited %d time(s) without having been signalled\n",
            retro_atomic_load_acquire_int(&bad_wait));
      bad = 1;
   }
   if (retro_atomic_load_acquire_int(&bad_shared))
   {
      printf("FAIL: a fence was inside two calls at once %d time(s)\n",
            retro_atomic_load_acquire_int(&bad_shared));
      bad = 1;
   }
#ifndef OLD_SINGLE_FENCE
   if (retro_atomic_load_acquire_int(&fence.armed) || fence.held)
   {
      printf("FAIL: a cell is still armed or held after the last wait\n");
      bad = 1;
   }
   for (i = 0; i < HW_FENCE_CELLS; i++)
   {
      if (     retro_atomic_load_acquire_int(&fence.busy[i])
            || retro_atomic_load_acquire_int(&cells[i].state))
      {
         printf("FAIL: cell %u is not idle after the last wait\n", i);
         bad = 1;
      }
   }
#endif
   /* Both sides must have been at the slot: a run in which the core
    * never found an armed fence tested one thread. */
   if (waited < FRAMES / 100)
   {
      printf("FAIL: the core's thread waited only %u fence(s) over %d arms\n",
            waited, FRAMES);
      bad = 1;
   }
   if (!bad)
      printf("[pass] hw ring fence handoff: %d arms, %u waited by the core's thread\n",
            FRAMES, waited);
   return bad;
}
