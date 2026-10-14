/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vulkan_video_import.h"

int main(void)
{
   vulkan_video_layout_t layout;
   assert(vulkan_video_layout(&layout, 3, 3, IMAGE_PIXFMT_NV12, 0));
   assert(layout.bytes == 84 && layout.header[2] == 64);
   assert(layout.header[3] == 3 && layout.header[4] == 76);
   assert(layout.header[5] == 77 && layout.header[6] == 4);
   assert(layout.header[7] == 2 && layout.header[14] == 1);
   assert(vulkan_video_layout(&layout, 3, 3, IMAGE_PIXFMT_P010, IMAGE_YUV_FLAG_FULL_RANGE));
   assert(layout.bytes == 100 && layout.header[3] == 6);
   assert(layout.header[4] == 84 && layout.header[5] == 86);
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
