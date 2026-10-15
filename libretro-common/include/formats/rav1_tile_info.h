/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_TILE_INFO_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_TILE_INFO_H__

#include <formats/rav1_tiles.h>

typedef struct rav1_tile_info
{
   rav1_tile_layout_t layout;
   unsigned mi_col_starts[65], mi_row_starts[65];
   unsigned uniform_spacing, context_update_tile_id;
} rav1_tile_info_t;

static INLINE unsigned rav1_bits_ns(rav1_bits_t *bits, unsigned n)
{
   unsigned w = 0, m, value, tail;
   if (!n || n > 65536)
   {
      bits->failed = 1;
      return 0;
   }
   while (n >> w)
      w++;
   m = (1u << w) - n;
   value = rav1_bits_read(bits, w - 1);
   if (value < m)
      return value;
   tail = rav1_bits_read(bits, 1);
   return (value << 1) - m + tail;
}

/* Input is positioned at tile_info(), without byte alignment. Dimensions are
 * the coded frame size before super-resolution upscaling, in pixels. Failure
 * clears output and leaves the input bit cursor unchanged. */
static INLINE bool rav1_tile_info_parse(rav1_tile_info_t *out,
                                        rav1_bits_t *input, unsigned width,
                                        unsigned height, bool use_128x128)
{
   rav1_bits_t bits;
   unsigned mi_cols, mi_rows, sb_cols, sb_rows, shift, max_width, max_area;
   unsigned min_cols = 0, max_cols = 0, max_rows = 0, min_tiles = 0, min_rows;
   unsigned cols_log2, rows_log2, tile_width, tile_height;
   unsigned start, i, limit, size, widest = 0, value;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!input || input->failed || !input->data || input->byte > input->size ||
       input->bit > 7 || !width || width > 65536 || !height || height > 65536)
      return false;
   bits = *input;
   mi_cols = 2 * ((width + 7) >> 3);
   mi_rows = 2 * ((height + 7) >> 3);
   shift = use_128x128 ? 5 : 4;
   sb_cols = (mi_cols + (1u << shift) - 1) >> shift;
   sb_rows = (mi_rows + (1u << shift) - 1) >> shift;
   max_width = 4096 >> (shift + 2);
   max_area = (4096 * 2304) >> (2 * (shift + 2));
   while ((max_width << min_cols) < sb_cols)
      min_cols++;
   limit = sb_cols < 64 ? sb_cols : 64;
   while ((1u << max_cols) < limit)
      max_cols++;
   limit = sb_rows < 64 ? sb_rows : 64;
   while ((1u << max_rows) < limit)
      max_rows++;
   while ((max_area << min_tiles) < sb_rows * sb_cols)
      min_tiles++;
   if (min_tiles < min_cols)
      min_tiles = min_cols;
   out->uniform_spacing = rav1_bits_read(&bits, 1);
   if (out->uniform_spacing)
   {
      cols_log2 = min_cols;
      while (cols_log2 < max_cols && rav1_bits_read(&bits, 1))
         cols_log2++;
      tile_width = (sb_cols + (1u << cols_log2) - 1) >> cols_log2;
      i = 0;
      for (start = 0; start < sb_cols; start += tile_width)
      {
         if (i == 64)
            goto fail;
         out->mi_col_starts[i++] = start << shift;
      }
      out->mi_col_starts[i] = mi_cols;
      out->layout.columns = i;
      min_rows = min_tiles > cols_log2 ? min_tiles - cols_log2 : 0;
      if (min_rows > max_rows)
         goto fail;
      rows_log2 = min_rows;
      while (rows_log2 < max_rows && rav1_bits_read(&bits, 1))
         rows_log2++;
      tile_height = (sb_rows + (1u << rows_log2) - 1) >> rows_log2;
      i = 0;
      for (start = 0; start < sb_rows; start += tile_height)
      {
         if (i == 64)
            goto fail;
         out->mi_row_starts[i++] = start << shift;
      }
      out->mi_row_starts[i] = mi_rows;
      out->layout.rows = i;
   }
   else
   {
      i = start = 0;
      while (start < sb_cols)
      {
         if (i == 64 || bits.failed)
            goto fail;
         out->mi_col_starts[i++] = start << shift;
         limit = sb_cols - start;
         if (limit > max_width)
            limit = max_width;
         size = rav1_bits_ns(&bits, limit) + 1;
         if (size > widest)
            widest = size;
         start += size;
      }
      out->mi_col_starts[i] = mi_cols;
      out->layout.columns = i;
      cols_log2 = 0;
      while ((1u << cols_log2) < i)
         cols_log2++;
      value = sb_rows * sb_cols;
      if (min_tiles)
         value >>= min_tiles + 1;
      tile_height = value / widest;
      if (!tile_height)
         tile_height = 1;
      i = start = 0;
      while (start < sb_rows)
      {
         if (i == 64 || bits.failed)
            goto fail;
         out->mi_row_starts[i++] = start << shift;
         limit = sb_rows - start;
         if (limit > tile_height)
            limit = tile_height;
         start += rav1_bits_ns(&bits, limit) + 1;
      }
      out->mi_row_starts[i] = mi_rows;
      out->layout.rows = i;
      rows_log2 = 0;
      while ((1u << rows_log2) < i)
         rows_log2++;
   }
   out->layout.columns_log2 = cols_log2;
   out->layout.rows_log2 = rows_log2;
   if (cols_log2 || rows_log2)
   {
      out->context_update_tile_id =
          rav1_bits_read(&bits, cols_log2 + rows_log2);
      out->layout.size_bytes = rav1_bits_read(&bits, 2) + 1;
      if (out->context_update_tile_id >= out->layout.columns * out->layout.rows)
         goto fail;
   }
   if (!bits.failed)
   {
      *input = bits;
      return true;
   }
fail:
   memset(out, 0, sizeof(*out));
   return false;
}

#endif
