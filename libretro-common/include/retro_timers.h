/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_timers.h).
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

#ifndef __LIBRETRO_COMMON_TIMERS_H
#define __LIBRETRO_COMMON_TIMERS_H

#include <stdint.h>

#if defined(XENON)
#include <time/time.h>
#elif !defined(__PSL1GHT__) && defined(__PS3__)
#include <sys/timer.h>
#elif defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
#include <unistd.h>
#elif defined(WIIU)
#include <wiiu/os/thread.h>
#elif defined(PSP)
#include <pspthreadman.h>
#elif defined(VITA)
#include <psp2/kernel/threadmgr.h>
#elif defined(_3DS)
#include <3ds.h>
#else
#include <time.h>
#endif

#if defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(_WIN32) && defined(_XBOX)
#include <Xtl.h>
#endif

#include <limits.h>

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif
#include <retro_inline.h>

#ifdef DJGPP
#define timespec timeval
#define tv_nsec tv_usec
#include <unistd.h>

extern int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

static int nanosleepDOS(const struct timespec *rqtp, struct timespec *rmtp)
{
   usleep(1000000L * rqtp->tv_sec + rqtp->tv_nsec / 1000);

   if (rmtp)
      rmtp->tv_sec = rmtp->tv_nsec=0;

   return 0;
}

#define nanosleep nanosleepDOS
#endif

/**
 * Briefly suspends the running thread.
 *
 * @param msec The time to sleep for, in milliseconds.
 **/
#if defined(VITA)
#define retro_sleep(msec) (sceKernelDelayThread(1000 * (msec)))
#elif defined(_3DS)
#define retro_sleep(msec) (svcSleepThread(1000000 * (s64)(msec)))
#elif defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#define retro_sleep(msec) (SleepEx((msec), FALSE))
#elif defined(_WIN32) && !defined(_XBOX)
/* Desktop Windows.
 *
 * Sleep() resolves at the granularity of the global timer period,
 * which defaults to ~15.6 ms - longer than a single frame at any
 * common refresh rate. Frame Delay and Scanline Sync both depend on
 * sub-frame sleeps, so this is implemented out of line (in
 * libretro-common/time/rtime.c) on top of a high resolution waitable
 * timer where the running system provides one, falling back to plain
 * Sleep() everywhere else.
 *
 * NOTE: implemented in rtime.c rather than inline here because the
 * backing timer object must be per thread; see the comments there. */
#ifdef __cplusplus
extern "C" {
#endif
void retro_sleep(unsigned msec);
#ifdef __cplusplus
}
#endif
#elif defined(_WIN32)
#define retro_sleep(msec) (Sleep((msec)))
#elif defined(XENON)
#define retro_sleep(msec) (udelay(1000 * (msec)))
#elif !defined(__PSL1GHT__) && defined(__PS3__)
#define retro_sleep(msec) (sys_timer_usleep(1000 * (msec)))
#elif defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
#define retro_sleep(msec) (usleep(1000 * (msec)))
#elif defined(WIIU)
#define retro_sleep(msec) (OSSleepTicks(ms_to_ticks((msec))))
#elif defined(__EMSCRIPTEN__)
/* defined in frontend */
#ifdef __cplusplus
extern "C" {
#endif
void retro_sleep(unsigned msec);
#ifdef __cplusplus
}
#endif
#else
static INLINE void retro_sleep(unsigned msec)
{
   struct timespec tv;
   tv.tv_sec          = msec / 1000;
   tv.tv_nsec         = (msec % 1000) * 1000000;
   nanosleep(&tv, NULL);
}
#endif

/**
 * Briefly suspends the running thread, with microsecond resolution.
 *
 * Platforms whose sleep primitive is microsecond- or nanosecond-native
 * honour the request as given. Platforms that can only express whole
 * milliseconds round to nearest, so requests below 500 us degenerate to
 * a yield rather than an unwanted full millisecond of latency.
 *
 * @param usec The time to sleep for, in microseconds.
 **/
#if defined(VITA)
#define retro_sleep_us(usec) (sceKernelDelayThread((usec)))
#elif defined(_3DS)
#define retro_sleep_us(usec) (svcSleepThread(1000 * (s64)(usec)))
#elif defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#define retro_sleep_us(usec) (SleepEx(((usec) + 500) / 1000, FALSE))
#elif defined(_WIN32) && !defined(_XBOX)
/* Desktop Windows: see the note on retro_sleep() above. Implemented in
 * libretro-common/time/rtime.c, where it is the primitive that
 * retro_sleep() itself is expressed in terms of. */
#ifdef __cplusplus
extern "C" {
#endif
void retro_sleep_us(unsigned usec);
#ifdef __cplusplus
}
#endif
#elif defined(_WIN32)
#define retro_sleep_us(usec) (Sleep(((usec) + 500) / 1000))
#elif defined(XENON)
#define retro_sleep_us(usec) (udelay((usec)))
#elif !defined(__PSL1GHT__) && defined(__PS3__)
#define retro_sleep_us(usec) (sys_timer_usleep((usec)))
#elif defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
#define retro_sleep_us(usec) (usleep((usec)))
#elif defined(WIIU)
#define retro_sleep_us(usec) (OSSleepTicks(ms_to_ticks(((usec) + 500) / 1000)))
#elif defined(__EMSCRIPTEN__)
#define retro_sleep_us(usec) (retro_sleep(((usec) + 500) / 1000))
#else
static INLINE void retro_sleep_us(unsigned usec)
{
   struct timespec tv;
   tv.tv_sec          = usec / 1000000;
   tv.tv_nsec         = (usec % 1000000) * 1000;
   nanosleep(&tv, NULL);
}
#endif

#endif
