/* The C library's system interface: heap, locks, threads, time.
 *
 * devkitPPC's newlib has had three sets of system hooks, all served
 * here.  From r49 (sys/lock.h defines __COND_INITIALIZER) the locks are
 * the kernel's mutexes outright: _LOCK_T is one zero-initialised word
 * and _LOCK_RECURSIVE_T a word and a depth, exactly gk_mutex_t and
 * gk_rmutex_t, and conditions and threads are hooks too.  From newlib
 * 4.3 there are separate hooks for plain and recursive locks, both one
 * int; before it one set of hooks takes the kind at init. */

#include <errno.h>
#include <malloc.h>
#include <newlib.h>
#include <reent.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/iosupport.h>
#include <sys/lock.h>
#include <sys/time.h>

#include <gekko/power.h>

#include "kernel.h"

#if defined(__COND_INITIALIZER)
#define GK_NEWLIB_HOOKS 3
#elif __NEWLIB__ > 4 || (__NEWLIB__ == 4 && __NEWLIB_MINOR__ >= 3)
#define GK_NEWLIB_HOOKS 2
#else
#define GK_NEWLIB_HOOKS 1
#endif

typedef char assert_lock_size[sizeof(_LOCK_T) == sizeof(gk_mutex_t) ? 1 : -1];
#if GK_NEWLIB_HOOKS >= 3
typedef char assert_rlock_size[
   sizeof(_LOCK_RECURSIVE_T) == sizeof(gk_rmutex_t) ? 1 : -1];
#endif
typedef char assert_ctx_size[sizeof(struct gk_ctx) == CTX_SIZE ? 1 : -1];
typedef char assert_fp_off[
   offsetof(struct gk_thread, fp) == THR_FP ? 1 : -1];
typedef char assert_fp_size[sizeof(struct gk_fpctx) == FPX_SIZE ? 1 : -1];

/* ---- heap: MEM1 first, then MEM2 for good ---- */

static gk_arena_t *heap_arena;

void *_sbrk_r(struct _reent *r, ptrdiff_t incr)
{
   uint8_t *prev;
   uint32_t level = gk_irq_disable();
   if (!heap_arena)
      heap_arena = &gk_mem1;
   if (incr > 0 && heap_arena->lo + incr > heap_arena->hi)
   {
      if (heap_arena == &gk_mem1 && gk_mem2.hi > gk_mem2.lo
            && gk_mem2.lo + incr <= gk_mem2.hi)
         heap_arena = &gk_mem2;
      else
      {
         gk_irq_restore(level);
         r->_errno = ENOMEM;
         return (void*)-1;
      }
   }
   prev = heap_arena->lo;
   heap_arena->lo += incr;
   gk_irq_restore(level);
   return prev;
}

static gk_rmutex_t malloc_lock;

void __syscall_malloc_lock(struct _reent *r)
{
   (void)r;
   gk_rmutex_lock(&malloc_lock);
}

void __syscall_malloc_unlock(struct _reent *r)
{
   (void)r;
   gk_rmutex_unlock(&malloc_lock);
}

/* newlib declares this but leaves it to the system. */
int posix_memalign(void **out, size_t align, size_t size)
{
   void *p;
   if (!align || (align & (align - 1)) || (align % sizeof(void*)))
      return EINVAL;
   if (!(p = memalign(align, size)) && size)
      return ENOMEM;
   *out = p;
   return 0;
}

/* ---- per-thread C library state ---- */

static struct _reent *cur_reent(void)
{
   struct gk_thread *t = gk_cur;
   if (!t || t == gk_sched_main())
      return _impure_ptr;
   return &t->reent;
}

#if GK_NEWLIB_HOOKS >= 2
struct _reent *__syscall_getreent(void) { return cur_reent(); }
#else
struct _reent *__getreent(void)         { return cur_reent(); }
#endif

/* ---- locks ---- */

#if GK_NEWLIB_HOOKS >= 3
void __syscall_lock_init(_LOCK_T *lock)            { *lock = 0; }
void __syscall_lock_acquire(_LOCK_T *lock)
{
   gk_mutex_lock((gk_mutex_t*)lock);
}
int __syscall_lock_try_acquire(_LOCK_T *lock)
{
   return gk_mutex_trylock((gk_mutex_t*)lock);
}
void __syscall_lock_release(_LOCK_T *lock)
{
   gk_mutex_unlock((gk_mutex_t*)lock);
}
void __syscall_lock_close(_LOCK_T *lock)           { (void)lock; }

void __syscall_lock_init_recursive(_LOCK_RECURSIVE_T *lock)
{
   memset(lock, 0, sizeof(*lock));
}
void __syscall_lock_acquire_recursive(_LOCK_RECURSIVE_T *lock)
{
   gk_rmutex_lock((gk_rmutex_t*)lock);
}
int __syscall_lock_try_acquire_recursive(_LOCK_RECURSIVE_T *lock)
{
   return gk_rmutex_trylock((gk_rmutex_t*)lock);
}
void __syscall_lock_release_recursive(_LOCK_RECURSIVE_T *lock)
{
   gk_rmutex_unlock((gk_rmutex_t*)lock);
}
void __syscall_lock_close_recursive(_LOCK_RECURSIVE_T *lock)
{
   (void)lock;
}
#else
/* A plain int lock is a gk_mutex_t word.  A recursive one holds 1 + its
 * slot in rlocks, which no thread pointer is small enough to equal.
 * newlib initialises a lock it finds zero just before taking it, so init
 * never clears a word another thread may have taken in between. */
#define GK_RLOCKS 128

static gk_rmutex_t rlocks[GK_RLOCKS];
static int        *rlock_owner[GK_RLOCKS];

static gk_rmutex_t *rlock_get(int *lock)
{
   uint32_t i = (uint32_t)*lock - 1u;
   return (i < GK_RLOCKS && rlock_owner[i] == lock) ? &rlocks[i] : NULL;
}

static gk_rmutex_t *rlock_init(int *lock)
{
   gk_rmutex_t *m;
   uint32_t     i;
   uint32_t level = gk_irq_disable();
   if (!(m = rlock_get(lock)))
   {
      for (i = 0; i < GK_RLOCKS && rlock_owner[i]; i++);
      if (i == GK_RLOCKS)
         gk_panic("out of C library locks");
      rlock_owner[i]  = lock;
      rlocks[i].m.word = 0;
      rlocks[i].depth  = 0;
      *lock           = (int)(i + 1u);
      m               = &rlocks[i];
   }
   gk_irq_restore(level);
   return m;
}

static void rlock_close(int *lock)
{
   uint32_t level = gk_irq_disable();
   gk_rmutex_t *m = rlock_get(lock);
   if (m)
   {
      rlock_owner[m - rlocks] = NULL;
      *lock                   = 0;
   }
   gk_irq_restore(level);
}

static void rlock_release(int *lock)
{
   gk_rmutex_t *m = rlock_get(lock);
   if (m)
      gk_rmutex_unlock(m);
}

#if GK_NEWLIB_HOOKS == 2
void __syscall_lock_init(_LOCK_T *lock)            { (void)lock; }
void __syscall_lock_acquire(_LOCK_T *lock)
{
   gk_mutex_lock((gk_mutex_t*)lock);
}
int __syscall_lock_try_acquire(_LOCK_T *lock)
{
   return gk_mutex_trylock((gk_mutex_t*)lock);
}
void __syscall_lock_release(_LOCK_T *lock)
{
   gk_mutex_unlock((gk_mutex_t*)lock);
}
void __syscall_lock_close(_LOCK_T *lock)           { (void)lock; }

void __syscall_lock_init_recursive(_LOCK_RECURSIVE_T *lock)
{
   rlock_init(lock);
}
void __syscall_lock_acquire_recursive(_LOCK_RECURSIVE_T *lock)
{
   gk_rmutex_lock(rlock_init(lock));
}
int __syscall_lock_try_acquire_recursive(_LOCK_RECURSIVE_T *lock)
{
   return gk_rmutex_trylock(rlock_init(lock));
}
void __syscall_lock_release_recursive(_LOCK_RECURSIVE_T *lock)
{
   rlock_release(lock);
}
void __syscall_lock_close_recursive(_LOCK_RECURSIVE_T *lock)
{
   rlock_close(lock);
}
#else
static int rlock_is_handle(const int *lock)
{
   return (uint32_t)*lock - 1u < GK_RLOCKS;
}

int __syscall_lock_init(int *lock, int recursive)
{
   if (recursive)
      rlock_init(lock);
   return 0;
}
int __syscall_lock_acquire(int *lock)
{
   if (rlock_is_handle(lock))
      gk_rmutex_lock(rlock_init(lock));
   else
      gk_mutex_lock((gk_mutex_t*)lock);
   return 0;
}
int __syscall_lock_release(int *lock)
{
   if (rlock_is_handle(lock))
      rlock_release(lock);
   else
      gk_mutex_unlock((gk_mutex_t*)lock);
   return 0;
}
int __syscall_lock_close(int *lock)
{
   if (rlock_is_handle(lock))
      rlock_close(lock);
   return 0;
}
#endif
#endif

static uint64_t ns_to_ticks(uint64_t ns)
{
   if (ns == UINT64_MAX)
      return GK_WAIT_FOREVER;
   return ns / 1000u * gk_tb_hz / 1000000u;
}

#if GK_NEWLIB_HOOKS >= 3
int __syscall_cond_signal(_COND_T *cond)
{
   gk_cond_signal((gk_cond_t*)cond);
   return 0;
}

int __syscall_cond_broadcast(_COND_T *cond)
{
   gk_cond_broadcast((gk_cond_t*)cond);
   return 0;
}

int __syscall_cond_wait(_COND_T *cond, _LOCK_T *lock, uint64_t timeout_ns)
{
   return gk_cond_wait((gk_cond_t*)cond, (gk_mutex_t*)lock,
         ns_to_ticks(timeout_ns)) == GK_ETIMEDOUT ? ETIMEDOUT : 0;
}

int __syscall_cond_wait_recursive(_COND_T *cond, _LOCK_RECURSIVE_T *lock,
      uint64_t timeout_ns)
{
   return gk_cond_wait_r((gk_cond_t*)cond, (gk_rmutex_t*)lock,
         ns_to_ticks(timeout_ns)) == GK_ETIMEDOUT ? ETIMEDOUT : 0;
}

void __syscall_cond_close(_COND_T *cond) { (void)cond; }

/* ---- pthreads ---- */

int __syscall_thread_create(struct __pthread_t **thread,
      void *(*func)(void*), void *arg, void *stack_addr, size_t stack_size)
{
   gk_thread_t *t = gk_thread_create(func, arg, stack_addr,
         stack_size ? stack_size : 64 * 1024, GK_PRIO_DEFAULT);
   if (!t)
      return EAGAIN;
   *thread = (struct __pthread_t*)t;
   return 0;
}

void *__syscall_thread_join(struct __pthread_t *thread)
{
   return gk_thread_join((gk_thread_t*)thread);
}

int __syscall_thread_detach(struct __pthread_t *thread)
{
   gk_thread_detach((gk_thread_t*)thread);
   return 0;
}

void __syscall_thread_exit(void *value)
{
   gk_thread_exit(value);
}

struct __pthread_t *__syscall_thread_self(void)
{
   return (struct __pthread_t*)gk_thread_self();
}

int __syscall_tls_create(uint32_t *key, void (*destructor)(void*))
{
   return gk_tls_create(key, destructor) ? EAGAIN : 0;
}

int __syscall_tls_set(uint32_t key, const void *value)
{
   return gk_tls_set(key, value) ? EINVAL : 0;
}

void *__syscall_tls_get(uint32_t key)
{
   return gk_tls_get(key);
}

int __syscall_tls_delete(uint32_t key)
{
   gk_tls_delete(key);
   return 0;
}
#endif

/* ---- thread-local slots ---- */

static uint32_t tls_used;
static void   (*tls_dtor[GK_TLS_SLOTS])(void*);

int gk_tls_create(uint32_t *key, void (*dtor)(void*))
{
   uint32_t i;
   uint32_t level = gk_irq_disable();
   for (i = 0; i < GK_TLS_SLOTS; i++)
      if (!(tls_used & (1u << i)))
      {
         tls_used   |= 1u << i;
         tls_dtor[i] = dtor;
         gk_irq_restore(level);
         *key = i;
         return 0;
      }
   gk_irq_restore(level);
   return -1;
}

void gk_tls_delete(uint32_t key)
{
   uint32_t level = gk_irq_disable();
   if (key < GK_TLS_SLOTS)
   {
      tls_used     &= ~(1u << key);
      tls_dtor[key] = NULL;
   }
   gk_irq_restore(level);
}

int gk_tls_set(uint32_t key, const void *value)
{
   if (key >= GK_TLS_SLOTS)
      return -1;
   gk_cur->tls[key] = (void*)value;
   return 0;
}

void *gk_tls_get(uint32_t key)
{
   return key < GK_TLS_SLOTS ? gk_cur->tls[key] : NULL;
}

void gk_tls_run_dtors(struct gk_thread *t)
{
   int pass, i;
   for (pass = 0; pass < 4; pass++)
   {
      int any = 0;
      for (i = 0; i < GK_TLS_SLOTS; i++)
      {
         void *v = t->tls[i];
         if (v && tls_dtor[i])
         {
            t->tls[i] = NULL;
            tls_dtor[i](v);
            any = 1;
         }
      }
      if (!any)
         break;
   }
}

/* ---- time ---- */

static uint64_t     rtc_base_s;   /* wall-clock seconds at tick 0 */
static volatile int wall_set;     /* from the clock chip or settime */

void gk_set_wall_clock(uint64_t unix_seconds)
{
   rtc_base_s = unix_seconds - gk_ticks() / gk_tb_hz;
}

static void ticks_to_ts(uint64_t t, struct timespec *ts)
{
   ts->tv_sec  = (time_t)(t / gk_tb_hz);
   ts->tv_nsec = (long)((t % gk_tb_hz) * 1000000000u / gk_tb_hz);
}

int __syscall_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
   if (clock_id == CLOCK_REALTIME && !wall_set)
   {
      /* The clock chip, on the first wall-clock read. */
      wall_set = 1;
      gk_rtc_sync();
   }
   ticks_to_ts(gk_ticks(), tp);
   if (clock_id == CLOCK_REALTIME)
      tp->tv_sec += (time_t)rtc_base_s;
   return 0;
}

int __syscall_clock_settime(clockid_t clock_id, const struct timespec *tp)
{
   if (clock_id != CLOCK_REALTIME)
      return EINVAL;
   wall_set = 1;
   gk_set_wall_clock((uint64_t)tp->tv_sec);
   return 0;
}

int __syscall_clock_getres(clockid_t clock_id, struct timespec *res)
{
   (void)clock_id;
   res->tv_sec  = 0;
   res->tv_nsec = (long)(1000000000u / gk_tb_hz) + 1;
   return 0;
}

int __syscall_gettod_r(struct _reent *ptr, struct timeval *tp,
      struct timezone *tz)
{
   struct timespec ts;
   (void)ptr;
   if (tp)
   {
      __syscall_clock_gettime(CLOCK_REALTIME, &ts);
      tp->tv_sec  = ts.tv_sec;
      tp->tv_usec = ts.tv_nsec / 1000;
   }
   if (tz)
   {
      tz->tz_minuteswest = 0;
      tz->tz_dsttime     = 0;
   }
   return 0;
}

int __syscall_nanosleep(const struct timespec *req, struct timespec *rem)
{
   gk_sleep_ticks((uint64_t)req->tv_sec * gk_tb_hz
         + ns_to_ticks((uint64_t)req->tv_nsec));
   if (rem)
   {
      rem->tv_sec  = 0;
      rem->tv_nsec = 0;
   }
   return 0;
}

/* ---- leaving ---- */

void __syscall_exit(int rc)
{
   (void)rc;
   gk_exit_to_loader();
}

#if GK_NEWLIB_HOOKS >= 2
void __syscall_abort(void)
{
   gk_panic("abort()");
}

void __syscall_assert_func(const char *file, int line, const char *func,
      const char *expr)
{
   gk_panic("assertion \"%s\" failed: %s:%d (%s)", expr, file, line,
         func ? func : "");
}
#endif

/* ---- stdout and stderr: lines to the debug channel ---- */

static char     out_line[256];
static unsigned out_len;

static void out_flush(void)
{
   out_line[out_len] = '\0';
   gk_debug_line(out_line);
   out_len = 0;
}

static ssize_t out_write(struct _reent *r, void *fd, const char *p,
      size_t len)
{
   size_t i;
   uint32_t level = gk_irq_disable();
   (void)r;
   (void)fd;
   for (i = 0; i < len; i++)
   {
      if (p[i] == '\n')
         out_flush();
      else
      {
         out_line[out_len++] = p[i];
         if (out_len == sizeof(out_line) - 1)
            out_flush();
      }
   }
   gk_irq_restore(level);
   return (ssize_t)len;
}

static devoptab_t dotab_out;

void gk_newlib_init(void)
{
   dotab_out.name         = "stdout";
   dotab_out.write_r      = out_write;
   devoptab_list[STD_OUT] = &dotab_out;
   devoptab_list[STD_ERR] = &dotab_out;
}
