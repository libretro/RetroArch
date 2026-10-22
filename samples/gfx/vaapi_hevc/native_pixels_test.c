/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Native VA oracle checks require assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_pixels.h"

static VAImage image;
static uint8_t data[512], preserved[512];
static uint32_t crc[3];
/* Independent Python zlib CRCs of logical planar samples, not VA row storage.
 */
static const uint32_t expected[2][2][3] = {
      {{0x7845a0c6u, 0x4c7275b1u, 0x4bd14c5au},
            {0x6392843bu, 0x7a4b8bbfu, 0x810d1554u}},
      {{0xe94f625bu, 0x8a8da584u, 0x22d7c578u},
            {0xa80750d5u, 0xda7c0830u, 0x098da88du}}};

static void setup(unsigned ten)
{
   unsigned p, row, col, bytes = ten ? 2 : 1;
   image.format.fourcc     = ten ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   image.format.byte_order = VA_LSB_FIRST;
   image.width             = 8;
   image.height            = 6;
   image.num_planes        = 2;
   image.data_size         = sizeof(data);
   image.pitches[0] = image.pitches[1] = 12 * bytes;
   image.offsets[0]                    = 5;
   image.offsets[1]                    = 128 * bytes;
   memset(data, 0xa5, sizeof(data));
   for (p = 0; p < 3; p++)
      for (row = 0; row < (p ? 3u : 6u); row++)
         for (col = 0; col < (p ? 4u : 8u); col++)
         {
            unsigned value =
                  ten ? (p ? (p == 1 ? 400 : 700) + 80 * row + 9 * col
                           : 100 * row + 7 * col)
                     : (p ? (p == 1 ? 80 : 140) + 10 * row + col
                           : 10 * row + col);
            size_t offset =
                  p ? image.offsets[1] + row * image.pitches[1]
                              + (2 * col + p - 1) * bytes
                    : image.offsets[0] + row * image.pitches[0] + col * bytes;
            if (ten)
               value = (value << 6) | 45u;
            data[offset] = (uint8_t)value;
            if (ten)
               data[offset + 1] = (uint8_t)(value >> 8);
         }
   memcpy(preserved, data, sizeof(data));
}

static void execute(unsigned ten)
{
   unsigned crop, p, row, bytes = ten ? 2 : 1;
   setup(ten);
   for (crop = 0; crop < 2; crop++)
   {
      assert(native_image_crc(&image, data, crop ? 2 : 0, crop ? 2 : 0,
            crop ? 4 : 8, crop ? 2 : 6, crc));
      assert(!memcmp(crc, expected[ten][crop], sizeof(crc)));
      assert(!memcmp(data, preserved, sizeof(data)));
   }
   image.offsets[0] = 128 * bytes;
   image.offsets[1] = 5;
   memset(data, 0xa5, sizeof(data));
   for (p = 0; p < 2; p++)
      for (row = 0; row < (p ? 3u : 6u); row++)
         memcpy(data + image.offsets[p] + row * image.pitches[p],
               preserved + (p ? 128 * bytes : 5) + row * image.pitches[p],
               8 * bytes);
   assert(native_image_crc(&image, data, 0, 0, 8, 6, crc));
   assert(!memcmp(crc, expected[ten][0], sizeof(crc)));
#define REFUSE(change)                                                         \
   do                                                                          \
   {                                                                           \
      setup(ten);                                                              \
      change;                                                                  \
      crc[0] = crc[1] = crc[2] = 123;                                          \
      assert(!native_image_crc(&image, data, 0, 0, 8, 6, crc));                \
      assert(crc[0] == 123 && crc[1] == 123 && crc[2] == 123);                 \
   } while (0)
   REFUSE(image.num_planes = 3);
   REFUSE(image.format.fourcc = VA_FOURCC_I420);
   REFUSE(image.width = 7);
   REFUSE(image.height = 5);
   REFUSE(image.pitches[0] = 0);
   REFUSE(image.pitches[1] = 1);
   REFUSE(image.pitches[0] = (uint32_t)-1);
   REFUSE(image.offsets[0] = (uint32_t)-1);
   REFUSE(image.offsets[1] = image.offsets[0]);
   REFUSE(image.data_size = image.offsets[1] + image.pitches[1] * 2);
   if (ten)
      REFUSE(image.format.byte_order = VA_MSB_FIRST);
#undef REFUSE
   setup(ten);
   assert(!native_image_crc(&image, data, 1, 0, 4, 2, crc));
   assert(!native_image_crc(&image, data, 0, 1, 4, 2, crc));
   assert(!native_image_crc(&image, data, 6, 0, 4, 2, crc));
   assert(!native_image_crc(&image, data, 0, 4, 4, 4, crc));
   assert(!native_image_crc(&image, data, (unsigned)-1, 0, 4, 2, crc));
   assert(!native_image_crc(&image, data, 0, 0, 0, 2, crc));
}

int main(void)
{
   static const uint8_t vector[] = "123456789";
   uint32_t value                = (uint32_t)-1;
   unsigned i;
   for (i = 0; i < 9; i++)
      value = native_crc_byte(value, vector[i]);
   assert(~value == UINT32_C(0xcbf43926));
   execute(0);
   execute(1);
   puts("PASS: native VA oracle CRCs, cropped padded NV12/P010, ignored low bits and invalid extent refusal; no device execution");
   return 0;
}
