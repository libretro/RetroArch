/* The functions RETRO_ENVIRONMENT_GET_CALENDAR_TIME_INTERFACE hands a
 * core (calendar_time.c): negotiation, the snapshot the callbacks read,
 * the held clock, a capture under a fixed TZ, and core threads reading
 * during a load while the main thread holds and releases the clock.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <time/rtime.h>

#include "calendar_time.h"

#define LOAD_READERS 3
#define LOAD_TOGGLES 200000L

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

static struct retro_calendar_time_interface cal;

typedef struct load_reader
{
   int64_t start_sec;
   unsigned long reads;
   unsigned long wrong;
   unsigned long current;
   unsigned long held;
} load_reader_t;

static retro_atomic_int_t load_started = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t load_stop    = RETRO_ATOMIC_INT_INITIALIZER(0);

static void test_negotiation(void)
{
   static const unsigned asked[] = { 1, 2, 7 };
   struct retro_calendar_time_interface iface;
   unsigned had = failures;
   unsigned i;

   CHECK(!calendar_time_get_interface(NULL), "NULL data accepted");

   memset(&iface, 0, sizeof(iface));
   CHECK(!calendar_time_get_interface(&iface), "version 0 accepted");
   CHECK(iface.interface_version == 0,
         "version 0 answered with %u", iface.interface_version);

   for (i = 0; i < sizeof(asked) / sizeof(asked[0]); i++)
   {
      memset(&iface, 0, sizeof(iface));
      iface.interface_version = asked[i];
      CHECK(calendar_time_get_interface(&iface),
            "version %u refused", asked[i]);
      CHECK(iface.interface_version == 1,
            "version %u answered with %u", asked[i], iface.interface_version);
      CHECK(iface.get_start_time && iface.get_current_time && iface.get_zone,
            "version %u: a callback is NULL", asked[i]);
   }

   if (failures == had)
      printf("PASS test_negotiation\n");
}

static void check_none(const char *when)
{
   struct retro_calendar_time t;
   struct retro_calendar_zone z;
   CHECK(!cal.get_start_time(&t), "%s: start time returned", when);
   CHECK(!cal.get_current_time(&t), "%s: current time returned", when);
   CHECK(!cal.get_zone(&z), "%s: zone returned", when);
}

static void test_empty(void)
{
   unsigned had = failures;

   check_none("before any capture");
   calendar_time_clear();
   check_none("after a clear");

   if (failures == had)
      printf("PASS test_empty\n");
}

static void test_published(void)
{
   struct retro_calendar_time start = { 1782907200, 123456789, 3600 };
   struct retro_calendar_zone zone  = { 0, "Europe/London" };
   struct retro_calendar_time t;
   struct retro_calendar_zone z;
   int64_t t0  = 0;
   int64_t t1  = 0;
   int32_t ns  = 0;
   bool ok;
   unsigned had = failures;

   calendar_time_publish(&start, &zone);

   memset(&t, 0xff, sizeof(t));
   CHECK(cal.get_start_time(&t), "no start time after a publish");
   CHECK(t.sec == 1782907200 && t.nsec == 123456789 && t.utc_offset == 3600,
         "start time %.0f.%09ld %+ld", (double)t.sec, (long)t.nsec,
         (long)t.utc_offset);

   memset(&z, 0, sizeof(z));
   CHECK(cal.get_zone(&z), "no zone after a publish");
   CHECK(!strcmp(z.name, "Europe/London") && z.standard_offset == 0,
         "zone \"%s\" %+ld", z.name, (long)z.standard_offset);

   rtime_calendar_now(&t0, &ns);
   memset(&t, 0xff, sizeof(t));
   ok = cal.get_current_time(&t);
   rtime_calendar_now(&t1, &ns);
   CHECK(ok, "no current time after a publish");
   CHECK(t.sec >= t0 && t.sec <= t1, "current time %.0f outside %.0f..%.0f",
         (double)t.sec, (double)t0, (double)t1);
   CHECK(t.nsec >= 0 && t.nsec <= 999999999, "nsec %ld", (long)t.nsec);
   CHECK(t.utc_offset == 3600, "current utc_offset %+ld",
         (long)t.utc_offset);

   if (failures == had)
      printf("PASS test_published\n");
}

static void test_held(void)
{
   struct retro_calendar_time t;
   unsigned had = failures;

   calendar_time_set_held(true);
   CHECK(!cal.get_current_time(&t), "current time while held");
   CHECK(cal.get_start_time(&t), "no start time while held");
   calendar_time_set_held(false);
   CHECK(cal.get_current_time(&t), "no current time once released");

   if (failures == had)
      printf("PASS test_held\n");
}

static void test_capture(void)
{
   static char tz_jst[] = "TZ=JST-9";
   struct retro_calendar_time t;
   struct retro_calendar_zone z;
   int64_t t0  = 0;
   int64_t t1  = 0;
   int32_t ns  = 0;
   unsigned had = failures;

   putenv(tz_jst);
   rtime_calendar_now(&t0, &ns);
   calendar_time_capture();
   rtime_calendar_now(&t1, &ns);

   memset(&t, 0xff, sizeof(t));
   CHECK(cal.get_start_time(&t), "no start time after a capture");
   CHECK(t.sec >= t0 && t.sec <= t1, "start time %.0f outside %.0f..%.0f",
         (double)t.sec, (double)t0, (double)t1);
   CHECK(t.nsec >= 0 && t.nsec <= 999999999, "nsec %ld", (long)t.nsec);
   CHECK(t.utc_offset == 32400, "utc_offset %+ld under JST-9",
         (long)t.utc_offset);

   memset(&z, 0, sizeof(z));
   CHECK(cal.get_zone(&z), "no zone after a capture");
   CHECK(z.standard_offset == 32400, "standard_offset %+ld under JST-9",
         (long)z.standard_offset);
   CHECK(z.name[0] == '\0', "zone name \"%s\" under JST-9", z.name);

   memset(&t, 0xff, sizeof(t));
   CHECK(cal.get_current_time(&t) && t.utc_offset == 32400,
         "current utc_offset %+ld under JST-9", (long)t.utc_offset);

   if (failures == had)
      printf("PASS test_capture\n");
}

static void test_clear(void)
{
   struct retro_calendar_time t;
   unsigned had = failures;

   calendar_time_clear();
   check_none("after a clear");
   calendar_time_capture();
   CHECK(cal.get_start_time(&t), "no start time after a capture");

   if (failures == had)
      printf("PASS test_clear\n");
}

static void load_reader_thread(void *data)
{
   load_reader_t *r = (load_reader_t*)data;

   retro_atomic_fetch_add_int(&load_started, 1);
   while (!retro_atomic_load_acquire_int(&load_stop))
   {
      struct retro_calendar_time t;
      struct retro_calendar_zone z;

      r->reads++;
      if (     !cal.get_start_time(&t)
            || t.sec != r->start_sec || t.utc_offset != 3600
            || !cal.get_zone(&z)
            || z.standard_offset != 0 || strcmp(z.name, "Europe/London"))
         r->wrong++;
      if (cal.get_current_time(&t))
      {
         r->current++;
         if (t.utc_offset != 3600)
            r->wrong++;
      }
      else
         r->held++;
   }
}

/* Between retro_load_game and retro_unload_game core threads read
 * while the main thread holds and releases the clock every frame. */
static void test_readers_during_load(void)
{
   struct retro_calendar_time start = { 1782907200, 5, 3600 };
   struct retro_calendar_zone zone  = { 0, "Europe/London" };
   sthread_t *threads[LOAD_READERS];
   load_reader_t readers[LOAD_READERS];
   unsigned long reads   = 0;
   unsigned long wrong   = 0;
   unsigned long current = 0;
   unsigned long held    = 0;
   int started           = 0;
   long k;
   unsigned i;
   unsigned had = failures;

   memset(readers, 0, sizeof(readers));
   calendar_time_set_held(false);
   calendar_time_publish(&start, &zone);

   for (i = 0; i < LOAD_READERS; i++)
   {
      readers[i].start_sec = start.sec;
      threads[i] = sthread_create(load_reader_thread, &readers[i]);
      CHECK(threads[i] != NULL, "reader %u not started", i);
      if (threads[i])
         started++;
   }

   while (retro_atomic_load_acquire_int(&load_started) < started)
      ;

   for (k = 0; k < LOAD_TOGGLES; k++)
      calendar_time_set_held((k & 1) ? true : false);

   retro_atomic_store_release_int(&load_stop, 1);
   for (i = 0; i < LOAD_READERS; i++)
      if (threads[i])
         sthread_join(threads[i]);
   calendar_time_set_held(false);
   calendar_time_clear();

   for (i = 0; i < LOAD_READERS; i++)
   {
      reads   += readers[i].reads;
      wrong   += readers[i].wrong;
      current += readers[i].current;
      held    += readers[i].held;
   }
   printf("test_readers_during_load: %u readers, %lu reads, %lu current, "
         "%lu held, %lu wrong\n", (unsigned)started, reads, current, held,
         wrong);

   CHECK(wrong == 0, "%lu wrong reads", wrong);
   CHECK(reads > 0, "no reads");

   if (failures == had)
      printf("PASS test_readers_during_load\n");
}

int main(void)
{
   rtime_init();

   test_negotiation();

   memset(&cal, 0, sizeof(cal));
   cal.interface_version = RETRO_CALENDAR_TIME_INTERFACE_VERSION;
   if (!calendar_time_get_interface(&cal))
   {
      fprintf(stderr, "FAIL calendar_time_test: no interface\n");
      return 1;
   }

   test_empty();
   test_published();
   test_held();
   test_capture();
   test_clear();
   test_readers_during_load();

   rtime_deinit();

   if (failures)
   {
      fprintf(stderr, "FAIL calendar_time_test: %u failures\n", failures);
      return (int)failures;
   }
   printf("PASS calendar_time_test\n");
   return 0;
}
