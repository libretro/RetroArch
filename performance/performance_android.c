/*
 * Copyright (C) 2010 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* ChangeLog for this library:
 *
 * NDK r7c: Fix CPU count computation. The old method only reported the
 *           number of _active_ CPUs when the library was initialized,
 *           which could be less than the real total.
 *
 * NDK r5: Handle buggy kernels which report a CPU Architecture number of 7
 *         for an ARMv6 CPU (see below).
 *
 *         Handle kernels that only report 'neon', and not 'vfpv3'
 *         (VFPv3 is mandated by the ARM architecture is Neon is implemented)
 *
 *         Handle kernels that only report 'vfpv3d16', and not 'vfpv3'
 *
 *         Fix x86 compilation. Report ANDROID_CPU_FAMILY_X86 in
 *         android_getCpuFamily().
 *
 * NDK r4: Initial release
 */
#include <sys/system_properties.h>
#ifdef __arm__
#include <machine/cpu-features.h>
#endif
#include <pthread.h>
#include "performance_android.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

static  pthread_once_t     g_once;
static  AndroidCpuFamily   g_cpuFamily;
static  uint64_t           g_cpuFeatures;
static  int                g_cpuCount;

#ifdef __arm__
#  define DEFAULT_CPU_FAMILY  ANDROID_CPU_FAMILY_ARM
#elif defined __i386__
#  define DEFAULT_CPU_FAMILY  ANDROID_CPU_FAMILY_X86
#else
#  define DEFAULT_CPU_FAMILY  ANDROID_CPU_FAMILY_UNKNOWN
#endif

#ifdef __i386__
static __inline__ void cpu_x86_cpuid(int func, int values[4])
{
    int a, b, c, d;
    /* We need to preserve ebx since we're compiling PIC code */
    /* this means we can't use "=b" for the second output register */
    __asm__ __volatile__ ( \
      "push %%ebx\n"
      "cpuid\n" \
      "mov %1, %%ebx\n"
      "pop %%ebx\n"
      : "=a" (a), "=r" (b), "=c" (c), "=d" (d) \
      : "a" (func) \
    );
    values[0] = a;
    values[1] = b;
    values[2] = c;
    values[3] = d;
}
#endif

/* Read the content of /proc/cpuinfo into a user-provided buffer.
 * Return the length of the data, or -1 on error. Does *not*
 * zero-terminate the content. Will not read more
 * than 'buffsize' bytes.
 */
static int
cpu_read_file(const char*  pathname, char*  buffer, size_t  buffsize)
{
    int  fd, len;

    fd = open(pathname, O_RDONLY);
    if (fd < 0)
        return -1;

    do {
        len = read(fd, buffer, buffsize);
    } while (len < 0 && errno == EINTR);

    close(fd);

    return len;
}

/* Extract the content of a the first occurence of a given field in
 * the content of /proc/cpuinfo and return it as a heap-allocated
 * string that must be freed by the caller.
 *
 * Return NULL if not found
 */
static char*
extract_cpuinfo_field(char* buffer, int buflen, const char* field)
{
   int  fieldlen = strlen(field);
   char* bufend = buffer + buflen;
   char* result = NULL;
   int len;
   const char *p, *q;

   /* Look for first field occurence, and ensures it starts the line.
   */
   p = buffer;
   bufend = buffer + buflen;
   for (;;)
   {
      p = memmem(p, bufend-p, field, fieldlen);
      if (p == NULL)
         goto EXIT;

      if (p == buffer || p[-1] == '\n')
         break;

      p += fieldlen;
   }

   /* Skip to the first column followed by a space */
   p += fieldlen;
   p  = memchr(p, ':', bufend-p);
   if (p == NULL || p[1] != ' ')
      goto EXIT;

   /* Find the end of the line */
   p += 2;
   q = memchr(p, '\n', bufend-p);
   if (q == NULL)
      q = bufend;

   /* Copy the line into a heap-allocated buffer */
   len = q-p;
   result = malloc(len+1);
   if (result == NULL)
      goto EXIT;

   memcpy(result, p, len);
   result[len] = '\0';

EXIT:
   return result;
}

/* Checks that a space-separated list of items contains one given 'item'.
 * Returns 1 if found, 0 otherwise.
 */
static int
has_list_item(const char* list, const char* item)
{
    const char*  p = list;
    int itemlen = strlen(item);

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
static const char*
parse_decimal(const char* input, const char* limit, int* result)
{
    const char* p = input;
    int val = 0;
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

/* This small data type is used to represent a CPU list / mask, as read
 * from sysfs on Linux. See http://www.kernel.org/doc/Documentation/cputopology.txt
 *
 * For now, we don't expect more than 32 cores on mobile devices, so keep
 * everything simple.
 */
typedef struct {
    uint32_t mask;
} CpuList;

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
static void cpulist_parse(CpuList* list, const char* line, int line_len)
{
   const char* p = line;
   const char* end = p + line_len;
   const char* q;

   /* NOTE: the input line coming from sysfs typically contains a
    * trailing newline, so take care of it in the code below
    */
   while (p < end && *p != '\n')
   {
      int val, start_value, end_value;

      /* Find the end of current item, and put it into 'q' */
      q = memchr(p, ',', end-p);
      if (q == NULL)
         q = end;

      /* Get first value */
      p = parse_decimal(p, q, &start_value);
      if (p == NULL)
         goto BAD_FORMAT;

      end_value = start_value;

      /* If we're not at the end of the item, expect a dash and
       * and integer; extract end value.
       */
      if (p < q && *p == '-')
      {
         p = parse_decimal(p+1, q, &end_value);
         if (p == NULL)
            goto BAD_FORMAT;
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

BAD_FORMAT:
   ;
}

/* Read a CPU list from one sysfs file */
static void
cpulist_read_from(CpuList* list, const char* filename)
{
   char   file[64];
   int    filelen;

   list->mask = 0;

   filelen = cpu_read_file(filename, file, sizeof file);

   if (filelen < 0)
   {
      RARCH_ERR("Could not read %s: %s\n", filename, strerror(errno));
      return;
   }

   cpulist_parse(list, file, filelen);
}

/* Return the number of cpus present on a given device.
 *
 * To handle all weird kernel configurations, we need to compute the
 * intersection of the 'present' and 'possible' CPU lists and count
 * the result.
 */
static int get_cpu_count(void)
{
   CpuList cpus_present[1];
   CpuList cpus_possible[1];

   cpulist_read_from(cpus_present, "/sys/devices/system/cpu/present");
   cpulist_read_from(cpus_possible, "/sys/devices/system/cpu/possible");

   /* Compute the intersection of both sets to get the actual number of
    * CPU cores that can be used on this device by the kernel.
    */
   cpus_present->mask &= cpus_possible->mask;

   return __builtin_popcount(cpus_present->mask);
}

static void android_cpuInit(void)
{
   char cpuinfo[4096];
   int  cpuinfo_len;

   g_cpuFamily   = DEFAULT_CPU_FAMILY;
   g_cpuFeatures = 0;
   g_cpuCount    = 1;

   cpuinfo_len = cpu_read_file("/proc/cpuinfo", cpuinfo, sizeof cpuinfo);
   RARCH_LOG("cpuinfo_len is (%d):\n%.*s\n", cpuinfo_len,
         cpuinfo_len >= 0 ? cpuinfo_len : 0, cpuinfo);

   if (cpuinfo_len < 0)
      return;

   /* Count the CPU cores, the value may be 0 for single-core CPUs */
   g_cpuCount = get_cpu_count();
   if (g_cpuCount == 0)
      g_cpuCount = 1;

   RARCH_LOG("found cpuCount = %d\n", g_cpuCount);

#ifdef __ARM_ARCH__
   /* Extract architecture from the "CPU Architecture" field.
    * The list is well-known, unlike the the output of
    * the 'Processor' field which can vary greatly.
    *
    * See the definition of the 'proc_arch' array in
    * $KERNEL/arch/arm/kernel/setup.c and the 'c_show' function in
    * same file.
    */
   char* cpuArch = extract_cpuinfo_field(cpuinfo, cpuinfo_len, "CPU architecture");

   if (cpuArch != NULL)
   {
      char*  end;
      long   archNumber;
      int    hasARMv7 = 0;

      RARCH_LOG("found cpuArch = '%s'\n", cpuArch);

      /* read the initial decimal number, ignore the rest */
      archNumber = strtol(cpuArch, &end, 10);

      /* Here we assume that ARMv8 will be upwards compatible with v7
       * in the future. Unfortunately, there is no 'Features' field to
       * indicate that Thumb-2 is supported.
       */
      if (end > cpuArch && archNumber >= 7)
         hasARMv7 = 1;

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
      if (hasARMv7)
      {
         char* cpuProc = extract_cpuinfo_field(cpuinfo, cpuinfo_len,
               "Processor");
         if (cpuProc != NULL)
         {
            RARCH_LOG("found cpuProc = '%s'\n", cpuProc);
            if (has_list_item(cpuProc, "(v6l)"))
            {
               RARCH_ERR("CPU processor and architecture mismatch!!\n");
               hasARMv7 = 0;
            }
            free(cpuProc);
         }
      }

      if (hasARMv7)
         g_cpuFeatures |= ANDROID_CPU_ARM_FEATURE_ARMv7;

      /* The LDREX / STREX instructions are available from ARMv6 */
      if (archNumber >= 6)
         g_cpuFeatures |= ANDROID_CPU_ARM_FEATURE_LDREX_STREX;

      free(cpuArch);
   }

   /* Extract the list of CPU features from 'Features' field */
   char* cpuFeatures = extract_cpuinfo_field(cpuinfo, cpuinfo_len, "Features");

   if (cpuFeatures != NULL)
   {
      RARCH_LOG("found cpuFeatures = '%s'\n", cpuFeatures);

      if (has_list_item(cpuFeatures, "vfpv3"))
         g_cpuFeatures |= ANDROID_CPU_ARM_FEATURE_VFPv3;

      else if (has_list_item(cpuFeatures, "vfpv3d16"))
         g_cpuFeatures |= ANDROID_CPU_ARM_FEATURE_VFPv3;

      if (has_list_item(cpuFeatures, "neon"))
      {
         /* Note: Certain kernels only report neon but not vfpv3
          *       in their features list. However, ARM mandates
          *       that if Neon is implemented, so must be VFPv3
          *       so always set the flag.
          */
         g_cpuFeatures |= ANDROID_CPU_ARM_FEATURE_NEON |
            ANDROID_CPU_ARM_FEATURE_VFPv3;
      }
      free(cpuFeatures);
   }
#endif /* __ARM_ARCH__ */

#ifdef __i386__
   g_cpuFamily = ANDROID_CPU_FAMILY_X86;

   int regs[4];

   /* According to http://en.wikipedia.org/wiki/CPUID */
#define VENDOR_INTEL_b  0x756e6547
#define VENDOR_INTEL_c  0x6c65746e
#define VENDOR_INTEL_d  0x49656e69

   cpu_x86_cpuid(0, regs);
   int vendorIsIntel = (regs[1] == VENDOR_INTEL_b &&
         regs[2] == VENDOR_INTEL_c &&
         regs[3] == VENDOR_INTEL_d);

   cpu_x86_cpuid(1, regs);
   if ((regs[2] & (1 << 9)) != 0)
      g_cpuFeatures |= ANDROID_CPU_X86_FEATURE_SSSE3;
   if ((regs[2] & (1 << 23)) != 0)
      g_cpuFeatures |= ANDROID_CPU_X86_FEATURE_POPCNT;
   if (vendorIsIntel && (regs[2] & (1 << 22)) != 0)
      g_cpuFeatures |= ANDROID_CPU_X86_FEATURE_MOVBE;
#endif

#ifdef _MIPS_ARCH
   g_cpuFamily = ANDROID_CPU_FAMILY_MIPS;
#endif /* _MIPS_ARCH */
}

AndroidCpuFamily android_getCpuFamily(void)
{
    pthread_once(&g_once, android_cpuInit);
    return g_cpuFamily;
}

uint64_t android_getCpuFeatures(void)
{
    pthread_once(&g_once, android_cpuInit);
    return g_cpuFeatures;
}

int android_getCpuCount(void)
{
    pthread_once(&g_once, android_cpuInit);
    return g_cpuCount;
}
