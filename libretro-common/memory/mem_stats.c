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
