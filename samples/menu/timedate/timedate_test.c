/* The menu clock (menu/menu_timedate.c): the date order a time zone
 * gives, the style Automatic and Manual draw, every style's text with
 * each separator, the AM/PM designators, short buffers, and the host
 * zone read through rtime.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <boolean.h>
#include <time/rtime.h>

#include "menu/menu_timedate.h"

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

static const char *order_name(unsigned order)
{
   switch (order)
   {
      case MENU_TIMEDATE_DATE_ORDER_DMY:
         return "DMY";
      case MENU_TIMEDATE_DATE_ORDER_MDY:
         return "MDY";
      case MENU_TIMEDATE_DATE_ORDER_YMD:
         return "YMD";
      default:
         break;
   }
   return "unknown";
}

static void test_zone_order(void)
{
   static const struct
   {
      const char *tz;
      unsigned order;
   } cases[] = {
      { "Europe/Amsterdam",               MENU_TIMEDATE_DATE_ORDER_DMY },
      { "America/Mexico_City",            MENU_TIMEDATE_DATE_ORDER_DMY },
      { "Australia/Sydney",               MENU_TIMEDATE_DATE_ORDER_DMY },
      { "America/New_York",               MENU_TIMEDATE_DATE_ORDER_MDY },
      { "America/Indiana/Knox",           MENU_TIMEDATE_DATE_ORDER_MDY },
      { "America/Kentucky/Louisville",    MENU_TIMEDATE_DATE_ORDER_MDY },
      { "US/Pacific",                     MENU_TIMEDATE_DATE_ORDER_MDY },
      { "Asia/Manila",                    MENU_TIMEDATE_DATE_ORDER_MDY },
      { "EST5EDT",                        MENU_TIMEDATE_DATE_ORDER_MDY },
      { "Asia/Tokyo",                     MENU_TIMEDATE_DATE_ORDER_YMD },
      { "Europe/Stockholm",               MENU_TIMEDATE_DATE_ORDER_YMD },
      { "America/Toronto",                MENU_TIMEDATE_DATE_ORDER_YMD },
      { "Canada/Eastern",                 MENU_TIMEDATE_DATE_ORDER_YMD },
      { "UTC",                            MENU_TIMEDATE_DATE_ORDER_UNKNOWN },
      { "Etc/GMT+3",                      MENU_TIMEDATE_DATE_ORDER_UNKNOWN },
      { "Zulu",                           MENU_TIMEDATE_DATE_ORDER_UNKNOWN },
      { "",                               MENU_TIMEDATE_DATE_ORDER_UNKNOWN }
   };
   unsigned had = failures;
   unsigned i;

   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      unsigned got = menu_timedate_zone_order(cases[i].tz);
      CHECK(got == cases[i].order, "\"%s\": %s, wanted %s", cases[i].tz,
            order_name(got), order_name(cases[i].order));
   }
   CHECK(menu_timedate_zone_order(NULL) == MENU_TIMEDATE_DATE_ORDER_UNKNOWN,
         "NULL zone has an order");

   if (failures == had)
      printf("PASS test_zone_order\n");
}

static void test_resolve(void)
{
   unsigned had = failures;
   unsigned style;
   unsigned order;

   /* Manual draws the chosen style whatever the zone and the 12-hour
    * option say */
   for (style = 0; style < MENU_TIMEDATE_STYLE_LAST; style++)
      for (order = 0; order <= MENU_TIMEDATE_DATE_ORDER_YMD; order++)
      {
         CHECK(MENU_TIMEDATE_RESOLVE_STYLE(MENU_TIMEDATE_MODE_MANUAL,
                  style, true, order) == style,
               "manual, style %u, order %u", style, order);
         CHECK(MENU_TIMEDATE_RESOLVE_STYLE(MENU_TIMEDATE_MODE_MANUAL,
                  style, false, order) == style,
               "manual 24h, style %u, order %u", style, order);
      }

   /* Automatic: no year, day first only where the zone writes it so */
   CHECK(MENU_TIMEDATE_RESOLVE_STYLE(MENU_TIMEDATE_MODE_AUTOMATIC,
         MENU_TIMEDATE_STYLE_YMD_HMS, false, MENU_TIMEDATE_DATE_ORDER_DMY)
         == MENU_TIMEDATE_STYLE_DDMM_HM, "automatic DMY 24h");
   CHECK(MENU_TIMEDATE_RESOLVE_STYLE(MENU_TIMEDATE_MODE_AUTOMATIC,
         MENU_TIMEDATE_STYLE_YMD_HMS, true, MENU_TIMEDATE_DATE_ORDER_DMY)
         == MENU_TIMEDATE_STYLE_DDMM_HM_AMPM, "automatic DMY 12h");
   for (order = 0; order <= MENU_TIMEDATE_DATE_ORDER_YMD; order++)
   {
      if (order == MENU_TIMEDATE_DATE_ORDER_DMY)
         continue;
      CHECK(MENU_TIMEDATE_RESOLVE_STYLE(MENU_TIMEDATE_MODE_AUTOMATIC,
            MENU_TIMEDATE_STYLE_HM, false, order)
            == MENU_TIMEDATE_STYLE_MD_HM, "automatic %s 24h",
            order_name(order));
      CHECK(MENU_TIMEDATE_RESOLVE_STYLE(MENU_TIMEDATE_MODE_AUTOMATIC,
            MENU_TIMEDATE_STYLE_HM, true, order)
            == MENU_TIMEDATE_STYLE_MD_HM_AMPM, "automatic %s 12h",
            order_name(order));
   }

   if (failures == had)
      printf("PASS test_resolve\n");
}

static void fill_tm(struct tm *tm, int hour)
{
   memset(tm, 0, sizeof(*tm));
   tm->tm_year = 2026 - 1900;
   tm->tm_mon  = 10;
   tm->tm_mday = 20;
   tm->tm_hour = hour;
   tm->tm_min  = 5;
   tm->tm_sec  = 9;
   tm->tm_wday = 5;
   tm->tm_yday = 323;
}

static void test_format(void)
{
   /* Each style at 17:05:09 on 2026-11-20, hyphen separated */
   static const char *const want[MENU_TIMEDATE_STYLE_LAST] = {
      "2026-11-20 17:05:09", "2026-11-20 17:05", "2026-11-20", "2026-11",
      "11-20-2026 17:05:09", "11-20-2026 17:05", "11-20 17:05",
      "11-20-2026", "11-20",
      "20-11-2026 17:05:09", "20-11-2026 17:05", "20-11 17:05",
      "20-11-2026", "20-11",
      "17:05:09", "17:05",
      "2026-11-20 05:05:09 PM", "2026-11-20 05:05 PM",
      "11-20-2026 05:05:09 PM", "11-20-2026 05:05 PM", "11-20 05:05 PM",
      "20-11-2026 05:05:09 PM", "20-11-2026 05:05 PM", "20-11 05:05 PM",
      "05:05:09 PM", "05:05 PM"
   };
   struct tm tm;
   char s[64];
   size_t len;
   unsigned had = failures;
   unsigned style;

   fill_tm(&tm, 17);
   for (style = 0; style < MENU_TIMEDATE_STYLE_LAST; style++)
   {
      len = menu_timedate_format(s, sizeof(s), style,
            MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN, &tm);
      CHECK(!strcmp(s, want[style]) && len == strlen(want[style]),
            "style %u: \"%s\" (%u), wanted \"%s\"", style, s,
            (unsigned)len, want[style]);
   }

   len = menu_timedate_format(s, sizeof(s), MENU_TIMEDATE_STYLE_YMD_HM,
         MENU_TIMEDATE_DATE_SEPARATOR_SLASH, &tm);
   CHECK(!strcmp(s, "2026/11/20 17:05"), "slash: \"%s\"", s);
   len = menu_timedate_format(s, sizeof(s), MENU_TIMEDATE_STYLE_DDMMYYYY,
         MENU_TIMEDATE_DATE_SEPARATOR_PERIOD, &tm);
   CHECK(!strcmp(s, "20.11.2026"), "period: \"%s\"", s);
   len = menu_timedate_format(s, sizeof(s), MENU_TIMEDATE_STYLE_MD,
         MENU_TIMEDATE_DATE_SEPARATOR_LAST + 4, &tm);
   CHECK(!strcmp(s, "11-20"), "bad separator: \"%s\"", s);

   fill_tm(&tm, 5);
   menu_timedate_format(s, sizeof(s), MENU_TIMEDATE_STYLE_HM_AMPM,
         MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN, &tm);
   CHECK(!strcmp(s, "05:05 AM"), "morning: \"%s\"", s);
   fill_tm(&tm, 0);
   menu_timedate_format(s, sizeof(s), MENU_TIMEDATE_STYLE_HM_AMPM,
         MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN, &tm);
   CHECK(!strcmp(s, "12:05 AM"), "midnight: \"%s\"", s);
   fill_tm(&tm, 12);
   menu_timedate_format(s, sizeof(s), MENU_TIMEDATE_STYLE_HM_AMPM,
         MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN, &tm);
   CHECK(!strcmp(s, "12:05 PM"), "noon: \"%s\"", s);

   /* An unknown style is empty, not whatever the buffer held */
   memset(s, 'x', sizeof(s));
   len = menu_timedate_format(s, sizeof(s), MENU_TIMEDATE_STYLE_LAST,
         MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN, &tm);
   CHECK(len == 0 && s[0] == '\0', "unknown style: %u \"%.8s\"",
         (unsigned)len, s);

   if (failures == had)
      printf("PASS test_format\n");
}

/* Short buffers: never written past, always terminated, the length
 * never more than what is there */
static void test_short_buffers(void)
{
   struct tm tm;
   char s[32];
   size_t cap;
   unsigned had = failures;

   fill_tm(&tm, 17);
   for (cap = 1; cap < 24; cap++)
   {
      size_t len;
      memset(s, '#', sizeof(s));
      len = menu_timedate_format(s, cap,
            MENU_TIMEDATE_STYLE_YMD_HMS_AMPM,
            MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN, &tm);
      CHECK(s[cap] == '#', "cap %u: written past", (unsigned)cap);
      CHECK(memchr(s, '\0', cap) != NULL, "cap %u: not terminated",
            (unsigned)cap);
      CHECK(len == strlen(s), "cap %u: length %u for \"%s\"",
            (unsigned)cap, (unsigned)len, s);
   }
   CHECK(menu_timedate_format(s, 0, MENU_TIMEDATE_STYLE_HM,
         MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN, &tm) == 0, "zero buffer");

   if (failures == had)
      printf("PASS test_short_buffers\n");
}

#if !defined(_WIN32)
static void with_tz(const char *tz, unsigned want)
{
   unsigned got;
   setenv("TZ", tz, 1);
   got = menu_timedate_host_order();
   CHECK(got == want, "TZ=%s: %s, wanted %s", tz, order_name(got),
         order_name(want));
}
#endif

/* The host's zone, through rtime_zone_name(): a name gives its order,
 * a POSIX rule names no place */
static void test_host_order(void)
{
   unsigned had = failures;
#if !defined(_WIN32)
   with_tz("Europe/Berlin",         MENU_TIMEDATE_DATE_ORDER_DMY);
   with_tz(":America/Chicago",      MENU_TIMEDATE_DATE_ORDER_MDY);
   with_tz("Asia/Seoul",            MENU_TIMEDATE_DATE_ORDER_YMD);
   with_tz("JST-9",                 MENU_TIMEDATE_DATE_ORDER_UNKNOWN);
   with_tz("EST5EDT,M3.2.0,M11.1.0",MENU_TIMEDATE_DATE_ORDER_UNKNOWN);
   with_tz("",                      MENU_TIMEDATE_DATE_ORDER_UNKNOWN);
   unsetenv("TZ");
#else
   /* The Windows zone needs no environment; only that it answers */
   CHECK(menu_timedate_host_order() <= MENU_TIMEDATE_DATE_ORDER_YMD,
         "host order out of range");
#endif
   if (failures == had)
      printf("PASS test_host_order\n");
}

/* The designators are taken once; the C locale has AM and PM */
static void test_capture(void)
{
   struct tm tm;
   char s[64];
   unsigned had = failures;

   menu_timedate_capture_ampm();
   menu_timedate_capture_ampm();
   fill_tm(&tm, 23);
   menu_timedate_format(s, sizeof(s), MENU_TIMEDATE_STYLE_HM_AMPM,
         MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN, &tm);
   CHECK(strlen(s) > 6 && !strncmp(s, "11:05 ", 6), "after capture: \"%s\"",
         s);

   if (failures == had)
      printf("PASS test_capture\n");
}

int main(void)
{
   rtime_init();

   test_zone_order();
   test_resolve();
   test_format();
   test_short_buffers();
   test_host_order();
   test_capture();

   rtime_deinit();

   if (failures)
   {
      fprintf(stderr, "FAIL timedate_test: %u failures\n", failures);
      return 1;
   }
   printf("PASS timedate_test\n");
   return 0;
}
