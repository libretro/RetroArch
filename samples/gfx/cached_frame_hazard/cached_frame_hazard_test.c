/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cached_frame_hazard_test.c).
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

/* Stress test for the cached-frame hazard-pointer protocol in
 * gfx/video_driver.c.
 *
 * The protocol is reproduced here rather than linked, so the test
 * stays self-contained (the real thing lives in a TU that drags in
 * most of the frontend).  Keep the two in step: the reader and
 * retire sequences below are line-for-line the ones in
 * video_driver_cached_frame_read() and
 * video_driver_cached_frame_retire().
 *
 * What it checks: a producer publishes a heap buffer, readers read
 * it, and the producer frees it behind a retire().  Under ASan any
 * hole in the protocol surfaces as a heap-use-after-free.
 *
 * Build with -DTORTURE to widen the window that follows the tuple
 * snapshot.  A reader must sample the generation before that
 * snapshot; sample it after and a whole retire fits in the gap, so
 * the re-validate compares equal and the callback runs on freed
 * memory.  The plain build does not reach that ordering reliably,
 * the torture build fails on it in under a second.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>

#if !defined(RETRO_ATOMIC_LOCK_FREE) || !defined(RETRO_ATOMIC_HAS_PTR) || !defined(RETRO_ATOMIC_HAS_CAS)
int main(void)
{
   printf("cached_frame_hazard: atomics backend '%s' has no lock-free "
          "pointer CAS; the tested path is compiled out on this target.\n",
          RETRO_ATOMIC_BACKEND_NAME);
   return 0;
}
#else

#define HAZARD_SLOTS   8
#define FRAME_BYTES    4096
#define RUN_MSEC       4000
#define READER_THREADS 6
/* Bound on the producer's wait for a reader to pick up the frame it
 * just published, counted in scheduler handoffs rather than spins.
 * A liveness guard, not a timeout anyone should hit. */
#define OVERLAP_YIELD_BOUND 10000

/* Mirrors the file-scope state in gfx/video_driver.c. */
static retro_atomic_ptr_t  hazard[HAZARD_SLOTS];
static retro_atomic_int_t  generation;
/* The tuple is published exactly as gfx/video_driver.c publishes it,
 * so the two stay in step.  Whether that seqlock hands out torn
 * tuples is libretro-common/samples/atomic's business; what this
 * harness tests is the hazard protocol layered over it. */
static retro_atomic_size_t cache_seq;
static retro_atomic_ptr_t  cache_data;
/* Both dimensions in one word, as gfx/video_driver.c packs them: one
 * relaxed store out, one relaxed load back, and they cannot be seen
 * half-updated even without the stamp. */
static retro_atomic_int_t  cache_dims;
#define CACHE_DIMS_PACK(w, h) ((int)(((unsigned)(h) << 16) | ((unsigned)(w) & 0xffffu)))
#define CACHE_DIMS_W(d)       ((unsigned)(d) & 0xffffu)
#define CACHE_DIMS_H(d)       (((unsigned)(d) >> 16) & 0xffffu)

static retro_atomic_int_t stop;

/* Diagnostics.  The reader-side counters are atomic even though
 * exactness does not matter here: plain increments from six reader
 * threads are real races, and TSan reporting them drowns out any
 * race that actually matters.  The producer-only counters stay
 * plain. */
static retro_atomic_int_t reads_with_pixels;
static retro_atomic_int_t reads_empty;
static retro_atomic_int_t revalidate_retries;
static long               retires;
static long               drain_spins;

static int hazard_acquire(const void *ptr)
{
   int i;
   for (i = 0; i < HAZARD_SLOTS; i++)
      if (retro_atomic_cas_ptr(&hazard[i], NULL, (void*)ptr))
         return i;
   return -1;
}

static void hazard_release(int slot)
{
   if (slot >= 0)
      retro_atomic_store_release_ptr(&hazard[slot], NULL);
}

static void hazard_drain(void)
{
   int i;
   for (i = 0; i < HAZARD_SLOTS; i++)
      while (retro_atomic_load_acquire_ptr(&hazard[i]))
      {
         drain_spins++;
         retro_cpu_relax();
      }
}

#define SEQ_TRIES 64

static bool cache_snapshot(const void **data,
      unsigned *width, unsigned *height)
{
   unsigned tries;

   for (tries = 0; tries < SEQ_TRIES; tries++)
   {
      size_t s2;
      size_t s1 = retro_atomic_load_acquire_size(&cache_seq);

      if (s1 & 1)
      {
         retro_cpu_relax();
         continue;
      }

      {
         int dims = retro_atomic_load_relaxed_int(&cache_dims);
         *data    = (const void*)retro_atomic_load_relaxed_ptr(&cache_data);
         *width   = CACHE_DIMS_W(dims);
         *height  = CACHE_DIMS_H(dims);
      }

      retro_atomic_thread_fence_acquire();
      s2      = retro_atomic_load_acquire_size(&cache_seq);
      if (s1 == s2)
         return true;
   }

   *data   = NULL;
   *width  = 0;
   *height = 0;
   return false;
}

static void cache_store(const void *data, unsigned width, unsigned height)
{
   size_t s = retro_atomic_load_relaxed_size(&cache_seq);

   retro_atomic_store_release_size(&cache_seq, s + 1);
   retro_atomic_thread_fence_release();

   retro_atomic_store_relaxed_ptr(&cache_data, (void*)data);
   retro_atomic_store_relaxed_int(&cache_dims, CACHE_DIMS_PACK(width, height));

   retro_atomic_store_release_size(&cache_seq, s + 2);
}

static void cached_frame_publish(const void *data,
      unsigned width, unsigned height)
{
   if (data)
      cache_store(data, width, height);
   else
      cache_store((const void*)retro_atomic_load_relaxed_ptr(&cache_data),
            width, height);
}

static void cached_frame_invalidate(void)
{
   cache_store(NULL, 0, 0);
}

static void cached_frame_retire(void)
{
   cached_frame_invalidate();
   /* Bump first, then scan: a reader that armed before the bump is
    * visible to the scan, one that arms after it sees the new
    * generation and retries. */
   retro_atomic_fetch_add_int(&generation, 1);
   hazard_drain();
}

static void cached_frame_read(void)
{
   const void *data;
   unsigned    width  = 0;
   unsigned    height = 0;
   int         slot   = -1;
   int         gen;

   for (;;)
   {
      /* Generation BEFORE the tuple. See the header comment. */
      gen    = retro_atomic_load_acquire_int(&generation);

      cache_snapshot(&data, &width, &height);

#ifdef TORTURE
      /* The delay belongs HERE, between the tuple snapshot and
       * everything that follows: that is the window a retire slips
       * through unnoticed when the generation is sampled after the
       * snapshot rather than before it.  Move this line and the
       * regression stops being tested. */
      retro_sleep(1);
#endif

      if (!data)
      {
         retro_atomic_inc_int(&reads_empty);
         return;
      }

      if ((slot = hazard_acquire(data)) < 0)
      {
         retro_atomic_inc_int(&reads_empty);
         return;
      }

      if (retro_atomic_load_acquire_int(&generation) != gen)
      {
         hazard_release(slot);
         slot = -1;
         retro_atomic_inc_int(&revalidate_retries);
         continue;
      }
      break;
   }

   /* Stand-in for a consumer's framebuffer memcpy: touch every byte
    * so ASan sees the access. */
   {
      volatile unsigned char sink = 0;
      const unsigned char   *p    = (const unsigned char*)data;
      size_t i, n = (size_t)width * height;
      for (i = 0; i < n; i++)
         sink ^= p[i];
      (void)sink;
      retro_atomic_inc_int(&reads_with_pixels);
   }

   hazard_release(slot);
}

static void reader_thread(void *unused)
{
   (void)unused;
   while (!retro_atomic_load_acquire_int(&stop))
      cached_frame_read();
}

/* Owns the buffer and is the only thing that frees it, always behind
 * a retire() -- the driver-teardown pattern. */
static void producer_thread(void *unused)
{
   (void)unused;
   while (!retro_atomic_load_acquire_int(&stop))
   {
      unsigned char *buf = (unsigned char*)malloc(FRAME_BYTES);
      if (!buf)
         return;
      memset(buf, 0xA5, FRAME_BYTES);

      cached_frame_publish(buf, 64, 64);
      cached_frame_publish(NULL, 64, 64);  /* a duped frame */

      /* Wait for a reader to take this frame before retiring it.
       * Publishing and retiring back to back leaves the overlap to
       * chance -- a publish is a handful of stores -- and a run where
       * no reader was ever inside the buffer exercises neither the
       * drain nor the re-validate.  Each turn yields rather than
       * spins: on one core, or on a loaded machine, spinning is what
       * stops the reader being waited for from ever running. */
      {
         int before = retro_atomic_load_acquire_int(&reads_with_pixels);
         int turns  = 0;

         while (retro_atomic_load_acquire_int(&reads_with_pixels) == before
               && turns++ < OVERLAP_YIELD_BOUND
               && !retro_atomic_load_acquire_int(&stop))
            retro_sleep(0);
      }

      cached_frame_retire();
      retires++;
      free(buf);                           /* safe iff the protocol holds */
   }
}

int main(void)
{
   sthread_t *readers[READER_THREADS];
   sthread_t *producer;
   int        i;

   for (i = 0; i < READER_THREADS; i++)
      readers[i] = sthread_create(reader_thread, NULL);
   producer = sthread_create(producer_thread, NULL);

   retro_sleep(RUN_MSEC);
   retro_atomic_store_release_int(&stop, 1);

   sthread_join(producer);
   for (i = 0; i < READER_THREADS; i++)
      sthread_join(readers[i]);

   printf("backend=%s\n", RETRO_ATOMIC_BACKEND_NAME);
   {
      int pixels  = retro_atomic_load_acquire_int(&reads_with_pixels);
      int empty   = retro_atomic_load_acquire_int(&reads_empty);
      int retried = retro_atomic_load_acquire_int(&revalidate_retries);

      printf("retires=%ld reads_with_pixels=%d reads_empty=%d\n",
            retires, pixels, empty);
      printf("drain_spins=%ld revalidate_retries=%d\n",
            drain_spins, retried);

      /* A run that never contended proves nothing.  The two builds
       * reach different halves of the protocol, so each asserts on
       * the half it is shaped to exercise: the plain build wants
       * readers holding hazards while a retire waits, the torture
       * build wants readers catching a retire mid-arm. */
      if (!retires)
      {
         printf("FAIL: the producer never ran\n");
         return 1;
      }
#ifdef TORTURE
      if (!retried)
      {
         printf("FAIL: no reader ever caught a racing retire; "
               "the re-validate path went untested\n");
         return 1;
      }
#else
      if (!pixels)
      {
         printf("FAIL: producer and readers never overlapped\n");
         return 1;
      }
      if (!drain_spins)
      {
         printf("FAIL: no retire ever waited on a reader; "
               "the drain path went untested\n");
         return 1;
      }
#endif
   }
   printf("OK\n");
   return 0;
}
#endif
