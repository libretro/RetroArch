/* Regression test for the Bluetooth scan task, tasks/task_bluetooth.c,
 * on the real threaded task queue.
 *
 * The scan ran inside one handler call and the drivers waited out the
 * ten-second discovery window in it - bluez in a sleep, bluetoothctl in
 * a blocking "scan on" - holding the one task thread, and every task
 * queued behind it, for the whole window. It now begins, is parked and
 * looks every poll whether the driver is done, and the queue runs
 * other tasks meanwhile.
 *
 * The contract this pins, with the window shortened to 300 ms:
 *
 *   begin, then end        -> once each, a window apart
 *   during the window      -> another task runs at once
 *   the callback           -> fires after end, with the task done
 *   the window             -> a pass a poll, not a run of them
 *   a driver done early    -> the scan ends then, not at the window
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <queues/task_queue.h>

#include "bluetooth/bluetooth_driver.h"
#include "msg_hash.h"

bool task_push_bluetooth_scan(retro_task_callback_t cb);

/* ---- stand-ins for the driver layer and strings ---- */

static retro_atomic_int_t begins = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t ends   = RETRO_ATOMIC_INT_INITIALIZER(0);
static long long begin_at, end_at;

static long long mono_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* the driver's hold: every call on it before its release */
struct bluetooth_hold { int unused; };
static struct bluetooth_hold the_hold;
static retro_atomic_int_t holds    = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t releases = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t misuse   = RETRO_ATOMIC_INT_INITIALIZER(0);

static void hold_used(struct bluetooth_hold *hold)
{
   if (     hold != &the_hold
         || retro_atomic_load_acquire_int(&releases)
            >= retro_atomic_load_acquire_int(&holds))
      retro_atomic_fetch_add_int(&misuse, 1);
}

struct bluetooth_hold *driver_bluetooth_scan_hold(void)
{ retro_atomic_fetch_add_int(&holds, 1); return &the_hold; }
void driver_bluetooth_scan_release(struct bluetooth_hold *hold)
{ hold_used(hold); retro_atomic_fetch_add_int(&releases, 1); }
void driver_bluetooth_scan_begin(struct bluetooth_hold *hold)
{ hold_used(hold); begin_at = mono_ms(); retro_atomic_fetch_add_int(&begins, 1); }
void driver_bluetooth_scan_end(struct bluetooth_hold *hold)
{ hold_used(hold); end_at = mono_ms(); retro_atomic_fetch_add_int(&ends, 1); }
const char *msg_hash_to_str(enum msg_hash_enums msg) { return "msg"; }

/* the driver says it has found all it will */
static retro_atomic_int_t driver_done = RETRO_ATOMIC_INT_INITIALIZER(0);
bool driver_bluetooth_scan_done(struct bluetooth_hold *hold)
{ hold_used(hold); return retro_atomic_load_acquire_int(&driver_done) != 0; }

/* ---- the task under test's neighbours ---- */

static retro_atomic_int_t other_ran  = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t scan_done  = RETRO_ATOMIC_INT_INITIALIZER(0);
static long long other_at;

static void other_handler(retro_task_t *task)
{
   other_at = mono_ms();
   retro_atomic_store_release_int(&other_ran, 1);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void scan_cb(retro_task_t *task, void *td, void *ud, const char *err)
{
   retro_atomic_store_release_int(&scan_done, 1);
}

/* The scan's passes: it asks its flags on each one after the first. */
static retro_atomic_int_t scan_passes = RETRO_ATOMIC_INT_INITIALIZER(0);
uint8_t __real_task_get_flags(retro_task_t *task);
uint8_t __wrap_task_get_flags(retro_task_t *task)
{
   if (task->callback == scan_cb)
      retro_atomic_fetch_add_int(&scan_passes, 1);
   return __real_task_get_flags(task);
}

static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   printf("  [%s] %s\n", cond ? "pass" : "FAIL", what);
   if (!cond)
      failures++;
}

int main(void)
{
   retro_task_t *other;
   long long pushed_at;
   int i;

   task_queue_init(true, NULL);

   check(task_push_bluetooth_scan(scan_cb), "the scan task is pushed");
   for (i = 0; i < 1000 && !retro_atomic_load_acquire_int(&begins); i++)
      usleep(1000);
   check(retro_atomic_load_acquire_int(&begins) == 1, "discovery begins");

   other          = task_init();
   other->handler = other_handler;
   other->flags  |= RETRO_TASK_FLG_MUTE;
   pushed_at      = mono_ms();
   task_queue_push(other);
   for (i = 0; i < 2000 && !retro_atomic_load_acquire_int(&other_ran); i++)
      usleep(1000);
   check(retro_atomic_load_acquire_int(&other_ran)
         && other_at - pushed_at < 100,
         "another task runs during the scan window");
   check(!retro_atomic_load_acquire_int(&ends),
         "while discovery is still running");

   for (i = 0; i < 3000 && !retro_atomic_load_acquire_int(&scan_done); i++)
   {
      task_queue_check();
      usleep(1000);
   }
   check(retro_atomic_load_acquire_int(&ends) == 1, "discovery ends once");
   check(end_at - begin_at >= 280 && end_at - begin_at < 1000,
         "a window after it began");
   check(retro_atomic_load_acquire_int(&scan_done), "the scan's callback fires");
   check(retro_atomic_load_acquire_int(&begins) == 1, "and it began only once");
   /* a pass a poll through the window, parked between them */
   check(retro_atomic_load_acquire_int(&scan_passes)
         <= BLUETOOTH_SCAN_WINDOW_US / BLUETOOTH_SCAN_POLL_US + 3,
         "the window passes in a pass a poll");

   /* a driver that has found all it will 100 ms in: ended then */
   retro_atomic_store_release_int(&scan_done, 0);
   check(task_push_bluetooth_scan(scan_cb), "a second scan is pushed");
   for (i = 0; i < 1000 && retro_atomic_load_acquire_int(&begins) < 2; i++)
      usleep(1000);
   usleep(100000);
   retro_atomic_store_release_int(&driver_done, 1);
   for (i = 0; i < 3000 && !retro_atomic_load_acquire_int(&scan_done); i++)
   {
      task_queue_check();
      usleep(1000);
   }
   check(retro_atomic_load_acquire_int(&ends) == 2
         && end_at - begin_at < 280,
         "a driver done early ends the scan before the window");
   check(   retro_atomic_load_acquire_int(&holds) == 2
         && retro_atomic_load_acquire_int(&releases) == 2,
         "each scan takes the driver once and lets it go once");
   check(!retro_atomic_load_acquire_int(&misuse),
         "the driver is used only while held, and only the hold taken");

   task_queue_deinit();

   if (failures)
   {
      printf("\n%u failure(s)\n", failures);
      return 1;
   }
   printf("\nall passed\n");
   return 0;
}
