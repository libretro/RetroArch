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
#include <string/stdstring.h>

/* Pick exactly one backend, once, and key the includes and the bodies
 * below off that same choice.  They were once two separate chains, and
 * a platform matching an early arm while still defining __unix__ -
 * Orbis and Emscripten do, DJGPP does - took its own arm's includes and
 * a different arm's code. */
#if defined(_3DS)
#define MEM_STATS_CTR         1
#elif defined(GEKKO)
#define MEM_STATS_GX          1
#elif defined(VITA)
#define MEM_STATS_VITA        1
#elif defined(HAVE_LIBNX)
#define MEM_STATS_LIBNX       1
#elif defined(SWITCH)
#define MEM_STATS_SWITCH      1
#elif defined(ORBIS)
#define MEM_STATS_ORBIS       1
#elif (defined(__CELLOS_LV2__) || defined(__PSL1GHT__)) && defined(HAVE_MEMINFO)
#define MEM_STATS_PS3         1
#elif defined(PS2)
#define MEM_STATS_PS2         1
#elif defined(__EMSCRIPTEN__)
#define MEM_STATS_EMSCRIPTEN  1
#elif defined(__APPLE__)
#define MEM_STATS_APPLE       1
#elif defined(__WINRT__) || defined(_WIN32)
#define MEM_STATS_WIN32       1
/* DJGPP defines __unix__ and has neither /proc nor the sysconf names */
#elif (defined(__linux__) || defined(__unix__)) && !defined(__DJGPP__)
#define MEM_STATS_PROC        1
#endif

#if defined(MEM_STATS_CTR)
#include <3ds.h>
/* osGetMemRegionSize/Free and MEMREGION_ALL are declared here */
#include <3ds/os.h>
#elif defined(MEM_STATS_GX)
/* SYSMEM1_SIZE is RetroArch's own, not the SDK's - platform_gx.c and
 * gx_gfx.c both take it from here.  SYS_GetArena1Size is gccore's. */
#include <defines/gx_defines.h>
#include <gccore.h>
#include <ogcsys.h>
#include <memory/mem2_manager.h>
#elif defined(MEM_STATS_VITA)
/* the bootstrap's sbrk defines the heap window this platform gets */
extern char *_newlib_heap_base, *_newlib_heap_end, *_newlib_heap_cur;
#elif defined(MEM_STATS_LIBNX)
#include <switch.h>
#elif defined(MEM_STATS_SWITCH)
#include <malloc.h>
#elif defined(MEM_STATS_ORBIS)
#include <orbis/libkernel.h>
#elif defined(MEM_STATS_PS3)
#ifdef __PSL1GHT__
#include <ppu-lv2.h>
#endif
/* Neither SDK declares this: the syscall is reached by number, and the
 * shape it fills in is spelled out here, as the frontend used to. */
typedef struct
{
   uint32_t total;
   uint32_t avail;
} sys_memory_info_t;
#ifdef __PSL1GHT__
#define sys_memory_get_user_memory_size(x) lv2syscall1(352, x)
#else
#define sys_memory_get_user_memory_size(x) system_call_1(352, x)
#endif
#elif defined(MEM_STATS_EMSCRIPTEN)
#include <emscripten/heap.h>
#include <malloc.h>
#elif defined(MEM_STATS_APPLE)
#include <TargetConditionals.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#elif defined(MEM_STATS_WIN32)
#include <windows.h>
#elif defined(MEM_STATS_PROC)
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#if defined(MEM_STATS_PROC)
#define MEM_STATS_PROC_MEMINFO   "/proc/meminfo"
#define MEM_STATS_TAG_TOTAL      "MemTotal:"
#define MEM_STATS_TAG_AVAILABLE  "MemAvailable:"
#define MEM_STATS_TAG_FREE       "MemFree:"
#define MEM_STATS_TAG_BUFFERS    "Buffers:"
#define MEM_STATS_TAG_CACHED     "Cached:"
#define MEM_STATS_TAG_SHMEM      "Shmem:"

/* One read of /proc/meminfo, which is the kernel formatting a page and
 * a half on demand rather than anything touching a disk - but stdio
 * would still put a heap-allocated buffer and a call per line in front
 * of it, so take it in one go into stack space and pick through that.
 * Everything wanted is in the first few hundred bytes, so the scan
 * stops as soon as it has the lot.
 *
 * MemAvailable is the kernel's own estimate of what can be had without
 * swapping, and is preferred wherever the kernel is new enough to
 * publish it; the older sum is the fallback. */
/* One read of /proc/meminfo, which is the kernel formatting a page and
 * a half on demand rather than anything touching a disk - but stdio
 * would put a heap-allocated buffer and a call per line in front of it,
 * so take it in one go into stack space.
 *
 * Then one pass, not one per field: every line is looked at once, and
 * the first character rejects almost all of them before any string
 * comparison happens.  The six values wanted sit in the first few
 * hundred bytes, so the walk stops as soon as it has them and never
 * reaches the rest of the file.
 *
 * MemAvailable is the kernel's own estimate of what can be had without
 * swapping, and is preferred wherever the kernel publishes it; the
 * older sum is the fallback.
 */
#define MEM_STATS_MATCH(tag, dst) \
   if (!strncmp(line, tag, STRLEN_CONST(tag))) \
   { \
      dst = strtoul(line + STRLEN_CONST(tag), NULL, 10); \
      need--; \
      break; \
   }

static void mem_stats_proc_meminfo(uint64_t *total, uint64_t *avail)
{
   char    buf[2048];
   char   *line;
   ssize_t got;
   int     need = 6;
   unsigned long mem_total = 0, mem_available = 0, mem_free = 0;
   unsigned long buffers = 0, cached = 0, shmem = 0;
   int     have_avail = 0;
   int     fd = open(MEM_STATS_PROC_MEMINFO, O_RDONLY);

   if (fd < 0)
      return;
   got = read(fd, buf, sizeof(buf) - 1);
   close(fd);
   if (got <= 0)
      return;
   buf[got] = '\0';

   line = buf;
   while (line && need)
   {
      /* one character throws out every line that cannot be a match,
       * which is most of them */
      switch (*line)
      {
         case 'M':
            MEM_STATS_MATCH("MemTotal:",     mem_total)
            MEM_STATS_MATCH("MemFree:",      mem_free)
            /* MemAvailable is the one whose absence has to be known
             * about, the fallback sum existing for exactly that */
            if (!strncmp(line, "MemAvailable:",
                     STRLEN_CONST("MemAvailable:")))
            {
               mem_available = strtoul(line
                     + STRLEN_CONST("MemAvailable:"), NULL, 10);
               have_avail    = 1;
               need--;
            }
            break;
         case 'B':
            MEM_STATS_MATCH("Buffers:",      buffers)
            break;
         case 'C':
            /* anchored at the line start, so "SwapCached:" - which
             * contains this - cannot be picked up by mistake */
            MEM_STATS_MATCH("Cached:",       cached)
            break;
         case 'S':
            MEM_STATS_MATCH("Shmem:",        shmem)
            break;
         default:
            break;
      }
      if ((line = strchr(line, '\n')))
         line++;
   }

   if (total)
      *total = (uint64_t)mem_total * 1024;
   if (!avail)
      return;
   if (have_avail)
   {
      *avail = (uint64_t)mem_available * 1024;
      return;
   }
   /* Subtracting shmem from the sum on unsigned longs wraps to an
    * enormous figure if it ever exceeds it, and an enormous figure here
    * means every admission test in the program says yes.  It should not
    * happen; it costs nothing to make sure it cannot. */
   {
      unsigned long sum = mem_free + buffers + cached;
      *avail = (uint64_t)((sum > shmem) ? (sum - shmem) : 0) * 1024;
   }
}
#endif

uint64_t mem_stats_total(void)
{
#if defined(MEM_STATS_CTR)
   return osGetMemRegionSize(MEMREGION_ALL);
#elif defined(MEM_STATS_GX)
#if defined(HW_RVL) && !defined(IS_SALAMANDER)
   return SYSMEM1_SIZE + gx_mem2_total();
#else
   return SYSMEM1_SIZE;
#endif
#elif defined(MEM_STATS_VITA)
   return (uint64_t)(_newlib_heap_end - _newlib_heap_base);
#elif defined(MEM_STATS_LIBNX)
   {
      uint64_t total = 0;
      if (R_SUCCEEDED(svcGetInfo(&total, InfoType_TotalMemorySize,
                  CUR_PROCESS_HANDLE, 0)))
         return total;
      return 0;
   }
#elif defined(MEM_STATS_SWITCH)
   {
      struct mallinfo mem_info = mallinfo();
      return mem_info.usmblks;
   }
#elif defined(MEM_STATS_WIN32)
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
#elif defined(MEM_STATS_ORBIS)
   {
      size_t max_mem = 0, cur_mem = 0;
      get_user_mem_size(&max_mem, &cur_mem);
      return (uint64_t)max_mem;
   }
#elif defined(MEM_STATS_PS3)
   {
      sys_memory_info_t mem_info;
      sys_memory_get_user_memory_size((uint64_t)(uintptr_t)&mem_info);
      return (uint64_t)mem_info.total;
   }
#elif defined(MEM_STATS_EMSCRIPTEN)
   /* The ceiling the module may grow its heap to, which is the only
    * "total" that means anything in a wasm process. */
   return (uint64_t)emscripten_get_heap_max();
#elif defined(MEM_STATS_PS2)
   return 32 * 1024 * 1024;
#elif defined(MEM_STATS_APPLE)
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
#elif defined(MEM_STATS_PROC)
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
#if defined(MEM_STATS_CTR)
   return osGetMemRegionFree(MEMREGION_ALL);
#elif defined(MEM_STATS_GX)
   {
      /* SYS_GetArena1Size() reports remaining MEM1 directly. */
      uint64_t total = SYS_GetArena1Size();
#if defined(HW_RVL) && !defined(IS_SALAMANDER)
      total += (gx_mem2_total() - gx_mem2_used());
#endif
      return total;
   }
#elif defined(MEM_STATS_VITA)
   return (uint64_t)(_newlib_heap_end - _newlib_heap_cur);
#elif defined(MEM_STATS_LIBNX)
   {
      uint64_t total = 0, used = 0;
      if (     R_SUCCEEDED(svcGetInfo(&total, InfoType_TotalMemorySize,
                  CUR_PROCESS_HANDLE, 0))
            && R_SUCCEEDED(svcGetInfo(&used, InfoType_UsedMemorySize,
                  CUR_PROCESS_HANDLE, 0)))
         return total - used;
      return 0;
   }
#elif defined(MEM_STATS_SWITCH)
   {
      struct mallinfo mem_info = mallinfo();
      return mem_info.fordblks;
   }
#elif defined(MEM_STATS_WIN32)
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
#elif defined(MEM_STATS_ORBIS)
   /* get_user_mem_size reports the ceiling and what is taken, so free
    * is the difference.  The frontend wired the taken figure straight
    * into the free slot, which had this platform reporting the opposite
    * of what every caller asked for. */
   {
      size_t max_mem = 0, cur_mem = 0;
      get_user_mem_size(&max_mem, &cur_mem);
      return (max_mem > cur_mem) ? (uint64_t)(max_mem - cur_mem) : 0;
   }
#elif defined(MEM_STATS_PS3)
   /* named get_mem_used in the frontend, but what it read was avail */
   {
      sys_memory_info_t mem_info;
      sys_memory_get_user_memory_size((uint64_t)(uintptr_t)&mem_info);
      return (uint64_t)mem_info.avail;
   }
#elif defined(MEM_STATS_EMSCRIPTEN)
   /* What is left before that ceiling: the allocator's tally of what it
    * holds, taken off the maximum the heap may reach.  Growing into the
    * space between the current heap and that maximum is the runtime's
    * business, so counting from the maximum is what a caller asking
    * whether it can afford something wants to know. */
   {
      uint64_t limit = (uint64_t)emscripten_get_heap_max();
      uint64_t used  = (uint64_t)mallinfo().uordblks;
      return (limit > used) ? (limit - used) : 0;
   }
#elif defined(MEM_STATS_PS2)
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
#elif defined(MEM_STATS_APPLE)
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
#elif defined(MEM_STATS_PROC)
   {
      uint64_t avail = 0;
      mem_stats_proc_meminfo(NULL, &avail);
      return avail;
   }
#else
   return 0;
#endif
}
