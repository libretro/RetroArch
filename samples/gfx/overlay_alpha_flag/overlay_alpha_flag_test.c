/* overlay_alpha_flag_test.c -- the overlay alpha update flag from
 * gfx/video_thread_wrapper.c, reproduced and beaten on.
 *
 * The protocol: alpha values are float bits in atomic ints; the main
 * thread stores a value relaxed then store-releases alpha_update,
 * fire-and-forget. The video thread's per-frame apply clears the
 * flag with an acquire RMW (fetch_and 0) BEFORE reading the values.
 * The claims a regression would break:
 *
 *   1. No update is ever lost: after the writer finishes and one
 *      more apply runs, the applied shadow equals the final values
 *      at every index. A set landing mid-apply re-raises the flag
 *      and lands whole next round - clearing the flag first is what
 *      makes the loss impossible.
 *   2. Values are whole (atomic bits, never a torn float), which
 *      ThreadSanitizer holds alongside the pairing.
 *
 * The sabotage lane clears the flag AFTER the read loop - the
 * intuitive order - and loses updates, which is the point.
 *
 * Paced with short sleeps so a single-processor machine interleaves
 * the threads. */

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

#define N_IDX    24
#define N_WRITES 4000

static retro_atomic_int_t g_alpha[N_IDX];
static retro_atomic_int_t g_update;
static retro_atomic_int_t g_writer_done;
static retro_atomic_int_t g_in_stall;   /* sabotage choreography */

static int   g_sabotage_clear_after = 0;

static int fbits(float f)   { int b; memcpy(&b,&f,sizeof(b)); return b; }
static float bfloat(int b)  { float f; memcpy(&f,&b,sizeof(f)); return f; }

static void writer(void *arg)
{
   int w;
   (void)arg;
   for (w = 1; w <= N_WRITES; w++)
   {
      unsigned idx = (unsigned)(w * 2654435761u) % N_IDX;
      if (g_sabotage_clear_after && w == N_WRITES)
      {
         /* The choreographed final write: land it inside the
          * reader's read-to-clear window, where clear-after loses
          * it. Paced wait, not a spin. */
         while (!retro_atomic_load_acquire_int(&g_in_stall))
            sleep_us(40);
      }
      retro_atomic_store_relaxed_int(&g_alpha[idx],
            fbits((float)w / (float)N_WRITES));
      retro_atomic_store_release_int(&g_update, 1);
      if ((w & 15) == 0)
         sleep_us(90);
   }
   retro_atomic_store_release_int(&g_writer_done, 1);
}

int main(void)
{
   sthread_t *w;
   float shadow[N_IDX];
   int i, applies = 0, mismatches = 0;

   for (i = 0; i < N_IDX; i++)
   {
      retro_atomic_store_relaxed_int(&g_alpha[i], fbits(1.0f));
      shadow[i] = 1.0f;
   }
   retro_atomic_store_relaxed_int(&g_update, 0);
   retro_atomic_store_relaxed_int(&g_writer_done, 0);
   retro_atomic_store_relaxed_int(&g_in_stall, 0);

   w = sthread_create(writer, NULL);

   for (;;)
   {
      int done = retro_atomic_load_acquire_int(&g_writer_done);
      if (g_sabotage_clear_after)
      {
         /* SABOTAGE: read first, clear after - a set landing between
          * the read of its index and the clear is wiped unseen. */
         if (retro_atomic_load_acquire_int(&g_update))
         {
            for (i = 0; i < N_IDX; i++)
               shadow[i] = bfloat(retro_atomic_load_relaxed_int(&g_alpha[i]));
            /* the read-to-clear window, held open for the writer */
            retro_atomic_store_release_int(&g_in_stall, 1);
            sleep_us(400);
            retro_atomic_store_relaxed_int(&g_update, 0);
            retro_atomic_store_relaxed_int(&g_in_stall, 0);
            applies++;
         }
      }
      else if (retro_atomic_fetch_and_int(&g_update, 0))
      {
         for (i = 0; i < N_IDX; i++)
            shadow[i] = bfloat(retro_atomic_load_relaxed_int(&g_alpha[i]));
         if ((applies & 63) == 0)
            sleep_us(200);
         applies++;
      }
      if (done)
      {
         /* one final apply pass mirrors the next frame after the
          * last set: its flag, if raised, is honoured above on the
          * next loop - run one explicit round then stop */
         if (retro_atomic_fetch_and_int(&g_update, 0)
             || g_sabotage_clear_after)
         {
            for (i = 0; i < N_IDX; i++)
               if (!g_sabotage_clear_after)
                  shadow[i] = bfloat(retro_atomic_load_relaxed_int(&g_alpha[i]));
            applies++;
         }
         break;
      }
      sleep_us(60);
   }
   sthread_join(w);

   for (i = 0; i < N_IDX; i++)
      if (shadow[i] != bfloat(retro_atomic_load_relaxed_int(&g_alpha[i])))
         mismatches++;

   printf("overlay_alpha_flag: %d writes, %d applies, %d stale slot(s)\n",
         N_WRITES, applies, mismatches);
   if (mismatches)
   {
      printf("overlay_alpha_flag: FAILED (update lost)\n");
      return 1;
   }
   printf("overlay_alpha_flag: ok\n");
   return 0;
}
