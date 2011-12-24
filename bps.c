/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bps.h"
#include "movie.h"
#include <stdint.h>
#include "boolean.h"

enum bps_mode
{
   SOURCE_READ = 0,
   TARGET_READ,
   SOURCE_COPY,
   TARGET_COPY
};

struct bps_data
{
   const uint8_t *modify_data, *source_data; 
   uint8_t *target_data;
   size_t modify_length, source_length, target_length;
   size_t modify_offset, source_offset, target_offset;
   uint32_t modify_checksum, source_checksum, target_checksum;

   size_t source_relative_offset, target_relative_offset, output_offset;
};

static uint8_t bps_read(struct bps_data *bps)
{
   uint8_t data = bps->modify_data[bps->modify_offset++];
   bps->modify_checksum = crc32_adjust(bps->modify_checksum, data);
   return data;
}

static uint64_t bps_decode(struct bps_data *bps)
{
   uint64_t data = 0, shift = 1;

   for (;;)
   {
      uint8_t x = bps_read(bps);
      data += (x & 0x7f) * shift;
      if (x & 0x80)
         break;
      shift <<= 7;
      data += shift;
   }

   return data;
}

static void bps_write(struct bps_data *bps, uint8_t data)
{
   bps->target_data[bps->output_offset++] = data;
   bps->target_checksum = crc32_adjust(bps->target_checksum, data);
}

bps_error_t bps_apply_patch(
      const uint8_t *modify_data, size_t modify_length,
      const uint8_t *source_data, size_t source_length,
      uint8_t *target_data, size_t *target_length)
{
   if (modify_length < 19)
      return BPS_PATCH_TOO_SMALL;

   struct bps_data bps = {0};
   bps.modify_data = modify_data;
   bps.modify_length = modify_length;
   bps.target_data = target_data;
   bps.target_length = *target_length;
   bps.source_data = source_data;
   bps.source_length = source_length;
   bps.modify_checksum = ~0;
   bps.target_checksum = ~0;

   if ((bps_read(&bps) != 'B') || (bps_read(&bps) != 'P') || (bps_read(&bps) != 'S') || (bps_read(&bps) != '1'))
      return BPS_PATCH_INVALID_HEADER;

   size_t modify_source_size = bps_decode(&bps);
   size_t modify_target_size = bps_decode(&bps);
   size_t modify_markup_size = bps_decode(&bps);
   for (size_t i = 0; i < modify_markup_size; i++)
      bps_read(&bps);

   if (modify_source_size > bps.source_length)
      return BPS_SOURCE_TOO_SMALL;
   if (modify_target_size > bps.target_length)
      return BPS_TARGET_TOO_SMALL;

   while (bps.modify_offset < bps.modify_length - 12)
   {
      size_t length = bps_decode(&bps);
      unsigned mode = length & 3;
      length = (length >> 2) + 1;

      switch (mode)
      {
         case SOURCE_READ:
            while (length--)
               bps_write(&bps, bps.source_data[bps.output_offset]);
            break;

         case TARGET_READ:
            while (length--)
               bps_write(&bps, bps_read(&bps));
            break;

         case SOURCE_COPY:
         case TARGET_COPY:
         {
            int offset = bps_decode(&bps);
            bool negative = offset & 1;
            offset >>= 1;
            if (negative)
               offset = -offset;

            if (mode == SOURCE_COPY)
            {
               bps.source_offset += offset;
               while (length--)
                  bps_write(&bps, bps.source_data[bps.source_offset++]);
            }
            else
            {
               bps.target_offset += offset;
               while (length--)
                  bps_write(&bps, bps.target_data[bps.target_offset++]);
               break;
            }
            break;
         }
      }
   }

   uint32_t modify_source_checksum = 0, modify_target_checksum = 0, modify_modify_checksum = 0;
   for (unsigned i = 0; i < 32; i += 8)
      modify_source_checksum |= bps_read(&bps) << i;
   for (unsigned i = 0; i < 32; i += 8)
      modify_target_checksum |= bps_read(&bps) << i;

   uint32_t checksum = ~bps.modify_checksum;
   for (unsigned i = 0; i < 32; i += 8)
      modify_modify_checksum |= bps_read(&bps) << i;

   bps.source_checksum = crc32_calculate(bps.source_data, bps.source_length);
   bps.target_checksum = ~bps.target_checksum;

   if (bps.source_checksum != modify_source_checksum)
      return BPS_SOURCE_CHECKSUM_INVALID;
   if (bps.target_checksum != modify_target_checksum)
      return BPS_TARGET_CHECKSUM_INVALID;
   if (checksum != modify_modify_checksum)
      return BPS_PATCH_CHECKSUM_INVALID;

   *target_length = modify_target_size;

   return BPS_SUCCESS;
}

