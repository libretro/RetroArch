/* Threads and synchronisation.
 *
 * One processor, strict priorities (higher runs first), round robin
 * between equals.  Every blocking primitive is built on gk_futex_*:
 * a thread sleeps on a 32-bit word and is woken by address, so a lock
 * is a plain zero-initialised word that needs no setup or teardown. */

#ifndef GEKKO_THREAD_H
#define GEKKO_THREAD_H

#include <gekko/gekko.h>

#define GK_PRIO_IDLE    0
#define GK_PRIO_MIN     1
#define GK_PRIO_DEFAULT 64
#define GK_PRIO_MAX     127

#define GK_WAIT_FOREVER ((uint64_t)-1)

/* Results of a wait. */
#define GK_OK        0
#define GK_ETIMEDOUT 1
#define GK_EAGAIN    2

typedef struct gk_thread gk_thread_t;
typedef void *(*gk_thread_fn)(void *arg);

/* stack may be NULL to allocate one.  Returns NULL on failure. */
gk_thread_t *gk_thread_create(gk_thread_fn fn, void *arg,
      void *stack, size_t stack_size, int prio);
void        *gk_thread_join(gk_thread_t *t);
void         gk_thread_detach(gk_thread_t *t);
void         gk_thread_exit(void *ret);
gk_thread_t *gk_thread_self(void);
void         gk_thread_yield(void);
void         gk_thread_set_prio(gk_thread_t *t, int prio);
int          gk_thread_get_prio(gk_thread_t *t);

/* Sleep the calling thread; ticks are time base ticks. */
void gk_sleep_ticks(uint64_t ticks);
void gk_sleep_us(uint64_t us);

/* Sleep while *addr == expected, until woken or the timeout (ticks,
 * or GK_WAIT_FOREVER) passes.  GK_EAGAIN if *addr already differed. */
int gk_futex_wait(volatile uint32_t *addr, uint32_t expected,
      uint64_t timeout);
/* Wake up to count sleepers on addr, highest priority first. */
int gk_futex_wake(volatile uint32_t *addr, int count);

/* Mutex: one word, 0 = unlocked.  Ownership passes straight to the
 * highest-priority waiter on unlock. */
typedef struct gk_mutex
{
   volatile uint32_t word;
} gk_mutex_t;

/* Recursive mutex: the same word plus a depth. */
typedef struct gk_rmutex
{
   gk_mutex_t m;
   uint32_t depth;
} gk_rmutex_t;

typedef struct gk_cond
{
   volatile uint32_t seq;
} gk_cond_t;

typedef struct gk_sem
{
   volatile uint32_t count;
} gk_sem_t;

#define GK_MUTEX_INIT  { 0 }
#define GK_RMUTEX_INIT { { 0 }, 0 }
#define GK_COND_INIT   { 0 }

void gk_mutex_lock(gk_mutex_t *m);
int  gk_mutex_trylock(gk_mutex_t *m);   /* 0 on success */
int  gk_mutex_timedlock(gk_mutex_t *m, uint64_t timeout);
void gk_mutex_unlock(gk_mutex_t *m);

void gk_rmutex_lock(gk_rmutex_t *m);
int  gk_rmutex_trylock(gk_rmutex_t *m);
void gk_rmutex_unlock(gk_rmutex_t *m);

/* Waits are relative timeouts in ticks; GK_ETIMEDOUT on expiry. */
int  gk_cond_wait(gk_cond_t *c, gk_mutex_t *m, uint64_t timeout);
int  gk_cond_wait_r(gk_cond_t *c, gk_rmutex_t *m, uint64_t timeout);
void gk_cond_signal(gk_cond_t *c);
void gk_cond_broadcast(gk_cond_t *c);

void gk_sem_init(gk_sem_t *s, uint32_t count);
int  gk_sem_wait(gk_sem_t *s, uint64_t timeout);
void gk_sem_post(gk_sem_t *s);

/* Thread-local slots for the C library and rthreads. */
#define GK_TLS_SLOTS 16
int   gk_tls_create(uint32_t *key, void (*dtor)(void*));
void  gk_tls_delete(uint32_t key);
int   gk_tls_set(uint32_t key, const void *value);
void *gk_tls_get(uint32_t key);

#endif
