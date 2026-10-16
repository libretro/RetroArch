/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VIDEO_IMPORT_COLOR_H
#define RARCH_VIDEO_IMPORT_COLOR_H
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>
#include <formats/image_yuv_blit.h>
#include <formats/video_color.h>

static INLINE double video_import_hable(double x)
{
   return ((x * (0.15 * x + 0.05) + 0.004)
         / (x * (0.15 * x + 0.50) + 0.06)) - 0.02 / 0.30;
}

/* HDR: transfer, 2020-to-709, exposure, white normalization.
 * Importers use a negative normalization for linear scRGB output. */
static INLINE bool video_import_color(unsigned *yuv,
      const video_color_metadata_t *color, float coefficients[6], float hdr[4])
{
   unsigned matrix = color ? color->matrix : 0;
   unsigned transfer = color ? color->transfer : 0;
   unsigned flags = *yuv;
   bool is_hdr = transfer == 16 || transfer == 18;
   memset(hdr, 0, 4 * sizeof(float));
   if (color && color->range)
   {
      if (color->range > 2)
         return false;
      flags = (flags & ~IMAGE_YUV_FLAG_FULL_RANGE)
         | (color->range == 2 ? IMAGE_YUV_FLAG_FULL_RANGE : 0);
   }
   if (is_hdr)
   {
      double lo = 1.0, hi = 8.0, gain = 2.0;
      double peak = 1000.0;
      unsigned i;
      if (color->primaries != 0 && color->primaries != 2
            && color->primaries != 1 && color->primaries != 9)
         return false;
      if (transfer == 16)
      {
         unsigned cll = color->max_cll;
         if (!cll && (color->mastering.present & 0x100u))
            cll = color->mastering.luminance_max / 10000u;
         if (cll >= 400 && cll <= 10000)
            peak = cll;
      }
      peak /= 203.0;
      for (i = 0; i < 48; i++)
      {
         gain = (lo + hi) * 0.5;
         if (video_import_hable(gain) / video_import_hable(gain * peak) < 0.8)
            lo = gain;
         else
            hi = gain;
      }
      hdr[0] = (float)transfer;
      hdr[1] = color->primaries == 1 ? 0.0f : 1.0f;
      hdr[2] = (float)gain;
      hdr[3] = (float)video_import_hable(gain * peak);
      if (!matrix || matrix == 2)
         matrix = 9;
   }
   if (matrix == 1)
      flags |= IMAGE_YUV_FLAG_BT709;
   else if (matrix == 5 || matrix == 6)
      flags &= ~IMAGE_YUV_FLAG_BT709;
   else if (matrix != 0 && matrix != 2 && !(is_hdr && matrix == 9))
      return false;
   image_yuv_coefficients(flags, coefficients);
   if (matrix == 9)
   {
      float scale = (flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 1.0f : 255.0f / 224.0f;
      coefficients[0] = (flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 1.0f : 255.0f / 219.0f;
      coefficients[1] = (flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 0.0f : -16.0f / 219.0f;
      coefficients[2] = 1.4746f * scale;
      coefficients[3] = -0.16455313f * scale;
      coefficients[4] = -0.57135313f * scale;
      coefficients[5] = 1.8814f * scale;
   }
   *yuv = flags;
   return true;
}
#endif
