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

#include "ups.h"
#include "movie.h"
#include "msvc/msvc_compat.h"

struct ups_data
{
   const uint8_t *patch_data, *source_data; 
   uint8_t *target_data;
   unsigned patch_length, source_length, target_length;
   unsigned patch_offset, source_offset, target_offset;
   unsigned patch_checksum, source_checksum, target_checksum;
};

static uint8_t patch_read(struct ups_data *data) 
{
   if (data->patch_offset < data->patch_length) 
   {
      uint8_t n = data->patch_data[data->patch_offset++];
      data->patch_checksum = crc32_adjust(data->patch_checksum, n);
      return n;
   }
   return 0x00;
}

static uint8_t source_read(struct ups_data *data) 
{
   if (data->source_offset < data->source_length) 
   {
      uint8_t n = data->source_data[data->source_offset++];
      data->source_checksum = crc32_adjust(data->source_checksum, n);
      return n;
   }
   return 0x00;
}

static void target_write(struct ups_data *data, uint8_t n) 
{
   if (data->target_offset < data->target_length) 
   {
      data->target_data[data->target_offset] = n;
      data->target_checksum = crc32_adjust(data->target_checksum, n);
   }

   data->target_offset++;
}

static uint64_t decode(struct ups_data *data) 
{
   uint64_t offset = 0, shift = 1;
   while (true) 
   {
      uint8_t x = patch_read(data);
      offset += (x & 0x7f) * shift;
      if (x & 0x80) 
         break;
      shift <<= 7;
      offset += shift;
   }
   return offset;
}

ups_error_t ups_apply_patch(
      const uint8_t *patchdata, size_t patchlength,
      const uint8_t *sourcedata, size_t sourcelength,
      uint8_t *targetdata, size_t *targetlength)
{
   struct ups_data data = {0};
   data.patch_data = patchdata;
   data.source_data = sourcedata;
   data.target_data = targetdata;
   data.patch_length = patchlength;
   data.source_length = sourcelength;
   data.target_length = *targetlength;
   data.patch_checksum = ~0;
   data.source_checksum = ~0;
   data.target_checksum = ~0;

   if (data.patch_length < 18) 
      return UPS_PATCH_INVALID;
   if (patch_read(&data) != 'U') 
      return UPS_PATCH_INVALID;
   if (patch_read(&data) != 'P') 
      return UPS_PATCH_INVALID;
   if (patch_read(&data) != 'S') 
      return UPS_PATCH_INVALID;
   if (patch_read(&data) != '1') 
      return UPS_PATCH_INVALID;

   unsigned source_read_length = decode(&data);
   unsigned target_read_length = decode(&data);

   if (data.source_length != source_read_length && data.source_length != target_read_length) 
      return UPS_SOURCE_INVALID;
   *targetlength = (data.source_length == source_read_length ? target_read_length : source_read_length);
   if (data.target_length < *targetlength) 
      return UPS_TARGET_TOO_SMALL;
   data.target_length = *targetlength;

   while (data.patch_offset < data.patch_length - 12) 
   {
      unsigned length = decode(&data);
      while (length--) 
         target_write(&data, source_read(&data));
      while (true) 
      {
         uint8_t patch_xor = patch_read(&data);
         target_write(&data, patch_xor ^ source_read(&data));
         if (patch_xor == 0) break;
      }
   }

   while (data.source_offset < data.source_length) 
      target_write(&data, source_read(&data));
   while (data.target_offset < data.target_length) 
      target_write(&data, source_read(&data));

   uint32_t patch_read_checksum = 0, source_read_checksum = 0, target_read_checksum = 0;
   for (unsigned i = 0; i < 4; i++) 
      source_read_checksum |= patch_read(&data) << (i * 8);
   for (unsigned i = 0; i < 4; i++) 
      target_read_checksum |= patch_read(&data) << (i * 8);

   uint32_t patch_result_checksum = ~data.patch_checksum;
   data.source_checksum = ~data.source_checksum;
   data.target_checksum = ~data.target_checksum;

   for (unsigned i = 0; i < 4; i++) 
      patch_read_checksum |= patch_read(&data) << (i * 8);

   if (patch_result_checksum != patch_read_checksum) 
      return UPS_PATCH_INVALID;

   if (data.source_checksum == source_read_checksum && data.source_length == source_read_length) 
   {
      if (data.target_checksum == target_read_checksum && data.target_length == target_read_length) 
         return UPS_SUCCESS;
      return UPS_TARGET_INVALID;
   } 
   else if (data.source_checksum == target_read_checksum && data.source_length == target_read_length) 
   {
      if (data.target_checksum == source_read_checksum && data.target_length == source_read_length) 
         return UPS_SUCCESS;
      return UPS_TARGET_INVALID;
   } 
   else
      return UPS_SOURCE_INVALID;
}

