/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vulkan_memory_type_test.c).
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

/* vulkan_cpu_write_memory_types() against the memory layouts devices
 * report: a discrete card with the legacy 256 MB BAR window, the same
 * card with resizable BAR, an APU, and a unified-memory device. Without
 * resizable BAR the answer must be the caller's own choice alone - the
 * first host-visible coherent type, which the Vulkan ordering rule puts
 * ahead of the BAR type - so those devices allocate exactly as before. */

#include <stdio.h>
#include <string.h>

#include "../../../gfx/common/vulkan_memory_type.h"

#define MB ((VkDeviceSize)1024 * 1024)
#define HV (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
#define DL VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT

static int fails;

static void add_heap(VkPhysicalDeviceMemoryProperties *p, VkDeviceSize size, int device_local)
{
   p->memoryHeaps[p->memoryHeapCount].size  = size;
   p->memoryHeaps[p->memoryHeapCount].flags = device_local ? VK_MEMORY_HEAP_DEVICE_LOCAL_BIT : 0;
   p->memoryHeapCount++;
}

static void add_type(VkPhysicalDeviceMemoryProperties *p, VkMemoryPropertyFlags flags, uint32_t heap)
{
   p->memoryTypes[p->memoryTypeCount].propertyFlags = flags;
   p->memoryTypes[p->memoryTypeCount].heapIndex     = heap;
   p->memoryTypeCount++;
}

/* What the callers pass as their own choice: the first type with the
 * host-visible coherent flags (vulkan_find_memory_type). */
static uint32_t first_host_visible(const VkPhysicalDeviceMemoryProperties *p, uint32_t bits)
{
   uint32_t i;
   for (i = 0; i < p->memoryTypeCount; i++)
      if ((bits & (1u << i)) && (p->memoryTypes[i].propertyFlags & HV) == HV)
         return i;
   return 0;
}

static void expect(const char *what, const VkPhysicalDeviceMemoryProperties *p,
      uint32_t bits, unsigned n_want, uint32_t first_want, uint32_t second_want)
{
   uint32_t types[2];
   const uint32_t fallback = first_host_visible(p, bits);
   const unsigned n = vulkan_cpu_write_memory_types(p, bits, fallback, types);
   int ok = (n == n_want) && types[0] == first_want
         && (n < 2 || types[1] == second_want);
   if (n_want == 1 && first_want != fallback)
      ok = 0;
   printf("%s %s: %u type(s), %u%s\n", ok ? "ok  " : "FAIL", what, n,
         (unsigned)types[0], n > 1 ? " then the caller's" : "");
   if (!ok)
      fails++;
}

int main(void)
{
   VkPhysicalDeviceMemoryProperties p;

   /* Discrete, no resizable BAR: VRAM, system memory, and a 256 MB BAR
    * window that is device local and host visible. */
   memset(&p, 0, sizeof(p));
   add_heap(&p, 8192 * MB, 1);
   add_heap(&p, 16384 * MB, 0);
   add_heap(&p, 256 * MB, 1);
   add_type(&p, DL, 0);
   add_type(&p, HV, 1);
   add_type(&p, HV | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 1);
   add_type(&p, DL | HV, 2);
   expect("256 MB BAR window: the caller's type alone", &p, ~0u, 1, 1, 0);

   /* The same with the window the driver reports on some NVIDIA cards. */
   p.memoryHeaps[2].size = 214 * MB;
   expect("214 MB BAR window: the caller's type alone", &p, ~0u, 1, 1, 0);

   /* Resizable BAR: the device-local host-visible type sits in the VRAM
    * heap, all 8 GB of it. */
   memset(&p, 0, sizeof(p));
   add_heap(&p, 8192 * MB, 1);
   add_heap(&p, 16384 * MB, 0);
   add_type(&p, DL, 0);
   add_type(&p, HV, 1);
   add_type(&p, HV | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 1);
   add_type(&p, DL | HV, 0);
   expect("resizable BAR: VRAM first, then the caller's type", &p, ~0u, 2, 3, 1);

   /* A buffer whose requirements leave that type out keeps its own. */
   expect("resizable BAR, type not allowed: the caller's type alone", &p, 0x7u, 1, 1, 0);

   /* An APU: a visible VRAM carve-out above 256 MB next to GTT. */
   memset(&p, 0, sizeof(p));
   add_heap(&p, 2048 * MB, 1);
   add_heap(&p, 14336 * MB, 0);
   add_type(&p, DL, 0);
   add_type(&p, HV, 1);
   add_type(&p, DL | HV, 0);
   expect("APU carve-out: the carve-out first, then GTT", &p, ~0u, 2, 2, 1);

   /* Unified memory: one heap, and the caller's type is already the
    * device-local host-visible one. */
   memset(&p, 0, sizeof(p));
   add_heap(&p, 8192 * MB, 1);
   add_type(&p, DL, 0);
   add_type(&p, DL | HV, 0);
   expect("unified memory: the caller's type, once", &p, ~0u, 1, 1, 0);

   if (fails)
   {
      printf("%d case(s) failed\n", fails);
      return 1;
   }
   printf("all cases passed\n");
   return 0;
}
