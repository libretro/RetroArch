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
#elif defined(EMSCRIPTEN)
#include <emscripten/emscripten.h>
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
#elif defined(EMSCRIPTEN)
#define retro_sleep(msec) (emscripten_sleep(msec))
#else
static INLINE void retro_sleep(unsigned msec)
{
   struct timespec tv;
   tv.tv_sec          = msec / 1000;
   tv.tv_nsec         = (msec % 1000) * 1000000;
   nanosleep(&tv, NULL);
}
#endif

#endif
