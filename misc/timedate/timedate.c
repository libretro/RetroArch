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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/capability.h>
#include <linux/rtc.h>

#include <file/file_path.h>

#include "timedate.h"

/* Calendar */

unsigned timedate_year_max(void)
{
   return (sizeof(time_t) < 8) ? 2037 : 2099;
}

bool timedate_is_leap_year(unsigned year)
{
   return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

unsigned timedate_days_in_month(unsigned year, unsigned month)
{
   static const unsigned char days[12] =
         { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

   if (month < 1 || month > 12)
      return 31;
   if (month == 2 && timedate_is_leap_year(year))
      return 29;
   return days[month - 1];
}

void timedate_normalize(struct timedate_fields *f)
{
   unsigned dim;

   if (f->year < TIMEDATE_YEAR_MIN)
      f->year = TIMEDATE_YEAR_MIN;
   else if (f->year > timedate_year_max())
      f->year = timedate_year_max();

   if (f->month < 1)
      f->month = 1;
   else if (f->month > 12)
      f->month = 12;

   dim = timedate_days_in_month(f->year, f->month);
   if (f->day < 1)
      f->day = 1;
   else if (f->day > dim)
      f->day = dim;

   if (f->hour > 23)
      f->hour = 23;
   if (f->minute > 59)
      f->minute = 59;
}

void timedate_get_local(struct timedate_fields *f)
{
   struct tm tm_now;
   time_t    t = time(NULL);

   memset(&tm_now, 0, sizeof(tm_now));
   localtime_r(&t, &tm_now);

   f->year   = (unsigned)(tm_now.tm_year + 1900);
   f->month  = (unsigned)(tm_now.tm_mon + 1);
   f->day    = (unsigned)tm_now.tm_mday;
   f->hour   = (unsigned)tm_now.tm_hour;
   f->minute = (unsigned)tm_now.tm_min;

   timedate_normalize(f);
}

/* Hardware real-time clock */

enum timedate_rtc_result
{
   TIMEDATE_RTC_OK = 0,
   TIMEDATE_RTC_UNAVAILABLE, /* no RTC, or it cannot be written */
   TIMEDATE_RTC_FAILED       /* RTC present but this write failed */
};

static const char *rtc_nodes[] = { "/dev/rtc0", "/dev/rtc" };

/* Set once a write shows the RTC cannot be written at all,
 * e.g. a driver without set_time. Holds for the session. */
static bool rtc_write_unsupported = false;

static const char *rtc_find_node(void)
{
   size_t i;
   for (i = 0; i < sizeof(rtc_nodes) / sizeof(rtc_nodes[0]); i++)
      if (path_is_character_special(rtc_nodes[i]))
         return rtc_nodes[i];
   return NULL;
}

static bool rtc_have_cap_sys_time(void)
{
   struct __user_cap_header_struct hdr;
   struct __user_cap_data_struct   data[2];

   memset(&hdr,  0, sizeof(hdr));
   memset(data,  0, sizeof(data));
   hdr.version = _LINUX_CAPABILITY_VERSION_3;
   hdr.pid     = 0;

   /* If the query itself fails, do not hide anything on its
    * account; the write will report what really happens. */
   if (syscall(SYS_capget, &hdr, data) != 0)
      return true;

   return (data[CAP_TO_INDEX(CAP_SYS_TIME)].effective
         & CAP_TO_MASK(CAP_SYS_TIME)) != 0;
}

/* Errors that mean the RTC can never be written from here. */
static bool rtc_errno_is_permanent(int err)
{
   switch (err)
   {
      case EPERM:
      case EACCES:
      case EROFS:
      case ENOTTY:
      case ENODEV:
      case ENXIO:
#ifdef EOPNOTSUPP
      case EOPNOTSUPP:
#endif
         return true;
      default:
         break;
   }
   return false;
}

bool timedate_rtc_can_write(void)
{
   const char *node;

   if (rtc_write_unsupported)
      return false;
   if (!(node = rtc_find_node()))
      return false;
   /* The node is opened read-only for RTC_SET_TIME; a process running
    * as a user needs read access to it as well as CAP_SYS_TIME */
   if (access(node, R_OK) != 0)
      return false;
   return rtc_have_cap_sys_time();
}

/* Writes @t to the hardware RTC in UTC (hwclock --systohc --utc).
 * A failure that shows the RTC cannot be written at all is
 * remembered, so timedate_rtc_can_write() returns false afterwards. */
static enum timedate_rtc_result timedate_rtc_write_utc(time_t t)
{
   struct rtc_time rtc;
   struct tm       tm_utc;
   const char     *node;
   int             fd;
   int             err;

   if (!timedate_rtc_can_write())
      return TIMEDATE_RTC_UNAVAILABLE;

   node = rtc_find_node();
   fd   = open(node, O_RDONLY | O_CLOEXEC);
   if (fd < 0)
   {
      /* EBUSY: another process holds the (exclusive) RTC open;
       * worth retrying later, so not latched. */
      if (rtc_errno_is_permanent(errno))
         rtc_write_unsupported = true;
      return TIMEDATE_RTC_FAILED;
   }

   if (!gmtime_r(&t, &tm_utc))
   {
      close(fd);
      return TIMEDATE_RTC_FAILED;
   }

   memset(&rtc, 0, sizeof(rtc));
   rtc.tm_sec   = tm_utc.tm_sec;
   rtc.tm_min   = tm_utc.tm_min;
   rtc.tm_hour  = tm_utc.tm_hour;
   rtc.tm_mday  = tm_utc.tm_mday;
   rtc.tm_mon   = tm_utc.tm_mon;
   rtc.tm_year  = tm_utc.tm_year;
   rtc.tm_wday  = tm_utc.tm_wday;
   rtc.tm_yday  = tm_utc.tm_yday;
   rtc.tm_isdst = 0;

   if (ioctl(fd, RTC_SET_TIME, &rtc) == 0)
   {
      close(fd);
      return TIMEDATE_RTC_OK;
   }

   err = errno;
   if (rtc_errno_is_permanent(err))
      rtc_write_unsupported = true;
   else if (err == EINVAL)
   {
      /* Older kernels (4.9 included) return EINVAL both for a
       * time out of the RTC's range and for a driver without
       * set_time. Write back what the RTC holds now: if that is
       * refused too, the RTC is read-only. */
      struct rtc_time cur;
      memset(&cur, 0, sizeof(cur));
      if (     ioctl(fd, RTC_RD_TIME, &cur) == 0
            && ioctl(fd, RTC_SET_TIME, &cur) != 0
            && (errno == EINVAL || rtc_errno_is_permanent(errno)))
         rtc_write_unsupported = true;
   }

   close(fd);
   return TIMEDATE_RTC_FAILED;
}

/* System clock */

enum timedate_set_result timedate_set_local(
      const struct timedate_fields *f, bool write_rtc)
{
   struct timedate_fields chk = *f;
   struct timespec        ts;
   struct tm              tm_in;
   time_t                 t;

   timedate_normalize(&chk);
   if (memcmp(&chk, f, sizeof(chk)))
      return TIMEDATE_SET_INVALID;

   memset(&tm_in, 0, sizeof(tm_in));
   tm_in.tm_year  = (int)f->year - 1900;
   tm_in.tm_mon   = (int)f->month - 1;
   tm_in.tm_mday  = (int)f->day;
   tm_in.tm_hour  = (int)f->hour;
   tm_in.tm_min   = (int)f->minute;
   tm_in.tm_sec   = 0;
   tm_in.tm_isdst = -1;

   /* Local wall-clock time to seconds since the epoch; the RTC
    * then gets UTC whatever the system time zone is. */
   /* Before the epoch in UTC (1 January 1970 east of Greenwich)
    * cannot be set */
   if ((t = mktime(&tm_in)) == (time_t)-1 || t < 0)
      return TIMEDATE_SET_INVALID;

   ts.tv_sec  = t;
   ts.tv_nsec = 0;
   if (clock_settime(CLOCK_REALTIME, &ts) != 0)
      return (errno == EPERM)
         ? TIMEDATE_SET_NO_PERMISSION
         : TIMEDATE_SET_FAILED;

   if (     write_rtc
         && timedate_rtc_can_write()
         && timedate_rtc_write_utc(t) == TIMEDATE_RTC_FAILED)
      return TIMEDATE_SET_OK_RTC_FAILED;

   return TIMEDATE_SET_OK;
}
