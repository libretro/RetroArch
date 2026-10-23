/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_TILES_H
#define RARCH_VAAPI_AV1_TILES_H
#include <va/va.h>
#include <va/va_dec_av1.h>
#include <formats/rav1_hw.h>

static INLINE bool vaapi_av1_tiles_overlap(
      const void *a, size_t an, const void *b, size_t bn)
{
   uintptr_t aa = (uintptr_t)a, bb = (uintptr_t)b;
   return aa <= bb ? bb - aa < an : aa - bb < bn;
}

/* Whole FRAME OBU, normal tiles only. Caller storage is independent of input.
 * Refusal preserves output/count. Success writes only count native records.
 * No packet copy, native call, allocation or borrowed pointer in the records.
 */
static INLINE bool vaapi_av1_tiles(VASliceParameterBufferAV1 *out,
      unsigned capacity, unsigned *count, const uint8_t *packet, size_t size,
      const rav1_hw_picture_t *picture)
{
   rav1_obu_t obu;
   rav1_tile_group_t group;
   rav1_tile_t tile;
   const rav1_tile_layout_t *layout;
   size_t offset = 0, base, bytes;
   uintptr_t address;
   unsigned total, i;
   if (!out || !count || !packet || !size || size > UINT32_MAX || !picture
         || !capacity || capacity > 4096 || picture->header.show_existing)
      return false;
   layout = &picture->tiles.layout;
   if (!layout->columns || layout->columns > 64 || !layout->rows
         || layout->rows > 64 || layout->size_bytes > 4)
      return false;
   total = layout->columns * layout->rows;
   bytes = (size_t)capacity * sizeof(*out);
   if (total > capacity || picture->tiles.context_update_tile_id >= total
         || vaapi_av1_tiles_overlap(out, bytes, packet, size)
         || vaapi_av1_tiles_overlap(out, bytes, picture, sizeof(*picture))
         || vaapi_av1_tiles_overlap(out, bytes, count, sizeof(*count))
         || vaapi_av1_tiles_overlap(count, sizeof(*count), packet, size)
         || vaapi_av1_tiles_overlap(
               count, sizeof(*count), picture, sizeof(*picture))
         || rav1_obu_next(&obu, packet, size, &offset, true) != 1
         || offset != size || obu.type != 6 || obu.temporal_id
         || obu.spatial_id)
      return false;
   address = (uintptr_t)picture->tile_group.data;
   if (address < (uintptr_t)obu.payload || address - (uintptr_t)packet > size)
      return false;
   base = (size_t)(address - (uintptr_t)packet);
   if (picture->tile_group.size != size - base || picture->tile_group.first
         || picture->tile_group.next || picture->tile_group.last != total - 1
         || picture->tile_group.columns != layout->columns
         || picture->tile_group.size_bytes != layout->size_bytes)
      return false;
   /* Reparse all spans before publishing any native record. */
   if (!rav1_tile_group_init(&group, picture->tile_group.data,
             picture->tile_group.size, layout, true)
         || group.offset != picture->tile_group.offset)
      return false;
   for (i = 0; i < total; i++)
   {
      size_t at;
      if (rav1_tile_group_next(&tile, &group) != 1)
         return false;
      at = base + (size_t)(tile.data - group.data);
      memset(&out[i], 0, sizeof(out[i]));
      out[i].slice_data_size = (uint32_t)tile.size;
      out[i].slice_data_offset = (uint32_t)at;
      out[i].slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
      out[i].tile_row = (uint16_t)tile.row;
      out[i].tile_column = (uint16_t)tile.column;
   }
   *count = total;
   return true;
}
#endif
