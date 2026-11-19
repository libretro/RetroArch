/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <string.h>
#include <formats/rpyrowave.h>
#include "rpyrowave_internal.h"

static uint32_t rpyrowave_le32(const uint8_t *data)
{
   return (uint32_t)data[0] | ((uint32_t)data[1] << 8)
      | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

int rpyrowave_read_sequence(const uint8_t *data, size_t size,
      rpyrowave_sequence_t *sequence)
{
   rpyrowave_sequence_t parsed;
   uint32_t dimensions;
   uint32_t flags;
   if (!data || !sequence)
      return RPYROWAVE_INVALID;
   if (size < 8)
      return RPYROWAVE_TRUNCATED;
   dimensions = rpyrowave_le32(data);
   flags      = rpyrowave_le32(data + 4);
   if (!(dimensions & 0x80000000U))
      return RPYROWAVE_INVALID;
   if ((flags >> 24) & 3)
      return RPYROWAVE_UNSUPPORTED;
   parsed.width            = (dimensions & 0x3fff) + 1;
   parsed.height           = ((dimensions >> 14) & 0x3fff) + 1;
   parsed.sequence         = (dimensions >> 28) & 7;
   parsed.total_blocks     = flags & 0xffffff;
   parsed.chroma_444       = (flags >> 26) & 1;
   parsed.primaries_bt2020  = (flags >> 27) & 1;
   parsed.transfer_pq      = (flags >> 28) & 1;
   parsed.transform_bt2020 = (flags >> 29) & 1;
   parsed.limited_range    = (flags >> 30) & 1;
   parsed.chroma_left      = flags >> 31;
   if (!parsed.chroma_444 && ((parsed.width | parsed.height) & 1))
      return RPYROWAVE_INVALID;
   *sequence = parsed;
   return RPYROWAVE_OK;
}

int rpyrowave_read_block(const uint8_t *data, size_t size,
      rpyrowave_block_t *block)
{
   rpyrowave_block_t parsed;
   uint32_t first;
   uint32_t second;
   if (!data || !block)
      return RPYROWAVE_INVALID;
   if (size < 8)
      return RPYROWAVE_TRUNCATED;
   first  = rpyrowave_le32(data);
   second = rpyrowave_le32(data + 4);
   if (first & 0x80000000U)
      return RPYROWAVE_INVALID;
   parsed.bytes      = ((first >> 16) & 0xfff) * 4;
   parsed.ballot     = first & 0xffff;
   parsed.sequence   = (first >> 28) & 7;
   parsed.index      = second >> 8;
   parsed.quant_code = second & 0xff;
   if (parsed.bytes < 8)
      return RPYROWAVE_INVALID;
   if (parsed.bytes > size)
      return RPYROWAVE_TRUNCATED;
   *block = parsed;
   return RPYROWAVE_OK;
}

static unsigned rpyrowave_group_count(unsigned ballot)
{
   unsigned count = 0;
   for (; ballot; ballot >>= 1)
      count += ballot & 1;
   return count;
}

uint32_t rpyrowave_block_layout(const rpyrowave_sequence_t *sequence,
      uint32_t index, rpyrowave_location_t *location)
{
   rpyrowave_location_t found;
   unsigned width;
   unsigned height;
   unsigned component;
   unsigned band;
   uint32_t base = 0;
   int level;
   if (!sequence || !sequence->width || !sequence->height
         || sequence->width > 16384 || sequence->height > 16384
         || sequence->chroma_444 > 1
         || (!sequence->chroma_444
            && ((sequence->width | sequence->height) & 1)))
      return 0;
   width  = (sequence->width + 31) & ~31U;
   height = (sequence->height + 31) & ~31U;
   if (width < 128)
      width = 128;
   if (height < 128)
      height = 128;
   memset(&found, 0, sizeof(found));
   for (level = 4; level >= 0; level--)
   {
      unsigned band_width  = width >> (level + 1);
      unsigned band_height = height >> (level + 1);
      unsigned columns     = (band_width + 31) / 32;
      unsigned rows        = (band_height + 31) / 32;
      uint32_t count       = columns * rows;
      for (component = 0; component < 3; component++)
      {
         if (!level && component && !sequence->chroma_444)
            continue;
         for (band = level == 4 ? 0 : 1; band < 4; band++)
         {
            if (index >= base && index - base < count)
            {
               found.component = component;
               found.level     = (unsigned)level;
               found.band      = band;
               found.x         = ((index - base) % columns) * 32;
               found.y         = ((index - base) / columns) * 32;
               found.width     = band_width;
               found.height    = band_height;
            }
            base += count;
         }
      }
   }
   if (location && index < base)
      *location = found;
   return base;
}

int rpyrowave_read_file_header(const uint8_t *data, size_t size,
      rpyrowave_file_header_t *header)
{
   rpyrowave_file_header_t parsed;
   uint8_t sequence_bytes[8];
   int result;
   if (!data || !header)
      return RPYROWAVE_INVALID;
   if (size < 24)
      return RPYROWAVE_TRUNCATED;
   if (memcmp(data, "PWV1", 4) || data[22] || data[23]
         || (data[7] & 0xf0) || data[8] || data[9] || data[10]
         || (data[11] & 3))
      return RPYROWAVE_INVALID;
   if (data[21] != 1)
      return RPYROWAVE_UNSUPPORTED;
   if (data[20] && data[20] != 8 && data[20] != 10 && data[20] != 16)
      return RPYROWAVE_INVALID;
   memcpy(sequence_bytes, data + 4, 8);
   sequence_bytes[3] |= 0x80;
   result = rpyrowave_read_sequence(sequence_bytes, 8, &parsed.sequence);
   if (result != RPYROWAVE_OK)
      return result;
   parsed.frame_rate_num      = rpyrowave_le32(data + 12);
   parsed.frame_rate_den      = rpyrowave_le32(data + 16);
   parsed.reference_bit_depth = data[20];
   if (parsed.frame_rate_num && !parsed.frame_rate_den)
      return RPYROWAVE_INVALID;
   *header = parsed;
   return RPYROWAVE_OK;
}

static unsigned rpyrowave_magnitude(const uint8_t *planes,
      unsigned count, unsigned pixel)
{
   unsigned value = 0;
   unsigned plane;
   for (plane = 0; plane < count; plane++)
      value = (value << 1) | ((planes[plane] >> pixel) & 1);
   return value;
}

int rpyrowave_decode_block_region(const uint8_t *data, size_t size,
      float *coefficients, size_t stride, unsigned width, unsigned height,
      unsigned clear_region, rpyrowave_block_t *block)
{
   rpyrowave_block_t parsed;
   const uint8_t *codes;
   const uint8_t *scales;
   const uint8_t *signs;
   size_t offset;
   unsigned groups;
   unsigned compact;
   unsigned group;
   unsigned sub;
   unsigned pixel;
   unsigned nonzero = 0;
   unsigned sign_index = 0;
   double base_scale;
   int result;
   if (!coefficients || !block || !width || !height || width > 32 || height > 32
         || stride < width || stride > (size_t)-1 / (height * sizeof(float)))
      return RPYROWAVE_INVALID;
   result = rpyrowave_read_block(data, size, &parsed);
   if (result != RPYROWAVE_OK)
      return result;
   groups = rpyrowave_group_count(parsed.ballot);
   offset = 8 + 3 * groups;
   if (offset > parsed.bytes)
      return RPYROWAVE_INVALID;
   codes  = data + 8;
   scales = codes + 2 * groups;
   for (compact = 0; compact < groups; compact++)
   {
      unsigned code = codes[compact * 2]
         | ((unsigned)codes[compact * 2 + 1] << 8);
      for (sub = 0; sub < 8; sub++)
      {
         unsigned count = ((code >> (sub * 2)) & 3)
            + (scales[compact] & 15);
         if (count > parsed.bytes - offset)
            return RPYROWAVE_INVALID;
         for (pixel = 0; pixel < 8; pixel++)
            if (rpyrowave_magnitude(data + offset, count, pixel))
               nonzero++;
         offset += count;
      }
   }
   if ((nonzero + 7) / 8 > parsed.bytes - offset)
      return RPYROWAVE_INVALID;
   signs = data + offset;
   base_scale = ldexp((double)(8 + (parsed.quant_code & 7)),
         1 - (int)(parsed.quant_code >> 3));
   if (clear_region)
      for (group = 0; group < height; group++)
         memset(coefficients + group * stride, 0, width * sizeof(*coefficients));
   compact = 0;
   offset  = 8 + 3 * groups;
   for (group = 0; group < 16; group++)
   {
      unsigned code;
      double scale;
      if (!(parsed.ballot & (1U << group)))
         continue;
      code  = codes[compact * 2]
         | ((unsigned)codes[compact * 2 + 1] << 8);
      scale = base_scale * (0.25 + (scales[compact] >> 4) * 0.125);
      for (sub = 0; sub < 8; sub++)
      {
         unsigned count = ((code >> (sub * 2)) & 3)
            + (scales[compact] & 15);
         for (pixel = 0; pixel < 8; pixel++)
         {
            unsigned value = rpyrowave_magnitude(data + offset,
                  count, pixel);
            if (value)
            {
               unsigned x = (group % 4) * 8 + (sub / 4) * 4 + pixel / 2;
               unsigned y = (group / 4) * 8 + (sub % 4) * 2 + pixel % 2;
               float decoded = (float)(((double)value + 0.5) * scale);
               if ((signs[sign_index / 8] >> (sign_index % 8)) & 1)
                  decoded = -decoded;
               if (x < width && y < height)
                  coefficients[y * stride + x] = decoded;
               sign_index++;
            }
         }
         offset += count;
      }
      compact++;
   }
   *block = parsed;
   return RPYROWAVE_OK;
}

int rpyrowave_decode_block(const uint8_t *data, size_t size,
      float *coefficients, rpyrowave_block_t *block)
{
   return rpyrowave_decode_block_region(data, size, coefficients,
         32, 32, 32, 1, block);
}

static unsigned rpyrowave_coefficient_offset(unsigned group,
      unsigned sub, unsigned pixel)
{
   unsigned x = (group % 4) * 8 + (sub / 4) * 4 + pixel / 2;
   unsigned y = (group / 4) * 8 + (sub % 4) * 2 + pixel % 2;
   return y * 32 + x;
}

int rpyrowave_encode_block(const int32_t *coefficients,
      unsigned quant_code, uint32_t index, unsigned sequence,
      uint8_t *data, size_t capacity, size_t *written)
{
   uint16_t codes[16];
   uint8_t baselines[16];
   uint8_t depths[128];
   unsigned ballot = 0;
   unsigned groups = 0;
   unsigned payload = 0;
   unsigned nonzero = 0;
   unsigned group;
   unsigned sub;
   unsigned pixel;
   unsigned compact = 0;
   unsigned sign_index = 0;
   size_t bytes;
   size_t offset;
   size_t sign_offset;
   if (!coefficients || !written || quant_code > 255
         || index > 0xffffff || sequence > 7)
      return RPYROWAVE_INVALID;
   for (group = 0; group < 16; group++)
   {
      unsigned maximum = 0;
      unsigned baseline;
      codes[group] = 0;
      for (sub = 0; sub < 8; sub++)
      {
         unsigned combined = 0;
         unsigned bits = 0;
         for (pixel = 0; pixel < 8; pixel++)
         {
            int32_t value = coefficients[
               rpyrowave_coefficient_offset(group, sub, pixel)];
            if (value < -262143 || value > 262143)
               return RPYROWAVE_INVALID;
            if (value)
               nonzero++;
            combined |= (unsigned)(value < 0 ? -value : value);
         }
         for (; combined; combined >>= 1)
            bits++;
         depths[group * 8 + sub] = (uint8_t)bits;
         if (bits > maximum)
            maximum = bits;
      }
      baseline = maximum > 3 ? maximum - 3 : 0;
      baselines[group] = (uint8_t)baseline;
      if (!maximum)
         continue;
      ballot |= 1U << group;
      groups++;
      for (sub = 0; sub < 8; sub++)
      {
         unsigned bits = depths[group * 8 + sub];
         if (bits < baseline)
            bits = baseline;
         depths[group * 8 + sub] = (uint8_t)bits;
         codes[group] |= (uint16_t)((bits - baseline) << (sub * 2));
         payload += bits;
      }
   }
   if (!ballot)
   {
      *written = 0;
      return RPYROWAVE_OK;
   }
   sign_offset = 8 + groups * 3 + payload;
   bytes       = (sign_offset + (nonzero + 7) / 8 + 3) & ~(size_t)3;
   *written    = bytes;
   if (capacity < bytes)
      return RPYROWAVE_BUFFER_TOO_SMALL;
   if (!data)
      return RPYROWAVE_INVALID;
   memset(data, 0, bytes);
   data[0] = (uint8_t)ballot;
   data[1] = (uint8_t)(ballot >> 8);
   data[2] = (uint8_t)(bytes / 4);
   data[3] = (uint8_t)((bytes / 4) >> 8) | (uint8_t)(sequence << 4);
   data[4] = (uint8_t)quant_code;
   data[5] = (uint8_t)index;
   data[6] = (uint8_t)(index >> 8);
   data[7] = (uint8_t)(index >> 16);
   offset  = 8 + groups * 3;
   for (group = 0; group < 16; group++)
   {
      if (!(ballot & (1U << group)))
         continue;
      data[8 + compact * 2] = (uint8_t)codes[group];
      data[9 + compact * 2] = (uint8_t)(codes[group] >> 8);
      data[8 + groups * 2 + compact] = baselines[group];
      for (sub = 0; sub < 8; sub++)
      {
         unsigned bits = depths[group * 8 + sub];
         for (pixel = 0; pixel < 8; pixel++)
         {
            unsigned plane;
            int32_t signed_value = coefficients[
               rpyrowave_coefficient_offset(group, sub, pixel)];
            unsigned value = (unsigned)(signed_value < 0
                  ? -signed_value : signed_value);
            for (plane = 0; plane < bits; plane++)
               if ((value >> (bits - plane - 1)) & 1)
                  data[offset + plane] |= (uint8_t)(1U << pixel);
            if (value)
            {
               if (signed_value < 0)
                  data[sign_offset + sign_index / 8] |=
                     (uint8_t)(1U << (sign_index % 8));
               sign_index++;
            }
         }
         offset += bits;
      }
      compact++;
   }
   return RPYROWAVE_OK;
}
