/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "../../../gfx/common/vulkan_encode_hevc_params.h"

static void refused(vulkan_encode_hevc_params_t *p,
      const vulkan_encode_hevc_config_t *c, StdVideoH265LevelIdc maximum)
{
   const unsigned char *bytes = (const unsigned char*)p;
   size_t i;
   memset(p, 0xa5, sizeof(*p));
   assert(!vulkan_encode_hevc_params(p, c, maximum));
   for (i = 0; i < sizeof(*p); i++)
      assert(bytes[i] == 0xa5);
}

int main(void)
{
   vulkan_encode_hevc_config_t c = {320, 192, 60, 1, 1500000, 8, 64,
      VULKAN_ENCODE_HEVC_BT709, false};
   vulkan_encode_hevc_config_t bad;
   vulkan_encode_hevc_params_t *p = (vulkan_encode_hevc_params_t*)malloc(sizeof(*p));
   unsigned char *alias = (unsigned char*)calloc(1, sizeof(*p) + sizeof(c));
   vulkan_encode_hevc_config_t *inside;
   assert(p && alias);
   assert(vulkan_encode_hevc_params(p, &c, STD_VIDEO_H265_LEVEL_IDC_6_2));
   assert(p->ptl.general_level_idc == STD_VIDEO_H265_LEVEL_IDC_2_0);
   assert(p->vps.pProfileTierLevel == &p->ptl && p->sps.pProfileTierLevel == &p->ptl);
   assert(p->vps.pDecPicBufMgr == &p->dpb && p->sps.pDecPicBufMgr == &p->dpb);
   assert(p->sps.pSequenceParameterSetVui == &p->vui);
   assert(p->sps.pShortTermRefPicSet == &p->reference);
   c.fps_num = 61;
   refused(p, &c, STD_VIDEO_H265_LEVEL_IDC_2_0);
   c.fps_num = 60;
   c.bitrate++;
   refused(p, &c, STD_VIDEO_H265_LEVEL_IDC_2_0);
   c.bitrate--;
   c.fps_num = 60000;
   c.fps_den = 1001;
   assert(vulkan_encode_hevc_params(p, &c, STD_VIDEO_H265_LEVEL_IDC_2_0));
   assert(p->vui.vui_time_scale == 60000 && p->vui.vui_num_units_in_tick == 1001);
   c.width = 1920;
   c.height = 1080;
   c.bitrate = 16000000;
   assert(vulkan_encode_hevc_params(p, &c, STD_VIDEO_H265_LEVEL_IDC_4_1));
   refused(p, &c, STD_VIDEO_H265_LEVEL_IDC_4_0);
   c.width = 7680;
   c.height = 4320;
   c.fps_num = 60;
   c.fps_den = 1;
   c.bit_depth = 10;
   c.color = VULKAN_ENCODE_HEVC_PQ;
   refused(p, &c, STD_VIDEO_H265_LEVEL_IDC_6_0);
   assert(vulkan_encode_hevc_params(p, &c, STD_VIDEO_H265_LEVEL_IDC_6_1));
   assert(p->ptl.general_profile_idc == STD_VIDEO_H265_PROFILE_IDC_MAIN_10);
   assert(p->vui.colour_primaries == 9 && p->vui.transfer_characteristics == 16);
   assert(p->vui.matrix_coeffs == 9 && p->sps.bit_depth_luma_minus8 == 2);
   assert(!p->vui.pHrdParameters && !p->vps.pHrdParameters);
   c.width = 1918;
   c.height = 1078;
   c.ctb_size = 16;
   c.color = VULKAN_ENCODE_HEVC_UNSPECIFIED;
   assert(vulkan_encode_hevc_params(p, &c, STD_VIDEO_H265_LEVEL_IDC_6_2));
   assert(p->coded_width - 2 * p->sps.conf_win_right_offset == 1918);
   assert(p->coded_height - 2 * p->sps.conf_win_bottom_offset == 1078);
   assert(!p->vui.flags.colour_description_present_flag);
   assert(p->sps.log2_diff_max_min_luma_coding_block_size == 1);
   assert(p->sps.log2_diff_max_min_luma_transform_block_size == 2);
   assert(p->sps.max_transform_hierarchy_depth_inter == 2);
   assert(p->sps.max_transform_hierarchy_depth_intra == 2);
#define BAD(field, value) \
   do \
   { \
      bad = c; \
      bad.field = value; \
      refused(p, &bad, STD_VIDEO_H265_LEVEL_IDC_6_2); \
   } while (0)
   BAD(width, 0);
   BAD(width, 8193);
   BAD(height, 1077);
   BAD(fps_num, 0);
   BAD(fps_den, 0);
   BAD(bitrate, 0);
   BAD(bit_depth, 12);
   BAD(ctb_size, 8);
   BAD(color, (enum vulkan_encode_hevc_color)-1);
   BAD(color, (enum vulkan_encode_hevc_color)4);
#undef BAD
   c.bit_depth = 8;
   c.color = VULKAN_ENCODE_HEVC_PQ;
   refused(p, &c, STD_VIDEO_H265_LEVEL_IDC_6_2);
   c.color = VULKAN_ENCODE_HEVC_UNSPECIFIED;
   inside = (vulkan_encode_hevc_config_t*)alias;
   *inside = c;
   assert(!vulkan_encode_hevc_params((vulkan_encode_hevc_params_t*)alias,
            inside, STD_VIDEO_H265_LEVEL_IDC_6_2));
   assert(inside->width == c.width && inside->height == c.height);
   assert(inside->fps_num == c.fps_num && inside->fps_den == c.fps_den);
   assert(inside->bitrate == c.bitrate && inside->bit_depth == c.bit_depth);
   assert(inside->ctb_size == c.ctb_size && inside->color == c.color);
   assert(inside->full_range == c.full_range);
   assert(!vulkan_encode_hevc_params(NULL, &c, STD_VIDEO_H265_LEVEL_IDC_6_2));
   assert(!vulkan_encode_hevc_params(p, NULL, STD_VIDEO_H265_LEVEL_IDC_6_2));
   free(alias);
   free(p);
   puts("HEVC parameter level/rate limits, crops, caller ownership and failure atomicity PASS");
   return 0;
}
