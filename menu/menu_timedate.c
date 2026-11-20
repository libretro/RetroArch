/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <locale.h>
#include <string.h>
#include <time.h>

#include <boolean.h>
#include <compat/strl.h>
#include <encodings/utf.h>
#include <time/rtime.h>

#include "menu_timedate.h"

/* Zones whose people write the month first: US states and territories,
 * the US-associated Pacific, and the Philippines. */
static const char *const tz_mdy[] = {
   "America/New_York",    "America/Chicago",     "America/Denver",
   "America/Phoenix",     "America/Los_Angeles", "America/Anchorage",
   "America/Juneau",      "America/Sitka",       "America/Metlakatla",
   "America/Nome",        "America/Yakutat",     "America/Adak",
   "America/Boise",       "America/Detroit",     "America/Menominee",
   "America/Fort_Wayne",  "America/Indianapolis","America/Knox_IN",
   "America/Louisville",  "America/Shiprock",
   "Pacific/Honolulu",    "Pacific/Johnston",    "Pacific/Guam",
   "Pacific/Saipan",      "Pacific/Pago_Pago",   "Pacific/Samoa",
   "Pacific/Midway",      "Pacific/Majuro",      "Pacific/Kwajalein",
   "Pacific/Chuuk",       "Pacific/Truk",        "Pacific/Yap",
   "Pacific/Pohnpei",     "Pacific/Ponape",      "Pacific/Kosrae",
   "Asia/Manila",
   "EST5EDT", "CST6CDT", "MST7MDT", "PST8PDT", "EST", "MST", "HST",
   "Navajo",  "Kwajalein"
};

static const char *const tz_mdy_prefix[] = {
   "America/Indiana/", "America/Kentucky/", "America/North_Dakota/", "US/"
};

/* Zones whose people write the year first. Canada's national standard
 * (CSA Z234.5) is year-month-day, the least surprising of the orders
 * in everyday use there. */
static const char *const tz_ymd[] = {
   "Asia/Shanghai",   "Asia/Chongqing", "Asia/Chungking", "Asia/Harbin",
   "Asia/Urumqi",     "Asia/Kashgar",   "Asia/Taipei",    "Asia/Tokyo",
   "Asia/Seoul",      "Asia/Pyongyang", "Asia/Ulaanbaatar",
   "Asia/Ulan_Bator", "Asia/Hovd",      "Asia/Choibalsan","Asia/Tehran",
   "Asia/Kathmandu",  "Asia/Katmandu",  "Asia/Thimphu",   "Asia/Thimbu",
   "Europe/Budapest", "Europe/Vilnius", "Europe/Stockholm",
   "PRC", "ROC", "ROK", "Japan", "Iran",
   "America/Toronto",     "America/Montreal",      "America/Vancouver",
   "America/Edmonton",    "America/Winnipeg",      "America/Halifax",
   "America/St_Johns",    "America/Regina",        "America/Moncton",
   "America/Glace_Bay",   "America/Goose_Bay",     "America/Iqaluit",
   "America/Whitehorse",  "America/Dawson",        "America/Dawson_Creek",
   "America/Yellowknife", "America/Inuvik",        "America/Cambridge_Bay",
   "America/Rankin_Inlet","America/Resolute",      "America/Swift_Current",
   "America/Thunder_Bay", "America/Nipigon",       "America/Rainy_River",
   "America/Atikokan",    "America/Blanc-Sablon",  "America/Creston",
   "America/Fort_Nelson", "America/Pangnirtung",   "America/Coral_Harbour"
};

/* Zones tied to no place, which say nothing about how dates are
 * written. */
static const char *const tz_none[] = {
   "UTC", "UCT", "GMT", "Universal", "Zulu", "Greenwich"
};

#define TZ_COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* One strftime() format per style and date separator (hyphen, slash,
 * period); the AM/PM styles get their designator appended. */
static const char *const menu_timedate_formats
      [MENU_TIMEDATE_STYLE_LAST][MENU_TIMEDATE_DATE_SEPARATOR_LAST] = {
   /* YMD_HMS           */ { "%Y-%m-%d %H:%M:%S", "%Y/%m/%d %H:%M:%S", "%Y.%m.%d %H:%M:%S" },
   /* YMD_HM            */ { "%Y-%m-%d %H:%M", "%Y/%m/%d %H:%M", "%Y.%m.%d %H:%M" },
   /* YMD               */ { "%Y-%m-%d", "%Y/%m/%d", "%Y.%m.%d" },
   /* YM                */ { "%Y-%m", "%Y/%m", "%Y.%m" },
   /* MDYYYY_HMS        */ { "%m-%d-%Y %H:%M:%S", "%m/%d/%Y %H:%M:%S", "%m.%d.%Y %H:%M:%S" },
   /* MDYYYY_HM         */ { "%m-%d-%Y %H:%M", "%m/%d/%Y %H:%M", "%m.%d.%Y %H:%M" },
   /* MD_HM             */ { "%m-%d %H:%M", "%m/%d %H:%M", "%m.%d %H:%M" },
   /* MDYYYY            */ { "%m-%d-%Y", "%m/%d/%Y", "%m.%d.%Y" },
   /* MD                */ { "%m-%d", "%m/%d", "%m.%d" },
   /* DDMMYYYY_HMS      */ { "%d-%m-%Y %H:%M:%S", "%d/%m/%Y %H:%M:%S", "%d.%m.%Y %H:%M:%S" },
   /* DDMMYYYY_HM       */ { "%d-%m-%Y %H:%M", "%d/%m/%Y %H:%M", "%d.%m.%Y %H:%M" },
   /* DDMM_HM           */ { "%d-%m %H:%M", "%d/%m %H:%M", "%d.%m %H:%M" },
   /* DDMMYYYY          */ { "%d-%m-%Y", "%d/%m/%Y", "%d.%m.%Y" },
   /* DDMM              */ { "%d-%m", "%d/%m", "%d.%m" },
   /* HMS               */ { "%H:%M:%S", "%H:%M:%S", "%H:%M:%S" },
   /* HM                */ { "%H:%M", "%H:%M", "%H:%M" },
   /* YMD_HMS_AMPM      */ { "%Y-%m-%d %I:%M:%S", "%Y/%m/%d %I:%M:%S", "%Y.%m.%d %I:%M:%S" },
   /* YMD_HM_AMPM       */ { "%Y-%m-%d %I:%M", "%Y/%m/%d %I:%M", "%Y.%m.%d %I:%M" },
   /* MDYYYY_HMS_AMPM   */ { "%m-%d-%Y %I:%M:%S", "%m/%d/%Y %I:%M:%S", "%m.%d.%Y %I:%M:%S" },
   /* MDYYYY_HM_AMPM    */ { "%m-%d-%Y %I:%M", "%m/%d/%Y %I:%M", "%m.%d.%Y %I:%M" },
   /* MD_HM_AMPM        */ { "%m-%d %I:%M", "%m/%d %I:%M", "%m.%d %I:%M" },
   /* DDMMYYYY_HMS_AMPM */ { "%d-%m-%Y %I:%M:%S", "%d/%m/%Y %I:%M:%S", "%d.%m.%Y %I:%M:%S" },
   /* DDMMYYYY_HM_AMPM  */ { "%d-%m-%Y %I:%M", "%d/%m/%Y %I:%M", "%d.%m.%Y %I:%M" },
   /* DDMM_HM_AMPM      */ { "%d-%m %I:%M", "%d/%m %I:%M", "%d.%m %I:%M" },
   /* HMS_AMPM          */ { "%I:%M:%S", "%I:%M:%S", "%I:%M:%S" },
   /* HM_AMPM           */ { "%I:%M", "%I:%M", "%I:%M" },
};

/* The designators for hours 0-11 and 12-23, UTF-8. Written once on
 * the main thread before any clock is drawn. */
static char menu_timedate_ampm[2][32] = { "AM", "PM" };

static bool tz_in(const char *tz, const char *const *list, size_t count)
{
   size_t i;
   for (i = 0; i < count; i++)
      if (!strcmp(tz, list[i]))
         return true;
   return false;
}

static bool tz_under(const char *tz, const char *const *list, size_t count)
{
   size_t i;
   for (i = 0; i < count; i++)
      if (!strncmp(tz, list[i], strlen(list[i])))
         return true;
   return false;
}

enum menu_timedate_date_order menu_timedate_zone_order(const char *tz)
{
   if (!tz || !*tz)
      return MENU_TIMEDATE_DATE_ORDER_UNKNOWN;
   if (     !strncmp(tz, "Etc/", 4)
         || tz_in(tz, tz_none, TZ_COUNT(tz_none)))
      return MENU_TIMEDATE_DATE_ORDER_UNKNOWN;
   if (     tz_in(tz, tz_mdy, TZ_COUNT(tz_mdy))
         || tz_under(tz, tz_mdy_prefix, TZ_COUNT(tz_mdy_prefix)))
      return MENU_TIMEDATE_DATE_ORDER_MDY;
   if (     tz_in(tz, tz_ymd, TZ_COUNT(tz_ymd))
         || !strncmp(tz, "Canada/", 7))
      return MENU_TIMEDATE_DATE_ORDER_YMD;
   return MENU_TIMEDATE_DATE_ORDER_DMY;
}

enum menu_timedate_date_order menu_timedate_host_order(void)
{
   char tz[64];
   if (!rtime_zone_name(tz, sizeof(tz)))
      return MENU_TIMEDATE_DATE_ORDER_UNKNOWN;
   return menu_timedate_zone_order(tz);
}

void menu_timedate_capture_ampm(void)
{
   static bool captured = false;
   char local[32];
   struct tm tm;
   unsigned i;

   if (captured)
      return;
   captured = true;

   setlocale(LC_TIME, "");
   memset(&tm, 0, sizeof(tm));
   tm.tm_mday = 1;
   tm.tm_year = 100;
   for (i = 0; i < 2; i++)
   {
      tm.tm_hour = i ? 13 : 1;
      if (!strftime(local, sizeof(local), "%p", &tm) || !*local)
         continue;
#if defined(__linux__) && !defined(ANDROID)
      strlcpy(menu_timedate_ampm[i], local, sizeof(menu_timedate_ampm[i]));
#else
      {
         char utf8[sizeof(menu_timedate_ampm[i])];
         if (local_to_utf8_string(local, utf8, sizeof(utf8)) && *utf8)
            strlcpy(menu_timedate_ampm[i], utf8,
                  sizeof(menu_timedate_ampm[i]));
      }
#endif
   }
}

size_t menu_timedate_format(char *s, size_t len, unsigned style,
      unsigned separator, const struct tm *tm)
{
   size_t _len;
   if (!len)
      return 0;
   *s = '\0';
   if (style >= MENU_TIMEDATE_STYLE_LAST)
      return 0;
   if (separator >= MENU_TIMEDATE_DATE_SEPARATOR_LAST)
      separator = MENU_TIMEDATE_DATE_SEPARATOR_HYPHEN;
   /* A buffer too short leaves strftime()'s output unspecified */
   if (!(_len = strftime(s, len,
         menu_timedate_formats[style][separator], tm)))
   {
      *s = '\0';
      return 0;
   }
   if (style >= MENU_TIMEDATE_STYLE_YMD_HMS_AMPM && _len + 1 < len)
   {
      s[_len++] = ' ';
      _len     += strlcpy(s + _len,
            menu_timedate_ampm[tm->tm_hour >= 12], len - _len);
      if (_len >= len)
         _len = len - 1;
   }
   return _len;
}
