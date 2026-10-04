/* Regression test for the tpool_wait predicate in
 * libretro-common/rthreads/tpool.c.
 *
 * Background
 * ----------
 * tpool_wait is the public "wait until all queued work has been
 * processed" entry point.  Pre-fix, its predicate was:
 *
 *     (!tp->stop && tp->working_cnt != 0) ||
 *     (tp->stop && tp->thread_cnt != 0)
 *
 * That is, in the non-stopping case it waited only on working_cnt.
 * working_cnt is the number of jobs currently being executed by a
 * worker -- it is incremented when a worker pops a job off the
 * queue, not when a producer pushes one on.
 *
 * So the following sequence:
 *
 *     tp = tpool_create(N);
 *     for (i = 0; i < M; i++) tpool_add_work(tp, job, ctx);
 *     tpool_wait(tp);    // <-- can return immediately
 *     // ctx state read here, expecting all M jobs to have run
 *
 * could return from tpool_wait before any worker had picked up the
 * first job: working_cnt was 0 because no worker had yet dequeued.
 * Callers that read shared state after tpool_wait would observe
 * zero or partial completion.
 *
 * Worker code already signals working_cond only when both
 * working_cnt == 0 AND work_first == NULL (tpool.c:139), so the
 * fix is to widen the wait predicate to match: also wait while
 * work_first is non-NULL.
 *
 * What this test asserts
 * ----------------------
 * 1. After a sequence of tpool_add_work followed by tpool_wait,
 *    every queued job has run.  Verified by an under-lock counter.
 * 2. tpool_create(0) defaults to a working pool (documented in
 *    tpool.h) that completes a posted job before tpool_wait
 *    returns.
 * 3. tpool_create / tpool_destroy round-trips cleanly across many
 *    iterations (heap consistency checked by ASan/UBSan/LSan in
 *    the workflow).
 * 4. tpool_destroy on a pool with queued-but-not-yet-run work does
 *    not crash and does not corrupt the heap.  tpool_destroy is
 *    documented to discard outstanding queued work, so we do NOT
 *    assert on the counter -- we only verify clean teardown.
 * 5. No worker thread is still running when tpool_destroy returns.
 *    The pool used to detach its threads and return once each had
 *    counted itself out, which a thread does before it has ended:
 *    the caller went on - to free what the jobs used, or to leave
 *    the process - with pool threads still on their way out.  Each
 *    worker is marked with a thread-specific value whose destructor
 *    runs as the thread ends; every destructor must have run by the
 *    time tpool_destroy is back.  POSIX threads only.
 *
 * What this test does NOT assert
 * ------------------------------
 * It does not exercise tpool_wait followed by further
 * tpool_add_work, since that is not a documented use pattern.  It
 * does not exercise the single-threaded fallback (HAVE_THREADS
 * off); tpool.c requires threads.
 *
 * How the regression is caught
 * ----------------------------
 * Without the fix, test_work_executes_once and
 * test_zero_threads_default both fail observably: the counter is
 * less than the expected job count (typically zero, since the
 * producer outpaces the workers' first dequeue).  The test prints
 * a clear FAIL line and exits non-zero so the CI workflow flags
 * it.  Built under ASan + UBSan (the workflow default), any
 * collateral heap or UB issue is also caught.
 *
 * The test is bounded: tight loops are sized to run inside the
 * workflow's per-binary 60-second timeout on a github-hosted
 * runner.  Wall-clock under ASan + UBSan is under one second.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rthreads/rthreads.h>
#include <rthreads/tpool.h>

#if !defined(_WIN32)
#include <pthread.h>
#include <retro_atomic.h>
#define HAVE_EXIT_PROBE 1
#endif

#define POOL_THREADS         4
#define WORK_JOBS         1000
#define ROUNDTRIP_ITERS   2000
#define STRESS_CYCLES      200
#define STRESS_JOBS         32
#define EXIT_CYCLES        300

struct work_ctx
{
   slock_t *lock;
   int      counter;
};

static void inc_job(void *arg)
{
   struct work_ctx *ctx = (struct work_ctx *)arg;
   slock_lock(ctx->lock);
   ctx->counter++;
   slock_unlock(ctx->lock);
}

/* -----------------------------------------------------------------
 * Test 1: tpool_wait correctly drains the queue.
 *
 * This is the regression case.  Before the predicate fix, the
 * counter would commonly read 0 here: working_cnt was still 0 at
 * the moment tpool_wait was entered (no worker had yet dequeued
 * any of the just-pushed work) and the old predicate returned
 * immediately.
 * ----------------------------------------------------------------- */
static int test_work_executes_once(void)
{
   tpool_t        *tp;
   int             i;
   struct work_ctx ctx;
   int             rc = 0;

   ctx.counter = 0;
   ctx.lock    = slock_new();
   if (!ctx.lock)
   {
      printf("[FAIL] test_work_executes_once: slock_new failed\n");
      return 1;
   }

   tp = tpool_create(POOL_THREADS);
   if (!tp)
   {
      printf("[FAIL] test_work_executes_once: tpool_create returned NULL\n");
      slock_free(ctx.lock);
      return 1;
   }

   for (i = 0; i < WORK_JOBS; i++)
   {
      if (!tpool_add_work(tp, inc_job, &ctx))
      {
         printf("[FAIL] test_work_executes_once: tpool_add_work failed at i=%d\n", i);
         rc = 1;
         break;
      }
   }

   tpool_wait(tp);
   tpool_destroy(tp);

   if (!rc)
   {
      slock_lock(ctx.lock);
      if (ctx.counter != WORK_JOBS)
      {
         printf("[FAIL] test_work_executes_once: counter=%d expected=%d\n",
               ctx.counter, WORK_JOBS);
         rc = 1;
      }
      else
         printf("[PASS] test_work_executes_once (%d jobs across %d threads)\n",
               WORK_JOBS, POOL_THREADS);
      slock_unlock(ctx.lock);
   }

   slock_free(ctx.lock);
   return rc;
}

/* -----------------------------------------------------------------
 * Test 2: tpool_create(0) gives a working default pool.
 *
 * tpool.h documents num=0 as defaulting to 2 threads.  Smoke-test
 * that this path produces a usable pool that runs a single job to
 * completion before tpool_wait returns.
 * ----------------------------------------------------------------- */
static int test_zero_threads_default(void)
{
   tpool_t        *tp;
   struct work_ctx ctx;
   int             rc = 0;

   ctx.counter = 0;
   ctx.lock    = slock_new();
   if (!ctx.lock)
   {
      printf("[FAIL] test_zero_threads_default: slock_new failed\n");
      return 1;
   }

   tp = tpool_create(0);
   if (!tp)
   {
      printf("[FAIL] test_zero_threads_default: tpool_create(0) returned NULL\n");
      slock_free(ctx.lock);
      return 1;
   }

   if (!tpool_add_work(tp, inc_job, &ctx))
   {
      printf("[FAIL] test_zero_threads_default: tpool_add_work failed\n");
      rc = 1;
   }

   tpool_wait(tp);
   tpool_destroy(tp);

   if (!rc)
   {
      slock_lock(ctx.lock);
      if (ctx.counter != 1)
      {
         printf("[FAIL] test_zero_threads_default: counter=%d expected=1\n",
               ctx.counter);
         rc = 1;
      }
      else
         printf("[PASS] test_zero_threads_default\n");
      slock_unlock(ctx.lock);
   }

   slock_free(ctx.lock);
   return rc;
}

/* -----------------------------------------------------------------
 * Test 3: create/destroy round-trip with no work.
 *
 * Heap consistency check.  Workers transition straight from their
 * initial scond_wait to the stop branch; on the workflow runner
 * with ASan+UBSan, any heap-buffer-overflow / use-after-free /
 * undefined behaviour during teardown surfaces here.
 * ----------------------------------------------------------------- */
static int test_create_destroy_no_work(void)
{
   int i;
   for (i = 0; i < ROUNDTRIP_ITERS; i++)
   {
      tpool_t *tp = tpool_create(POOL_THREADS);
      if (!tp)
      {
         printf("[FAIL] test_create_destroy_no_work: tpool_create returned NULL at i=%d\n", i);
         return 1;
      }
      tpool_destroy(tp);
   }
   printf("[PASS] test_create_destroy_no_work (%d iterations x %d threads)\n",
         ROUNDTRIP_ITERS, POOL_THREADS);
   return 0;
}

/* -----------------------------------------------------------------
 * Test 4: stress -- create / push some work / destroy without
 * waiting.
 *
 * tpool_destroy is documented to discard outstanding queued work,
 * so the counter is non-deterministic and we don't check it.
 * What we do check is that the destroyer terminates and the heap
 * stays consistent across many fast cycles -- ASan/UBSan/LSan
 * carry the verification.  This case was the one I originally
 * (incorrectly) flagged as a UAF in the audit; the real situation
 * is that scond_wait re-acquires the mutex before the destroyer
 * can free it, so the original code is heap-safe here.  Keeping
 * the test in place as a guard against any future regression that
 * would actually break that invariant.
 * ----------------------------------------------------------------- */
static int test_stress_destroy_with_pending(void)
{
   int              i;
   int              j;
   struct work_ctx  ctx;

   ctx.counter = 0;
   ctx.lock    = slock_new();
   if (!ctx.lock)
   {
      printf("[FAIL] test_stress_destroy_with_pending: slock_new failed\n");
      return 1;
   }

   for (i = 0; i < STRESS_CYCLES; i++)
   {
      tpool_t *tp = tpool_create(POOL_THREADS);
      if (!tp)
      {
         printf("[FAIL] test_stress_destroy_with_pending: tpool_create returned NULL at i=%d\n", i);
         slock_free(ctx.lock);
         return 1;
      }
      for (j = 0; j < STRESS_JOBS; j++)
         tpool_add_work(tp, inc_job, &ctx);
      /* Deliberately no tpool_wait here. */
      tpool_destroy(tp);
   }

   printf("[PASS] test_stress_destroy_with_pending (%d cycles x %d jobs, ran=%d)\n",
         STRESS_CYCLES, STRESS_JOBS, ctx.counter);

   slock_free(ctx.lock);
   return 0;
}

/* -----------------------------------------------------------------
 * Test 5: no worker outlives tpool_destroy.
 * ----------------------------------------------------------------- */
#ifdef HAVE_EXIT_PROBE
static pthread_key_t      exit_key;
static retro_atomic_int_t exit_marked; /* workers holding a marking job */
static retro_atomic_int_t exit_ended;  /* marked workers that have ended */

static void exit_probe_dtor(void *p)
{
   (void)p;
   retro_atomic_fetch_add_int(&exit_ended, 1);
}

/* One per worker: marks the thread, then holds it until every worker
 * has one, so that no worker takes two. */
static void exit_mark_job(void *arg)
{
   pthread_setspecific(exit_key, arg);
   retro_atomic_fetch_add_int(&exit_marked, 1);
   while (retro_atomic_load_acquire_int(&exit_marked) < POOL_THREADS)
      sthread_yield();
}

static int test_no_worker_outlives_destroy(void)
{
   int i, j;

   if (pthread_key_create(&exit_key, exit_probe_dtor))
   {
      printf("[FAIL] test_no_worker_outlives_destroy: pthread_key_create\n");
      return 1;
   }

   for (i = 0; i < EXIT_CYCLES; i++)
   {
      int ended;
      tpool_t *tp = tpool_create(POOL_THREADS);
      if (!tp)
      {
         printf("[FAIL] test_no_worker_outlives_destroy: tpool_create returned NULL at i=%d\n", i);
         return 1;
      }
      retro_atomic_int_init(&exit_marked, 0);
      retro_atomic_int_init(&exit_ended, 0);
      for (j = 0; j < POOL_THREADS; j++)
         tpool_add_work(tp, exit_mark_job, &exit_key);
      tpool_wait(tp);
      tpool_destroy(tp);

      ended = retro_atomic_load_acquire_int(&exit_ended);
      if (ended != POOL_THREADS)
      {
         printf("[FAIL] test_no_worker_outlives_destroy: cycle %d, %d of %d"
               " workers had ended when tpool_destroy returned\n",
               i, ended, POOL_THREADS);
         return 1;
      }
   }

   pthread_key_delete(exit_key);
   printf("[PASS] test_no_worker_outlives_destroy (%d cycles x %d threads)\n",
         EXIT_CYCLES, POOL_THREADS);
   return 0;
}
#endif

int main(void)
{
   int failures = 0;

   failures += test_work_executes_once();
   failures += test_zero_threads_default();
   failures += test_create_destroy_no_work();
   failures += test_stress_destroy_with_pending();
#ifdef HAVE_EXIT_PROBE
   failures += test_no_worker_outlives_destroy();
#endif

   if (failures)
   {
      printf("\n%d tpool_wait regression test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll tpool_wait regression tests passed.\n");
   return 0;
}
