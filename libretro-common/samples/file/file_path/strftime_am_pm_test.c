/* strftime_am_pm(): the designator for each half of the day, the
 * length it returns, a buffer too short, and repeated calls, which
 * take the locale once and allocate nothing.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <locale.h>

#include <file/file_path.h>

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

static void fill_tm(struct tm *tm, int hour)
{
   memset(tm, 0, sizeof(*tm));
   tm->tm_year = 2026 - 1900;
   tm->tm_mon  = 10;
   tm->tm_mday = 20;
   tm->tm_hour = hour;
   tm->tm_min  = 5;
   tm->tm_sec  = 9;
}

int main(void)
{
   struct tm tm;
   char s[64];
   char first[64];
#ifndef _WIN32
   char want[64];
#endif
   char am[32];
   char pm[32];
   char taken[128];
   const char *name;
   size_t len;
   unsigned i;

   /* Hours 1 and 13 give the two designators on the 12-hour clock */
   fill_tm(&tm, 13);
   len = strftime_am_pm(s, sizeof(s), "%I:%M %p", &tm);
   CHECK(len == strlen(s), "length %u for \"%s\"", (unsigned)len, s);
   strcpy(first, s);
   fill_tm(&tm, 1);
   strftime_am_pm(s, sizeof(s), "%I:%M %p", &tm);

   /* The locale taken is the host's LC_TIME (C if that is not
    * installed), whose designators may be empty (de_DE, en_ZA) but are
    * never the same non-empty text */
   name     = setlocale(LC_TIME, NULL);
   taken[0] = '\0';
   if (name && strlen(name) < sizeof(taken))
      strcpy(taken, name);
   CHECK(*taken, "no LC_TIME name");
   name = setlocale(LC_TIME, "");
   CHECK(!strcmp(name ? name : "C", taken),
         "took LC_TIME \"%s\", host's is \"%s\"", taken,
         name ? name : "(none)");
   /* A leading 'x': an empty %p alone is strftime()'s 0, no text */
   strftime(am, sizeof(am), "x%p", &tm);
   fill_tm(&tm, 13);
   strftime(pm, sizeof(pm), "x%p", &tm);
   CHECK((!am[1] && !pm[1]) || strcmp(am, pm), "AM and PM are both \"%s\"",
         am + 1);
   if (!strcmp(taken, "C") || !strcmp(taken, "POSIX"))
      CHECK(!strcmp(am, "xAM") && !strcmp(pm, "xPM"),
            "C locale gave \"%s\" and \"%s\"", am + 1, pm + 1);
#ifdef _WIN32
   /* Windows' designators are in the codepage, the result in UTF-8 */
   CHECK(!strncmp(first, "01:05 ", 6) && !strncmp(s, "01:05 ", 6)
         && !strcmp(s, first) == !strcmp(am, pm),
         "\"%s\" and \"%s\"", first, s);
#else
   sprintf(want, "01:05 %s", pm + 1);
   CHECK(!strcmp(first, want), "13:05: \"%s\", want \"%s\"", first, want);
   sprintf(want, "01:05 %s", am + 1);
   CHECK(!strcmp(s, want), "01:05: \"%s\", want \"%s\"", s, want);
#endif

   /* Repeated calls give the same text */
   fill_tm(&tm, 23);
   strftime_am_pm(first, sizeof(first), "%Y-%m-%d %I:%M:%S %p", &tm);
   CHECK(!strncmp(first, "2026-11-20 11:05:09", 19), "\"%s\"", first);
   for (i = 0; i < 10000; i++)
   {
      strftime_am_pm(s, sizeof(s), "%Y-%m-%d %I:%M:%S %p", &tm);
      if (strcmp(s, first))
         break;
   }
   CHECK(i == 10000, "call %u gave \"%s\", the first \"%s\"", i, s, first);

   /* Too short: nothing past the buffer, an empty string, length 0 */
   memset(s, '#', sizeof(s));
   len = strftime_am_pm(s, 6, "%Y-%m-%d %I:%M:%S %p", &tm);
   CHECK(len == 0 && s[0] == '\0' && s[6] == '#',
         "short buffer: %u \"%.6s\"", (unsigned)len, s);

   if (failures)
   {
      fprintf(stderr, "FAIL strftime_am_pm_test: %u failures\n", failures);
      return 1;
   }
   printf("PASS strftime_am_pm_test\n");
   return 0;
}
