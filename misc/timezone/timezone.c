/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - gavin_darkglider
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

#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#if defined(ANDROID)
#include <sys/system_properties.h>
#elif defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && !defined(WINAPI_FAMILY)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
      || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__HAIKU__)
#include <unistd.h>
#endif

#include "timezone.h"

/* Zones whose people write month before day. US states and
 * territories, the US-associated Pacific, and the Philippines. */
static const char *tz_mdy[] = {
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

static const char *tz_mdy_prefix[] = {
   "America/Indiana/", "America/Kentucky/", "America/North_Dakota/", "US/"
};

/* Zones whose people write year first. */
static const char *tz_ymd[] = {
   "Asia/Shanghai",   "Asia/Chongqing", "Asia/Chungking", "Asia/Harbin",
   "Asia/Urumqi",     "Asia/Kashgar",   "Asia/Taipei",    "Asia/Tokyo",
   "Asia/Seoul",      "Asia/Pyongyang", "Asia/Ulaanbaatar",
   "Asia/Ulan_Bator", "Asia/Hovd",      "Asia/Choibalsan","Asia/Tehran",
   "Asia/Kathmandu",  "Asia/Katmandu",  "Asia/Thimphu",   "Asia/Thimbu",
   "Europe/Budapest", "Europe/Vilnius", "Europe/Stockholm",
   "PRC", "ROC", "ROK", "Japan", "Iran"
};

/* Canada's national standard (CSA Z234.5) is year-month-day;
 * in everyday use both other orders appear, so the standard
 * is the least surprising pick. */
static const char *tz_ymd_prefix[] = {
   "Canada/"
};

static const char *tz_canada[] = {
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

#define TZ_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && !defined(WINAPI_FAMILY)
/* Windows time zone key names (not localized) whose people write the
 * month first, or the year first. The US zones are shared with Canada
 * and Mexico; the larger population decides. */
static const char *tz_win_mdy[] = {
   "Eastern Standard Time",  "US Eastern Standard Time",
   "Central Standard Time",  "Mountain Standard Time",
   "US Mountain Standard Time", "Pacific Standard Time",
   "Alaskan Standard Time",  "Aleutian Standard Time",
   "Hawaiian Standard Time", "West Pacific Standard Time"
};

static const char *tz_win_ymd[] = {
   "China Standard Time",  "Tokyo Standard Time", "Korea Standard Time",
   "North Korea Standard Time", "Taipei Standard Time",
   "Ulaanbaatar Standard Time", "Iran Standard Time",
   "Nepal Standard Time"
};
#endif

static bool tz_in_list(const char *tz, const char **list, size_t count)
{
   size_t i;
   for (i = 0; i < count; i++)
      if (string_is_equal(tz, list[i]))
         return true;
   return false;
}

static bool tz_has_prefix(const char *tz, const char **list, size_t count)
{
   size_t i;
   for (i = 0; i < count; i++)
      if (!strncmp(tz, list[i], strlen(list[i])))
         return true;
   return false;
}

enum timezone_date_order timezone_get_date_order(const char *tz)
{
   if (string_is_empty(tz))
      return TIMEZONE_DATE_ORDER_UNKNOWN;

   /* Accept the TZ ":" form too */
   if (*tz == ':')
      tz++;

   /* Zones that belong to no place say nothing about how dates are
    * written there */
   if (     string_starts_with(tz, "Etc/")
         || string_is_equal(tz, "UTC")
         || string_is_equal(tz, "UCT")
         || string_is_equal(tz, "GMT")
         || string_is_equal(tz, "Universal")
         || string_is_equal(tz, "Zulu")
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && !defined(WINAPI_FAMILY)
         || string_starts_with(tz, "UTC")
#endif
         )
      return TIMEZONE_DATE_ORDER_UNKNOWN;

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && !defined(WINAPI_FAMILY)
   if (tz_in_list(tz, tz_win_mdy, TZ_COUNT(tz_win_mdy)))
      return TIMEZONE_DATE_ORDER_MDY;
   if (tz_in_list(tz, tz_win_ymd, TZ_COUNT(tz_win_ymd)))
      return TIMEZONE_DATE_ORDER_YMD;
#endif

   if (     tz_in_list(tz, tz_mdy, TZ_COUNT(tz_mdy))
         || tz_has_prefix(tz, tz_mdy_prefix, TZ_COUNT(tz_mdy_prefix)))
      return TIMEZONE_DATE_ORDER_MDY;

   if (     tz_in_list(tz, tz_ymd, TZ_COUNT(tz_ymd))
         || tz_in_list(tz, tz_canada, TZ_COUNT(tz_canada))
         || tz_has_prefix(tz, tz_ymd_prefix, TZ_COUNT(tz_ymd_prefix)))
      return TIMEZONE_DATE_ORDER_YMD;

   return TIMEZONE_DATE_ORDER_DMY;
}


bool timezone_get_system(char *s, size_t len)
{
   const char *tz;

   if (!s || !len)
      return false;
   *s = '\0';

   /* An explicit TZ wins on every platform that honours it */
   if (!string_is_empty(tz = getenv("TZ")))
   {
      const char *zi;
      if (*tz == ':')
         tz++;
      /* TZ may be a file path into the zoneinfo tree */
      if ((zi = strstr(tz, "zoneinfo/")))
         tz = zi + STRLEN_CONST("zoneinfo/");
      strlcpy(s, tz, len);
      return true;
   }

#if defined(ANDROID)
   {
      char prop[PROP_VALUE_MAX];
      if (__system_property_get("persist.sys.timezone", prop) > 0)
      {
         strlcpy(s, prop, len);
         return true;
      }
   }
#elif defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && !defined(WINAPI_FAMILY)
#if _WIN32_WINNT >= 0x0600
   {
      DYNAMIC_TIME_ZONE_INFORMATION dtzi;
      if (GetDynamicTimeZoneInformation(&dtzi) != TIME_ZONE_ID_INVALID)
      {
         WideCharToMultiByte(CP_UTF8, 0, dtzi.TimeZoneKeyName, -1,
               s, (int)len, NULL, NULL);
         return !string_is_empty(s);
      }
   }
#endif
#elif (defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
      || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__HAIKU__)) \
      && !defined(__SWITCH__)
   {
      /* /etc/localtime links into the zoneinfo tree on Linux, the BSDs
       * and macOS (/var/db/timezone/zoneinfo/...) */
      char    target[PATH_MAX_LENGTH];
      ssize_t n = readlink("/etc/localtime", target, sizeof(target) - 1);
      if (n > 0)
      {
         const char *zi;
         target[n] = '\0';
         if ((zi = strstr(target, "zoneinfo/")))
         {
            strlcpy(s, zi + STRLEN_CONST("zoneinfo/"), len);
            return true;
         }
      }
   }
#endif

   return false;
}

