/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAAPI_NATIVE_RGB_H
#define VAAPI_NATIVE_RGB_H
#include <retro_inline.h>
#include <stdint.h>

/* Independent SDR reference; no shader constants or production color helper. */
static INLINE void native_rgb(unsigned y, unsigned cb, unsigned cr,
      unsigned ten, unsigned full, unsigned bt709, uint8_t out[4])
{
   double scale = ten ? 4.0 : 1.0;
   double kr    = bt709 ? 0.2126 : 0.299;
   double kb    = bt709 ? 0.0722 : 0.114;
   double yy    = full ? y / (ten ? 1023.0 : 255.0)
                       : (y - 16.0 * scale) / (219.0 * scale);
   double u =
         (cb - 128.0 * scale) / (full ? (ten ? 1023.0 : 255.0) : 224.0 * scale);
   double v =
         (cr - 128.0 * scale) / (full ? (ten ? 1023.0 : 255.0) : 224.0 * scale);
   double rgb[3];
   unsigned i;
   rgb[0] = yy + 2.0 * (1.0 - kr) * v;
   rgb[2] = yy + 2.0 * (1.0 - kb) * u;
   rgb[1] = (yy - kr * rgb[0] - kb * rgb[2]) / (1.0 - kr - kb);
   for (i = 0; i < 3; i++)
      out[i] = rgb[i] <= 0.0   ? 0
               : rgb[i] >= 1.0 ? 255
                               : (uint8_t)(rgb[i] * 255.0 + 0.5);
   out[3] = 255;
}
#endif
