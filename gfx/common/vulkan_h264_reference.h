/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VULKAN_H264_REFERENCE_H
#define VULKAN_H264_REFERENCE_H

#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>

/* NVIDIA 617.42 on RTX 5090 needs short-term reference FrameNum as
 * signed FrameNumWrap for POC type 2. Raw frame_num corrupts prediction at a
 * numbering wrap. Keep this driver workaround separate from the Vulkan
 * contract used by other devices; long-term indices are never wrapped. */
static INLINE bool vulkan_h264_reference_wrap_driver(unsigned vendor,
      unsigned device, unsigned driver)
{
   return vendor == 0x10de && device == 0x2b85
      && driver == ((617u << 22) | (42u << 14));
}

static INLINE uint16_t vulkan_h264_reference_frame_num(unsigned frame_num,
      unsigned current, unsigned log2_max_frame_num, unsigned poc_type,
      bool long_term, bool wrap_driver)
{
   if (wrap_driver && poc_type == 2 && !long_term && frame_num > current
         && log2_max_frame_num >= 4 && log2_max_frame_num <= 16)
      return (uint16_t)((int)frame_num - (1 << log2_max_frame_num));
   return (uint16_t)frame_num;
}
#endif
