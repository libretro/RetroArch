/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cpu_class.h).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Per-processor performance class: the one source of truth shared by
 * cpu_features_get_processor_order() (features_cpu.c) and
 * sthread_prefer_fast_cores() / sthread_get_core_topology()
 * (rthreads.c), so a thread pinned "to the fast cores" and a thread
 * pinned "to the strongest processor" land on the same silicon.
 *
 * This is a private implementation header: every function is static
 * and the file is included by exactly the two .c files above, because
 * neither may link the other (rthreads.c is built on its own by the
 * sample harnesses, features_cpu.c by the cores).
 *
 * cpu_class_read() fills cls[id] for id < len with a class where a
 * higher value is a faster core, and returns the number of ids
 * covered, or 0 when the platform publishes no class at all - a
 * homogeneous part, or one this code cannot see into - in which case
 * every processor is to be taken as the same class.
 *
 * Linux, in order of trust:
 *  1. The kernel's own hybrid classification: on Intel hybrid parts
 *     the perf driver publishes the P-cores as cpu_core/cpus and the
 *     E-cores as cpu_atom/cpus.
 *  2. cpu_capacity, the scheduler's per-CPU throughput figure on ARM
 *     big.LITTLE, where the biggest cluster is 1024 and a little one a
 *     few hundred. A three-tier part (one prime, some big, some
 *     little) keeps its big cores in the fast class by taking
 *     everything within CPU_CLASS_CAPACITY_PCT of the best.
 *  3. cpuinfo_max_freq, within CPU_CLASS_FREQ_PCT of the highest. The
 *     band is what makes this usable on x86: favoured cores (Intel
 *     Turbo Boost Max 3.0, AMD CPPC preferred cores) and the two CCDs
 *     of an X3D part differ from their siblings by a few percent,
 *     while a real slow cluster sits 20-40% lower.
 * Windows 10 1607+: CPU Sets EfficiencyClass, which rises with
 *  performance (P = 1, E = 0 on Intel hybrid). Resolved at run time so
 *  older targets still link and simply report nothing.
 * Everything else: nothing.
 *
 * A builder may point CPU_CLASS_SYSFS at a fixture tree in place of
 * /sys/devices/system/cpu. */

#ifndef __LIBRETRO_SDK_CPU_CLASS_H
#define __LIBRETRO_SDK_CPU_CLASS_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>

#ifndef CPU_CLASS_SYSFS
#define CPU_CLASS_SYSFS "/sys/devices/system/cpu"
#endif
#define CPU_CLASS_CAPACITY_PCT 70
#define CPU_CLASS_FREQ_PCT     85
#define CPU_CLASS_MAX_IDS      256

#if defined(__linux__)

/* Reads one unsigned long from a sysfs file; 0 when absent or empty. */
static unsigned long cpu_class_sysfs_ulong(const char *path)
{
   unsigned long v = 0;
   FILE *f         = fopen(path, "r");
   if (!f)
      return 0;
   if (fscanf(f, "%lu", &v) != 1)
      v = 0;
   fclose(f);
   return v;
}

/* Parses a sysfs cpulist ("0-7,16-23") at path, setting mark[id] for
 * each id < len it names; returns whether the file existed and named
 * at least one processor. */
static bool cpu_class_sysfs_cpulist(const char *path,
      unsigned char *mark, size_t len)
{
   char  line[512];
   char *p;
   bool  any = false;
   FILE *f   = fopen(path, "r");
   if (!f)
      return false;
   line[0] = '\0';
   if (!fgets(line, sizeof(line), f))
      line[0] = '\0';
   fclose(f);
   for (p = line; *p; )
   {
      char *next;
      unsigned long lo, hi, i;
      if (*p < '0' || *p > '9')
      {
         p++;
         continue;
      }
      lo = hi = strtoul(p, &next, 10);
      if (*next == '-')
         hi = strtoul(next + 1, &next, 10);
      for (i = lo; i <= hi && i < len; i++)
      {
         mark[i] = 1;
         any     = true;
      }
      p = next;
   }
   return any;
}

/* Reads "<cpu>/<leaf>" for every id < len; sets cls[id] to 1 where the
 * value is within pct percent of the highest, 0 otherwise (including
 * ids with no value). Returns the highest id with a value plus one, or
 * 0 when none had one. */
static size_t cpu_class_by_value(const char *leaf, unsigned pct,
      unsigned char *cls, size_t len)
{
   unsigned long best = 0;
   size_t        i, n = 0;
   /* 32-bit is plenty: kHz clocks top out below 10^7 and capacities
    * at 1024; unsigned long would double the frame past the 2 KiB
    * thread-stack budget (PSP, GX). */
   unsigned      vals[CPU_CLASS_MAX_IDS];

   if (len > CPU_CLASS_MAX_IDS)
      len = CPU_CLASS_MAX_IDS;
   for (i = 0; i < len; i++)
   {
      char path[256];
      snprintf(path, sizeof(path), CPU_CLASS_SYSFS "/cpu%u/%s",
            (unsigned)i, leaf);
      vals[i] = (unsigned)cpu_class_sysfs_ulong(path);
      if (vals[i])
      {
         n = i + 1;
         if (vals[i] > best)
            best = vals[i];
      }
   }
   for (i = 0; i < len; i++)
      cls[i] = (vals[i] && vals[i] * 100 >= best * pct) ? 1 : 0;
   return n;
}

static size_t cpu_class_read(unsigned char *cls, size_t len)
{
   unsigned char atom[CPU_CLASS_MAX_IDS];
   size_t        i, n;

   if (len > CPU_CLASS_MAX_IDS)
      len = CPU_CLASS_MAX_IDS;
   memset(cls,  0, len);
   memset(atom, 0, sizeof(atom));

   /* 1. Intel hybrid, as the kernel sees it (/sys/devices/cpu_core and
    *    cpu_atom, beside /sys/devices/system). Only meaningful when
    *    both kinds exist: cpu_core alone is what a homogeneous Intel
    *    part publishes. */
   if (   cpu_class_sysfs_cpulist(CPU_CLASS_SYSFS "/../../cpu_core/cpus", cls, len)
       && cpu_class_sysfs_cpulist(CPU_CLASS_SYSFS "/../../cpu_atom/cpus", atom, len))
   {
      n = 0;
      for (i = 0; i < len; i++)
         if (cls[i] || atom[i])
            n = i + 1;
      return n;
   }
   memset(cls, 0, len);

   /* 2. Scheduler capacity (ARM), then 3. maximum clock. */
   if ((n = cpu_class_by_value("cpu_capacity", CPU_CLASS_CAPACITY_PCT, cls, len)))
      return n;
   return cpu_class_by_value("cpufreq/cpuinfo_max_freq", CPU_CLASS_FREQ_PCT,
         cls, len);
}

#elif defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)

#include <windows.h>

typedef BOOL (WINAPI *cpu_class_get_cpusets_t)(void*, ULONG, ULONG*,
      HANDLE, ULONG);

/* SYSTEM_CPU_SET_INFORMATION, read by offset since older SDKs lack the
 * type: Size at 0, Type at 4 (0 = CpuSet), Id at 8, Group at 12,
 * LogicalProcessorIndex at 14, CoreIndex at 15, EfficiencyClass at 18.
 * Ids are the processor's index within group 0, the numbering an
 * affinity mask is built from; other groups are beyond a 64-bit mask
 * and are left out. */
static size_t cpu_class_read(unsigned char *cls, size_t len)
{
   HMODULE k32                     = GetModuleHandleA("kernel32.dll");
   cpu_class_get_cpusets_t getinfo = NULL;
   unsigned char *buf              = NULL;
   ULONG len_needed                = 0;
   ULONG off;
   size_t n                        = 0;

   memset(cls, 0, len);
   if (k32)
      getinfo = (cpu_class_get_cpusets_t)(void (*)(void))
         GetProcAddress(k32, "GetSystemCpuSetInformation");
   if (!getinfo)
      return 0;
   getinfo(NULL, 0, &len_needed, GetCurrentProcess(), 0);
   if (!len_needed || !(buf = (unsigned char*)malloc(len_needed)))
      return 0;
   if (!getinfo(buf, len_needed, &len_needed, GetCurrentProcess(), 0))
   {
      free(buf);
      return 0;
   }
   for (off = 0; off + 20 <= len_needed; )
   {
      DWORD size = *(DWORD*)(buf + off);
      if (size < 20)
         break;
      if (*(DWORD*)(buf + off + 4) == 0 && *(WORD*)(buf + off + 12) == 0)
      {
         unsigned id = buf[off + 14];
         if (id < len)
         {
            cls[id] = buf[off + 18];
            if ((size_t)id + 1 > n)
               n = id + 1;
         }
      }
      off += size;
   }
   free(buf);
   return n;
}

#else

static size_t cpu_class_read(unsigned char *cls, size_t len)
{
   memset(cls, 0, len);
   return 0;
}

#endif

#endif
