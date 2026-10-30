/* tpool_handoff_test.c -- every job posted is run, once, in order,
 * and no worker sleeps on one.
 *
 * The pool hands jobs over without a lock: producers and workers claim
 * queue slots with a compare-and-swap, and a worker with nothing to do
 * parks by itself and is woken by the post that needs it. What can go
 * wrong there is a matter of two threads' steps falling in one
 * particular order - a worker parking just as a job is posted, and the
 * job sitting unrun - so tpool.c is built here with its test yields
 * on, which make those orders common instead of rare.
 *
 *   1  Jobs posted one at a time to a pool that is mostly asleep: each
 *      must be run before the next is posted, within a deadline. A
 *      lost wake-up is a job that never runs.
 *   2  Several producers posting more jobs than the queue's ring
 *      holds: every job runs exactly once.
 *   3  One producer, one worker: the jobs run in the order posted,
 *      through the ring filling up and emptying again.
 *   4  A pool destroyed with jobs waiting frees them.
 *   5  Every worker stalled while a backlog four times the ring builds
 *      up, several rounds: each job runs exactly once, and no round
 *      allocates an overflow node - the pool's spares cover it.
 *   6  Bursts past the ring from several producers at once, into a
 *      pool that sleeps between them, many rounds: the overflow is
 *      pushed onto while a worker drains it, and every burst runs
 *      out before the next.
 *   7  A job pushed onto the overflow while a helper drains it, the one
 *      worker turned away from the drain and gone to sleep: the helper
 *      looks again before it lets the overflow go, and the job runs.
 *   8  A worker that drains from its last look before sleeping, and
 *      wakes itself for a job it moved into the ring: the wake goes on
 *      to the other, sleeping worker, which runs that job while the
 *      first runs one that waits for it.
 *   9  One producer's posts while another's overflow job is in and not
 *      yet counted: they run in the order posted, and before the other
 *      producer is let go.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <features/features_cpu.h>

#define TPOOL_FUZZ_SCHEDULE
#include <retro_atomic.h>
/* The lane whose hooks are on, read by the pool's threads. */
static retro_atomic_int_t hook_lane;
struct tpool_thread;
static void lane_drain(void);
static void lane_post(void);
static void lane_park(struct tpool_thread *self);
#define TPOOL_DRAIN_HOOK()     lane_drain()
#define TPOOL_POST_HOOK()      lane_post()
#define TPOOL_PARK_HOOK(self)  lane_park(self)
#include "../../../rthreads/tpool.c"

#define WAKE_ROUNDS   5000
#define PRODUCERS     3
#define JOBS_EACH     5000
#define ORDER_JOBS    3000
#define DEADLINE_US   10000000

static int fails;

static void check(const char *what, int ok)
{
   printf("  %-58s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

/* 1 */
static retro_atomic_int_t woke;

static void wake_job(void *arg)
{
   (void)arg;
   retro_atomic_store_release_int(&woke, 1);
}

static void test_no_lost_wakeup(void)
{
   unsigned i, lost = 0;
   tpool_t *tp = tpool_create(3);
   if (!tp)
   {
      check("pool created", 0);
      return;
   }
   for (i = 0; i < WAKE_ROUNDS && !lost; i++)
   {
      retro_time_t t0 = cpu_features_get_time_usec();
      retro_atomic_store_release_int(&woke, 0);
      if (!tpool_add_work(tp, wake_job, NULL))
      {
         lost = 1;
         break;
      }
      while (!retro_atomic_load_acquire_int(&woke))
      {
         if (cpu_features_get_time_usec() - t0 > DEADLINE_US)
         {
            lost = 1;
            break;
         }
         sthread_yield();
      }
   }
   check("a job posted to a sleeping pool is always run", !lost);
   if (!lost)
      tpool_destroy(tp);
}

/* 2 */
static retro_atomic_int_t ran[PRODUCERS * JOBS_EACH];
static retro_atomic_int_t ran_total;
static tpool_t           *once_pool;

static void once_job(void *arg)
{
   retro_atomic_fetch_add_int((retro_atomic_int_t*)arg, 1);
   retro_atomic_fetch_add_int(&ran_total, 1);
}

static void once_producer(void *arg)
{
   unsigned i, base = (unsigned)(size_t)arg * JOBS_EACH;
   for (i = 0; i < JOBS_EACH; i++)
      if (!tpool_add_work(once_pool, once_job, &ran[base + i]))
         once_job(&ran[base + i]);
}

static void test_exactly_once(void)
{
   unsigned   i, wrong = 0;
   sthread_t *threads[PRODUCERS];

   if (!(once_pool = tpool_create(4)))
   {
      check("pool created", 0);
      return;
   }
   for (i = 0; i < PRODUCERS; i++)
      threads[i] = sthread_create(once_producer, (void*)(size_t)i);
   for (i = 0; i < PRODUCERS; i++)
      if (threads[i])
         sthread_join(threads[i]);
      else
         wrong++;
   /* A job nobody woke for would leave tpool_wait() waiting too */
   {
      retro_time_t t0 = cpu_features_get_time_usec();
      while (retro_atomic_load_acquire_int(&ran_total)
            < PRODUCERS * JOBS_EACH)
      {
         if (cpu_features_get_time_usec() - t0 > DEADLINE_US)
         {
            check("every job from several producers was run", 0);
            printf("tpool_handoff_test: FAIL\n");
            exit(1);
         }
         sthread_yield();
      }
   }
   tpool_wait(once_pool);
   for (i = 0; i < PRODUCERS * JOBS_EACH; i++)
      if (retro_atomic_load_acquire_int(&ran[i]) != 1)
         wrong++;
   check("every job from several producers ran exactly once", !wrong);
   tpool_destroy(once_pool);
}

/* 3 */
static int order_next;
static int order_wrong;

static void order_job(void *arg)
{
   /* one worker: nothing else touches these */
   if ((int)(size_t)arg != order_next)
      order_wrong++;
   order_next++;
}

static void test_in_order(void)
{
   unsigned i;
   tpool_t *tp = tpool_create(1);
   if (!tp)
   {
      check("pool created", 0);
      return;
   }
   for (i = 0; i < ORDER_JOBS; i++)
      if (!tpool_add_work(tp, order_job, (void*)(size_t)i))
         order_wrong++;
   tpool_wait(tp);
   check("one producer's jobs run in the order posted",
         !order_wrong && order_next == ORDER_JOBS);
   tpool_destroy(tp);
}

/* 4 */
static void idle_job(void *arg)
{
   (void)arg;
   sthread_yield();
}

static void test_destroy_pending(void)
{
   unsigned i, c;
   for (c = 0; c < 50; c++)
   {
      tpool_t *tp = tpool_create(2);
      if (!tp)
      {
         check("pool created", 0);
         return;
      }
      for (i = 0; i < 600; i++)
         tpool_add_work(tp, idle_job, NULL);
      tpool_destroy(tp);
   }
   check("a pool destroyed with jobs waiting comes apart", 1);
}

/* 5 */
#define SAT_WORKERS 2
#define SAT_JOBS    (TPOOL_QUEUE_SIZE * 4)
#define SAT_ROUNDS  4

static retro_atomic_int_t sat_gate;
static retro_atomic_int_t sat_parked;
static retro_atomic_int_t sat_ran[SAT_JOBS];

static void sat_stall(void *arg)
{
   (void)arg;
   retro_atomic_fetch_add_int(&sat_parked, 1);
   while (!retro_atomic_load_acquire_int(&sat_gate))
      sthread_yield();
}

static void sat_job(void *arg)
{
   retro_atomic_fetch_add_int((retro_atomic_int_t*)arg, 1);
}

static void test_saturation(void)
{
   unsigned r, i, wrong = 0, overflowed = 1;
   tpool_t *tp = tpool_create(SAT_WORKERS);
   if (!tp)
   {
      check("pool created", 0);
      return;
   }
   for (r = 0; r < SAT_ROUNDS; r++)
   {
      retro_time_t t0 = cpu_features_get_time_usec();
      retro_atomic_store_release_int(&sat_gate, 0);
      retro_atomic_store_release_int(&sat_parked, 0);
      for (i = 0; i < SAT_WORKERS; i++)
         tpool_add_work(tp, sat_stall, NULL);
      while (retro_atomic_load_acquire_int(&sat_parked) < SAT_WORKERS
            && cpu_features_get_time_usec() - t0 < DEADLINE_US)
         sthread_yield();
      for (i = 0; i < SAT_JOBS; i++)
      {
         retro_atomic_store_release_int(&sat_ran[i], 0);
         if (!tpool_add_work(tp, sat_job, &sat_ran[i]))
            wrong++;
      }
      if (!retro_atomic_load_acquire_int(&tp->over))
         overflowed = 0;
      retro_atomic_store_release_int(&sat_gate, 1);
      tpool_wait(tp);
      for (i = 0; i < SAT_JOBS; i++)
         if (retro_atomic_load_acquire_int(&sat_ran[i]) != 1)
            wrong++;
   }
   check("a backlog past the ring builds with every worker stalled",
         overflowed);
   check("every job of a stalled backlog ran exactly once", !wrong);
   check("a backlog the spares cover allocates no overflow node",
         !retro_atomic_load_acquire_int(&tp->over_allocs));
   tpool_destroy(tp);
}

/* 6 */
#define BURST_ROUNDS 400
#define BURST_EACH   (TPOOL_QUEUE_SIZE * 2 / 3)
static tpool_t           *burst_pool;
static retro_atomic_int_t burst_go, burst_ran;

static void burst_job(void *arg)
{
   (void)arg;
   retro_atomic_fetch_add_int(&burst_ran, 1);
}

static void burst_producer(void *arg)
{
   unsigned i;
   int      round = (int)(size_t)arg;
   while (retro_atomic_load_acquire_int(&burst_go) != round)
      sthread_yield();
   for (i = 0; i < BURST_EACH; i++)
      if (!tpool_add_work(burst_pool, burst_job, NULL))
         burst_job(NULL);
}

static void test_bursts(void)
{
   unsigned r, i, lost = 0;
   if (!(burst_pool = tpool_create(4)))
   {
      check("burst pool created", 0);
      return;
   }
   for (r = 1; r <= BURST_ROUNDS && !lost; r++)
   {
      sthread_t   *threads[PRODUCERS];
      retro_time_t t0;
      int          want = (int)(r * PRODUCERS * BURST_EACH);
      for (i = 0; i < PRODUCERS; i++)
         threads[i] = sthread_create(burst_producer, (void*)(size_t)r);
      retro_atomic_store_release_int(&burst_go, (int)r);
      for (i = 0; i < PRODUCERS; i++)
         if (threads[i])
            sthread_join(threads[i]);
      t0 = cpu_features_get_time_usec();
      while (retro_atomic_load_acquire_int(&burst_ran) < want)
      {
         if (cpu_features_get_time_usec() - t0 > DEADLINE_US)
         {
            lost = r;
            break;
         }
         sthread_yield();
      }
   }
   check("every burst past the ring ran out before the next", !lost);
   if (lost)
   {
      printf("tpool_handoff_test: FAIL (burst %u left jobs unrun)\n", lost);
      exit(1);
   }
   tpool_wait(burst_pool);
   tpool_destroy(burst_pool);
}

/* 7 */
static retro_atomic_int_t hook_armed, hook_inside, hook_release;
static retro_atomic_int_t late_gate, late_entered, late_ran;
static tpool_t           *late_pool;

static void wait_set(retro_atomic_int_t *flag)
{
   retro_time_t t0 = cpu_features_get_time_usec();
   while (     !retro_atomic_load_acquire_int(flag)
         && cpu_features_get_time_usec() - t0 < DEADLINE_US)
      sthread_yield();
}

static void drain_hook(void)
{
   if (!retro_atomic_load_acquire_int(&hook_armed))
      return;
   retro_atomic_store_release_int(&hook_armed, 0);
   retro_atomic_store_release_int(&hook_inside, 1);
   wait_set(&hook_release);
}

static void late_stall(void *arg)
{
   (void)arg;
   retro_atomic_store_release_int(&late_entered, 1);
   wait_set(&late_gate);
}

static void late_quick(void *arg)
{
   (void)arg;
}

static void late_job(void *arg)
{
   (void)arg;
   retro_atomic_store_release_int(&late_ran, 1);
}

static void late_helper(void *arg)
{
   unsigned i;
   (void)arg;
   /* the ring's jobs, then the drain that takes the overflow */
   for (i = 0; i <= TPOOL_QUEUE_SIZE; i++)
      tpool_help(late_pool);
}

static void test_late_overflow(void)
{
   sthread_t   *helper;
   retro_time_t t0;
   unsigned     i;
   if (!(late_pool = tpool_create(1)))
   {
      check("late pool created", 0);
      return;
   }
   tpool_add_work(late_pool, late_stall, NULL);
   wait_set(&late_entered);
   for (i = 0; i < TPOOL_QUEUE_SIZE; i++)
      tpool_add_work(late_pool, late_quick, NULL);
   tpool_add_work(late_pool, late_quick, NULL);   /* the overflow */
   retro_atomic_store_release_int(&hook_armed, 1);
   retro_atomic_store_release_int(&hook_lane, 7);
   helper = sthread_create(late_helper, NULL);
   wait_set(&hook_inside);
   /* onto the overflow, behind the drain's back */
   tpool_add_work(late_pool, late_job, NULL);
   /* the worker finds the ring empty and the drain taken, and sleeps */
   retro_atomic_store_release_int(&late_gate, 1);
   t0 = cpu_features_get_time_usec();
   while (     !retro_atomic_load_acquire_int(&late_pool->threads[0].parked)
         && cpu_features_get_time_usec() - t0 < DEADLINE_US)
      sthread_yield();
   retro_atomic_store_release_int(&hook_release, 1);
   if (helper)
      sthread_join(helper);
   t0 = cpu_features_get_time_usec();
   while (     !retro_atomic_load_acquire_int(&late_ran)
         && cpu_features_get_time_usec() - t0 < 2000000)
      sthread_yield();
   check("a job pushed behind a drain's back runs",
         retro_atomic_load_acquire_int(&late_ran));
   if (!retro_atomic_load_acquire_int(&late_ran))
   {
      /* still on the overflow: a wait or a destroy would hang */
      printf("tpool_handoff_test: FAIL\n");
      exit(1);
   }
   tpool_wait(late_pool);
   tpool_destroy(late_pool);
   retro_atomic_store_release_int(&hook_lane, 0);
}

/* 8 */
static tpool_t           *self_pool;
static uintptr_t          self_helper_id, self_w0_id;
static retro_atomic_int_t self_gate_a, self_gate_b;
static retro_atomic_int_t self_m_arm, self_m_in, self_m_go;
static retro_atomic_int_t self_p_arm, self_p_in, self_p_go;
static retro_atomic_int_t self_d_arm, self_d_in, self_d_go;
static retro_atomic_int_t self_k_ran, self_l_saw_k, self_l_done;

static void self_park(struct tpool_thread *t)
{
   if (     t == &self_pool->threads[0]
         && retro_atomic_load_acquire_int(&self_p_arm))
   {
      retro_atomic_store_release_int(&self_p_arm, 0);
      retro_atomic_store_release_int(&self_p_in, 1);
      wait_set(&self_p_go);
   }
}

static void self_drain(void)
{
   uintptr_t me = sthread_get_current_thread_id();
   if (me == self_helper_id && retro_atomic_load_acquire_int(&self_m_arm))
   {
      retro_atomic_store_release_int(&self_m_arm, 0);
      retro_atomic_store_release_int(&self_m_in, 1);
      wait_set(&self_m_go);
   }
   if (me == self_w0_id && retro_atomic_load_acquire_int(&self_d_arm))
   {
      retro_atomic_store_release_int(&self_d_arm, 0);
      retro_atomic_store_release_int(&self_d_in, 1);
      wait_set(&self_d_go);
   }
}

static void self_stall(void *arg)
{
   wait_set((retro_atomic_int_t*)arg);
}

static void self_k(void *arg)
{
   (void)arg;
   retro_atomic_store_release_int(&self_k_ran, 1);
}

/* waits, a while, for K to run on the other worker */
static void self_l(void *arg)
{
   retro_time_t t0 = cpu_features_get_time_usec();
   (void)arg;
   while (     !retro_atomic_load_acquire_int(&self_k_ran)
         && cpu_features_get_time_usec() - t0 < 2000000)
      sthread_yield();
   retro_atomic_store_release_int(&self_l_saw_k,
         retro_atomic_load_acquire_int(&self_k_ran));
   retro_atomic_store_release_int(&self_l_done, 1);
}

static void self_helper(void *arg)
{
   unsigned i, n = (unsigned)(size_t)arg;
   self_helper_id = sthread_get_current_thread_id();
   for (i = 0; i < n; i++)
      tpool_help(self_pool);
}

static void self_wait_parked(unsigned i)
{
   retro_time_t t0 = cpu_features_get_time_usec();
   while (     !retro_atomic_load_acquire_int(&self_pool->threads[i].parked)
         && cpu_features_get_time_usec() - t0 < DEADLINE_US)
      sthread_yield();
}

static void self_wait_taken(void)
{
   retro_time_t t0 = cpu_features_get_time_usec();
   while (     retro_atomic_load_acquire_int(&self_pool->count.n.queued)
         && cpu_features_get_time_usec() - t0 < DEADLINE_US)
      sthread_yield();
}

static void test_self_wake(void)
{
   sthread_t *m;
   unsigned   i;
   if (!(self_pool = tpool_create(2)))
   {
      check("self-wake pool created", 0);
      return;
   }
   self_w0_id = sthread_get_thread_id(self_pool->threads[0].thread);
   retro_atomic_store_release_int(&hook_lane, 8);
   self_wait_parked(0);
   self_wait_parked(1);
   /* each worker stalled: the first post wakes the first worker */
   tpool_add_work(self_pool, self_stall, &self_gate_a);
   self_wait_taken();
   self_wait_parked(1);
   tpool_add_work(self_pool, self_stall, &self_gate_b);
   self_wait_taken();
   /* the ring full and one on the overflow; a helper empties the ring
    * and holds the drain */
   for (i = 0; i < TPOOL_QUEUE_SIZE + 1; i++)
      tpool_add_work(self_pool, late_quick, NULL);
   retro_atomic_store_release_int(&self_m_arm, 1);
   m = sthread_create(self_helper, (void*)(size_t)(TPOOL_QUEUE_SIZE + 1));
   wait_set(&self_m_in);
   /* the first worker is turned away from the drain and stops short of
    * parking; the helper finishes */
   retro_atomic_store_release_int(&self_p_arm, 1);
   retro_atomic_store_release_int(&self_gate_a, 1);
   wait_set(&self_p_in);
   retro_atomic_store_release_int(&self_m_go, 1);
   if (m)
      sthread_join(m);
   /* the ring full again with L and K behind it, and nobody parked to
    * wake for them; the helper empties the ring */
   for (i = 0; i < TPOOL_QUEUE_SIZE; i++)
      tpool_add_work(self_pool, late_quick, NULL);
   tpool_add_work(self_pool, self_l, NULL);
   tpool_add_work(self_pool, self_k, NULL);
   m = sthread_create(self_helper, (void*)(size_t)TPOOL_QUEUE_SIZE);
   if (m)
      sthread_join(m);
   /* the first worker parks, looks once more and drains; the second
    * is turned away and sleeps */
   retro_atomic_store_release_int(&self_d_arm, 1);
   retro_atomic_store_release_int(&self_p_go, 1);
   wait_set(&self_d_in);
   retro_atomic_store_release_int(&self_gate_b, 1);
   self_wait_parked(1);
   retro_atomic_store_release_int(&self_d_go, 1);
   wait_set(&self_l_done);
   check("a job a worker moved for another runs while it is busy",
         retro_atomic_load_acquire_int(&self_l_saw_k));
   tpool_wait(self_pool);
   tpool_destroy(self_pool);
   retro_atomic_store_release_int(&hook_lane, 0);
}

/* 9 */
static tpool_t           *order_pool;
static uintptr_t          order_p2_id;
static retro_atomic_int_t order_gate, order_p2_in, order_p2_go;
static retro_atomic_int_t order_seq, order_at[5];

static void order_posted(void)
{
   if (     order_p2_id
         && sthread_get_current_thread_id() == order_p2_id)
   {
      retro_atomic_store_release_int(&order_p2_in, 1);
      wait_set(&order_p2_go);
   }
}

static void order_tag(void *arg)
{
   retro_atomic_store_release_int(&order_at[(size_t)arg],
         retro_atomic_fetch_add_int(&order_seq, 1) + 1);
}

static void order_p2(void *arg)
{
   (void)arg;
   order_p2_id = sthread_get_current_thread_id();
   tpool_add_work(order_pool, order_tag, (void*)2);
}

static void order_helper(void *arg)
{
   unsigned i;
   for (i = 0; i < (unsigned)(size_t)arg; i++)
      tpool_help(order_pool);
}

static void lane_drain(void)
{
   switch (retro_atomic_load_acquire_int(&hook_lane))
   {
      case 7: drain_hook(); break;
      case 8: self_drain(); break;
      default: break;
   }
}

static void lane_post(void)
{
   if (retro_atomic_load_acquire_int(&hook_lane) == 9)
      order_posted();
}

static void lane_park(struct tpool_thread *self)
{
   if (retro_atomic_load_acquire_int(&hook_lane) == 8)
      self_park(self);
}

static void test_counted_order(void)
{
   sthread_t   *p2, *m;
   retro_time_t t0;
   unsigned     i;
   if (!(order_pool = tpool_create(1)))
   {
      check("order pool created", 0);
      return;
   }
   retro_atomic_store_release_int(&hook_lane, 9);
   while (!retro_atomic_load_acquire_int(&order_pool->threads[0].parked))
      sthread_yield();
   tpool_add_work(order_pool, self_stall, &order_gate);
   while (retro_atomic_load_acquire_int(&order_pool->count.n.queued))
      sthread_yield();
   for (i = 0; i < TPOOL_QUEUE_SIZE; i++)
      tpool_add_work(order_pool, late_quick, NULL);
   tpool_add_work(order_pool, order_tag, (void*)1); /* the overflow */
   /* another producer's job in, held before it returns */
   p2 = sthread_create(order_p2, NULL);
   wait_set(&order_p2_in);
   m  = sthread_create(order_helper, (void*)(size_t)(TPOOL_QUEUE_SIZE + 1));
   if (m)
      sthread_join(m);
   tpool_add_work(order_pool, order_tag, (void*)3);
   tpool_add_work(order_pool, order_tag, (void*)4);
   retro_atomic_store_release_int(&order_gate, 1);
   t0 = cpu_features_get_time_usec();
   while (     !retro_atomic_load_acquire_int(&order_at[4])
         && cpu_features_get_time_usec() - t0 < 2000000)
      sthread_yield();
   check("one producer's jobs run in order beside another's held one",
            retro_atomic_load_acquire_int(&order_at[3])
         && retro_atomic_load_acquire_int(&order_at[4])
         &&    retro_atomic_load_acquire_int(&order_at[3])
             < retro_atomic_load_acquire_int(&order_at[4]));
   retro_atomic_store_release_int(&order_p2_go, 1);
   if (p2)
      sthread_join(p2);
   retro_atomic_store_release_int(&hook_lane, 0);
   tpool_wait(order_pool);
   tpool_destroy(order_pool);
}

#ifdef RETRO_ATOMIC_LOCK_FREE
static retro_atomic_int_t try_ran;
static void try_job(void *arg)
{
   (void)arg;
   retro_atomic_fetch_add_int(&try_ran, 1);
}
#define TRY_JOBS 2000
static retro_atomic_int_t try_counts[PRODUCERS * TRY_JOBS];
static unsigned char try_accepted[PRODUCERS * TRY_JOBS];
static tpool_t *try_pool;
static void try_count_job(void *arg)
{
   retro_atomic_fetch_add_int((retro_atomic_int_t*)arg, 1);
}
static void try_producer(void *arg)
{
   unsigned i, base = (unsigned)(size_t)arg * TRY_JOBS;
   for (i = 0; i < TRY_JOBS; i++)
      try_accepted[base + i] = tpool_try_add_work(try_pool, try_count_job, &try_counts[base + i]);
}
static void test_try_contention(void)
{
   sthread_t *threads[PRODUCERS];
   unsigned i, wrong = 0, accepted = 0;
   try_pool = tpool_create(4);
   if (!try_pool) { check("contended try pool created", 0); return; }
   for (i = 0; i < PRODUCERS * TRY_JOBS; i++)
      retro_atomic_store_release_int(&try_counts[i], 0);
   memset(try_accepted, 0, sizeof(try_accepted));
   for (i = 0; i < PRODUCERS; i++)
   {
      threads[i] = sthread_create(try_producer, (void*)(size_t)i);
      if (!threads[i]) wrong++;
   }
   for (i = 0; i < PRODUCERS; i++)
      if (threads[i]) sthread_join(threads[i]);
   tpool_wait(try_pool);
   for (i = 0; i < PRODUCERS * TRY_JOBS; i++)
   {
      accepted += try_accepted[i];
      if (retro_atomic_load_acquire_int(&try_counts[i]) != try_accepted[i]) wrong++;
   }
   check("contended try posts execute exactly the accepted jobs", !wrong && accepted);
   check("contended try posts never use overflow or leave counts",
         !retro_atomic_load_acquire_int(&try_pool->over)
         && !retro_atomic_load_acquire_int(&try_pool->count.n.outstanding));
   tpool_destroy(try_pool);
}
static void test_try_post(void)
{
   tpool_t *tp = tpool_create(1);
   unsigned i, accepted = 0;
   retro_time_t start;
   if (!tp) { check("try pool created", 0); return; }
   retro_atomic_store_release_int(&try_ran, 0);
   retro_atomic_store_release_int(&sat_gate, 0);
   retro_atomic_store_release_int(&sat_parked, 0);
   tpool_add_work(tp, sat_stall, NULL);
   start = cpu_features_get_time_usec();
   while (!retro_atomic_load_acquire_int(&sat_parked)
         && cpu_features_get_time_usec() - start < DEADLINE_US) sthread_yield();
   check("try worker is stalled", retro_atomic_load_acquire_int(&sat_parked) == 1);
   for (i = 0; i < TPOOL_QUEUE_SIZE; i++)
      if (tpool_try_add_work(tp, try_job, NULL)) accepted++;
   check("try fills the ring without allocating overflow", accepted == TPOOL_QUEUE_SIZE
         && !retro_atomic_load_acquire_int(&tp->over));
   check("try refuses a full ring without changing accounting",
         !tpool_try_add_work(tp, try_job, NULL)
         && retro_atomic_load_acquire_int(&tp->count.n.outstanding) == TPOOL_QUEUE_SIZE + 1);
   check("try rejects missing pool or callback", !tpool_try_add_work(NULL, try_job, NULL)
         && !tpool_try_add_work(tp, NULL, NULL));
   tpool_add_work(tp, try_job, NULL);
   check("try does not bypass pending overflow", !tpool_try_add_work(tp, try_job, NULL));
   retro_atomic_store_release_int(&sat_gate, 1);
   tpool_wait(tp);
   check("every accepted try job runs once", retro_atomic_load_acquire_int(&try_ran)
         == TPOOL_QUEUE_SIZE + 1);
   check("try accepts again after draining", tpool_try_add_work(tp, try_job, NULL));
   tpool_wait(tp);
   check("drained try count balances", retro_atomic_load_acquire_int(&try_ran)
         == TPOOL_QUEUE_SIZE + 2 && !retro_atomic_load_acquire_int(&tp->count.n.outstanding));
   tpool_destroy(tp);
}
#endif

int main(void)
{
   test_no_lost_wakeup();
   test_exactly_once();
   test_in_order();
   test_destroy_pending();
   test_saturation();
   test_bursts();
#ifdef RETRO_ATOMIC_LOCK_FREE
   test_late_overflow();
   test_self_wake();
   test_counted_order();
   test_try_post();
   test_try_contention();
#endif
   if (fails)
   {
      printf("tpool_handoff_test: FAIL (%d)\n", fails);
      return 1;
   }
   printf("tpool_handoff_test: PASS\n");
   return 0;
}
