/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RVP9_CONFIG_H
#define LIBRETRO_RVP9_CONFIG_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>
#include <formats/video_color.h>

typedef struct rvp9_config
{
   uint8_t profile, level, bit_depth, chroma;
   uint8_t full_range, primaries, transfer, matrix;
} rvp9_config_t;

/* Complete version-1 vpcC FullBox payload, excluding its size/type header.
 * CICP values stay raw; codec admission resolves their meaning separately. */
static INLINE bool rvp9_config_parse(const uint8_t *data, size_t size,
      rvp9_config_t *out)
{
   rvp9_config_t value;
   if (!out)
      return false;
   if (!data || size != 12 || data[0] != 1 || data[1] || data[2]
         || data[3] || data[10] || data[11])
      goto invalid;
   value.profile    = data[4];
   value.level      = data[5];
   value.bit_depth  = data[6] >> 4;
   value.chroma     = (data[6] >> 1) & 7;
   value.full_range = data[6] & 1;
   value.primaries  = data[7];
   value.transfer   = data[8];
   value.matrix     = data[9];
   if (value.profile > 3 || value.chroma > 3
         || (value.profile < 2 ? value.bit_depth != 8
               : (value.bit_depth != 10 && value.bit_depth != 12))
         || ((value.profile & 1) ? value.chroma < 2 : value.chroma > 1)
         || (!value.matrix && value.chroma != 3))
      goto invalid;
   switch (value.level)
   {
      case 0:
      case 10:
      case 11:
      case 20:
      case 21:
      case 30:
      case 31:
      case 40:
      case 41:
      case 50:
      case 51:
      case 52:
      case 60:
      case 61:
      case 62:
         break;
      default:
         goto invalid;
   }
   *out = value;
   return true;
invalid:
   memset(out, 0, sizeof(*out));
   return false;
}

static INLINE bool rvp9_config_merge_value(unsigned *out, unsigned value)
{
   if (value == 2)
      return true;
   if (*out && *out != 2 && *out != value)
      return false;
   *out = value;
   return true;
}

/* Preserve mastering/light metadata and reject contradictory descriptions. */
static INLINE bool rvp9_config_apply_color(const rvp9_config_t *config,
      const video_color_metadata_t *container, video_color_metadata_t *out)
{
   video_color_metadata_t color;
   unsigned range;
   if (!out)
      return false;
   memset(&color, 0, sizeof(color));
   if (container)
      color = *container;
   memset(out, 0, sizeof(*out));
   if (!config || !config->primaries || !config->transfer
         || config->full_range > 1 || color.range > 2)
      return false;
   range = config->full_range ? 2 : 1;
   if ((color.range && color.range != range)
         || !rvp9_config_merge_value(&color.primaries, config->primaries)
         || !rvp9_config_merge_value(&color.transfer, config->transfer)
         || !rvp9_config_merge_value(&color.matrix, config->matrix))
      return false;
   color.range = range;
   *out = color;
   return true;
}
#endif
