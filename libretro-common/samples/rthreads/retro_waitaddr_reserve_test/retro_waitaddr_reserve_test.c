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


/* retro_waitaddr_reserve(): once the records for N waiters are made, N
 * threads waiting at once never allocate - a wait reached from a
 * signal handler must not call calloc or malloc.  The allocator is
 * counted with the linker's --wrap: any allocation the waits make
 * after the reservation fails the test.  Each thread's search for a
 * record starts at a place its stack decides, so the threads start in
 * chunks that were never made. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <retro_atomic.h>
#include <rthreads/retro_waitaddr.h>

#define THREADS 32
#define ROUNDS  200

static int failures;
static retro_atomic_int_t g_allocs;
static retro_atomic_int_t g_word[THREADS];
static retro_atomic_int_t g_seen[THREADS];

#define CHECK(cond) do { if (!(cond)) { \
   fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

void *__real_calloc(size_t n, size_t size);
void *__real_malloc(size_t size);

void *__wrap_calloc(size_t n, size_t size)
{
   retro_atomic_inc_int(&g_allocs);
   return __real_calloc(n, size);
}

void *__wrap_malloc(size_t size)
{
   retro_atomic_inc_int(&g_allocs);
   return __real_malloc(size);
}

static void *waiter(void *arg)
{
   const int me = (int)(size_t)arg;
   int round;

   for (round = 0; round < ROUNDS; round++)
   {
      /* Until the main thread moves this thread's word past round */
      while (retro_atomic_load_acquire_int(&g_word[me]) == round)
      {
         retro_waitaddr_t w;
         w.addr = &g_word[me];
         w.old  = (uint32_t)round;
         retro_waitaddr_wait(&w, 1, -1);
      }
      retro_atomic_store_release_int(&g_seen[me], round + 1);
      retro_waitaddr_notify_all(&g_seen[me]);
   }

   return NULL;
}

int main(void)
{
   pthread_t t[THREADS];
   int i, round, before;

   alarm(120);

   CHECK(!retro_waitaddr_reserve(64 * 64 + 1));
   CHECK(retro_waitaddr_reserve(THREADS + 1));
   before = retro_atomic_load_acquire_int(&g_allocs);

   for (i = 0; i < THREADS; i++)
      pthread_create(&t[i], NULL, waiter, (void*)(size_t)i);

   for (round = 0; round < ROUNDS; round++)
   {
      for (i = 0; i < THREADS; i++)
      {
         retro_atomic_store_release_int(&g_word[i], round + 1);
         retro_waitaddr_notify_all(&g_word[i]);
      }

      /* The main thread waits too: its record is one of the reserved */
      for (i = 0; i < THREADS; i++)
      {
         int seen;
         while ((seen = retro_atomic_load_acquire_int(&g_seen[i])) < round + 1)
         {
            retro_waitaddr_t w;
            w.addr = &g_seen[i];
            w.old  = (uint32_t)seen;
            retro_waitaddr_wait(&w, 1, -1);
         }
      }
   }

   for (i = 0; i < THREADS; i++)
      pthread_join(t[i], NULL);

   CHECK(retro_atomic_load_acquire_int(&g_allocs) == before);
   if (retro_atomic_load_acquire_int(&g_allocs) != before)
      fprintf(stderr, "allocations during the waits: %d\n",
            retro_atomic_load_acquire_int(&g_allocs) - before);

   printf("retro_waitaddr_reserve_test: %d threads, %d rounds, %d allocations after the reservation: %s\n",
         THREADS, ROUNDS, retro_atomic_load_acquire_int(&g_allocs) - before, failures ? "FAIL" : "ok");
   return failures ? 1 : 0;
}
