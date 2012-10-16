/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "benchmark.h"

#if defined(__CELLOS_LV2__) || defined(GEKKO)
#ifndef _PPU_INTRINSICS_H
#include <ppu_intrinsics.h>
#endif
#elif defined(_XBOX360)
#include <PPCIntrinsics.h>
#elif defined(__linux__)
#include <sys/time.h>
#endif

rarch_perf_tick_t rarch_get_perf_counter(void)
{
   rarch_perf_tick_t time = 0;
#ifdef _XBOX1

#define rdtsc	__asm __emit 0fh __asm __emit 031h
   LARGE_INTEGER time_tmp;
   rdtsc;
   __asm	mov	time_tmp.LowPart, eax;
   __asm	mov	time_tmp.HighPart, edx;
   time = time_tmp.QuadPart;

#elif defined(__linux__)
   struct timespec tv;
   if (clock_gettime(CLOCK_MONOTONIC_RAW, &tv) == 0)
      time = (rarch_perf_tick_t)tv.tv_sec * 1000000000 + (rarch_perf_tick_t)tv.tv_nsec;
   else
      time = 0;

#elif defined(__GNUC__) && !defined(RARCH_CONSOLE)

#if defined(__i386__) || defined(__i486__)
   asm volatile ("rdtsc" : "=A" (time));
#elif defined(__x86_64__)
   unsigned a, d;
   asm volatile ("rdtsc" : "=a" (a), "=d" (d));
   time = (rarch_perf_tick_t)a | ((rarch_perf_tick_t)d << 32);
#endif

#elif defined(__ARM_ARCH_6__) || defined(ANDROID)
    asm volatile( "mrc p15, 0, %0, c9, c13, 0" : "=r"(time) );
#elif defined(__CELLOS_LV2__) || defined(GEKKO) || defined(_XBOX360)
   time = __mftb();
#endif

   return time;
}
