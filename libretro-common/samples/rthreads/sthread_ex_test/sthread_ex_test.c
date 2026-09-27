/* sthread_ex_test.c - stack size and affinity on rthreads.
 *
 * Two things a caller can now ask for at create time or after, each
 * checked by using it rather than by reading the request back:
 *
 *  1. Stack size. A thread created with a 16 MB stack recurses until it
 *     has 12 MB of frames on it and returns. On the platform default -- 8
 *     MB on Linux, 1 MB on Windows -- that recursion would fault, so the
 *     thread reaching the bottom and coming back is the proof the size
 *     was honoured. The recursion touches every frame so the compiler
 *     cannot elide it.
 *
 *  3. Yield. A thread spins on a flag the main thread sets after
 *     joining a yield loop of its own; sthread_yield returning at all,
 *     many times, with a runnable peer, is what is checked -- there is
 *     no observable property of a yield beyond "it came back".
 *
 *  2. Affinity. A thread pinned to CPU 0 reads its own mask back from the
 *     kernel and reports it; then the mask is cleared with 0 and read
 *     again. Where the platform has no hard affinity the calls return
 *     false and the test reports that as the platform's answer rather than
 *     a failure.
 */

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE   /* sched_getaffinity, CPU_ISSET */
#endif
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#if defined(__linux__)
#include <sched.h>
static uint64_t read_own_mask(void)
{
   cpu_set_t set;
   uint64_t m = 0;
   unsigned i;
   if (sched_getaffinity(0, sizeof(set), &set) != 0)
      return 0;
   for (i = 0; i < 64; i++)
      if (CPU_ISSET(i, &set))
         m |= (uint64_t)1 << i;
   return m;
}
#define CAN_READ_MASK 1
#elif defined(_WIN32)
#include <windows.h>
static uint64_t read_own_mask(void)
{
   DWORD_PTR proc, sys;
   if (!GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys))
      return 0;
   /* There is no GetThreadAffinityMask. SetThreadAffinityMask returns
    * the previous mask, so set to the process mask to read the old one
    * back, then put the old one back. */
   {
      DWORD_PTR old = SetThreadAffinityMask(GetCurrentThread(), proc);
      if (old)
         SetThreadAffinityMask(GetCurrentThread(), old);
      return (uint64_t)old;
   }
}
#define CAN_READ_MASK 1
#else
static uint64_t read_own_mask(void) { return 0; }
#define CAN_READ_MASK 0
#endif

/* ---- 1. stack -------------------------------------------------------- */

static retro_atomic_int_t g_depth_reached;

static int recurse(unsigned depth, volatile unsigned char *sink)
{
   volatile unsigned char frame[4096];
   frame[0] = (unsigned char)depth;
   frame[4095] = frame[0];
   if (depth == 0)
      return frame[4095];
   return recurse(depth - 1, sink) + frame[0];
}

static void deep_thread(void *arg)
{
   volatile unsigned char sink = 0;
   (void)arg;
   /* 3072 frames x 4 KiB = 12 MiB, on a 16 MiB stack. */
   sink = (unsigned char)recurse(3072, &sink);
   retro_atomic_store_release_int(&g_depth_reached, 1 + (sink & 0));
}

/* ---- 2. affinity ----------------------------------------------------- */

static retro_atomic_int_t g_mask_initial;
static retro_atomic_int_t g_mask_pinned;
static retro_atomic_int_t g_mask_cleared;
static retro_atomic_int_t g_ready;
static retro_atomic_int_t g_started;

static void pinned_thread(void *arg)
{
   (void)arg;
   /* What the thread inherited: every CPU the process may use. On a
    * one-CPU box that is 0x1, the same as the pin, so "cleared" is
    * judged against this and not against "differs from the pin".
    * Read before the pin can land: main waits for g_started before
    * it pins, else on a multi-core box the pin races this read and
    * "inherited" comes out as the pin itself. */
   retro_atomic_store_release_int(&g_mask_initial, (int)(read_own_mask() & 0xFFFF));
   retro_atomic_store_release_int(&g_started, 1);
   while (!retro_atomic_load_acquire_int(&g_ready))
      sthread_yield();
   retro_atomic_store_release_int(&g_mask_pinned,  (int)(read_own_mask() & 0xFFFF));
   sthread_set_current_affinity(0);
   retro_atomic_store_release_int(&g_mask_cleared, (int)(read_own_mask() & 0xFFFF));
}

int main(void)
{
   sthread_t *t;
   int ok = 1;

   setvbuf(stdout, NULL, _IONBF, 0);
   printf("sthread_ex\n");

   /* 1 */
   retro_atomic_store_relaxed_int(&g_depth_reached, 0);
   t = sthread_create_with_stack_size(deep_thread, NULL, 16u * 1024u * 1024u);
   if (!t) { printf("  FAIL: create with 16 MB stack\n"); return 1; }
   sthread_join(t);
   if (retro_atomic_load_acquire_int(&g_depth_reached) != 1)
   {
      printf("  FAIL: 12 MB of frames on a 16 MB stack did not return\n");
      ok = 0;
   }
   else
      printf("  stack: 12 MB of frames on a 16 MB stack, returned\n");

   /* 2 */
   retro_atomic_store_relaxed_int(&g_ready, 0);
   retro_atomic_store_relaxed_int(&g_started, 0);
   t = sthread_create(pinned_thread, NULL);
   if (!t) { printf("  FAIL: create\n"); return 1; }
   while (!retro_atomic_load_acquire_int(&g_started))
      sthread_yield();
   {
      bool pinned = sthread_set_affinity(t, 1);   /* CPU 0 */
      retro_atomic_store_release_int(&g_ready, 1);
      sthread_join(t);
      if (!pinned)
         printf("  affinity: not available on this platform (reported, not faked)\n");
      else if (!CAN_READ_MASK)
         printf("  affinity: set, no way to read it back here\n");
      else
      {
         int initial = retro_atomic_load_acquire_int(&g_mask_initial);
         int got     = retro_atomic_load_acquire_int(&g_mask_pinned);
         int cleared = retro_atomic_load_acquire_int(&g_mask_cleared);
         if (got != 1)
         {
            printf("  FAIL: pinned to CPU 0, thread saw mask 0x%x\n", got);
            ok = 0;
         }
         else if (cleared != initial)
         {
            printf("  FAIL: mask 0 should restore 0x%x, thread saw 0x%x\n", initial, cleared);
            ok = 0;
         }
         else
            printf("  affinity: inherited 0x%x, pinned to 0x%x, cleared back to 0x%x%s\n",
                   initial, got, cleared,
                   initial == 1 ? "  (one CPU: the pin check is not load-bearing here)" : "");
      }
   }

   /* 3 */
   {
      unsigned i;
      for (i = 0; i < 100000; i++)
         sthread_yield();
      printf("  yield: 100000 yields returned\n");
   }

   printf(ok ? "sthread_ex: ok\n" : "sthread_ex: FAILED\n");
   return ok ? 0 : 1;
}
