/* hw_context_mirror_test.c -- the hw_context_type publication from
 * gfx/video_driver.c, reproduced and beaten on.
 *
 * The protocol: hw_render is main-thread state; its context_type is
 * mirrored in an atomic that SET_HW_RENDER store-releases after
 * copying the callback in and video_driver_free_hw_context()
 * store-releases (to NONE) after context_destroy() and the memset.
 * video_driver_is_hw_context() on any thread is one acquire load of
 * the mirror. The claim a regression would break:
 *
 *   An acquire reader that observes NONE is guaranteed the teardown
 *   completed: every byte of the struct reads as zero. (Reading the
 *   struct after observing a non-NONE mirror is NOT licensed - the
 *   struct belongs to the main thread - and this harness never does
 *   it; ThreadSanitizer holds that discipline too, via the
 *   release/acquire pair.)
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

#define N_CYCLES 3000
#define CTX_NONE 0

struct fake_hw
{
   char blob[192];   /* stands in for retro_hw_render_callback */
};

static struct fake_hw       g_hw;
static retro_atomic_int_t   g_mirror;
static retro_atomic_int_t   g_stop;

/* sabotage hook: 1 = release the mirror BEFORE the memset, breaking
 * the ordering claim */
static int g_sabotage_early_release = 0;

static void main_thread_sim(void *arg)
{
   int cyc;
   (void)arg;
   for (cyc = 1; cyc <= N_CYCLES; cyc++)
   {
      int type = 1 + (cyc % 7);
      /* SET_HW_RENDER: fill, then publish. */
      memset(&g_hw, (unsigned char)type, sizeof(g_hw));
      retro_atomic_store_release_int(&g_mirror, type);

      sleep_us((cyc & 15) == 0 ? 150 : 20);

      /* free_hw_context: destroy+memset, then publish NONE - the
       * mid-memset stall gives an incorrectly early release a
       * window a reader lands in. */
      if (g_sabotage_early_release)
         retro_atomic_store_release_int(&g_mirror, CTX_NONE);
      memset(g_hw.blob, 0, sizeof(g_hw.blob) / 2);
      if ((cyc & 127) == 0)
         sleep_us(250);
      memset(g_hw.blob + sizeof(g_hw.blob) / 2, 0,
            sizeof(g_hw.blob) - sizeof(g_hw.blob) / 2);
      if (!g_sabotage_early_release)
         retro_atomic_store_release_int(&g_mirror, CTX_NONE);
   }
   retro_atomic_store_release_int(&g_stop, 1);
}

int main(void)
{
   sthread_t *m;
   int dirty = 0, none_seen = 0;

   memset(&g_hw, 0, sizeof(g_hw));
   retro_atomic_store_relaxed_int(&g_mirror, CTX_NONE);
   retro_atomic_store_relaxed_int(&g_stop, 0);

   m = sthread_create(main_thread_sim, NULL);

   while (!retro_atomic_load_acquire_int(&g_stop))
   {
      if (retro_atomic_load_acquire_int(&g_mirror) == CTX_NONE)
      {
         size_t i;
         int bad = 0;
         for (i = 0; i < sizeof(g_hw.blob); i++)
            if (g_hw.blob[i])
               bad = 1;
         /* A non-zero byte here is only valid if a NEW set landed
          * since our load; re-check the mirror to discount it. */
         if (bad
             && retro_atomic_load_acquire_int(&g_mirror) == CTX_NONE)
            dirty++;
         none_seen++;
      }
      sleep_us(70);
   }
   sthread_join(m);

   printf("hw_context_mirror: %d cycles, NONE observed %d times\n",
         N_CYCLES, none_seen);
   if (!none_seen)
      printf("  WARN: reader never sampled a NONE window\n");
   if (dirty)
   {
      printf("  FAIL: %d observation(s) of NONE with a dirty struct\n",
            dirty);
      printf("hw_context_mirror: FAILED\n");
      return 1;
   }
   printf("hw_context_mirror: ok\n");
   return 0;
}
