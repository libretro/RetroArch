/* overlay_vp_seqlock_test.c -- the overlay viewport's seqlock from
 * gfx/video_driver.c, reproduced and beaten on.
 *
 * The protocol: four floats cross from the main thread (overlay
 * load) to the viewport-scaling thread as float bits in atomic ints
 * under a generation counter - bumped odd before a rewrite, even
 * after; a reader retries while it is odd or changed across the
 * copy. The claims a regression would break:
 *
 *   1. No torn tuple: every 4-float snapshot a reader takes came
 *      from exactly one writer round (the fields encode the round,
 *      and must agree).
 *   2. Progress: readers converge - rewrites are rare and short, so
 *      the retry loop terminates and snapshots keep flowing.
 *
 * Paced with short sleeps so a single-processor machine interleaves
 * the threads; unlike the string seqlock this shape avoids, every
 * shared field here is an atomic int, so ThreadSanitizer holds it
 * too. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#if defined(_WIN32)
#include <windows.h>
static void sleep_us(unsigned us) { Sleep(us / 1000u + 1u); }
#else
#include <unistd.h>
static void sleep_us(unsigned us) { usleep(us); }
#endif

#define N_ROUNDS 4000

static retro_atomic_int_t g_seq;
static retro_atomic_int_t g_bits[4];
static retro_atomic_int_t g_stop;

static int float_bits(float f) { int b; memcpy(&b,&f,sizeof(b)); return b; }
static float bits_float(int b) { float f; memcpy(&f,&b,sizeof(f)); return f; }

/* Round r encodes as {r, r+0.25, r+0.5, r+0.75}: a consistent
 * snapshot's fields all decode to the same round. */
static void writer(void *arg)
{
   int r;
   (void)arg;
   for (r = 1; r <= N_ROUNDS; r++)
   {
      int i;
      int s0 = retro_atomic_load_relaxed_int(&g_seq);
      retro_atomic_store_release_int(&g_seq, s0 + 1);
      /* The driver's fence, reproduced: a store-release orders what
       * precedes it, so without this the field stores below may be
       * hoisted above the odd stamp on a weakly ordered machine and
       * a reader sees an even stamp either side of a half-written
       * tuple. x86 does not reorder store with store, so no lane
       * here can make that visible - the fence is carried so this
       * harness reproduces the protocol rather than only the shape
       * that happens to work on the host. */
      retro_atomic_thread_fence_release();
      for (i = 0; i < 4; i++)
      {
         retro_atomic_store_relaxed_int(&g_bits[i],
               float_bits((float)r + (float)i * 0.25f));
         /* A stalled writer every so often: under the protocol,
          * readers spin on the odd counter through the stall; with
          * the counter sabotaged away they read the half-written
          * tuple, which is what gives the sabotage lane teeth on a
          * single-processor machine. */
         if (i == 1 && (r & 127) == 0)
            sleep_us(300);
      }
      retro_atomic_store_release_int(&g_seq, s0 + 2);
      if ((r & 15) == 0)
         sleep_us(120);
   }
   retro_atomic_store_release_int(&g_stop, 1);
}

int main(void)
{
   sthread_t *w;
   float vp[4];
   int torn = 0, taken = 0, last_round = 0, i;

   retro_atomic_store_relaxed_int(&g_seq, 0);
   for (i = 0; i < 4; i++)
      retro_atomic_store_relaxed_int(&g_bits[i], float_bits(0.0f));
   retro_atomic_store_relaxed_int(&g_stop, 0);

   w = sthread_create(writer, NULL);

   while (!retro_atomic_load_acquire_int(&g_stop) || last_round < N_ROUNDS)
   {
      int r0;
      for (;;)
      {
         int s1 = retro_atomic_load_acquire_int(&g_seq);
         if (s1 & 1)
            continue;
         for (i = 0; i < 4; i++)
            vp[i] = bits_float(retro_atomic_load_relaxed_int(&g_bits[i]));
         retro_atomic_thread_fence_acquire();
         if (retro_atomic_load_relaxed_int(&g_seq) == s1)
            break;
      }

      r0 = (int)vp[0];
      if (r0)
      {
         for (i = 1; i < 4; i++)
            if ((int)vp[i] != r0 || vp[i] != (float)r0 + (float)i * 0.25f)
               torn++;
         if (r0 > last_round)
            last_round = r0;
      }
      taken++;
      sleep_us(60);
   }
   sthread_join(w);

   printf("overlay_vp_seqlock: %d rounds written, %d snapshots, "
          "last round seen %d\n", N_ROUNDS, taken, last_round);
   if (torn)
      printf("  FAIL: %d torn field(s)\n", torn);
   if (last_round != N_ROUNDS)
      printf("  FAIL: final round never seen\n");
   if (torn || last_round != N_ROUNDS)
   {
      printf("overlay_vp_seqlock: FAILED\n");
      return 1;
   }
   printf("overlay_vp_seqlock: ok\n");
   return 0;
}
