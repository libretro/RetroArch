/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_reference.h"
#ifdef NDEBUG
#error Assertions are required for this test
#endif

static void store(uint8_t *data, unsigned index, unsigned value, unsigned depth)
{
   if (depth == 8)
      data[index] = (uint8_t)value;
   else
   {
      value <<= 6;
      data[index * 2] = (uint8_t)value;
      data[index * 2 + 1] = (uint8_t)(value >> 8);
   }
}

int main(void)
{
   static const uint8_t expected[3][4]
         = {{255, 113, 0, 255}, {195, 133, 78, 255}, {142, 142, 142, 255}};
   static const uint8_t gradient[8] = {32, 224, 96, 160, 160, 96, 224, 32};
   static const uint8_t edge[4] = {0, 170, 255, 255};
   uint8_t y[32], uv[32], pixel[4], sentinel[4] = {7, 8, 9, 10};
   unsigned depth, position, full, i, scale;
   for (depth = 8; depth <= 10; depth += 2)
   {
      scale = depth == 10 ? 4 : 1;
      for (i = 0; i < 16; i++)
         store(y, i, 128 * scale, depth);
      for (i = 0; i < 8; i++)
         store(uv, i, gradient[i] * scale, depth);
      for (position = 0; position < 3; position++)
      {
         assert(av1_reference_pixel(
               y, uv, 4, 4, depth, 0, position, 1, 1, pixel));
         assert(!memcmp(pixel, expected[position], sizeof(pixel)));
         assert(av1_reference_pixel(
               y, uv, 4, 4, depth, 0, position, 3, 3, pixel));
         assert(!memcmp(pixel, edge, sizeof(pixel)));
      }
      for (full = 0; full < 2; full++)
      {
         for (i = 0; i < 8; i++)
            store(uv, i, 128 * scale, depth);
         for (i = 0; i < 16; i++)
            store(y, i, full ? 0 : 16 * scale, depth);
         assert(av1_reference_pixel(y, uv, 4, 4, depth, full, 2, 1, 1, pixel));
         assert(!pixel[0] && !pixel[1] && !pixel[2] && pixel[3] == 255);
         for (i = 0; i < 16; i++)
            store(y, i, full ? (depth == 10 ? 1023 : 255) : 235 * scale, depth);
         assert(av1_reference_pixel(y, uv, 4, 4, depth, full, 1, 1, 1, pixel));
         assert(pixel[0] == 255 && pixel[1] == 255 && pixel[2] == 255);
      }
      for (i = 0; i < 15; i++)
         store(y, i, 128 * scale, depth);
      for (i = 0; i < 12; i++)
         store(uv, i, 128 * scale, depth);
      store(uv, 10, 224 * scale, depth);
      store(uv, 11, 32 * scale, depth);
      assert(av1_reference_pixel(y, uv, 5, 3, depth, 0, 2, 4, 2, pixel));
      assert(!memcmp(pixel, edge, sizeof(pixel)));
   }
   memcpy(pixel, sentinel, sizeof(pixel));
   assert(!av1_reference_pixel(y, uv, 4, 4, 8, 0, 3, 1, 1, pixel));
   assert(!memcmp(pixel, sentinel, sizeof(pixel)));
   assert(!av1_reference_pixel(y, uv, 4, 4, 9, 0, 0, 1, 1, pixel));
   assert(!av1_reference_pixel(y, uv, 0, 4, 8, 0, 0, 1, 1, pixel));
   puts("PASS: AV1 Metal reference siting, NV12/P010 levels and odd image "
        "edges");
   return 0;
}
