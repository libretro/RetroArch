/* Regression test for the Linux IIO illuminance sensor poller in
 * input/common/linux_common.c.
 *
 * The poll thread used to sleep a whole period after every reading,
 * and close stopped it with pthread_cancel() into that sleep. A rate
 * change applied only after the current sleep ran out, and the thread
 * could only be ended by cancellation. It now waits on a condition
 * between readings that close and rate changes signal.
 *
 * The contract this pins, against a fake sysfs tree:
 *
 *   open                -> first reading available immediately
 *   value changes       -> a later reading reflects it, at the rate
 *   rate raised from 1 Hz -> applies at once, not after the 1 s wait
 *   close at 1 Hz       -> returns at once, not after the wait
 *   no sensor present   -> open reports none
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boolean.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

#include "input/common/linux_common.h"

/* verbosity.h stand-ins */
void RARCH_LOG(const char *f, ...) { (void)f; }
void RARCH_ERR(const char *f, ...) { (void)f; }
void RARCH_DBG(const char *f, ...) { (void)f; }

#define DEV_DIR  IIO_TEST_DIR "/iio:device0"
#define DEV_FILE DEV_DIR "/in_illuminance_input"

static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   printf("  [%s] %s\n", cond ? "pass" : "FAIL", what);
   if (!cond)
      failures++;
}

static void write_lux(const char *text)
{
   FILE *f = fopen(DEV_FILE ".tmp", "w");
   fputs(text, f);
   fclose(f);
   rename(DEV_FILE ".tmp", DEV_FILE);
}

/* Waits up to limit_ms for the reading to become want; returns ms. */
static int64_t wait_reading(linux_illuminance_sensor_t *s, float want,
      int limit_ms)
{
   retro_time_t start = cpu_features_get_time_usec();
   while ((cpu_features_get_time_usec() - start) / 1000 < limit_ms)
   {
      float v = linux_get_illuminance_reading(s);
      if (v > want - 0.01f && v < want + 0.01f)
         break;
      retro_sleep(1);
   }
   return (int64_t)((cpu_features_get_time_usec() - start) / 1000);
}

int main(void)
{
   linux_illuminance_sensor_t *s;
   retro_time_t start;
   int64_t      took;

   mkdir(IIO_TEST_DIR, 0755);
   mkdir(DEV_DIR, 0755);
   write_lux("123.5\n");

   printf("open, read, follow a change\n");
   s = linux_open_illuminance_sensor(20);
   check(s != NULL, "the fake sensor is found");
   if (!s)
      return 1;
   check(linux_get_illuminance_reading(s) > 123.49f
      && linux_get_illuminance_reading(s) < 123.51f,
         "first reading is there at once");
   write_lux("50\n");
   check(wait_reading(s, 50.0f, 1000) < 300, "a change shows up at 20 Hz");

   printf("rate change applies at once\n");
   linux_set_illuminance_sensor_rate(s, 1);
   retro_sleep(100);                 /* now in a 1 s wait */
   write_lux("7\n");
   linux_set_illuminance_sensor_rate(s, 100);
   took = wait_reading(s, 7.0f, 2000);
   check(took < 200, "raising the rate from 1 Hz reads well before the 1 s wait ends");

   printf("close wakes the thread\n");
   linux_set_illuminance_sensor_rate(s, 1);
   retro_sleep(100);                 /* in a 1 s wait again */
   start = cpu_features_get_time_usec();
   linux_close_illuminance_sensor(s);
   took  = (int64_t)((cpu_features_get_time_usec() - start) / 1000);
   check(took < 100, "close at 1 Hz returns without waiting out the period");

   printf("no sensor\n");
   remove(DEV_FILE);
   rmdir(DEV_DIR);
   check(linux_open_illuminance_sensor(5) == NULL, "open reports none");
   rmdir(IIO_TEST_DIR);

   if (failures)
   {
      printf("\n%u failure(s)\n", failures);
      return 1;
   }
   printf("\nall passed\n");
   return 0;
}
