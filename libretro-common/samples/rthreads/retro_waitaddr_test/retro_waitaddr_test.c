/* retro_waitaddr: parking on the value of words, waking by address.
 *
 *  - a wait on a word that already differs returns at once; one whose
 *    bound expires says so, and not before the bound;
 *  - a notify with nobody waiting does nothing (and its cost is
 *    printed);
 *  - a waiter is woken by a notify on its address, and on any one of
 *    several addresses it waits on;
 *  - notify_one releases one waiter of several on a word, notify_all
 *    the rest;
 *  - tokens: threads take tokens out of a counter, sleeping on it when
 *    it is empty, while others add tokens one at a time with a
 *    notify_one each.  Every token is taken and every taker gets home:
 *    a wake lost anywhere leaves a taker asleep and the test hung
 *    (the alarm ends it).  Sixteen counters, so buckets hold waiters
 *    on several addresses.
 *
 * Built a second time with ThreadSanitizer in CI.
 *
 * Usage: retro_waitaddr_test */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>

#include <retro_atomic.h>
#include <rthreads/retro_waitaddr.h>

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
   fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

static double now_ms(void)
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static void wait_word(retro_atomic_int_t *word, int seen)
{
   retro_waitaddr_t w;
   w.addr = word;
   w.old  = (uint32_t)seen;
   retro_waitaddr_wait(&w, 1, -1);
}

/* --- one thread ------------------------------------------------------- */

static void alone(void)
{
   retro_atomic_int_t word;
   retro_waitaddr_t w;
   double a, b;
   int i;

   retro_atomic_int_init(&word, 5);
   w.addr = &word;

   /* Already different */
   w.old = 4;
   a = now_ms();
   CHECK(retro_waitaddr_wait(&w, 1, -1));
   CHECK(retro_waitaddr_wait(&w, 1, 2000000));
   CHECK(now_ms() - a < 500);

   /* The bound */
   w.old = 5;
   a = now_ms();
   CHECK(!retro_waitaddr_wait(&w, 1, 30000));
   b = now_ms();
   CHECK(b - a >= 29 && b - a < 2000);

   /* Nobody waiting */
   a = now_ms();
   for (i = 0; i < 2000000; i++)
   {
      retro_atomic_fetch_add_int(&word, 1);
      retro_waitaddr_notify_one(&word);
   }
   b = now_ms();
   printf("retro_waitaddr_test: a store and a notify with nobody waiting: %.1f ns\n",
         (b - a) * 1e6 / 2000000);
}

/* --- waiters and notifiers -------------------------------------------- */

#define SLEEPERS 6

static retro_atomic_int_t gate;        /* what the sleepers wait on */
static retro_atomic_int_t others[3];
static retro_atomic_int_t awake;
static retro_atomic_int_t parked;

static void *sleeper(void *arg)
{
   (void)arg;
   retro_atomic_fetch_add_int(&parked, 1);
   while (retro_atomic_load_acquire_int(&gate) == 0)
      wait_word(&gate, 0);
   retro_atomic_fetch_sub_int(&gate, 1);
   retro_atomic_fetch_add_int(&awake, 1);
   return NULL;
}

static void *multi_sleeper(void *arg)
{
   retro_waitaddr_t w[3];
   int i;
   (void)arg;
   for (;;)
   {
      int sum = 0;
      for (i = 0; i < 3; i++)
      {
         w[i].addr = &others[i];
         w[i].old  = 0;
         sum += retro_atomic_load_acquire_int(&others[i]);
      }
      if (sum)
         break;
      retro_atomic_store_release_int(&parked, 1);
      retro_waitaddr_wait(w, 3, -1);
   }
   retro_atomic_fetch_add_int(&awake, 1);
   return NULL;
}

/* Until n, or a second has passed */
static int settles_at(retro_atomic_int_t *v, int n)
{
   double end = now_ms() + 1000;
   while (now_ms() < end)
   {
      if (retro_atomic_load_acquire_int(v) == n)
         return 1;
      sched_yield();
   }
   return retro_atomic_load_acquire_int(v) == n;
}

static void nap_ms(int ms)
{
   struct timespec t;
   t.tv_sec  = 0;
   t.tv_nsec = ms * 1000000L;
   nanosleep(&t, NULL);
}

static void wakes(void)
{
   pthread_t t[SLEEPERS], m;
   int i, which;

   /* Several addresses: woken through each in turn */
   for (which = 0; which < 3; which++)
   {
      retro_atomic_store_release_int(&awake, 0);
      retro_atomic_store_release_int(&parked, 0);
      for (i = 0; i < 3; i++)
         retro_atomic_store_release_int(&others[i], 0);
      pthread_create(&m, NULL, multi_sleeper, NULL);
      CHECK(settles_at(&parked, 1));
      nap_ms(20);
      CHECK(retro_atomic_load_acquire_int(&awake) == 0);
      retro_atomic_store_release_int(&others[which], 1);
      retro_waitaddr_notify_one(&others[which]);
      pthread_join(m, NULL);
      CHECK(retro_atomic_load_acquire_int(&awake) == 1);
   }

   /* One of several, then the rest. A sleeper takes one unit of the
    * gate when it wakes, so with one unit put in exactly one can get
    * home however many are woken - and notify_one must get one home. */
   retro_atomic_store_release_int(&awake, 0);
   retro_atomic_store_release_int(&parked, 0);
   retro_atomic_store_release_int(&gate, 0);
   for (i = 0; i < SLEEPERS; i++)
      pthread_create(&t[i], NULL, sleeper, NULL);
   CHECK(settles_at(&parked, SLEEPERS));
   nap_ms(20);
   CHECK(retro_atomic_load_acquire_int(&awake) == 0);

   for (i = 1; i <= SLEEPERS - 2; i++)
   {
      retro_atomic_fetch_add_int(&gate, 1);
      retro_waitaddr_notify_one(&gate);
      CHECK(settles_at(&awake, i));
   }

   retro_atomic_fetch_add_int(&gate, 2);
   retro_waitaddr_notify_all(&gate);
   for (i = 0; i < SLEEPERS; i++)
      pthread_join(t[i], NULL);
   CHECK(retro_atomic_load_acquire_int(&awake) == SLEEPERS);
}

/* --- tokens ----------------------------------------------------------- */

#define COUNTERS  16
#define TAKERS    3   /* a counter */
#define GIVERS    2   /* a counter */
#define TOKENS    20000 /* a giver */

static retro_atomic_int_t tokens[COUNTERS];
static retro_atomic_int_t taken[COUNTERS];

static void *taker(void *arg)
{
   int c = (int)(size_t)arg;
   for (;;)
   {
      int v = retro_atomic_load_acquire_int(&tokens[c]);
      if (v > 0)
      {
         if (retro_atomic_cas_int(&tokens[c], v, v - 1))
            retro_atomic_fetch_add_int(&taken[c], 1);
         continue;
      }
      /* The end is told through the word waited on: anything else
       * could change between the look at it and the sleep */
      if (v < 0)
         return NULL;
      wait_word(&tokens[c], 0);
   }
}

static void *giver(void *arg)
{
   int c = (int)(size_t)arg;
   int i;
   for (i = 0; i < TOKENS; i++)
   {
      retro_atomic_fetch_add_int(&tokens[c], 1);
      retro_waitaddr_notify_one(&tokens[c]);
   }
   return NULL;
}

static void token_run(void)
{
   pthread_t take[COUNTERS][TAKERS], give[COUNTERS][GIVERS];
   int c, i, short_of = 0;

   for (c = 0; c < COUNTERS; c++)
      for (i = 0; i < TAKERS; i++)
         pthread_create(&take[c][i], NULL, taker, (void*)(size_t)c);
   for (c = 0; c < COUNTERS; c++)
      for (i = 0; i < GIVERS; i++)
         pthread_create(&give[c][i], NULL, giver, (void*)(size_t)c);
   for (c = 0; c < COUNTERS; c++)
      for (i = 0; i < GIVERS; i++)
         pthread_join(give[c][i], NULL);

   /* Every token taken, with no more notifies than the givers made */
   for (c = 0; c < COUNTERS; c++)
      short_of += !settles_at(&taken[c], GIVERS * TOKENS);
   CHECK(short_of == 0);

   for (c = 0; c < COUNTERS; c++)
   {
      retro_atomic_store_release_int(&tokens[c], -1);
      retro_waitaddr_notify_all(&tokens[c]);
   }
   for (c = 0; c < COUNTERS; c++)
      for (i = 0; i < TAKERS; i++)
         pthread_join(take[c][i], NULL);
}

int main(void)
{
   alarm(60);
   alone();
   wakes();
   token_run();
   retro_waitaddr_free_all();

   /* And again after everything was let go of */
   wakes();
   retro_waitaddr_free_all();

   printf("retro_waitaddr_test: %s\n", failures ? "FAIL" : "ok");
   return failures ? 1 : 0;
}
