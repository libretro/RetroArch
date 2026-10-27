/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "mediacodec_color.h"
#include <formats/rvp9_color.h>
#include <string.h>
static bool mediacodec_color_merge(unsigned *coded, unsigned native)
{
   if (!native)
      return true;
   if (*coded && *coded != 2 && *coded != native)
      return false;
   *coded = native;
   return true;
}
bool mediacodec_color_resolve(const mediacodec_output_format_t *native,
      const video_color_metadata_t *coded, video_color_metadata_t *out)
{
   video_color_metadata_t color;
   unsigned primaries = 0, matrix = 0, transfer = 0, range = 0;
   if (!out)
      return false;
   memset(&color, 0, sizeof(color));
   if (coded)
      color = *coded;
   memset(out, 0, sizeof(*out));
   if (!native || color.range > 2)
      return false;
   switch (native->color_standard)
   {
   case 0:
      break;
   case 1:
      primaries = matrix = 1;
      break;
   case 2:
      primaries = 5;
      matrix = 5;
      break;
   case 3:
      primaries = 5;
      matrix = 1;
      break;
   case 4:
      primaries = matrix = 6;
      break;
   case 5:
      primaries = 6;
      matrix = 7;
      break;
   case 6:
      primaries = matrix = 9;
      break;
   default:
      {
         static const unsigned primaries_map[7] = { 0, 1, 4, 5, 6, 8, 9 };
         static const unsigned matrix_map[7] = { 0, 1, 4, 6, 7, 9, 10 };
         unsigned packed;
         /* Android ColorUtils packs extended primary/matrix pairs. */
         if (native->color_standard < 64 || native->color_standard >= 113)
            return false;
         packed = (unsigned)native->color_standard - 64;
         primaries = primaries_map[packed % 7];
         matrix = matrix_map[packed / 7];
      }
      break;
   }
   if ((native->color_standard == 2 || native->color_standard == 4)
         && (color.matrix == 5 || color.matrix == 6))
      matrix = color.matrix;
   switch (native->color_transfer)
   {
   case 0:
      break;
   case 1:
      transfer = 8;
      break;
   case 3:
      /* Android combines SDR-video transfer variants. Preserve the exact coded
       * curve when it belongs to that family instead of rewriting it as 170M.
       */
      if (color.transfer == 1 || color.transfer == 6 || color.transfer == 14
            || color.transfer == 15)
         transfer = color.transfer;
      else
         transfer = 6;
      break;
   case 6:
      transfer = 16;
      break;
   case 7:
      transfer = 18;
      break;
   default:
      return false;
   }
   switch (native->color_range)
   {
   case 0:
      break;
   case 1:
      range = 2;
      break;
   case 2:
      range = 1;
      break;
   default:
      return false;
   }
   if (!mediacodec_color_merge(&color.primaries, primaries)
         || !mediacodec_color_merge(&color.matrix, matrix)
         || !mediacodec_color_merge(&color.transfer, transfer))
      return false;
   /* Range uses 2 for full, not the CICP unspecified code point. */
   if (range && color.range && range != color.range)
      return false;
   if (range)
      color.range = range;
   *out = color;
   return true;
}

bool mediacodec_vp9_color(const rvp9_hdr *header,
      const video_color_metadata_t *container, video_color_metadata_t *out)
{
   return rvp9_color_resolve(header, container, out);
}

bool mediacodec_av1_color(const rav1_sequence_t *sequence,
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
   if (!sequence || sequence->profile || sequence->monochrome
         || (sequence->bit_depth != 8 && sequence->bit_depth != 10)
         || sequence->subsampling_x != 1 || sequence->subsampling_y != 1
         || sequence->color_range > 1 || color.range > 2
         || !sequence->color_primaries || !sequence->transfer_characteristics
         || !sequence->matrix_coefficients)
      return false;
   range = sequence->color_range ? 2 : 1;
   if ((color.range && color.range != range)
         || (sequence->color_primaries != 2
               && !mediacodec_color_merge(
                     &color.primaries, sequence->color_primaries))
         || (sequence->transfer_characteristics != 2
               && !mediacodec_color_merge(
                     &color.transfer, sequence->transfer_characteristics))
         || (sequence->matrix_coefficients != 2
               && !mediacodec_color_merge(
                     &color.matrix, sequence->matrix_coefficients)))
      return false;
   color.range = range;
   *out = color;
   return true;
}

bool mediacodec_color_configure(
      const video_color_metadata_t *color, mediacodec_config_t *config)
{
   int standard = 0, transfer = 0, range = 0;
   bool primaries, matrix;
   if (!color || !config || color->range > 2)
      return false;
   primaries = color->primaries && color->primaries != 2;
   matrix = color->matrix && color->matrix != 2;
   if (primaries && matrix)
   {
      if (color->primaries == 1 && color->matrix == 1)
         standard = 1;
      else if (color->primaries == 9 && color->matrix == 9)
         standard = 6;
      else if ((color->primaries == 5 || color->primaries == 6)
            && (color->matrix == 5 || color->matrix == 6))
         standard = color->primaries == 5 ? 2 : 4;
      else
         return false;
   }
   switch (color->transfer)
   {
   case 0:
   case 2:
      break;
   case 1:
   case 6:
   case 14:
   case 15:
      transfer = 3;
      break;
   case 8:
      transfer = 1;
      break;
   case 16:
      transfer = 6;
      break;
   case 18:
      transfer = 7;
      break;
   default:
      return false;
   }
   if (color->range)
      range = color->range == 2 ? 1 : 2;
   config->color_standard = standard;
   config->color_transfer = transfer;
   config->color_range = range;
   return true;
}
