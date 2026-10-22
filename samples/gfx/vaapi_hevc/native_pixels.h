/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAAPI_NATIVE_PIXELS_H
#define VAAPI_NATIVE_PIXELS_H

#include <va/va.h>
#include <boolean.h>
#include <retro_inline.h>
#include <stdint.h>

/* Oracle only: normalize a mapped NV12/P010 VAImage to the fixture's I420 or
 * little-endian I420-10 CRCs. No production renderer uses CPU readback. */
static INLINE uint32_t native_crc_byte(uint32_t crc, unsigned byte)
{
   unsigned i;
   crc ^= byte;
   for (i = 0; i < 8; i++)
      crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & (0u - (crc & 1u)));
   return crc;
}

/* Failure preserves out. The mapped allocation contains image->data_size
 * bytes; out is separate storage. All extents pass before reading pixels. */
static INLINE bool native_image_crc(const VAImage *image, const uint8_t *data,
      unsigned x, unsigned y, unsigned width, unsigned height, uint32_t out[3])
{
   size_t start[2], span[2];
   uint32_t result[3];
   unsigned p, row, col, bytes;
   bool ten;
   if (!image || !data || !out || image->num_planes != 2 || !image->width
         || !image->height
         || ((image->width | image->height | x | y | width | height) & 1u)
         || !width || !height || x > image->width || y > image->height
         || width > image->width - x || height > image->height - y
         || (image->format.fourcc != VA_FOURCC_NV12
               && image->format.fourcc != VA_FOURCC_P010))
      return false;
   ten = image->format.fourcc == VA_FOURCC_P010;
   if (ten && image->format.byte_order != VA_LSB_FIRST)
      return false;
   bytes = ten ? 2 : 1;
   for (p = 0; p < 2; p++)
   {
      size_t rows      = p ? image->height / 2 : image->height;
      size_t row_bytes = (size_t)image->width * bytes;
      size_t remaining;
      start[p] = image->offsets[p];
      if (!image->pitches[p] || image->pitches[p] < row_bytes
            || start[p] > image->data_size
            || row_bytes > image->data_size - start[p])
         return false;
      remaining = image->data_size - start[p] - row_bytes;
      if (rows - 1 > remaining / image->pitches[p])
         return false;
      span[p] = (rows - 1) * image->pitches[p] + row_bytes;
   }
   if (start[0] <= start[1] ? start[1] - start[0] < span[0]
                           : start[0] - start[1] < span[1])
      return false;
   for (p = 0; p < 3; p++)
   {
      unsigned w   = p ? width / 2 : width;
      unsigned h   = p ? height / 2 : height;
      uint32_t crc = (uint32_t)-1;
      for (row = 0; row < h; row++)
         for (col = 0; col < w; col++)
         {
            size_t offset =
                  p ? start[1] + (y / 2 + row) * (size_t)image->pitches[1]
                              + ((x / 2 + col) * 2 + p - 1) * bytes
                    : start[0] + (y + row) * (size_t)image->pitches[0]
                              + (x + col) * bytes;
            unsigned sample = data[offset];
            if (ten)
               sample = (sample | ((unsigned)data[offset + 1] << 8)) >> 6;
            crc = native_crc_byte(crc, sample & 255u);
            if (ten)
               crc = native_crc_byte(crc, sample >> 8);
         }
      result[p] = ~crc;
   }
   out[0] = result[0];
   out[1] = result[1];
   out[2] = result[2];
   return true;
}
#endif
