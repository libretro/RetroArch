/* The chain runner in frontend/thread_elevation.c against a scripted
 * backend list. What it pins down: backends are tried in list order
 * and the first grant ends the chain; a brokered backend is handed the
 * calling thread's id and where the chain resumes; a PENDING answer
 * returns to the caller at once and names the backend; a pending
 * request refused on the backend's own thread carries on with the
 * brokered backends after it, for the same thread, and never goes back
 * to a self-acting one. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <sys/syscall.h>

#include <boolean.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "frontend/thread_elevation.h"

extern long syscall(long number, ...);

static int failures;

/* What each call saw, in order. */
static struct
{
   char     who[16][16];
   uint64_t tid[16];
   unsigned next[16];
   unsigned n;
} calls;

static slock_t *calls_lock;

static void record(const char *who, uint64_t tid, unsigned next)
{
   slock_lock(calls_lock);
   if (calls.n < 16)
   {
      strncpy(calls.who[calls.n], who, 15);
      calls.who[calls.n][15] = '\0';
      calls.tid[calls.n]     = tid;
      calls.next[calls.n]    = next;
      calls.n++;
   }
   slock_unlock(calls_lock);
}

/* How each scripted backend answers, set per case. */
static enum thread_elevation_result self_answer;
static enum thread_elevation_result grant_answer;
static enum thread_elevation_result misplaced_answer;
static int                          slow_goes_async;

static enum thread_elevation_result self_raise(uint64_t tid, unsigned next)
{
   record("self", tid, next);
   return self_answer;
}

/* Brokered and asynchronous: answers PENDING, then is refused on a
 * thread of its own and hands the chain on. */
static sthread_t *slow_thread;
static uint64_t   slow_tid;
static unsigned   slow_next;

static void slow_worker(void *data)
{
   (void)data;
   usleep(20000);
   thread_elevation_continue(slow_tid, slow_next);
}

static enum thread_elevation_result slow_raise(uint64_t tid, unsigned next)
{
   record("slow", tid, next);
   if (!slow_goes_async)
      return THREAD_ELEVATION_REFUSED;
   slow_tid    = tid;
   slow_next   = next;
   slow_thread = sthread_create(slow_worker, NULL);
   return slow_thread ? THREAD_ELEVATION_PENDING : THREAD_ELEVATION_REFUSED;
}

/* A self-acting backend placed after a brokered one, which the ordering
 * rule forbids. On the caller's own thread it is tried like any other;
 * continue() must never call it, since it would change the wrong
 * thread - and it would grant if asked there. */
static enum thread_elevation_result misplaced_raise(uint64_t tid,
      unsigned next)
{
   record("misplaced", tid, next);
   return misplaced_answer;
}

static enum thread_elevation_result grant_raise(uint64_t tid, unsigned next)
{
   record("grant", tid, next);
   return grant_answer;
}

/* Additive: a hint that goes on top of whatever the chain grants. */
static enum thread_elevation_result hint_answer;
static enum thread_elevation_result hint_raise(uint64_t tid, unsigned next)
{
   record("hint", tid, next);
   return hint_answer;
}

static const thread_elevation_backend_t be_self =
   { self_raise, "self", false, false };
static const thread_elevation_backend_t be_slow =
   { slow_raise, "slow", true, false };
static const thread_elevation_backend_t be_misplaced =
   { misplaced_raise, "misplaced", false, false };
static const thread_elevation_backend_t be_grant =
   { grant_raise, "grant", true, false };
static const thread_elevation_backend_t be_hint =
   { hint_raise, "hint", false, true };

/* The additive one last in the list: the runner tries it first anyway */
const thread_elevation_backend_t *thread_elevation_backends[] = {
   &be_self, &be_slow, &be_misplaced, &be_grant, &be_hint, NULL
};

/* The real EEVDF backend, on whatever kernel this runs on */
extern const thread_elevation_backend_t thread_elevation_eevdf;

static void reset(void)
{
   memset(&calls, 0, sizeof(calls));
   slow_thread = NULL;
}

static void expect_calls(const char *name, const char *const *want,
      unsigned n)
{
   unsigned i;
   if (calls.n != n)
   {
      printf("   FAIL %s: %u calls, expected %u\n", name, calls.n, n);
      failures++;
      return;
   }
   for (i = 0; i < n; i++)
      if (strcmp(calls.who[i], want[i]))
      {
         printf("   FAIL %s: call %u was %s, expected %s\n",
               name, i, calls.who[i], want[i]);
         failures++;
         return;
      }
   printf("   ok   %s\n", name);
}

static void check(const char *name, int ok)
{
   if (ok)
      printf("   ok   %s\n", name);
   else
   {
      printf("   FAIL %s\n", name);
      failures++;
   }
}

struct eevdf_attr
{
   uint32_t size, policy;
   uint64_t flags;
   int32_t  nice;
   uint32_t prio;
   uint64_t runtime, deadline, period;
};

static int eevdf_result;
static uint64_t eevdf_runtime;
static int eevdf_rt_set;
static uint32_t eevdf_rt_policy;
static int eevdf_rt_result;

/* Made real time first, where this runner may: the slice must not
 * touch it. */
static void eevdf_rt_worker(void *data)
{
   struct eevdf_attr a;
   struct sched_param sp;
   (void)data;
   memset(&sp, 0, sizeof(sp));
   sp.sched_priority = 1;
   if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp) != 0)
      return;
   eevdf_rt_set    = 1;
   eevdf_rt_result = thread_elevation_eevdf.raise(0, 0);
   memset(&a, 0, sizeof(a));
   syscall(SYS_sched_getattr, 0, &a, (unsigned)sizeof(a), 0);
   eevdf_rt_policy = a.policy;
}
static void eevdf_worker(void *data)
{
   struct eevdf_attr a;
   (void)data;
   eevdf_result = thread_elevation_eevdf.raise(0, 0);
   memset(&a, 0, sizeof(a));
   syscall(SYS_sched_getattr, 0, &a, (unsigned)sizeof(a), 0);
   eevdf_runtime = a.runtime;
}

int main(void)
{
   const char *via;
   const char *added = NULL;
   enum thread_elevation_result r;
   uint64_t me = (uint64_t)syscall(SYS_gettid);

   calls_lock       = slock_new();
   misplaced_answer = THREAD_ELEVATION_REFUSED;
   hint_answer      = THREAD_ELEVATION_REFUSED;

   printf("1. the first grant ends the chain\n");
   {
      static const char *const want[] = { "hint", "self" };
      reset(); self_answer = THREAD_ELEVATION_GRANTED;
      via = NULL;
      r   = thread_elevation_raise_current(&via, &added);
      check("granted", r == THREAD_ELEVATION_GRANTED && !via);
      expect_calls("nothing after it tried", want, 2);
   }

   printf("2. everything refuses synchronously\n");
   {
      static const char *const want[] = { "hint", "self", "slow", "misplaced", "grant" };
      reset(); self_answer = THREAD_ELEVATION_REFUSED;
      slow_goes_async = 0; grant_answer = THREAD_ELEVATION_REFUSED;
      r = thread_elevation_raise_current(NULL, NULL);
      check("refused", r == THREAD_ELEVATION_REFUSED);
      expect_calls("every backend, in order", want, 5);
      check("brokered backends got the caller's thread id",
            calls.tid[2] == me && calls.tid[4] == me);
      check("each told where the chain resumes",
            calls.next[2] == 2 && calls.next[4] == 4);
   }

   printf("3. pending, then refused on its own thread\n");
   {
      static const char *const want[] = { "hint", "self", "slow", "grant" };
      reset(); self_answer = THREAD_ELEVATION_REFUSED;
      slow_goes_async = 1; grant_answer = THREAD_ELEVATION_GRANTED;
      misplaced_answer = THREAD_ELEVATION_GRANTED;
      via = NULL;
      r   = thread_elevation_raise_current(&via, &added);
      check("caller told pending, by name",
            r == THREAD_ELEVATION_PENDING && via && !strcmp(via, "slow"));
      if (slow_thread)
         sthread_join(slow_thread); /* the harness waits; the caller never does */
      expect_calls("chain carried on, skipping the self-acting one", want, 4);
      check("for the same thread", calls.tid[3] == me);
   }

   printf("4. pending, refused, nothing left that grants\n");
   {
      static const char *const want[] = { "hint", "self", "slow", "grant" };
      reset(); self_answer = THREAD_ELEVATION_REFUSED;
      slow_goes_async = 1; grant_answer = THREAD_ELEVATION_REFUSED;
      misplaced_answer = THREAD_ELEVATION_GRANTED;
      r = thread_elevation_raise_current(NULL, NULL);
      check("pending", r == THREAD_ELEVATION_PENDING);
      if (slow_thread)
         sthread_join(slow_thread);
      expect_calls("ran out quietly", want, 4);
   }

   printf("5. an additive backend goes on top of the chain\n");
   {
      static const char *const want[] = { "hint", "self" };
      reset(); hint_answer = THREAD_ELEVATION_GRANTED;
      self_answer = THREAD_ELEVATION_GRANTED;
      added = NULL;
      r     = thread_elevation_raise_current(&via, &added);
      check("the chain still granted after it", r == THREAD_ELEVATION_GRANTED);
      check("and it was reported by name", added && !strcmp(added, "hint"));
      expect_calls("tried first, and the chain went on", want, 2);
      reset(); self_answer = THREAD_ELEVATION_REFUSED;
      slow_goes_async = 1; grant_answer = THREAD_ELEVATION_GRANTED;
      misplaced_answer = THREAD_ELEVATION_REFUSED;
      r = thread_elevation_raise_current(NULL, NULL);
      if (slow_thread)
         sthread_join(slow_thread);
      {
         unsigned k, hints = 0;
         for (k = 0; k < calls.n; k++)
            hints += !strcmp(calls.who[k], "hint");
         check("never reached by a continuation", hints == 1);
      }
      hint_answer = THREAD_ELEVATION_REFUSED;
   }

   printf("6. the EEVDF slice on this kernel\n");
   {
      sthread_t *t = sthread_create(eevdf_worker, NULL);
      sthread_join(t);
      if (eevdf_result == THREAD_ELEVATION_GRANTED)
         check("granted: the slice reads back as 100 us", eevdf_runtime == 100000);
      else
         check("refused: the thread keeps the slice it had", eevdf_runtime != 100000);
      printf("        (%s, slice now %llu ns)\n",
            eevdf_result == THREAD_ELEVATION_GRANTED ? "granted" : "refused",
            (unsigned long long)eevdf_runtime);
      t = sthread_create(eevdf_rt_worker, NULL);
      sthread_join(t);
      if (eevdf_rt_set)
         check("a real-time thread is left real time",
               eevdf_rt_result == THREAD_ELEVATION_REFUSED
               && eevdf_rt_policy == SCHED_RR);
      else
         printf("   --   no real time for this runner; that case is skipped\n");
   }

   slock_free(calls_lock);
   if (failures)
   {
      printf("thread elevation: %d failure(s)\n", failures);
      return 1;
   }
   printf("thread elevation: chain order and continuation hold\n");
   return 0;
}
