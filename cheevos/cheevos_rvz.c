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
#include "cheevos_locals.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <lrc_hash.h>
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
#define RVZ_CHUNK_CACHE_SIZE 8  /* Cache up to 8 decompressed chunks (~1MB with 128KB chunks) */

/* Disc format constants */
#define RVZ_DISC_HEADER_SIZE     128      /* Disc header stored in header_2 */
#define RVZ_SECTOR_ALIGNMENT     0x8000   /* 32KB sector boundary alignment */
#define RVZ_WII_SECTOR_SIZE      0x8000   /* Wii sector size (32KB, same as alignment) */
#define RVZ_WII_SECTOR_HASH_SIZE 0x0400   /* Hash header size per sector (1KB) */
#define RVZ_WII_SECTOR_DATA_SIZE 0x7C00   /* Data size per sector (31KB, without hashes) */
#define RVZ_MAX_BLOCK_SIZE       0x1000000 /* 16MB maximum block size */
#define RVZ_SHA1_SIZE            20       /* SHA-1 hash size in bytes */
#define RVZ_DATA_OFFSET_MULT     4        /* data_offset field is divided by 4 */
#define RVZ_PARTITION_KEY_SIZE   16       /* AES-128 key size */

#define RVZ_WII_BLOCKS_PER_GROUP    0x40      /* 64 blocks per group */
#define RVZ_WII_BLOCK_HEADER_SIZE   0x0400    /* 1KB hash header per block */
#define RVZ_WII_BLOCK_DATA_SIZE     0x7C00    /* 31KB data per block */
#define RVZ_WII_BLOCK_TOTAL_SIZE    0x8000    /* 32KB total per block (header + data) */

#define RVZ_WII_GROUP_HEADER_SIZE   0x10000   /* 64KB of headers per group (64 × 0x400) */
#define RVZ_WII_GROUP_DATA_SIZE     0x1F0000  /* ~2MB data per group (64 × 0x7C00) */
#define RVZ_WII_GROUP_TOTAL_SIZE    0x200000  /* 2MB total per group (64 × 0x8000) */

#define RVZ_WII_PARTITION_HEADER_SIZE 0x20000 /* 128KB partition header (data[0] only) */

/* Wii hash tree constants (for encryption/hash generation) */
#define RVZ_WII_H0_CHUNK_SIZE       0x0400    /* Each H0 hash covers 0x400 bytes of data */
#define RVZ_WII_H0_HASHES_PER_BLOCK 31        /* 31 H0 hashes per block (31 × 0x400 = 0x7C00) */

/* Special return value for invalid transformations (reading headers) */
#define RVZ_INVALID_OFFSET          UINT64_MAX

/* Console detection */
#define RVZ_OFFSET_WII_MAGIC     0x18     /* Offset of Wii magic word */
#define RVZ_OFFSET_GC_MAGIC      0x1C     /* Offset of GameCube magic word */

/* RVZPack and data_size flags */
#define RVZ_COMPRESSED_FLAG      0x80000000 /* Bit 31 of data_size indicates compression */
#define RVZ_SIZE_MASK            0x7FFFFFFF /* Mask for actual size without flag bit */
#define RVZ_PACK_JUNK_FLAG       0x80000000 /* Bit 31 of RVZPack block indicates junk */

/* Header structures based on docs/WiaAndRvz.md from Dolphin */
#pragma pack(push, 1)

typedef struct
{
   uint32_t magic;
   uint32_t version;
   uint32_t version_compatible;
   uint32_t header_2_size;
   uint8_t  header_2_hash[RVZ_SHA1_SIZE];
   uint64_t iso_file_size;
   uint64_t wia_file_size;
   uint8_t  header_1_hash[RVZ_SHA1_SIZE];
} rvz_header_1_t;

typedef struct
{
   uint32_t disc_type;
   uint32_t compression_type;
   int32_t  compression_level;
   uint32_t chunk_size;
   uint8_t  disc_header[RVZ_DISC_HEADER_SIZE];
   uint32_t num_partition_entries;
   uint32_t partition_entry_size;
   uint64_t partition_entries_offset;
   uint8_t  partition_entries_hash[RVZ_SHA1_SIZE];
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
   uint32_t first_sector;      /* Starting sector (sector = 32KB) */
   uint32_t number_of_sectors; /* Number of sectors */
   uint32_t group_index;       /* First group entry index */
   uint32_t number_of_groups;  /* Number of group entries */
   uint32_t exception_lists;   /* Number of exception lists per chunk (computed) */
} rvz_partition_data_entry_t;

typedef struct
{
   uint8_t partition_key[16];  /* AES-128 key (not needed for reading) */
   rvz_partition_data_entry_t data_entries[2]; /* [0]=boot/FST, [1]=game data */
} rvz_partition_entry_t;

typedef struct
{
   uint32_t data_offset;  /* Divided by 4 */
   uint32_t data_size;    /* MSB indicates compression */
   uint32_t rvz_packed_size;
} rvz_group_entry_t;

#pragma pack(pop)

/* Wii partition table entry for ISO offset mapping */
typedef struct
{
   uint32_t iso_offset_shifted;  /* Partition offset >> 2 (ISO offset / 4) */
   uint32_t partition_type;      /* 0=Game, 1=Update, 2=Channel */
} wii_partition_table_entry_t;

/* Entry descriptor for sorted lookup */
typedef struct
{
   uint64_t end_offset;           /* End offset of this entry (for binary search) */
   uint64_t iso_offset;           /* ISO offset for partitions (where hash function expects to read) */
   uint32_t rvz_first_sector;     /* RVZ storage location (for translation) */
   uint16_t entry_index;          /* Index into raw_data_entries or partition_entries */
   uint8_t  partition_data_index; /* For partitions: which data[] (0 or 1) */
   bool     is_partition;         /* true=partition, false=raw data */
} rvz_entry_descriptor_t;

/* Decompressed chunk cache entry */
typedef struct
{
   uint32_t group_index;
   uint64_t decompressed_size;
   uint8_t* data;
   bool     valid;
} rvz_chunk_cache_entry_t;

/* Wii encryption cache entry (for re-encrypting 64-block groups during hashing) */
typedef struct
{
   uint64_t group_offset;      /* Partition offset of this 2MB group (offset / 0x200000) */
   uint16_t entry_index;       /* Which partition entry this belongs to */
   uint8_t* encrypted_data;    /* 64 sectors * 0x8000 = 0x200000 bytes */
   bool     valid;
} wii_encryption_cache_entry_t;

/* Lagged Fibonacci Generator for junk data */
#define LFG_K 521
#define LFG_J 32
#define LFG_SEED_SIZE 17
#define LFG_SEED_SIZE_BYTES      68       /* 17 u32s = 68 bytes */
#define LFG_BUFFER_SIZE_BYTES    (LFG_K * 4) /* Buffer size in bytes */

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

/* Partition offset calculation results
 * Used by rvz_calculate_partition_offsets() to return all computed offsets
 * and flags needed for reading partition data.
 */
typedef struct
{
   uint64_t offset_in_partition;      /* ISO space - position within partition (includes virtual 0x400 headers) */
   uint64_t offset_in_entry;          /* Decrypted space - position within current data entry (no headers) */
   uint64_t file_offset;              /* File offset within RVZ chunk (decrypted space) */
   uint32_t group_index;              /* RVZ group number to read from */
   uint64_t offset_in_block;          /* Offset within the 0x7C00-byte decrypted block */
   uint64_t block_index;              /* Block number within the entry */
   bool     needs_encryption;         /* True if data needs re-encryption for hashing */
} rvz_partition_offsets_t;

/* Partition offset mapping for Wii discs */
typedef struct
{
   uint64_t partition_offset;  /* Base offset of partition in ISO (e.g., 0xF800000) */
   uint64_t data_offset;       /* Absolute offset of partition data (e.g., 0xF820000) */
   uint32_t partition_type;    /* Partition type from disc image (0=Game, 1=Update, 2=Channel) */
} rvz_partition_mapping_t;

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

   /* Partition entries (for Wii discs) */
   rvz_partition_entry_t* partition_entries;
   uint32_t num_partition_entries;

   /* Group entries (map chunks to compressed data) */
   rvz_group_entry_t* group_entries;
   uint32_t num_group_entries;

   /* Sorted entry descriptors for fast lookup */
   rvz_entry_descriptor_t* sorted_entries;
   uint32_t num_sorted_entries;

   /* Partition offset mappings for Wii partition hash correction */
   rvz_partition_mapping_t* partition_mappings;
   uint32_t num_partition_mappings;

   /* Chunk cache */
   rvz_chunk_cache_entry_t cache[RVZ_CHUNK_CACHE_SIZE];
   uint32_t cache_next;  /* LRU replacement index */

   /* Wii encryption cache (for re-encrypting 64-block groups during hashing) */
   wii_encryption_cache_entry_t encryption_cache[2];  /* 2 entries of 2MB each = 4MB max */
   uint32_t encryption_cache_next;  /* LRU replacement index */
   bool skip_reencryption;  /* Flag to prevent recursion when reading for encryption */

   /* Reusable buffer for decompression (avoids malloc/free per chunk) */
   uint8_t* temp_buffer;
   uint32_t temp_buffer_size;
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
   memcpy(header->header_2_hash, buffer + 0x10, RVZ_SHA1_SIZE);
   header->iso_file_size         = rvz_read_be64(buffer + 0x24);
   header->wia_file_size         = rvz_read_be64(buffer + 0x2C);
   memcpy(header->header_1_hash, buffer + 0x34, RVZ_SHA1_SIZE);

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
   memcpy(header->disc_header, buffer + 0x10, RVZ_DISC_HEADER_SIZE);
   header->num_partition_entries     = rvz_read_be32(buffer + 0x90);
   header->partition_entry_size      = rvz_read_be32(buffer + 0x94);
   header->partition_entries_offset  = rvz_read_be64(buffer + 0x98);
   memcpy(header->partition_entries_hash, buffer + 0xA0, RVZ_SHA1_SIZE);
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
         CHEEVOS_ERR("[RVZ] Failed to decompress metadata: %s\n", ZSTD_getErrorName(result));
         free(decompressed_data);
         return NULL;
      }

      return decompressed_data;
   }
#endif

   CHEEVOS_ERR("[RVZ] Unsupported compression type for metadata: %u\n", compression_type);
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
      CHEEVOS_ERR("[RVZ] Failed to decompress raw data entries\n");
      return false;
   }

   for (i = 0; i < rvz->header_2.num_raw_data_entries; i++)
   {
      uint8_t* entry = buffer + (i * entry_size);
      rvz->raw_data_entries[i].data_offset  = rvz_read_be64(entry + 0x00);
      rvz->raw_data_entries[i].data_size    = rvz_read_be64(entry + 0x08);
      rvz->raw_data_entries[i].group_index  = rvz_read_be32(entry + 0x10);
      rvz->raw_data_entries[i].num_groups   = rvz_read_be32(entry + 0x14);
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
      CHEEVOS_ERR("[RVZ] Failed to decompress group entries\n");
      return false;
   }

   for (i = 0; i < rvz->header_2.num_group_entries; i++)
   {
      uint8_t* entry = buffer + (i * entry_size);
      rvz->group_entries[i].data_offset      = rvz_read_be32(entry + 0x00);
      rvz->group_entries[i].data_size        = rvz_read_be32(entry + 0x04);
      rvz->group_entries[i].rvz_packed_size  = rvz_read_be32(entry + 0x08);
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

   /* Bit shift and byteswap for LFG output (unconditional swap required by algorithm) */
   for (i = 0; i < LFG_K; i++)
   {
      x = lfg->buffer[i];
      x = (x & 0xFF00FFFF) | ((x >> 2) & 0x00FF0000);
      /* Unconditional byte swap */
      lfg->buffer[i] = SWAP32(x);
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
   while (lfg->position_bytes >= LFG_BUFFER_SIZE_BYTES)
   {
      lfg_forward_one(lfg);
      lfg->position_bytes -= LFG_BUFFER_SIZE_BYTES;
   }
}

static void lfg_get_bytes(lfg_state_t* lfg, uint32_t count, uint8_t* out)
{
   uint8_t* buffer_bytes = (uint8_t*)lfg->buffer;

   while (count > 0)
   {
      uint32_t length = count;
      if (length > LFG_BUFFER_SIZE_BYTES - lfg->position_bytes)
         length = LFG_BUFFER_SIZE_BYTES - lfg->position_bytes;

      memcpy(out, buffer_bytes + lfg->position_bytes, length);

      lfg->position_bytes += length;
      count -= length;
      out += length;

      if (lfg->position_bytes == LFG_BUFFER_SIZE_BYTES)
      {
         lfg_forward_one(lfg);
         lfg->position_bytes = 0;
      }
   }
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
            return true; /* Partial read is OK */

         /* Read size field (big-endian) */
         size_field = rvz_read_be32(pack->intermediate_data + pack->intermediate_pos);
         pack->intermediate_pos += 4;

         /* Check junk flag (bit 31) */
         pack->current_block_is_junk = (size_field & RVZ_PACK_JUNK_FLAG) != 0;
         pack->current_block_size = size_field & RVZ_SIZE_MASK;
         pack->current_block_remaining = pack->current_block_size;

         /* Zero size means: read next size field (continue loop) */
         if (pack->current_block_size == 0)
            continue;
         if (pack->current_block_size > RVZ_MAX_BLOCK_SIZE)
         {
            CHEEVOS_ERR("[RVZ] RVZPack: Block size too large: 0x%08X\n", pack->current_block_size);
            return false;
         }

         /* Handle junk blocks - read LFG seed and initialize */
         if (pack->current_block_is_junk)
         {
            /* Junk blocks have 17 u32s (68 bytes) of LFG seed data */
            if (pack->intermediate_pos + LFG_SEED_SIZE_BYTES > pack->intermediate_size)
            {
               CHEEVOS_ERR("[RVZ] RVZPack: Not enough data for LFG seed\n");
               return false;
            }

            lfg_set_seed(&pack->lfg, pack->intermediate_data + pack->intermediate_pos);
            pack->intermediate_pos += LFG_SEED_SIZE_BYTES;

            /* Forward LFG by data_offset % sector_alignment */
            lfg_forward(&pack->lfg, pack->data_offset % RVZ_SECTOR_ALIGNMENT);
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

/* Helper: Parse exception list from decompressed data
 * Returns the number of bytes consumed (including alignment padding)
 * Format: [Exception Count (2 bytes BE)] [Entries (22 bytes each)] [Optional Padding]
 * The total size is aligned to 4 bytes for the last exception list.
 */
static uint32_t rvz_parse_exception_list(const uint8_t* data, uint32_t data_size,
                                         uint32_t bytes_used, bool align)
{
   uint16_t exception_count;
   uint32_t exception_list_size;

   /* Check if we have enough data for count */
   if (data_size < 2)
      return 0;

   /* Read 2-byte exception count (BIG-endian) at offset 0 */
   exception_count = (data[0] << 8) | data[1];

   /* Calculate exception list size: count (2 bytes) + entries (22 bytes each) */
   exception_list_size = 2 + (exception_count * 22);  /* 22 = sizeof(HashExceptionEntry) */

   /* For last exception list (align=true), align total size to 4-byte boundary */
   if (align)
   {
      uint32_t total = bytes_used + exception_list_size;
      uint32_t aligned = ((total + 3) / 4) * 4;  /* Round up to multiple of 4 */
      exception_list_size = aligned - bytes_used;
   }

   return exception_list_size;
}

/* ========================================================================
 * Decompression Helper Functions
 * ======================================================================== */

/* Helper: Handle exception lists - parse and remove from buffer
 * This function parses N exception lists from the start of a buffer,
 * removes them via memmove, and returns the number of bytes consumed.
 *
 * Returns: number of bytes consumed by exception lists (0 if none)
 */
static uint32_t rvz_handle_exception_lists(
    uint8_t* buffer,
    uint32_t buffer_size,
    uint32_t num_exception_lists,
    uint32_t compression_type,
    bool is_compressed_group)
{
   uint32_t bytes_used_for_exceptions = 0;
   uint32_t i;
   bool compressed_exceptions;

   if (num_exception_lists == 0)
      return 0;

   /* Determine if exception lists are compressed
    * For compressed groups (Zstd/etc): exception lists are compressed if compression > Purge
    * For uncompressed groups: exception lists are always uncompressed */
   if (is_compressed_group)
      compressed_exceptions = (compression_type > RVZ_COMPRESSION_PURGE);
   else
      compressed_exceptions = false;

   /* Parse all exception lists */
   for (i = 0; i < num_exception_lists; i++)
   {
      bool is_last = (i == num_exception_lists - 1);
      bool align;
      uint32_t list_size;

      /* Only align if: uncompressed exceptions AND last in list */
      align = !compressed_exceptions && is_last;

      list_size = rvz_parse_exception_list(
         buffer + bytes_used_for_exceptions,
         buffer_size - bytes_used_for_exceptions,
         bytes_used_for_exceptions,
         align);

      if (list_size == 0)
      {
         CHEEVOS_ERR("[RVZ] Failed to parse exception list %u\n", i);
         return 0;
      }

      bytes_used_for_exceptions += list_size;
   }

   /* Remove exception lists from buffer if any were found */
   if (bytes_used_for_exceptions > 0)
   {
      uint32_t actual_data_size;

      /* Sanity check */
      if (bytes_used_for_exceptions > buffer_size)
      {
         CHEEVOS_ERR("[RVZ] ERROR: exception list size (%u) > buffer size (%u)\n",
                     bytes_used_for_exceptions, buffer_size);
         return 0;
      }

      actual_data_size = buffer_size - bytes_used_for_exceptions;

      /* Move data after exception lists to the beginning */
      memmove(buffer, buffer + bytes_used_for_exceptions, actual_data_size);
      memset(buffer + actual_data_size, 0, bytes_used_for_exceptions);
   }

   return bytes_used_for_exceptions;
}

/* Helper: Get the correct decompression size for a group */
static uint32_t rvz_get_group_decompression_size(rcheevos_rvz_file_t* rvz,
                                                 uint32_t group_index)
{
   uint32_t i, j;
   uint32_t chunk_size = rvz->header_2.chunk_size;

   /* Check raw data entries */
   for (i = 0; i < rvz->num_raw_data_entries; i++)
   {
      rvz_raw_data_entry_t* raw = &rvz->raw_data_entries[i];
      uint32_t entry_base_group = raw->group_index;
      uint32_t entry_num_groups = raw->num_groups;

      if (group_index >= entry_base_group &&
          group_index < entry_base_group + entry_num_groups)
      {
         /* Found the entry - calculate remaining data */
         uint32_t group_offset_in_entry = (group_index - entry_base_group) * chunk_size;
         uint64_t entry_data_size = raw->data_size;
         uint64_t remaining;

         /* Account for alignment: raw data offset is aligned down to sector boundary,
          * so we need to add back the skipped bytes to get the actual data size */
         uint64_t data_offset_unaligned = raw->data_offset;
         uint64_t sector_size = 0x8000; /* RVZ uses 32KB sectors */
         uint64_t skipped_bytes = data_offset_unaligned % sector_size;
         entry_data_size += skipped_bytes;

         /* Raw data size is as-is */
         if (group_offset_in_entry < entry_data_size)
         {
            remaining = entry_data_size - group_offset_in_entry;
            return (uint32_t)((remaining < chunk_size) ? remaining : chunk_size);
         }

         /* This shouldn't happen, but if it does, use default */
         return chunk_size;
      }
   }

   /* For partition data (Wii), groups contain blocks of decrypted data
    * The number of blocks per group depends on chunk_size
    * Each decrypted block is 0x7C00 bytes (no hash header) */
   for (i = 0; i < rvz->num_partition_entries; i++)
   {
      for (j = 0; j < 2; j++)
      {
         rvz_partition_data_entry_t* pdata = &rvz->partition_entries[i].data_entries[j];
         if (group_index >= pdata->group_index &&
             group_index < pdata->group_index + pdata->number_of_groups)
         {
            /* This is a partition data group */
            /* Calculate blocks per group based on chunk_size
             * chunk_size is in ISO space (with headers), convert to decrypted space */
            uint32_t adjusted_chunk_size = ((uint64_t)chunk_size * RVZ_WII_SECTOR_DATA_SIZE) / RVZ_WII_SECTOR_SIZE;
            uint32_t blocks_per_group = adjusted_chunk_size / RVZ_WII_SECTOR_DATA_SIZE;
            uint32_t group_offset_in_partition = group_index - pdata->group_index;
            uint32_t total_blocks = pdata->number_of_sectors;
            uint32_t remaining_blocks = total_blocks - (group_offset_in_partition * blocks_per_group);

            /* Check if this is the last group (may have fewer blocks) */
            if (remaining_blocks < blocks_per_group)
               return remaining_blocks * RVZ_WII_SECTOR_DATA_SIZE;
            else
               return blocks_per_group * RVZ_WII_SECTOR_DATA_SIZE;
         }
      }
   }

   /* Not partition data - use chunk_size */
   return chunk_size;
}

/* ========================================================================
 * Main Decompression Dispatcher
 * ======================================================================== */

static bool rvz_group_is_partition_data(rcheevos_rvz_file_t* rvz, uint32_t group_index)
{
   uint32_t i, j;

   /* Check if this group belongs to any partition data entry */
   for (i = 0; i < rvz->num_partition_entries; i++)
   {
      for (j = 0; j < 2; j++)
      {
         rvz_partition_data_entry_t* data = &rvz->partition_entries[i].data_entries[j];
         if (group_index >= data->group_index &&
             group_index < data->group_index + data->number_of_groups)
         {
            return true;
         }
      }
   }

   return false;
}

static uint64_t rvz_get_group_offset_in_data(rcheevos_rvz_file_t* rvz, uint32_t group_index)
{
   uint32_t i, j;
   uint32_t local_group_index;
   uint32_t chunk_size = rvz->header_2.chunk_size;

   /* Check raw data entries first */
   for (i = 0; i < rvz->num_raw_data_entries; i++)
   {
      rvz_raw_data_entry_t* raw = &rvz->raw_data_entries[i];
      if (group_index >= raw->group_index &&
          group_index < raw->group_index + raw->num_groups)
      {
         local_group_index = group_index - raw->group_index;
         return (uint64_t)local_group_index * chunk_size;
      }
   }

   /* Check partition data entries */
   for (i = 0; i < rvz->num_partition_entries; i++)
   {
      for (j = 0; j < 2; j++)
      {
         rvz_partition_data_entry_t* pdata = &rvz->partition_entries[i].data_entries[j];
         if (group_index >= pdata->group_index &&
             group_index < pdata->group_index + pdata->number_of_groups)
         {
            /* For partition groups, data_offset should be relative to partition start (for LFG forwarding)
             * Calculate the offset from the partition's first data sector (data[0]) */

            /* Wii partitions use adjusted chunk_size for group offsets (31KB vs 32KB) */
            uint32_t adjusted_chunk_size = (uint32_t)((uint64_t)chunk_size * RVZ_WII_SECTOR_DATA_SIZE / RVZ_WII_SECTOR_SIZE);
            local_group_index = group_index - pdata->group_index;

            return (uint64_t)local_group_index * adjusted_chunk_size;
         }
      }
   }

   /* Not found - shouldn't happen */
   CHEEVOS_ERR("[RVZ] Could not find data entry for group %u\n", group_index);
   return 0;
}

/* Uncompressed RVZPack (no Zstd)
 * Used for:
 * - GameCube data that is RVZPack-encoded but not compressed
 * - Wii partition data that is RVZPack-encoded but not compressed
 */
static bool rvz_decompress_rvzpack_only(rcheevos_rvz_file_t* rvz,
                                        uint32_t group_index,
                                        const uint8_t* compressed_data,
                                        uint32_t compressed_size,
                                        uint32_t decompressed_size,
                                        uint8_t** out_data,
                                        uint64_t* out_size)
{
   rvz_group_entry_t* group = &rvz->group_entries[group_index];
   uint8_t* decompressed_data;
   rvz_pack_state_t* pack_state;
   uint32_t bytes_written;

   decompressed_data = (uint8_t*)calloc(1, decompressed_size);
   if (!decompressed_data)
      return false;

   /* Allocate pack_state on heap to avoid stack overflow (structure is > 2KB) */
   pack_state = (rvz_pack_state_t*)calloc(1, sizeof(rvz_pack_state_t));
   if (!pack_state)
   {
      free(decompressed_data);
      return false;
   }

   pack_state->intermediate_data = (uint8_t*)compressed_data;
   pack_state->intermediate_size = compressed_size;
   pack_state->intermediate_pos = 0;
   pack_state->rvz_packed_size = group->rvz_packed_size;
   pack_state->data_offset = rvz_get_group_offset_in_data(rvz, group_index);

   if (!rvz_pack_decompress(pack_state, decompressed_data, decompressed_size, &bytes_written))
   {
      CHEEVOS_ERR("[RVZ] RVZPack decompression failed\n");
      free(pack_state);
      free(decompressed_data);
      return false;
   }

   free(pack_state);

   *out_data = decompressed_data;
   *out_size = decompressed_size;
   return true;
}

#ifdef HAVE_ZSTD
/* Two-stage decompression: Zstd → RVZPack
 * Used for:
 * - GameCube with RVZPack
 * - Wii partition data with RVZPack
 */
static bool rvz_decompress_zstd_rvzpack(rcheevos_rvz_file_t* rvz,
                                        uint32_t group_index,
                                        const uint8_t* compressed_data,
                                        uint32_t compressed_size,
                                        uint32_t decompressed_size,
                                        uint8_t** out_data,
                                        uint64_t* out_size)
{
   rvz_group_entry_t* group = &rvz->group_entries[group_index];
   uint8_t* zstd_output;
   uint32_t zstd_output_size;
   uint8_t* decompressed_data;
   rvz_pack_state_t* pack_state;
   uint32_t bytes_written;
   size_t result;

   /* Stage 1: Zstd decompress to intermediate buffer
    * Query Zstd for the actual decompressed size */
   {
      unsigned long long frame_size = ZSTD_getFrameContentSize(compressed_data, compressed_size);
      if (frame_size == ZSTD_CONTENTSIZE_ERROR || frame_size == ZSTD_CONTENTSIZE_UNKNOWN)
      {
         /* Fall back to rvz_packed_size if frame size unknown */
         zstd_output_size = group->rvz_packed_size;
      }
      else
      {
         zstd_output_size = (uint32_t)frame_size;
      }
   }

   zstd_output = (uint8_t*)malloc(zstd_output_size);
   if (!zstd_output)
      return false;

   result = ZSTD_decompress(zstd_output, zstd_output_size,
                            compressed_data, compressed_size);

   if (ZSTD_isError(result))
   {
      CHEEVOS_ERR("[RVZ] Zstd decompression (stage 1) failed: %s\n", ZSTD_getErrorName(result));
      free(zstd_output);
      return false;
   }

   /* Update size to actual decompressed size */
   zstd_output_size = (uint32_t)result;

   /* For partition data with two-stage decompression, exception lists appear
    * in the Zstd output and must be removed BEFORE RVZPack processing. */
   if (rvz_group_is_partition_data(rvz, group_index))
   {
      uint32_t pi, pj;
      rvz_partition_data_entry_t* pdata = NULL;
      uint32_t bytes_used_for_exceptions;

      /* Find which partition data entry this group belongs to */
      for (pi = 0; pi < rvz->num_partition_entries; pi++)
      {
         for (pj = 0; pj < 2; pj++)
         {
            rvz_partition_data_entry_t* candidate = &rvz->partition_entries[pi].data_entries[pj];
            if (group_index >= candidate->group_index &&
                group_index < candidate->group_index + candidate->number_of_groups)
            {
               pdata = candidate;
               goto found_partition_twostage;
            }
         }
      }
found_partition_twostage:

      if (!pdata)
      {
         CHEEVOS_ERR("[RVZ] Could not find partition data entry for group %u\n", group_index);
         free(zstd_output);
         return false;
      }

      /* Handle exception lists from Zstd output (compressed group) */
      bytes_used_for_exceptions = rvz_handle_exception_lists(
         zstd_output, zstd_output_size,
         pdata->exception_lists,
         rvz->header_2.compression_type,
         true);  /* is_compressed_group = true */

      if (bytes_used_for_exceptions == 0 && pdata->exception_lists > 0)
      {
         CHEEVOS_ERR("[RVZ] Failed to handle exception lists for group %u\n", group_index);
         free(zstd_output);
         return false;
      }

      /* Update zstd_output_size to reflect data after exception removal */
      zstd_output_size -= bytes_used_for_exceptions;
   }

   /* Stage 2: RVZPack decompress to final buffer */
   decompressed_data = (uint8_t*)calloc(1, decompressed_size);
   if (!decompressed_data)
   {
      free(zstd_output);
      return false;
   }

   /* Allocate pack_state on heap to avoid stack overflow (structure is > 2KB) */
   pack_state = (rvz_pack_state_t*)calloc(1, sizeof(rvz_pack_state_t));
   if (!pack_state)
   {
      free(zstd_output);
      free(decompressed_data);
      return false;
   }

   pack_state->intermediate_data = zstd_output;
   pack_state->intermediate_size = zstd_output_size;  /* Use actual Zstd output size */
   pack_state->intermediate_pos = 0;
   pack_state->rvz_packed_size = group->rvz_packed_size;
   pack_state->data_offset = rvz_get_group_offset_in_data(rvz, group_index);

   /* RVZPack should output decompressed_size bytes (full chunk or actual group size)
    * For GameCube: decompressed_size = chunk_size (131072)
    * For Wii partition data: decompressed_size may be smaller due to exception lists */
   if (!rvz_pack_decompress(pack_state, decompressed_data, decompressed_size, &bytes_written))
   {
      CHEEVOS_ERR("[RVZ] RVZPack decompression (stage 2) failed\n");
      free(pack_state);
      free(zstd_output);
      free(decompressed_data);
      return false;
   }

   /* Clean up intermediate buffer */
   free(pack_state);
   free(zstd_output);

   /* Return final buffer */
   *out_data = decompressed_data;
   *out_size = decompressed_size;
   return true;
}

/* Single-stage Zstd decompression (no RVZPack)
 * Used for:
 * - GameCube raw data
 * - Wii partition data without RVZPack
 */
static bool rvz_decompress_zstd_only(rcheevos_rvz_file_t* rvz,
                                     uint32_t group_index,
                                     const uint8_t* compressed_data,
                                     uint32_t compressed_size,
                                     uint32_t decompressed_size,
                                     uint8_t** out_data,
                                     uint64_t* out_size)
{
   uint8_t* decompressed_data;
   uint32_t buffer_size;
   size_t result;

   /* For partition data, the compressed data includes exception lists which can make
    * the decompressed size larger than chunk_size. Query ZSTD for actual size.
    * We'll allocate a larger buffer for decompression, then shrink after removing exceptions. */
   buffer_size = decompressed_size;  /* Default to chunk_size */

   if (rvz_group_is_partition_data(rvz, group_index))
   {
      unsigned long long frame_size = ZSTD_getFrameContentSize(compressed_data, compressed_size);
      if (frame_size != ZSTD_CONTENTSIZE_ERROR && frame_size != ZSTD_CONTENTSIZE_UNKNOWN)
      {
         buffer_size = (uint32_t)frame_size;
      }
      else
      {
         /* Fall back to 2x chunk_size as safety margin */
         buffer_size = rvz->header_2.chunk_size * 2;
      }
   }

   decompressed_data = (uint8_t*)calloc(1, buffer_size);
   if (!decompressed_data)
      return false;

   /* Use buffer_size (full buffer) instead of actual_decompressed_size
    * to avoid "buffer too small" errors when frame size > rvz_packed_size */
   result = ZSTD_decompress(decompressed_data, buffer_size,
                            compressed_data, compressed_size);

   if (ZSTD_isError(result))
   {
      CHEEVOS_ERR("[RVZ] Zstd decompression failed: %s\n", ZSTD_getErrorName(result));
      free(decompressed_data);
      return false;
   }

   /* For partition data groups, skip exception lists at the start
    * Exception lists exist regardless of whether the group is compressed */
   if (rvz_group_is_partition_data(rvz, group_index))
   {
      uint32_t pi, pj;
      rvz_partition_data_entry_t* pdata = NULL;
      uint32_t bytes_used_for_exceptions;

      /* Find which partition data entry this group belongs to */
      for (pi = 0; pi < rvz->num_partition_entries; pi++)
      {
         for (pj = 0; pj < 2; pj++)
         {
            rvz_partition_data_entry_t* candidate = &rvz->partition_entries[pi].data_entries[pj];
            if (group_index >= candidate->group_index &&
                group_index < candidate->group_index + candidate->number_of_groups)
            {
               pdata = candidate;
               goto found_partition;
            }
         }
      }
found_partition:

      if (!pdata)
      {
         CHEEVOS_ERR("[RVZ] Could not find partition data entry for group %u\n", group_index);
         free(decompressed_data);
         return false;
      }

      /* Handle exception lists from decompressed data (compressed group) */
      bytes_used_for_exceptions = rvz_handle_exception_lists(
         decompressed_data, (uint32_t)result,
         pdata->exception_lists,
         rvz->header_2.compression_type,
         true);  /* is_compressed_group = true */

      if (bytes_used_for_exceptions == 0 && pdata->exception_lists > 0)
      {
         CHEEVOS_ERR("[RVZ] Failed to handle exception lists for group %u\n", group_index);
         free(decompressed_data);
         return false;
      }

      if (bytes_used_for_exceptions > 0)
      {
         /* Update decompressed_size to reflect actual size after exception removal */
         decompressed_size = (uint32_t)result - bytes_used_for_exceptions;

         /* If we allocated a larger buffer, reallocate to chunk_size now */
         if (buffer_size > rvz->header_2.chunk_size)
         {
            uint8_t* final_buffer = (uint8_t*)calloc(1, decompressed_size);
            if (!final_buffer)
            {
               free(decompressed_data);
               return false;
            }
            memcpy(final_buffer, decompressed_data, decompressed_size);
            free(decompressed_data);
            decompressed_data = final_buffer;
         }
      }
   }

   *out_data = decompressed_data;
   *out_size = decompressed_size;
   return true;
}
#endif

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

   if (group_index >= rvz->num_group_entries)
   {
      CHEEVOS_ERR("[RVZ] Invalid group index: %u\n", group_index);
      return false;
   }

   group = &rvz->group_entries[group_index];

   /* Check if this chunk is compressed */
   is_compressed = (group->data_size & RVZ_COMPRESSED_FLAG) != 0;
   compressed_size = group->data_size & RVZ_SIZE_MASK;

   /* Special case: all zeros */
   if (compressed_size == 0)
   {
      decompressed_size  = rvz_get_group_decompression_size(rvz, group_index);
      decompressed_data  = (uint8_t*)calloc(1, decompressed_size);
      if (!decompressed_data)
         return false;

      *out_data = decompressed_data;
      *out_size = decompressed_size;
      return true;
   }

   compressed_offset = (uint64_t)group->data_offset * RVZ_DATA_OFFSET_MULT;
   /* Get decompression size for this group:
    * - Partition data: calculated based on chunk_size and blocks_per_group
    * - Raw data: chunk_size or less for last chunk
    * - Adjusts for last group if it has fewer blocks */
   decompressed_size = rvz_get_group_decompression_size(rvz, group_index);

   /* Read compressed data using reusable buffer */
   if (rvz->temp_buffer_size < compressed_size)
   {
      /* Need to grow the buffer */
      if (rvz->temp_buffer)
         free(rvz->temp_buffer);
      rvz->temp_buffer = (uint8_t*)malloc(compressed_size);
      if (!rvz->temp_buffer)
      {
         rvz->temp_buffer_size = 0;
         return false;
      }
      rvz->temp_buffer_size = compressed_size;
   }
   compressed_data = rvz->temp_buffer;

   if (filestream_seek(rvz->file, compressed_offset, SEEK_SET) != 0)
      return false;

   if (filestream_read(rvz->file, compressed_data, compressed_size) != compressed_size)
      return false;

   /*
    * Route to appropriate decompression path based on compression and RVZPack flags.
    *
    * There are 4 possible decompression paths:
    * 1. Compressed + RVZPack: Zstd -> RVZPack (two-stage)
    * 2. Compressed only: Zstd (single-stage)
    * 3. Uncompressed + RVZPack: RVZPack only
    * 4. Uncompressed + No RVZPack: Raw copy (+ exception lists for partition data)
    */
   if (is_compressed)
   {
      uint32_t compression_type = rvz->header_2.compression_type;

      switch (compression_type)
      {
#ifdef HAVE_ZSTD
         case RVZ_COMPRESSION_ZSTD:
            if (group->rvz_packed_size != 0)
               return rvz_decompress_zstd_rvzpack(rvz, group_index, compressed_data,
                                                  compressed_size, decompressed_size,
                                                  out_data, out_size);
            else
               return rvz_decompress_zstd_only(rvz, group_index, compressed_data,
                                               compressed_size, decompressed_size,
                                               out_data, out_size);
#endif
         default:
            CHEEVOS_ERR("[RVZ] Unsupported compression type: %u\n", compression_type);
            return false;
      }
   }
   else
   {
      /* Uncompressed data */
      if (group->rvz_packed_size != 0)
         return rvz_decompress_rvzpack_only(rvz, group_index, compressed_data,
                                            compressed_size, decompressed_size,
                                            out_data, out_size);

      /* Raw uncompressed data - copy and handle exception lists if needed */
      decompressed_data = (uint8_t*)malloc(compressed_size > decompressed_size ? compressed_size : decompressed_size);
      if (!decompressed_data)
         return false;

      memcpy(decompressed_data, compressed_data, compressed_size);

      /* Zero-fill remaining bytes if needed */
      if (decompressed_size > compressed_size)
         memset(decompressed_data + compressed_size, 0, decompressed_size - compressed_size);

      /* Handle exception lists for uncompressed partition data */
      if (rvz_group_is_partition_data(rvz, group_index))
      {
         uint32_t pi, pj;
         rvz_partition_data_entry_t* pdata = NULL;
         uint32_t bytes_used_for_exceptions;

         /* Find partition data entry for this group */
         for (pi = 0; pi < rvz->num_partition_entries; pi++)
         {
            for (pj = 0; pj < 2; pj++)
            {
               rvz_partition_data_entry_t* candidate = &rvz->partition_entries[pi].data_entries[pj];
               if (group_index >= candidate->group_index &&
                   group_index < candidate->group_index + candidate->number_of_groups)
               {
                  pdata = candidate;
                  goto found_partition_uncompressed;
               }
            }
         }
found_partition_uncompressed:

         if (!pdata)
         {
            CHEEVOS_ERR("[RVZ] Could not find partition data entry for uncompressed group %u\n", group_index);
            free(decompressed_data);
            return false;
         }

         /* Parse and remove exception lists */
         bytes_used_for_exceptions = rvz_handle_exception_lists(
            decompressed_data, compressed_size,
            pdata->exception_lists,
            rvz->header_2.compression_type,
            false);  /* is_compressed_group = false */

         if (bytes_used_for_exceptions == 0 && pdata->exception_lists > 0)
         {
            CHEEVOS_ERR("[RVZ] Failed to handle exception lists for uncompressed group %u\n", group_index);
            free(decompressed_data);
            return false;
         }

         /* Adjust size after exception removal */
         if (bytes_used_for_exceptions > 0)
            decompressed_size = compressed_size - bytes_used_for_exceptions;
      }
   }

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

   /* Update LRU */
   rvz->cache_next = (rvz->cache_next + 1) % RVZ_CHUNK_CACHE_SIZE;

   *out_size = entry->decompressed_size;
   return entry->data;
}

/* ========================================================================
 * AES-128-CBC ENCRYPTION (from tiny-AES-c)
 * ========================================================================
 * Source: https://github.com/kokke/tiny-AES-c
 * Licensed under the Unlicense (public domain)
 */

/* AES constants for AES-128 */
#define AES_BLOCKLEN 16 /* Block length in bytes - AES is 128b block only */
#define AES_KEYLEN 16   /* Key length in bytes */
#define AES_keyExpSize 176
#define Nb 4
#define Nk 4
#define Nr 10

struct rvz_aes_ctx
{
  uint8_t RoundKey[AES_keyExpSize];
  uint8_t Iv[AES_BLOCKLEN];
};

typedef uint8_t aes_state_t[4][4];

/* S-box */
static const uint8_t rvz_sbox[256] = {
   0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
   0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
   0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
   0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
   0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
   0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
   0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
   0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
   0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
   0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
   0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
   0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
   0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
   0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
   0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
   0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };

/* The round constant word array, Rcon[i], contains the values given by
 * x to the power (i-1) being powers of x (x is denoted as {02}) in the field GF(2^8)
 */
static const uint8_t rvz_rcon[11] = {
  0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };

#define getRvz_SboxValue(num) (rvz_sbox[(num)])

/* This function produces Nb(Nr+1) round keys. The round keys are used in each round to decrypt the states. */
static void rvz_aes_key_expansion(uint8_t RoundKey[AES_keyExpSize], const uint8_t Key[AES_KEYLEN])
{
  unsigned i, j, k;
  uint8_t tempa[4]; /* Used for the column/row operations */

  /* The first round key is the key itself. */
  for (i = 0; i < Nk; ++i)
  {
    RoundKey[(i * 4) + 0] = Key[(i * 4) + 0];
    RoundKey[(i * 4) + 1] = Key[(i * 4) + 1];
    RoundKey[(i * 4) + 2] = Key[(i * 4) + 2];
    RoundKey[(i * 4) + 3] = Key[(i * 4) + 3];
  }

  /* All other round keys are found from the previous round keys. */
  for (i = Nk; i < Nb * (Nr + 1); ++i)
  {
    {
      k = (i - 1) * 4;
      tempa[0]=RoundKey[k + 0];
      tempa[1]=RoundKey[k + 1];
      tempa[2]=RoundKey[k + 2];
      tempa[3]=RoundKey[k + 3];

    }

    if (i % Nk == 0)
    {
      /* This function shifts the 4 bytes in a word to the left once. */
      /* [a0,a1,a2,a3] becomes [a1,a2,a3,a0] */

      /* Function RotWord() */
      {
        const uint8_t u8tmp = tempa[0];
        tempa[0] = tempa[1];
        tempa[1] = tempa[2];
        tempa[2] = tempa[3];
        tempa[3] = u8tmp;
      }

      /* SubWord() is a function that takes a four-byte input word and
       * applies the S-box to each of the four bytes to produce an output word.
       */

      /* Function Subword() */
      {
        tempa[0] = getRvz_SboxValue(tempa[0]);
        tempa[1] = getRvz_SboxValue(tempa[1]);
        tempa[2] = getRvz_SboxValue(tempa[2]);
        tempa[3] = getRvz_SboxValue(tempa[3]);
      }

      tempa[0] = tempa[0] ^ rvz_rcon[i/Nk];
    }

    j = i * 4; k=(i - Nk) * 4;
    RoundKey[j + 0] = RoundKey[k + 0] ^ tempa[0];
    RoundKey[j + 1] = RoundKey[k + 1] ^ tempa[1];
    RoundKey[j + 2] = RoundKey[k + 2] ^ tempa[2];
    RoundKey[j + 3] = RoundKey[k + 3] ^ tempa[3];
  }
}

static void rvz_aes_init_ctx_iv(struct rvz_aes_ctx* ctx, const uint8_t key[AES_KEYLEN], const uint8_t iv[AES_BLOCKLEN])
{
  rvz_aes_key_expansion(ctx->RoundKey, key);
  memcpy (ctx->Iv, iv, AES_BLOCKLEN);
}

static uint8_t rvz_aes_xtime(uint8_t x)
{
   return ((x<<1) ^ (((x>>7) & 1) * 0x1b));
}

static void rvz_aes_add_round_key(uint8_t round, aes_state_t* state, const uint8_t* RoundKey)
{
   uint8_t i,j;
   for (i = 0; i < 4; ++i)
   {
      for (j = 0; j < 4; ++j)
      {
         (*state)[i][j] ^= RoundKey[(round * Nb * 4) + (i * Nb) + j];
      }
   }
}

static void rvz_aes_sub_bytes(aes_state_t* state)
{
   uint8_t i, j;
   for (i = 0; i < 4; ++i)
   {
      for (j = 0; j < 4; ++j)
      {
         (*state)[j][i] = getRvz_SboxValue((*state)[j][i]);
      }
   }
}

static void rvz_aes_shift_rows(aes_state_t* state)
{
   uint8_t temp;

   temp           = (*state)[0][1];
   (*state)[0][1] = (*state)[1][1];
   (*state)[1][1] = (*state)[2][1];
   (*state)[2][1] = (*state)[3][1];
   (*state)[3][1] = temp;

   temp           = (*state)[0][2];
   (*state)[0][2] = (*state)[2][2];
   (*state)[2][2] = temp;

   temp           = (*state)[1][2];
   (*state)[1][2] = (*state)[3][2];
   (*state)[3][2] = temp;

   temp           = (*state)[0][3];
   (*state)[0][3] = (*state)[3][3];
   (*state)[3][3] = (*state)[2][3];
   (*state)[2][3] = (*state)[1][3];
   (*state)[1][3] = temp;
}

static void rvz_aes_shift_columns(aes_state_t* state)
{
   uint8_t i;
   uint8_t Tmp, Tm, t;
   for (i = 0; i < 4; ++i)
   {
      t   = (*state)[i][0];
      Tmp = (*state)[i][0] ^ (*state)[i][1] ^ (*state)[i][2] ^ (*state)[i][3] ;
      Tm  = (*state)[i][0] ^ (*state)[i][1] ; Tm = rvz_aes_xtime(Tm);  (*state)[i][0] ^= Tm ^ Tmp ;
      Tm  = (*state)[i][1] ^ (*state)[i][2] ; Tm = rvz_aes_xtime(Tm);  (*state)[i][1] ^= Tm ^ Tmp ;
      Tm  = (*state)[i][2] ^ (*state)[i][3] ; Tm = rvz_aes_xtime(Tm);  (*state)[i][2] ^= Tm ^ Tmp ;
      Tm  = (*state)[i][3] ^ t ;              Tm = rvz_aes_xtime(Tm);  (*state)[i][3] ^= Tm ^ Tmp ;
   }
}

static void rvz_aes_cipher(aes_state_t* state, const uint8_t* RoundKey)
{
   uint8_t round = 0;

   rvz_aes_add_round_key(0, state, RoundKey);

   for (round = 1; ; ++round)
   {
      rvz_aes_sub_bytes(state);
      rvz_aes_shift_rows(state);
      if (round == Nr) {
         break;
      }
      rvz_aes_shift_columns(state);
      rvz_aes_add_round_key(round, state, RoundKey);
   }
   rvz_aes_add_round_key(Nr, state, RoundKey);
}

static void rvz_aes_xor_with_iv(uint8_t* buf, const uint8_t* Iv)
{
   uint8_t i;
   for (i = 0; i < AES_BLOCKLEN; ++i)
   {
      buf[i] ^= Iv[i];
   }
}

static void AES_CBC_encrypt_buffer(struct rvz_aes_ctx *ctx, uint8_t* buf, size_t length)
{
   size_t i;
   uint8_t *Iv = ctx->Iv;
   for (i = 0; i < length; i += AES_BLOCKLEN)
   {
      rvz_aes_xor_with_iv(buf, Iv);
      rvz_aes_cipher((aes_state_t*)buf, ctx->RoundKey);
      Iv = buf;
      buf += AES_BLOCKLEN;
   }
   memcpy(ctx->Iv, Iv, AES_BLOCKLEN);
}

/* ========================================================================
 * WII DISC ENCRYPTION
 * ======================================================================== */

/* Helper: Re-encrypt Wii partition 64-block group for hashing
 * Takes 64 blocks of decrypted data (64 * 0x7C00 = 0x1F0000 bytes)
 * Produces 64 blocks of encrypted data (64 * 0x8000 = 0x200000 bytes)
 *
 * Wii encryption uses a 3-level hash tree shared across 64 blocks:
 * - H0[i] = SHA-1 of data chunk i (31 chunks of 0x400 bytes per block)
 * - H1[i] = SHA-1 of block i's entire H0 array (620 bytes), shared across 8-block subgroups
 * - H2[k] = SHA-1 of subgroup k's H1 array (160 bytes), shared across all 64 blocks
 */
static void wii_encrypt_group(const uint8_t* decrypted_blocks,
                              const uint8_t* partition_key,
                              uint8_t* encrypted_blocks)
{
   /* Store hash trees per-block:
    * - Each block has its own H0 array (31 hashes)
    * - Blocks in the same subgroup share the same H1 array (8 hashes)
    * - All blocks share the same H2 array (8 hashes)
    */
   typedef struct {
      uint8_t h0[RVZ_WII_H0_HASHES_PER_BLOCK * 20];  /* H0 hashes for this block's 31 chunks */
      uint8_t h1[8 * 20];   /* H1 hashes for this block's subgroup (shared) */
      uint8_t h2[8 * 20];   /* H2 hashes (shared by all 64 blocks) */
   } block_hashes_t;

   block_hashes_t block_hashes[64];
   struct rvz_aes_ctx aes;
   uint8_t iv[AES_BLOCKLEN];
   unsigned block, subgroup, i, h1_base;

   /* Step 1: Generate H0 hashes for all 64 blocks */
   for (block = 0; block < 64; block++)
   {
      const uint8_t* block_data = decrypted_blocks + (block * RVZ_WII_SECTOR_DATA_SIZE);

      for (i = 0; i < RVZ_WII_H0_HASHES_PER_BLOCK; i++)
         sha1_digest(block_data + (i * RVZ_WII_H0_CHUNK_SIZE), RVZ_WII_H0_CHUNK_SIZE, block_hashes[block].h0 + (i * 20));
   }

   /* Step 2: Generate H1 hashes for each block and store in subgroup base
    * block. Each block's H1 hash is computed from its H0 and stored in the
    * subgroup base block's H1 array at position [block % 8]
    */
   for (block = 0; block < 64; block++)
   {
      h1_base = (block / 8) * 8;  /* Subgroup base block (0, 8, 16, ...) */
      i = block % 8;               /* Index within subgroup (0-7) */

      /* Store this block's H1 hash in the subgroup base block */
      sha1_digest(block_hashes[block].h0, 31 * 20,
                  block_hashes[h1_base].h1 + (i * 20));
   }

   /* Step 2b: Copy H1 arrays from subgroup base to all blocks in subgroup
    * After computing all 8 H1 hashes for a subgroup, copy the complete H1 array
    * to all other blocks in that subgroup
    */
   for (subgroup = 0; subgroup < 8; subgroup++)
   {
      h1_base = subgroup * 8;
      for (i = 1; i < 8; i++)
      {
         memcpy(block_hashes[h1_base + i].h1, block_hashes[h1_base].h1, 8 * 20);
      }
   }

   /* Step 3: Generate H2 hashes and store in block 0
    * H2[k] = SHA-1 of subgroup k's H1 array (all blocks in subgroup have same H1)
    */
   for (subgroup = 0; subgroup < 8; subgroup++)
   {
      h1_base = subgroup * 8;
      sha1_digest(block_hashes[h1_base].h1, 8 * 20,
                  block_hashes[0].h2 + (subgroup * 20));
   }

   /* Step 3b: Copy H2 array from block 0 to all other blocks */
   for (block = 1; block < 64; block++)
   {
      memcpy(block_hashes[block].h2, block_hashes[0].h2, 8 * 20);
   }

   /* Step 4: Encrypt all 64 blocks */
   for (block = 0; block < 64; block++)
   {
      const uint8_t* block_data = decrypted_blocks + (block * RVZ_WII_SECTOR_DATA_SIZE);
      uint8_t* encrypted_block = encrypted_blocks + (block * RVZ_WII_SECTOR_SIZE);
      uint8_t hash_header[RVZ_WII_SECTOR_HASH_SIZE];

      /* Build hash header with this block's H0, H1 (shared in subgroup), and H2 (shared by all) */
      memset(hash_header, 0, RVZ_WII_SECTOR_HASH_SIZE);
      memcpy(hash_header, block_hashes[block].h0, RVZ_WII_H0_HASHES_PER_BLOCK * 20);     /* H0 at 0x000 */
      memcpy(hash_header + 0x280, block_hashes[block].h1, 8 * 20);  /* H1 at 0x280 */
      memcpy(hash_header + 0x340, block_hashes[block].h2, 8 * 20);  /* H2 at 0x340 */

      /* Encrypt hash header with IV=0 */
      memset(iv, 0, AES_BLOCKLEN);
      rvz_aes_init_ctx_iv(&aes, partition_key, iv);
      memcpy(encrypted_block, hash_header, RVZ_WII_SECTOR_HASH_SIZE);
      AES_CBC_encrypt_buffer(&aes, encrypted_block, RVZ_WII_SECTOR_HASH_SIZE);

      /* Extract IV from encrypted header[0x3D0] for data encryption */
      memcpy(iv, encrypted_block + 0x3D0, AES_BLOCKLEN);

      /* Encrypt data with extracted IV */
      rvz_aes_init_ctx_iv(&aes, partition_key, iv);
      memcpy(encrypted_block + RVZ_WII_SECTOR_HASH_SIZE, block_data, RVZ_WII_SECTOR_DATA_SIZE);
      AES_CBC_encrypt_buffer(&aes, encrypted_block + RVZ_WII_SECTOR_HASH_SIZE, RVZ_WII_SECTOR_DATA_SIZE);
   }
}

/* Helper: Calculate file offset for reading a decrypted block from a Wii partition
 *
 * Wii partitions can have multiple data entries (e.g., data[0] for boot/FST, data[1] for game).
 * Each data entry is stored separately in the RVZ file at its own ISO offset.
 * This function determines which data entry contains the requested block and returns
 * the file offset to read it from.
 *
 * @param handle        RVZ file handle
 * @param entry_index   Partition entry index
 * @param virtual_block_number  Block number within partition (0-based)
 * @return File offset to read block from, or RVZ_INVALID_OFFSET on error
 *
 * Example:
 *   data[0]: 64 sectors at ISO 0x70000, covers decrypted [0x0, 0x1F0000)
 *   data[1]: 5417 sectors at ISO 0x270000, covers decrypted [0x1F0000, ...)
 *
 *   Block 0  (decrypted offset 0x0)      -> data[0], file offset 0x70000
 *   Block 63 (decrypted offset 0x1EC00)  -> data[0], file offset 0x25EC00
 *   Block 64 (decrypted offset 0x1F0000) -> data[1], file offset 0x270000
 */
static uint64_t wii_get_block_file_offset(rcheevos_rvz_file_t* handle,
                                          uint16_t entry_index,
                                          uint64_t virtual_block_number)
{
   rvz_partition_entry_t* pentry = &handle->partition_entries[entry_index];
   uint64_t decrypted_offset = virtual_block_number * RVZ_WII_SECTOR_DATA_SIZE;
   uint64_t cumulative_decrypted_offset = 0;
   unsigned i, j;

   /* Iterate through data entries to find which one contains this block */
   for (i = 0; i < 2; i++)
   {
      rvz_partition_data_entry_t* data_entry = &pentry->data_entries[i];
      uint64_t entry_decrypted_size = (uint64_t)data_entry->number_of_sectors * RVZ_WII_SECTOR_DATA_SIZE;

      /* Check if this block falls within this data entry's decrypted range */
      if (decrypted_offset >= cumulative_decrypted_offset &&
          decrypted_offset < cumulative_decrypted_offset + entry_decrypted_size)
      {
         /* Found the right data entry.
          * Calculate offset relative to THIS data entry's start.
          * Then find THIS data entry's base offset in sorted_entries. */

         uint64_t offset_within_entry = decrypted_offset - cumulative_decrypted_offset;

         /* Find THIS data entry (data[i]) in sorted_entries */
         for (j = 0; j < handle->num_sorted_entries; j++)
         {
            if (handle->sorted_entries[j].is_partition &&
                handle->sorted_entries[j].entry_index == entry_index &&
                handle->sorted_entries[j].partition_data_index == i)  /* Use i, not hardcoded 0! */
            {
               uint64_t entry_base = handle->sorted_entries[j].iso_offset;
               uint64_t result_offset;

               /* COORDINATE SPACE FIX: Convert offset_within_entry from decrypted space to ISO space.
                * offset_within_entry is in decrypted space (0x7C00 blocks, no headers).
                * entry_base is in ISO space (0x8000 blocks, with headers).
                * We must convert offset_within_entry to ISO space before adding.
                * Apply the inverse transformation formula directly: */
               uint64_t block_num = offset_within_entry / RVZ_WII_BLOCK_DATA_SIZE;
               uint64_t offset_in_block = offset_within_entry % RVZ_WII_BLOCK_DATA_SIZE;
               uint64_t offset_within_entry_iso = block_num * RVZ_WII_BLOCK_TOTAL_SIZE +
                                                   RVZ_WII_BLOCK_HEADER_SIZE +
                                                   offset_in_block;

               /* For data[0], iso_offset points to partition header, need to add partition header size
                * For data[1+], iso_offset points directly to data start */
               if (i == 0)
               {
                  entry_base += RVZ_WII_PARTITION_HEADER_SIZE;  /* Skip partition header for data[0] */
               }

               result_offset = entry_base + offset_within_entry_iso;

               return result_offset;
            }
         }

         /* Shouldn't happen - sorted_entries should have data[i] */
         return RVZ_INVALID_OFFSET;
      }

      cumulative_decrypted_offset += entry_decrypted_size;
   }

   /* Block not found in any data entry */
   return RVZ_INVALID_OFFSET;
}

static size_t rvz_read_stateless(rcheevos_rvz_file_t* handle, uint64_t offset,
                                 void* buffer, size_t size);

/* Helper: Get encrypted group from 64-block group cache
 * Returns pointer to entire encrypted group (0x200000 bytes = 64 sectors)
 * partition_group_offset: absolute file offset aligned to group boundary (0x200000)
 * partition_data_base: absolute file offset where partition data starts (e.g., 0x70000)
 */
static uint8_t* wii_get_encrypted_sector(rcheevos_rvz_file_t* handle,
                                         uint64_t partition_group_offset,
                                         uint64_t partition_data_base,
                                         uint16_t entry_index,
                                         const uint8_t* partition_key)
{
   /* Convert absolute ISO offset to partition-relative offset (ISO space, with virtual hash headers)
    * partition_group_offset and partition_data_base are both in ISO space */
   uint64_t partition_relative_offset = partition_group_offset - partition_data_base;

   /* Calculate group index (based on ISO space groups)
    * Each group in ISO space is 64 blocks * 0x8000 bytes = RVZ_WII_GROUP_TOTAL_SIZE */
   uint64_t group_index = partition_relative_offset / RVZ_WII_GROUP_TOTAL_SIZE;

   /* Calculate group offset in decrypted space (for cache key)
    * In RVZ, decrypted groups are 64 blocks * 0x7C00 bytes = RVZ_WII_GROUP_DATA_SIZE */
   uint64_t group_offset = group_index * RVZ_WII_GROUP_DATA_SIZE;

   unsigned i;
   wii_encryption_cache_entry_t* entry;
   uint8_t* decrypted_blocks;
   uint64_t blocks_to_read, block;

   /* Check cache first (cache uses partition-relative group_offset) */
   for (i = 0; i < 2; i++)
   {
      if (handle->encryption_cache[i].valid &&
          handle->encryption_cache[i].group_offset == group_offset &&
          handle->encryption_cache[i].entry_index == entry_index)
      {
         /* Return pointer to the base of the encrypted group */
         return handle->encryption_cache[i].encrypted_data;
      }
   }

   /* Not in cache - need to encrypt this group */
   entry = &handle->encryption_cache[handle->encryption_cache_next];

   /* Allocate buffer if needed */
   if (!entry->encrypted_data)
   {
      entry->encrypted_data = (uint8_t*)malloc(RVZ_WII_GROUP_TOTAL_SIZE);  /* 64 blocks * 0x8000 */
      if (!entry->encrypted_data)
         return NULL;
   }

   /* Allocate temporary buffer for decrypted blocks */
   decrypted_blocks = (uint8_t*)malloc(RVZ_WII_GROUP_DATA_SIZE);  /* 64 blocks * 0x7C00 */
   if (!decrypted_blocks)
      return NULL;

   /* Read all 64 blocks of decrypted data from partition.
    * Use stateless read to avoid position corruption during recursive reads.
    */
   blocks_to_read = 64;
   handle->skip_reencryption = true;  /* Prevent recursion */

   for (block = 0; block < blocks_to_read; block++)
   {
      /* Calculate which block we're reading (within the entire partition)
       * Virtual block number = group_index * 64 + block (64 blocks per group) */
      uint64_t virtual_block_number = group_index * 64 + block;
      uint8_t* dest = decrypted_blocks + (block * RVZ_WII_SECTOR_DATA_SIZE);

      /* Calculate file offset for reading this decrypted block.
       * This accounts for multiple data entries - each data entry is stored at a different
       * ISO offset, so blocks are NOT contiguous in the file when crossing data entry boundaries.
       */

      uint64_t read_offset = wii_get_block_file_offset(handle, entry_index, virtual_block_number);

      if (read_offset == RVZ_INVALID_OFFSET)
      {
         /* Block doesn't exist in partition data (e.g., beyond data[0] in Twilight Princess).
          * Fill with zeros - this is correct for padding beyond actual partition data. */
         memset(dest, 0, RVZ_WII_SECTOR_DATA_SIZE);
      }
      else
      {
         /* STATELESS READ: No position manipulation!
          * This prevents position corruption during recursive re-encryption reads.
          */
         if (rvz_read_stateless(handle, read_offset, dest, RVZ_WII_SECTOR_DATA_SIZE) != RVZ_WII_SECTOR_DATA_SIZE)
         {
            handle->skip_reencryption = false;
            free(decrypted_blocks);
            return NULL;
         }
      }
   }
   handle->skip_reencryption = false;

   /* Encrypt the entire 64-block group */
   wii_encrypt_group(decrypted_blocks, partition_key, entry->encrypted_data);

   free(decrypted_blocks);

   /* Update cache entry */
   entry->group_offset = group_offset;
   entry->entry_index = entry_index;
   entry->valid = true;

   /* Update LRU */
   handle->encryption_cache_next = (handle->encryption_cache_next + 1) % 2;

   /* Return pointer to base of encrypted group */
   return entry->encrypted_data;
}

static int64_t wii_encrypt_sector_range(
   rcheevos_rvz_file_t* handle,
   const rvz_entry_descriptor_t* entry_desc,
   uint64_t to_copy,
   uint8_t* output,
   uint64_t output_offset,
   uint64_t iso_offset_in_partition)
{
   rvz_partition_entry_t* pentry = &handle->partition_entries[entry_desc->entry_index];
   const uint8_t* partition_key = pentry->partition_key;
   uint64_t partition_data_base = 0;
   uint64_t group_boundary;
   uint64_t offset_in_group;
   const uint8_t* encrypted_sector;
   uint32_t j;

   /* Find partition data[0] ISO offset, then add partition header size to skip to data start
    * We need to search sorted_entries to find data[0] for this partition */
   for (j = 0; j < handle->num_sorted_entries; j++)
   {
      if (handle->sorted_entries[j].is_partition &&
          handle->sorted_entries[j].entry_index == entry_desc->entry_index &&
          handle->sorted_entries[j].partition_data_index == 0)
      {
         partition_data_base = handle->sorted_entries[j].iso_offset + RVZ_WII_PARTITION_HEADER_SIZE;
         break;
      }
   }

   /*
    * The iso_offset_in_partition parameter is in ISO space (with virtual hash headers).
    * We need to:
    * 1. Determine which 64-block encrypted group this offset falls into
    * 2. Calculate the offset within that group (in ISO space, including headers)
    * 3. Find the group boundary in absolute ISO space
    *
    * Group structure:
    * - ISO space: 64 blocks × 0x8000 = RVZ_WII_GROUP_TOTAL_SIZE bytes (with 0x400 headers)
    * - Decrypted space: 64 blocks × 0x7C00 = RVZ_WII_GROUP_DATA_SIZE bytes (no headers)
    *
    * The transformation layer handles this coordinate mapping correctly.
    */

   /* Calculate group boundary and offset using ISO space arithmetic */
   {
      uint64_t group_index_for_offset;  /* Which group (0, 1, 2, ...) */


      /* Calculate which group this offset belongs to
       * In ISO space, groups are 0x200000 bytes (64 blocks × 0x8000) */
      group_index_for_offset = iso_offset_in_partition / RVZ_WII_GROUP_TOTAL_SIZE;  /* ISO space */

      /* Calculate the ISO file offset where this group starts
       * group_boundary is in absolute ISO space (file offset) */
      group_boundary = partition_data_base + (group_index_for_offset * RVZ_WII_GROUP_TOTAL_SIZE);  /* ISO space */

      /* Calculate offset within the group (in ISO space, includes 0x400 headers per block) */
      offset_in_group = iso_offset_in_partition % RVZ_WII_GROUP_TOTAL_SIZE;  /* ISO space */
   }

   encrypted_sector = wii_get_encrypted_sector(handle, group_boundary,
                                                partition_data_base,
                                                entry_desc->entry_index,
                                                partition_key);

   /* NOTE: wii_get_encrypted_sector preserves handle->position internally,
    * so we don't need to save/restore it here. */

   /* Note: Encryption may cause cache eviction, but chunk_data remains valid
    * as it was saved before the encryption call */

   if (!encrypted_sector)
   {
      CHEEVOS_ERR("[RVZ_READ] Failed to encrypt group at file offset 0x%llx\n",
                  (unsigned long long)group_boundary);
      return 0;
   }

   /* Copy from the encrypted group at the correct offset
    * The encrypted group is 64 sectors * 0x8000 = RVZ_WII_GROUP_TOTAL_SIZE bytes
    * Each sector has 0x400 hash + 0x7c00 data
    * Make sure we don't read past the end of the group */
   {
      uint64_t bytes_left_in_group = RVZ_WII_GROUP_TOTAL_SIZE - offset_in_group;
      uint64_t bytes_to_copy = to_copy;
      if (bytes_to_copy > bytes_left_in_group)
         bytes_to_copy = bytes_left_in_group;

      memcpy(output + output_offset, encrypted_sector + offset_in_group, (size_t)bytes_to_copy);

      return (int64_t)bytes_to_copy;
   }
}

static bool rvz_validate_partition_bounds(
   uint32_t group_index,
   const rvz_partition_data_entry_t* pdata)
{
   /* Check if group_index is within bounds
    * If beyond stored groups, caller should generate zeros for unstored regions. */
   if (group_index >= pdata->group_index + pdata->number_of_groups)
      return false;  /* Beyond stored data */

   return true;  /* Valid - proceed with read */
}

/**
 * Simple ISO to decrypted offset transformation (no validation)
 *
 * Transforms an offset from ISO space to decrypted space without validation.
 * Use this when you know the offset is in a data region and want the
 * transformation only.
 *
 * @param offset_in_partition_iso  Offset within partition in ISO space
 * @return Offset within partition in decrypted space
 *
 * Example:
 *   offset_in_partition_iso = 0x400 (first byte after block 0 header)
 *   → Returns: 0x0 (first byte in decrypted space)
 *
 *   offset_in_partition_iso = 0x8400 (first byte after block 1 header)
 *   → Returns: 0x7C00 (first byte of block 1 in decrypted space)
 */
static uint64_t wii_iso_to_decrypted_offset_simple(uint64_t offset_in_partition_iso)
{
   uint64_t block_index = offset_in_partition_iso / RVZ_WII_SECTOR_SIZE;
   uint64_t offset_in_block_iso = offset_in_partition_iso % RVZ_WII_SECTOR_SIZE;

   /* If offset points to header region, treat it as start of data */
   if (offset_in_block_iso < RVZ_WII_SECTOR_HASH_SIZE)
      return block_index * RVZ_WII_SECTOR_DATA_SIZE;

   /* Otherwise, apply standard transformation */
   return block_index * RVZ_WII_SECTOR_DATA_SIZE +
          (offset_in_block_iso - RVZ_WII_SECTOR_HASH_SIZE);
}

/* ============================================================================
 * STATELESS HELPER FUNCTIONS
 * ============================================================================
 * These are the internal stateless implementations that take offset as parameter.
 * They NEVER modify handle->position.
 */

/**
 * Stateless version of rvz_calculate_partition_offsets.
 * Takes offset as parameter instead of using handle->position.
 */
static int rvz_calculate_partition_offsets_stateless(
   rcheevos_rvz_file_t* handle,
   uint64_t offset,
   const rvz_entry_descriptor_t* entry_desc,
   rvz_partition_data_entry_t** pdata,
   uint64_t partition_data_base,
   uint32_t chunk_size,
   rvz_partition_offsets_t* offsets)
{
   uint64_t offset_from_iso_offset;
   uint32_t effective_chunk_size;
   uint64_t file_block;
   bool switched_to_data1 = false;

   /* Calculate partition_data_base: ISO offset where data[0]'s data starts
    * This is data[0].first_sector * RVZ_WII_SECTOR_SIZE (ISO space with encrypted headers)
    * Already calculated by caller and passed in */

   if (entry_desc->partition_data_index == 0)
   {
      /* data[0]: Use transformation layer to handle ISO → decrypted conversion */
      offset_from_iso_offset = offset - entry_desc->iso_offset;

      if (offset_from_iso_offset < RVZ_WII_PARTITION_HEADER_SIZE)
      {
         /* Reading partition header - caller should handle this separately */
         offsets->offset_in_partition = offset_from_iso_offset;
         return -1;
      }
      /* Reading data[0] data - calculate ISO offset within partition */
      offsets->offset_in_partition = offset_from_iso_offset - RVZ_WII_PARTITION_HEADER_SIZE;  /* ISO space */
   }
   else
   {
      /* data[1]: Calculate ISO offset within partition
       * Both data[0] and data[1] use the SAME partition_data_base reference point */
      offsets->offset_in_partition = offset - partition_data_base;  /* ISO space */
   }

   /* Check if offset_in_partition is past the end of the current data entry's decrypted size.
    * If so, we need to switch to data[1] */
   if (entry_desc->partition_data_index == 0)
   {
      uint64_t data0_decrypted_size = (uint64_t)(*pdata)->number_of_sectors * RVZ_WII_SECTOR_DATA_SIZE;
      uint64_t offset_in_partition_decrypted = wii_iso_to_decrypted_offset_simple(offsets->offset_in_partition);

      if (offset_in_partition_decrypted >= data0_decrypted_size)
      {
         uint64_t data0_size_iso;
         /* Switch to data[1] */
         *pdata = &handle->partition_entries[entry_desc->entry_index].data_entries[1];
         /* Recalculate offset_in_partition to be relative to data[1] start (in ISO space) */
         data0_size_iso = (uint64_t)handle->partition_entries[entry_desc->entry_index].data_entries[0].number_of_sectors * RVZ_WII_SECTOR_SIZE;
         offsets->offset_in_partition = offset - (partition_data_base + data0_size_iso);
         switched_to_data1 = true;
      }
   }

   /* COORDINATE SPACE FIX: Transform ISO space to decrypted space.
    * offset_in_partition is ALWAYS in ISO space (computed from ISO coordinates),
    * regardless of the skip_reencryption flag. The skip_reencryption flag only
    * controls whether we need to re-encrypt the OUTPUT, not the coordinate space
    * of the INPUT. ALWAYS convert ISO → decrypted. */
   offsets->offset_in_entry = wii_iso_to_decrypted_offset_simple(offsets->offset_in_partition);  /* Decrypted space */
   offsets->needs_encryption = !handle->skip_reencryption;

   /* For data[1] entries that didn't switch from data[0], subtract data[0]'s size
    * to make offset_in_entry relative to data[1] start */
   if (entry_desc->partition_data_index == 1 && !switched_to_data1)
   {
      uint64_t data0_size_decrypted = (uint64_t)handle->partition_entries[entry_desc->entry_index].data_entries[0].number_of_sectors * RVZ_WII_SECTOR_DATA_SIZE;
      offsets->offset_in_entry -= data0_size_decrypted;  /* Decrypted space */
   }

   /* Calculate block index and offset within block */
   offsets->block_index = offsets->offset_in_entry / RVZ_WII_SECTOR_DATA_SIZE;
   offsets->offset_in_block = offsets->offset_in_entry % RVZ_WII_SECTOR_DATA_SIZE;

   /* Calculate file_offset (position within RVZ chunk, in decrypted space) */
   offsets->file_offset = (offsets->block_index * RVZ_WII_SECTOR_DATA_SIZE) + offsets->offset_in_block;

   /* Calculate effective chunk size (decrypted group size) */
   effective_chunk_size = ((uint64_t)chunk_size * RVZ_WII_SECTOR_DATA_SIZE) / RVZ_WII_SECTOR_SIZE;

   /* Calculate group index */
   file_block = offsets->file_offset / effective_chunk_size;

   if (entry_desc->partition_data_index == 1)
   {
      /* Reading from data[1], use data[1]'s group_index directly */
      uint32_t data1_group_index_from_file = handle->partition_entries[entry_desc->entry_index].data_entries[1].group_index;
      offsets->group_index = data1_group_index_from_file + (uint32_t)file_block;
   }
   else
   {
      /* Reading from data[0], file_block is already relative to partition start */
      uint32_t data0_group_index = handle->partition_entries[entry_desc->entry_index].data_entries[0].group_index;
      offsets->group_index = data0_group_index + (uint32_t)file_block;
   }

   return 0;  /* Success */
}

/**
 * Stateless version of rvz_read_partition_data.
 * Takes offset as parameter instead of using handle->position.
 */
static size_t rvz_read_partition_data_stateless(
   rcheevos_rvz_file_t* handle,
   uint64_t offset,
   const rvz_partition_offsets_t* offsets,
   const rvz_entry_descriptor_t* entry_desc,
   uint64_t entry_end,
   uint32_t chunk_size,
   uint8_t* output,
   size_t total_read,
   size_t size)
{
   uint64_t offset_in_chunk;
   uint64_t chunk_data_size;
   uint32_t effective_chunk_size;
   uint8_t* chunk_data;
   uint64_t available;
   uint64_t to_copy;
   uint64_t bytes_left_in_block;

   /* Calculate effective chunk size (decrypted group size) */
   effective_chunk_size = ((uint64_t)chunk_size * RVZ_WII_SECTOR_DATA_SIZE) / RVZ_WII_SECTOR_SIZE;

   /* Get the decompressed chunk data */
   chunk_data = rvz_get_chunk(handle, offsets->group_index, &chunk_data_size);
   if (!chunk_data)
   {
      CHEEVOS_ERR("[RVZ_READ] ERROR: rvz_get_chunk returned NULL for group %u\n", offsets->group_index);
      return 0;
   }

   /* Calculate offset within chunk
    * Use effective_chunk_size (adjusted for partition data decrypted space) */
   offset_in_chunk = offsets->file_offset % effective_chunk_size;
   available = chunk_data_size - offset_in_chunk;
   to_copy = size - total_read;

   if (to_copy > available)
      to_copy = available;

   /* Don't read past current block's data region
    * COORDINATE SPACE FIX: offset_in_block is in decrypted space (0x7C00 blocks),
    * so we must use RVZ_WII_SECTOR_DATA_SIZE, not RVZ_WII_SECTOR_SIZE (0x8000). */
   bytes_left_in_block = RVZ_WII_SECTOR_DATA_SIZE - offsets->offset_in_block;
   if (to_copy > bytes_left_in_block)
      to_copy = bytes_left_in_block;

   /* Don't read past entry boundary - use offset parameter instead of handle->position */
   if (offset + to_copy > entry_end)
      to_copy = entry_end - offset;

   /* Prevent infinite loop: if no bytes to read, move past this entry */
   if (to_copy == 0)
      return 0;

   /* Check if we need to re-encrypt this data (Wii partition data for hashing).
    * We ALWAYS re-encrypt partition data when not skipping re-encryption,
    * regardless of alignment. The function handles partial sector reads by
    * calculating the offset within the encrypted group. Pass
    * offset_in_partition to correctly handle hash headers (which need to be
    * returned from the encrypted group, not as zeros). */
   if (!handle->skip_reencryption)
   {
      int64_t bytes_encrypted = wii_encrypt_sector_range(handle, entry_desc,
                                                          to_copy,
                                                          output, total_read,
                                                          offsets->offset_in_partition);
      if (bytes_encrypted <= 0)
         return 0;
      to_copy = (uint64_t)bytes_encrypted;
   }
   else
   {
      /* skip_reencryption flag set - return decrypted data as-is */
      memcpy(output + total_read, chunk_data + offset_in_chunk, (size_t)to_copy);
   }

   return (size_t)to_copy;
}

/* ============================================================================
 * STATELESS READ IMPLEMENTATION
 * ============================================================================
 * Internal stateless read function that takes offset as a parameter.
 * This prevents position state corruption during recursive reads.
 */

/**
 * Stateless disc header read (no position state modification).
 */
static size_t rvz_read_disc_header_stateless(
    rcheevos_rvz_file_t* handle,
    uint64_t offset,
    uint8_t* output,
    size_t size)
{
   /* Check if we're reading from disc header region */
   if (offset < RVZ_DISC_HEADER_SIZE)
   {
      uint64_t available = RVZ_DISC_HEADER_SIZE - offset;
      uint64_t to_copy = size;

      if (to_copy > available)
         to_copy = available;

      memcpy(output, handle->header_2.disc_header + offset, (size_t)to_copy);
      return (size_t)to_copy;
   }

   return 0;
}

/**
 * Stateless partition field patching (no position state modification).
 */
static size_t rvz_patch_partition_fields_stateless(
    rcheevos_rvz_file_t* handle,
    uint64_t offset,
    uint8_t* output,
    size_t size)
{
   uint32_t i;

   if (handle->num_partition_mappings == 0 || size == 0)
      return 0;

   for (i = 0; i < handle->num_partition_mappings; i++)
   {
      uint64_t partition_offset = handle->partition_mappings[i].partition_offset;
      uint64_t data_offset_field = partition_offset + 0x2B8;
      uint64_t data_size_field = partition_offset + 0x2BC;

      /* Check if we're reading the data offset field (4 bytes at partition+0x2B8) */
      if (offset >= data_offset_field && offset < data_offset_field + 4)
      {
         /* Return partition-relative offset. */
         uint64_t absolute_data_offset = handle->partition_mappings[i].data_offset;
         uint64_t relative_data_offset = absolute_data_offset - partition_offset;
         uint32_t shifted_offset = (uint32_t)(relative_data_offset >> 2);
         uint8_t offset_bytes[4];
         uint64_t offset_in_field;
         uint64_t to_copy;

         /* Convert to big-endian */
         offset_bytes[0] = (shifted_offset >> 24) & 0xFF;
         offset_bytes[1] = (shifted_offset >> 16) & 0xFF;
         offset_bytes[2] = (shifted_offset >> 8) & 0xFF;
         offset_bytes[3] = shifted_offset & 0xFF;

         offset_in_field = offset - data_offset_field;
         to_copy = 4 - offset_in_field;
         if (to_copy > size)
            to_copy = size;

         memcpy(output, offset_bytes + offset_in_field, (size_t)to_copy);
         return (size_t)to_copy;
      }
      else if (offset >= data_size_field && offset < data_size_field + 4)
      {
         /* Data size field - pass through without modification */
      }
   }

   return 0;
}

/**
 * Reads partition header using stateless approach.
 */
static int64_t rvz_read_partition_header_stateless(
   rcheevos_rvz_file_t* handle,
   uint64_t offset,
   uint64_t offset_in_partition,
   uint8_t* output,
   uint64_t size)
{
   uint32_t raw_idx;

   /* Find the raw entry that contains this partition header */
   for (raw_idx = 0; raw_idx < handle->num_raw_data_entries; raw_idx++)
   {
      rvz_raw_data_entry_t* raw_entry = &handle->raw_data_entries[raw_idx];
      uint64_t raw_start = raw_entry->data_offset - (raw_entry->data_offset % RVZ_SECTOR_ALIGNMENT);
      uint64_t raw_end = raw_entry->data_offset + raw_entry->data_size;

      if (offset >= raw_start && offset < raw_end)
      {
         /* Found it - read from this raw entry */
         uint64_t available_in_raw = raw_end - offset;
         uint64_t to_copy_header = size;
         if (to_copy_header > available_in_raw)
            to_copy_header = available_in_raw;
         if (to_copy_header > (RVZ_WII_PARTITION_HEADER_SIZE - offset_in_partition))
            to_copy_header = RVZ_WII_PARTITION_HEADER_SIZE - offset_in_partition;

         /* Use stateless read for raw entry */
         {
            uint32_t chunk_size_header = handle->header_2.chunk_size;
            uint64_t offset_in_raw_groups = offset - raw_start;
            uint32_t raw_group_index = (uint32_t)(offset_in_raw_groups / chunk_size_header) + raw_entry->group_index;
            uint64_t offset_in_raw_chunk = offset_in_raw_groups % chunk_size_header;
            uint64_t chunk_data_size_header;
            uint8_t* chunk_data_header;

            if (raw_group_index >= raw_entry->group_index + raw_entry->num_groups)
               return 0;

            chunk_data_header = rvz_get_chunk(handle, raw_group_index, &chunk_data_size_header);
            if (!chunk_data_header)
               return 0;

            if (offset_in_raw_chunk + to_copy_header > chunk_data_size_header)
               to_copy_header = chunk_data_size_header - offset_in_raw_chunk;

            memcpy(output, chunk_data_header + offset_in_raw_chunk, (size_t)to_copy_header);
            return (int64_t)to_copy_header;
         }
      }
   }

   /* Header not found in raw entries - return zeros */
   if (size > (RVZ_WII_PARTITION_HEADER_SIZE - offset_in_partition))
      size = RVZ_WII_PARTITION_HEADER_SIZE - offset_in_partition;

   memset(output, 0, (size_t)size);
   return (int64_t)size;
}

/**
 * Reads data from a partition entry - STATELESS version.
 * Takes offset as parameter instead of using handle->position.
 */
static size_t rvz_read_from_partition_entry_stateless(
   rcheevos_rvz_file_t* handle,
   const rvz_entry_descriptor_t* entry_desc,
   uint64_t entry_end,
   uint64_t offset,
   uint8_t* output,
   size_t bytes_requested)
{
   rvz_partition_data_entry_t* pdata =
      &handle->partition_entries[entry_desc->entry_index].data_entries[entry_desc->partition_data_index];
   uint64_t offset_in_block;
   uint32_t chunk_size;
   uint32_t group_index;
   size_t size = bytes_requested;

   /* Validate entry has ISO offset */
   if (entry_desc->iso_offset == 0)
   {
      CHEEVOS_ERR("[RVZ_READ] ERROR: Partition entry has iso_offset=0, this should not happen!\n");
      return 0;
   }

   /* Calculate offsets using helper functions */
   {
      uint64_t partition_data_base;
      rvz_partition_offsets_t offsets;
      int calc_result;

      partition_data_base = (uint64_t)handle->partition_entries[entry_desc->entry_index].data_entries[0].first_sector * RVZ_WII_SECTOR_SIZE;

      calc_result = rvz_calculate_partition_offsets_stateless(
         handle,
         offset,
         entry_desc,
         &pdata,
         partition_data_base,
         handle->header_2.chunk_size,
         &offsets);

      if (calc_result == -1)
      {
         /* Reading partition header */
         int64_t bytes_read = rvz_read_partition_header_stateless(handle, offset,
                                                                    offsets.offset_in_partition,
                                                                    output, size);
         if (bytes_read > 0)
            return (size_t)bytes_read;
         return 0;
      }

      offset_in_block = offsets.offset_in_block;
      group_index = offsets.group_index;
      chunk_size = handle->header_2.chunk_size;

      /* Bounds validation */
      if (!rvz_validate_partition_bounds(group_index, pdata))
      {
         /* Reading beyond stored RVZ data - generate zeros */
         uint64_t bytes_left_in_block_local = RVZ_WII_SECTOR_DATA_SIZE - offset_in_block;
         uint64_t to_copy_local = size;

         if (to_copy_local > bytes_left_in_block_local)
            to_copy_local = bytes_left_in_block_local;

         if (offset + to_copy_local > entry_end)
            to_copy_local = entry_end - offset;

         if (to_copy_local == 0)
            return 0;

         memset(output, 0, (size_t)to_copy_local);
         return (size_t)to_copy_local;
      }

      /* Read partition data */
      {
         size_t bytes_read = rvz_read_partition_data_stateless(handle, offset, &offsets, entry_desc,
                                                                 entry_end, chunk_size,
                                                                 output, 0, size);
         return bytes_read;
      }
   }
}

/**
 * Reads data from a raw (unencrypted) entry - STATELESS version.
 * Takes offset as parameter instead of using handle->position.
 */
static size_t rvz_read_from_raw_entry_stateless(
   rcheevos_rvz_file_t* handle,
   const rvz_entry_descriptor_t* entry_desc,
   uint64_t entry_start,
   uint64_t entry_end,
   uint64_t offset,
   uint8_t* output,
   size_t bytes_requested)
{
   rvz_raw_data_entry_t* raw_entry = &handle->raw_data_entries[entry_desc->entry_index];
   uint32_t chunk_size = handle->header_2.chunk_size;
   uint64_t offset_in_groups = offset - entry_start;
   uint32_t group_index = (uint32_t)(offset_in_groups / chunk_size) + raw_entry->group_index;
   uint64_t offset_in_chunk = offset_in_groups % chunk_size;
   uint64_t chunk_data_size, available, to_copy;
   uint8_t* chunk_data;

   /* Check if group_index is within bounds */
   if (group_index >= raw_entry->group_index + raw_entry->num_groups)
      return 0;

   chunk_data = rvz_get_chunk(handle, group_index, &chunk_data_size);
   if (!chunk_data)
      return 0;

   /* Calculate how much to read */
   available = chunk_data_size - offset_in_chunk;
   to_copy = bytes_requested;

   if (to_copy > available)
      to_copy = available;

   /* Don't read past entry boundary */
   if (offset + to_copy > entry_end)
      to_copy = entry_end - offset;

   if (to_copy == 0)
      return 0;

   memcpy(output, chunk_data + offset_in_chunk, (size_t)to_copy);
   return (size_t)to_copy;
}

static const rvz_entry_descriptor_t* rvz_find_entry_for_offset(
   rcheevos_rvz_file_t* handle,
   int64_t position,
   uint64_t* out_entry_start)
{
   uint32_t left, right, mid;
   uint32_t candidate_idx;
   rvz_entry_descriptor_t* candidate;
   uint64_t candidate_start;

   if (!handle || handle->num_sorted_entries == 0)
      return NULL;

   /* Binary search for entry containing current position */
   left = 0;
   right = handle->num_sorted_entries;

   while (left < right)
   {
      mid = left + (right - left) / 2;
      if (handle->sorted_entries[mid].end_offset <= (uint64_t)position)
         left = mid + 1;
      else
         right = mid;
   }

   /* Check if we found an entry that contains our position */
   if (left >= handle->num_sorted_entries)
      return NULL;

   candidate_idx = left;
   candidate = &handle->sorted_entries[candidate_idx];

   /* Calculate candidate's start offset to verify it contains our position */
   if (candidate->is_partition)
   {
      /* Partition entries use iso_offset as start */
      candidate_start = candidate->iso_offset;
   }
   else
   {
      /* Raw data entries: calculate start from offset */
      rvz_raw_data_entry_t* raw = &handle->raw_data_entries[candidate->entry_index];
      uint64_t skipped = raw->data_offset % RVZ_SECTOR_ALIGNMENT;
      candidate_start = raw->data_offset - skipped;
   }

   /* If position is before candidate's start, check if there's an overlapping
    * partition entry that covers this position */
   if ((uint64_t)position < candidate_start)
   {
      /* Look backward for an entry that might contain our position */
      uint32_t search_idx = candidate_idx;
      while (search_idx > 0)
      {
         rvz_entry_descriptor_t* alt;
         uint64_t alt_start;

         search_idx--;
         alt = &handle->sorted_entries[search_idx];

         if (alt->is_partition)
            alt_start = alt->iso_offset;
         else
         {
            rvz_raw_data_entry_t* raw = &handle->raw_data_entries[alt->entry_index];
            uint64_t skipped = raw->data_offset % RVZ_SECTOR_ALIGNMENT;
            alt_start = raw->data_offset - skipped;
         }

         /* Check if this alternative entry contains our position */
         if (alt_start <= (uint64_t)position &&
             (uint64_t)position < alt->end_offset)
         {
            /* Found a better match - prefer partition entries over raw */
            if (alt->is_partition || !candidate->is_partition)
            {
               candidate_idx = search_idx;
               candidate = alt;
               candidate_start = alt_start;
            }
            break;
         }

         /* If this entry ends before our position, stop searching */
         if (alt->end_offset <= (uint64_t)position)
            break;
      }
   }

   if (out_entry_start)
      *out_entry_start = candidate_start;

   return candidate;
}

/**
 * Stateless entry-with-gaps reader (no position state modification).
 */
static size_t rvz_read_entry_with_gaps_stateless(
    rcheevos_rvz_file_t* handle,
    uint64_t offset,
    uint8_t* output,
    size_t size)
{
   const rvz_entry_descriptor_t* entry_desc;
   uint64_t entry_start, entry_end;
   size_t bytes_read;

   /* Find entry containing current offset */
   entry_desc = rvz_find_entry_for_offset(handle, (int64_t)offset, &entry_start);

   if (!entry_desc)
   {
      /* No more entries - fill rest with zeros */
      memset(output, 0, size);
      return SIZE_MAX; /* Signal EOF */
   }

   entry_end = entry_desc->end_offset;

   /* If offset is before this entry, fill gap with zeros */
   if (offset < entry_start)
   {
      uint64_t to_zero = entry_start - offset;
      if (to_zero > size)
         to_zero = size;

      memset(output, 0, (size_t)to_zero);
      return (size_t)to_zero;
   }

   /* Read from the entry */
   if (entry_desc->is_partition)
   {
      bytes_read = rvz_read_from_partition_entry_stateless(handle, entry_desc,
                                                             entry_end,
                                                             offset, output, size);
   }
   else
   {
      bytes_read = rvz_read_from_raw_entry_stateless(handle, entry_desc,
                                                       entry_start, entry_end,
                                                       offset, output, size);
   }

   return bytes_read;
}

/**
 * Internal stateless read implementation.
 *
 * This is the core read logic that operates on explicit offsets without touching
 * handle->position. All internal reads use this function.
 *
 * @param handle  RVZ file handle
 * @param offset  Offset to read from (in ISO space)
 * @param buffer  Buffer to read data into
 * @param size    Number of bytes to read
 * @return Number of bytes actually read
 */
static size_t rvz_read_stateless(rcheevos_rvz_file_t* handle, uint64_t offset,
                                 void* buffer, size_t size)
{
   size_t total_read = 0;
   uint8_t* output   = (uint8_t*)buffer;
   uint64_t current_offset = offset;

   if (!handle || !buffer || size == 0)
      return 0;

   /* Clamp read to file size */
   if (offset + size > handle->header_1.iso_file_size)
   {
      if (offset >= handle->header_1.iso_file_size)
         return 0;
      size = (size_t)(handle->header_1.iso_file_size - offset);
   }

   /* Phase 1: Read from disc header if needed */
   if (current_offset < RVZ_DISC_HEADER_SIZE && total_read < size)
   {
      size_t bytes_read = rvz_read_disc_header_stateless(handle, current_offset,
                                                           output + total_read,
                                                           size - total_read);
      total_read += bytes_read;
      current_offset += bytes_read;
   }

   /* Phase 2: Patch partition fields if needed */
   if (total_read < size)
   {
      size_t patched_bytes = rvz_patch_partition_fields_stateless(handle, current_offset,
                                                                    output + total_read,
                                                                    size - total_read);
      if (patched_bytes > 0)
      {
         total_read += patched_bytes;
         current_offset += patched_bytes;
      }
   }

   /* Phase 3: Main read loop - read from entries with gap handling */
   while (total_read < size)
   {
      size_t bytes_read = rvz_read_entry_with_gaps_stateless(handle, current_offset,
                                                               output + total_read,
                                                               size - total_read);

      /* Check for special return values */
      if (bytes_read == SIZE_MAX)
         break; /* Filled buffer or reached EOF */

      if (bytes_read == 0)
      {
         /* Zero bytes could mean:
          * 1. Hit entry boundary exactly - there might be more entries
          * 2. True EOF - no more entries available
          * Advance offset by 1 byte and try again to skip past boundary */
         uint64_t next_entry_start;
         const rvz_entry_descriptor_t* next_entry;

         next_entry = rvz_find_entry_for_offset(handle, (int64_t)current_offset + 1, &next_entry_start);

         if (!next_entry)
            break; /* True EOF - no more entries */

         /* Check if next entry has size 0 (e.g., data[1] with 0 sectors in Twilight Princess) */
         if (next_entry->end_offset == next_entry->iso_offset)
         {
            /* Skip zero-size entry by advancing to its end */
            current_offset = next_entry->end_offset;
            continue;
         }

         /* There is a next entry - fill gap with zeros */
         if (next_entry_start > current_offset)
         {
            size_t gap_size = (size_t)(next_entry_start - current_offset);
            if (gap_size > size - total_read)
               gap_size = size - total_read;

            memset(output + total_read, 0, gap_size);
            total_read += gap_size;
            current_offset += gap_size;
         }
         else
         {
            /* Next entry starts at or before current offset - we're stuck reading from
             * the same entry that just returned 0 bytes. Skip to end of entry to avoid infinite loop.
             * This handles cases like partition header reads failing (Twilight Princess). */
            current_offset = next_entry->end_offset;
         }

         continue; /* Try reading from next entry */
      }

      total_read += bytes_read;
      current_offset += bytes_read;
   }

   return total_read;
}

static int rvz_compare_entries(const void* a, const void* b)
{
   const rvz_entry_descriptor_t* entry_a = (const rvz_entry_descriptor_t*)a;
   const rvz_entry_descriptor_t* entry_b = (const rvz_entry_descriptor_t*)b;

   if (entry_a->end_offset < entry_b->end_offset)
      return -1;
   if (entry_a->end_offset > entry_b->end_offset)
      return 1;
   return 0;
}

static bool rvz_build_sorted_entries(rcheevos_rvz_file_t* rvz)
{
   uint32_t i, j;
   uint32_t num_entries = 0;
   uint32_t entry_idx;
   uint64_t end_offset;

   /* Count raw data entries */
   for (i = 0; i < rvz->num_raw_data_entries; i++)
   {
      if (rvz->raw_data_entries[i].num_groups > 0)
         num_entries++;
   }

   /* Count partition data entries */
   for (i = 0; i < rvz->num_partition_entries; i++)
   {
      for (j = 0; j < 2; j++)
      {
         if (rvz->partition_entries[i].data_entries[j].number_of_sectors > 0)
            num_entries++;
      }
   }

   /* Allocate sorted entries array */
   rvz->sorted_entries = (rvz_entry_descriptor_t*)malloc(
      num_entries * sizeof(rvz_entry_descriptor_t));
   if (!rvz->sorted_entries)
   {
      CHEEVOS_ERR("[RVZ] Failed to allocate sorted entries\n");
      return false;
   }

   /* Populate sorted entries */
   entry_idx = 0;

   /* Add raw data entries */
   for (i = 0; i < rvz->num_raw_data_entries; i++)
   {
      if (rvz->raw_data_entries[i].num_groups > 0)
      {
         end_offset = rvz->raw_data_entries[i].data_offset +
                      rvz->raw_data_entries[i].data_size;
         rvz->sorted_entries[entry_idx].end_offset = end_offset;
         rvz->sorted_entries[entry_idx].iso_offset = 0;  /* Not used for raw entries */
         rvz->sorted_entries[entry_idx].rvz_first_sector = 0;  /* Not used for raw entries */
         rvz->sorted_entries[entry_idx].entry_index = i;
         rvz->sorted_entries[entry_idx].partition_data_index = 0;
         rvz->sorted_entries[entry_idx].is_partition = false;
         entry_idx++;
      }
   }

   /* Add partition data entries */
   for (i = 0; i < rvz->num_partition_entries; i++)
   {
      for (j = 0; j < 2; j++)
      {
         if (rvz->partition_entries[i].data_entries[j].number_of_sectors > 0)
         {
            /* first_sector is absolute ISO sector pointing to partition DATA start */
            uint64_t data_start = (uint64_t)rvz->partition_entries[i].data_entries[j].first_sector * RVZ_WII_SECTOR_SIZE;
            uint64_t data_size = (uint64_t)rvz->partition_entries[i].data_entries[j].number_of_sectors * RVZ_WII_SECTOR_SIZE;

            if (j == 0)
            {
               /* data[0]: ISO range includes partition header + data */
               rvz->sorted_entries[entry_idx].iso_offset = data_start - RVZ_WII_PARTITION_HEADER_SIZE;
               rvz->sorted_entries[entry_idx].end_offset = data_start + data_size;
            }
            else  /* j == 1 (data[1]) */
            {
               rvz->sorted_entries[entry_idx].iso_offset = data_start;
               rvz->sorted_entries[entry_idx].end_offset = data_start + data_size;
            }

            rvz->sorted_entries[entry_idx].rvz_first_sector = rvz->partition_entries[i].data_entries[j].first_sector;
            rvz->sorted_entries[entry_idx].entry_index = i;
            rvz->sorted_entries[entry_idx].partition_data_index = (uint8_t)j;
            rvz->sorted_entries[entry_idx].is_partition = true;
            entry_idx++;
         }
      }
   }

   /* Sort by end_offset */
   rvz->num_sorted_entries = num_entries;
   qsort(rvz->sorted_entries, rvz->num_sorted_entries,
         sizeof(rvz_entry_descriptor_t), rvz_compare_entries);

   return true;
}

static bool rvz_parse_partition_entries(RFILE* file, rcheevos_rvz_file_t* rvz)
{
   uint32_t i, j;
   uint8_t* buffer;
   uint32_t decompressed_size;

   if (rvz->header_2.num_partition_entries == 0)
      return true;

   rvz->partition_entries = (rvz_partition_entry_t*)calloc(
      rvz->header_2.num_partition_entries, sizeof(rvz_partition_entry_t));
   if (!rvz->partition_entries)
      return false;

   /* Partition entries are stored uncompressed (just read directly) */
   if (filestream_seek(file, rvz->header_2.partition_entries_offset, SEEK_SET) != 0)
   {
      CHEEVOS_ERR("[RVZ] Failed to seek to partition entries\n");
      return false;
   }

   decompressed_size = rvz->header_2.partition_entry_size * rvz->header_2.num_partition_entries;
   buffer = (uint8_t*)malloc(decompressed_size);
   if (!buffer)
   {
      CHEEVOS_ERR("[RVZ] Failed to allocate partition entries buffer\n");
      return false;
   }

   if (filestream_read(file, buffer, decompressed_size) != decompressed_size)
   {
      CHEEVOS_ERR("[RVZ] Failed to read partition entries\n");
      free(buffer);
      return false;
   }

   /* Parse each partition entry */
   for (i = 0; i < rvz->header_2.num_partition_entries; i++)
   {
      uint8_t* entry_base = buffer + (i * rvz->header_2.partition_entry_size);

      /* Copy partition key (16 bytes) */
      memcpy(rvz->partition_entries[i].partition_key, entry_base, RVZ_PARTITION_KEY_SIZE);

      /* Log partition key */
      {
         uint32_t k;
         RARCH_LOG("[RVZ] Partition %u key: ", i);
         for (k = 0; k < RVZ_PARTITION_KEY_SIZE; k++)
            RARCH_LOG("%02X ", rvz->partition_entries[i].partition_key[k]);
         RARCH_LOG("\n");
      }

      /* Parse the two PartitionDataEntry structures */
      for (j = 0; j < 2; j++)
      {
         uint8_t* data_entry = entry_base + RVZ_PARTITION_KEY_SIZE + (j * 16);
         rvz->partition_entries[i].data_entries[j].first_sector =
            rvz_read_be32(data_entry + 0x00);
         rvz->partition_entries[i].data_entries[j].number_of_sectors =
            rvz_read_be32(data_entry + 0x04);
         rvz->partition_entries[i].data_entries[j].group_index =
            rvz_read_be32(data_entry + 0x08);
         rvz->partition_entries[i].data_entries[j].number_of_groups =
            rvz_read_be32(data_entry + 0x0C);
      }
   }

   /* Calculate exception_lists for each partition data entry */
   #define WII_GROUP_DATA_SIZE 0x1FB400  /* 2,074,624 bytes */
   for (i = 0; i < rvz->header_2.num_partition_entries; i++)
   {
      for (j = 0; j < 2; j++)
      {
         uint32_t chunk_size = rvz->header_2.chunk_size;
         /* Adjust chunk_size for Wii partition data (31KB vs 32KB) */
         uint32_t adjusted_chunk_size = (uint32_t)((uint64_t)chunk_size * RVZ_WII_SECTOR_DATA_SIZE / RVZ_WII_SECTOR_SIZE);
         /* Calculate exception lists: max(1, adjusted_chunk_size / GROUP_DATA_SIZE) */
         uint32_t exception_lists = (adjusted_chunk_size >= WII_GROUP_DATA_SIZE) ?
            (adjusted_chunk_size / WII_GROUP_DATA_SIZE) : 1;
         rvz->partition_entries[i].data_entries[j].exception_lists = exception_lists;
      }
   }
   #undef WII_GROUP_DATA_SIZE

   free(buffer);
   rvz->num_partition_entries = rvz->header_2.num_partition_entries;
   return true;
}

#define RVZ_POSITION_SAVE(handle, saved_var) \
   saved_var = (handle)->position

#define RVZ_POSITION_RESTORE(handle, saved_var) \
   (handle)->position = (saved_var)

#define RVZ_POSITION_SET(handle, new_pos, reason) \
   (handle)->position = (new_pos)

static int rvz_parse_wii_partition_table(rcheevos_rvz_file_t* rvz,
                                         wii_partition_table_entry_t** out_entries,
                                         uint32_t* out_count)
{
   uint32_t i, j;
   uint32_t num_partition_groups;
   uint32_t total_partitions = 0;
   wii_partition_table_entry_t* all_partitions = NULL;
   uint32_t partition_idx = 0;
   uint8_t partition_table_header[32];  /* 4 groups * 8 bytes each */
   int64_t saved_position;

   /* Partition table is at offset 0x40000 in the disc
    * Structure:
    *   0x40000: num_partitions in group 0 (4 bytes)
    *   0x40004: offset to group 0 info (4 bytes, shifted)
    *   0x40008: num_partitions in group 1 (4 bytes)
    *   0x4000C: offset to group 1 info (4 bytes, shifted)
    *   ... (4 groups total)
    *
    * Each group info contains an array of partition entries:
    *   Each entry: 8 bytes (partition_offset_shifted, partition_type)
    */

   /* Save current position */
   RVZ_POSITION_SAVE(rvz, saved_position);

   /* Read partition table header */
   RVZ_POSITION_SET(rvz, 0x40000, "Seek to partition table");
   if (rcheevos_rvz_read(rvz, partition_table_header, 32) != 32)
   {
      CHEEVOS_ERR("[RVZ] Failed to read partition table header\n");
      RVZ_POSITION_RESTORE(rvz, saved_position);  /* Restore on error */
      return 0;
   }

   /* Read number of partition groups (first entry count, typically 4) */
   num_partition_groups = 4;  /* Always 4 groups in Wii discs */

   /* Count total partitions across all groups */
   for (i = 0; i < num_partition_groups; i++)
   {
      uint32_t num_partitions = rvz_read_be32(partition_table_header + (i * 8));
      total_partitions += num_partitions;
   }

   if (total_partitions == 0)
   {
      *out_entries = NULL;
      *out_count = 0;
      RVZ_POSITION_RESTORE(rvz, saved_position);  /* Restore - no partitions found */
      return 1; /* Success but no partitions */
   }

   /* Allocate array for all partition entries */
   all_partitions = (wii_partition_table_entry_t*)calloc(total_partitions,
                                                          sizeof(wii_partition_table_entry_t));
   if (!all_partitions)
   {
      RVZ_POSITION_RESTORE(rvz, saved_position);  /* Restore on allocation error */
      return 0;
   }

   /* Parse each group */
   for (i = 0; i < num_partition_groups; i++)
   {
      uint32_t num_partitions = rvz_read_be32(partition_table_header + (i * 8));
      uint32_t group_offset_shifted = rvz_read_be32(partition_table_header + (i * 8) + 4);
      uint64_t group_offset = (uint64_t)group_offset_shifted << 2;
      uint8_t partition_entry_data[8];

      /* Each partition entry is 8 bytes */
      for (j = 0; j < num_partitions; j++)
      {
         uint64_t entry_offset = group_offset + (j * 8);

         /* Seek to partition entry and read it */
         RVZ_POSITION_SET(rvz, entry_offset, "Seek to partition entry");
         if (rcheevos_rvz_read(rvz, partition_entry_data, 8) != 8)
         {
            CHEEVOS_ERR("[RVZ] Failed to read partition entry at 0x%llx\n",
                        (unsigned long long)entry_offset);
            continue;
         }

         all_partitions[partition_idx].iso_offset_shifted =
            rvz_read_be32(partition_entry_data);
         all_partitions[partition_idx].partition_type =
            rvz_read_be32(partition_entry_data + 4);

         partition_idx++;
      }
   }

   *out_entries = all_partitions;
   *out_count = partition_idx;

   /* Restore position */
   RVZ_POSITION_RESTORE(rvz, saved_position);  /* Restore after parsing partition table */
   return 1;
}

static void rvz_cleanup_on_error(rcheevos_rvz_file_t* rvz)
{
   if (!rvz)
      return;

   if (rvz->partition_mappings)
      free(rvz->partition_mappings);
   if (rvz->sorted_entries)
      free(rvz->sorted_entries);
   if (rvz->partition_entries)
      free(rvz->partition_entries);
   if (rvz->raw_data_entries)
      free(rvz->raw_data_entries);
   if (rvz->file)
      filestream_close(rvz->file);
   free(rvz);
}

/* Public API implementations */

void* rcheevos_rvz_open(const char* path)
{
   rcheevos_rvz_file_t* rvz;
   uint32_t i, j;

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
      CHEEVOS_ERR("[RVZ] Failed to parse header 1\n");
      rvz_cleanup_on_error(rvz);
      return NULL;
   }

   if (rvz->header_1.magic != RVZ_MAGIC && rvz->header_1.magic != WIA_MAGIC)
   {
      CHEEVOS_ERR("[RVZ] Invalid magic: 0x%08X\n", rvz->header_1.magic);
      rvz_cleanup_on_error(rvz);
      return NULL;
   }

   if (!rvz_parse_header_2(rvz->file, &rvz->header_2))
   {
      CHEEVOS_ERR("[RVZ] Failed to parse header 2\n");
      rvz_cleanup_on_error(rvz);
      return NULL;
   }

   /* Check compression support */
   if (rvz->header_2.compression_type != RVZ_COMPRESSION_NONE &&
       rvz->header_2.compression_type != RVZ_COMPRESSION_ZSTD)
   {
      CHEEVOS_ERR("[RVZ] Unsupported compression type: %u (only Zstd is currently supported)\n",
                rvz->header_2.compression_type);
      rvz_cleanup_on_error(rvz);
      return NULL;
   }

   /* Parse data structures */
   if (!rvz_parse_raw_data_entries(rvz->file, rvz))
   {
      CHEEVOS_ERR("[RVZ] Failed to parse raw data entries\n");
      rvz_cleanup_on_error(rvz);
      return NULL;
   }

   if (!rvz_parse_partition_entries(rvz->file, rvz))
   {
      CHEEVOS_ERR("[RVZ] Failed to parse partition entries\n");
      rvz_cleanup_on_error(rvz);
      return NULL;
   }

   if (!rvz_parse_group_entries(rvz->file, rvz))
   {
      CHEEVOS_ERR("[RVZ] Failed to parse group entries\n");
      rvz_cleanup_on_error(rvz);
      return NULL;
   }

   /* Build sorted entry list */
   if (!rvz_build_sorted_entries(rvz))
   {
      rvz_cleanup_on_error(rvz);
      return NULL;
   }

   /* Build partition offset mappings for Wii discs */
   if (rvz->num_partition_entries > 0)
   {
      wii_partition_table_entry_t* partition_table = NULL;
      uint32_t partition_table_count = 0;

      /* Parse the partition table from the disc image to get partition types */
      if (!rvz_parse_wii_partition_table(rvz, &partition_table, &partition_table_count))
      {
         CHEEVOS_ERR("[RVZ] Failed to parse Wii partition table\n");
         rvz_cleanup_on_error(rvz);
         return NULL;
      }

      rvz->partition_mappings = (rvz_partition_mapping_t*)calloc(
         rvz->num_partition_entries, sizeof(rvz_partition_mapping_t));
      if (!rvz->partition_mappings)
      {
         CHEEVOS_ERR("[RVZ] Failed to allocate partition mappings\n");
         if (partition_table)
            free(partition_table);
         rvz_cleanup_on_error(rvz);
         return NULL;
      }

      /* Build mappings from partition entries directly using first_sector */
      rvz->num_partition_mappings = 0;
      for (i = 0; i < rvz->num_partition_entries; i++)
      {
         /* Use data_entries[0].first_sector as the partition data offset */
         uint64_t partition_data_offset = (uint64_t)rvz->partition_entries[i].data_entries[0].first_sector * RVZ_WII_SECTOR_SIZE;

         /* Partition header is RVZ_WII_PARTITION_HEADER_SIZE bytes before data */
         uint64_t partition_offset = partition_data_offset - RVZ_WII_PARTITION_HEADER_SIZE;

         /* Find the matching partition type from the partition table */
         uint32_t partition_type = 0;  /* Default to GAME partition */
         for (j = 0; j < partition_table_count; j++)
         {
            uint64_t table_offset = (uint64_t)partition_table[j].iso_offset_shifted << 2;
            if (table_offset == partition_offset)
            {
               partition_type = partition_table[j].partition_type;
               break;
            }
         }

         rvz->partition_mappings[rvz->num_partition_mappings].partition_offset = partition_offset;
         rvz->partition_mappings[rvz->num_partition_mappings].data_offset = partition_data_offset;
         rvz->partition_mappings[rvz->num_partition_mappings].partition_type = partition_type;
         rvz->num_partition_mappings++;
      }

      /* Analyze partition header coverage by raw_data_entries */
      for (i = 0; i < rvz->num_partition_mappings; i++)
      {
         uint64_t partition_offset = rvz->partition_mappings[i].partition_offset;
         uint64_t tmd_offset = partition_offset + 0x2c0; /* TMD typically at +0x2c0 */

         for (j = 0; j < rvz->num_raw_data_entries; j++)
         {
            uint64_t raw_start = rvz->raw_data_entries[j].data_offset;
            uint64_t raw_end = raw_start + rvz->raw_data_entries[j].data_size;

            if (raw_start <= tmd_offset && raw_end > tmd_offset)
               break;
         }
      }

      /* Free the partition table */
      if (partition_table)
         free(partition_table);
   }

   /* Initialize cache */
   rvz->cache_next = 0;
   for (i = 0; i < RVZ_CHUNK_CACHE_SIZE; i++)
   {
      rvz->cache[i].valid = false;
      rvz->cache[i].data  = NULL;
   }

   /* Initialize encryption cache */
   rvz->encryption_cache_next = 0;
   rvz->skip_reencryption = false;
   for (i = 0; i < 2; i++)
   {
      rvz->encryption_cache[i].valid = false;
      rvz->encryption_cache[i].encrypted_data = NULL;
      rvz->encryption_cache[i].group_offset = 0;
      rvz->encryption_cache[i].entry_index = 0;
   }

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

   RVZ_POSITION_SET(handle, new_pos, "rcheevos_rvz_seek");
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

   if (!handle || !buffer || size == 0)
      return 0;

   /* Call stateless implementation with current position */
   total_read = rvz_read_stateless(handle, (uint64_t)handle->position, buffer, size);

   /* Update position based on bytes actually read */
   handle->position += (int64_t)total_read;

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

   /* Free encryption cache */
   for (i = 0; i < 2; i++)
   {
      if (handle->encryption_cache[i].encrypted_data)
      {
         free(handle->encryption_cache[i].encrypted_data);
         handle->encryption_cache[i].encrypted_data = NULL;
      }
   }

   /* Free data structures */
   if (handle->raw_data_entries)
      free(handle->raw_data_entries);

   if (handle->partition_entries)
      free(handle->partition_entries);

   if (handle->sorted_entries)
      free(handle->sorted_entries);

   if (handle->group_entries)
      free(handle->group_entries);

   if (handle->partition_mappings)
      free(handle->partition_mappings);

   if (handle->temp_buffer)
      free(handle->temp_buffer);

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
   rcheevos_rvz_seek(rvz, RVZ_OFFSET_WII_MAGIC, SEEK_SET);
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
   rcheevos_rvz_seek(rvz, RVZ_OFFSET_GC_MAGIC, SEEK_SET);
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
