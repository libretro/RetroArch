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
#include <libretro.h>
#include <features/features_cpu.h>
#include <retro_atomic.h>
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

#if defined(WEBOS)
#include <sys/stat.h>
#endif

/* iOS/OSX specific. Lacks clock_gettime(), so implement it. */
#ifdef __MACH__
#include <sys/time.h>
#include <AvailabilityMacros.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 0
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

/* clock_gettime() was added in iOS 10.0 / macOS 10.12 Sierra.
 * On deployment targets below those versions the symbol doesn't
 * exist in libSystem and the link fails, so fall back to a
 * gettimeofday() shim.  Gated on MIN_REQUIRED (deployment target)
 * rather than MAX_ALLOWED (SDK) because the binary needs to run on
 * the deployment target, and Apple weak-links clock_gettime when
 * targeting < 10.12 even from a newer SDK. */
#if (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) \
       && __IPHONE_OS_VERSION_MIN_REQUIRED < 100000) \
 || (defined(MAC_OS_X_VERSION_MIN_REQUIRED) \
       && MAC_OS_X_VERSION_MIN_REQUIRED < 101200)
#define RA_NEEDS_CLOCK_GETTIME_SHIM 1
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

#if defined(__MACH__) && defined(RA_NEEDS_CLOCK_GETTIME_SHIM)
#else
#define ra_clock_gettime clock_gettime
#endif

#ifdef __EMSCRIPTEN__
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
#elif defined(__EMSCRIPTEN__)
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
   tv.tv_sec  = 0;
   tv.tv_nsec = 0;
   if (ra_clock_gettime(CLOCK_MONOTONIC, &tv) < 0)
      return 0;
   return tv.tv_sec * INT64_C(1000000) + (tv.tv_nsec + 500) / 1000;
#elif defined(__EMSCRIPTEN__)
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
#include <limits.h>
void x86_cpuid(uint32_t func, int32_t flags[4])
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
#elif defined(_MSC_VER) && INT_MAX == 2147483647
   __cpuid((int*)flags, func);
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

/* RunFast mode is a 32-bit VFP control, and this writes FPSCR - so it
 * needs both an ARM32 target and an FPU worth configuring.  NEON
 * implies VFP, which is why its presence is the proxy used here; a
 * VFP-less core such as the armv5te arm926ej-s would fault on the
 * write.
 *
 * CPU_ARM_RUNFAST is the single condition for the definition AND
 * every call.  They were separate conditions and drifted: the calls
 * ended up reachable where the definition was not, which is a link
 * error on exactly the targets with no NEON - Miyoo armv5te and
 * armv7 builds without an FPU selected. */
#if (defined(__ARM_NEON) || defined(__ARM_NEON__)) && defined(__arm__)
#define CPU_ARM_RUNFAST 1
#endif

#ifdef CPU_ARM_RUNFAST
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
#endif /* CPU_ARM_RUNFAST */

#if defined(__linux__) && !defined(CPU_X86)
static unsigned char check_arm_cpu_feature(const char* feature)
{
   char line[1024];
   unsigned char status = 0;
   FILE *fp = fopen("/proc/cpuinfo", "r");

   if (!fp)
      return 0;

   while (fgets(line, sizeof(line), fp))
   {
      const char *list;
      const char *p;
      size_t flen;

      if (strncmp(line, "Features\t: ", 11))
         continue;

      /* Feature names are space separated and several are prefixes of
       * others - 'aes' sits inside 'sveaes', 'sha3' inside 'svesha3',
       * 'asimd' inside 'asimddp' - so compare whole tokens. */
      flen = strlen(feature);
      list = line + 11;

      for (p = list; (p = strstr(p, feature)); p += flen)
      {
         char after  = p[flen];
         char before = (p == list) ? ' ' : p[-1];
         if (     (before == ' ')
               && (after == ' ' || after == '\n' || after == '\0'))
         {
            status = 1;
            break;
         }
      }

      break;
   }

   fclose(fp);

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
static void cpulist_parse(CpuList* list, const char *buf, ssize_t len)
{
   const char* p   = buf;
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
   char   buf[512];
   size_t _len;
   FILE  *fp  = fopen(filename, "r");

   list->mask = 0;

   if (!fp)
      return;

   _len      = fread(buf, 1, sizeof(buf) - 1, fp);
   fclose(fp);
   buf[_len] = '\0';

   cpulist_parse(list, buf, (ssize_t)_len);
}
#endif

#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
/* GetLogicalProcessorInformation arrived in XP SP3 and Server 2003, so
 * it is resolved rather than linked: on Win9x, NT 4 and 2000 the export
 * is absent, GetProcAddress reports so, and the caller falls back to
 * the plain processor count. The Ex form carrying EfficiencyClass is
 * Windows 7 and later and would rank efficiency cores, which this does
 * not attempt. */
typedef BOOL (WINAPI *cpu_glpi_t)(
      PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

/* Caller frees. Returns NULL, leaving *count untouched, wherever the
 * export or the query is unavailable. */
static SYSTEM_LOGICAL_PROCESSOR_INFORMATION *cpu_win32_slpi(DWORD *count)
{
   SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buf = NULL;
   DWORD       _len = 0;
   HMODULE     k32  = GetModuleHandleA("kernel32.dll");
   cpu_glpi_t  glpi = k32 ? (cpu_glpi_t)(void (*)(void))
      GetProcAddress(k32, "GetLogicalProcessorInformation") : NULL;

   if (!glpi)
      return NULL;

   /* The first call is expected to fail, and reports the size wanted. */
   if (glpi(NULL, &_len))
      return NULL;
   if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || _len == 0)
      return NULL;
   if (_len % sizeof(*buf))
      return NULL;

   if (!(buf = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)malloc(_len)))
      return NULL;

   if (!glpi(buf, &_len))
   {
      free(buf);
      return NULL;
   }

   *count = _len / (DWORD)sizeof(*buf);
   return buf;
}
#endif

#if defined(__linux__)
/* Number of distinct SMT sibling groups under
 * /sys/devices/system/cpu, which is one per physical core. The list a
 * sibling file holds names every processor sharing that core, so the
 * lowest id in it identifies the group and counting distinct ones
 * counts cores. */
static unsigned linux_core_amount_physical(unsigned logical)
{
   char     path[64];
   char     line[64];
   unsigned seen[128];
   unsigned n_seen = 0;
   unsigned found  = 0;
   unsigned i;

   /* One entry per core, so a machine wider than the table is left to
    * the logical count rather than answered wrongly. */
   if (logical > (unsigned)(sizeof(seen) / sizeof(seen[0])))
      return logical;

   for (i = 0; i < 256 && found < logical; i++)
   {
      FILE    *fp;
      unsigned first;
      unsigned j;

      snprintf(path, sizeof(path),
            "/sys/devices/system/cpu/cpu%u/topology/thread_siblings_list", i);

      if (!(fp = fopen(path, "r")))
         continue;

      line[0] = '\0';
      if (!fgets(line, sizeof(line), fp))
      {
         fclose(fp);
         continue;
      }
      fclose(fp);

      found++;

      if (sscanf(line, "%u", &first) != 1)
         continue;

      for (j = 0; j < n_seen; j++)
         if (seen[j] == first)
            break;

      if (j == n_seen && n_seen < (unsigned)(sizeof(seen) / sizeof(seen[0])))
         seen[n_seen++] = first;
   }

   /* Nothing readable, so the kernel is not publishing topology here. */
   if (n_seen == 0)
      return logical;

   return n_seen;
}
#endif

#if defined(__linux__)
/* First unsigned in a one-line sysfs file, or @fallback where the file
 * is missing or unreadable. */
static unsigned sysfs_read_uint(const char *path, unsigned fallback)
{
   char     line[64];
   unsigned val;
   FILE    *fp = fopen(path, "r");

   if (!fp)
      return fallback;

   line[0] = '\0';
   if (!fgets(line, sizeof(line), fp))
   {
      fclose(fp);
      return fallback;
   }
   fclose(fp);

   if (sscanf(line, "%u", &val) != 1)
      return fallback;
   return val;
}
#endif

#if (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)) || defined(__linux__)
struct cpu_proc_rank
{
   unsigned freq; /* kHz, higher is a stronger core */
   unsigned id;   /* OS processor identifier */
   unsigned smt;  /* 0 for the first processor on its core, else 1 */
};

/* Strongest core first, a core ahead of its own SMT siblings, and the
 * identifier as the tie-break so the result does not depend on the
 * order the entries were gathered in. */
static int cpu_proc_rank_cmp(const void *a, const void *b)
{
   const struct cpu_proc_rank *l = (const struct cpu_proc_rank *)a;
   const struct cpu_proc_rank *r = (const struct cpu_proc_rank *)b;

   if (l->freq != r->freq)
      return (l->freq > r->freq) ? -1 : 1;
   if (l->smt  != r->smt)
      return (l->smt  < r->smt)  ? -1 : 1;
   if (l->id   != r->id)
      return (l->id   < r->id)   ? -1 : 1;
   return 0;
}
#endif

size_t cpu_features_get_processor_order(unsigned *s, size_t len)
{
   size_t n = 0;

   if (!s || !len)
      return 0;

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   {
      DWORD count = 0;
      SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buf = cpu_win32_slpi(&count);

      if (buf)
      {
         struct cpu_proc_rank *rank;
         size_t cap = (size_t)cpu_features_get_core_amount();

         if (cap < 1)
            cap = 1;
         if (cap > 1024)
            cap = 1024;

         if ((rank = (struct cpu_proc_rank *)
                  malloc(cap * sizeof(struct cpu_proc_rank))))
         {
            DWORD i;
            for (i = 0; i < count && n < cap; i++)
            {
               unsigned  bit;
               unsigned  seen = 0;
               ULONG_PTR mask = buf[i].ProcessorMask;

               if (buf[i].Relationship != RelationProcessorCore)
                  continue;

               /* One mask bit per processor on this core, the lowest
                * of them being the one an SMT sibling shares with. */
               for (bit = 0; bit < sizeof(ULONG_PTR) * 8 && n < cap; bit++)
               {
                  if (!(mask & (((ULONG_PTR)1) << bit)))
                     continue;
                  rank[n].freq = 0;
                  rank[n].id   = bit;
                  rank[n].smt  = seen ? 1 : 0;
                  seen++;
                  n++;
               }
            }

            if (n)
            {
               qsort(rank, n, sizeof(struct cpu_proc_rank),
                     cpu_proc_rank_cmp);
               if (n > len)
                  n = len;
               for (i = 0; i < (DWORD)n; i++)
                  s[i] = rank[i].id;
            }
            free(rank);
         }
         free(buf);

         if (n)
            return n;
      }
   }
#endif

#if defined(__linux__)
   {
      struct cpu_proc_rank *rank;
      char     path[96];
      unsigned i;
      /* Every processor is ranked before any is handed back: gathering
       * only the first @len of them would sort a set chosen by
       * identifier and hand back the weakest cores on a layout that
       * numbers the little cluster first. */
      size_t   cap = (size_t)cpu_features_get_core_amount();

      if (cap < 1)
         cap = 1;
      if (cap > 1024)
         cap = 1024;

      if (!(rank = (struct cpu_proc_rank *)
               malloc(cap * sizeof(struct cpu_proc_rank))))
         return 0;

      for (i = 0; i < 1024 && n < cap; i++)
      {
         unsigned first;

         snprintf(path, sizeof(path),
               "/sys/devices/system/cpu/cpu%u/topology/thread_siblings_list", i);
         /* A processor with no sibling list is one the kernel is not
          * publishing, rather than one that shares no core. */
         first = sysfs_read_uint(path, (unsigned)-1);
         if (first == (unsigned)-1)
            continue;

         snprintf(path, sizeof(path),
               "/sys/devices/system/cpu/cpu%u/cpufreq/cpuinfo_max_freq", i);

         rank[n].freq = sysfs_read_uint(path, 0);
         rank[n].id   = i;
         rank[n].smt  = (i == first) ? 0 : 1;
         n++;
      }

      if (n)
      {
         qsort(rank, n, sizeof(struct cpu_proc_rank), cpu_proc_rank_cmp);
         if (n > len)
            n = len;
         for (i = 0; i < (unsigned)n; i++)
            s[i] = rank[i].id;
      }

      free(rank);

      if (n)
         return n;
   }
#endif

   /* No topology to rank by, so name each processor once in order. */
   {
      unsigned amount = cpu_features_get_core_amount();
      for (n = 0; n < len && n < (size_t)amount; n++)
         s[n] = (unsigned)n;
   }

   return n;
}

unsigned cpu_features_get_core_amount_physical(void)
{
   unsigned logical = cpu_features_get_core_amount();

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   {
      DWORD count = 0;
      SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buf = cpu_win32_slpi(&count);

      if (buf)
      {
         DWORD    i;
         unsigned cores = 0;
         for (i = 0; i < count; i++)
            if (buf[i].Relationship == RelationProcessorCore)
               cores++;
         free(buf);
         if (cores > 0)
            return cores;
      }
   }
#elif defined(__APPLE__)
   {
      /* Darwin publishes both counts, so the physical one is a read
       * rather than a derivation. */
      int    val  = 0;
      size_t _len = sizeof(val);
      if (   sysctlbyname("hw.physicalcpu", &val, &_len, NULL, 0) == 0
          && val > 0)
         return (unsigned)val;
   }
#elif defined(__linux__)
   return linux_core_amount_physical(logical);
#endif

   /* Every other target either has no SMT to discount or publishes no
    * way to tell, and the thread count is the safe answer in both
    * cases. */
   return logical;
}

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

#if defined(__MACH__) && defined(CPU_X86)
/* Whole-token search of one of the machdep.cpu.*features sysctls, each
 * of which is a space separated list of CPUID feature names. */
static bool darwin_cpu_feature_present(const char *key, const char *want)
{
   char   buf[1024];
   size_t len  = sizeof(buf);
   size_t wlen = strlen(want);
   const char *p;

   buf[0] = '\0';
   if (sysctlbyname(key, buf, &len, NULL, 0) != 0)
      return false;
   buf[sizeof(buf) - 1] = '\0';

   for (p = buf; (p = strstr(p, want)); p += wlen)
   {
      char after  = p[wlen];
      char before = (p == buf) ? ' ' : p[-1];
      if ((before == ' ') && (after == ' ' || after == '\0'))
         return true;
   }
   return false;
}
#endif

static uint64_t cpu_features_probe(void)
{
   uint64_t cpu        = 0;
#if defined(CPU_X86) && !defined(__MACH__)
   int vendor_is_intel = 0;
   /* Set once the OS is known to preserve YMM state, which AVX, FMA3
    * and FMA4 all depend on. */
   int ymm_state       = 0;
   const int avx_flags = (1 << 27) | (1 << 28);
#endif
#if defined(__MACH__)
   /* sysctlbyname() returns 0 (success) whenever the key exists, regardless
    * of its value. On Intel Macs the hw.optional.* keys are always present
    * but report 0 when the feature is unsupported (e.g. avx512f on pre-Skylake
    * Xeon CPUs). We therefore have to read the value, not just the success
    * code. */
   int      _val        = 0;
   size_t   _len        = sizeof(_val);
   if (   sysctlbyname("hw.optional.floatingpoint", &_val, &_len, NULL, 0) == 0
       && _val)
      cpu |= RETRO_SIMD_CMOV;

#if defined(CPU_X86)
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.mmx", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_MMX | RETRO_SIMD_MMXEXT;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.sse", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_SSE;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.sse2", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_SSE2;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.sse3", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_SSE3;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.supplementalsse3", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_SSSE3;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.sse4_1", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_SSE4;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.sse4_2", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_SSE42;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.aes", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_AES;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.avx1_0", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_AVX;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.fma", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_FMA3;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.avx2_0", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_AVX2;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.avx512f", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_AVX512;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.altivec", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_VMX;
   /* Darwin publishes no hw.optional key for PCLMULQDQ, so read the
    * CPUID leaf-1 feature-name list instead.  Matching is on whole
    * space-separated tokens: a plain strstr() would also fire on a
    * hypothetical future "PCLMULQDQ2". */
   if (darwin_cpu_feature_present("machdep.cpu.features", "PCLMULQDQ"))
      cpu |= RETRO_SIMD_PCLMUL;
   /* SHA-NI lives in the leaf-7 list rather than the leaf-1 one. */
   if (darwin_cpu_feature_present("machdep.cpu.leaf7_features", "SHA"))
      cpu |= RETRO_SIMD_SHA1 | RETRO_SIMD_SHA256;
#else
   _val = 0;
   _len = sizeof(_val);
   /* Older key first; newer systems also carry the FEAT_ spelling. */
   if (   (sysctlbyname("hw.optional.armv8_crc32", &_val, &_len, NULL, 0) == 0
           && _val)
       || (_val = 0, _len = sizeof(_val),
           sysctlbyname("hw.optional.arm.FEAT_CRC32", &_val, &_len, NULL, 0) == 0
           && _val))
      cpu |= RETRO_SIMD_CRC32;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.arm.FEAT_AES", &_val, &_len, NULL, 0) == 0
         && _val)
      cpu |= RETRO_SIMD_AES;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.arm.FEAT_SHA512", &_val, &_len, NULL, 0) == 0
         && _val)
      cpu |= RETRO_SIMD_SHA512;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.arm.FEAT_SHA1", &_val, &_len, NULL, 0) == 0
         && _val)
      cpu |= RETRO_SIMD_SHA1;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.arm.FEAT_SHA256", &_val, &_len, NULL, 0) == 0
         && _val)
      cpu |= RETRO_SIMD_SHA256;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.neon", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_NEON;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.neon_fp16", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_VFPV3;
   _val = 0;
   _len = sizeof(_val);
   if (sysctlbyname("hw.optional.neon_hpfp", &_val, &_len, NULL, 0) == 0 && _val)
      cpu |= RETRO_SIMD_VFPV4;
#endif
#elif defined(_XBOX1)
   cpu |= RETRO_SIMD_MMX | RETRO_SIMD_SSE | RETRO_SIMD_MMXEXT;
#elif defined(CPU_X86)
   unsigned max_flag   = 0;
   int32_t flags[4];
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
            flags[1] == VENDOR_INTEL_b 
         && flags[2] == VENDOR_INTEL_c
         && flags[3] == VENDOR_INTEL_d);

   /* CPUID register contents are unsigned; flags[] is int32_t, so the
    * widening is spelled out rather than left implicit -- leaf
    * 0x80000000 legitimately returns values with the top bit set. */
   max_flag = (uint32_t)flags[0];
   /* Does CPUID not support func = 1? (unlikely ...) */
   if (max_flag < 1) 
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

   if (flags[2] & (1 << 1))
      cpu |= RETRO_SIMD_PCLMUL;

   /* Must only perform xgetbv check if we have
    * AVX CPU support (guaranteed to have at least i686). */
   if (((flags[2] & avx_flags) == avx_flags)
         && ((xgetbv_x86(0) & 0x6) == 0x6))
   {
      ymm_state = 1;
      cpu      |= RETRO_SIMD_AVX;
   }

   /* FMA3 accumulates in YMM, so it answers only where the OS keeps
    * that state. Leaf 1 ECX is still in flags here; the extended leaf
    * carrying FMA4 is read further down. */
   if (ymm_state && (flags[2] & (1 << 12)))
      cpu |= RETRO_SIMD_FMA3;

   if (max_flag >= 7)
   {
      /* CPUID leaf 7 sub-leaf 0 requires ECX = 0.
       * x86_cpuid() does not set ECX, so call cpuid directly. */
      int32_t flags7[4];
#if defined(__GNUC__)
      __asm__ volatile (
            "mov %%" REG_b ", %%" REG_S "\n"
            "cpuid\n"
            "xchg %%" REG_b ", %%" REG_S "\n"
            : "=a"(flags7[0]), "=S"(flags7[1]), "=c"(flags7[2]), "=d"(flags7[3])
            : "a"(7), "c"(0));
#elif defined(_MSC_VER) && INT_MAX == 2147483647
#if _MSC_VER >= 1600
      __cpuidex((int*)flags7, 7, 0);
#else
      {
         int *p = (int*)flags7;
         __asm {
            mov eax, 7
            xor ecx, ecx
            cpuid
            mov esi, p
            mov [esi],      eax
            mov [esi + 4],  ebx
            mov [esi + 8],  ecx
            mov [esi + 12], edx
         }
      }
#endif
#else
      memset(flags7, 0, sizeof(flags7));
#endif

      if (flags7[1] & (1 << 5))
         cpu |= RETRO_SIMD_AVX2;

      /* SHA-NI is one bit covering both digests. */
      if (flags7[1] & (1 << 29))
         cpu |= RETRO_SIMD_SHA1 | RETRO_SIMD_SHA256;

      /* AVX-512 Foundation detection.
       * Requires CPUID leaf 7 sub-leaf 0 EBX bit 16 (AVX-512F),
       * and OS support for saving ZMM state:
       * xgetbv XCR0 bits 1,2 (SSE/AVX) and 5,6,7 (opmask, ZMM_Hi256, Hi16_ZMM). */
      if ((flags7[1] & (1 << 16))
            && ((xgetbv_x86(0) & 0xe6) == 0xe6))
         cpu |= RETRO_SIMD_AVX512;

      /* Leaf 7 EAX carries the highest sub-leaf the CPU implements, so
       * consult it before asking for sub-leaf 1: an out of range
       * sub-leaf returns zeroes on current parts but is not documented
       * to. */
      if (flags7[0] >= 1)
      {
         int32_t flags71[4];
#if defined(__GNUC__)
         __asm__ volatile (
               "mov %%" REG_b ", %%" REG_S "\n"
               "cpuid\n"
               "xchg %%" REG_b ", %%" REG_S "\n"
               : "=a"(flags71[0]), "=S"(flags71[1]), "=c"(flags71[2]), "=d"(flags71[3])
               : "a"(7), "c"(1));
#elif defined(_MSC_VER) && INT_MAX == 2147483647
#if _MSC_VER >= 1600
         __cpuidex((int*)flags71, 7, 1);
#else
         {
            int *p = (int*)flags71;
            __asm {
               mov eax, 7
               mov ecx, 1
               cpuid
               mov esi, p
               mov [esi],      eax
               mov [esi + 4],  ebx
               mov [esi + 8],  ecx
               mov [esi + 12], edx
            }
         }
#endif
#else
         memset(flags71, 0, sizeof(flags71));
#endif

         if (flags71[0] & (1 << 0))
            cpu |= RETRO_SIMD_SHA512;
      }
   }

   x86_cpuid(0x80000000, flags);
   max_flag = (uint32_t)flags[0];
   if (max_flag >= 0x80000001u)
   {
      x86_cpuid(0x80000001, flags);
      if (flags[3] & (1 << 23))
         cpu |= RETRO_SIMD_MMX;
      if (flags[3] & (1 << 22))
         cpu |= RETRO_SIMD_MMXEXT;
      /* LZCNT / ABM (ECX bit 5). macOS x86 uses the sysctl path above and
       * does not report this bit; consumers there fall back accordingly. */
      if (flags[2] & (1 << 5))
         cpu |= RETRO_SIMD_LZCNT;
      /* FMA4, an AMD encoding that no Zen part implements, under the
       * same OS state as FMA3. */
      if (ymm_state && (flags[2] & (1 << 16)))
         cpu |= RETRO_SIMD_FMA4;
   }
#elif defined(__linux__)
   if (check_arm_cpu_feature("neon"))
   {
      cpu |= RETRO_SIMD_NEON;
#ifdef CPU_ARM_RUNFAST
      arm_enable_runfast_mode();
#endif
   }

   if (check_arm_cpu_feature("vfpv3"))
      cpu |= RETRO_SIMD_VFPV3;

   if (check_arm_cpu_feature("vfpv4"))
      cpu |= RETRO_SIMD_VFPV4;

   /* Part of the optional Cryptographic Extension, which an
    * implementation may leave out or hold in reset, so a 64-bit ARM
    * CPU does not imply it. */
   if (check_arm_cpu_feature("aes"))
      cpu |= RETRO_SIMD_AES;

   /* aarch64 lists it as "crc32" in the Features: line. */
   if (check_arm_cpu_feature("crc32"))
      cpu |= RETRO_SIMD_CRC32;

   if (check_arm_cpu_feature("sha512"))
      cpu |= RETRO_SIMD_SHA512;

   if (check_arm_cpu_feature("sha1"))
      cpu |= RETRO_SIMD_SHA1;

   if (check_arm_cpu_feature("sha2"))
      cpu |= RETRO_SIMD_SHA256;

   if (check_arm_cpu_feature("asimd"))
   {
      cpu |= RETRO_SIMD_ASIMD;

      /* ASIMD *is* NEON: Advanced SIMD is what aarch64 kernels call
       * it in the Features: line, where 32-bit ARM says "neon".  It
       * is architecturally mandatory in ARMv8-A, so a CPU reporting
       * asimd has NEON by definition and callers testing
       * RETRO_SIMD_NEON must see it.
       *
       * This used to be gated on __ARM_NEON__, which is the LEGACY
       * 32-bit spelling.  aarch64 toolchains define __ARM_NEON (no
       * trailing underscores) instead - verified against the NDK's
       * own compiler, which reports __ARM_NEON 1 and no __ARM_NEON__
       * for aarch64-linux-android - so the guard was false on every
       * 64-bit build and NEON was never reported there.  That is why
       * System Information listed ASIMD alone on a device whose CPU
       * has had NEON since it was designed. */
      cpu |= RETRO_SIMD_NEON;

#ifdef CPU_ARM_RUNFAST
      arm_enable_runfast_mode();
#endif
   }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
   cpu |= RETRO_SIMD_NEON;
#ifdef CPU_ARM_RUNFAST
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

/* The probe is not cheap: on x86 it issues a chain of serialising CPUID
 * instructions, and on ARM/Linux it opens and line-parses /proc/cpuinfo
 * once per feature queried.  The answer cannot change over the lifetime
 * of the process, so probe once and hand out the cached mask thereafter.
 * Callers that sit on a per-frame or per-asset path (the image decoders
 * select their SIMD kernels this way) were paying the full probe on every
 * single call. */
uint64_t cpu_features_get(void)
{
   /* The mask is published with a release store and consumed with an
    * acquire load, so a thread that observes the ready flag is
    * guaranteed to see the fully written mask.
    *
    * The probe being idempotent is not on its own enough: if several
    * threads reach it before any has published, they all write
    * cpu_features_cache with plain stores, and those writes are
    * unordered with respect to one another.  Same value or not, that
    * is a data race and ThreadSanitizer reports it.  Elect exactly one
    * writer with an atomic increment; the threads that lose return the
    * value they computed themselves, which is identical, so nobody
    * spins and nobody writes memory another thread is writing. */
   static uint64_t           cpu_features_cache;
   static retro_atomic_int_t cpu_features_claim; /* 0 = unclaimed */
   static retro_atomic_int_t cpu_features_ready; /* 0 = not probed yet */
   uint64_t                  cpu;

   if (retro_atomic_load_acquire_int(&cpu_features_ready))
      return cpu_features_cache;

   cpu = cpu_features_probe();

   if (retro_atomic_fetch_add_int(&cpu_features_claim, 1) == 0)
   {
      cpu_features_cache = cpu;
      retro_atomic_store_release_int(&cpu_features_ready, 1);
   }

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
   int pos    = 0;
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
      FILE *fp = fopen("/proc/cpuinfo", "r");

      if (!fp)
         return;

      while (fgets(line, sizeof(line), fp))
      {
         if (strncmp(line, "model name", 10))
            continue;

         if ((model_name = strstr(line + 10, ": ")))
         {
            model_name += 2;
            strlcpy(s, model_name, len);
         }

         break;
      }

      fclose(fp);

#if defined(WEBOS)
      struct stat st;
      if (stat("/usr/bin/lscpu", &st) == 0)
      {
         FILE *pipe = popen("/usr/bin/lscpu", "r");
         if (pipe)
         {
            char buf[256];

            while (fgets(buf, sizeof(buf), pipe))
            {
               if (strncmp(buf, "Model name:", 11) == 0)
               {
                  const char *p = strchr(buf, ':');
                  if (p)
                  {
                     size_t len2;
                     p++; /* skip ':' */
                     while (*p == ' ' || *p == '\t')
                        p++;
                     len2 = strcspn(p, "\r\n");

                     if (len2 > 0)
                     {
                        char *tmp = malloc(len2 + 1);
                        if (tmp)
                        {
                           memcpy(tmp, p, len2);
                           tmp[len2] = '\0';

                           if (s[0] != '\0')
                           {
                              size_t oldlen  = strlen(s);
                              char *combined = (char*)malloc(oldlen + len2 + 4);
                              if (combined)
                              {
                                 memcpy(combined, s, oldlen);
                                 combined[oldlen]     = ' ';
                                 combined[oldlen + 1] = '(';
                                 memcpy(combined + oldlen + 2, tmp, len2);
                                 combined[oldlen + 2 + len2]     = ')';
                                 combined[oldlen + 2 + len2 + 1] = '\0';

                                 strlcpy(s, combined, len);
                                 free(combined);
                              }
                           }
                           else
                              strlcpy(s, tmp, len);
                           free(tmp);
                        }
                     }
                  }
                  break;
               }
            }
            pclose(pipe);
         }
      }
#endif
   }
#endif
}
