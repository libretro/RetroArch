/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 */

#include <formats/rpyrowave.h>
#include <formats/rpyrowave_gpu.h>
#include <string.h>

int rpyrowave_gpu_color_parameters(const rpyrowave_sequence_t *sequence,
      unsigned bit_depth, unsigned inverse, unsigned flip_y,
      unsigned texture_width, unsigned texture_height,
      unsigned x, unsigned y, rpyrowave_color_parameters_t *parameters)
{
   rpyrowave_color_parameters_t result;
   unsigned maximum;
   unsigned multiplier;
   if (!parameters || !rpyrowave_block_layout(sequence, 0, NULL)
         || bit_depth < 8 || bit_depth > 16 || inverse > 1 || flip_y > 1
         || sequence->transform_bt2020 > 1 || sequence->limited_range > 1
         || sequence->primaries_bt2020 > 1 || sequence->transfer_pq > 1
         || sequence->chroma_left > 1 || x > texture_width || y > texture_height
         || sequence->width > texture_width - x || sequence->height > texture_height - y)
      return RPYROWAVE_INVALID;
   memset(&result, 0, sizeof(result));
   result.image_width  = (sequence->width + 31) & ~31U;
   result.image_height = (sequence->height + 31) & ~31U;
   if (result.image_width < 128) result.image_width = 128;
   if (result.image_height < 128) result.image_height = 128;
   result.width        = sequence->width;
   result.height       = sequence->height;
   result.chroma_444   = sequence->chroma_444;
   result.chroma_left  = sequence->chroma_left;
   result.inverse      = inverse;
   result.flip_y       = flip_y;
   result.texture_x    = x;
   result.texture_y    = y;
   result.colorimetry  = sequence->primaries_bt2020 | (sequence->transfer_pq << 1);
   result.red          = sequence->transform_bt2020 ? 0.2627f : 0.2126f;
   result.blue         = sequence->transform_bt2020 ? 0.0593f : 0.0722f;
   maximum             = (1U << bit_depth) - 1;
   multiplier          = 1U << (bit_depth - 8);
   result.chroma_center = (float)(128 * multiplier) / maximum;
   result.luma_offset  = sequence->limited_range ? (float)(16 * multiplier) / maximum : 0.0f;
   result.luma_scale   = sequence->limited_range ? (float)(219 * multiplier) / maximum : 1.0f;
   result.chroma_scale = sequence->limited_range ? (float)(224 * multiplier) / maximum : 1.0f;
   *parameters = result;
   return RPYROWAVE_OK;
}

int rpyrowave_gpu_color_parameters_ex(const rpyrowave_sequence_t *sequence,
      unsigned bit_depth, unsigned inverse, unsigned flip_y,
      unsigned texture_width, unsigned texture_height,
      unsigned x, unsigned y, unsigned scale_source, rpyrowave_color_parameters_t *parameters)
{
   int result;
   if (scale_source > 1) return RPYROWAVE_INVALID;
   if (!scale_source || inverse)
      return rpyrowave_gpu_color_parameters(sequence, bit_depth, inverse, flip_y,
            texture_width, texture_height, x, y, parameters);
   if (!sequence || !texture_width || !texture_height || texture_width > 16384
         || texture_height > 16384 || x || y) return RPYROWAVE_INVALID;
   result = rpyrowave_gpu_color_parameters(sequence, bit_depth, inverse, flip_y,
         sequence->width, sequence->height, 0, 0, parameters);
   if (result == RPYROWAVE_OK)
   {
      parameters->padding[0] = (float)texture_width / sequence->width;
      parameters->padding[1] = (float)texture_height / sequence->height;
   }
   return result;
}

int rpyrowave_dwt_record(unsigned aligned_width, unsigned aligned_height,
      unsigned chroma_444, unsigned inverse,
      rpyrowave_dwt_record_t record, void *userdata)
{
   rpyrowave_dwt_parameters_t parameters;
   unsigned source = 0;
   int iteration;
   if (!record || aligned_width < 128 || aligned_width > 16384
         || aligned_height < 128 || aligned_height > 16384
         || (aligned_width & 31) || (aligned_height & 31)
         || chroma_444 > 1 || inverse > 1)
      return RPYROWAVE_INVALID;
   parameters.image_width  = aligned_width;
   parameters.image_height = aligned_height;
   parameters.inverse      = inverse;
   parameters.chroma_444   = chroma_444;
   parameters.reserved     = 0;
   for (iteration = 0; iteration < 5; iteration++)
   {
      unsigned axis;
      parameters.level = inverse ? 4 - iteration : iteration;
      for (axis = 0; axis < 2; axis++)
      {
         parameters.axis = inverse ? 1 - axis : axis;
         for (parameters.phase = 0; parameters.phase < 5; parameters.phase++)
         {
            if (!record(userdata, &parameters, source,
                     ((aligned_width >> parameters.level) + 7) / 8,
                     ((aligned_height >> parameters.level) + 7) / 8, 3))
               return RPYROWAVE_INVALID;
            source ^= 1;
         }
      }
   }
   return RPYROWAVE_OK;
}

static void rpyrowave_gpu_store32(uint8_t *data, uint32_t value)
{
   data[0] = (uint8_t)value;
   data[1] = (uint8_t)(value >> 8);
   data[2] = (uint8_t)(value >> 16);
   data[3] = (uint8_t)(value >> 24);
}

int rpyrowave_gpu_layout(const rpyrowave_sequence_t *sequence, uint32_t maximum_bytes,
      rpyrowave_gpu_layout_t *layout)
{
   rpyrowave_gpu_layout_t result;
   uint32_t count = rpyrowave_block_layout(sequence, 0, NULL);
   uint32_t offset;
   if (!layout || !count || maximum_bytes < 8)
      return RPYROWAVE_INVALID;
   offset = (64 + count * 16 + 255) & ~255U;
   if (maximum_bytes > 0xfffffffcU - offset)
      return RPYROWAVE_INVALID;
   result.image_width  = (sequence->width + 31) & ~31U;
   result.image_height = (sequence->height + 31) & ~31U;
   if (result.image_width < 128)
      result.image_width = 128;
   if (result.image_height < 128)
      result.image_height = 128;
   result.block_count     = count;
   result.wire_offset     = offset;
   result.workspace_bytes = (offset + maximum_bytes + 3) & ~3U;
   result.maximum_bytes   = maximum_bytes;
   result.width           = sequence->width;
   result.height          = sequence->height;
   result.chroma_444      = sequence->chroma_444;
   *layout = result;
   return RPYROWAVE_OK;
}

static int rpyrowave_gpu_parameters(const rpyrowave_gpu_layout_t *layout,
      const rpyrowave_sequence_t *sequence, rpyrowave_packet_parameters_t *parameters)
{
   if (!layout || !sequence || sequence->width != layout->width
         || sequence->height != layout->height || sequence->chroma_444 != layout->chroma_444)
      return RPYROWAVE_UNSUPPORTED;
   if (sequence->sequence > 7 || sequence->chroma_444 > 1
         || sequence->primaries_bt2020 > 1 || sequence->transfer_pq > 1
         || sequence->transform_bt2020 > 1 || sequence->limited_range > 1
         || sequence->chroma_left > 1)
      return RPYROWAVE_INVALID;
   parameters->image_width     = layout->image_width;
   parameters->image_height    = layout->image_height;
   parameters->block_count     = layout->block_count;
   parameters->operation       = 0;
   parameters->maximum_bytes   = layout->maximum_bytes;
   parameters->sequence        = sequence->sequence;
   parameters->width           = sequence->width;
   parameters->height          = sequence->height;
   parameters->usability       = (sequence->chroma_444 << 26)
      | (sequence->primaries_bt2020 << 27) | (sequence->transfer_pq << 28)
      | (sequence->transform_bt2020 << 29) | (sequence->limited_range << 30)
      | (sequence->chroma_left << 31);
   parameters->fixed_quant_code = 0;
   parameters->fixed_quantizer  = 0;
   parameters->reserved         = 0;
   return RPYROWAVE_OK;
}

int rpyrowave_gpu_blocks(const rpyrowave_gpu_layout_t *layout,
      rpyrowave_gpu_block_t *blocks, size_t count)
{
   rpyrowave_sequence_t sequence;
   rpyrowave_gpu_layout_t expected;
   uint32_t index;
   if (!layout || !blocks || count < layout->block_count)
      return RPYROWAVE_INVALID;
   memset(&sequence, 0, sizeof(sequence));
   sequence.width      = layout->width;
   sequence.height     = layout->height;
   sequence.chroma_444 = layout->chroma_444;
   if (rpyrowave_gpu_layout(&sequence, layout->maximum_bytes, &expected)
         || memcmp(layout, &expected, sizeof(expected)))
      return RPYROWAVE_INVALID;
   for (index = 0; index < layout->block_count; index++)
   {
      rpyrowave_location_t location;
      uint32_t origin;
      rpyrowave_block_layout(&sequence, index, &location);
      origin = location.component * layout->image_width * layout->image_height;
      if (location.band & 1)
         origin += location.width;
      if (location.band & 2)
         origin += location.height * layout->image_width;
      origin += location.y * layout->image_width + location.x;
      blocks[index].origin = origin;
      blocks[index].width  = location.width - location.x;
      blocks[index].height = location.height - location.y;
      blocks[index].index  = index;
      if (blocks[index].width > 32)
         blocks[index].width = 32;
      if (blocks[index].height > 32)
         blocks[index].height = 32;
   }
   return RPYROWAVE_OK;
}

static int rpyrowave_gpu_packet_emit(rpyrowave_packet_record_t record, void *userdata,
      const rpyrowave_packet_parameters_t *parameters, unsigned groups)
{
   return record(userdata, parameters, groups > 65535 ? 65535 : groups,
         (groups + 65534) / 65535);
}

int rpyrowave_gpu_encode_record(const rpyrowave_gpu_layout_t *layout,
      const rpyrowave_sequence_t *sequence, uint32_t maximum_bytes, int fixed_quant_code,
      rpyrowave_packet_record_t record, void *userdata)
{
   rpyrowave_packet_parameters_t parameters;
   unsigned iteration;
   int result;
   if (!record || fixed_quant_code < -1 || fixed_quant_code > 255)
      return RPYROWAVE_INVALID;
   result = rpyrowave_gpu_parameters(layout, sequence, &parameters);
   if (result != RPYROWAVE_OK)
      return result;
   if (maximum_bytes < 8 || maximum_bytes > layout->maximum_bytes)
      return RPYROWAVE_INVALID;
   parameters.maximum_bytes = maximum_bytes;
   parameters.fixed_quantizer = fixed_quant_code >= 0;
   parameters.fixed_quant_code = fixed_quant_code >= 0 ? (uint32_t)fixed_quant_code : 0;
   if (!rpyrowave_gpu_packet_emit(record, userdata, &parameters,
            (((layout->wire_offset + maximum_bytes + 3) / 4) + 127) / 128))
      return RPYROWAVE_INVALID;
   if (fixed_quant_code < 0)
      for (iteration = 0; iteration < 8; iteration++)
      {
         parameters.operation = RPYROWAVE_PACKET_COST;
         if (!rpyrowave_gpu_packet_emit(record, userdata, &parameters, layout->block_count))
            return RPYROWAVE_INVALID;
         parameters.operation = RPYROWAVE_PACKET_SEARCH;
         if (!rpyrowave_gpu_packet_emit(record, userdata, &parameters, 1))
            return RPYROWAVE_INVALID;
      }
   parameters.operation = RPYROWAVE_PACKET_COST;
   if (!rpyrowave_gpu_packet_emit(record, userdata, &parameters, layout->block_count))
      return RPYROWAVE_INVALID;
   parameters.operation = RPYROWAVE_PACKET_PREFIX;
   if (!rpyrowave_gpu_packet_emit(record, userdata, &parameters, 1))
      return RPYROWAVE_INVALID;
   parameters.operation = RPYROWAVE_PACKET_PACK;
   return rpyrowave_gpu_packet_emit(record, userdata, &parameters, layout->block_count)
      ? RPYROWAVE_OK : RPYROWAVE_INVALID;
}

int rpyrowave_gpu_index_frame(const rpyrowave_gpu_layout_t *layout,
      void *workspace, size_t workspace_size, size_t frame_bytes,
      unsigned allow_partial, rpyrowave_packet_parameters_t *parameters)
{
   rpyrowave_sequence_t sequence;
   rpyrowave_packet_parameters_t result;
   uint8_t *bytes = (uint8_t*)workspace;
   const uint8_t *frame;
   size_t offset = 8;
   uint32_t received = 0;
   int status;
   if (!layout || !workspace || !parameters || workspace_size < layout->workspace_bytes
         || layout->wire_offset > workspace_size || layout->wire_offset < 64
         || layout->block_count > (layout->wire_offset - 64) / 16
         || frame_bytes > workspace_size - layout->wire_offset
         || frame_bytes > layout->maximum_bytes || frame_bytes < 8)
      return RPYROWAVE_INVALID;
   frame = bytes + layout->wire_offset;
   status = rpyrowave_read_sequence(frame, frame_bytes, &sequence);
   if (status != RPYROWAVE_OK)
      return status;
   status = rpyrowave_gpu_parameters(layout, &sequence, &result);
   if (status != RPYROWAVE_OK)
      return status;
   if (sequence.total_blocks > layout->block_count)
      return RPYROWAVE_INVALID;
   memset(bytes, 0, layout->wire_offset);
   while (offset < frame_bytes)
   {
      rpyrowave_block_t block;
      uint8_t *metadata;
      status = rpyrowave_read_block(frame + offset, frame_bytes - offset, &block);
      if (status != RPYROWAVE_OK)
         return status;
      if (block.sequence != sequence.sequence)
         return RPYROWAVE_INVALID;
      if (block.ballot && block.index < layout->block_count)
      {
         metadata = bytes + 64 + block.index * 16;
         if (!(metadata[0] | metadata[1] | metadata[2] | metadata[3]))
         {
            rpyrowave_gpu_store32(metadata, (uint32_t)block.bytes);
            rpyrowave_gpu_store32(metadata + 4, (uint32_t)offset);
            received++;
         }
      }
      offset += block.bytes;
   }
   if (received > sequence.total_blocks)
      return RPYROWAVE_INVALID;
   if (!allow_partial && received != sequence.total_blocks)
      return RPYROWAVE_TRUNCATED;
   rpyrowave_gpu_store32(bytes + 4, (uint32_t)frame_bytes);
   rpyrowave_gpu_store32(bytes + 8, received);
   result.maximum_bytes = (uint32_t)frame_bytes;
   result.operation = RPYROWAVE_PACKET_UNPACK;
   *parameters = result;
   return RPYROWAVE_OK;
}

int rpyrowave_gpu_decode_record(const rpyrowave_gpu_layout_t *layout,
      const rpyrowave_packet_parameters_t *parameters,
      rpyrowave_packet_record_t record, void *userdata)
{
   if (!layout || !parameters || !record || parameters->operation != RPYROWAVE_PACKET_UNPACK
         || parameters->block_count != layout->block_count
         || parameters->image_width != layout->image_width
         || parameters->image_height != layout->image_height
         || parameters->width != layout->width || parameters->height != layout->height
         || parameters->maximum_bytes > layout->maximum_bytes || parameters->maximum_bytes < 8)
      return RPYROWAVE_INVALID;
   return rpyrowave_gpu_packet_emit(record, userdata, parameters, layout->block_count)
      ? RPYROWAVE_OK : RPYROWAVE_INVALID;
}
