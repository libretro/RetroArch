/* Checks frontend/drivers/gamemode_linux.c, Game Mode without Feral's
 * libgamemode:
 *
 *  - entering returns at once, whatever the bus does;
 *  - every thread present gets I/O priority best-effort level 0, and a
 *    thread created afterwards inherits it;
 *  - leaving puts the previous priority back on every thread;
 *  - with a power profiles daemon on the system bus, entering holds the
 *    "performance" profile and leaving releases that same hold;
 *  - leaving with @exiting set never starts a helper.
 *
 *  - the performance profile is only held on AC power: pulling the
 *    charger releases it, plugging back in holds it again.
 *
 *   gamemode_test <log file> hold|refuse|none
 *   gamemode_test <log file> plug|unplugged <fake_upower pid>
 *
 * "hold" and "refuse" expect run.sh's fake_ppd on DBUS_SYSTEM_BUS_ADDRESS
 * writing to <log file>; "none" expects nothing to answer there. "plug"
 * and "unplugged" also expect fake_upower, starting on AC or on battery,
 * which flips the power source on each SIGUSR1.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>

#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "frontend/drivers/gamemode_linux.h"

#define IOPRIO_WHO_PROCESS 1
#define IOPRIO_BE0         (2 << 13)

static int fails;

#define CHECK(cond, ...) do { \
   if (!(cond)) { fprintf(stderr, "FAIL: " __VA_ARGS__); \
      fputc('\n', stderr); fails++; } } while (0)

/* verbosity.h without HAVE_LOGGER */
void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   fputs("   log: ", stdout);
   vprintf(fmt, ap);
   va_end(ap);
   fflush(stdout);
}
void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   fputs("   warn: ", stdout);
   vprintf(fmt, ap);
   va_end(ap);
   fflush(stdout);
}

static long now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static int ioprio_of(long tid)
{
   return (int)syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, tid);
}

static long gettid_(void) { return syscall(SYS_gettid); }

/* A thread that exists before Game Mode is entered. */
static retro_atomic_int_t old_tid;
static retro_atomic_int_t old_stop;
static void old_thread(void *d)
{
   (void)d;
   retro_atomic_store_release_int(&old_tid, (int)gettid_());
   while (!retro_atomic_load_acquire_int(&old_stop))
      usleep(1000);
}

/* A thread created inside Game Mode reads its own priority. */
static retro_atomic_int_t new_prio;
static void new_thread(void *d)
{
   (void)d;
   retro_atomic_store_release_int(&new_prio, ioprio_of(0));
}

/* Waits up to 10 s for a line of @log that starts with @prefix. */
static int wait_line(const char *log, const char *prefix)
{
   long end = now_ms() + 10000;
   size_t n = strlen(prefix);
   do
   {
      char line[512];
      FILE *f = fopen(log, "r");
      if (f)
      {
         while (fgets(line, sizeof(line), f))
            if (!strncmp(line, prefix, n))
            {
               fclose(f);
               return 1;
            }
         fclose(f);
      }
      usleep(10000);
   } while (now_ms() < end);
   return 0;
}

static int count_lines(const char *log, const char *prefix)
{
   char line[512];
   int c = 0;
   size_t n = strlen(prefix);
   FILE *f = fopen(log, "r");
   if (!f)
      return 0;
   while (fgets(line, sizeof(line), f))
      if (!strncmp(line, prefix, n))
         c++;
   fclose(f);
   return c;
}

static int wait_count(const char *log, const char *prefix, int want)
{
   long end = now_ms() + 10000;
   do
   {
      if (count_lines(log, prefix) >= want)
         return 1;
      usleep(10000);
   } while (now_ms() < end);
   return 0;
}

/* Flips fake_upower's power source and waits until it has. */
static void flip(const char *log, long pid, int want_battery_lines)
{
   kill((pid_t)pid, SIGUSR1);
   wait_count(log, "battery ", want_battery_lines);
}

static int plug_test(const char *log, const char *mode, long pid)
{
   if (!strcmp(mode, "plug"))
   {
      CHECK(gamemode_linux_set(true, false), "entering reported failure");
      CHECK(wait_count(log, "hold performance", 1), "no hold on AC");
      flip(log, pid, 1);
      CHECK(wait_line(log, "release 41"), "pulling the charger did not release the hold");
      flip(log, pid, 2);
      CHECK(wait_count(log, "hold performance", 2), "plugging back in did not hold again");
      CHECK(gamemode_linux_set(false, false), "leaving reported failure");
      CHECK(wait_line(log, "release 42"), "leaving did not release the second hold");
   }
   else
   {
      CHECK(gamemode_linux_set(true, false), "entering reported failure");
      CHECK(ioprio_of(0) == IOPRIO_BE0, "I/O priority not raised on battery");
      usleep(500000);
      CHECK(count_lines(log, "hold ") == 0, "held the performance profile on battery");
      flip(log, pid, 1);
      CHECK(wait_count(log, "hold performance", 1), "plugging in did not hold");
      CHECK(gamemode_linux_set(false, false), "leaving reported failure");
      CHECK(wait_line(log, "release 41"), "leaving did not release");
   }
   usleep(200000);
   CHECK(count_lines(log, "release") == count_lines(log, "hold "),
         "holds and releases do not pair up");
   if (fails)
      return 1;
   printf("PASS %s\n", mode);
   return 0;
}

int main(int argc, char **argv)
{
   const char *log, *mode;
   sthread_t *old, *nt;
   int before, otid;
   long t0, dt;

   if (argc < 3)
      return 2;
   log  = argv[1];
   mode = argv[2];
   if (argc > 3)
      return plug_test(log, mode, strtol(argv[3], NULL, 10));

   before = ioprio_of(0);
   CHECK(before != IOPRIO_BE0, "test needs a starting I/O priority other than BE/0");

   old = sthread_create(old_thread, NULL);
   while (!(otid = retro_atomic_load_acquire_int(&old_tid)))
      usleep(1000);

   t0 = now_ms();
   CHECK(gamemode_linux_set(true, false), "entering reported failure");
   dt = now_ms() - t0;
   CHECK(dt < 50, "entering took %ld ms; it must not wait on the bus", dt);

   CHECK(ioprio_of(0)    == IOPRIO_BE0, "main thread not at BE/0 (%#x)", ioprio_of(0));
   CHECK(ioprio_of(otid) == IOPRIO_BE0, "existing thread not at BE/0 (%#x)", ioprio_of(otid));

   retro_atomic_store_release_int(&new_prio, -1);
   nt = sthread_create(new_thread, NULL);
   sthread_join(nt);
   CHECK(retro_atomic_load_acquire_int(&new_prio) == IOPRIO_BE0,
         "new thread did not inherit BE/0 (%#x)",
         retro_atomic_load_acquire_int(&new_prio));

   /* Entering twice is one hold. */
   CHECK(gamemode_linux_set(true, false), "re-entering reported failure");

   if (!strcmp(mode, "hold") || !strcmp(mode, "refuse"))
   {
      CHECK(wait_line(log, "hold performance Game Mode org.libretro.RetroArch"),
            "no HoldProfile(performance) reached the daemon");
      usleep(200000);
      CHECK(count_lines(log, "hold ") == 1, "expected one HoldProfile, got %d",
            count_lines(log, "hold "));
   }

   t0 = now_ms();
   CHECK(gamemode_linux_set(false, false), "leaving reported failure");
   dt = now_ms() - t0;
   CHECK(dt < 50, "leaving took %ld ms", dt);
   CHECK(ioprio_of(0)    == before, "main thread priority not restored (%#x)", ioprio_of(0));
   CHECK(ioprio_of(otid) == before, "existing thread priority not restored (%#x)", ioprio_of(otid));

   if (!strcmp(mode, "hold"))
      CHECK(wait_line(log, "release 41"), "the hold was not released with its cookie");
   else
   {
      usleep(300000);
      CHECK(count_lines(log, "release") == 0, "release sent for a hold never granted");
   }

   /* Enter, then leave as on exit: priority back, no new bus traffic. */
   if (!strcmp(mode, "hold"))
   {
      CHECK(gamemode_linux_set(true, false), "second enter reported failure");
      CHECK(wait_count(log, "hold performance", 2),
            "second enter did not hold again");
      CHECK(gamemode_linux_set(false, true), "exit leave reported failure");
      CHECK(ioprio_of(0) == before, "exit leave did not restore priority");
      usleep(300000);
      CHECK(count_lines(log, "release") == 1,
            "exit leave talked to the bus; the hold must go with the connection");
   }

   retro_atomic_store_release_int(&old_stop, 1);
   sthread_join(old);

   if (fails)
      return 1;
   printf("PASS %s\n", mode);
   return 0;
}
