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
#include <stdlib.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <compat/strl.h>
#include <streams/file_stream.h>
#include <libretro.h>

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#include <intrin.h>
#endif

#if defined(__CELLOS_LV2__)
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

/**
 * cpu_features_get_perf_counter:
 *
 * Gets performance counter.
 *
 * Returns: performance counter.
 **/
retro_perf_tick_t cpu_features_get_perf_counter(void)
{
   retro_perf_tick_t time_ticks = 0;
#if defined(__linux__) || defined(__QNX__) || defined(__MACH__)
   struct timespec tv;
   if (clock_gettime(CLOCK_MONOTONIC, &tv) == 0)
      time_ticks = (retro_perf_tick_t)tv.tv_sec * 1000000000 +
         (retro_perf_tick_t)tv.tv_nsec;

#elif defined(__GNUC__) && defined(__i386__) || defined(__i486__) || defined(__i686__)
   __asm__ volatile ("rdtsc" : "=A" (time_ticks));
#elif defined(__GNUC__) && defined(__x86_64__)
   unsigned a, d;
   __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
   time_ticks = (retro_perf_tick_t)a | ((retro_perf_tick_t)d << 32);
#elif defined(__ARM_ARCH_6__)
   __asm__ volatile( "mrc p15, 0, %0, c9, c13, 0" : "=r"(time_ticks) );
#elif defined(__CELLOS_LV2__) || defined(_XBOX360) || defined(__powerpc__) || defined(__ppc__) || defined(__POWERPC__)
   time_ticks = __mftb();
#elif defined(GEKKO)
   time_ticks = gettime();
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
 * cpu_features_get_time_usec:
 *
 * Gets time in microseconds.
 *
 * Returns: time in microseconds.
 **/
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
#error "Your platform does not have a timer function implemented in cpu_features_get_time_usec(). Cannot continue."
#endif
}

#if defined(__x86_64__) || defined(__i386__) || defined(__i486__) || defined(__i686__)
#define CPU_X86
#endif

#if defined(_MSC_VER) && !defined(_XBOX)
#include <intrin.h>
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
   printf("Unknown compiler. Cannot check CPUID with inline assembly.\n");
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
   printf("Unknown compiler. Cannot check xgetbv bits.\n");
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

#if defined(__linux__)

#ifdef __ARM_ARCH__
/* Extract the content of a the first occurrence of a given field in
 * the content of /proc/cpuinfo and return it as a heap-allocated
 * string that must be freed by the caller.
 *
 * Return NULL if not found
 */
static char *extract_cpuinfo_field(char* buffer,
      ssize_t length, const char* field)
{
   int len;
   const char *q;
   int  fieldlen = strlen(field);
   char* bufend  = buffer + length;
   char* result  = NULL;
   /* Look for first field occurrence,
    * and ensures it starts the line. */
   const char *p = buffer;

   for (;;)
   {
      p = memmem(p, bufend-p, field, fieldlen);
      if (p == NULL)
         return result;

      if (p == buffer || p[-1] == '\n')
         break;

      p += fieldlen;
   }

   /* Skip to the first column followed by a space */
   p += fieldlen;
   p  = memchr(p, ':', bufend-p);
   if (p == NULL || p[1] != ' ')
      return result;

   /* Find the end of the line */
   p += 2;
   q  = memchr(p, '\n', bufend-p);
   if (q == NULL)
      q = bufend;

   /* Copy the line into a heap-allocated buffer */
   len    = q-p;
   result = malloc(len+1);
   if (result == NULL)
      return result;

   memcpy(result, p, len);
   result[len] = '\0';

   return result;
}

/* Checks that a space-separated list of items
 * contains one given 'item'.
 * Returns 1 if found, 0 otherwise.
 */
static int has_list_item(const char* list, const char* item)
{
    const char*  p = list;
    int    itemlen = strlen(item);

    if (list == NULL)
        return 0;

    while (*p)
    {
        const char*  q;

        /* skip spaces */
        while (*p == ' ' || *p == '\t')
            p++;

        /* find end of current list item */
        q = p;
        while (*q && *q != ' ' && *q != '\t')
            q++;

        if (itemlen == q-p && !memcmp(p, item, itemlen))
            return 1;

        /* skip to next item */
        p = q;
    }
    return 0;
}
#endif

typedef struct
{
    uint32_t mask;
} CpuList;


#if !defined(_SC_NPROCESSORS_ONLN)
/* Parse an decimal integer starting from 'input', but not going further
 * than 'limit'. Return the value into '*result'.
 *
 * NOTE: Does not skip over leading spaces, or deal with sign characters.
 * NOTE: Ignores overflows.
 *
 * The function returns NULL in case of error (bad format), or the new
 * position after the decimal number in case of success (which will always
 * be <= 'limit').
 */
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

/* Parse a textual list of cpus and store the result inside a CpuList object.
 * Input format is the following:
 * - comma-separated list of items (no spaces)
 * - each item is either a single decimal number (cpu index), or a range made
 *   of two numbers separated by a single dash (-). Ranges are inclusive.
 *
 * Examples:   0
 *             2,4-127,128-143
 *             0-1
 */
static void cpulist_parse(CpuList* list, char **buf, ssize_t length)
{
   const char* p   = (const char*)buf;
   const char* end = p + length;

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
      p = parse_decimal(p, q, &start_value);
      if (p == NULL)
         return;

      end_value = start_value;

      /* If we're not at the end of the item, expect a dash and
       * and integer; extract end value.
       */
      if (p < q && *p == '-')
      {
         p = parse_decimal(p+1, q, &end_value);
         if (p == NULL)
            return;
      }

      /* Set bits CPU list bits */
      for (val = start_value; val <= end_value; val++)
      {
         if ((unsigned)val < 32)
            list->mask |= (uint32_t)(1U << val);
      }

      /* Jump to next item */
      p = q;
      if (p < end)
         p++;
   }
}

/* Read a CPU list from one sysfs file */
static void cpulist_read_from(CpuList* list, const char* filename)
{
   ssize_t length;
   char *buf  = NULL;

   list->mask = 0;

   if (filestream_read_file(filename, (void**)&buf, &length) != 1)
      return;

   cpulist_parse(list, &buf, length);
   if (buf)
      free(buf);
   buf = NULL;
}
#endif

#endif

/**
 * cpu_features_get_core_amount:
 *
 * Gets the amount of available CPU cores.
 *
 * Returns: amount of CPU cores available.
 **/
unsigned cpu_features_get_core_amount(void)
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

/**
 * cpu_features_get:
 *
 * Gets CPU features..
 *
 * Returns: bitmask of all CPU features available.
 **/
uint64_t cpu_features_get(void)
{
   int flags[4];
   int vendor_shuffle[3];
   char vendor[13]     = {0};
   size_t len          = 0;
   uint64_t cpu_flags  = 0;
   uint64_t cpu        = 0;
   unsigned max_flag   = 0;
#if defined(CPU_X86) && !defined(__MACH__)
   int vendor_is_intel = 0;
   const int avx_flags = (1 << 27) | (1 << 28);
#endif

   char buf[sizeof(" MMX MMXEXT SSE SSE2 SSE3 SSSE3 SS4 SSE4.2 AES AVX AVX2 NEON VMX VMX128 VFPU PS")];

   memset(buf, 0, sizeof(buf));

   (void)len;
   (void)cpu_flags;
   (void)flags;
   (void)max_flag;
   (void)vendor;
   (void)vendor_shuffle;

#if defined(__MACH__)
   len     = sizeof(size_t);
   if (sysctlbyname("hw.optional.mmx", NULL, &len, NULL, 0) == 0)
   {
      cpu |= RETRO_SIMD_MMX;
      cpu |= RETRO_SIMD_MMXEXT;
   }

   len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.sse", NULL, &len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSE;

   len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.sse2", NULL, &len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSE2;

   len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.sse3", NULL, &len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSE3;

   len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.supplementalsse3", NULL, &len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSSE3;

   len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.sse4_1", NULL, &len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSE4;

   len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.sse4_2", NULL, &len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_SSE42;

   len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.aes", NULL, &len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_AES;

   len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.avx1_0", NULL, &len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_AVX;

   len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.avx2_0", NULL, &len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_AVX2;

   len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.altivec", NULL, &len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_VMX;

   len            = sizeof(size_t);
   if (sysctlbyname("hw.optional.neon", NULL, &len, NULL, 0) == 0)
      cpu |= RETRO_SIMD_NEON;

#elif defined(CPU_X86)
   (void)avx_flags;

   x86_cpuid(0, flags);
   vendor_shuffle[0] = flags[1];
   vendor_shuffle[1] = flags[3];
   vendor_shuffle[2] = flags[2];
   memcpy(vendor, vendor_shuffle, sizeof(vendor_shuffle));

   printf("[CPUID]: Vendor: %s\n", vendor);

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
#elif defined(__linux__)

   static bool cpu_inited_once = false;
   static  uint64_t g_cpuFeatures;

   enum
   {
      CPU_ARM_FEATURE_ARMv7       = (1 << 0),
      CPU_ARM_FEATURE_VFPv3       = (1 << 1),
      CPU_ARM_FEATURE_NEON        = (1 << 2),
      CPU_ARM_FEATURE_LDREX_STREX = (1 << 3)
   };

   if (!cpu_inited_once)
   {
      ssize_t  length;
      void *buf = NULL;

      g_cpuFeatures = 0;

      if (filestream_read_file("/proc/cpuinfo", &buf, &length) == 1)
      {
#ifdef __ARM_ARCH__
         /* Extract architecture from the "CPU Architecture" field.
          * The list is well-known, unlike the the output of
          * the 'Processor' field which can vary greatly.
          *
          * See the definition of the 'proc_arch' array in
          * $KERNEL/arch/arm/kernel/setup.c and the 'c_show' function in
          * same file.
          */
         char* cpu_arch = extract_cpuinfo_field(buf, length, "CPU architecture");

         if (cpu_arch)
         {
            char*  end;
            int    has_armv7 = 0;
            /* read the initial decimal number, ignore the rest */
            long   arch_number = strtol(cpu_arch, &end, 10);

            RARCH_LOG("Found CPU architecture = '%s'\n", cpu_arch);

            /* Here we assume that ARMv8 will be upwards compatible with v7
             * in the future. Unfortunately, there is no 'Features' field to
             * indicate that Thumb-2 is supported.
             */
            if (end > cpu_arch && arch_number >= 7)
               has_armv7 = 1;

            /* Unfortunately, it seems that certain ARMv6-based CPUs
             * report an incorrect architecture number of 7!
             *
             * See http://code.google.com/p/android/issues/detail?id=10812
             *
             * We try to correct this by looking at the 'elf_format'
             * field reported by the 'Processor' field, which is of the
             * form of "(v7l)" for an ARMv7-based CPU, and "(v6l)" for
             * an ARMv6-one.
             */
            if (has_armv7)
            {
               char *cpu_proc = extract_cpuinfo_field(buf, length,
                     "Processor");

               if (cpu_proc != NULL)
               {
                  RARCH_LOG("found cpu_proc = '%s'\n", cpu_proc);
                  if (has_list_item(cpu_proc, "(v6l)"))
                  {
                     /* CPU processor and architecture mismatch. */
                     has_armv7 = 0;
                  }
                  free(cpu_proc);
               }
            }

            if (has_armv7)
               g_cpuFeatures |= CPU_ARM_FEATURE_ARMv7;

            /* The LDREX / STREX instructions are available from ARMv6 */
            if (arch_number >= 6)
               g_cpuFeatures |= CPU_ARM_FEATURE_LDREX_STREX;

            free(cpu_arch);
         }

         /* Extract the list of CPU features from 'Features' field */
         char* cpu_features = extract_cpuinfo_field(buf, length, "Features");

         if (cpu_features)
         {
            RARCH_LOG("found cpu_features = '%s'\n", cpu_features);

            if (has_list_item(cpu_features, "vfpv3"))
               g_cpuFeatures |= CPU_ARM_FEATURE_VFPv3;

            else if (has_list_item(cpu_features, "vfpv3d16"))
               g_cpuFeatures |= CPU_ARM_FEATURE_VFPv3;

            /* Note: Certain kernels only report NEON but not VFPv3
             * in their features list. However, ARM mandates
             * that if NEON is implemented, so must be VFPv3
             * so always set the flag.
             */
            if (has_list_item(cpu_features, "neon"))
               g_cpuFeatures |= CPU_ARM_FEATURE_NEON | CPU_ARM_FEATURE_VFPv3;
            free(cpu_features);
         }
#endif /* __ARM_ARCH__ */

         if (buf)
            free(buf);
         buf = NULL;
      }
      cpu_inited_once = true;
   }

   cpu_flags = g_cpuFeatures;

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

   return cpu;
}
