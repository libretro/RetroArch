/* Regression test for the Bluetooth scan task, tasks/task_bluetooth.c,
 * on the real threaded task queue.
 *
 * The scan ran inside one handler call and the drivers waited out the
 * ten-second discovery window in it - bluez in a sleep, bluetoothctl in
 * a blocking "scan on" - holding the one task thread, and every task
 * queued behind it, for the whole window. It is now two steps: begin,
 * then the task reschedules itself for the end of the window and the
 * queue runs other tasks meanwhile.
 *
 * The contract this pins, with the window shortened to 300 ms:
 *
 *   begin, then end        -> once each, a window apart
 *   during the window      -> another task runs at once
 *   the callback           -> fires after end, with the task done
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

void driver_bluetooth_scan_begin(void)
{ begin_at = mono_ms(); retro_atomic_fetch_add_int(&begins, 1); }
void driver_bluetooth_scan_end(void)
{ end_at = mono_ms(); retro_atomic_fetch_add_int(&ends, 1); }
const char *msg_hash_to_str(enum msg_hash_enums msg) { return "msg"; }

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

   task_queue_deinit();

   if (failures)
   {
      printf("\n%u failure(s)\n", failures);
      return 1;
   }
   printf("\nall passed\n");
   return 0;
}
