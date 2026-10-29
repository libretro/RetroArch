/* Regression test for retro_atomic.h's Wii U backend.
 *
 * The Wii U's Espresso needs a cache flush inside a lwarx/stwcx. pair
 * for the reservation to hold across its cores, which no compiler emits
 * and Cafe OS's own atomics carry. The backend therefore sends every
 * read-modify-write to coreinit's OS*Atomic and keeps loads, stores and
 * fences inline. coreinit_stub.c stands in for those exports with
 * counted host atomics, so this checks, at every push:
 *
 *  1. RETRO_ATOMIC_FORCE_WIIU selects the backend, which claims
 *     RETRO_ATOMIC_LOCK_FREE, RETRO_ATOMIC_HAS_CAS and
 *     RETRO_ATOMIC_HAS_PTR, and not RETRO_ATOMIC_HAS_64.
 *  2. Each int read-modify-write makes exactly one coreinit call, the
 *     one that does that operation, and returns the previous value with
 *     the storage left as specified - a failing compare-and-swap leaving
 *     it untouched.
 *  3. Loads, stores and fences make no coreinit call at all.
 *  4. Threads hammering one counter through fetch_add, fetch_sub and a
 *     compare-and-swap loop lose no update.
 *
 * Built for 32-bit PowerPC (CC=powerpc-linux-gnu-gcc, RUNNER=qemu-ppc
 * with -static), size_t and pointers are words and their operations go
 * through coreinit too, which the test then checks the same way; on a
 * 64-bit host they take the builtins and only their results are
 * checked. */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <pthread.h>

#include <retro_atomic.h>

#include "coreinit_stub.h"

#if !defined(RETRO_ATOMIC_BACKEND_WIIU)
#error "retro_atomic.h: RETRO_ATOMIC_FORCE_WIIU did not select the Wii U backend"
#endif
#if !defined(RETRO_ATOMIC_LOCK_FREE)
#error "retro_atomic.h: the Wii U backend must claim RETRO_ATOMIC_LOCK_FREE"
#endif
#if !defined(RETRO_ATOMIC_HAS_CAS) || !defined(RETRO_ATOMIC_HAS_PTR)
#error "retro_atomic.h: the Wii U backend must offer CAS and pointer operations"
#endif
#if defined(RETRO_ATOMIC_HAS_64)
#error "retro_atomic.h: the Wii U backend has no 64-bit operations to offer"
#endif

static int failures;

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
   failures++; } } while (0)

static int snapshot[COREINIT_STUB_COUNT];

static void calls_mark(void)
{
   int i;
   for (i = 0; i < COREINIT_STUB_COUNT; i++)
      snapshot[i] = coreinit_stub_calls[i];
}

/* Exactly one call, to which, since calls_mark(); -1: none at all */
static int calls_since(int which)
{
   int i, total = 0, hit = 0;
   for (i = 0; i < COREINIT_STUB_COUNT; i++)
   {
      int n  = coreinit_stub_calls[i] - snapshot[i];
      total += n;
      if (i == which)
         hit = n;
   }
   return which < 0 ? total == 0 : (total == 1 && hit == 1);
}

static void check_int(void)
{
   retro_atomic_int_t v;
   int old, ok;

   retro_atomic_int_init(&v, 5);

   calls_mark();
   CHECK(retro_atomic_load_acquire_int(&v) == 5);
   CHECK(retro_atomic_load_relaxed_int(&v) == 5);
   CHECK(retro_atomic_load_seq_cst_int(&v) == 5);
   retro_atomic_store_release_int(&v, 7);
   retro_atomic_store_relaxed_int(&v, 8);
   retro_atomic_thread_fence_acquire();
   retro_atomic_thread_fence_release();
   retro_atomic_thread_fence_seq_cst();
   CHECK(calls_since(-1));
   CHECK(v == 8);

   calls_mark();
   old = retro_atomic_fetch_add_int(&v, 3);
   CHECK(old == 8 && v == 11 && calls_since(COREINIT_STUB_ADD));

   calls_mark();
   old = retro_atomic_fetch_sub_int(&v, 4);
   CHECK(old == 11 && v == 7 && calls_since(COREINIT_STUB_ADD));

   calls_mark();
   old = retro_atomic_fetch_add_seq_cst_int(&v, 1);
   CHECK(old == 7 && v == 8 && calls_since(COREINIT_STUB_ADD));

   calls_mark();
   old = retro_atomic_fetch_or_int(&v, 0x30);
   CHECK(old == 8 && v == 0x38 && calls_since(COREINIT_STUB_OR));

   calls_mark();
   old = retro_atomic_fetch_and_int(&v, 0x18);
   CHECK(old == 0x38 && v == 0x18 && calls_since(COREINIT_STUB_AND));

   calls_mark();
   old = retro_atomic_exchange_int(&v, -2);
   CHECK(old == 0x18 && v == -2 && calls_since(COREINIT_STUB_SWAP));

   calls_mark();
   ok = retro_atomic_cas_int(&v, 9, 1);
   CHECK(!ok && v == -2 && calls_since(COREINIT_STUB_CAS));

   calls_mark();
   ok = retro_atomic_cas_int(&v, -2, 1);
   CHECK(ok && v == 1 && calls_since(COREINIT_STUB_CAS));

   /* Subtracting INT_MIN wraps as the API's two's complement says */
   retro_atomic_int_init(&v, 0);
   old = retro_atomic_fetch_sub_int(&v, INT_MIN);
   CHECK(old == 0 && v == INT_MIN);

   calls_mark();
   retro_atomic_inc_int(&v);
   CHECK(calls_since(COREINIT_STUB_ADD));
   calls_mark();
   retro_atomic_dec_int(&v);
   CHECK(v == INT_MIN && calls_since(COREINIT_STUB_ADD));
}

static void check_size_and_ptr(void)
{
   retro_atomic_size_t s;
   retro_atomic_ptr_t  p;
   static int a, b;
   size_t old;
   void  *was;

   retro_atomic_size_init(&s, 40);
   calls_mark();
   CHECK(retro_atomic_load_acquire_size(&s) == 40);
   retro_atomic_store_release_size(&s, 41);
   CHECK(retro_atomic_load_relaxed_size(&s) == 41);
   CHECK(calls_since(-1));

   calls_mark();
   old = retro_atomic_fetch_add_size(&s, 2);
   CHECK(old == 41 && s == 43);
   if (sizeof(size_t) == 4)
      CHECK(calls_since(COREINIT_STUB_ADD));
   calls_mark();
   old = retro_atomic_fetch_sub_size(&s, 3);
   CHECK(old == 43 && s == 40);
   if (sizeof(size_t) == 4)
      CHECK(calls_since(COREINIT_STUB_ADD));

   retro_atomic_ptr_init(&p, &a);
   calls_mark();
   CHECK(retro_atomic_load_acquire_ptr(&p) == (void*)&a);
   retro_atomic_store_release_ptr(&p, &b);
   CHECK(retro_atomic_load_relaxed_ptr(&p) == (void*)&b);
   CHECK(calls_since(-1));

   calls_mark();
   was = retro_atomic_exchange_ptr(&p, &a);
   CHECK(was == (void*)&b && p == (void*)&a);
   if (sizeof(void*) == 4)
      CHECK(calls_since(COREINIT_STUB_SWAP));

   calls_mark();
   CHECK(!retro_atomic_cas_ptr(&p, &b, NULL) && p == (void*)&a);
   CHECK(retro_atomic_cas_ptr(&p, &a, &b) && p == (void*)&b);
   if (sizeof(void*) == 4)
   {
      CHECK(coreinit_stub_calls[COREINIT_STUB_CAS]
            - snapshot[COREINIT_STUB_CAS] == 2);
   }
}

#define THREADS    4
#define ITERATIONS 20000

static retro_atomic_int_t counter;
static retro_atomic_int_t stuck;

static void *hammer(void *arg)
{
   int i;
   (void)arg;
   for (i = 0; i < ITERATIONS; i++)
   {
      int seen, tries = 0;
      retro_atomic_fetch_add_int(&counter, 3);
      retro_atomic_fetch_sub_int(&counter, 1);
      /* A compare-and-swap that never reports success would spin here
       * for ever; give up and say so instead. */
      do
      {
         seen = retro_atomic_load_acquire_int(&counter);
         if (++tries > 1000000)
         {
            retro_atomic_inc_int(&stuck);
            return NULL;
         }
      } while (!retro_atomic_cas_int(&counter, seen, seen + 1));
   }
   return NULL;
}

static void check_threads(void)
{
   pthread_t t[THREADS];
   int i;

   retro_atomic_int_init(&counter, 0);
   retro_atomic_int_init(&stuck, 0);
   for (i = 0; i < THREADS; i++)
      CHECK(pthread_create(&t[i], NULL, hammer, NULL) == 0);
   for (i = 0; i < THREADS; i++)
      pthread_join(t[i], NULL);
   CHECK(retro_atomic_load_acquire_int(&stuck) == 0);
   CHECK(retro_atomic_load_acquire_int(&counter) == THREADS * ITERATIONS * 3);
}

int main(void)
{
   printf("backend: %s, size_t %u bytes, pointers %u\n",
         RETRO_ATOMIC_BACKEND_NAME, (unsigned)sizeof(size_t),
         (unsigned)sizeof(void*));
   check_int();
   check_size_and_ptr();
   check_threads();

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("retro_atomic_wiiu_backend: ok\n");
   return 0;
}
