/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_TILES_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_TILES_H__

#include <formats/rav1_sequence.h>

typedef struct rav1_tile_layout
{
   unsigned columns, rows, columns_log2, rows_log2, size_bytes;
} rav1_tile_layout_t;

typedef struct rav1_tile
{
   const uint8_t *data;
   size_t size;
   unsigned index, row, column;
} rav1_tile_t;

typedef struct rav1_tile_group
{
   const uint8_t *data;
   size_t size, offset;
   unsigned first, last, next, columns, size_bytes;
} rav1_tile_group_t;

static INLINE bool rav1_tile_span(const uint8_t *data, size_t size,
                                  size_t *offset, unsigned size_bytes,
                                  bool last, rav1_tile_t *tile)
{
   size_t pos = *offset;
   uint64_t length = 0;
   unsigned i;
   if (pos > size || size_bytes > 4)
      return false;
   if (last)
      length = size - pos;
   else
   {
      if (!size_bytes || size_bytes > size - pos)
         return false;
      for (i = 0; i < size_bytes; i++)
         length |= (uint64_t)data[pos++] << (8 * i);
      length++;
   }
   if (!length || length > size - pos)
      return false;
   tile->data = data + pos;
   tile->size = (size_t)length;
   *offset = pos + tile->size;
   return true;
}

/* Input starts at tile_group_obu(), after any frame header. Layout comes from
 * that frame's tile_info(). Retain immutable input until iteration finishes. */
static INLINE bool rav1_tile_group_init(rav1_tile_group_t *out,
                                        const uint8_t *data, size_t size,
                                        const rav1_tile_layout_t *layout,
                                        bool frame_obu)
{
   rav1_tile_group_t group;
   rav1_tile_t tile;
   rav1_bits_t bits;
   size_t offset;
   unsigned count, present = 0, i, columns_log2 = 0, rows_log2 = 0;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!data || !size || size > UINT32_MAX || !layout || !layout->columns ||
       layout->columns > 64 || !layout->rows || layout->rows > 64 ||
       layout->size_bytes > 4)
      return false;
   count = layout->columns * layout->rows;
   for (i = 1; i < layout->columns; i <<= 1)
      columns_log2++;
   for (i = 1; i < layout->rows; i <<= 1)
      rows_log2++;
   if ((count > 1 && !layout->size_bytes) ||
       layout->columns_log2 != columns_log2 || layout->rows_log2 != rows_log2)
      return false;
   bits.data = data;
   bits.size = size;
   bits.byte = 0;
   bits.bit = bits.failed = 0;
   memset(&group, 0, sizeof(group));
   group.last = count - 1;
   if (count > 1)
      present = rav1_bits_read(&bits, 1);
   if (present)
   {
      if (frame_obu)
         return false;
      group.first = rav1_bits_read(&bits, columns_log2 + rows_log2);
      group.last = rav1_bits_read(&bits, columns_log2 + rows_log2);
   }
   if (group.first > group.last || group.last >= count)
      return false;
   while (bits.bit && !bits.failed)
      if (rav1_bits_read(&bits, 1))
         return false;
   if (bits.failed)
      return false;
   group.data = data;
   group.size = size;
   group.offset = offset = bits.byte;
   group.next = group.first;
   group.columns = layout->columns;
   group.size_bytes = layout->size_bytes;
   /* Validate every span before exposing even the first tile. */
   for (i = group.first; i <= group.last; i++)
      if (!rav1_tile_span(data, size, &offset, group.size_bytes,
                          i == group.last, &tile))
         return false;
   *out = group;
   return true;
}

/* 1 tile, 0 end, -1 malformed. Failure clears out and preserves the cursor. */
static INLINE int rav1_tile_group_next(rav1_tile_t *out,
                                       rav1_tile_group_t *group)
{
   rav1_tile_t tile;
   size_t offset;
   if (!out)
      return -1;
   memset(out, 0, sizeof(*out));
   if (!group || !group->data || !group->columns || group->columns > 64 ||
       group->last >= 4096 || group->first > group->last ||
       group->next < group->first || group->next > group->last + 1 ||
       group->offset > group->size)
      return -1;
   if (group->next > group->last)
      return 0;
   offset = group->offset;
   memset(&tile, 0, sizeof(tile));
   if (!rav1_tile_span(group->data, group->size, &offset, group->size_bytes,
                       group->next == group->last, &tile))
      return -1;
   tile.index = group->next;
   tile.row = tile.index / group->columns;
   tile.column = tile.index % group->columns;
   group->offset = offset;
   group->next++;
   *out = tile;
   return 1;
}

#endif
