/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RVP9_COLOR_H
#define LIBRETRO_RVP9_COLOR_H
#include <formats/rvp9.h>
#include <formats/rvp9_config.h>

/* Shared 8/10-bit 4:2:0 coded/container admission. No inferred transfer. */
static INLINE bool rvp9_color_resolve(const rvp9_hdr *header,
      const video_color_metadata_t *container, video_color_metadata_t *out)
{
   static const unsigned primaries[6] = {0, 0, 1, 6, 7, 9};
   static const unsigned matrices[6] = {0, 6, 1, 6, 7, 9};
   video_color_metadata_t color;
   unsigned matrix, range;
   if (!out)
      return false;
   memset(&color, 0, sizeof(color));
   if (container)
      color = *container;
   memset(out, 0, sizeof(*out));
   if (!header || (header->profile != 0 && header->profile != 2)
         || header->bit_depth != (header->profile == 2 ? 10 : 8)
         || header->subsampling_x != 1 || header->subsampling_y != 1
         || header->color_range < 0 || header->color_range > 1
         || header->color_space < 0 || header->color_space > 5
         || color.range > 2)
      return false;
   matrix = matrices[header->color_space];
   if (header->color_space == 1)
   {
      if ((color.primaries && color.primaries != 2
                && color.primaries != 5 && color.primaries != 6)
            || (color.matrix && color.matrix != 2
                && color.matrix != 5 && color.matrix != 6))
         return false;
      if (color.matrix == 5)
         matrix = 5;
   }
   range = header->color_range ? 2 : 1;
   if ((color.range && color.range != range)
         || (primaries[header->color_space]
               && !rvp9_config_merge_value(&color.primaries,
                     primaries[header->color_space]))
         || (matrix && !rvp9_config_merge_value(&color.matrix, matrix)))
      return false;
   color.range = range;
   *out = color;
   return true;
}
#endif
