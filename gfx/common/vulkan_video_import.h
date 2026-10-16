/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_VIDEO_IMPORT_H
#define RARCH_VULKAN_VIDEO_IMPORT_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>
#include <formats/image.h>
#include <formats/image_yuv_blit.h>
#include "video_import_color.h"

#define VULKAN_VIDEO_HEADER_WORDS 20
typedef struct vulkan_video_layout
{
   size_t bytes;
   uint32_t header[VULKAN_VIDEO_HEADER_WORDS];
   unsigned sample_bytes;
} vulkan_video_layout_t;

static INLINE bool vulkan_video_layout(vulkan_video_layout_t *out,
      unsigned width, unsigned height, unsigned pixfmt, unsigned yuv)
{
   uint64_t y_bytes, c_off, bytes;
   unsigned bps, i;
   float coefficients[6];
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!width || !height || width > 65535 || height > 65535
         || (pixfmt != IMAGE_PIXFMT_NV12 && pixfmt != IMAGE_PIXFMT_P010)) return false;
   bps = pixfmt == IMAGE_PIXFMT_P010 ? 2 : 1;
   y_bytes = (uint64_t)width * height * bps;
   c_off = (VULKAN_VIDEO_HEADER_WORDS * 4 + y_bytes + 3) & ~UINT64_C(3);
   bytes = c_off + (uint64_t)((width + 1u) / 2) * ((height + 1u) / 2) * 2 * bps;
   bytes = (bytes + 3) & ~UINT64_C(3);
   if (bytes > UINT32_MAX || bytes > SIZE_MAX) return false;
   out->bytes = (size_t)bytes; out->sample_bytes = bps;
   out->header[0] = width; out->header[1] = height;
   out->header[2] = VULKAN_VIDEO_HEADER_WORDS * 4; out->header[3] = width * bps;
   out->header[4] = (uint32_t)c_off; out->header[5] = (uint32_t)c_off + bps;
   out->header[6] = ((width + 1u) / 2) * 2 * bps; out->header[7] = 2 * bps;
   image_yuv_coefficients(yuv, coefficients);
   for (i = 0; i < 6; i++) memcpy(&out->header[8 + i], &coefficients[i], sizeof(float));
   out->header[14] = bps;
   out->header[15] = !!(yuv & IMAGE_YUV_FLAG_FULL_RANGE);
   return true;
}
static INLINE bool vulkan_video_color(vulkan_video_layout_t *layout,
      unsigned yuv, const video_color_metadata_t *color)
{
   float coefficients[6], hdr[4];
   if (!video_import_color(&yuv, color, coefficients, hdr))
      return false;
   memcpy(&layout->header[8], coefficients, sizeof(coefficients));
   layout->header[15] = !!(yuv & IMAGE_YUV_FLAG_FULL_RANGE);
   memcpy(&layout->header[16], hdr, sizeof(hdr));
   return true;
}
#endif
