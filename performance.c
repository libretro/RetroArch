/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <stdio.h>
#include "libretro.h"
#include "performance.h"
#include "general.h"

#ifdef ANDROID
#include "performance/performance_android.h"
#endif

#if !defined(_WIN32) && !defined(RARCH_CONSOLE)
#include <unistd.h>
#endif

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#endif

#if defined(__CELLOS_LV2__) || defined(GEKKO)
#ifndef _PPU_INTRINSICS_H
#include <ppu_intrinsics.h>
#endif
#elif defined(_XBOX360)
#include <PPCIntrinsics.h>
#elif defined(_POSIX_MONOTONIC_CLOCK) || defined(ANDROID) || defined(__QNX__)
// POSIX_MONOTONIC_CLOCK is not being defined in Android headers despite support being present.
#include <time.h>
#endif

#ifdef __QNX__
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 2
#endif
#endif

#if defined(__PSL1GHT__)
#include <sys/time.h>
#elif defined(__CELLOS_LV2__)
#include <sys/sys_time.h>
#endif

#ifdef GEKKO
#include <ogc/lwp_watchdog.h>
#endif

// OSX specific. OSX lacks clock_gettime().
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

#include <string.h>

#define MAX_COUNTERS 64
static struct retro_perf_counter *perf_counters[MAX_COUNTERS];
static unsigned perf_ptr;

void rarch_perf_register(struct retro_perf_counter *perf)
{
   if (perf_ptr >= MAX_COUNTERS)
      return;

   perf_counters[perf_ptr++] = perf;
   perf->registered = true;
}

void rarch_perf_log(void)
{
   unsigned i;
   RARCH_LOG("[PERF]: Performance counters:\n");
   for (i = 0; i < perf_ptr; i++)
      RETRO_PERFORMANCE_LOG(perf_counters[i]->ident, *perf_counters[i]);
}

retro_perf_tick_t rarch_get_perf_counter(void)
{
   retro_perf_tick_t time = 0;
#ifdef _XBOX1

#define rdtsc	__asm __emit 0fh __asm __emit 031h
   LARGE_INTEGER time_tmp;
   rdtsc;
   __asm	mov	time_tmp.LowPart, eax;
   __asm	mov	time_tmp.HighPart, edx;
   time = time_tmp.QuadPart;

#elif defined(__linux__) || defined(__QNX__)
   struct timespec tv;
   if (clock_gettime(CLOCK_MONOTONIC, &tv) == 0)
      time = (retro_perf_tick_t)tv.tv_sec * 1000000000 + (retro_perf_tick_t)tv.tv_nsec;
   else
      time = 0;

#elif defined(__GNUC__) && !defined(RARCH_CONSOLE)

#if defined(__i386__) || defined(__i486__) || defined(__i686__)
   asm volatile ("rdtsc" : "=A" (time));
#elif defined(__x86_64__)
   unsigned a, d;
   asm volatile ("rdtsc" : "=a" (a), "=d" (d));
   time = (retro_perf_tick_t)a | ((retro_perf_tick_t)d << 32);
#endif

#elif defined(__ARM_ARCH_6__)
    asm volatile( "mrc p15, 0, %0, c9, c13, 0" : "=r"(time) );
#elif defined(__CELLOS_LV2__) || defined(GEKKO) || defined(_XBOX360)
   time = __mftb();
#endif

   return time;
}

retro_time_t rarch_get_time_usec(void)
{
#if defined(_WIN32)
   static LARGE_INTEGER freq;
   if (!freq.QuadPart && !QueryPerformanceFrequency(&freq)) // Frequency is guaranteed to not change.
      return 0;

   LARGE_INTEGER count;
   if (!QueryPerformanceCounter(&count))
      return 0;
   return count.QuadPart * 1000000 / freq.QuadPart;
#elif defined(__CELLOS_LV2__)
   return sys_time_get_system_time();
#elif defined(GEKKO)
   return ticks_to_microsecs(gettime());
#elif defined(__MACH__) // OSX doesn't have clock_gettime ...
   clock_serv_t cclock;
   mach_timespec_t mts;
   host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
   clock_get_time(cclock, &mts);
   mach_port_deallocate(mach_task_self(), cclock);
   return mts.tv_sec * INT64_C(1000000) + (mts.tv_nsec + 500) / 1000;
#elif defined(_POSIX_MONOTONIC_CLOCK) || defined(__QNX__) || defined(ANDROID)
   struct timespec tv;
   if (clock_gettime(CLOCK_MONOTONIC, &tv) < 0)
      return 0;
   return tv.tv_sec * INT64_C(1000000) + (tv.tv_nsec + 500) / 1000;
#elif defined(EMSCRIPTEN)
   return emscripten_get_now() * 1000;
#else
#error "Your platform does not have a timer function implemented in rarch_get_time_usec(). Cannot continue."
#endif
}

#if defined(__x86_64__) || defined(__i386__) || defined(__i486__) || defined(__i686__)
#define CPU_X86
#endif

#if defined(_MSC_VER) && !defined(_XBOX)
#include <intrin.h>
#endif

#ifdef CPU_X86
static void x86_cpuid(int func, int flags[4])
{
   // On Android, we compile RetroArch with PIC, and we are not allowed to clobber the ebx
   // register.
#ifdef __x86_64__
#define REG_b "rbx"
#define REG_S "rsi"
#else
#define REG_b "ebx"
#define REG_S "esi"
#endif

#if defined(__GNUC__)
   asm volatile (
         "mov %%" REG_b ", %%" REG_S "\n"
         "cpuid\n"
         "xchg %%" REG_b ", %%" REG_S "\n"
         : "=a"(flags[0]), "=S"(flags[1]), "=c"(flags[2]), "=d"(flags[3])
         : "a"(func));
#elif defined(_MSC_VER)
   __cpuid(flags, func);
#else
   RARCH_WARN("Unknown compiler. Cannot check CPUID with inline assembly.\n");
   memset(flags, 0, 4 * sizeof(int));
#endif
}
#endif

void rarch_get_cpu_features(unsigned *cpu)
{
   *cpu = 0;

#if defined(CPU_X86)
   int flags[4];
   x86_cpuid(0, flags);

   char vendor[13] = {0};
   const int vendor_shuffle[3] = { flags[1], flags[3], flags[2] };
   memcpy(vendor, vendor_shuffle, sizeof(vendor_shuffle));
   RARCH_LOG("[CPUID]: Vendor: %s\n", vendor);

   if (flags[0] < 1) // Does CPUID not support func = 1? (unlikely ...)
      return;

   x86_cpuid(1, flags);

   if (flags[3] & (1 << 25))
      *cpu |= RETRO_SIMD_SSE;

   if (flags[3] & (1 << 26))
      *cpu |= RETRO_SIMD_SSE2;

   if (flags[2] & (1 << 0))
      *cpu |= RETRO_SIMD_SSE3;

   if (flags[2] & (1 << 9))
      *cpu |= RETRO_SIMD_SSSE3;

   const int avx_flags = (1 << 27) | (1 << 28);
   if ((flags[2] & avx_flags) == avx_flags)
      *cpu |= RETRO_SIMD_AVX;

   RARCH_LOG("[CPUID]: SSE:   %u\n", !!(*cpu & RETRO_SIMD_SSE));
   RARCH_LOG("[CPUID]: SSE2:  %u\n", !!(*cpu & RETRO_SIMD_SSE2));
   RARCH_LOG("[CPUID]: SSE3:  %u\n", !!(*cpu & RETRO_SIMD_SSE3));
   RARCH_LOG("[CPUID]: SSSE3: %u\n", !!(*cpu & RETRO_SIMD_SSSE3));
   RARCH_LOG("[CPUID]: AVX:   %u\n", !!(*cpu & RETRO_SIMD_AVX));
#elif defined(ANDROID) && defined(ANDROID_ARM)
   uint64_t cpu_flags = android_getCpuFeatures();

   if (cpu_flags & ANDROID_CPU_ARM_FEATURE_NEON)
      *cpu |= RETRO_SIMD_NEON;

   RARCH_LOG("[CPUID]: NEON: %u\n", !!(*cpu & RETRO_SIMD_NEON));
#elif defined(HAVE_NEON)
   *cpu |= RETRO_SIMD_NEON;
   RARCH_LOG("[CPUID]: NEON: %u\n", !!(*cpu & RETRO_SIMD_NEON));
#elif defined(__CELLOS_LV2__)
   *cpu |= RETRO_SIMD_VMX;
   RARCH_LOG("[CPUID]: VMX: %u\n", !!(*cpu & RETRO_SIMD_VMX));
#elif defined(XBOX360)
   *cpu |= RETRO_SIMD_VMX128;
   RARCH_LOG("[CPUID]: VMX128: %u\n", !!(*cpu & RETRO_SIMD_VMX128));
#endif
}
