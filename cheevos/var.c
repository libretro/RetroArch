/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2017 - Andre Leiradella
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

#include <ctype.h>
#include <stdint.h>

#include <libretro.h>

#include "var.h"

#include "../retroarch.h"
#include "../core.h"
#include "../verbosity.h"

static void STUB_LOG(const char *fmt, ...)
{
   (void)fmt;
}

#ifdef CHEEVOS_VERBOSE
#define CHEEVOS_LOG RARCH_LOG
#else
#define CHEEVOS_LOG STUB_LOG
#endif

/*****************************************************************************
Parsing
*****************************************************************************/

static cheevos_var_size_t cheevos_var_parse_prefix(const char** memaddr)
{
   /* Careful not to use ABCDEF here, this denotes part of an actual variable! */
   const char* str = *memaddr;
   cheevos_var_size_t size;

   switch (toupper((unsigned char)*str++))
   {
      case 'M':
         size = CHEEVOS_VAR_SIZE_BIT_0;
         break;
      case 'N':
         size = CHEEVOS_VAR_SIZE_BIT_1;
         break;
      case 'O':
         size = CHEEVOS_VAR_SIZE_BIT_2;
         break;
      case 'P':
         size = CHEEVOS_VAR_SIZE_BIT_3;
         break;
      case 'Q':
         size = CHEEVOS_VAR_SIZE_BIT_4;
         break;
      case 'R':
         size = CHEEVOS_VAR_SIZE_BIT_5;
         break;
      case 'S':
         size = CHEEVOS_VAR_SIZE_BIT_6;
         break;
      case 'T':
         size = CHEEVOS_VAR_SIZE_BIT_7;
         break;
      case 'L':
         size = CHEEVOS_VAR_SIZE_NIBBLE_LOWER;
         break;
      case 'U':
         size = CHEEVOS_VAR_SIZE_NIBBLE_UPPER;
         break;
      case 'H':
         size = CHEEVOS_VAR_SIZE_EIGHT_BITS;
         break;
      case 'X':
         size = CHEEVOS_VAR_SIZE_THIRTYTWO_BITS;
         break;
      default:
         str--;
         /* fall through */
      case ' ':
         size = CHEEVOS_VAR_SIZE_SIXTEEN_BITS;
         break;
   }

   *memaddr = str;
   return size;
}

static size_t cheevos_var_reduce(size_t addr, size_t mask)
{
   while (mask)
   {
      size_t tmp = (mask - 1) & ~mask;
      addr = (addr & tmp) | ((addr >> 1) & ~tmp);
      mask = (mask & (mask - 1)) >> 1;
   }

   return addr;
}

static size_t cheevos_var_highest_bit(size_t n)
{
   n |= n >>  1;
   n |= n >>  2;
   n |= n >>  4;
   n |= n >>  8;
   n |= n >> 16;

   return n ^ (n >> 1);
}

void cheevos_var_parse(cheevos_var_t* var, const char** memaddr)
{
   char *end       = NULL;
   const char *str = *memaddr;
   unsigned base   = 16;

   var->is_bcd = false;

   if (toupper((unsigned char)*str) == 'D' && str[1] == '0' && toupper((unsigned char)str[2]) == 'X')
   {
      /* d0x + 4 hex digits */
      str += 3;
      var->type = CHEEVOS_VAR_TYPE_DELTA_MEM;
   }
   else if (toupper((unsigned char)*str) == 'B' && str[1] == '0' && toupper((unsigned char)str[2]) == 'X')
   {
      /* b0x (binary-coded decimal) */
      str += 3;
      var->is_bcd = true;
      var->type = CHEEVOS_VAR_TYPE_ADDRESS;
   }
   else if (*str == '0' && toupper((unsigned char)str[1]) == 'X')
   {
      /* 0x + 4 hex digits */
      str += 2;
      var->type = CHEEVOS_VAR_TYPE_ADDRESS;
   }
   else
   {
      var->type = CHEEVOS_VAR_TYPE_VALUE_COMP;

      if (toupper((unsigned char)*str) == 'H')
         str++;
      else
      {
         if (toupper((unsigned char)*str) == 'V')
            str++;

         base = 10;
      }
   }

   if (var->type != CHEEVOS_VAR_TYPE_VALUE_COMP)
   {
      var->size = cheevos_var_parse_prefix(&str);
   }

   var->value = (unsigned)strtol(str, &end, base);
   *memaddr   = end;
}

void cheevos_var_patch_addr(cheevos_var_t* var, cheevos_console_t console)
{
   rarch_system_info_t *system = runloop_get_system_info();

   var->bank_id = -1;

   if (console == CHEEVOS_CONSOLE_NINTENDO)
   {
      if (var->value >= 0x0800 && var->value < 0x2000)
      {
         CHEEVOS_LOG(CHEEVOS_TAG "NES memory address in mirrorred RAM %X, adjusted to %X\n", var->value, var->value & 0x07ff);
         var->value &= 0x07ff;
      }
   }
   else if (console == CHEEVOS_CONSOLE_GAMEBOY_COLOR)
   {
      if (var->value >= 0xe000 && var->value <= 0xfdff)
      {
         CHEEVOS_LOG(CHEEVOS_TAG "GBC memory address in echo RAM %X, adjusted to %X\n", var->value, var->value - 0x2000);
         var->value -= 0x2000;
      }
   }
   else if (console == CHEEVOS_CONSOLE_NEOGEO_POCKET)
   {
      if (var->value >= 0x4000 && var->value <= 0x7fff)
      CHEEVOS_LOG(CHEEVOS_TAG "NGP memory address %X adjusted to %X\n", var->value, var->value - 0x004000);
      var->value -= 0x4000;
   }

   if (system->mmaps.num_descriptors != 0)
   {
      const rarch_memory_descriptor_t *desc = NULL;
      const rarch_memory_descriptor_t *end  = NULL;

      /* Patch the address to correctly map it to the mmaps */
      if (console == CHEEVOS_CONSOLE_GAMEBOY_ADVANCE)
      {
         if (var->value < 0x8000) /* Internal RAM */
         {
            CHEEVOS_LOG(CHEEVOS_TAG "GBA memory address %X adjusted to %X\n", var->value, var->value + 0x3000000);
            var->value += 0x3000000;
         }
         else /* Work RAM */
         {
            CHEEVOS_LOG(CHEEVOS_TAG "GBA memory address %X adjusted to %X\n", var->value, var->value + 0x2000000 - 0x8000);
            var->value += 0x2000000 - 0x8000;
         }
      }
      else if (console == CHEEVOS_CONSOLE_PC_ENGINE)
      {
         CHEEVOS_LOG(CHEEVOS_TAG "PCE memory address %X adjusted to %X\n", var->value, var->value + 0x1f0000);
         var->value += 0x1f0000;
      }
      else if (console == CHEEVOS_CONSOLE_SUPER_NINTENDO)
      {
         if (var->value < 0x020000) /* Work RAM */
         {
            CHEEVOS_LOG(CHEEVOS_TAG "SNES memory address %X adjusted to %X\n", var->value, var->value + 0x7e0000);
            var->value += 0x7e0000;
         }
         else /* Save RAM */
         {
            CHEEVOS_LOG(CHEEVOS_TAG "SNES memory address %X adjusted to %X\n", var->value, var->value + 0x006000 - 0x020000);
            var->value += 0x006000 - 0x020000;
         }
      }

      desc = system->mmaps.descriptors;
      end  = desc + system->mmaps.num_descriptors;

      for (; desc < end; desc++)
      {
         if (((desc->core.start ^ var->value) & desc->core.select) == 0)
         {
            unsigned addr = var->value;
            var->bank_id  = (int)(desc - system->mmaps.descriptors);
            var->value    = (unsigned)cheevos_var_reduce(
               (addr - desc->core.start) & desc->disconnect_mask,
               desc->core.disconnect);

            if (var->value >= desc->core.len)
               var->value -= cheevos_var_highest_bit(var->value);

            var->value += desc->core.offset;

            CHEEVOS_LOG(CHEEVOS_TAG "address %X set to descriptor %d at offset %X\n", addr, var->bank_id + 1, var->value);
            break;
         }
      }
   }
   else
   {
      unsigned i;

      for (i = 0; i < 4; i++)
      {
         retro_ctx_memory_info_t meminfo;

         switch (i)
         {
            case 0:
               meminfo.id = RETRO_MEMORY_SYSTEM_RAM;
               break;
            case 1:
               meminfo.id = RETRO_MEMORY_SAVE_RAM;
               break;
            case 2:
               meminfo.id = RETRO_MEMORY_VIDEO_RAM;
               break;
            case 3:
               meminfo.id = RETRO_MEMORY_RTC;
               break;
         }

         core_get_memory(&meminfo);

         if (var->value < meminfo.size)
         {
            var->bank_id = i;
            break;
         }

         /* HACK subtract the correct amount of bytes to reach the save RAM */
         if (i == 0 && console == CHEEVOS_CONSOLE_NINTENDO)
            var->value -= 0x6000;
         else
            var->value -= meminfo.size;
      }
   }
}

/*****************************************************************************
Testing
*****************************************************************************/

uint8_t* cheevos_var_get_memory(const cheevos_var_t* var)
{
   uint8_t* memory = NULL;

   if (var->bank_id >= 0)
   {
      rarch_system_info_t* system = runloop_get_system_info();

      if (system->mmaps.num_descriptors != 0)
         memory = (uint8_t*)system->mmaps.descriptors[var->bank_id].core.ptr;
      else
      {
         retro_ctx_memory_info_t meminfo = {NULL, 0, 0};

         switch (var->bank_id)
         {
            case 0:
               meminfo.id = RETRO_MEMORY_SYSTEM_RAM;
               break;
            case 1:
               meminfo.id = RETRO_MEMORY_SAVE_RAM;
               break;
            case 2:
               meminfo.id = RETRO_MEMORY_VIDEO_RAM;
               break;
            case 3:
               meminfo.id = RETRO_MEMORY_RTC;
               break;
            default:
               RARCH_ERR(CHEEVOS_TAG "invalid bank id: %s\n", var->bank_id);
               break;
         }

         core_get_memory(&meminfo);
         memory = (uint8_t*)meminfo.data;
      }

      if (memory)
         memory += var->value;
   }

   return memory;
}

unsigned cheevos_var_get_value(cheevos_var_t* var)
{
   const uint8_t* memory = NULL;
   unsigned value        = 0;

   switch (var->type)
   {
      case CHEEVOS_VAR_TYPE_VALUE_COMP:
         value = var->value;
         break;

      case CHEEVOS_VAR_TYPE_ADDRESS:
      case CHEEVOS_VAR_TYPE_DELTA_MEM:
         memory = cheevos_var_get_memory(var);

         if (memory)
         {
            value = memory[0];

            switch (var->size)
            {
               case CHEEVOS_VAR_SIZE_BIT_0:
                  value &= 1;
                  break;
               case CHEEVOS_VAR_SIZE_BIT_1:
                  value = (value >> 1) & 1;
                  break;
               case CHEEVOS_VAR_SIZE_BIT_2:
                  value = (value >> 2) & 1;
                  break;
               case CHEEVOS_VAR_SIZE_BIT_3:
                  value = (value >> 3) & 1;
                  break;
               case CHEEVOS_VAR_SIZE_BIT_4:
                  value = (value >> 4) & 1;
                  break;
               case CHEEVOS_VAR_SIZE_BIT_5:
                  value = (value >> 5) & 1;
                  break;
               case CHEEVOS_VAR_SIZE_BIT_6:
                  value = (value >> 6) & 1;
                  break;
               case CHEEVOS_VAR_SIZE_BIT_7:
                  value = (value >> 7) & 1;
                  break;
               case CHEEVOS_VAR_SIZE_NIBBLE_LOWER:
                  value &= 0x0f;
                  break;
               case CHEEVOS_VAR_SIZE_NIBBLE_UPPER:
                  value = (value >> 4) & 0x0f;
                  break;
               case CHEEVOS_VAR_SIZE_EIGHT_BITS:
                  break;
               case CHEEVOS_VAR_SIZE_SIXTEEN_BITS:
                  value |= memory[1] << 8;
                  break;
               case CHEEVOS_VAR_SIZE_THIRTYTWO_BITS:
                  value |= memory[1] << 8;
                  value |= memory[2] << 16;
                  value |= memory[3] << 24;
                  break;
            }
         }

         if (var->type == CHEEVOS_VAR_TYPE_DELTA_MEM)
         {
            unsigned previous = var->previous;
            var->previous     = value;
            value = previous;
         }

         break;

      case CHEEVOS_VAR_TYPE_DYNAMIC_VAR:
         /* We shouldn't get here... */
         break;
   }

   if(var->is_bcd)
      return (((value >> 4) & 0xf) * 10) + (value & 0xf);
   else
      return value;
}
