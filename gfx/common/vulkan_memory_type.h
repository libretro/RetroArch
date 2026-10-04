/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2026 - The RetroArch team
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

#ifndef VULKAN_MEMORY_TYPE_H__
#define VULKAN_MEMORY_TYPE_H__

/* Which memory types a buffer the CPU only writes and the GPU reads goes
 * to, and in what order. No Vulkan calls: the answer is a function of
 * the device's memory properties alone, so it can be checked without a
 * device (samples/gfx/vulkan_memory_type). */

#include <stdint.h>

#include <retro_inline.h>

#include "../include/vulkan/vulkan_core.h"

/* A device-local host-visible heap at or below this size is the legacy
 * PCI BAR window: 256 MB or less, shared with the driver, and refused
 * long before the device runs out. Above it the device maps all of its
 * memory (resizable BAR), or is a unified-memory device. */
#define VULKAN_BAR_WINDOW_MAX ((VkDeviceSize)256 * 1024 * 1024)

#define VULKAN_CPU_WRITE_FLAGS ( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT \
                               | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT \
                               | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)

/* Fills types[] with the memory types to try, best first, and returns
 * how many: the device-local host-visible coherent type when its heap is
 * larger than a BAR window, then fallback_type (the caller's usual
 * host-visible choice) - or fallback_type alone when that is the same
 * type or there is no such heap. Without resizable BAR the answer is
 * fallback_type alone, which is what the caller would have used. */
static INLINE unsigned vulkan_cpu_write_memory_types(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t type_bits, uint32_t fallback_type, uint32_t types[2])
{
   uint32_t i;
   unsigned n = 0;

   for (i = 0; i < mem_props->memoryTypeCount && i < VK_MAX_MEMORY_TYPES; i++)
   {
      const VkMemoryType *t = &mem_props->memoryTypes[i];
      if (!(type_bits & (1u << i)))
         continue;
      if ((t->propertyFlags & VULKAN_CPU_WRITE_FLAGS) != VULKAN_CPU_WRITE_FLAGS)
         continue;
      if (t->heapIndex >= mem_props->memoryHeapCount)
         continue;
      if (mem_props->memoryHeaps[t->heapIndex].size <= VULKAN_BAR_WINDOW_MAX)
         continue;
      if (i != fallback_type)
         types[n++] = i;
      break;
   }

   types[n++] = fallback_type;
   return n;
}

#endif
