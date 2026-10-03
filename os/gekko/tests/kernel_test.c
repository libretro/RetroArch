/* Kernel checks, run in Dolphin by os/gekko/tests/run-dolphin.sh:
 * boot, heap, sleep, priorities, round robin, mutex hand-off,
 * condition variables, semaphores, lazy FP, pthreads and loader
 * arguments (run-dolphin.sh with ARGS='sd:/k.dol|sd:/roms|a b.nes'). */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gekko/thread.h>
#include <gekko/irq.h>

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (cond) \
         gk_debug_printf("ok   %s", what); \
      else \
      { \
         gk_debug_printf("FAIL %s", what); \
         failures++; \
      } \
   } while (0)

/* ---- priorities: a higher thread runs the moment it is ready ---- */

static volatile int order[8];
static volatile int order_n;

static void *prio_fn(void *arg)
{
   order[order_n++] = (int)(intptr_t)arg;
   return NULL;
}

static void test_priorities(void)
{
   gk_thread_t *lo, *hi;
   order_n = 0;
   lo = gk_thread_create(prio_fn, (void*)1, NULL, 0, GK_PRIO_DEFAULT - 10);
   hi = gk_thread_create(prio_fn, (void*)2, NULL, 0, GK_PRIO_DEFAULT + 10);
   order[order_n++] = 3;
   gk_thread_join(hi);
   gk_thread_join(lo);
   CHECK(order_n == 3 && order[0] == 2 && order[1] == 3 && order[2] == 1,
         "priority: higher preempts at create, lower waits for a block");
}

/* ---- round robin and mutex hand-off between equals ---- */

static gk_mutex_t mx = GK_MUTEX_INIT;
static volatile uint32_t shared;
static volatile uint32_t spins[2];

static void *rr_fn(void *arg)
{
   int id = (int)(intptr_t)arg;
   int i;
   for (i = 0; i < 20000; i++)
   {
      gk_mutex_lock(&mx);
      shared++;
      gk_mutex_unlock(&mx);
      spins[id]++;
   }
   return NULL;
}

static void test_round_robin(void)
{
   gk_thread_t *a = gk_thread_create(rr_fn, (void*)0, NULL, 0, 30);
   gk_thread_t *b = gk_thread_create(rr_fn, (void*)1, NULL, 0, 30);
   gk_thread_join(a);
   gk_thread_join(b);
   CHECK(shared == 40000, "mutex: 40000 increments from two threads");
}

static volatile int slice_seen[2];
static volatile int slice_stop;

static void *spin_fn(void *arg)
{
   int id = (int)(intptr_t)arg;
   while (!slice_stop)
      slice_seen[id] = 1;
   return NULL;
}

static void test_slices(void)
{
   gk_thread_t *a, *b;
   uint64_t until;
   slice_stop = 0;
   a = gk_thread_create(spin_fn, (void*)0, NULL, 0, 20);
   b = gk_thread_create(spin_fn, (void*)1, NULL, 0, 20);
   until = gk_ticks() + GK_US_TO_TICKS(60000);
   gk_sleep_us(50000);
   slice_stop = 1;
   gk_thread_join(a);
   gk_thread_join(b);
   CHECK(slice_seen[0] && slice_seen[1] && gk_ticks() < until + GK_US_TO_TICKS(100000),
         "slices: two busy equals both run, a higher sleeper wakes on time");
}

/* ---- sleep accuracy ---- */

static void test_sleep(void)
{
   uint64_t t0 = gk_ticks(), dt;
   gk_sleep_us(20000);
   dt = GK_TICKS_TO_US(gk_ticks() - t0);
   gk_debug_printf("     sleep 20000 us took %u us", (unsigned)dt);
   CHECK(dt >= 20000 && dt < 22000, "sleep: 20 ms within 2 ms");
}

/* ---- condition variable: producer / consumer ---- */

static gk_mutex_t cmx = GK_MUTEX_INIT;
static gk_cond_t  cv  = GK_COND_INIT;
static volatile int queue_n, consumed;

static void *consumer_fn(void *arg)
{
   int i;
   (void)arg;
   for (i = 0; i < 1000; i++)
   {
      gk_mutex_lock(&cmx);
      while (!queue_n)
         gk_cond_wait(&cv, &cmx, GK_WAIT_FOREVER);
      queue_n--;
      consumed++;
      gk_mutex_unlock(&cmx);
   }
   return NULL;
}

static void test_cond(void)
{
   int i, timed;
   gk_thread_t *c = gk_thread_create(consumer_fn, NULL, NULL, 0, 40);
   for (i = 0; i < 1000; i++)
   {
      gk_mutex_lock(&cmx);
      queue_n++;
      gk_cond_signal(&cv);
      gk_mutex_unlock(&cmx);
      if (!(i & 7))
         gk_thread_yield();
   }
   gk_thread_join(c);
   CHECK(consumed == 1000, "cond: 1000 items through a producer/consumer");
   gk_mutex_lock(&cmx);
   timed = gk_cond_wait(&cv, &cmx, GK_US_TO_TICKS(5000));
   gk_mutex_unlock(&cmx);
   CHECK(timed == GK_ETIMEDOUT, "cond: a timed wait expires");
}

/* ---- semaphore posted from another thread ---- */

static gk_sem_t sem;

static void *poster_fn(void *arg)
{
   int i;
   (void)arg;
   for (i = 0; i < 100; i++)
   {
      gk_sem_post(&sem);
      gk_sleep_us(100);
   }
   return NULL;
}

static void test_sem(void)
{
   int i, got = 0;
   gk_thread_t *p;
   gk_sem_init(&sem, 0);
   p = gk_thread_create(poster_fn, NULL, NULL, 0, 50);
   for (i = 0; i < 100; i++)
      if (gk_sem_wait(&sem, GK_US_TO_TICKS(100000)) == GK_OK)
         got++;
   gk_thread_join(p);
   CHECK(got == 100, "sem: 100 posts, 100 waits");
}

/* ---- lazy FP: two threads keep their own registers ---- */

static volatile double fp_out[2];

static void *fp_fn(void *arg)
{
   int id = (int)(intptr_t)arg;
   volatile double acc = id ? 1.0 : -1.0;
   double step = id ? 0.5 : 0.25;
   int i;
   for (i = 0; i < 20000; i++)
   {
      acc += step;
      if (!(i & 255))
         gk_thread_yield();
   }
   fp_out[id] = acc;
   return NULL;
}

static void test_fp(void)
{
   double mine = 3.0;
   gk_thread_t *a = gk_thread_create(fp_fn, (void*)0, NULL, 0, 25);
   gk_thread_t *b = gk_thread_create(fp_fn, (void*)1, NULL, 0, 25);
   mine *= 7.0;
   gk_thread_join(a);
   gk_thread_join(b);
   mine += 0.5;
   CHECK(fp_out[0] == -1.0 + 20000 * 0.25 && fp_out[1] == 1.0 + 20000 * 0.5
         && mine == 21.5, "fp: each thread keeps its own registers");
}

/* ---- an exception handler that uses FP, inside a thread that does ---- */

static volatile double trap_seen;

static int trap_handler(unsigned vector, struct gk_ctx *ctx)
{
   volatile double x = (double)ctx->gpr[3];
   trap_seen = x * 1.5;
   ctx->srr0 += 4;
   return 1;
}

static void test_handler_fp(void)
{
   double in = 1234.25, out = 0;
   gk_exception_set(0x0700, trap_handler);
   __asm__ __volatile__(
         "lfd 14,%1\n"
         "li 3,10\n"
         "tw 31,0,0\n"
         "stfd 14,%0\n"
         : "=m"(out) : "m"(in) : "fr14", "r3", "memory");
   gk_exception_set(0x0700, NULL);
   CHECK(trap_seen == 15.0 && out == 1234.25,
         "exceptions: a handler may use FP; the thread's registers survive");
}

/* ---- the C library ---- */

static void *errno_fn(void *arg)
{
   (void)arg;
   errno = EINVAL;
   return (void*)(intptr_t)errno;
}

static void test_libc(void)
{
   pthread_t pt;
   void *ret = NULL;
   char buf[32];
   void *big;
   errno = 0;
   CHECK(pthread_create(&pt, NULL, errno_fn, NULL) == 0
         && pthread_join(pt, &ret) == 0
         && (intptr_t)ret == EINVAL && errno == 0,
         "libc: pthreads, errno per thread");
   snprintf(buf, sizeof(buf), "%d-%s-%.2f", 42, "x", 1.5);
   CHECK(!strcmp(buf, "42-x-1.50"), "libc: snprintf");
   big = malloc(32u << 20);
   gk_debug_printf("     32 MiB at %p", big);
#ifdef HW_RVL
   CHECK(big && (uint32_t)big >= 0x90000000u, "heap: past MEM1 into MEM2");
#else
   CHECK(!big, "heap: no room for 32 MiB on a GameCube");
#endif
   free(big);
   big = NULL;
   CHECK(posix_memalign(&big, 64, 100) == 0 && big
         && !((uint32_t)big & 63)
         && posix_memalign(&big, 24, 100) == EINVAL,
         "libc: posix_memalign");
   free(big);
}

#ifdef HW_RVL
/* The L2 runs with the fetch and castout enhancements. */
static void test_l2(void)
{
   uint32_t hid4;
   __asm__ __volatile__("mfspr %0,1011" : "=r"(hid4));
   CHECK((hid4 & 0x24300000u) == 0x24300000u,
         "l2: HID4 has the fetch and castout enhancements");
}
#endif

static void test_args(int argc, char **argv)
{
   if (!argc)
   {
      CHECK(!argv || !argv[0], "args: none without a loader");
      return;
   }
   CHECK(argc == 3 && !strcmp(argv[0], "sd:/k.dol")
         && !strcmp(argv[1], "sd:/roms") && !strcmp(argv[2], "a b.nes")
         && !argv[3], "args: the loader's command line");
}

int main(int argc, char **argv)
{
   gk_debug_printf("kernel test: tb %u Hz, MEM1 %p-%p, MEM2 %p-%p",
         (unsigned)gk_tb_hz, gk_mem1.lo, gk_mem1.hi, gk_mem2.lo, gk_mem2.hi);
   test_priorities();
   test_sleep();
   test_round_robin();
   test_slices();
   test_cond();
   test_sem();
   test_fp();
   test_handler_fp();
   test_libc();
#ifdef HW_RVL
   test_l2();
#endif
   test_args(argc, argv);
   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
   return 0;
}
