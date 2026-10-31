/* Exercise production Linux deadline accounting without scheduler noise. */
#if defined(__linux__)
#define _GNU_SOURCE
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
static int deadline_clock(clockid_t clock, struct timespec *now);
static long deadline_syscall(long number, ...);
#define clock_gettime deadline_clock
#define syscall deadline_syscall
#include "../../../rthreads/retro_eventcount.c"
#undef syscall
#undef clock_gettime

static uint64_t now_ns, step_ns;
static unsigned clocks, parks;
static int64_t parked_ns;
static retro_eventcount_t *wake_target;

static int deadline_clock(clockid_t clock, struct timespec *now)
{
   (void)clock;
   clocks++;
   now->tv_sec = (time_t)(now_ns / 1000000000);
   now->tv_nsec = (long)(now_ns % 1000000000);
   now_ns += step_ns;
   if (wake_target && clocks == 2)
      retro_eventcount_notify(wake_target);
   return 0;
}

static long deadline_syscall(long number, ...)
{
   va_list args;
   int op;
   (void)number;
   va_start(args, number);
   (void)va_arg(args, void*);
   op = va_arg(args, int);
   (void)va_arg(args, int);
#if defined(RETRO_EC_ADDR_LINUX)
   if (op == FUTEX_WAIT_PRIVATE)
   {
      const struct timespec *ts = va_arg(args, const struct timespec*);
      parks++;
      parked_ns = (int64_t)ts->tv_sec * 1000000000 + ts->tv_nsec;
   }
#else
   (void)op;
#endif
   va_end(args);
   errno = ETIMEDOUT;
   return -1;
}

int main(void)
{
#if defined(RETRO_EC_ADDR_LINUX)
   retro_eventcount_t ec;
   const char *values[] = { "0", "32", "0x40", "8192", "8193",
      "4294967295", "184467440737095516160", "-1", "12bad", "" };
   const unsigned expected[] = { 0, 32, 64, 8192, 8192, 8192, 8192, 0, 0, 0 };
   unsigned i;
   int failed = 0, key;
   bool result;
   if (!retro_eventcount_init(&ec)) return 1;
   setenv("RETRO_EVENTCOUNT_SPIN_US", "0", 1);
   for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
   {
      setenv("RETRO_EVENTCOUNT_SPIN", values[i], 1);
      retro_atomic_store_release_int(&ec_linux_spin_state, 0);
      if (retro_eventcount_spin_iters() != expected[i])
      {
         printf("FAIL override %s\n", values[i]);
         failed++;
      }
   }
   unsetenv("RETRO_EVENTCOUNT_SPIN");
   unsetenv("RETRO_EVENTCOUNT_SPIN_US");
   /* Never execute an unvalidated override in the baseline probe. */
   ec_linux_spin = 8;
   retro_atomic_store_release_int(&ec_linux_spin_state, 2);
   for (i = 0; i < 6; i++)
   {
      clocks = parks = 0;
      parked_ns = 0;
      now_ns = UINT64_C(1999990000);
      step_ns = i == 5 ? 1 : i == 3 ? 120000 : 20000;
      wake_target = i == 4 ? &ec : NULL;
      key = retro_eventcount_prepare_wait(&ec);
      result = retro_eventcount_commit_wait_timeout(&ec, key,
            i == 0 ? 0 : i == 1 ? -1 : i == 5 ? 1 : 100);
      if (retro_atomic_load_acquire_int(&ec.waiters) != 0
            || (i < 2 && (clocks || parks || result))
            || (i == 2 && (parks != 1 || parked_ns != 60000 || result))
            || (i == 3 && (parks || result))
            || (i == 4 && (parks || !result))
            || (i == 5 && (parks != 1 || parked_ns != 998 || result)))
      {
         printf("FAIL deadline case %u: clocks=%u parks=%u ns=%ld result=%d\n",
               i, clocks, parks, (long)parked_ns, result);
         failed++;
      }
   }
   retro_eventcount_free(&ec);
   if (!failed) puts("PASS Linux deadlines, polling and bounded spin overrides");
   return failed != 0;
#else
   (void)deadline_clock;
   (void)deadline_syscall;
   return 0;
#endif
}
#else
int main(void) { return 0; }
#endif
