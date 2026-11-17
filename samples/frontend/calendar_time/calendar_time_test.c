/* The functions RETRO_ENVIRONMENT_GET_CALENDAR_TIME_INTERFACE hands a
 * core (calendar_time.c): negotiation, the snapshot the callbacks read,
 * the held clock, a capture under a fixed TZ, and core threads reading
 * while the main thread publishes, which only ever see whole snapshots.
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

#define TORN_READERS   3
#define TORN_PUBLISHES 200000L

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

typedef struct torn_reader
{
   unsigned long copies;
   unsigned long torn;
   unsigned long backwards;
   unsigned long distinct;
} torn_reader_t;

static retro_atomic_int_t torn_started = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t torn_stop    = RETRO_ATOMIC_INT_INITIALIZER(0);

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
   struct retro_calendar_zone zone  = { "Europe/London", 0 };
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

static void torn_reader_thread(void *data)
{
   torn_reader_t *r = (torn_reader_t*)data;
   int64_t last_start = 0;
   long last_zone     = 0;

   retro_atomic_fetch_add_int(&torn_started, 1);
   while (!retro_atomic_load_acquire_int(&torn_stop))
   {
      struct retro_calendar_time t;
      struct retro_calendar_zone z;

      if (cal.get_start_time(&t))
      {
         int64_t k = t.sec;
         r->copies++;
         if (     t.nsec       != (int32_t)(k % 1000000000)
               || t.utc_offset != (int32_t)-(k % 50400))
            r->torn++;
         else if (k < last_start)
            r->backwards++;
         else if (k != last_start)
         {
            r->distinct++;
            last_start = k;
         }
      }

      if (cal.get_zone(&z))
      {
         char *end;
         long k;
         r->copies++;
         z.name[sizeof(z.name) - 1] = '\0';
         k = strtol(z.name, &end, 10);
         if (     end == z.name
               || *end
               || z.standard_offset != (int32_t)(k % 50400))
            r->torn++;
         else if (k < last_zone)
            r->backwards++;
         else
            last_zone = k;
      }
   }
}

static void test_readers_never_torn(void)
{
   sthread_t *threads[TORN_READERS];
   torn_reader_t readers[TORN_READERS];
   struct retro_calendar_time start;
   struct retro_calendar_zone zone;
   unsigned long copies    = 0;
   unsigned long torn      = 0;
   unsigned long backwards = 0;
   unsigned long fewest    = (unsigned long)-1;
   int started             = 0;
   long k;
   unsigned i;
   unsigned had = failures;

   memset(readers, 0, sizeof(readers));
   memset(&zone, 0, sizeof(zone));
   calendar_time_clear();

   for (i = 0; i < TORN_READERS; i++)
   {
      threads[i] = sthread_create(torn_reader_thread, &readers[i]);
      CHECK(threads[i] != NULL, "reader %u not started", i);
      if (threads[i])
         started++;
   }

   /* Every reader is in its loop before the first publish. */
   while (retro_atomic_load_acquire_int(&torn_started) < started)
      ;

   for (k = 1; k <= TORN_PUBLISHES; k++)
   {
      start.sec            = k;
      start.nsec           = (int32_t)(k % 1000000000);
      start.utc_offset     = (int32_t)-(k % 50400);
      sprintf(zone.name, "%ld", k);
      zone.standard_offset = (int32_t)(k % 50400);
      calendar_time_publish(&start, &zone);
   }

   retro_atomic_store_release_int(&torn_stop, 1);
   for (i = 0; i < TORN_READERS; i++)
      if (threads[i])
         sthread_join(threads[i]);

   for (i = 0; i < TORN_READERS; i++)
   {
      copies    += readers[i].copies;
      torn      += readers[i].torn;
      backwards += readers[i].backwards;
      if (readers[i].distinct < fewest)
         fewest = readers[i].distinct;
   }
   printf("test_readers_never_torn: %u readers, %lu copies, %lu torn, "
         "%lu backwards, fewest distinct %lu\n",
         (unsigned)started, copies, torn, backwards, fewest);

   CHECK(torn == 0, "%lu torn copies", torn);
   CHECK(backwards == 0, "%lu copies went backwards", backwards);
   CHECK(fewest >= 2, "a reader saw only %lu distinct snapshots", fewest);

   if (failures == had)
      printf("PASS test_readers_never_torn\n");
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
   test_readers_never_torn();

   rtime_deinit();

   if (failures)
   {
      fprintf(stderr, "FAIL calendar_time_test: %u failures\n", failures);
      return (int)failures;
   }
   printf("PASS calendar_time_test\n");
   return 0;
}
