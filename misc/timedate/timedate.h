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

#ifndef _MISC_TIMEDATE_H
#define _MISC_TIMEDATE_H

#include <time.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Setting the system clock and the hardware real-time clock (RTC) by
 * hand, for Lakka's Date and Time menu. */

/* Earliest year offered when setting the clock by hand: the Unix
 * epoch. Lakka may run on old hardware whose clock is not Y2K
 * compliant, so pre-2000 dates stay settable; an RTC that cannot
 * hold such a date refuses the write and only the system clock is
 * set. (TLS certificate checks fail on dates that far back.) */
#define TIMEDATE_YEAR_MIN 1970

/* Latest year offered: 2037 where time_t is 32 bits (the 2038
 * limit), otherwise 2099, the end of the range common RTCs hold. */
unsigned timedate_year_max(void);

/* A wall-clock date and time, as a person reads it. */
struct timedate_fields
{
   unsigned year;   /* e.g. 2026 */
   unsigned month;  /* 1-12 */
   unsigned day;    /* 1-31, see timedate_days_in_month() */
   unsigned hour;   /* 0-23 */
   unsigned minute; /* 0-59 */
};

enum timedate_set_result
{
   TIMEDATE_SET_OK = 0,
   TIMEDATE_SET_OK_RTC_FAILED, /* system clock set, RTC write failed */
   TIMEDATE_SET_INVALID,       /* fields do not name a real local time */
   TIMEDATE_SET_NO_PERMISSION, /* process lacks CAP_SYS_TIME */
   TIMEDATE_SET_FAILED         /* clock_settime refused otherwise */
};

bool timedate_is_leap_year(unsigned year);

unsigned timedate_days_in_month(unsigned year, unsigned month);

/* Fill @f with the current local time (seconds dropped). */
void timedate_get_local(struct timedate_fields *f);

/* Clamp @f into range, including the day to the length of the
 * month in that year (31 January -> February gives 28 or 29). */
void timedate_normalize(struct timedate_fields *f);

/**
 * timedate_set_local:
 * @f         : local wall-clock time to set, seconds taken as 0.
 * @write_rtc : also store it in the hardware clock, if one can
 *              be written.
 *
 * Sets the system clock (CLOCK_REALTIME).
 **/
enum timedate_set_result timedate_set_local(
      const struct timedate_fields *f, bool write_rtc);

/**
 * timedate_rtc_can_write:
 *
 * Whether the hardware real-time clock can be written: an RTC
 * device node exists and is readable by this process, the process
 * holds CAP_SYS_TIME, and no earlier write has shown the RTC to be
 * read-only.
 *
 * Returns: true if writing the RTC is worth attempting.
 */
bool timedate_rtc_can_write(void);

RETRO_END_DECLS

#endif
