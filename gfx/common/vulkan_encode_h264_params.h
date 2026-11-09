/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_ENCODE_H264_PARAMS_H
#define RARCH_VULKAN_ENCODE_H264_PARAMS_H

#include <stdint.h>
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>
#include "../include/vk_video/vulkan_video_codec_h264std.h"

typedef struct vulkan_encode_h264_config
{
   unsigned width, height, fps_num, fps_den, bitrate;
   bool full_range;
} vulkan_encode_h264_config_t;

/* Stable caller storage; the SPS borrows its VUI until session creation. */
typedef struct vulkan_encode_h264_params
{
   StdVideoH264SequenceParameterSet sps;
   StdVideoH264PictureParameterSet pps;
   StdVideoH264SequenceParameterSetVui vui;
   unsigned coded_width, coded_height;
} vulkan_encode_h264_params_t;

/* Progressive High8 BT709, levels 4.0-5.2, one reference, no B or HRD.
 * Device
 * profile, extent, rate-control and resource support are checked separately. */
static INLINE bool vulkan_encode_h264_params(
      vulkan_encode_h264_params_t *out,
      const vulkan_encode_h264_config_t *config, StdVideoH264LevelIdc max_level)
{
   static const unsigned limits[6][3] = {
      {8192, 245760, 30000000}, {8192, 245760, 75000000},
      {8704, 522240, 75000000}, {22080, 589824, 202500000},
      {36864, 983040, 360000000}, {36864, 2073600, 360000000}
   };
   unsigned width, height, blocks, level;
   uintptr_t a = (uintptr_t)out, b = (uintptr_t)config;
   if (!out || !config ||
       (a <= b ? b - a < sizeof(*out) : a - b < sizeof(*config)) ||
       !config->width || config->width > 4096 || (config->width & 1) ||
       !config->height || config->height > 4096 || (config->height & 1) ||
       !config->fps_num || config->fps_num > UINT32_MAX / 2 ||
       !config->fps_den || !config->bitrate ||
       max_level < STD_VIDEO_H264_LEVEL_IDC_4_0 ||
       max_level > STD_VIDEO_H264_LEVEL_IDC_6_2)
      return false;
   width  = (config->width + 15) / 16;
   height = (config->height + 15) / 16;
   blocks = width * height;
   for (level = 0; level < 6; level++)
      if (blocks <= limits[level][0] &&
          width * width <= 8 * limits[level][0] &&
          height * height <= 8 * limits[level][0] &&
          (uint64_t)blocks * config->fps_num <=
                (uint64_t)limits[level][1] * config->fps_den &&
          config->bitrate <= limits[level][2])
         break;
   if (level == 6 ||
       STD_VIDEO_H264_LEVEL_IDC_4_0 + level > (unsigned)max_level)
      return false;
   memset(out, 0, sizeof(*out));
   out->coded_width  = width * 16;
   out->coded_height = height * 16;
   out->sps.profile_idc = STD_VIDEO_H264_PROFILE_IDC_HIGH;
   out->sps.level_idc = (StdVideoH264LevelIdc)
         (STD_VIDEO_H264_LEVEL_IDC_4_0 + level);
   out->sps.chroma_format_idc = STD_VIDEO_H264_CHROMA_FORMAT_IDC_420;
   out->sps.pic_order_cnt_type = STD_VIDEO_H264_POC_TYPE_0;
   out->sps.flags.frame_mbs_only_flag = 1;
   out->sps.flags.direct_8x8_inference_flag = 1;
   out->sps.flags.vui_parameters_present_flag = 1;
   out->sps.flags.frame_cropping_flag =
         out->coded_width != config->width || out->coded_height != config->height;
   out->sps.max_num_ref_frames = 1;
   out->sps.pic_width_in_mbs_minus1 = width - 1;
   out->sps.pic_height_in_map_units_minus1 = height - 1;
   out->sps.frame_crop_right_offset = (out->coded_width - config->width) / 2;
   out->sps.frame_crop_bottom_offset = (out->coded_height - config->height) / 2;
   out->sps.pSequenceParameterSetVui = &out->vui;
   out->pps.flags.entropy_coding_mode_flag = 1;
   out->pps.flags.deblocking_filter_control_present_flag = 1;
   out->vui.flags.aspect_ratio_info_present_flag = 1;
   out->vui.flags.video_signal_type_present_flag = 1;
   out->vui.flags.color_description_present_flag = 1;
   out->vui.flags.video_full_range_flag = config->full_range;
   out->vui.flags.timing_info_present_flag = 1;
   out->vui.flags.fixed_frame_rate_flag = 1;
   out->vui.aspect_ratio_idc = STD_VIDEO_H264_ASPECT_RATIO_IDC_SQUARE;
   out->vui.video_format = 5;
   out->vui.colour_primaries = 1;
   out->vui.transfer_characteristics = 1;
   out->vui.matrix_coefficients = 1;
   out->vui.num_units_in_tick = config->fps_den;
   out->vui.time_scale = config->fps_num * 2;
   return true;
}
#endif
