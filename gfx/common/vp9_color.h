/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VP9_COLOR_H
#define RETROARCH_VP9_COLOR_H
#include <boolean.h>
#include <formats/rvp9.h>
#include <formats/video_color.h>
#include <retro_inline.h>
#include <string.h>

static INLINE bool vp9_color_merge(unsigned *value, unsigned coded)
{
   if (!coded)
      return true;
   if (*value && *value != 2 && *value != coded)
      return false;
   *value = coded;
   return true;
}

/* Shared packet-decoder admission. Preserve transfer/mastering/light metadata;
 * do not infer HDR from the VP9 color space. Failure clears out, which may
 * alias container but must not alias header. Profiles 0/2, 8/10-bit 4:2:0. */
static INLINE bool vp9_color_resolve(const rvp9_hdr *header,
      const video_color_metadata_t *container, video_color_metadata_t *out)
{
   video_color_metadata_t color;
   unsigned primaries = 0, matrix = 0, range;
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
         || color.range > 2)
      return false;
   switch (header->color_space)
   {
      case 0:
         break;
      case 1:
         if ((color.primaries && color.primaries != 2 && color.primaries != 5
                   && color.primaries != 6)
               || (color.matrix && color.matrix != 2 && color.matrix != 5
                     && color.matrix != 6))
            return false;
         matrix = color.matrix == 5 ? 5 : 6;
         break;
      case 2:
         primaries = matrix = 1;
         break;
      case 3:
         primaries = matrix = 6;
         break;
      case 4:
         primaries = matrix = 7;
         break;
      case 5:
         primaries = matrix = 9;
         break;
      default:
         return false;
   }
   range = header->color_range ? 2 : 1;
   if ((color.range && color.range != range)
         || !vp9_color_merge(&color.primaries, primaries)
         || !vp9_color_merge(&color.matrix, matrix))
      return false;
   color.range = range;
   *out        = color;
   return true;
}

#endif
