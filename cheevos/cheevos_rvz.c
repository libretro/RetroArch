/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 - RetroArch Team
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

#include "cheevos_rvz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <streams/file_stream.h>
#include <retro_endianness.h>

#ifdef HAVE_ZSTD
#include <zstd.h>
#endif

#include "../verbosity.h"
#include "../deps/rcheevos/include/rc_consoles.h"

/* RVZ/WIA format constants */
#define RVZ_MAGIC 0x52565A01  /* "RVZ\x1" in big endian */
#define WIA_MAGIC 0x57494101  /* "WIA\x1" in big endian */

#define RVZ_HEADER1_SIZE 0x48
#define RVZ_HEADER2_SIZE 0xDC

/* Compression types */
#define RVZ_COMPRESSION_NONE  0
#define RVZ_COMPRESSION_PURGE 1
#define RVZ_COMPRESSION_BZIP2 2
#define RVZ_COMPRESSION_LZMA  3
#define RVZ_COMPRESSION_LZMA2 4
#define RVZ_COMPRESSION_ZSTD  5

/* Cache configuration */
#define RVZ_CHUNK_CACHE_SIZE 4  /* Cache up to 4 decompressed chunks */

/* Header structures based on docs/WiaAndRvz.md from Dolphin */
#pragma pack(push, 1)

typedef struct
{
   uint32_t magic;
   uint32_t version;
   uint32_t version_compatible;
   uint32_t header_2_size;
   uint8_t  header_2_hash[20];  /* SHA-1 */
   uint64_t iso_file_size;
   uint64_t wia_file_size;
   uint8_t  header_1_hash[20];  /* SHA-1 */
} rvz_header_1_t;

typedef struct
{
   uint32_t disc_type;
   uint32_t compression_type;
   int32_t  compression_level;
   uint32_t chunk_size;
   uint8_t  disc_header[0x80];
   uint32_t num_partition_entries;
   uint32_t partition_entry_size;
   uint64_t partition_entries_offset;
   uint8_t  partition_entries_hash[20];
   uint32_t num_raw_data_entries;
   uint64_t raw_data_entries_offset;
   uint32_t raw_data_entries_size;
   uint32_t num_group_entries;
   uint64_t group_entries_offset;
   uint32_t group_entries_size;
   uint8_t  compressor_data_size;
   uint8_t  compressor_data[7];
} rvz_header_2_t;

typedef struct
{
   uint64_t data_offset;
   uint64_t data_size;
   uint32_t group_index;
   uint32_t num_groups;
} rvz_raw_data_entry_t;

typedef struct
{
   uint32_t data_offset;  /* Divided by 4 */
   uint32_t data_size;    /* MSB indicates compression */
   uint32_t rvz_packed_size;
} rvz_group_entry_t;

#pragma pack(pop)

/* Decompressed chunk cache entry */
typedef struct
{
   uint32_t group_index;
   uint64_t decompressed_size;
   uint8_t* data;
   bool     valid;
} rvz_chunk_cache_entry_t;

/* Lagged Fibonacci Generator for junk data */
#define LFG_K 521
#define LFG_J 32
#define LFG_SEED_SIZE 17

typedef struct
{
   uint32_t buffer[LFG_K];
   uint32_t position_bytes;
} lfg_state_t;

/* RVZPack decompressor state for two-stage decompression */
typedef struct
{
   uint8_t* intermediate_data;     /* Buffer holding Zstd-decompressed data */
   uint32_t intermediate_size;     /* Size of intermediate buffer */
   uint32_t intermediate_pos;      /* Current read position in intermediate buffer */
   uint32_t rvz_packed_size;       /* Expected final output size */
   uint64_t data_offset;           /* Current disc offset (for junk seeding) */
   uint32_t current_block_size;    /* Size of current block being processed */
   uint32_t current_block_remaining; /* Bytes remaining in current block */
   bool     current_block_is_junk; /* Is current block junk data? */
   lfg_state_t lfg;                /* LFG state for junk generation */
} rvz_pack_state_t;

/* RVZ file handle */
struct rcheevos_rvz_file
{
   RFILE*   file;
   int64_t  position;  /* Virtual position in decompressed disc */

   rvz_header_1_t header_1;
   rvz_header_2_t header_2;

   /* Raw data entries (for non-partitioned data like GameCube) */
   rvz_raw_data_entry_t* raw_data_entries;
   uint32_t num_raw_data_entries;

   /* Group entries (map chunks to compressed data) */
   rvz_group_entry_t* group_entries;
   uint32_t num_group_entries;

   /* Chunk cache */
   rvz_chunk_cache_entry_t cache[RVZ_CHUNK_CACHE_SIZE];
   uint32_t cache_next;  /* LRU replacement index */
};

typedef struct rcheevos_rvz_file rcheevos_rvz_file_t;

/* Helper: Read big-endian values */
static uint32_t rvz_read_be32(const uint8_t* data)
{
   return ((uint32_t)data[0] << 24) |
          ((uint32_t)data[1] << 16) |
          ((uint32_t)data[2] << 8) |
          ((uint32_t)data[3]);
}

static uint64_t rvz_read_be64(const uint8_t* data)
{
   return ((uint64_t)data[0] << 56) |
          ((uint64_t)data[1] << 48) |
          ((uint64_t)data[2] << 40) |
          ((uint64_t)data[3] << 32) |
          ((uint64_t)data[4] << 24) |
          ((uint64_t)data[5] << 16) |
          ((uint64_t)data[6] << 8) |
          ((uint64_t)data[7]);
}

/* Helper: Parse header 1 */
static bool rvz_parse_header_1(RFILE* file, rvz_header_1_t* header)
{
   uint8_t buffer[RVZ_HEADER1_SIZE];

   if (filestream_seek(file, 0, SEEK_SET) != 0)
      return false;

   if (filestream_read(file, buffer, RVZ_HEADER1_SIZE) != RVZ_HEADER1_SIZE)
      return false;

   /* Parse header fields (RVZ format uses big-endian) */
   header->magic                 = rvz_read_be32(buffer);
   header->version               = rvz_read_be32(buffer + 0x04);
   header->version_compatible    = rvz_read_be32(buffer + 0x08);
   header->header_2_size         = rvz_read_be32(buffer + 0x0C);
   memcpy(header->header_2_hash, buffer + 0x10, 20);
   header->iso_file_size         = rvz_read_be64(buffer + 0x24);
   header->wia_file_size         = rvz_read_be64(buffer + 0x2C);
   memcpy(header->header_1_hash, buffer + 0x34, 20);

   return true;
}

/* Helper: Parse header 2 */
static bool rvz_parse_header_2(RFILE* file, rvz_header_2_t* header)
{
   uint8_t buffer[RVZ_HEADER2_SIZE];

   if (filestream_seek(file, RVZ_HEADER1_SIZE, SEEK_SET) != 0)
      return false;

   if (filestream_read(file, buffer, RVZ_HEADER2_SIZE) != RVZ_HEADER2_SIZE)
      return false;

   header->disc_type                 = rvz_read_be32(buffer + 0x00);
   header->compression_type          = rvz_read_be32(buffer + 0x04);
   header->compression_level         = (int32_t)rvz_read_be32(buffer + 0x08);
   header->chunk_size                = rvz_read_be32(buffer + 0x0C);
   memcpy(header->disc_header, buffer + 0x10, 0x80);
   header->num_partition_entries     = rvz_read_be32(buffer + 0x90);
   header->partition_entry_size      = rvz_read_be32(buffer + 0x94);
   header->partition_entries_offset  = rvz_read_be64(buffer + 0x98);
   memcpy(header->partition_entries_hash, buffer + 0xA0, 20);
   header->num_raw_data_entries      = rvz_read_be32(buffer + 0xB4);
   header->raw_data_entries_offset   = rvz_read_be64(buffer + 0xB8);
   header->raw_data_entries_size     = rvz_read_be32(buffer + 0xC0);
   header->num_group_entries         = rvz_read_be32(buffer + 0xC4);
   header->group_entries_offset      = rvz_read_be64(buffer + 0xC8);
   header->group_entries_size        = rvz_read_be32(buffer + 0xD0);
   header->compressor_data_size      = buffer[0xD4];
   memcpy(header->compressor_data, buffer + 0xD5, 7);

   return true;
}

/* Helper: Decompress data (raw_data_entries, group_entries, etc are compressed) */
static uint8_t* rvz_decompress_data(RFILE* file, uint64_t offset, uint32_t compressed_size,
                                     uint32_t decompressed_size, uint32_t compression_type)
{
   uint8_t* compressed_data = NULL;
   uint8_t* decompressed_data = NULL;

   /* Read compressed data from file */
   compressed_data = (uint8_t*)malloc(compressed_size);
   if (!compressed_data)
      return NULL;

   if (filestream_seek(file, offset, SEEK_SET) != 0)
   {
      free(compressed_data);
      return NULL;
   }

   if (filestream_read(file, compressed_data, compressed_size) != compressed_size)
   {
      free(compressed_data);
      return NULL;
   }

   /* Decompress based on compression type */
   if (compression_type == RVZ_COMPRESSION_NONE)
   {
      /* Data is not compressed, return as-is */
      return compressed_data;
   }

#ifdef HAVE_ZSTD
   if (compression_type == RVZ_COMPRESSION_ZSTD)
   {
      size_t result;
      decompressed_data = (uint8_t*)malloc(decompressed_size);
      if (!decompressed_data)
      {
         free(compressed_data);
         return NULL;
      }

      result = ZSTD_decompress(decompressed_data, decompressed_size,
                               compressed_data, compressed_size);

      free(compressed_data);

      if (ZSTD_isError(result))
      {
         RARCH_ERR("[RVZ] Failed to decompress metadata: %s\n", ZSTD_getErrorName(result));
         free(decompressed_data);
         return NULL;
      }

      return decompressed_data;
   }
#endif

   RARCH_ERR("[RVZ] Unsupported compression type for metadata: %u\n", compression_type);
   free(compressed_data);
   return NULL;
}

/* Helper: Parse raw data entries */
static bool rvz_parse_raw_data_entries(RFILE* file, rcheevos_rvz_file_t* rvz)
{
   uint32_t i;
   uint8_t* buffer;
   uint32_t entry_size = sizeof(rvz_raw_data_entry_t);
   uint32_t decompressed_size;

   if (rvz->header_2.num_raw_data_entries == 0)
      return true;

   rvz->raw_data_entries = (rvz_raw_data_entry_t*)calloc(
      rvz->header_2.num_raw_data_entries, sizeof(rvz_raw_data_entry_t));
   if (!rvz->raw_data_entries)
      return false;

   /* Raw data entries are compressed - decompress them first */
   decompressed_size = entry_size * rvz->header_2.num_raw_data_entries;
   buffer = rvz_decompress_data(file,
                                 rvz->header_2.raw_data_entries_offset,
                                 rvz->header_2.raw_data_entries_size,
                                 decompressed_size,
                                 rvz->header_2.compression_type);
   if (!buffer)
   {
      RARCH_ERR("[RVZ] Failed to decompress raw data entries\n");
      return false;
   }

   for (i = 0; i < rvz->header_2.num_raw_data_entries; i++)
   {
      uint8_t* entry = buffer + (i * entry_size);
      rvz->raw_data_entries[i].data_offset  = rvz_read_be64(entry + 0x00);
      rvz->raw_data_entries[i].data_size    = rvz_read_be64(entry + 0x08);
      rvz->raw_data_entries[i].group_index  = rvz_read_be32(entry + 0x10);
      rvz->raw_data_entries[i].num_groups   = rvz_read_be32(entry + 0x14);

      RARCH_LOG("[RVZ] Raw data entry %u: offset=%llu size=%llu group_index=%u num_groups=%u\n",
                i, (unsigned long long)rvz->raw_data_entries[i].data_offset,
                (unsigned long long)rvz->raw_data_entries[i].data_size,
                rvz->raw_data_entries[i].group_index,
                rvz->raw_data_entries[i].num_groups);
   }

   free(buffer);
   rvz->num_raw_data_entries = rvz->header_2.num_raw_data_entries;
   return true;
}

/* Helper: Parse group entries */
static bool rvz_parse_group_entries(RFILE* file, rcheevos_rvz_file_t* rvz)
{
   uint32_t i;
   uint8_t* buffer;
   uint32_t entry_size = sizeof(rvz_group_entry_t);
   uint32_t decompressed_size;

   if (rvz->header_2.num_group_entries == 0)
      return true;

   rvz->group_entries = (rvz_group_entry_t*)calloc(
      rvz->header_2.num_group_entries, sizeof(rvz_group_entry_t));
   if (!rvz->group_entries)
      return false;

   /* Group entries are compressed - decompress them first */
   decompressed_size = entry_size * rvz->header_2.num_group_entries;
   buffer = rvz_decompress_data(file,
                                 rvz->header_2.group_entries_offset,
                                 rvz->header_2.group_entries_size,
                                 decompressed_size,
                                 rvz->header_2.compression_type);
   if (!buffer)
   {
      RARCH_ERR("[RVZ] Failed to decompress group entries\n");
      return false;
   }

   for (i = 0; i < rvz->header_2.num_group_entries; i++)
   {
      uint8_t* entry = buffer + (i * entry_size);
      rvz->group_entries[i].data_offset      = rvz_read_be32(entry + 0x00);
      rvz->group_entries[i].data_size        = rvz_read_be32(entry + 0x04);
      rvz->group_entries[i].rvz_packed_size  = rvz_read_be32(entry + 0x08);

      if (i < 5)
      {
         RARCH_LOG("[RVZ] Group %u: file_offset=%llu data_size=0x%08X rvz_packed_size=%u\n",
                   i, (unsigned long long)(rvz->group_entries[i].data_offset * 4ULL),
                   rvz->group_entries[i].data_size,
                   rvz->group_entries[i].rvz_packed_size);
      }
   }

   free(buffer);
   rvz->num_group_entries = rvz->header_2.num_group_entries;
   return true;
}

/* LFG implementation for junk data generation */
static void lfg_forward_one(lfg_state_t* lfg)
{
   uint32_t i;
   for (i = 0; i < LFG_J; i++)
      lfg->buffer[i] ^= lfg->buffer[i + LFG_K - LFG_J];

   for (i = LFG_J; i < LFG_K; i++)
      lfg->buffer[i] ^= lfg->buffer[i - LFG_J];
}

static void lfg_initialize(lfg_state_t* lfg)
{
   uint32_t i;
   uint32_t x;

   /* Initialize buffer from seed */
   for (i = LFG_SEED_SIZE; i < LFG_K; i++)
   {
      lfg->buffer[i] = (lfg->buffer[i - 17] << 23) ^
                       (lfg->buffer[i - 16] >> 9) ^
                       lfg->buffer[i - 1];
   }

   /* Byteswap and shift */
   for (i = 0; i < LFG_K; i++)
   {
      x = lfg->buffer[i];
      x = (x & 0xFF00FFFF) | ((x >> 2) & 0x00FF0000);
      /* Byteswap: swap32 from big-endian (buffer) to little-endian for output */
      lfg->buffer[i] = ((x & 0xFF000000) >> 24) |
                       ((x & 0x00FF0000) >> 8) |
                       ((x & 0x0000FF00) << 8) |
                       ((x & 0x000000FF) << 24);
   }

   /* Forward 4 times */
   for (i = 0; i < 4; i++)
      lfg_forward_one(lfg);
}

static void lfg_set_seed(lfg_state_t* lfg, const uint8_t* seed)
{
   uint32_t i;
   lfg->position_bytes = 0;

   /* Read seed as big-endian u32s */
   for (i = 0; i < LFG_SEED_SIZE; i++)
   {
      lfg->buffer[i] = rvz_read_be32(seed + i * 4);
   }

   lfg_initialize(lfg);
}

static void lfg_forward(lfg_state_t* lfg, uint32_t count)
{
   lfg->position_bytes += count;
   while (lfg->position_bytes >= LFG_K * 4)
   {
      lfg_forward_one(lfg);
      lfg->position_bytes -= LFG_K * 4;
   }
}

static void lfg_get_bytes(lfg_state_t* lfg, uint32_t count, uint8_t* out)
{
   uint8_t* buffer_bytes = (uint8_t*)lfg->buffer;

   while (count > 0)
   {
      uint32_t length = count;
      if (length > LFG_K * 4 - lfg->position_bytes)
         length = LFG_K * 4 - lfg->position_bytes;

      memcpy(out, buffer_bytes + lfg->position_bytes, length);

      lfg->position_bytes += length;
      count -= length;
      out += length;

      if (lfg->position_bytes == LFG_K * 4)
      {
         lfg_forward_one(lfg);
         lfg->position_bytes = 0;
      }
   }
}

/* Helper: Read big-endian uint32 from buffer */
static uint32_t rvz_pack_read_u32(const uint8_t* data)
{
   return ((uint32_t)data[0] << 24) |
          ((uint32_t)data[1] << 16) |
          ((uint32_t)data[2] << 8) |
          ((uint32_t)data[3]);
}

/* Helper: RVZPack decompressor - unpack size-prefixed data blocks */
static bool rvz_pack_decompress(rvz_pack_state_t* pack, uint8_t* output,
                                 uint32_t output_size, uint32_t* bytes_written)
{
   uint32_t size_field;
   uint32_t to_copy;

   *bytes_written = 0;

   while (*bytes_written < output_size)
   {
      /* Read new block header if needed */
      if (pack->current_block_remaining == 0)
      {
         /* Need at least 4 bytes for size field */
         if (pack->intermediate_pos + 4 > pack->intermediate_size)
         {
            RARCH_LOG("[RVZ] RVZPack: Not enough data for size field\n");
            return true; /* Partial read is OK */
         }

         /* Read size field (big-endian) */
         size_field = rvz_pack_read_u32(pack->intermediate_data + pack->intermediate_pos);
         pack->intermediate_pos += 4;

         /* Check junk flag (bit 31) */
         pack->current_block_is_junk = (size_field & 0x80000000) != 0;
         pack->current_block_size = size_field & 0x7FFFFFFF;
         pack->current_block_remaining = pack->current_block_size;

         /* Sanity check: block size shouldn't be 0 or too large */
         if (pack->current_block_size == 0)
         {
            RARCH_LOG("[RVZ] RVZPack: Warning - zero-sized block, ending decompression\n");
            return true;
         }
         if (pack->current_block_size > 0x1000000) /* 16MB max */
         {
            RARCH_ERR("[RVZ] RVZPack: Block size too large: 0x%08X\n", pack->current_block_size);
            return false;
         }

         RARCH_LOG("[RVZ] RVZPack: Block size=0x%08X junk=%d\n",
                   pack->current_block_size, pack->current_block_is_junk);

         /* Handle junk blocks - read LFG seed and initialize */
         if (pack->current_block_is_junk)
         {
            /* Junk blocks have 17 u32s (68 bytes) of LFG seed data */
            if (pack->intermediate_pos + 68 > pack->intermediate_size)
            {
               RARCH_LOG("[RVZ] RVZPack: Not enough data for LFG seed\n");
               return false;
            }

            lfg_set_seed(&pack->lfg, pack->intermediate_data + pack->intermediate_pos);
            pack->intermediate_pos += 68;

            /* Forward LFG by data_offset % 0x8000 */
            lfg_forward(&pack->lfg, pack->data_offset % 0x8000);

            RARCH_LOG("[RVZ] RVZPack: Junk block - will generate %u bytes with LFG (offset=%llu)\n",
                      pack->current_block_size, (unsigned long long)pack->data_offset);
         }
      }

      /* Copy/generate data for current block */
      to_copy = pack->current_block_remaining;
      if (to_copy > output_size - *bytes_written)
         to_copy = output_size - *bytes_written;

      if (pack->current_block_is_junk)
      {
         /* Generate junk data using LFG */
         lfg_get_bytes(&pack->lfg, to_copy, output + *bytes_written);
      }
      else
      {
         /* Copy real data from intermediate buffer */
         if (pack->intermediate_pos + to_copy > pack->intermediate_size)
         {
            to_copy = pack->intermediate_size - pack->intermediate_pos;
            if (to_copy == 0)
               return true; /* No more data available */
         }

         memcpy(output + *bytes_written,
                pack->intermediate_data + pack->intermediate_pos,
                to_copy);
         pack->intermediate_pos += to_copy;
      }

      *bytes_written += to_copy;
      pack->current_block_remaining -= to_copy;
      pack->data_offset += to_copy;
   }

   return true;
}

/* Helper: Decompress a chunk */
static bool rvz_decompress_chunk(rcheevos_rvz_file_t* rvz, uint32_t group_index,
                                 uint8_t** out_data, uint64_t* out_size)
{
   rvz_group_entry_t* group;
   uint64_t compressed_offset;
   uint32_t compressed_size;
   uint32_t decompressed_size;
   uint8_t* compressed_data   = NULL;
   uint8_t* decompressed_data = NULL;
   bool is_compressed;
   uint32_t actual_decompressed_size;
   rvz_pack_state_t pack_state;
   uint32_t bytes_written;

   if (group_index >= rvz->num_group_entries)
   {
      RARCH_ERR("[RVZ] Invalid group index: %u\n", group_index);
      return false;
   }

   group = &rvz->group_entries[group_index];

   /* Check if this chunk is compressed */
   is_compressed = (group->data_size & 0x80000000) != 0;
   compressed_size = group->data_size & 0x7FFFFFFF;

   /* Special case: all zeros */
   if (compressed_size == 0)
   {
      decompressed_size  = rvz->header_2.chunk_size;
      decompressed_data  = (uint8_t*)calloc(1, decompressed_size);
      if (!decompressed_data)
         return false;

      *out_data = decompressed_data;
      *out_size = decompressed_size;
      return true;
   }

   compressed_offset = (uint64_t)group->data_offset * 4;
   /* rvz_packed_size is the actual decompressed size from Zstd */
   actual_decompressed_size = group->rvz_packed_size ? group->rvz_packed_size : rvz->header_2.chunk_size;
   /* But chunks should be padded to full chunk_size */
   decompressed_size = rvz->header_2.chunk_size;

   /* Read compressed data */
   compressed_data = (uint8_t*)malloc(compressed_size);
   if (!compressed_data)
      return false;

   if (filestream_seek(rvz->file, compressed_offset, SEEK_SET) != 0)
   {
      free(compressed_data);
      return false;
   }

   if (filestream_read(rvz->file, compressed_data, compressed_size) != compressed_size)
   {
      free(compressed_data);
      return false;
   }

   /* Decompress based on compression type */
   if (is_compressed)
   {
      uint32_t compression_type = rvz->header_2.compression_type;

      switch (compression_type)
      {
#ifdef HAVE_ZSTD
         case RVZ_COMPRESSION_ZSTD:
         {
            size_t result;
            uint8_t* zstd_output;
            uint32_t zstd_output_size;

            /* Check if we need RVZPack decompression (two-stage) */
            if (group->rvz_packed_size != 0)
            {
               /* Two-stage decompression: Zstd -> RVZPack */
               RARCH_LOG("[RVZ] Group %u: Two-stage decompression (rvz_packed_size=%u)\n",
                         group_index, group->rvz_packed_size);

               /* Stage 1: Zstd decompress to intermediate buffer */
               zstd_output_size = group->rvz_packed_size;
               zstd_output = (uint8_t*)malloc(zstd_output_size);
               if (!zstd_output)
               {
                  free(compressed_data);
                  return false;
               }

               result = ZSTD_decompress(zstd_output, zstd_output_size,
                                        compressed_data, compressed_size);

               if (ZSTD_isError(result))
               {
                  RARCH_ERR("[RVZ] Zstd decompression (stage 1) failed: %s\n", ZSTD_getErrorName(result));
                  free(compressed_data);
                  free(zstd_output);
                  return false;
               }

               /* Stage 2: RVZPack decompress to final buffer */
               decompressed_data = (uint8_t*)calloc(1, decompressed_size);
               if (!decompressed_data)
               {
                  free(compressed_data);
                  free(zstd_output);
                  return false;
               }

               memset(&pack_state, 0, sizeof(pack_state));
               pack_state.intermediate_data = zstd_output;
               pack_state.intermediate_size = zstd_output_size;
               pack_state.intermediate_pos = 0;
               pack_state.rvz_packed_size = group->rvz_packed_size;
               pack_state.data_offset = 0;

               if (!rvz_pack_decompress(&pack_state, decompressed_data, decompressed_size, &bytes_written))
               {
                  RARCH_ERR("[RVZ] RVZPack decompression (stage 2) failed\n");
                  free(compressed_data);
                  free(zstd_output);
                  free(decompressed_data);
                  return false;
               }

               RARCH_LOG("[RVZ] RVZPack: Decompressed %u -> %u bytes\n",
                         zstd_output_size, bytes_written);

               free(zstd_output);
            }
            else
            {
               /* Single-stage decompression: Just Zstd */
               RARCH_LOG("[RVZ] Group %u: Single-stage decompression (Zstd only)\n", group_index);

               decompressed_data = (uint8_t*)calloc(1, decompressed_size);
               if (!decompressed_data)
               {
                  free(compressed_data);
                  return false;
               }

               result = ZSTD_decompress(decompressed_data, actual_decompressed_size,
                                        compressed_data, compressed_size);

               if (ZSTD_isError(result))
               {
                  RARCH_ERR("[RVZ] Zstd decompression failed: %s\n", ZSTD_getErrorName(result));
                  free(compressed_data);
                  free(decompressed_data);
                  return false;
               }
            }
            /* Remaining bytes (if any) are already zero from calloc */
            break;
         }
#endif
         default:
            RARCH_ERR("[RVZ] Unsupported compression type: %u\n", compression_type);
            free(compressed_data);
            return false;
      }
   }
   else
   {
      /* Data is uncompressed */
      if (group->rvz_packed_size != 0)
      {
         /* Uncompressed but RVZPack-encoded */
         RARCH_LOG("[RVZ] Group %u: Uncompressed RVZPack (rvz_packed_size=%u)\n",
                   group_index, group->rvz_packed_size);

         decompressed_data = (uint8_t*)calloc(1, decompressed_size);
         if (!decompressed_data)
         {
            free(compressed_data);
            return false;
         }

         memset(&pack_state, 0, sizeof(pack_state));
         pack_state.intermediate_data = compressed_data;
         pack_state.intermediate_size = compressed_size;
         pack_state.intermediate_pos = 0;
         pack_state.rvz_packed_size = group->rvz_packed_size;
         pack_state.data_offset = 0;

         if (!rvz_pack_decompress(&pack_state, decompressed_data, decompressed_size, &bytes_written))
         {
            RARCH_ERR("[RVZ] RVZPack decompression failed\n");
            free(compressed_data);
            free(decompressed_data);
            return false;
         }

         RARCH_LOG("[RVZ] RVZPack: Decompressed %u -> %u bytes\n",
                   compressed_size, bytes_written);

         free(compressed_data);
         compressed_data = NULL;  /* Avoid double-free */
      }
      else
      {
         /* Uncompressed, no RVZPack - just use raw data */
         decompressed_data = compressed_data;
         compressed_data   = NULL;  /* Transfer ownership */
      }
   }

   if (compressed_data)
      free(compressed_data);

   *out_data = decompressed_data;
   *out_size = decompressed_size;
   return true;
}

/* Helper: Get decompressed chunk (with cache) */
static uint8_t* rvz_get_chunk(rcheevos_rvz_file_t* rvz, uint32_t group_index, uint64_t* out_size)
{
   uint32_t i;
   rvz_chunk_cache_entry_t* entry;

   /* Check cache first */
   for (i = 0; i < RVZ_CHUNK_CACHE_SIZE; i++)
   {
      if (rvz->cache[i].valid && rvz->cache[i].group_index == group_index)
      {
         *out_size = rvz->cache[i].decompressed_size;
         return rvz->cache[i].data;
      }
   }

   /* Not in cache, decompress it */
   entry = &rvz->cache[rvz->cache_next];

   /* Free old cache entry */
   if (entry->valid && entry->data)
   {
      free(entry->data);
      entry->data = NULL;
   }

   /* Decompress new chunk */
   if (!rvz_decompress_chunk(rvz, group_index, &entry->data, &entry->decompressed_size))
   {
      entry->valid = false;
      return NULL;
   }

   entry->group_index = group_index;
   entry->valid       = true;

   /* Debug: Print bytes at key offsets */
   if (group_index == 0 && entry->decompressed_size >= 256)
   {
      size_t i;
      RARCH_LOG("[RVZ_CHUNK] Group %u decompressed to %llu bytes.\n",
                group_index, (unsigned long long)entry->decompressed_size);
      RARCH_LOG("[RVZ_CHUNK]   Bytes 0-31:   ");
      for (i = 0; i < 32; i++)
         RARCH_LOG("%02X ", entry->data[i]);
      RARCH_LOG("\n");
      RARCH_LOG("[RVZ_CHUNK]   Bytes 128-159: ");
      for (i = 128; i < 160; i++)
         RARCH_LOG("%02X ", entry->data[i]);
      RARCH_LOG("\n");
      RARCH_LOG("[RVZ_CHUNK]   Bytes 0x2440-0x2453 (apploader start): ");
      if (entry->decompressed_size >= 0x2454)
      {
         for (i = 0x2440; i < 0x2454; i++)
            RARCH_LOG("%02X ", entry->data[i]);
      }
      RARCH_LOG("\n");
   }

   /* Update LRU */
   rvz->cache_next = (rvz->cache_next + 1) % RVZ_CHUNK_CACHE_SIZE;

   *out_size = entry->decompressed_size;
   return entry->data;
}

/* Public API implementations */

void* rcheevos_rvz_open(const char* path)
{
   rcheevos_rvz_file_t* rvz;
   uint32_t i;

   if (!path)
      return NULL;

   rvz = (rcheevos_rvz_file_t*)calloc(1, sizeof(rcheevos_rvz_file_t));
   if (!rvz)
      return NULL;

   rvz->file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
                               RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!rvz->file)
   {
      free(rvz);
      return NULL;
   }

   /* Parse headers */
   if (!rvz_parse_header_1(rvz->file, &rvz->header_1))
   {
      RARCH_ERR("[RVZ] Failed to parse header 1\n");
      filestream_close(rvz->file);
      free(rvz);
      return NULL;
   }

   if (rvz->header_1.magic != RVZ_MAGIC && rvz->header_1.magic != WIA_MAGIC)
   {
      RARCH_ERR("[RVZ] Invalid magic: 0x%08X\n", rvz->header_1.magic);
      filestream_close(rvz->file);
      free(rvz);
      return NULL;
   }

   if (!rvz_parse_header_2(rvz->file, &rvz->header_2))
   {
      RARCH_ERR("[RVZ] Failed to parse header 2\n");
      filestream_close(rvz->file);
      free(rvz);
      return NULL;
   }

   /* Check compression support */
   if (rvz->header_2.compression_type != RVZ_COMPRESSION_NONE &&
       rvz->header_2.compression_type != RVZ_COMPRESSION_ZSTD)
   {
      RARCH_ERR("[RVZ] Unsupported compression type: %u (only Zstd is currently supported)\n",
                rvz->header_2.compression_type);
      filestream_close(rvz->file);
      free(rvz);
      return NULL;
   }

   /* Parse data structures */
   if (!rvz_parse_raw_data_entries(rvz->file, rvz))
   {
      RARCH_ERR("[RVZ] Failed to parse raw data entries\n");
      filestream_close(rvz->file);
      free(rvz);
      return NULL;
   }

   if (!rvz_parse_group_entries(rvz->file, rvz))
   {
      RARCH_ERR("[RVZ] Failed to parse group entries\n");
      if (rvz->raw_data_entries)
         free(rvz->raw_data_entries);
      filestream_close(rvz->file);
      free(rvz);
      return NULL;
   }

   /* Initialize cache */
   rvz->cache_next = 0;
   for (i = 0; i < RVZ_CHUNK_CACHE_SIZE; i++)
   {
      rvz->cache[i].valid = false;
      rvz->cache[i].data  = NULL;
   }

   RARCH_LOG("[RVZ] Opened %s format file (compression: %u, chunk_size: %u, iso_size: %llu)\n",
             rvz->header_1.magic == RVZ_MAGIC ? "RVZ" : "WIA",
             rvz->header_2.compression_type,
             rvz->header_2.chunk_size,
             (unsigned long long)rvz->header_1.iso_file_size);

   return rvz;
}

void rcheevos_rvz_seek(void* file_handle, int64_t offset, int origin)
{
   rcheevos_rvz_file_t* handle = (rcheevos_rvz_file_t*)file_handle;
   int64_t new_pos;

   if (!handle)
      return;

   switch (origin)
   {
      case SEEK_SET:
         new_pos = offset;
         break;
      case SEEK_CUR:
         new_pos = handle->position + offset;
         break;
      case SEEK_END:
         new_pos = (int64_t)handle->header_1.iso_file_size + offset;
         break;
      default:
         return;
   }

   if (new_pos < 0 || new_pos > (int64_t)handle->header_1.iso_file_size)
      return;

   handle->position = new_pos;
}

int64_t rcheevos_rvz_tell(void* file_handle)
{
   rcheevos_rvz_file_t* handle = (rcheevos_rvz_file_t*)file_handle;

   if (!handle)
      return -1;

   return handle->position;
}

size_t rcheevos_rvz_read(void* file_handle, void* buffer, size_t size)
{
   rcheevos_rvz_file_t* handle = (rcheevos_rvz_file_t*)file_handle;
   size_t total_read = 0;
   uint8_t* output   = (uint8_t*)buffer;

   if (!handle || !buffer || size == 0)
      return 0;

   RARCH_LOG("[RVZ_READ] pos=%lld size=%zu\n",
             (long long)handle->position, size);

   /* Clamp read to file size */
   if (handle->position + (int64_t)size > (int64_t)handle->header_1.iso_file_size)
      size = (size_t)((int64_t)handle->header_1.iso_file_size - handle->position);

   /* Read from disc header (first 128 bytes stored in header 2) */
   if (handle->position < 128 && total_read < size)
   {
      uint64_t offset_in_header = (uint64_t)handle->position;
      uint64_t available        = 128 - offset_in_header;
      uint64_t to_copy          = size - total_read;

      if (to_copy > available)
         to_copy = available;

      RARCH_LOG("[RVZ_READ] Reading %llu bytes from disc_header at offset %llu\n",
                (unsigned long long)to_copy, (unsigned long long)offset_in_header);

      memcpy(output + total_read,
             handle->header_2.disc_header + offset_in_header,
             (size_t)to_copy);

      total_read       += (size_t)to_copy;
      handle->position += (int64_t)to_copy;
   }

   /* Read from raw data entries (GameCube discs use this) */
   if (handle->num_raw_data_entries > 0)
   {
      uint32_t i;
      for (i = 0; i < handle->num_raw_data_entries && total_read < size; i++)
      {
         rvz_raw_data_entry_t* entry = &handle->raw_data_entries[i];
         /* entry_end should account for alignment */
         uint64_t skipped_data_for_end = entry->data_offset % 0x8000;
         uint64_t aligned_size = entry->data_size + skipped_data_for_end;
         int64_t entry_end = (int64_t)((entry->data_offset - skipped_data_for_end) + aligned_size);

         /* Fill gap with zeros if position is before this entry */
         if (handle->position < (int64_t)entry->data_offset && total_read < size)
         {
            uint64_t gap_size = (uint64_t)((int64_t)entry->data_offset - handle->position);
            uint64_t to_zero  = size - total_read;

            if (to_zero > gap_size)
               to_zero = gap_size;

            memset(output + total_read, 0, (size_t)to_zero);
            total_read       += (size_t)to_zero;
            handle->position += (int64_t)to_zero;
         }

         /* Check if current position is within this entry */
         if (handle->position >= (int64_t)entry->data_offset &&
             handle->position < entry_end)
         {
            uint32_t chunk_size       = handle->header_2.chunk_size;
            /* Dolphin aligns data_offset down to sector boundary (0x8000).
             * The first skipped_data bytes in the first group are padding. */
            uint64_t skipped_data     = entry->data_offset % 0x8000;
            uint64_t aligned_offset   = entry->data_offset - skipped_data;
            uint64_t offset_in_groups = (uint64_t)(handle->position - (int64_t)aligned_offset);
            uint32_t group_index      = (uint32_t)(offset_in_groups / chunk_size) + entry->group_index;
            uint64_t offset_in_chunk  = offset_in_groups % chunk_size;

            RARCH_LOG("[RVZ_READ] Entry %u: data_offset=%llu aligned=%llu skipped=%llu data_size=%llu entry_end=%lld\n",
                      i, (unsigned long long)entry->data_offset, (unsigned long long)aligned_offset,
                      (unsigned long long)skipped_data, (unsigned long long)entry->data_size, (long long)entry_end);
            RARCH_LOG("[RVZ_READ]   offset_in_groups=%llu group_index=%u (base=%u) offset_in_chunk=%llu\n",
                      (unsigned long long)offset_in_groups, group_index, entry->group_index,
                      (unsigned long long)offset_in_chunk);

            while (total_read < size && handle->position < entry_end)
            {
               uint64_t chunk_data_size;
               uint8_t* chunk_data;
               uint64_t available;
               uint64_t to_copy;

               /* Make sure group_index is still within this entry's groups */
               if (group_index >= entry->group_index + entry->num_groups)
                  break;

               chunk_data = rvz_get_chunk(handle, group_index, &chunk_data_size);
               if (!chunk_data)
                  break;

               /* Copy data from this chunk */
               available = chunk_data_size - offset_in_chunk;
               to_copy   = size - total_read;
               if (to_copy > available)
                  to_copy = available;

               /* Don't read past entry boundary */
               if (handle->position + (int64_t)to_copy > entry_end)
                  to_copy = (uint64_t)(entry_end - handle->position);

               RARCH_LOG("[RVZ_READ]   Copying %llu bytes from group %u offset %llu (chunk_size=%llu)\n",
                         (unsigned long long)to_copy, group_index,
                         (unsigned long long)offset_in_chunk, (unsigned long long)chunk_data_size);

               memcpy(output + total_read, chunk_data + offset_in_chunk, (size_t)to_copy);

               total_read        += (size_t)to_copy;
               handle->position  += (int64_t)to_copy;
               offset_in_chunk    = 0;  /* Subsequent chunks start at offset 0 */
               group_index++;
            }
         }
      }
   }

   /* Fill any remaining bytes with zeros (reading past all entries) */
   if (total_read < size)
   {
      size_t remaining = size - total_read;
      memset(output + total_read, 0, remaining);
      total_read       += remaining;
      handle->position += (int64_t)remaining;
   }

   return total_read;
}

void rcheevos_rvz_close(void* file_handle)
{
   rcheevos_rvz_file_t* handle = (rcheevos_rvz_file_t*)file_handle;
   uint32_t i;

   if (!handle)
      return;

   /* Free cache */
   for (i = 0; i < RVZ_CHUNK_CACHE_SIZE; i++)
   {
      if (handle->cache[i].data)
      {
         free(handle->cache[i].data);
         handle->cache[i].data = NULL;
      }
   }

   /* Free data structures */
   if (handle->raw_data_entries)
      free(handle->raw_data_entries);

   if (handle->group_entries)
      free(handle->group_entries);

   /* Close file */
   if (handle->file)
      filestream_close(handle->file);

   free(handle);
}

uint32_t rcheevos_rvz_get_console_id(const char* path)
{
   void* rvz = NULL;
   uint8_t magic[4];
   uint32_t console_id = RC_CONSOLE_UNKNOWN;

   /* Open the RVZ file */
   rvz = rcheevos_rvz_open(path);
   if (!rvz)
      return RC_CONSOLE_UNKNOWN;

   /* Check for Wii magic word at offset 0x18 */
   rcheevos_rvz_seek(rvz, 0x18, SEEK_SET);
   if (rcheevos_rvz_read(rvz, magic, 4) == 4)
   {
      if (magic[0] == 0x5D && magic[1] == 0x1C &&
          magic[2] == 0x9E && magic[3] == 0xA3)
      {
         console_id = RC_CONSOLE_WII;
         goto cleanup;
      }
   }

   /* Check for GameCube magic word at offset 0x1C */
   rcheevos_rvz_seek(rvz, 0x1C, SEEK_SET);
   if (rcheevos_rvz_read(rvz, magic, 4) == 4)
   {
      if (magic[0] == 0xC2 && magic[1] == 0x33 &&
          magic[2] == 0x9F && magic[3] == 0x3D)
      {
         console_id = RC_CONSOLE_GAMECUBE;
      }
   }

cleanup:
   rcheevos_rvz_close(rvz);
   return console_id;
}
