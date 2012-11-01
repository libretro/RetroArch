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

#include "performance.h"

#ifdef PERF_TEST

#if defined(__CELLOS_LV2__) || defined(GEKKO)
#ifndef _PPU_INTRINSICS_H
#include <ppu_intrinsics.h>
#endif
#elif defined(_XBOX360)
#include <PPCIntrinsics.h>
#elif defined(__linux__)
#include <sys/time.h>
#endif

#define MAX_COUNTERS 64
static struct rarch_perf_counter *perf_counters[MAX_COUNTERS];
static unsigned perf_ptr;

void rarch_perf_register(struct rarch_perf_counter *perf)
{
   if (perf_ptr >= MAX_COUNTERS)
      return;

   perf_counters[perf_ptr++] = perf;
   perf->registered = true;
}

void rarch_perf_log(void)
{
   RARCH_LOG("[PERF]: Performance counters:\n");
   for (unsigned i = 0; i < perf_ptr; i++)
      RARCH_PERFORMANCE_LOG(perf_counters[i]->ident, *perf_counters[i]);
}

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
   if (clock_gettime(CLOCK_MONOTONIC, &tv) == 0)
      time = (rarch_perf_tick_t)tv.tv_sec * 1000000000 + (rarch_perf_tick_t)tv.tv_nsec;
   else
      time = 0;

#elif defined(__GNUC__) && !defined(RARCH_CONSOLE)

#if defined(__i386__) || defined(__i486__) || defined(__i686__)
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
#endif

#if defined(__x86_64__) || defined(__i386__) || defined(__i486__) || defined(__i686__)
#define CPU_X86
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef CPU_X86
static void x86_cpuid(int func, int flags[4])
{
#ifdef __GNUC__
   asm volatile("cpuid\n" :
         "=a"(flags[0]),
         "=b"(flags[1]),
         "=c"(flags[2]),
         "=d"(flags[3]) : "a"(func));
#elif defined(_MSC_VER)
   __cpuid(flags, func);
#endif
}
#endif

void rarch_get_cpu_features(struct rarch_cpu_features *cpu)
{
   memset(cpu, 0, sizeof(*cpu));

#ifdef CPU_X86
   int flags[4];
   x86_cpuid(0, flags);

   char vendor[13] = {0};
   const int vendor_shuffle[3] = { flags[1], flags[3], flags[2] };
   memcpy(vendor, vendor_shuffle, sizeof(vendor_shuffle));
   RARCH_LOG("[CPUID]: Vendor: %s\n", vendor);

   if (flags[0] < 1) // Does CPUID not support func = 1? (unlikely ...)
      return;

   x86_cpuid(1, flags);
   cpu->sse  = flags[3] & (1 << 25);
   cpu->sse2 = flags[3] & (1 << 26);

   int avx_flags = (1 << 27) | (1 << 28);
   cpu->avx = (flags[2] & avx_flags) == avx_flags; // Is this enough?

   RARCH_LOG("[CPUID]: SSE:  %d\n", cpu->sse);
   RARCH_LOG("[CPUID]: SSE2: %d\n", cpu->sse2);
   RARCH_LOG("[CPUID]: AVX:  %d\n", cpu->avx);
#endif
}

