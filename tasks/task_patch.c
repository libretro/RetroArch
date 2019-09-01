/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

/* BPS/UPS/IPS implementation from bSNES (nall::).
 * Modified for RetroArch. */

/* TODO/FIXME - turn this into actual task */

#include <stdint.h>
#include <string.h>

#include <boolean.h>

#include <compat/msvc.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include <encodings/crc32.h>

#include "../msg_hash.h"
#include "../verbosity.h"

enum bps_mode
{
   SOURCE_READ = 0,
   TARGET_READ,
   SOURCE_COPY,
   TARGET_COPY
};

enum patch_error
{
   PATCH_UNKNOWN = 0,
   PATCH_SUCCESS,
   PATCH_PATCH_TOO_SMALL,
   PATCH_PATCH_INVALID_HEADER,
   PATCH_PATCH_INVALID,
   PATCH_SOURCE_TOO_SMALL,
   PATCH_TARGET_ALLOC_FAILED,
   PATCH_SOURCE_INVALID,
   PATCH_TARGET_INVALID,
   PATCH_SOURCE_CHECKSUM_INVALID,
   PATCH_TARGET_CHECKSUM_INVALID,
   PATCH_PATCH_CHECKSUM_INVALID
};

struct bps_data
{
   const uint8_t *modify_data;
   const uint8_t *source_data;
   uint8_t *target_data;
   size_t modify_length;
   size_t source_length;
   size_t target_length;
   size_t modify_offset;
   size_t source_offset;
   size_t target_offset;
   uint32_t modify_checksum;
   uint32_t source_checksum;
   uint32_t target_checksum;
   size_t source_relative_offset;
   size_t target_relative_offset;
   size_t output_offset;
};

struct ups_data
{
   const uint8_t *patch_data;
   const uint8_t *source_data;
   uint8_t *target_data;
   unsigned patch_length;
   unsigned source_length;
   unsigned target_length;
   unsigned patch_offset;
   unsigned source_offset;
   unsigned target_offset;
   unsigned patch_checksum;
   unsigned source_checksum;
   unsigned target_checksum;
};

typedef enum patch_error (*patch_func_t)(const uint8_t*, uint64_t,
      const uint8_t*, uint64_t, uint8_t**, uint64_t*);

static uint8_t bps_read(struct bps_data *bps)
{
   uint8_t data         = bps->modify_data[bps->modify_offset++];
   bps->modify_checksum = ~(encoding_crc32(
         ~bps->modify_checksum, &data, 1));
   return data;
}

static uint64_t bps_decode(struct bps_data *bps)
{
   uint64_t data = 0, shift = 1;

   for (;;)
   {
      uint8_t x  = bps_read(bps);
      data      += (x & 0x7f) * shift;
      if (x & 0x80)
         break;
      shift    <<= 7;
      data      += shift;
   }

   return data;
}

static void bps_write(struct bps_data *bps, uint8_t data)
{
   bps->target_data[bps->output_offset++] = data;
   bps->target_checksum = ~(encoding_crc32(~bps->target_checksum, &data, 1));
}

static enum patch_error bps_apply_patch(
      const uint8_t *modify_data, uint64_t modify_length,
      const uint8_t *source_data, uint64_t source_length,
      uint8_t **target_data, uint64_t *target_length)
{
   size_t i;
   uint32_t checksum;
   size_t modify_source_size;
   size_t modify_target_size;
   size_t modify_markup_size;
   struct bps_data bps;
   uint32_t modify_source_checksum = 0;
   uint32_t modify_target_checksum = 0;
   uint32_t modify_modify_checksum = 0;

   if (modify_length < 19)
      return PATCH_PATCH_TOO_SMALL;

   bps.modify_data            = modify_data;
   bps.source_data            = source_data;
   bps.target_data            = *target_data;
   bps.modify_length          = modify_length;
   bps.source_length          = source_length;
   bps.target_length          = *target_length;
   bps.modify_offset          = 0;
   bps.source_offset          = 0;
   bps.target_offset          = 0;
   bps.modify_checksum        = ~0;
   bps.source_checksum        = 0;
   bps.target_checksum        = ~0;
   bps.source_relative_offset = 0;
   bps.target_relative_offset = 0;
   bps.output_offset          = 0;

   if (  (bps_read(&bps) != 'B') ||
         (bps_read(&bps) != 'P') ||
         (bps_read(&bps) != 'S') ||
         (bps_read(&bps) != '1'))
      return PATCH_PATCH_INVALID_HEADER;

   modify_source_size  = bps_decode(&bps);
   modify_target_size  = bps_decode(&bps);
   modify_markup_size  = bps_decode(&bps);

   for (i = 0; i < modify_markup_size; i++)
      bps_read(&bps);

   if (modify_source_size > bps.source_length)
      return PATCH_SOURCE_TOO_SMALL;
   if (modify_target_size > bps.target_length){
      uint8_t *prov=(uint8_t*)malloc((size_t)modify_target_size);
      if (prov!=NULL){
         free(*target_data);
	      bps.target_data=prov;
         *target_data=prov;
         bps.target_length=modify_target_size;
      }else
         return PATCH_TARGET_ALLOC_FAILED;
   }

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
            int    offset = (int)bps_decode(&bps);
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

   for (i = 0; i < 32; i += 8)
      modify_source_checksum |= bps_read(&bps) << i;
   for (i = 0; i < 32; i += 8)
      modify_target_checksum |= bps_read(&bps) << i;

   checksum = ~bps.modify_checksum;
   for (i = 0; i < 32; i += 8)
      modify_modify_checksum |= bps_read(&bps) << i;

   bps.source_checksum = encoding_crc32(0,
         bps.source_data, bps.source_length);
   bps.target_checksum = ~bps.target_checksum;

   if (bps.source_checksum != modify_source_checksum)
      return PATCH_SOURCE_CHECKSUM_INVALID;

   if (bps.target_checksum != modify_target_checksum)
      return PATCH_TARGET_CHECKSUM_INVALID;

   if (checksum != modify_modify_checksum)
      return PATCH_PATCH_CHECKSUM_INVALID;

   *target_length = modify_target_size;

   return PATCH_SUCCESS;
}

static uint8_t ups_patch_read(struct ups_data *data)
{
   if (data && data->patch_offset < data->patch_length)
   {
      uint8_t n = data->patch_data[data->patch_offset++];
      data->patch_checksum =
         ~(encoding_crc32(~data->patch_checksum, &n, 1));
      return n;
   }
   return 0x00;
}

static uint8_t ups_source_read(struct ups_data *data)
{
   if (data && data->source_offset < data->source_length)
   {
      uint8_t n = data->source_data[data->source_offset++];
      data->source_checksum =
         ~(encoding_crc32(~data->source_checksum, &n, 1));
      return n;
   }
   return 0x00;
}

static void ups_target_write(struct ups_data *data, uint8_t n)
{

   if (data && data->target_offset < data->target_length)
   {
      data->target_data[data->target_offset] = n;
      data->target_checksum =
         ~(encoding_crc32(~data->target_checksum, &n, 1));
   }

   if (data)
      data->target_offset++;
}

static uint64_t ups_decode(struct ups_data *data)
{
   uint64_t offset = 0, shift = 1;
   while (true)
   {
      uint8_t x = ups_patch_read(data);
      offset   += (x & 0x7f) * shift;

      if (x & 0x80)
         break;
      shift <<= 7;
      offset += shift;
   }
   return offset;
}

static enum patch_error ups_apply_patch(
      const uint8_t *patchdata, uint64_t patchlength,
      const uint8_t *sourcedata, uint64_t sourcelength,
      uint8_t **targetdata, uint64_t *targetlength)
{
   size_t i;
   struct ups_data data;
   unsigned source_read_length;
   unsigned target_read_length;
   uint32_t patch_result_checksum;
   uint32_t patch_read_checksum  = 0;
   uint32_t source_read_checksum = 0;
   uint32_t target_read_checksum = 0;

   data.patch_data      = patchdata;
   data.source_data     = sourcedata;
   data.target_data     = *targetdata;
   data.patch_length    = (unsigned)patchlength;
   data.source_length   = (unsigned)sourcelength;
   data.target_length   = (unsigned)*targetlength;
   data.patch_offset    = 0;
   data.source_offset   = 0;
   data.target_offset   = 0;
   data.patch_checksum  = ~0;
   data.source_checksum = ~0;
   data.target_checksum = ~0;

   if (data.patch_length < 18)
      return PATCH_PATCH_INVALID;

   if (
         (ups_patch_read(&data) != 'U') ||
         (ups_patch_read(&data) != 'P') ||
         (ups_patch_read(&data) != 'S') ||
         (ups_patch_read(&data) != '1')
      )
      return PATCH_PATCH_INVALID;

   source_read_length = (unsigned)ups_decode(&data);
   target_read_length = (unsigned)ups_decode(&data);

   if (     (data.source_length != source_read_length)
         && (data.source_length != target_read_length))
      return PATCH_SOURCE_INVALID;

   *targetlength = (data.source_length == source_read_length ?
         target_read_length : source_read_length);

   if (data.target_length < *targetlength){
      uint8_t *prov=(uint8_t*)malloc((size_t)*targetlength);
      if(prov!=NULL){
         free(*targetdata);
         *targetdata=prov;
         data.target_data=prov;
      }else
         return PATCH_TARGET_ALLOC_FAILED;
   }

   data.target_length = (unsigned)*targetlength;
   
   while (data.patch_offset < data.patch_length - 12)
   {
      unsigned length = (unsigned)ups_decode(&data);
      while (length--)
         ups_target_write(&data, ups_source_read(&data));
      while (true)
      {
         uint8_t patch_xor = ups_patch_read(&data);
         ups_target_write(&data, patch_xor ^ ups_source_read(&data));
         if (patch_xor == 0)
            break;
      }
   }

   while (data.source_offset < data.source_length)
      ups_target_write(&data, ups_source_read(&data));
   while (data.target_offset < data.target_length)
      ups_target_write(&data, ups_source_read(&data));

   for (i = 0; i < 4; i++)
      source_read_checksum |= ups_patch_read(&data) << (i * 8);
   for (i = 0; i < 4; i++)
      target_read_checksum |= ups_patch_read(&data) << (i * 8);

   patch_result_checksum = ~data.patch_checksum;
   data.source_checksum  = ~data.source_checksum;
   data.target_checksum  = ~data.target_checksum;

   for (i = 0; i < 4; i++)
      patch_read_checksum |= ups_patch_read(&data) << (i * 8);

   if (patch_result_checksum != patch_read_checksum)
      return PATCH_PATCH_INVALID;

   if (data.source_checksum == source_read_checksum
         && data.source_length == source_read_length)
   {
      if (data.target_checksum == target_read_checksum
            && data.target_length == target_read_length)
         return PATCH_SUCCESS;
      return PATCH_TARGET_INVALID;
   }
   else if (data.source_checksum == target_read_checksum
         && data.source_length == target_read_length)
   {
      if (data.target_checksum == source_read_checksum
            && data.target_length == source_read_length)
         return PATCH_SUCCESS;
      return PATCH_TARGET_INVALID;
   }

   return PATCH_SOURCE_INVALID;
}

static enum patch_error ips_alloc_targetdata(
      const uint8_t *patchdata, uint64_t patchlen,
      uint64_t sourcelength,
      uint8_t **targetdata, uint64_t *targetlength)
{
   uint8_t *prov_alloc;
   uint32_t offset = 5;
   *targetlength = sourcelength;

   for (;;)
   {
      uint32_t address;
      unsigned length;

      if (offset > patchlen - 3)
         break;

      address  = patchdata[offset++] << 16;
      address |= patchdata[offset++] << 8;
      address |= patchdata[offset++] << 0;

      if (address == 0x454f46) /* EOF */
      {
         if (offset == patchlen)
         {
            prov_alloc=(uint8_t*)malloc((size_t)*targetlength);
            if (!prov_alloc)
               return PATCH_TARGET_ALLOC_FAILED;
            free(*targetdata);
            *targetdata=prov_alloc;
            return PATCH_SUCCESS;
         }
         else if (offset == patchlen - 3)
         {
            uint32_t size = patchdata[offset++] << 16;
            size |= patchdata[offset++] << 8;
            size |= patchdata[offset++] << 0;
            *targetlength = size;
            prov_alloc=(uint8_t*)malloc((size_t)*targetlength);
            if (!prov_alloc)
               return PATCH_TARGET_ALLOC_FAILED;
            free(*targetdata);
            *targetdata=prov_alloc;
            return PATCH_SUCCESS;
         }
      }

      if (offset > patchlen - 2)
         break;

      length  = patchdata[offset++] << 8;
      length |= patchdata[offset++] << 0;

      if (length) /* Copy */
      {
         if (offset > patchlen - length)
            break;

         while (length--)
         {
            address++;
            offset++;
         }
      }
      else /* RLE */
      {
         if (offset > patchlen - 3)
            break;

         length  = patchdata[offset++] << 8;
         length |= patchdata[offset++] << 0;

         if (length == 0) /* Illegal */
            break;

         while (length--)
            address++;

         offset++;
      }

      if (address > *targetlength)
         *targetlength = address;
   }

   return PATCH_PATCH_INVALID;
}

static enum patch_error ips_apply_patch(
      const uint8_t *patchdata, uint64_t patchlen,
      const uint8_t *sourcedata, uint64_t sourcelength,
      uint8_t **targetdata, uint64_t *targetlength)
{
   uint32_t offset = 5;
   enum patch_error error_patch = PATCH_UNKNOWN;
   if (patchlen < 8 ||
         patchdata[0] != 'P' ||
         patchdata[1] != 'A' ||
         patchdata[2] != 'T' ||
         patchdata[3] != 'C' ||
         patchdata[4] != 'H')
      return PATCH_PATCH_INVALID;
   
   error_patch = ips_alloc_targetdata( patchdata,     patchlen,
                                       sourcelength,  targetdata,
                                                      targetlength);
   if ( error_patch != PATCH_SUCCESS)
      return error_patch;

   memcpy(*targetdata, sourcedata, (size_t)sourcelength);

   for (;;)
   {
      uint32_t address;
      unsigned length;

      if (offset > patchlen - 3)
         break;

      address  = patchdata[offset++] << 16;
      address |= patchdata[offset++] << 8;
      address |= patchdata[offset++] << 0;

      if (address == 0x454f46) /* EOF */
      {
         if (offset == patchlen)
            return PATCH_SUCCESS;
         else if (offset == patchlen - 3)
         {
            uint32_t size = patchdata[offset++] << 16;
            size |= patchdata[offset++] << 8;
            size |= patchdata[offset++] << 0;
            return PATCH_SUCCESS;
         }
      }

      if (offset > patchlen - 2)
         break;

      length  = patchdata[offset++] << 8;
      length |= patchdata[offset++] << 0;

      if (length) /* Copy */
      {
         if (offset > patchlen - length)
            break;

         while (length--)
            (*targetdata)[address++] = patchdata[offset++];
      }
      else /* RLE */
      {
         if (offset > patchlen - 3)
            break;

         length  = patchdata[offset++] << 8;
         length |= patchdata[offset++] << 0;

         if (length == 0) /* Illegal */
            break;

         while (length--)
            (*targetdata)[address++] = patchdata[offset];

         offset++;
      }
   }

   return PATCH_PATCH_INVALID;
}

static bool apply_patch_content(uint8_t **buf,
      ssize_t *size, const char *patch_desc, const char *patch_path,
      patch_func_t func, void *patch_data, int64_t patch_size)
{
   enum patch_error err     = PATCH_UNKNOWN;
   ssize_t ret_size         = *size;
   uint8_t *ret_buf         = *buf;
   uint64_t target_size     = 0;
   uint8_t *patched_content = NULL;

   RARCH_LOG("Found %s file in \"%s\", attempting to patch ...\n",
         patch_desc, patch_path);

   err = func((const uint8_t*)patch_data, patch_size, ret_buf,
         ret_size, &patched_content, &target_size);

   if (err == PATCH_SUCCESS)
   {
      free(ret_buf);
      *buf  = patched_content;
      *size = target_size;
   }
   else
      RARCH_ERR("%s %s: %s #%u\n",
            msg_hash_to_str(MSG_FAILED_TO_PATCH),
            patch_desc,
            msg_hash_to_str(MSG_ERROR),
            (unsigned)err);

   return true;
}

static bool try_bps_patch(bool allow_bps, const char *name_bps,
      uint8_t **buf, ssize_t *size)
{
   if (allow_bps && !string_is_empty(name_bps))
      if (path_is_valid(name_bps))
      {
         int64_t patch_size;
         bool ret                 = false;
         void *patch_data         = NULL;

         if (!filestream_read_file(name_bps, &patch_data, &patch_size))
            return false;

         if (patch_size >= 0)
         {
            ret                      = apply_patch_content(
                  buf, size, "BPS", name_bps,
                  bps_apply_patch, patch_data, patch_size);
         }

         if (patch_data)
            free(patch_data);
         return ret;
      }
   return false;
}

static bool try_ups_patch(bool allow_ups, const char *name_ups,
      uint8_t **buf, ssize_t *size)
{
   if (allow_ups && !string_is_empty(name_ups))
      if (path_is_valid(name_ups))
      {
         int64_t patch_size;
         bool ret                 = false;
         void *patch_data         = NULL;

         if (!filestream_read_file(name_ups, &patch_data, &patch_size))
            return false;

         if (patch_size >= 0)
         {
            ret                      = apply_patch_content(
                  buf, size, "UPS", name_ups,
                  ups_apply_patch, patch_data, patch_size);
         }

         if (patch_data)
            free(patch_data);
         return ret;
      }
   return false;
}

static bool try_ips_patch(bool allow_ips,
      const char *name_ips, uint8_t **buf, ssize_t *size)
{
   if (allow_ips && !string_is_empty(name_ips))
      if (path_is_valid(name_ips))
      {
         int64_t patch_size;
         bool ret                 = false;
         void *patch_data         = NULL;

         if (!filestream_read_file(name_ips, &patch_data, &patch_size))
            return false;

         if (patch_size >= 0)
         {
            ret                      = apply_patch_content(
                  buf, size, "IPS", name_ips,
                  ips_apply_patch, patch_data, patch_size);
         }

         if (patch_data)
            free(patch_data);
         return ret;
      }
   return false;
}

/**
 * patch_content:
 * @buf          : buffer of the content file.
 * @size         : size   of the content file.
 *
 * Apply patch to the content file in-memory.
 *
 **/
static bool patch_content(
      bool is_ips_pref,
      bool is_bps_pref,
      bool is_ups_pref,
      const char *name_ips,
      const char *name_bps,
      const char *name_ups,
      uint8_t **buf,
      void *data)
{
   ssize_t *size    = (ssize_t*)data;
   bool allow_ups   = !is_bps_pref && !is_ips_pref;
   bool allow_ips   = !is_ups_pref && !is_bps_pref;
   bool allow_bps   = !is_ups_pref && !is_ips_pref;

   if (    (unsigned)is_ips_pref
         + (unsigned)is_bps_pref
         + (unsigned)is_ups_pref > 1)
   {
      RARCH_WARN("%s\n",
            msg_hash_to_str(MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED));
      return false;
   }

   if (     !try_ips_patch(allow_ips, name_ips, buf, size)
         && !try_bps_patch(allow_bps, name_bps, buf, size)
         && !try_ups_patch(allow_ups, name_ups, buf, size))
   {
      RARCH_LOG("%s\n",
            msg_hash_to_str(MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH));
      return false;
   }

   return true;
}
