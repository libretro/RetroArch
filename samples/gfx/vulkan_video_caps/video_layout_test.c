/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vulkan_video_import.h"

static void color_check(void)
{
   video_color_metadata_t color;
   float coefficients[6], hdr[4], peak[4];
   unsigned flags = IMAGE_YUV_FLAG_BT709;
   memset(&color, 0, sizeof(color));
   color.range = 2;
   color.matrix = 6;
   assert(video_import_color(&flags, &color, coefficients, hdr));
   assert(flags == IMAGE_YUV_FLAG_FULL_RANGE);
   assert(!hdr[0] && !hdr[1] && !hdr[2] && !hdr[3]);
   color.transfer = 16;
   color.matrix = 2;
   color.max_cll = 2000;
   assert(video_import_color(&flags, &color, coefficients, peak));
   assert(peak[0] == 16 && peak[1] == 1);
   assert(coefficients[2] > 1.47f && coefficients[2] < 1.48f);
   color.max_cll = 0;
   color.mastering.present = 0x100u;
   color.mastering.luminance_max = 20000000;
   assert(video_import_color(&flags, &color, coefficients, hdr));
   assert(!memcmp(peak, hdr, sizeof(hdr)));
   color.primaries = 1;
   assert(video_import_color(&flags, &color, coefficients, hdr));
   assert(hdr[1] == 0);
   color.transfer = 18;
   assert(video_import_color(&flags, &color, coefficients, peak));
   color.max_cll = 10000;
   assert(video_import_color(&flags, &color, coefficients, hdr));
   assert(!memcmp(peak, hdr, sizeof(hdr)));
   color.matrix = 10;
   assert(!video_import_color(&flags, &color, coefficients, hdr));
   color.matrix = 9;
   color.primaries = 12;
   assert(!video_import_color(&flags, &color, coefficients, hdr));
   color.primaries = 9;
   color.range = 3;
   assert(!video_import_color(&flags, &color, coefficients, hdr));
}

int main(void)
{
   vulkan_video_layout_t layout;
   color_check();
   assert(vulkan_video_layout(&layout, 3, 3, IMAGE_PIXFMT_NV12, 0));
   assert(layout.bytes == 100 && layout.header[2] == 80);
   assert(layout.header[3] == 3 && layout.header[4] == 92);
   assert(layout.header[5] == 93 && layout.header[6] == 4);
   assert(layout.header[7] == 2 && layout.header[14] == 1);
   assert(vulkan_video_layout(&layout, 3, 3, IMAGE_PIXFMT_P010, IMAGE_YUV_FLAG_FULL_RANGE));
   assert(layout.bytes == 116 && layout.header[3] == 6);
   assert(layout.header[4] == 100 && layout.header[5] == 102);
   assert(layout.header[6] == 8 && layout.header[7] == 4);
   assert(layout.header[14] == 2 && layout.header[15] == 1);
   assert(!vulkan_video_layout(&layout, 0, 3, IMAGE_PIXFMT_NV12, 0));
   assert(!layout.bytes);
   assert(!vulkan_video_layout(&layout, UINT32_MAX, 3, IMAGE_PIXFMT_P010, 0));
   assert(!vulkan_video_layout(&layout, 65535, 65535, IMAGE_PIXFMT_P010, 0));
   assert(!vulkan_video_layout(&layout, 3, 3, 0, 0));
   assert(!vulkan_video_layout(NULL, 3, 3, IMAGE_PIXFMT_NV12, 0));
   puts("Vulkan video layout: PASS");
   return 0;
}
