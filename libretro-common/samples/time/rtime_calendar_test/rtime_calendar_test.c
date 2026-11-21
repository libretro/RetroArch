/* rtime's calendar reads: the clock against time(), the UTC and
 * standard offsets against fixed zones, and the zone name from TZ,
 * the /etc/localtime link, Android's zone property or Windows. Each
 * lane prints PASS or FAIL; the exit status is the failure count. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#if defined(ANDROID)
#include <sys/system_properties.h>
#endif

#include <boolean.h>
#include <compat/strl.h>
#include <time/rtime.h>

static int failures;

static void lane(const char *name, const char *fail)
{
   if (fail && *fail)
   {
      printf("FAIL %s: %s\n", name, fail);
      failures++;
   }
   else
      printf("PASS %s\n", name);
}

static bool name_shaped(const char *s)
{
   for (; *s; s++)
   {
      char c = *s;
      if (!(   (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9')
            || c == '_' || c == '+' || c == '-' || c == '/'))
         return false;
   }
   return true;
}

static void test_now(const char *label)
{
   char detail[128];
   int64_t s  = -1;
   int32_t ns = -1;
   time_t t0  = time(NULL);
   bool ok    = rtime_calendar_now(&s, &ns);
   time_t t1  = time(NULL);
   detail[0]  = '\0';
   /* time() may read a clock that trails the precise one by a tick,
    * so t1 can still show the second s has just left. */
   if (!ok)
      strcpy(detail, "no clock");
   else if (s < (int64_t)t0 || s > (int64_t)t1 + 1)
      sprintf(detail, "%ld not in [%ld, %ld]", (long)s, (long)t0, (long)t1);
   else if (ns < 0 || ns > 999999999)
      sprintf(detail, "nsec %ld", (long)ns);
   lane(label, detail);
}

static void test_host_invariants(const char *label)
{
   char name[64];
   char detail[160];
   char line[192];
   int64_t now = 0;
   int32_t ns  = 0;
   int32_t o;
   int32_t d;
   detail[0]   = '\0';
   rtime_calendar_now(&now, &ns);
   o = rtime_utc_offset(now);
   d = rtime_standard_offset(now);
   rtime_zone_name(name, sizeof(name));
   if (     o < -50400 || o > 50400 || o % 900
         || d < -50400 || d > 50400 || d % 900)
      sprintf(detail, "utc_offset %ld, standard_offset %ld",
            (long)o, (long)d);
   /* Never below standard time, and DST is at most 2 h. */
   else if (o - d < 0 || o - d > 7200)
      sprintf(detail, "utc_offset %ld is %ld from standard_offset %ld",
            (long)o, (long)(o - d), (long)d);
   else if (!name_shaped(name))
      sprintf(detail, "name \"%s\"", name);
   sprintf(line, "%s (utc_offset %ld, standard_offset %ld, name \"%s\")",
         label, (long)o, (long)d, name);
   lane(line, detail);
}

#if !defined(_WIN32)
struct offset_row
{
   const char *tz;
   int64_t sec;
   int32_t utc;
   int32_t std;
};

static const struct offset_row offset_rows[] = {
   { "Europe/London",          1768478400,      0,      0 },
   { "Europe/London",          1782907200,   3600,      0 },
   { "Europe/Dublin",          1768478400,      0,      0 },
   { "Europe/Dublin",          1782907200,   3600,      0 },
   { "America/New_York",       1768478400, -18000, -18000 },
   { "America/New_York",       1782907200, -14400, -18000 },
   { "Australia/Sydney",       1768478400,  39600,  36000 },
   { "Australia/Sydney",       1782907200,  36000,  36000 },
   { "Australia/Lord_Howe",    1768478400,  39600,  37800 },
   { "Pacific/Chatham",        1768478400,  49500,  45900 },
   { "Asia/Kolkata",           1782907200,  19800,  19800 },
   { "Pacific/Honolulu",       1767225600, -36000, -36000 },
   { "Pacific/Kiritimati",     1798718400,  50400,  50400 },
   { "JST-9",                  1782907200,  32400,  32400 },
   { "EST5EDT,M3.2.0,M11.1.0", 1782907200, -14400, -18000 },
   { "EST5EDT,M3.2.0,M11.1.0", 1768478400, -18000, -18000 }
};

static void test_offsets(void)
{
   size_t i;
   for (i = 0; i < sizeof(offset_rows) / sizeof(offset_rows[0]); i++)
   {
      char label[96];
      char detail[128];
      char path[128];
      const struct offset_row *r = &offset_rows[i];
      int32_t o;
      int32_t d;
      detail[0] = '\0';
      sprintf(label, "offsets %s at %ld", r->tz, (long)r->sec);
      if (strchr(r->tz, '/'))
      {
         FILE *f;
         sprintf(path, "/usr/share/zoneinfo/%s", r->tz);
         if (!(f = fopen(path, "rb")))
         {
            printf("SKIP %s: no %s\n", label, path);
            continue;
         }
         fclose(f);
      }
      setenv("TZ", r->tz, 1);
      o = rtime_utc_offset(r->sec);
      d = rtime_standard_offset(r->sec);
      if (o != r->utc || d != r->std)
         sprintf(detail, "utc_offset %ld, standard_offset %ld; want %ld, %ld",
               (long)o, (long)d, (long)r->utc, (long)r->std);
      lane(label, detail);
   }
}

static void check_name(const char *label, const char *want)
{
   char name[64];
   char detail[192];
   size_t n  = rtime_zone_name(name, sizeof(name));
   detail[0] = '\0';
   if (strcmp(name, want) || n != strlen(want))
      sprintf(detail, "\"%s\" (length %ld), want \"%s\"",
            name, (long)n, want);
   lane(label, detail);
}

struct name_row
{
   const char *tz;
   const char *want;
};

static const struct name_row name_rows[] = {
   { "Europe/London",                        "Europe/London"    },
   { ":America/New_York",                    "America/New_York" },
   { "/usr/share/zoneinfo/Asia/Tokyo",       "Asia/Tokyo"       },
   { "/usr/share/zoneinfo/posix/Asia/Tokyo", "Asia/Tokyo"       },
   { "UTC",                                  "UTC"              },
   { "JST-9",                                ""                 },
   { "EST5EDT,M3.2.0,M11.1.0",               ""                 },
   { "Europe/../etc/passwd",                 ""                 },
   { "",                                     "UTC"              }
};

static void test_zone_names(void)
{
   char label[96];
   char buf[8];
   char detail[96];
   char link[256];
   const char *want = "";
   size_t i;
   size_t n;
   ssize_t len;
   int32_t o;
   int32_t d;

   for (i = 0; i < sizeof(name_rows) / sizeof(name_rows[0]); i++)
   {
      sprintf(label, "zone name TZ=%s", name_rows[i].tz);
      setenv("TZ", name_rows[i].tz, 1);
      check_name(label, name_rows[i].want);
   }

   /* libc reads TZ set but empty as UTC, whatever the host zone. */
   detail[0] = '\0';
   setenv("TZ", "", 1);
   o = rtime_utc_offset(1782907200);
   d = rtime_standard_offset(1782907200);
   if (o || d)
      sprintf(detail, "utc_offset %ld, standard_offset %ld; want 0, 0",
            (long)o, (long)d);
   lane("offsets TZ empty", detail);

   /* The link by the path rule, read here on its own. */
   len = readlink("/etc/localtime", link, sizeof(link) - 1);
   if (len > 0)
   {
      const char *part;
      link[len] = '\0';
      if ((part = strstr(link, "zoneinfo/")))
      {
         part += strlen("zoneinfo/");
         if (!strncmp(part, "posix/", 6) || !strncmp(part, "right/", 6))
            part += 6;
         want = part;
      }
   }
   /* A path without zoneinfo/ is followed like the host link. */
   setenv("TZ", "/etc/localtime", 1);
   check_name("zone name TZ=/etc/localtime", want);
   setenv("TZ", ":/etc/localtime", 1);
   check_name("zone name TZ=:/etc/localtime", want);
#if !defined(ANDROID)
   unsetenv("TZ");
   sprintf(label, "zone name TZ unset, /etc/localtime");
   check_name(label, want);
#endif

   detail[0] = '\0';
   setenv("TZ", "Europe/London", 1);
   memset(buf, 'x', sizeof(buf));
   n = rtime_zone_name(buf, sizeof(buf));
   if (n || buf[0])
      sprintf(detail, "returned %ld, buf[0] 0x%02x", (long)n,
            (unsigned)(unsigned char)buf[0]);
   lane("zone name Europe/London into 8 bytes", detail);
}
#endif

#if defined(ANDROID)
static const char *prop_value = "";
static int prop_reads;
static bool prop_wrong_name;

int __system_property_get(const char *name, char *value)
{
   prop_reads++;
   if (strcmp(name, "persist.sys.timezone"))
   {
      prop_wrong_name = true;
      value[0]        = '\0';
      return 0;
   }
   return (int)strlcpy(value, prop_value, PROP_VALUE_MAX);
}

static void test_android_property(void)
{
   char detail[64];

   unsetenv("TZ");
   prop_value = "Asia/Tokyo";
   check_name("android property Asia/Tokyo, TZ unset", "Asia/Tokyo");
   prop_value = "";
   check_name("android property empty, TZ unset", "");

   detail[0]  = '\0';
   prop_value = "Asia/Tokyo";
   prop_reads = 0;
   setenv("TZ", "Europe/London", 1);
   check_name("android TZ=Europe/London over the property",
         "Europe/London");
   if (prop_reads)
      sprintf(detail, "property read %d times", prop_reads);
   lane("android property not read while TZ is set", detail);

   detail[0]  = '\0';
   prop_reads = 0;
   setenv("TZ", "", 1);
   check_name("android TZ empty over the property", "UTC");
   if (prop_reads)
      sprintf(detail, "property read %d times", prop_reads);
   lane("android property not read while TZ is empty", detail);

   lane("android property name", prop_wrong_name
         ? "asked for a property other than persist.sys.timezone" : NULL);
}
#endif

#if defined(_WIN32)
static void test_windows_tz(void)
{
   char name[64];
   char detail[160];
   int64_t now = 0;
   int32_t ns  = 0;
   int32_t o;
   int32_t d;
   size_t n;
   detail[0]   = '\0';
   rtime_calendar_now(&now, &ns);
   _putenv("TZ=JST-9");
   o = rtime_utc_offset(now);
   d = rtime_standard_offset(now);
   n = rtime_zone_name(name, sizeof(name));
   if (o != 32400 || d != 32400 || n || name[0])
      sprintf(detail, "utc_offset %ld, standard_offset %ld, name \"%s\"",
            (long)o, (long)d, name);
   lane("windows TZ=JST-9", detail);
   _putenv("TZ=");
   test_host_invariants("host invariants after TZ=");
}
#endif

/* The calendar arithmetic: known days, the leap rules, the months, and
 * every day of 110,000 years there and back */
static void test_gregorian(void)
{
   static const struct { int64_t y; unsigned m, d; int64_t days; unsigned wday; } known[] = {
      {  1970,  1,  1,       0, 4 },  /* a Thursday */
      {  1969, 12, 31,      -1, 3 },
      {  2000,  2, 29,   11016, 2 },
      {  2000,  3,  1,   11017, 3 },
      {  2026, 11, 21,   20778, 6 },  /* a Saturday */
      {  1600,  2, 29, -135081, 2 },
      {     1,  1,  1, -719162, 1 },  /* a Monday */
      {     0,  3,  1, -719468, 3 },
      {  9999, 12, 31, 2932896, 5 }
   };
   char     fail[160] = "";
   unsigned i;
   int64_t  days;

   for (i = 0; i < sizeof(known) / sizeof(known[0]) && !*fail; i++)
   {
      int64_t  y;
      unsigned m, d;
      if (rtime_days_from_civil(known[i].y, known[i].m, known[i].d) != known[i].days)
         snprintf(fail, sizeof(fail), "%d-%u-%u is not day %d", (int)known[i].y,
               known[i].m, known[i].d, (int)known[i].days);
      else if (rtime_weekday(known[i].days) != known[i].wday)
         snprintf(fail, sizeof(fail), "day %d is not weekday %u", (int)known[i].days,
               known[i].wday);
      rtime_civil_from_days(known[i].days, &y, &m, &d);
      if (!*fail && (y != known[i].y || m != known[i].m || d != known[i].d))
         snprintf(fail, sizeof(fail), "day %d is not %d-%u-%u", (int)known[i].days,
               (int)known[i].y, known[i].m, known[i].d);
   }
   if (!*fail && (!rtime_is_leap_year(2000) || rtime_is_leap_year(1900)
         || !rtime_is_leap_year(2024) || rtime_is_leap_year(2023)
         || !rtime_is_leap_year(0) || !rtime_is_leap_year(-4)))
      strlcpy(fail, "the leap rule", sizeof(fail));
   if (!*fail && (rtime_days_in_month(2024, 2) != 29 || rtime_days_in_month(2023, 2) != 28
         || rtime_days_in_month(2023, 4) != 30 || rtime_days_in_month(2023, 12) != 31
         || rtime_days_in_month(2023, 0) != 0 || rtime_days_in_month(2023, 13) != 0))
      strlcpy(fail, "the month lengths", sizeof(fail));

   /* there and back, the weekday one more each day, each date a real one */
   for (days = -20000000; days <= 20000000 && !*fail; days++)
   {
      int64_t  y;
      unsigned m, d;
      rtime_civil_from_days(days, &y, &m, &d);
      if (rtime_days_from_civil(y, m, d) != days || d < 1 || d > rtime_days_in_month(y, m))
         snprintf(fail, sizeof(fail), "day %d to %d-%u-%u and back", (int)days, (int)y, m, d);
      else if (rtime_weekday(days) != (rtime_weekday(days - 1) + 1) % 7)
         snprintf(fail, sizeof(fail), "the weekday of day %d", (int)days);
   }
   lane("Gregorian calendar", fail);
}

int main(void)
{
   test_now("now before rtime_init");
   rtime_init();
   test_now("now after rtime_init");
   test_host_invariants("host invariants");
#if defined(_WIN32)
   test_windows_tz();
#else
   test_gregorian();
   test_offsets();
   test_zone_names();
#endif
#if defined(ANDROID)
   test_android_property();
#endif
   rtime_deinit();
   return failures;
}
