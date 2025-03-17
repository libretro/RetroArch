/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (features_cpu.c).
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

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <compat/strl.h>
#include <streams/file_stream.h>
#include <libretro.h>
#include <features/features_cpu.h>
#include <retro_timers.h>

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#endif

#ifdef __PSL1GHT__
#include <lv2/systime.h>
#endif

#if defined(_XBOX360)
#include <PPCIntrinsics.h>
#elif !defined(__MACH__) && !defined(__FreeBSD__) && (defined(__POWERPC__) || defined(__powerpc__) || defined(__ppc__) || defined(__PPC64__) || defined(__powerpc64__))
#ifndef _PPU_INTRINSICS_H
#include <ppu_intrinsics.h>
#endif
#elif defined(_POSIX_MONOTONIC_CLOCK) || defined(ANDROID) || defined(__QNX__) || defined(DJGPP)
/* POSIX_MONOTONIC_CLOCK is not being defined in Android headers despite support being present. */
#include <time.h>
#endif

#if defined(__QNX__) && !defined(CLOCK_MONOTONIC)
#define CLOCK_MONOTONIC 2
#endif

#if defined(PSP)
#include <pspkernel.h>
#endif

#if defined(PSP) || defined(__PSL1GHT__)
#include <sys/time.h>
#endif

#if defined(PSP)
#include <psprtc.h>
#endif

#if defined(VITA)
#include <psp2/kernel/processmgr.h>
#include <psp2/rtc.h>
#endif

#if defined(ORBIS)
#include <orbis/libkernel.h>
#endif

#if defined(PS2)
#include <ps2sdkapi.h>
#endif

#if !defined(__PSL1GHT__) && defined(__PS3__)
#include <sys/sys_time.h>
#endif

#ifdef GEKKO
#include <ogc/lwp_watchdog.h>
#endif

#ifdef WIIU
#include <wiiu/os/time.h>
#endif

#if defined(HAVE_LIBNX)
#include <switch.h>
#elif defined(SWITCH)
#include <libtransistor/types.h>
#include <libtransistor/svc.h>
#endif

#if defined(_3DS)
#include <3ds/svc.h>
#include <3ds/os.h>
#include <3ds/services/cfgu.h>
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

/**
 * TODO/FIXME: clock_gettime function is part of iOS 10 now
 **/
#if __IPHONE_OS_VERSION_MIN_REQUIRED < 100000
static int ra_clock_gettime(int clk_ik, struct timespec *t)
{
   struct timeval now;
   int rv     = gettimeofday(&now, NULL);
   if (rv)
      return rv;
   t->tv_sec  = now.tv_sec;
   t->tv_nsec = now.tv_usec * 1000;
   return 0;
}
#endif
#endif

#if defined(__MACH__) && __IPHONE_OS_VERSION_MIN_REQUIRED < 100000
#else
#define ra_clock_gettime clock_gettime
#endif

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

#if defined(BSD) || defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#include <string.h>

retro_perf_tick_t cpu_features_get_perf_counter(void)
{
   retro_perf_tick_t time_ticks = 0;
#if defined(_WIN32)
   long tv_sec, tv_usec;
#if defined(_MSC_VER) && _MSC_VER <= 1200
   static const unsigned __int64 epoch = 11644473600000000;
#else
   static const unsigned __int64 epoch = 11644473600000000ULL;
#endif
   FILETIME file_time;
   SYSTEMTIME system_time;
   ULARGE_INTEGER ularge;

   GetSystemTime(&system_time);
   SystemTimeToFileTime(&system_time, &file_time);
   ularge.LowPart  = file_time.dwLowDateTime;
   ularge.HighPart = file_time.dwHighDateTime;

   tv_sec     = (long)((ularge.QuadPart - epoch) / 10000000L);
   tv_usec    = (long)(system_time.wMilliseconds * 1000);
   time_ticks = (1000000 * tv_sec + tv_usec);
#elif defined(GEKKO)
   time_ticks = gettime();
#elif !defined(__MACH__) && !defined(__FreeBSD__) && (defined(_XBOX360) || defined(__powerpc__) || defined(__ppc__) || defined(__POWERPC__) || defined(__PSL1GHT__) || defined(__PPC64__) || defined(__powerpc64__))
   time_ticks = __mftb();
#elif (defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK > 0) || defined(__QNX__) || defined(ANDROID)
   struct timespec tv;
   if (ra_clock_gettime(CLOCK_MONOTONIC, &tv) == 0)
      time_ticks = (retro_perf_tick_t)tv.tv_sec * 1000000000 +
         (retro_perf_tick_t)tv.tv_nsec;

#elif defined(__GNUC__) && defined(__i386__) || defined(__i486__) || defined(__i686__) || defined(_M_X64) || defined(_M_AMD64)
   __asm__ volatile ("rdtsc" : "=A" (time_ticks));
#elif defined(__GNUC__) && defined(__x86_64__) || defined(_M_IX86)
   unsigned a, d;
   __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
   time_ticks = (retro_perf_tick_t)a | ((retro_perf_tick_t)d << 32);
#elif defined(__ARM_ARCH_6__)
   __asm__ volatile( "mrc p15, 0, %0, c9, c13, 0" : "=r"(time_ticks) );
#elif defined(__aarch64__)
   __asm__ volatile( "mrs %0, cntvct_el0" : "=r"(time_ticks) );
#elif defined(PSP) || defined(VITA)
   time_ticks = sceKernelGetSystemTimeWide();
#elif defined(ORBIS)
   sceRtcGetCurrentTick((SceRtcTick*)&time_ticks);
#elif defined(PS2)
   time_ticks = ps2_clock();
#elif defined(_3DS)
   time_ticks = svcGetSystemTick();
#elif defined(WIIU)
   time_ticks = OSGetSystemTime();
#elif defined(HAVE_LIBNX)
   time_ticks = armGetSystemTick();
#elif defined(EMSCRIPTEN)
   time_ticks = emscripten_get_now() * 1000;
#endif

   return time_ticks;
}

retro_time_t cpu_features_get_time_usec(void)
{
#if defined(_WIN32)
   static LARGE_INTEGER freq;
   LARGE_INTEGER count;

   /* Frequency is guaranteed to not change. */
   if (!freq.QuadPart && !QueryPerformanceFrequency(&freq))
      return 0;

   if (!QueryPerformanceCounter(&count))
      return 0;
   return (count.QuadPart / freq.QuadPart * 1000000) + (count.QuadPart % freq.QuadPart * 1000000 / freq.QuadPart);
#elif defined(__PSL1GHT__)
   return sysGetSystemTime();
#elif !defined(__PSL1GHT__) && defined(__PS3__)
   return sys_time_get_system_time();
#elif defined(GEKKO)
   return ticks_to_microsecs(gettime());
#elif defined(WIIU)
   return ticks_to_us(OSGetSystemTime());
#elif defined(SWITCH) || defined(HAVE_LIBNX)
   return (svcGetSystemTick() * 10) / 192;
#elif defined(_3DS)
   return osGetTime() * 1000;
#elif defined(_POSIX_MONOTONIC_CLOCK) || defined(__QNX__) || defined(ANDROID) || defined(__MACH__)
   struct timespec tv;
   if (ra_clock_gettime(CLOCK_MONOTONIC, &tv) < 0)
      return 0;
   return tv.tv_sec * INT64_C(1000000) + (tv.tv_nsec + 500) / 1000;
#elif defined(EMSCRIPTEN)
   return emscripten_get_now() * 1000;
#elif defined(PS2)
   return ps2_clock() / PS2_CLOCKS_PER_MSEC * 1000;
#elif defined(VITA) || defined(PSP)
   return sceKernelGetSystemTimeWide();
#elif defined(DJGPP)
   return uclock() * 1000000LL / UCLOCKS_PER_SEC;
#elif defined(ORBIS)
   return sceKernelGetProcessTime();
#else
#error "Your platform does not have a timer function implemented in cpu_features_get_time_usec(). Cannot continue."
#endif
}

#if defined(__x86_64__) || defined(__i386__) || defined(__i486__) || defined(__i686__) || (defined(_M_X64) && _MSC_VER > 1310) || (defined(_M_IX86) && _MSC_VER > 1310)
#define CPU_X86
#endif

#if defined(_MSC_VER) && !defined(_XBOX)
#if (_MSC_VER > 1310)
#include <intrin.h>
#endif
#endif

#if defined(CPU_X86) && !defined(__MACH__)
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
#ifndef NDEBUG
   printf("Unknown compiler. Cannot check CPUID with inline assembly.\n");
#endif
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
#ifndef NDEBUG
   printf("Unknown compiler. Cannot check xgetbv bits.\n");
#endif
   return 0;
#endif
}
#endif

#if defined(__ARM_NEON__)
#if defined(__arm__)
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
#endif

#if defined(__linux__) && !defined(CPU_X86)
static unsigned char check_arm_cpu_feature(const char* feature)
{
   char line[1024];
   unsigned char status = 0;
   RFILE *fp = filestream_open("/proc/cpuinfo",
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fp)
      return 0;

   while (filestream_gets(fp, line, sizeof(line)))
   {
      if (strncmp(line, "Features\t: ", 11))
         continue;

      if (strstr(line + 11, feature))
         status = 1;

      break;
   }

   filestream_close(fp);

   return status;
}

#if !defined(_SC_NPROCESSORS_ONLN)
/**
 * parse_decimal:
 *
 * Parse an decimal integer starting from 'input', but not going further
 * than 'limit'. Return the value into '*result'.
 *
 * NOTE: Does not skip over leading spaces, or deal with sign characters.
 * NOTE: Ignores overflows.
 *
 * The function returns NULL in case of error (bad format), or the new
 * position after the decimal number in case of success (which will always
 * be <= 'limit').
 *
 * Leaf function.
 **/
static const char *parse_decimal(const char* input,
      const char* limit, int* result)
{
    const char* p = input;
    int       val = 0;

    while (p < limit)
    {
        int d = (*p - '0');
        if ((unsigned)d >= 10U)
            break;
        val = val*10 + d;
        p++;
    }
    if (p == input)
        return NULL;

    *result = val;
    return p;
}

/**
 * cpulist_parse:
 * Parse a textual list of cpus and store the result inside a CpuList object.
 * Input format is the following:
 * - comma-separated list of items (no spaces)
 * - each item is either a single decimal number (cpu index), or a range made
 *   of two numbers separated by a single dash (-). Ranges are inclusive.
 *
 * Examples:   0
 *             2,4-127,128-143
 *             0-1
 **/
static void cpulist_parse(CpuList* list, char **buf, ssize_t len)
{
   const char* p   = (const char*)buf;
   const char* end = p + len;

   /* NOTE: the input line coming from sysfs typically contains a
    * trailing newline, so take care of it in the code below
    */
   while (p < end && *p != '\n')
   {
      int val, start_value, end_value;
      /* Find the end of current item, and put it into 'q' */
      const char *q = (const char*)memchr(p, ',', end-p);

      if (!q)
         q = end;

      /* Get first value */
      if (!(p = parse_decimal(p, q, &start_value)))
         return;

      end_value = start_value;

      /* If we're not at the end of the item, expect a dash and
       * and integer; extract end value.
       */
      if (p < q && *p == '-')
      {
         if (!(p = parse_decimal(p+1, q, &end_value)))
            return;
      }

      /* Set bits CPU list bits */
      for (val = start_value; val <= end_value; val++)
      {
         if ((unsigned)val < 32)
            list->mask |= (uint32_t)(UINT32_C(1) << val);
      }

      /* Jump to next item */
      p = q;
      if (p < end)
         p++;
   }
}

/**
 * cpulist_read_from:
 *
 * Read a CPU list from one sysfs file
 **/
static void cpulist_read_from(CpuList* list, const char* filename)
{
   ssize_t _len;
   char *buf  = NULL;

   list->mask = 0;

   if (filestream_read_file(filename, (void**)&buf, &_len) != 1)
      return;

   cpulist_parse(list, &buf, _len);
   if (buf)
      free(buf);
   buf = NULL;
}
#endif

#endif

unsigned cpu_features_get_core_amount(void)
{
#if defined(_WIN32) && !defined(_XBOX)
   /* Win32 */
   SYSTEM_INFO sysinfo;
#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
   GetNativeSystemInfo(&sysinfo);
#else
   GetSystemInfo(&sysinfo);
#endif
   return sysinfo.dwNumberOfProcessors;
#elif defined(GEKKO)
   return 1;
#elif defined(PSP) || defined(PS2)
   return 1;
#elif defined(__PSL1GHT__) || !defined(__PSL1GHT__) && defined(__PS3__)
   return 1; /* Only one PPU, SPUs don't really count */
#elif defined(VITA)
   return 4;
#elif defined(HAVE_LIBNX) || defined(SWITCH)
   return 4;
#elif defined(_3DS)
   u8 device_model = 0xFF;
   CFGU_GetSystemModel(&device_model);/*(0 = O3DS, 1 = O3DSXL, 2 = N3DS, 3 = 2DS, 4 = N3DSXL, 5 = N2DSXL)*/
   switch (device_model)
   {
		case 0:
		case 1:
		case 3:
			/*Old 3/2DS*/
			return 2;

		case 2:
		case 4:
		case 5:
			/*New 3/2DS*/
			return 4;

		default:
			/*Unknown Device Or Check Failed*/
			break;
   }
   return 1;
#elif defined(WIIU)
   return 3;
#elif defined(_SC_NPROCESSORS_ONLN)
   /* Linux, most UNIX-likes. */
   long ret = sysconf(_SC_NPROCESSORS_ONLN);
   if (ret <= 0)
      return (unsigned)1;
   return (unsigned)ret;
#elif defined(BSD) || defined(__APPLE__)
   /* BSD */
   /* Copypasta from stackoverflow, dunno if it works. */
   int num_cpu = 0;
   int mib[4];
   size_t _len = sizeof(num_cpu);

   mib[0] = CTL_HW;
   mib[1] = HW_AVAILCPU;
   sysctl(mib, 2, &num_cpu, &_len, NULL, 0);
   if (num_cpu < 1)
   {
      mib[1] = HW_NCPU;
      sysctl(mib, 2, &num_cpu, &_len, NULL, 0);
      if (num_cpu < 1)
         num_cpu = 1;
   }
   return num_cpu;
#elif defined(__linux__)
   CpuList  cpus_present[1];
   CpuList  cpus_possible[1];
   int amount = 0;

   cpulist_read_from(cpus_present, "/sys/devices/system/cpu/present");
   cpulist_read_from(cpus_possible, "/sys/devices/system/cpu/possible");

   /* Compute the intersection of both sets to get the actual number of
    * CPU cores that can be used on this device by the kernel.
    */
   cpus_present->mask &= cpus_possible->mask;
   amount              = __builtin_popcount(cpus_present->mask);

   if (amount == 0)
      return 1;
   return amount;
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

uint64_t cpu_features_get(void)
{
   uint64_t cpu        = 0;
#if defined(CPU_X86) && !defined(__MACH__)
   int vendor_is_intel = 0;
   const int avx_flags = (1 << 27) | (1 << 28);
#endif
#if defined(__MACH__)
   size_t _len          = sizeof(size_t);
   if (sysctlbyname("hw.optional.floatingpoint", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_CMOV;

#if defined(CPU_X86)
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.mmx", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_MMX | RETRO_SIMD_MMXEXT;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.sse", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSE;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.sse2", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSE2;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.sse3", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSE3;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.supplementalsse3", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSSE3;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.sse4_1", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSE4;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.sse4_2", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSE42;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.aes", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_AES;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.avx1_0", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_AVX;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.avx2_0", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_AVX2;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.altivec", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_VMX;
#else
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.neon", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_NEON;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.neon_fp16", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_VFPV3;
   _len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.neon_hpfp", NULL, &_len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_VFPV4;
#endif
#elif defined(_XBOX1)
   cpu |= RETRO_SIMD_MMX | RETRO_SIMD_SSE | RETRO_SIMD_MMXEXT;
#elif defined(CPU_X86)
   unsigned max_flag   = 0;
   int flags[4];
   int vendor_shuffle[3];
   char vendor[13];
   x86_cpuid(0, flags);
   vendor_shuffle[0] = flags[1];
   vendor_shuffle[1] = flags[3];
   vendor_shuffle[2] = flags[2];

   vendor[0]         = '\0';
   memcpy(vendor, vendor_shuffle, sizeof(vendor_shuffle));

   /* printf("[CPUID]: Vendor: %s\n", vendor); */

   vendor_is_intel = (
         flags[1] == VENDOR_INTEL_b &&
         flags[2] == VENDOR_INTEL_c &&
         flags[3] == VENDOR_INTEL_d);

   max_flag = flags[0];
   if (max_flag < 1) /* Does CPUID not support func = 1? (unlikely ...) */
      return 0;

   x86_cpuid(1, flags);

   if (flags[3] & (1 << 15))
      cpu |= RETRO_SIMD_CMOV;

   if (flags[3] & (1 << 23))
      cpu |= RETRO_SIMD_MMX;

   /* SSE also implies MMXEXT (according to FFmpeg source). */
   if (flags[3] & (1 << 25))
      cpu |= RETRO_SIMD_SSE | RETRO_SIMD_MMXEXT;

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
#elif defined(__linux__)
   if (check_arm_cpu_feature("neon"))
   {
      cpu |= RETRO_SIMD_NEON;
#if defined(__ARM_NEON__) && defined(__arm__)
      arm_enable_runfast_mode();
#endif
   }

   if (check_arm_cpu_feature("vfpv3"))
      cpu |= RETRO_SIMD_VFPV3;

   if (check_arm_cpu_feature("vfpv4"))
      cpu |= RETRO_SIMD_VFPV4;

   if (check_arm_cpu_feature("asimd"))
   {
      cpu |= RETRO_SIMD_ASIMD;
#ifdef __ARM_NEON__
      cpu |= RETRO_SIMD_NEON;
#if defined(__arm__)
      arm_enable_runfast_mode();
#endif
#endif
   }

#if 0
    check_arm_cpu_feature("swp");
    check_arm_cpu_feature("half");
    check_arm_cpu_feature("thumb");
    check_arm_cpu_feature("fastmult");
    check_arm_cpu_feature("vfp");
    check_arm_cpu_feature("edsp");
    check_arm_cpu_feature("thumbee");
    check_arm_cpu_feature("tls");
    check_arm_cpu_feature("idiva");
    check_arm_cpu_feature("idivt");
#endif

#elif defined(__ARM_NEON__)
   cpu |= RETRO_SIMD_NEON;
#if defined(__arm__)
   arm_enable_runfast_mode();
#endif
#elif defined(__ALTIVEC__)
   cpu |= RETRO_SIMD_VMX;
#elif defined(XBOX360)
   cpu |= RETRO_SIMD_VMX128;
#elif defined(PSP) || defined(PS2)
   cpu |= RETRO_SIMD_VFPU;
#elif defined(GEKKO)
   cpu |= RETRO_SIMD_PS;
#endif

   return cpu;
}

void cpu_features_get_model_name(char *s, int len)
{
#if defined(CPU_X86) && !defined(__MACH__)
   union {
      int32_t i[4];
      uint32_t u[4];
      uint8_t s[16];
   } flags;
   int i, j;
   int pos = 0;
   bool start = false;

   if (!s)
      return;

   x86_cpuid(0x80000000, flags.i);

   /* Check for additional cpuid attributes availability */
   if (flags.u[0] < 0x80000004)
      return;

   for (i = 0; i < 3; i++)
   {
      memset(flags.i, 0, sizeof(flags.i));
      x86_cpuid(0x80000002 + i, flags.i);

      for (j = 0; j < (int)sizeof(flags.s); j++)
      {
         if (!start && flags.s[j] == ' ')
            continue;
         else
            start = true;

         if (pos == len - 1)
         {
            /* truncate if we ran out of room */
            s[pos] = '\0';
            goto end;
         }

         s[pos++] = flags.s[j];
      }
   }
end:
   /* terminate our string */
   if (pos < len)
      s[pos] = '\0';
#elif defined(__MACH__)
   if (!s)
      return;
   {
      size_t __len = len;
      sysctlbyname("machdep.cpu.brand_string", s, &__len, NULL, 0);
   }
#elif defined(__linux__)
   if (!s)
      return;
   {
      char *model_name, line[128];
      RFILE *fp = filestream_open("/proc/cpuinfo",
            RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);

      if (!fp)
         return;

      while (filestream_gets(fp, line, sizeof(line)))
      {
         if (strncmp(line, "model name", 10))
            continue;

         if ((model_name = strstr(line + 10, ": ")))
         {
            model_name += 2;
            strncpy(s, model_name, len);
            s[len - 1] = '\0';
         }

         break;
      }

      filestream_close(fp);
   }
#endif
}
