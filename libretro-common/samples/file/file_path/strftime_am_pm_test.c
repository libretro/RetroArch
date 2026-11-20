/* strftime_am_pm(): the designator for each half of the day, the
 * length it returns, a buffer too short, and repeated calls, which
 * take the locale once and allocate nothing.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

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
   size_t len;
   unsigned i;

   /* Hours 1 and 13 give the two designators; whatever the host's
    * locale calls them, they differ and follow the same clock */
   fill_tm(&tm, 13);
   len = strftime_am_pm(s, sizeof(s), "%I:%M %p", &tm);
   CHECK(len == strlen(s), "length %u for \"%s\"", (unsigned)len, s);
   CHECK(!strncmp(s, "01:05", 5), "13:05 on the 12-hour clock: \"%s\"", s);
   strcpy(first, s);
   fill_tm(&tm, 1);
   strftime_am_pm(s, sizeof(s), "%I:%M %p", &tm);
   CHECK(!strncmp(s, "01:05", 5), "01:05: \"%s\"", s);
   CHECK(strcmp(s, first) || !strchr(s, ' '), "AM and PM read the same: \"%s\"",
         s);

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
