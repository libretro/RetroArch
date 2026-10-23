/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VT_AV1_NATIVE_REFERENCE_H
#define VT_AV1_NATIVE_REFERENCE_H
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_inline.h>

static INLINE unsigned av1_reference_code(
      const uint8_t *data, size_t at, unsigned depth)
{
   return depth == 8
         ? data[at]
         : ((unsigned)data[at * 2] | ((unsigned)data[at * 2 + 1] << 8)) >> 6;
}

/* Tightly packed NV12/P010 test readbacks, with no crop or padding. */
static INLINE bool av1_reference_pixel(const uint8_t *y, const uint8_t *uv,
      unsigned width, unsigned height, unsigned depth, unsigned full,
      unsigned position, unsigned x, unsigned row, uint8_t output[4])
{
   double chroma[2] = {0, 0}, rgb[3];
   double cx, cy, fx, fy, scale, maximum, luma, blue, red;
   unsigned cw, ch, dx, dy, channel;
   int left, top;
   if (!y || !uv || !output || !width || !height || width > 16384
         || height > 16384 || (depth != 8 && depth != 10) || full > 1
         || position > 2 || x >= width || row >= height)
      return false;
   cw = (width + 1) / 2;
   ch = (height + 1) / 2;
   cx = position ? x * 0.5 : x / 2;
   cy = position ? row * 0.5 - (position == 1 ? 0.25 : 0) : row / 2;
   left = (int)floor(cx);
   top = (int)floor(cy);
   fx = cx - left;
   fy = cy - top;
   for (dy = 0; dy < 2; dy++)
      for (dx = 0; dx < 2; dx++)
      {
         int col = left + (int)dx, line = top + (int)dy;
         double weight = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
         if (col < 0)
            col = 0;
         if (line < 0)
            line = 0;
         if ((unsigned)col >= cw)
            col = (int)cw - 1;
         if ((unsigned)line >= ch)
            line = (int)ch - 1;
         for (channel = 0; channel < 2; channel++)
            chroma[channel] += weight
                  * av1_reference_code(uv,
                        ((size_t)line * cw + (unsigned)col) * 2 + channel,
                        depth);
      }
   scale = depth == 10 ? 4 : 1;
   maximum = depth == 10 ? 1023 : 255;
   luma = (av1_reference_code(y, (size_t)row * width + x, depth)
                - (full ? 0 : 16 * scale))
         / (full ? maximum : 219 * scale);
   blue = (chroma[0] - 128 * scale) / (full ? maximum : 224 * scale);
   red = (chroma[1] - 128 * scale) / (full ? maximum : 224 * scale);
   rgb[0] = luma + 2 * (1 - 0.2126) * red;
   rgb[1] = luma - 2 * 0.0722 * (1 - 0.0722) / 0.7152 * blue
         - 2 * 0.2126 * (1 - 0.2126) / 0.7152 * red;
   rgb[2] = luma + 2 * (1 - 0.0722) * blue;
   for (channel = 0; channel < 3; channel++)
   {
      double value = rgb[channel] < 0 ? 0 : rgb[channel] > 1 ? 1 : rgb[channel];
      double linear = value < 0.081 ? value / 4.5
                                    : pow((value + 0.099) / 1.099, 1 / 0.45);
      value = linear <= 0.0031308 ? 12.92 * linear
                                  : 1.055 * pow(linear, 1 / 2.4) - 0.055;
      output[channel] = (uint8_t)floor(value * 255 + 0.5);
   }
   output[3] = 255;
   return true;
}
#endif
