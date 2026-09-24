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

static const thread_elevation_backend_t be_self =
   { self_raise, "self", false };
static const thread_elevation_backend_t be_slow =
   { slow_raise, "slow", true };
static const thread_elevation_backend_t be_misplaced =
   { misplaced_raise, "misplaced", false };
static const thread_elevation_backend_t be_grant =
   { grant_raise, "grant", true };

const thread_elevation_backend_t *thread_elevation_backends[] = {
   &be_self, &be_slow, &be_misplaced, &be_grant, NULL
};

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

int main(void)
{
   const char *via;
   enum thread_elevation_result r;
   uint64_t me = (uint64_t)syscall(SYS_gettid);

   calls_lock       = slock_new();
   misplaced_answer = THREAD_ELEVATION_REFUSED;

   printf("1. the first grant ends the chain\n");
   {
      static const char *const want[] = { "self" };
      reset(); self_answer = THREAD_ELEVATION_GRANTED;
      via = NULL;
      r   = thread_elevation_raise_current(&via);
      check("granted", r == THREAD_ELEVATION_GRANTED && !via);
      expect_calls("nothing after it tried", want, 1);
   }

   printf("2. everything refuses synchronously\n");
   {
      static const char *const want[] = { "self", "slow", "misplaced", "grant" };
      reset(); self_answer = THREAD_ELEVATION_REFUSED;
      slow_goes_async = 0; grant_answer = THREAD_ELEVATION_REFUSED;
      r = thread_elevation_raise_current(NULL);
      check("refused", r == THREAD_ELEVATION_REFUSED);
      expect_calls("every backend, in order", want, 4);
      check("brokered backends got the caller's thread id",
            calls.tid[1] == me && calls.tid[3] == me);
      check("each told where the chain resumes",
            calls.next[1] == 2 && calls.next[3] == 4);
   }

   printf("3. pending, then refused on its own thread\n");
   {
      static const char *const want[] = { "self", "slow", "grant" };
      reset(); self_answer = THREAD_ELEVATION_REFUSED;
      slow_goes_async = 1; grant_answer = THREAD_ELEVATION_GRANTED;
      misplaced_answer = THREAD_ELEVATION_GRANTED;
      via = NULL;
      r   = thread_elevation_raise_current(&via);
      check("caller told pending, by name",
            r == THREAD_ELEVATION_PENDING && via && !strcmp(via, "slow"));
      if (slow_thread)
         sthread_join(slow_thread); /* the harness waits; the caller never does */
      expect_calls("chain carried on, skipping the self-acting one", want, 3);
      check("for the same thread", calls.tid[2] == me);
   }

   printf("4. pending, refused, nothing left that grants\n");
   {
      static const char *const want[] = { "self", "slow", "grant" };
      reset(); self_answer = THREAD_ELEVATION_REFUSED;
      slow_goes_async = 1; grant_answer = THREAD_ELEVATION_REFUSED;
      misplaced_answer = THREAD_ELEVATION_GRANTED;
      r = thread_elevation_raise_current(NULL);
      check("pending", r == THREAD_ELEVATION_PENDING);
      if (slow_thread)
         sthread_join(slow_thread);
      expect_calls("ran out quietly", want, 3);
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
