/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtime.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_RTIME_H__
#define __LIBRETRO_SDK_RTIME_H__

#include <retro_common_api.h>

#include <boolean.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

RETRO_BEGIN_DECLS

/* The proleptic Gregorian calendar, days counted from 1970-01-01: pure
 * arithmetic, any thread, any year. After Howard Hinnant's algorithms.
 * Inline with a specifier of its own, as retro_atomic.h's are: some cores
 * define INLINE themselves, FBNeo's as "__inline static". */
#if defined(_WIN32) || defined(__INTEL_COMPILER)
#define RTIME_INLINE __inline
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define RTIME_INLINE inline
#elif defined(__GNUC__)
#define RTIME_INLINE __inline__
#else
#define RTIME_INLINE
#endif

/**
 * The day y-m-d is, counted from 1970-01-01 (negative before it).
 * @param m 1 to 12. @param d 1 to the month's length.
 */
static RTIME_INLINE int64_t rtime_days_from_civil(int64_t y, unsigned m, unsigned d)
{
   int64_t  era;
   unsigned yoe, doy, doe;
   y  -= m <= 2;
   era = (y >= 0 ? y : y - 399) / 400;
   yoe = (unsigned)(y - era * 400);
   doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
   doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
   return era * 146097 + (int64_t)doe - 719468;
}

/**
 * The date of a day counted from 1970-01-01: the inverse of
 * rtime_days_from_civil().
 */
static RTIME_INLINE void rtime_civil_from_days(int64_t days, int64_t *y,
      unsigned *m, unsigned *d)
{
   int64_t  z   = days + 719468;
   int64_t  era = (z >= 0 ? z : z - 146096) / 146097;
   unsigned doe = (unsigned)(z - era * 146097);
   unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
   unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
   unsigned mp  = (5 * doy + 2) / 153;
   *d = doy - (153 * mp + 2) / 5 + 1;
   *m = mp < 10 ? mp + 3 : mp - 9;
   *y = (int64_t)yoe + era * 400 + (*m <= 2);
}

static RTIME_INLINE bool rtime_is_leap_year(int64_t y)
{
   return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

/**
 * The length of month @m (1 to 12) of year @y; 0 for another month.
 */
static RTIME_INLINE unsigned rtime_days_in_month(int64_t y, unsigned m)
{
   static const uint8_t days[12] =
   { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
   if (m < 1 || m > 12)
      return 0;
   return m == 2 && rtime_is_leap_year(y) ? 29 : days[m - 1];
}

/**
 * The day of the week of a day counted from 1970-01-01 (a Thursday),
 * 0 Sunday to 6 Saturday.
 */
static RTIME_INLINE unsigned rtime_weekday(int64_t days)
{
   return (unsigned)(days >= -4 ? (days + 4) % 7 : (days + 5) % 7 + 6);
}

/* TODO/FIXME: Move all generic time handling functions
 * to this file */

/**
 * Must be called at program or core start.
 * May be called multiple times without ill effects,
 * but must only be called from the main thread.
 */
void rtime_init(void);

/**
 * Must be called upon program or core termination.
 * May be called multiple times without ill effects,
 * but must only be called from the main thread.
 */
void rtime_deinit(void);

/**
 * Thread-safe wrapper around standard \c localtime(),
 * which by itself is not guaranteed to be thread-safe.
 * @param timep Pointer to a time_t object to convert.
 * @param result Pointer to a tm object to store the result in.
 * @return \c result.
 * @see https://en.cppreference.com/w/c/chrono/localtime
 */
struct tm *rtime_localtime(const time_t *timep, struct tm *result);

/**
 * Reads the calendar clock. Any thread.
 * @param sec UTC seconds since 1970, leap seconds not counted.
 * @param nsec Nanoseconds into that second, 0 to 999999999.
 * @return \c false only when the host has no clock.
 */
bool rtime_calendar_now(int64_t *sec, int32_t *nsec);

/**
 * Seconds east of UTC on the host's local clock at \c sec, daylight
 * saving included; 0 if the conversion fails. Main thread only: it
 * reloads the host zone.
 */
int32_t rtime_utc_offset(int64_t sec);

/**
 * Seconds east of UTC in standard time, without daylight saving, for
 * the zone in effect at \c sec. Main thread only.
 */
int32_t rtime_standard_offset(int64_t sec);

/**
 * The host's IANA zone name, such as "Europe/London", NUL-terminated
 * in \c s. Main thread only.
 * @return Its length, or 0 with \c s empty when the name is unknown
 * or does not fit in \c len bytes.
 */
size_t rtime_zone_name(char *s, size_t len);

RETRO_END_DECLS

#endif
