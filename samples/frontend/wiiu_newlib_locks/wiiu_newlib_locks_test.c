/* os/wiiu/system/newlib_locks.c against stand-in Cafe OS mutexes: newlib's
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
#include "../../../os/wiiu/system/newlib_locks.c"
#undef malloc
#undef free

/* The hooks as the checks call them: newlib 4.3's set, or the older
 * one (the Makefile builds both), where the recursive locks are the
 * plain ones with init told the lock is recursive. */
#if defined(__NEWLIB__) && (__NEWLIB__ < 4 \
      || (__NEWLIB__ == 4 && __NEWLIB_MINOR__ < 3))
#define T_INIT(l)        (void)__syscall_lock_init((l), 0)
#define T_INIT_REC(l)    (void)__syscall_lock_init((l), 1)
#define T_ACQUIRE(l)     (void)__syscall_lock_acquire(l)
#define T_ACQUIRE_REC(l) (void)__syscall_lock_acquire(l)
#define T_TRY_REC(l)     __syscall_lock_try_acquire(l)
#define T_RELEASE(l)     (void)__syscall_lock_release(l)
#define T_RELEASE_REC(l) (void)__syscall_lock_release(l)
#define T_CLOSE(l)       (void)__syscall_lock_close(l)
#define T_CLOSE_REC(l)   (void)__syscall_lock_close(l)
#else
#define T_INIT(l)        __syscall_lock_init(l)
#define T_INIT_REC(l)    __syscall_lock_init_recursive(l)
#define T_ACQUIRE(l)     __syscall_lock_acquire(l)
#define T_ACQUIRE_REC(l) __syscall_lock_acquire_recursive(l)
#define T_TRY_REC(l)     __syscall_lock_try_acquire_recursive(l)
#define T_RELEASE(l)     __syscall_lock_release(l)
#define T_RELEASE_REC(l) __syscall_lock_release_recursive(l)
#define T_CLOSE(l)       __syscall_lock_close(l)
#define T_CLOSE_REC(l)   __syscall_lock_close_recursive(l)
#endif

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
      T_ACQUIRE_REC(&word);
      v       = counter;
      counter = v + 1;
      T_RELEASE_REC(&word);
   }
   return NULL;
}

static int try_result;

static void *try_other(void *arg)
{
   try_result = T_TRY_REC((int*)arg);
   if (try_result == 0)
      T_RELEASE_REC((int*)arg);
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
   T_INIT_REC(&rec);
   CHECK(rec == 0);
   T_ACQUIRE_REC(&rec);
   CHECK(T_TRY_REC(&rec) == 0);
   CHECK(try_from_other_thread(&rec) != 0);
   T_RELEASE_REC(&rec);
   CHECK(try_from_other_thread(&rec) != 0);
   T_RELEASE_REC(&rec);
   CHECK(try_from_other_thread(&rec) == 0);

   /* Close frees the mutex and returns the word to its initial state;
    * the next use creates a new one. */
   T_CLOSE_REC(&rec);
   CHECK(rec == 0);
   CHECK(pool_live == 1);
   T_ACQUIRE(&rec);
   CHECK(rec != 0);
   CHECK(pool_live == 2);
   T_RELEASE(&rec);
   T_CLOSE(&rec);
   T_CLOSE(&word);
   CHECK(pool_live == 0);

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("wiiu_newlib_locks: ok\n");
   return 0;
}
