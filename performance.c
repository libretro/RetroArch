/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include "compat/strl.h"
#include "verbosity.h"

#ifdef _WIN32
#define PERF_LOG_FMT "[PERF]: Avg (%s): %I64u ticks, %I64u runs.\n"
#else
#define PERF_LOG_FMT "[PERF]: Avg (%s): %llu ticks, %llu runs.\n"
#endif

#if !defined(_WIN32) && !defined(RARCH_CONSOLE)
#include <unistd.h>
#endif

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#include <intrin.h>
#endif

#if defined(__CELLOS_LV2__) || defined(GEKKO)
#ifndef _PPU_INTRINSICS_H
#include <ppu_intrinsics.h>
#endif
#elif defined(_XBOX360)
#include <PPCIntrinsics.h>
#elif defined(_POSIX_MONOTONIC_CLOCK) || defined(ANDROID) || defined(__QNX__)
/* POSIX_MONOTONIC_CLOCK is not being defined in Android headers despite support being present. */
#include <time.h>
#endif

#if defined(__QNX__) && !defined(CLOCK_MONOTONIC)
#define CLOCK_MONOTONIC 2
#endif

#if defined(PSP)
#include <sys/time.h>
#include <psprtc.h>
#endif

#if defined(VITA)
#include <psp2/kernel/processmgr.h>
#include <psp2/rtc.h>
#endif

#if defined(__PSL1GHT__)
#include <sys/time.h>
#elif defined(__CELLOS_LV2__)
#include <sys/sys_time.h>
#endif

#ifdef GEKKO
#include <ogc/lwp_watchdog.h>
#endif

/* iOS/OSX specific. Lacks clock_gettime(), so implement it. */
#ifdef __MACH__
#include <sys/time.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 0
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

static int clock_gettime(int clk_ik, struct timespec *t)
{
   struct timeval now;
   int rv = gettimeofday(&now, NULL);
   if (rv)
      return rv;
   t->tv_sec  = now.tv_sec;
   t->tv_nsec = now.tv_usec * 1000;
   return 0;
}
#endif

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

#if defined(BSD) || defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#include <string.h>

#if defined(__linux__)
#include "frontend/drivers/platform_linux.h"
#endif

static struct retro_perf_counter *perf_counters_rarch[MAX_COUNTERS];
static struct retro_perf_counter *perf_counters_libretro[MAX_COUNTERS];
static unsigned perf_ptr_rarch;
static unsigned perf_ptr_libretro;

struct retro_perf_counter **retro_get_perf_counter_rarch(void)
{
   return perf_counters_rarch;
}

struct retro_perf_counter **retro_get_perf_counter_libretro(void)
{
   return perf_counters_libretro;
}

unsigned retro_get_perf_count_rarch(void)
{
   return perf_ptr_rarch;
}

unsigned retro_get_perf_count_libretro(void)
{
   return perf_ptr_libretro;
}

void rarch_perf_register(struct retro_perf_counter *perf)
{
   if (
            !runloop_ctl(RUNLOOP_CTL_IS_PERFCNT_ENABLE, NULL)
         || perf->registered
         || perf_ptr_rarch >= MAX_COUNTERS
      )
      return;

   perf_counters_rarch[perf_ptr_rarch++] = perf;
   perf->registered = true;
}

void retro_perf_register(struct retro_perf_counter *perf)
{
   if (perf->registered || perf_ptr_libretro >= MAX_COUNTERS)
      return;

   perf_counters_libretro[perf_ptr_libretro++] = perf;
   perf->registered = true;
}

void retro_perf_clear(void)
{
   perf_ptr_libretro = 0;
   memset(perf_counters_libretro, 0, sizeof(perf_counters_libretro));
}

static void log_counters(struct retro_perf_counter **counters, unsigned num)
{
   unsigned i;
   for (i = 0; i < num; i++)
   {
      if (counters[i]->call_cnt)
      {
         RARCH_LOG(PERF_LOG_FMT,
               counters[i]->ident,
               (unsigned long long)counters[i]->total /
               (unsigned long long)counters[i]->call_cnt,
               (unsigned long long)counters[i]->call_cnt);
      }
   }
}

void rarch_perf_log(void)
{
   if (!runloop_ctl(RUNLOOP_CTL_IS_PERFCNT_ENABLE, NULL))
      return;

   RARCH_LOG("[PERF]: Performance counters (RetroArch):\n");
   log_counters(perf_counters_rarch, perf_ptr_rarch);
}

void retro_perf_log(void)
{
   RARCH_LOG("[PERF]: Performance counters (libretro):\n");
   log_counters(perf_counters_libretro, perf_ptr_libretro);
}

/**
 * retro_get_perf_counter:
 *
 * Gets performance counter.
 *
 * Returns: performance counter.
 **/
retro_perf_tick_t retro_get_perf_counter(void)
{
   retro_perf_tick_t time_ticks = 0;
#if defined(__linux__) || defined(__QNX__) || defined(__MACH__)
   struct timespec tv;
   if (clock_gettime(CLOCK_MONOTONIC, &tv) == 0)
      time_ticks = (retro_perf_tick_t)tv.tv_sec * 1000000000 +
         (retro_perf_tick_t)tv.tv_nsec;

#elif defined(__GNUC__) && !defined(RARCH_CONSOLE)

#if defined(__i386__) || defined(__i486__) || defined(__i686__)
   __asm__ volatile ("rdtsc" : "=A" (time_ticks));
#elif defined(__x86_64__)
   unsigned a, d;
   __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
   time_ticks = (retro_perf_tick_t)a | ((retro_perf_tick_t)d << 32);
#endif

#elif defined(__ARM_ARCH_6__)
   __asm__ volatile( "mrc p15, 0, %0, c9, c13, 0" : "=r"(time_ticks) );
#elif defined(__CELLOS_LV2__) || defined(GEKKO) || defined(_XBOX360) || defined(__powerpc__) || defined(__ppc__) || defined(__POWERPC__)
   time_ticks = __mftb();
#elif defined(PSP) || defined(VITA)
   sceRtcGetCurrentTick(&time_ticks);
#elif defined(_3DS)
   time_ticks = svcGetSystemTick();
#elif defined(__mips__)
   struct timeval tv;
   gettimeofday(&tv,NULL);
   time_ticks = (1000000 * tv.tv_sec + tv.tv_usec);
#elif defined(_WIN32)
   long tv_sec, tv_usec;
   static const unsigned __int64 epoch = 11644473600000000Ui64;
   FILETIME file_time;
   SYSTEMTIME system_time;
   ULARGE_INTEGER ularge;

   GetSystemTime(&system_time);
   SystemTimeToFileTime(&system_time, &file_time);
   ularge.LowPart = file_time.dwLowDateTime;
   ularge.HighPart = file_time.dwHighDateTime;

   tv_sec = (long)((ularge.QuadPart - epoch) / 10000000L);
   tv_usec = (long)(system_time.wMilliseconds * 1000);

   time_ticks = (1000000 * tv_sec + tv_usec);
#endif

   return time_ticks;
}

/**
 * retro_get_time_usec:
 *
 * Gets time in microseconds.
 *
 * Returns: time in microseconds.
 **/
retro_time_t retro_get_time_usec(void)
{
#if defined(_WIN32)
   static LARGE_INTEGER freq;
   LARGE_INTEGER count;

   /* Frequency is guaranteed to not change. */
   if (!freq.QuadPart && !QueryPerformanceFrequency(&freq))
      return 0;

   if (!QueryPerformanceCounter(&count))
      return 0;
   return count.QuadPart * 1000000 / freq.QuadPart;
#elif defined(__CELLOS_LV2__)
   return sys_time_get_system_time();
#elif defined(GEKKO)
   return ticks_to_microsecs(gettime());
#elif defined(_POSIX_MONOTONIC_CLOCK) || defined(__QNX__) || defined(ANDROID) || defined(__MACH__)
   struct timespec tv = {0};
   if (clock_gettime(CLOCK_MONOTONIC, &tv) < 0)
      return 0;
   return tv.tv_sec * INT64_C(1000000) + (tv.tv_nsec + 500) / 1000;
#elif defined(EMSCRIPTEN)
   return emscripten_get_now() * 1000;
#elif defined(__mips__)
   struct timeval tv;
   gettimeofday(&tv,NULL);
   return (1000000 * tv.tv_sec + tv.tv_usec);
#elif defined(_3DS)
   return osGetTime() * 1000;
#elif defined(VITA)
   return sceKernelGetProcessTimeWide();
#else
#error "Your platform does not have a timer function implemented in retro_get_time_usec(). Cannot continue."
#endif
}

#if defined(__x86_64__) || defined(__i386__) || defined(__i486__) || defined(__i686__)
#define CPU_X86
#endif

#if defined(_MSC_VER) && !defined(_XBOX)
#include <intrin.h>
#endif

#ifdef CPU_X86
void x86_cpuid(int func, int flags[4])
{
   /* On Android, we compile RetroArch with PIC, and we
    * are not allowed to clobber the ebx register. */
#ifdef __x86_64__
#define REG_b "rbx"
#define REG_S "rsi"
#else
#define REG_b "ebx"
#define REG_S "esi"
#endif

#if defined(__GNUC__)
   __asm__ volatile (
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

/* Only runs on i686 and above. Needs to be conditionally run. */
static uint64_t xgetbv_x86(uint32_t idx)
{
#if defined(__GNUC__)
   uint32_t eax, edx;
   __asm__ volatile (
         /* Older GCC versions (Apple's GCC for example) do
          * not understand xgetbv instruction.
          * Stamp out the machine code directly.
          */
         ".byte 0x0f, 0x01, 0xd0\n"
         : "=a"(eax), "=d"(edx) : "c"(idx));
   return ((uint64_t)edx << 32) | eax;
#elif _MSC_FULL_VER >= 160040219
   /* Intrinsic only works on 2010 SP1 and above. */
   return _xgetbv(idx);
#else
   RARCH_WARN("Unknown compiler. Cannot check xgetbv bits.\n");
   return 0;
#endif
}
#endif

#if defined(__ARM_NEON__)  
static void arm_enable_runfast_mode(void)
{
   /* RunFast mode. Enables flush-to-zero and some
    * floating point optimizations. */
   static const unsigned x = 0x04086060;
   static const unsigned y = 0x03000000;
   int r;
   __asm__ volatile(
         "fmrx	%0, fpscr   \n\t" /* r0 = FPSCR */
         "and	%0, %0, %1  \n\t" /* r0 = r0 & 0x04086060 */
         "orr	%0, %0, %2  \n\t" /* r0 = r0 | 0x03000000 */
         "fmxr	fpscr, %0   \n\t" /* FPSCR = r0 */
         : "=r"(r)
         : "r"(x), "r"(y)
        );
}
#endif

/**
 * retro_get_cpu_cores:
 *
 * Gets the amount of available CPU cores.
 *
 * Returns: amount of CPU cores available.
 **/
unsigned retro_get_cpu_cores(void)
{
#if defined(_WIN32) && !defined(_XBOX)
   /* Win32 */
   SYSTEM_INFO sysinfo;
   GetSystemInfo(&sysinfo);
   return sysinfo.dwNumberOfProcessors;
#elif defined(GEKKO)
   return 1;
#elif defined(PSP)
   return 1;
#elif defined(VITA)
   return 4;
#elif defined(_3DS)
   return 1;
#elif defined(_SC_NPROCESSORS_ONLN)
   /* Linux, most UNIX-likes. */
   long ret = sysconf(_SC_NPROCESSORS_ONLN);
   if (ret <= 0)
      return (unsigned)1;
   return ret;
#elif defined(BSD) || defined(__APPLE__)
   /* BSD */
   /* Copypasta from stackoverflow, dunno if it works. */
   int num_cpu = 0;
   int mib[4];
   size_t len = sizeof(num_cpu);

   mib[0] = CTL_HW;
   mib[1] = HW_AVAILCPU;
   sysctl(mib, 2, &num_cpu, &len, NULL, 0);
   if (num_cpu < 1)
   {
      mib[1] = HW_NCPU;
      sysctl(mib, 2, &num_cpu, &len, NULL, 0);
      if (num_cpu < 1)
         num_cpu = 1;
   }
   return num_cpu;
#elif defined(__linux__)
   return linux_get_cpu_count();
#elif defined(_XBOX360)
   return 3;
#else
   /* No idea, assume single core. */
   return 1;
#endif
}

/* According to http://en.wikipedia.org/wiki/CPUID */
#define VENDOR_INTEL_b  0x756e6547
#define VENDOR_INTEL_c  0x6c65746e
#define VENDOR_INTEL_d  0x49656e69

/**
 * retro_get_cpu_features:
 *
 * Gets CPU features..
 *
 * Returns: bitmask of all CPU features available.
 **/
uint64_t retro_get_cpu_features(void)
{
   int flags[4];
   int vendor_shuffle[3];
   char vendor[13]     = {0};
   uint64_t cpu_flags  = 0;
   uint64_t cpu        = 0;
   unsigned max_flag   = 0;
#if defined(CPU_X86)
   int vendor_is_intel = 0;
   const int avx_flags = (1 << 27) | (1 << 28);
#endif

   char buf[sizeof(" MMX MMXEXT SSE SSE2 SSE3 SSSE3 SS4 SSE4.2 AES AVX AVX2 NEON VMX VMX128 VFPU PS")];

   memset(buf, 0, sizeof(buf));

   (void)cpu_flags;
   (void)flags;
   (void)max_flag;
   (void)vendor;
   (void)vendor_shuffle;

#if defined(CPU_X86)
   (void)avx_flags;

   x86_cpuid(0, flags);
   vendor_shuffle[0] = flags[1];
   vendor_shuffle[1] = flags[3];
   vendor_shuffle[2] = flags[2];
   memcpy(vendor, vendor_shuffle, sizeof(vendor_shuffle));

   RARCH_LOG("[CPUID]: Vendor: %s\n", vendor);

   vendor_is_intel = (
         flags[1] == VENDOR_INTEL_b &&
         flags[2] == VENDOR_INTEL_c &&
         flags[3] == VENDOR_INTEL_d);

   max_flag = flags[0];
   if (max_flag < 1) /* Does CPUID not support func = 1? (unlikely ...) */
      return 0;

   x86_cpuid(1, flags);

   if (flags[3] & (1 << 23))
      cpu |= RETRO_SIMD_MMX;

   if (flags[3] & (1 << 25))
   {
      /* SSE also implies MMXEXT (according to FFmpeg source). */
      cpu |= RETRO_SIMD_SSE;
      cpu |= RETRO_SIMD_MMXEXT;
   }


   if (flags[3] & (1 << 26))
      cpu |= RETRO_SIMD_SSE2;

   if (flags[2] & (1 << 0))
      cpu |= RETRO_SIMD_SSE3;

   if (flags[2] & (1 << 9))
      cpu |= RETRO_SIMD_SSSE3;

   if (flags[2] & (1 << 19))
      cpu |= RETRO_SIMD_SSE4;

   if (flags[2] & (1 << 20))
      cpu |= RETRO_SIMD_SSE42;

   if ((flags[2] & (1 << 23)))
      cpu |= RETRO_SIMD_POPCNT;

   if (vendor_is_intel && (flags[2] & (1 << 22)))
      cpu |= RETRO_SIMD_MOVBE;

   if (flags[2] & (1 << 25))
      cpu |= RETRO_SIMD_AES;


   /* Must only perform xgetbv check if we have
    * AVX CPU support (guaranteed to have at least i686). */
   if (((flags[2] & avx_flags) == avx_flags)
         && ((xgetbv_x86(0) & 0x6) == 0x6))
      cpu |= RETRO_SIMD_AVX;

   if (max_flag >= 7)
   {
      x86_cpuid(7, flags);
      if (flags[1] & (1 << 5))
         cpu |= RETRO_SIMD_AVX2;
   }

   x86_cpuid(0x80000000, flags);
   max_flag = flags[0];
   if (max_flag >= 0x80000001u)
   {
      x86_cpuid(0x80000001, flags);
      if (flags[3] & (1 << 23))
         cpu |= RETRO_SIMD_MMX;
      if (flags[3] & (1 << 22))
         cpu |= RETRO_SIMD_MMXEXT;
   }
#elif defined(__MACH__)
    int has_neon, has_altivec;
    size_t len = sizeof(size_t);
#if defined(CTL_HW) && defined(HW_VECTORUNIT)
    int mib[2];
    mib[0] = CTL_HW;
    mib[1] = HW_VECTORUNIT;
    sysctl(mib, 2, &has_altivec, &len, NULL, 0);
#endif
    sysctlbyname("hw.optional.neon", &has_neon, &len, NULL, 0);

    if (has_neon)
       cpu |= RETRO_SIMD_NEON;
    if (has_altivec)
       cpu |= RETRO_SIMD_VMX;
#elif defined(__linux__)
   cpu_flags = linux_get_cpu_features();

   if (cpu_flags & CPU_ARM_FEATURE_NEON)
   {
      cpu |= RETRO_SIMD_NEON;
#ifdef __ARM_NEON__
      arm_enable_runfast_mode();
#endif
   }

   if (cpu_flags & CPU_ARM_FEATURE_VFPv3)
      cpu |= RETRO_SIMD_VFPV3;

#elif defined(__ARM_NEON__)
   cpu |= RETRO_SIMD_NEON;
   arm_enable_runfast_mode();
#elif defined(__ALTIVEC__)
   cpu |= RETRO_SIMD_VMX;
#elif defined(XBOX360)
   cpu |= RETRO_SIMD_VMX128;
#elif defined(PSP)
   cpu |= RETRO_SIMD_VFPU;
#elif defined(GEKKO)
   cpu |= RETRO_SIMD_PS;
#endif

   if (cpu & RETRO_SIMD_MMX)    strlcat(buf, " MMX", sizeof(buf));
   if (cpu & RETRO_SIMD_MMXEXT) strlcat(buf, " MMXEXT", sizeof(buf));
   if (cpu & RETRO_SIMD_SSE)    strlcat(buf, " SSE", sizeof(buf));
   if (cpu & RETRO_SIMD_SSE2)   strlcat(buf, " SSE2", sizeof(buf));
   if (cpu & RETRO_SIMD_SSE3)   strlcat(buf, " SSE3", sizeof(buf));
   if (cpu & RETRO_SIMD_SSSE3)  strlcat(buf, " SSSE3", sizeof(buf));
   if (cpu & RETRO_SIMD_SSE4)   strlcat(buf, " SSE4", sizeof(buf));
   if (cpu & RETRO_SIMD_SSE42)  strlcat(buf, " SSE4.2", sizeof(buf));
   if (cpu & RETRO_SIMD_AES)    strlcat(buf, " AES", sizeof(buf));
   if (cpu & RETRO_SIMD_AVX)    strlcat(buf, " AVX", sizeof(buf));
   if (cpu & RETRO_SIMD_AVX2)   strlcat(buf, " AVX2", sizeof(buf));
   if (cpu & RETRO_SIMD_NEON)   strlcat(buf, " NEON", sizeof(buf));
   if (cpu & RETRO_SIMD_VFPV3)  strlcat(buf, " VFPv3", sizeof(buf));
   if (cpu & RETRO_SIMD_VFPV4)  strlcat(buf, " VFPv4", sizeof(buf));
   if (cpu & RETRO_SIMD_VMX)    strlcat(buf, " VMX", sizeof(buf));
   if (cpu & RETRO_SIMD_VMX128) strlcat(buf, " VMX128", sizeof(buf));
   if (cpu & RETRO_SIMD_VFPU)   strlcat(buf, " VFPU", sizeof(buf));
   if (cpu & RETRO_SIMD_PS)     strlcat(buf, " PS", sizeof(buf));

   RARCH_LOG("[CPUID]: Features:%s\n", buf);

   return cpu;
}

int rarch_perf_init(struct retro_perf_counter *perf, const char *name)
{
   perf->ident = name;

   if (!perf->registered)
      rarch_perf_register(perf);

   return 0;
}

void retro_perf_start(struct retro_perf_counter *perf)
{
   if (!runloop_ctl(RUNLOOP_CTL_IS_PERFCNT_ENABLE, NULL) || !perf)
      return;

   perf->call_cnt++;
   perf->start = retro_get_perf_counter();
}

void retro_perf_stop(struct retro_perf_counter *perf)
{
   if (!runloop_ctl(RUNLOOP_CTL_IS_PERFCNT_ENABLE, NULL) || !perf)
      return;

   perf->total += retro_get_perf_counter() - perf->start;
}
