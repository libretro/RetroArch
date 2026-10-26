/* wiiu/system/newlib_locks.c against stand-in Cafe OS mutexes: newlib's
 * lock word starts at 0, several threads race to create its mutex,
 * exactly one mutex is published and kept, and it excludes. The
 * recursive, try and close paths are checked on one thread and
 * across two.
 *
 * newlib's lock word is an int holding the mutex address, as on the
 * Wii U's 32-bit address space, so the stand-in allocator hands out
 * slots from a static pool and the executable is linked at a low
 * fixed base (see the Makefile) to keep those addresses below 2^31. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#include <wiiu/os/mutex.h>

#define POOL_SLOTS 64
#define THREADS    8
#define ROUNDS     20000

struct slot
{
   OSMutex         os;   /* first: the pointer the code under test sees */
   pthread_mutex_t pm;
   int             live;
};

static struct slot pool[POOL_SLOTS];
static int pool_next;
static int pool_live;
static int failures;

#define CHECK(cond) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

static void *test_malloc(size_t size)
{
   int i;
   if (size != sizeof(OSMutex))
   {
      fprintf(stderr, "FAIL unexpected allocation of %u bytes\n",
            (unsigned)size);
      exit(1);
   }
   i = __atomic_fetch_add(&pool_next, 1, __ATOMIC_ACQ_REL);
   if (i >= POOL_SLOTS)
      return NULL;
   if ((uintptr_t)(unsigned)(uintptr_t)&pool[i].os != (uintptr_t)&pool[i].os
         || (uintptr_t)&pool[i].os > 0x7fffffffu)
   {
      fprintf(stderr, "FAIL pool above 2^31: link at a low base\n");
      exit(1);
   }
   return &pool[i].os;
}

static void test_free(void *p)
{
   struct slot *s = (struct slot*)p;
   if (!p)
      return;
   if (s->live)
   {
      pthread_mutex_destroy(&s->pm);
      s->live = 0;
      __atomic_fetch_sub(&pool_live, 1, __ATOMIC_ACQ_REL);
   }
}

/* Stand-ins for coreinit. OSMutex is recursive. */
void OSInitMutex(OSMutex *mutex)
{
   struct slot *s = (struct slot*)mutex;
   pthread_mutexattr_t attr;
   pthread_mutexattr_init(&attr);
   pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
   pthread_mutex_init(&s->pm, &attr);
   pthread_mutexattr_destroy(&attr);
   s->live = 1;
   __atomic_fetch_add(&pool_live, 1, __ATOMIC_ACQ_REL);
}

void OSLockMutex(OSMutex *mutex)
{
   pthread_mutex_lock(&((struct slot*)mutex)->pm);
}

BOOL OSTryLockMutex(OSMutex *mutex)
{
   return pthread_mutex_trylock(&((struct slot*)mutex)->pm) == 0;
}

void OSUnlockMutex(OSMutex *mutex)
{
   pthread_mutex_unlock(&((struct slot*)mutex)->pm);
}

#define malloc test_malloc
#define free   test_free
#include "../../../wiiu/system/newlib_locks.c"
#undef malloc
#undef free

static int word;            /* a newlib static lock: starts at 0 */
static unsigned counter;    /* guarded by word */
static pthread_barrier_t start;

static void *hammer(void *arg)
{
   int i;
   (void)arg;
   pthread_barrier_wait(&start);
   for (i = 0; i < ROUNDS; i++)
   {
      unsigned v;
      __syscall_lock_acquire_recursive(&word);
      v       = counter;
      counter = v + 1;
      __syscall_lock_release_recursive(&word);
   }
   return NULL;
}

static int try_result;

static void *try_other(void *arg)
{
   try_result = __syscall_lock_try_acquire_recursive((int*)arg);
   if (try_result == 0)
      __syscall_lock_release_recursive((int*)arg);
   return NULL;
}

static int try_from_other_thread(int *lock)
{
   pthread_t t;
   pthread_create(&t, NULL, try_other, lock);
   pthread_join(t, NULL);
   return try_result;
}

int main(void)
{
   pthread_t t[THREADS];
   int i;
   int rec = 0;

   /* Racing first use: one mutex published, the losers' freed. */
   pthread_barrier_init(&start, NULL, THREADS);
   for (i = 0; i < THREADS; i++)
      pthread_create(&t[i], NULL, hammer, NULL);
   for (i = 0; i < THREADS; i++)
      pthread_join(t[i], NULL);
   pthread_barrier_destroy(&start);
   CHECK(counter == (unsigned)THREADS * ROUNDS);
   CHECK(word != 0);
   CHECK(pool_live == 1);

   /* Recursive on one thread, excluded from another. */
   __syscall_lock_init_recursive(&rec);
   CHECK(rec == 0);
   __syscall_lock_acquire_recursive(&rec);
   CHECK(__syscall_lock_try_acquire_recursive(&rec) == 0);
   CHECK(try_from_other_thread(&rec) != 0);
   __syscall_lock_release_recursive(&rec);
   CHECK(try_from_other_thread(&rec) != 0);
   __syscall_lock_release_recursive(&rec);
   CHECK(try_from_other_thread(&rec) == 0);

   /* Close frees the mutex and returns the word to its initial state;
    * the next use creates a new one. */
   __syscall_lock_close_recursive(&rec);
   CHECK(rec == 0);
   CHECK(pool_live == 1);
   __syscall_lock_acquire(&rec);
   CHECK(rec != 0);
   CHECK(pool_live == 2);
   __syscall_lock_release(&rec);
   __syscall_lock_close(&rec);
   __syscall_lock_close(&word);
   CHECK(pool_live == 0);

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("wiiu_newlib_locks: ok\n");
   return 0;
}
