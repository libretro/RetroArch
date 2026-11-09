/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "../../../gfx/common/vulkan_encode_h264_params.h"
static vulkan_encode_h264_params_t params, saved;
static vulkan_encode_h264_config_t config;
static void refuse(StdVideoH264LevelIdc maximum)
{
   memset(&params, 0xa5, sizeof(params));
   memcpy(&saved, &params, sizeof(saved));
   assert(!vulkan_encode_h264_params(&params, &config, maximum));
   assert(!memcmp(&saved, &params, sizeof(saved)));
}
static void defaults(void)
{
   config.width = 192;
   config.height = 128;
   config.fps_num = 12;
   config.fps_den = 1;
   config.bitrate = 1000000;
   config.full_range = false;
}
int main(void)
{
   unsigned n;
   defaults();
   assert(vulkan_encode_h264_params(&params, &config, STD_VIDEO_H264_LEVEL_IDC_4_0));
   assert(params.sps.profile_idc == STD_VIDEO_H264_PROFILE_IDC_HIGH);
   assert(params.sps.level_idc == STD_VIDEO_H264_LEVEL_IDC_4_0);
   assert(params.coded_width == 192 && params.coded_height == 128);
   assert(params.sps.pSequenceParameterSetVui == &params.vui);
   assert(!params.sps.flags.frame_cropping_flag);
   assert(params.vui.time_scale == 24 && params.vui.num_units_in_tick == 1);
   assert(!params.vui.flags.video_full_range_flag);
   config.width = 1920; config.height = 1080;
   config.fps_num = 30000; config.fps_den = 1001;
   config.full_range = true;
   assert(vulkan_encode_h264_params(&params, &config, STD_VIDEO_H264_LEVEL_IDC_4_0));
   assert(params.coded_width == 1920 && params.coded_height == 1088);
   assert(params.sps.flags.frame_cropping_flag);
   assert(!params.sps.frame_crop_right_offset && params.sps.frame_crop_bottom_offset == 4);
   assert(params.vui.time_scale == 60000 && params.vui.num_units_in_tick == 1001);
   assert(params.vui.flags.video_full_range_flag);
   config.fps_num = 60000;
   refuse(STD_VIDEO_H264_LEVEL_IDC_4_1);
   assert(vulkan_encode_h264_params(&params, &config, STD_VIDEO_H264_LEVEL_IDC_4_2));
   assert(params.sps.level_idc == STD_VIDEO_H264_LEVEL_IDC_4_2);
   config.width = 3840; config.height = 2160;
   refuse(STD_VIDEO_H264_LEVEL_IDC_5_1);
   assert(vulkan_encode_h264_params(&params, &config, STD_VIDEO_H264_LEVEL_IDC_5_2));
   assert(params.sps.level_idc == STD_VIDEO_H264_LEVEL_IDC_5_2);
   for (n = 2; n < 32; n += 2)
   {
      defaults(); config.width = 160 + n; config.height = 64 + n;
      assert(vulkan_encode_h264_params(&params, &config, STD_VIDEO_H264_LEVEL_IDC_4_0));
      assert(params.coded_width - 2 * params.sps.frame_crop_right_offset == config.width);
      assert(params.coded_height - 2 * params.sps.frame_crop_bottom_offset == config.height);
   }
   defaults(); config.bitrate = 30000001;
   refuse(STD_VIDEO_H264_LEVEL_IDC_4_0);
   assert(vulkan_encode_h264_params(&params, &config, STD_VIDEO_H264_LEVEL_IDC_4_1));
   assert(params.sps.level_idc == STD_VIDEO_H264_LEVEL_IDC_4_1);
   defaults(); config.width = 4096; config.height = 4096;
   refuse(STD_VIDEO_H264_LEVEL_IDC_6_2);
   defaults(); config.width = 4097; refuse(STD_VIDEO_H264_LEVEL_IDC_6_2);
   defaults(); config.height = 129; refuse(STD_VIDEO_H264_LEVEL_IDC_6_2);
   defaults(); config.width = 0; refuse(STD_VIDEO_H264_LEVEL_IDC_6_2);
   defaults(); config.height = 0; refuse(STD_VIDEO_H264_LEVEL_IDC_6_2);
   defaults(); config.fps_num = 0; refuse(STD_VIDEO_H264_LEVEL_IDC_6_2);
   defaults(); config.fps_num = UINT_MAX; refuse(STD_VIDEO_H264_LEVEL_IDC_6_2);
   defaults(); config.fps_den = 0; refuse(STD_VIDEO_H264_LEVEL_IDC_6_2);
   defaults(); config.bitrate = 0; refuse(STD_VIDEO_H264_LEVEL_IDC_6_2);
   defaults(); refuse(STD_VIDEO_H264_LEVEL_IDC_3_2);
   refuse(STD_VIDEO_H264_LEVEL_IDC_INVALID);
   assert(!vulkan_encode_h264_params(NULL, &config, STD_VIDEO_H264_LEVEL_IDC_4_0));
   assert(!vulkan_encode_h264_params(&params, NULL, STD_VIDEO_H264_LEVEL_IDC_4_0));
   memcpy(&saved, &params, sizeof(saved));
   assert(!vulkan_encode_h264_params(&params,
            (const vulkan_encode_h264_config_t*)&params, STD_VIDEO_H264_LEVEL_IDC_4_0));
   assert(!memcmp(&saved, &params, sizeof(saved)));
   puts("Vulkan H264 encoder parameter geometry/rate/range: PASS");
   return 0;
}
