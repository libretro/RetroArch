/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_H__

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>

/* Borrowed low-overhead OBU views; payload parsing is separate. */
typedef struct rav1_obu
{
   const uint8_t *data, *payload;
   size_t size, payload_size;
   unsigned type, temporal_id, spatial_id, has_size;
} rav1_obu_t;

typedef struct rav1_config
{
   const uint8_t *obus;
   size_t obus_size;
   rav1_obu_t sequence;
   unsigned profile, level, tier, bit_depth, monochrome;
   unsigned subsampling_x, subsampling_y, chroma_sample_position;
   unsigned initial_presentation_delay; /* 0 absent, otherwise 1..16 */
} rav1_config_t;

/* 1 OBU, 0 end, -1 malformed. Failure leaves offset unchanged and out zero.
 * allow_unsized permits a final OBU to consume the remaining bounded sample
 * (MP4/Matroska); configuration OBUs require their size field. */
static INLINE int rav1_obu_next(rav1_obu_t *out, const uint8_t *data,
                                size_t size, size_t *offset, bool allow_unsized)
{
   rav1_obu_t obu;
   size_t pos, start;
   uint64_t length = 0;
   unsigned header, i;
   if (!out)
      return -1;
   memset(out, 0, sizeof(*out));
   if (!offset || *offset > size || (!data && size))
      return -1;
   if (*offset == size)
      return 0;
   memset(&obu, 0, sizeof(obu));
   start = pos = *offset;
   header = data[pos++];
   if (header & 0x81)
      return -1;
   obu.type = (header >> 3) & 15;
   obu.has_size = (header >> 1) & 1;
   if (header & 4)
   {
      unsigned extension;
      if (pos == size)
         return -1;
      extension = data[pos++];
      if (extension & 7)
         return -1;
      obu.temporal_id = extension >> 5;
      obu.spatial_id = (extension >> 3) & 3;
   }
   if (obu.has_size)
   {
      for (i = 0; i < 8; i++)
      {
         unsigned byte;
         if (pos == size)
            return -1;
         byte = data[pos++];
         length |= (uint64_t)(byte & 127) << (7 * i);
         if (!(byte & 128))
            break;
      }
      if (i == 8 || length > UINT32_MAX || length > size - pos)
         return -1;
   }
   else
   {
      if (!allow_unsized || size - pos > UINT32_MAX)
         return -1;
      length = size - pos;
   }
   obu.data = data + start;
   obu.payload = data + pos;
   obu.payload_size = (size_t)length;
   obu.size = pos - start + obu.payload_size;
   *offset = pos + obu.payload_size;
   *out = obu;
   return 1;
}

/* Parse av1C/Matroska CodecPrivate header fields and config OBU framing.
 * rav1_sequence.h validates sequence payloads and agreement with av1C.
 * All returned views borrow data; the caller must retain its storage. */
static INLINE bool rav1_config_parse(rav1_config_t *out, const uint8_t *data,
                                     size_t size)
{
   rav1_config_t config;
   rav1_obu_t obu;
   size_t offset = 0;
   int ret;
   unsigned high, twelve;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!data || size < 4 || data[0] != 0x81 || (data[3] & 0xe0) ||
       (!(data[3] & 0x10) && (data[3] & 15)))
      return false;
   memset(&config, 0, sizeof(config));
   config.profile = data[1] >> 5;
   config.level = data[1] & 31;
   config.tier = data[2] >> 7;
   high = (data[2] >> 6) & 1;
   twelve = (data[2] >> 5) & 1;
   config.bit_depth = twelve ? 12 : high ? 10 : 8;
   config.monochrome = (data[2] >> 4) & 1;
   config.subsampling_x = (data[2] >> 3) & 1;
   config.subsampling_y = (data[2] >> 2) & 1;
   config.chroma_sample_position = data[2] & 3;
   config.initial_presentation_delay = data[3] & 0x10 ? (data[3] & 15) + 1 : 0;
   if (config.profile > 2 || (config.level > 23 && config.level != 31) ||
       (config.level <= 7 && config.tier) ||
       (twelve && (!high || config.profile != 2)) ||
       (!config.subsampling_x && config.subsampling_y) ||
       config.chroma_sample_position == 3 ||
       ((!config.subsampling_x || !config.subsampling_y || config.monochrome) &&
        config.chroma_sample_position) ||
       (config.monochrome &&
        (!config.subsampling_x || !config.subsampling_y)) ||
       (config.profile == 0 &&
        (!config.subsampling_x || !config.subsampling_y)) ||
       (config.profile == 1 &&
        (config.monochrome || config.subsampling_x || config.subsampling_y)) ||
       (config.profile == 2 && !twelve && !config.monochrome &&
        (!config.subsampling_x || config.subsampling_y)))
      return false;
   config.obus = data + 4;
   config.obus_size = size - 4;
   while ((ret = rav1_obu_next(&obu, config.obus, config.obus_size, &offset,
                               false)) == 1)
      if (obu.type == 1)
      {
         if (config.sequence.data || obu.data != config.obus)
            return false;
         config.sequence = obu;
      }
   if (ret < 0)
      return false;
   *out = config;
   return true;
}

#endif
