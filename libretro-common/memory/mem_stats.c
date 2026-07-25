/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (mem_stats.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* mem_stats -- how much memory the machine has and how much is spare.
 *
 * Every platform answers this differently and several answer it badly,
 * so the answers were scattered through the frontend drivers, one copy
 * each, reachable only from inside RetroArch. They are all here now:
 * the question is the platform's, not the frontend's, and the callers
 * that ask it - deciding whether a file can be read whole, whether a
 * thumbnail can be animated, whether an overlay can be built - are
 * asking about the machine.
 *
 * A platform with nothing to say returns 0 from both, which the header
 * defines as "unknown" rather than "none".
 */

#include <stdlib.h>

#include <memory/mem_stats.h>

#if defined(_3DS)
#include <3ds.h>
#elif defined(GEKKO)
#include <ogcsys.h>
#include <memory/mem2_manager.h>
#elif defined(VITA)
/* the bootstrap's sbrk defines the heap window this platform gets */
extern char *_newlib_heap_base, *_newlib_heap_end, *_newlib_heap_cur;
#elif defined(SWITCH) || defined(HAVE_LIBNX)
#ifdef HAVE_LIBNX
#include <switch.h>
#else
#include <malloc.h>
#endif
#elif defined(PS2)
/* nothing to include: the estimate below is made with malloc */
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
/* Apple's own TargetConditionals answers this, so no define of ours is
 * needed: TARGET_OS_IPHONE covers the iOS family, and an SDK old enough
 * not to define it at all - the 10.5-era ones the HW_PHYSMEM fallback
 * below exists for - leaves it evaluating to 0, which is the macOS arm
 * and correct for those.  TARGET_OS_OSX would read better but only
 * arrived with the 10.12 SDK. */
#elif defined(__WINRT__) || defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__unix__)
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#endif

#if defined(__linux__) || defined(__unix__)
#define MEM_STATS_PROC_MEMINFO   "/proc/meminfo"
#define MEM_STATS_TAG_TOTAL      "MemTotal:"
#define MEM_STATS_TAG_AVAILABLE  "MemAvailable:"
#define MEM_STATS_TAG_FREE       "MemFree:"
#define MEM_STATS_TAG_BUFFERS    "Buffers:"
#define MEM_STATS_TAG_CACHED     "Cached:"
#define MEM_STATS_TAG_SHMEM      "Shmem:"

/* One pass over /proc/meminfo, since every value wanted is in it and
 * the file is a virtual one that is cheapest read in a single go.
 * MemAvailable is the kernel's own estimate of what can be had without
 * swapping and is preferred where the kernel is new enough to publish
 * it; the older sum is the fallback. */
static void mem_stats_proc_meminfo(uint64_t *total, uint64_t *avail)
{
   char line[256];
   unsigned long mem_total = 0, mem_available = 0, mem_free = 0;
   unsigned long buffers = 0, cached = 0, shmem = 0;
   int have_available = 0;
   FILE *f = fopen(MEM_STATS_PROC_MEMINFO, "r");

   if (!f)
      return;
   while (fgets(line, sizeof(line), f))
   {
      if (!strncmp(line, MEM_STATS_TAG_TOTAL,
               sizeof(MEM_STATS_TAG_TOTAL) - 1))
         mem_total = strtoul(line + sizeof(MEM_STATS_TAG_TOTAL) - 1,
               NULL, 10);
      else if (!strncmp(line, MEM_STATS_TAG_AVAILABLE,
               sizeof(MEM_STATS_TAG_AVAILABLE) - 1))
      {
         mem_available  = strtoul(line
               + sizeof(MEM_STATS_TAG_AVAILABLE) - 1, NULL, 10);
         have_available = 1;
      }
      else if (!strncmp(line, MEM_STATS_TAG_FREE,
               sizeof(MEM_STATS_TAG_FREE) - 1))
         mem_free = strtoul(line + sizeof(MEM_STATS_TAG_FREE) - 1,
               NULL, 10);
      else if (!strncmp(line, MEM_STATS_TAG_BUFFERS,
               sizeof(MEM_STATS_TAG_BUFFERS) - 1))
         buffers = strtoul(line + sizeof(MEM_STATS_TAG_BUFFERS) - 1,
               NULL, 10);
      else if (!strncmp(line, MEM_STATS_TAG_CACHED,
               sizeof(MEM_STATS_TAG_CACHED) - 1))
         cached = strtoul(line + sizeof(MEM_STATS_TAG_CACHED) - 1,
               NULL, 10);
      else if (!strncmp(line, MEM_STATS_TAG_SHMEM,
               sizeof(MEM_STATS_TAG_SHMEM) - 1))
         shmem = strtoul(line + sizeof(MEM_STATS_TAG_SHMEM) - 1,
               NULL, 10);
   }
   fclose(f);

   if (total)
      *total = (uint64_t)mem_total * 1024;
   if (avail)
   {
      if (have_available)
         *avail = (uint64_t)mem_available * 1024;
      else
         *avail = (uint64_t)((mem_free + buffers + cached) - shmem) * 1024;
   }
}
#endif

static uint64_t (*mem_stats_total_cb)(void) = NULL;
static uint64_t (*mem_stats_free_cb)(void)  = NULL;

void mem_stats_set_provider(uint64_t (*total)(void),
      uint64_t (*free_mem)(void))
{
   mem_stats_total_cb = total;
   mem_stats_free_cb  = free_mem;
}

uint64_t mem_stats_total(void)
{
   if (mem_stats_total_cb)
      return mem_stats_total_cb();
#if defined(_3DS)
   return osGetMemRegionSize(MEMREGION_ALL);
#elif defined(GEKKO)
#if defined(HW_RVL) && !defined(IS_SALAMANDER)
   return SYSMEM1_SIZE + gx_mem2_total();
#else
   return SYSMEM1_SIZE;
#endif
#elif defined(VITA)
   return (uint64_t)(_newlib_heap_end - _newlib_heap_base);
#elif defined(HAVE_LIBNX)
   {
      uint64_t total = 0;
      if (R_SUCCEEDED(svcGetInfo(&total, InfoType_TotalMemorySize,
                  CUR_PROCESS_HANDLE, 0)))
         return total;
      return 0;
   }
#elif defined(SWITCH)
   {
      struct mallinfo mem_info = mallinfo();
      return mem_info.usmblks;
   }
#elif defined(__WINRT__)
   {
      MEMORYSTATUSEX mem_info;
      mem_info.dwLength = sizeof(MEMORYSTATUSEX);
      GlobalMemoryStatusEx(&mem_info);
      return mem_info.ullTotalPhys;
   }
#elif defined(_WIN32)
   /* the Ex form arrived with 2000, and the one before it cannot
    * describe more than 4GB */
#if _WIN32_WINNT >= 0x0500
   {
      MEMORYSTATUSEX mem_info;
      mem_info.dwLength = sizeof(MEMORYSTATUSEX);
      GlobalMemoryStatusEx(&mem_info);
      return mem_info.ullTotalPhys;
   }
#else
   {
      MEMORYSTATUS mem_info;
      mem_info.dwLength = sizeof(MEMORYSTATUS);
      GlobalMemoryStatus(&mem_info);
      return mem_info.dwTotalPhys;
   }
#endif
#elif defined(PS2)
   return 32 * 1024 * 1024;
#elif defined(__APPLE__)
#if !TARGET_OS_IPHONE
   {
      uint64_t size = 0;
#ifdef HW_MEMSIZE
      {
         /* 64-bit total; HW_MEMSIZE exists from 10.6 onward. */
         int    mib[2]  = { CTL_HW, HW_MEMSIZE };
         size_t len     = sizeof(size);
         if (sysctl(mib, 2, &size, &len, NULL, 0) >= 0 && size)
            return size;
      }
#endif
      {
         /* 10.5 fallback: HW_PHYSMEM is 32-bit and saturates near 2-4
          * GB, but it is all early kernels expose (and a 10.5 SDK may
          * not even define HW_MEMSIZE). */
         unsigned int psize = 0;
         int    mib[2]      = { CTL_HW, HW_PHYSMEM };
         size_t len         = sizeof(psize);
         if (sysctl(mib, 2, &psize, &len, NULL, 0) >= 0)
            return (uint64_t)psize;
      }
      return 0;
   }
#else /* the iOS family */
   {
      task_vm_info_data_t vm_info;
      mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
      if (task_info(mach_task_self(), TASK_VM_INFO,
               (task_info_t)&vm_info, &count) == KERN_SUCCESS)
         return vm_info.phys_footprint + vm_info.limit_bytes_remaining;
      return 0;
   }
#endif
#elif defined(__linux__) || defined(__unix__)
#if defined(DINGUX)
   /* sysconf is not to be trusted here, so ask the file */
   {
      uint64_t total = 0;
      mem_stats_proc_meminfo(&total, NULL);
      return total;
   }
#else
   {
      uint64_t pages     = (uint64_t)sysconf(_SC_PHYS_PAGES);
      uint64_t page_size = (uint64_t)sysconf(_SC_PAGE_SIZE);
      return pages * page_size;
   }
#endif
#else
   return 0;
#endif
}

uint64_t mem_stats_free(void)
{
   if (mem_stats_free_cb)
      return mem_stats_free_cb();
#if defined(_3DS)
   return osGetMemRegionFree(MEMREGION_ALL);
#elif defined(GEKKO)
   {
      /* SYS_GetArena1Size() reports remaining MEM1 directly. */
      uint64_t total = SYS_GetArena1Size();
#if defined(HW_RVL) && !defined(IS_SALAMANDER)
      total += (gx_mem2_total() - gx_mem2_used());
#endif
      return total;
   }
#elif defined(VITA)
   return (uint64_t)(_newlib_heap_end - _newlib_heap_cur);
#elif defined(HAVE_LIBNX)
   {
      uint64_t total = 0, used = 0;
      if (     R_SUCCEEDED(svcGetInfo(&total, InfoType_TotalMemorySize,
                  CUR_PROCESS_HANDLE, 0))
            && R_SUCCEEDED(svcGetInfo(&used, InfoType_UsedMemorySize,
                  CUR_PROCESS_HANDLE, 0)))
         return total - used;
      return 0;
   }
#elif defined(SWITCH)
   {
      struct mallinfo mem_info = mallinfo();
      return mem_info.fordblks;
   }
#elif defined(__WINRT__)
   {
      MEMORYSTATUSEX mem_info;
      mem_info.dwLength = sizeof(MEMORYSTATUSEX);
      GlobalMemoryStatusEx(&mem_info);
      return mem_info.ullAvailPhys;
   }
#elif defined(_WIN32)
#if _WIN32_WINNT >= 0x0500
   {
      MEMORYSTATUSEX mem_info;
      mem_info.dwLength = sizeof(MEMORYSTATUSEX);
      GlobalMemoryStatusEx(&mem_info);
      return mem_info.ullAvailPhys;
   }
#else
   {
      MEMORYSTATUS mem_info;
      mem_info.dwLength = sizeof(MEMORYSTATUS);
      GlobalMemoryStatus(&mem_info);
      return mem_info.dwAvailPhys;
   }
#endif
#elif defined(PS2)
   /* No accounting to ask, so find out by trying: take the largest
    * block that can be had, three times over, and hand back what was
    * managed.  The probe transiently holds most of RAM, so the answer
    * is worked out once and kept - callers such as the memory overlay
    * poll it. */
   {
      static uint64_t cached = 0;
      uint64_t free_mem;
      size_t s0;
      void *p1 = NULL, *p2 = NULL, *p3 = NULL;

      if (cached)
         return cached;
      s0 = 32 * 1024 * 1024;
      while (s0 && (p1 = malloc(s0)) == NULL)
         s0 >>= 1;
      free_mem = s0;
      s0 = 32 * 1024 * 1024;
      while (s0 && (p2 = malloc(s0)) == NULL)
         s0 >>= 1;
      free_mem += s0;
      s0 = 32 * 1024 * 1024;
      while (s0 && (p3 = malloc(s0)) == NULL)
         s0 >>= 1;
      free_mem += s0;
      if (p1)
         free(p1);
      if (p2)
         free(p2);
      if (p3)
         free(p3);
      cached = free_mem;
      return cached;
   }
#elif defined(__APPLE__)
#if !TARGET_OS_IPHONE
   /* Free plus reclaimable (inactive) pages, through the 32-bit
    * host_statistics interface: it exists back to 10.0 and runs on 10.5
    * kernels, unlike host_statistics64 (10.6+) or a task_vm_info
    * footprint path (10.12+).  Reporting total minus this process's own
    * footprint would ignore every other process and the OS with it. */
   {
      vm_size_t              page_size = 0;
      vm_statistics_data_t   vm_stat;
      mach_msg_type_number_t count     = HOST_VM_INFO_COUNT;
      mach_port_t            host      = mach_host_self();
      if (     host_page_size(host, &page_size) == KERN_SUCCESS
            && host_statistics(host, HOST_VM_INFO, (host_info_t)&vm_stat,
                  &count) == KERN_SUCCESS)
         return ((uint64_t)vm_stat.free_count
               + (uint64_t)vm_stat.inactive_count) * (uint64_t)page_size;
      return 0;
   }
#else /* the iOS family */
   {
      task_vm_info_data_t vm_info;
      mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
      if (task_info(mach_task_self(), TASK_VM_INFO,
               (task_info_t)&vm_info, &count) == KERN_SUCCESS)
         return vm_info.limit_bytes_remaining;
      return 0;
   }
#endif
#elif defined(__linux__) || defined(__unix__)
   {
      uint64_t avail = 0;
      mem_stats_proc_meminfo(NULL, &avail);
      return avail;
   }
#else
   return 0;
#endif
}
