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
