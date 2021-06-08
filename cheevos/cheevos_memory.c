/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2016 - Andre Leiradella
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

#include "cheevos_memory.h"

#include "cheevos_locals.h"

#include "../retroarch.h"
#include "../verbosity.h"

#include "../deps/rcheevos/include/rcheevos.h"

#include <stdio.h>

uint8_t* rcheevos_memory_find(
      const rcheevos_memory_regions_t* regions, unsigned address)
{
   unsigned i;

   for (i = 0; i < regions->count; ++i)
   {
      const size_t size = regions->size[i];
      if (address < size)
      {
         if (regions->data[i] == NULL)
            break;

         return &regions->data[i][address];
      }

      address -= size;
   }

   return NULL;
}

static const char* rcheevos_memory_type(int type)
{
   switch (type)
   {
      case RC_MEMORY_TYPE_SAVE_RAM:
         return "SRAM";
      case RC_MEMORY_TYPE_VIDEO_RAM:
         return "VRAM";
      case RC_MEMORY_TYPE_UNUSED:
         return "UNUSED";
      default:
         break;
   }

   return "SYSTEM RAM";
}

static void rcheevos_memory_register_region(rcheevos_memory_regions_t* regions,
   int type, uint8_t* data, size_t size, const char* description)
{
   if (size == 0)
      return;

   if (regions->count == MAX_MEMORY_REGIONS)
   {
      CHEEVOS_LOG(RCHEEVOS_TAG "Too many memory memory regions to register\n");
      return;
   }

   if (!data && regions->count > 0 && !regions->data[regions->count - 1])
   {
      /* extend null region */
      regions->size[regions->count - 1] += size;
   }
   else if (data && regions->count > 0 &&
      data == (regions->data[regions->count - 1] + regions->size[regions->count - 1]))
   {
      /* extend non-null region */
      regions->size[regions->count - 1] += size;
   }
   else
   {
      /* create new region */
      regions->data[regions->count] = data;
      regions->size[regions->count] = size;
      ++regions->count;
   }

   regions->total_size += size;

   CHEEVOS_LOG(RCHEEVOS_TAG "Registered 0x%04X bytes of %s at $%06X (%s)\n", (unsigned)size,
      rcheevos_memory_type(type), (unsigned)(regions->total_size - size), description);
}

static void rcheevos_memory_init_without_regions(
      rcheevos_memory_regions_t* regions)
{
   /* no regions specified, assume system RAM followed by save RAM */
   char description[64];
   retro_ctx_memory_info_t meminfo;

   snprintf(description, sizeof(description), "offset 0x%06x", 0);

   meminfo.id = RETRO_MEMORY_SYSTEM_RAM;
   core_get_memory(&meminfo);
   rcheevos_memory_register_region(regions, RC_MEMORY_TYPE_SYSTEM_RAM, (uint8_t*)meminfo.data, meminfo.size, description);

   meminfo.id = RETRO_MEMORY_SAVE_RAM;
   core_get_memory(&meminfo);
   rcheevos_memory_register_region(regions, RC_MEMORY_TYPE_SAVE_RAM, (uint8_t*)meminfo.data, meminfo.size, description);
}

static const rarch_memory_descriptor_t* rcheevos_memory_get_descriptor(const rarch_memory_map_t* mmap, unsigned real_address)
{
   const rarch_memory_descriptor_t* desc = mmap->descriptors;
   const rarch_memory_descriptor_t* end = desc + mmap->num_descriptors;

   if (mmap->num_descriptors == 0)
      return NULL;

   for (; desc < end; desc++)
   {
      if (desc->core.select == 0)
      {
         /* if select is 0, attempt to explcitly match the address */
         if (real_address >= desc->core.start && real_address < desc->core.start + desc->core.len)
            return desc;
      }
      else
      {
         /* otherwise, attempt to match the address by matching the select bits */
         if (((desc->core.start ^ real_address) & desc->core.select) == 0)
         {
            /* sanity check - make sure the descriptor is large enough to hold the target address */
            if (real_address - desc->core.start < desc->core.len)
               return desc;
         }
      }
   }

   return NULL;
}

void rcheevos_memory_init_from_memory_map(rcheevos_memory_regions_t* regions, const rarch_memory_map_t* mmap, const rc_memory_regions_t* console_regions)
{
   char description[64];
   unsigned i;
   uint8_t* region_start;
   uint8_t* desc_start;
   size_t desc_size;
   size_t offset;

   for (i = 0; i < console_regions->num_regions; ++i)
   {
      const rc_memory_region_t* console_region = &console_regions->region[i];
      size_t console_region_size = console_region->end_address - console_region->start_address + 1;
      unsigned real_address = console_region->real_address;

      while (console_region_size > 0)
      {
         const rarch_memory_descriptor_t* desc = rcheevos_memory_get_descriptor(mmap, real_address);
         if (!desc)
         {
            if (console_region->type != RC_MEMORY_TYPE_UNUSED)
            {
               CHEEVOS_LOG(RCHEEVOS_TAG "Could not map region starting at $%06X\n",
                  real_address - console_region->real_address + console_region->start_address);
            }

            rcheevos_memory_register_region(regions, console_region->type, NULL, console_region_size, "null filler");
            break;
         }

         offset = real_address - desc->core.start;
         snprintf(description, sizeof(description),
               "descriptor %u, offset 0x%06X",
               (int)(desc - mmap->descriptors) + 1, (int)offset);

         if (desc->core.ptr)
         {
            desc_start   = (uint8_t*)desc->core.ptr + desc->core.offset;
            region_start = desc_start + offset;
         }
         else
            region_start = NULL;

         desc_size       = desc->core.len - offset;

         if (console_region_size > desc_size)
         {
            if (desc_size == 0)
            {
               if (console_region->type != RC_MEMORY_TYPE_UNUSED)
               {
                  CHEEVOS_LOG(RCHEEVOS_TAG "Could not map region starting at $%06X\n",
                     real_address - console_region->real_address + console_region->start_address);
               }

               rcheevos_memory_register_region(regions, console_region->type, NULL, console_region_size, "null filler");
               console_region_size = 0;
            }
            else
            {
               rcheevos_memory_register_region(regions, console_region->type, region_start, desc_size, description);
               console_region_size -= desc_size;
               real_address += desc_size;
            }
         }
         else
         {
            rcheevos_memory_register_region(regions, console_region->type, region_start, console_region_size, description);
            console_region_size = 0;
         }
      }
   }
}

static unsigned rcheevos_memory_console_region_to_ram_type(int region_type)
{
   switch (region_type)
   {
      case RC_MEMORY_TYPE_SAVE_RAM:
         return RETRO_MEMORY_SAVE_RAM;
      case RC_MEMORY_TYPE_VIDEO_RAM:
         return RETRO_MEMORY_VIDEO_RAM;
      default:
         break;
   }

   return RETRO_MEMORY_SYSTEM_RAM;
}

static void rcheevos_memory_init_from_unmapped_memory(rcheevos_memory_regions_t* regions, const rc_memory_regions_t* console_regions, int console)
{
   char description[64];
   unsigned i;

   for (i = 0; i < console_regions->num_regions; ++i)
   {
      size_t offset;
      unsigned j;
      retro_ctx_memory_info_t meminfo;
      const rc_memory_region_t* console_region = &console_regions->region[i];
      const size_t console_region_size         = 
         console_region->end_address - console_region->start_address + 1;
      unsigned base_address                    = 0;

      meminfo.id                               = rcheevos_memory_console_region_to_ram_type(console_region->type);

      for (j = 0; j <= i; ++j)
      {
         const rc_memory_region_t* console_region2 = &console_regions->region[j];
         if (rcheevos_memory_console_region_to_ram_type(
                  console_region2->type) == meminfo.id)
         {
            base_address = console_region2->start_address;
            break;
         }
      }
      offset = console_region->start_address - base_address;

      core_get_memory(&meminfo);

      if (offset < meminfo.size)
      {
         meminfo.size -= offset;

         if (meminfo.data)
         {
            snprintf(description, sizeof(description),
                  "offset 0x%06X", (int)offset);
            meminfo.data = (uint8_t*)meminfo.data + offset;
         }
         else
            snprintf(description, sizeof(description), "null filler");
      }
      else
      {
         if (console_region->type != RC_MEMORY_TYPE_UNUSED)
         {
            CHEEVOS_LOG(RCHEEVOS_TAG "Could not map region starting at $%06X\n", console_region->start_address);
         }

         meminfo.data = NULL;
         meminfo.size = 0;
      }

      if (console_region_size > meminfo.size)
      {
         /* want more than what is available, take what we can and null fill the rest */
         rcheevos_memory_register_region(regions, console_region->type, (uint8_t*)meminfo.data, meminfo.size, description);
         rcheevos_memory_register_region(regions, console_region->type, NULL, console_region_size - meminfo.size, "null filler");
      }
      else
      {
         /* only take as much as we need */
         rcheevos_memory_register_region(regions, console_region->type, (uint8_t*)meminfo.data, console_region_size, description);
      }
   }
}

void rcheevos_memory_destroy(rcheevos_memory_regions_t* regions)
{
   memset(regions, 0, sizeof(*regions));
}

bool rcheevos_memory_init(rcheevos_memory_regions_t* regions, int console)
{
   unsigned i;
   const rc_memory_regions_t* console_regions = rc_console_memory_regions(console);
   rcheevos_memory_regions_t new_regions;
   bool has_valid_region = false;

   if (!regions)
      return false;

   memset(&new_regions, 0, sizeof(new_regions));

   if (console_regions == NULL || console_regions->num_regions == 0)
   {
      rcheevos_memory_init_without_regions(&new_regions);
   }
   else
   {
      rarch_system_info_t* system = runloop_get_system_info();
      if (system->mmaps.num_descriptors != 0)
         rcheevos_memory_init_from_memory_map(&new_regions, &system->mmaps, console_regions);
      else
         rcheevos_memory_init_from_unmapped_memory(&new_regions, console_regions, console);
   }

   /* determine if any valid regions were found */
   for (i = 0; i < new_regions.count; i++)
   {
      if (new_regions.data[i])
      {
         has_valid_region = true;
         break;
      }
   }

   memcpy(regions, &new_regions, sizeof(*regions));
   return has_valid_region;
}
