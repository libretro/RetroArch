/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_ENCODE_HEVC_PARAMS_H
#define RARCH_VULKAN_ENCODE_HEVC_PARAMS_H
#include <stdint.h>
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>
#include "../include/vk_video/vulkan_video_codec_h265std.h"

enum vulkan_encode_hevc_color
{
   VULKAN_ENCODE_HEVC_UNSPECIFIED,
   VULKAN_ENCODE_HEVC_BT709,
   VULKAN_ENCODE_HEVC_PQ,
   VULKAN_ENCODE_HEVC_HLG
};
typedef struct vulkan_encode_hevc_config
{
   unsigned width, height, fps_num, fps_den, bitrate, bit_depth, ctb_size;
   enum vulkan_encode_hevc_color color;
   bool full_range;
} vulkan_encode_hevc_config_t;

/* Stable caller storage: VPS/SPS borrow this object's PTL, DPB and VUI.
 * Moving/copying it requires rebuilding those pointers. No HRD, B pictures,
 * tiles, long-term references or fabricated mastering/light metadata. */
typedef struct vulkan_encode_hevc_params
{
   StdVideoH265VideoParameterSet vps;
   StdVideoH265SequenceParameterSet sps;
   StdVideoH265PictureParameterSet pps;
   StdVideoH265SequenceParameterSetVui vui;
   StdVideoH265ProfileTierLevel ptl;
   StdVideoH265DecPicBufMgr dpb;
   StdVideoH265ShortTermRefPicSet reference;
   unsigned coded_width, coded_height;
} vulkan_encode_hevc_params_t;

/* Progressive Main/Main10 4:2:0, Main tier, one previous-picture reference.
 * Annex A limits are in luma samples, samples/second and bits/second. Device
 * profile, CTB/transform support, extents and actual rate control are separate
 * admission gates. Failure leaves caller storage untouched. */
static INLINE bool vulkan_encode_hevc_params(
      vulkan_encode_hevc_params_t *out,
      const vulkan_encode_hevc_config_t *config, StdVideoH265LevelIdc maximum)
{
   static const uint32_t limits[13][3] = {
      {36864, 552960, 128000}, {122880, 3686400, 1500000},
      {245760, 7372800, 3000000}, {552960, 16588800, 6000000},
      {983040, 33177600, 10000000}, {2228224, 66846720, 12000000},
      {2228224, 133693440, 20000000}, {8912896, 267386880, 25000000},
      {8912896, 534773760, 40000000}, {8912896, 1069547520, 60000000},
      {35651584, 1069547520, 60000000}, {35651584, 2139095040u, 120000000},
      {35651584, 4278190080u, 240000000}
   };
   static const uint8_t colors[4][3] = {
      {2, 2, 2}, {1, 1, 1}, {9, 16, 9}, {9, 18, 9}
   };
   uintptr_t a = (uintptr_t)out, b = (uintptr_t)config;
   unsigned width, height, level, samples, ctb_log2;
   if (!out || !config ||
         (a <= b ? b - a < sizeof(*out) : a - b < sizeof(*config)) ||
         !config->width || config->width > 8192 || (config->width & 1) ||
         !config->height || config->height > 8192 || (config->height & 1) ||
         !config->fps_num || !config->fps_den || !config->bitrate ||
         (config->bit_depth != 8 && config->bit_depth != 10) ||
         (config->ctb_size != 16 && config->ctb_size != 32 && config->ctb_size != 64) ||
         (unsigned)config->color > VULKAN_ENCODE_HEVC_HLG ||
         (config->color >= VULKAN_ENCODE_HEVC_PQ && config->bit_depth != 10) ||
         maximum < STD_VIDEO_H265_LEVEL_IDC_1_0 || maximum > STD_VIDEO_H265_LEVEL_IDC_6_2)
      return false;
   width = (config->width + 7) & ~7u;
   height = (config->height + 7) & ~7u;
   samples = width * height;
   for (level = 0; level <= (unsigned)maximum; level++)
      if (samples <= limits[level][0] &&
            (uint64_t)width * width <= (uint64_t)8 * limits[level][0] &&
            (uint64_t)height * height <= (uint64_t)8 * limits[level][0] &&
            (uint64_t)samples * config->fps_num <= (uint64_t)limits[level][1] * config->fps_den &&
            config->bitrate <= limits[level][2])
         break;
   if (level > (unsigned)maximum)
      return false;
   ctb_log2 = config->ctb_size == 64 ? 6 : config->ctb_size == 32 ? 5 : 4;
   memset(out, 0, sizeof(*out));
   out->coded_width = width;
   out->coded_height = height;
   out->ptl.general_profile_idc = config->bit_depth == 10
      ? STD_VIDEO_H265_PROFILE_IDC_MAIN_10 : STD_VIDEO_H265_PROFILE_IDC_MAIN;
   out->ptl.general_level_idc = (StdVideoH265LevelIdc)level;
   out->ptl.flags.general_progressive_source_flag = 1;
   out->ptl.flags.general_non_packed_constraint_flag = 1;
   out->ptl.flags.general_frame_only_constraint_flag = 1;
   out->dpb.max_dec_pic_buffering_minus1[0] = 1;
   out->vps.flags.vps_temporal_id_nesting_flag = 1;
   out->vps.flags.vps_timing_info_present_flag = 1;
   out->vps.flags.vps_poc_proportional_to_timing_flag = 1;
   out->vps.vps_num_units_in_tick = config->fps_den;
   out->vps.vps_time_scale = config->fps_num;
   out->vps.pDecPicBufMgr = &out->dpb;
   out->vps.pProfileTierLevel = &out->ptl;
   out->sps.chroma_format_idc = STD_VIDEO_H265_CHROMA_FORMAT_IDC_420;
   out->sps.pic_width_in_luma_samples = width;
   out->sps.pic_height_in_luma_samples = height;
   out->sps.bit_depth_luma_minus8 = out->sps.bit_depth_chroma_minus8 = config->bit_depth - 8;
   out->sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
   out->sps.log2_diff_max_min_luma_coding_block_size = ctb_log2 - 3;
   out->sps.log2_diff_max_min_luma_transform_block_size = ctb_log2 == 4 ? 2 : 3;
   out->sps.max_transform_hierarchy_depth_inter = ctb_log2 - 2;
   out->sps.max_transform_hierarchy_depth_intra = ctb_log2 - 2;
   out->sps.flags.sps_temporal_id_nesting_flag = 1;
   out->sps.flags.conformance_window_flag = width != config->width || height != config->height;
   out->sps.conf_win_right_offset = (width - config->width) / 2;
   out->sps.conf_win_bottom_offset = (height - config->height) / 2;
   out->sps.flags.vui_parameters_present_flag = 1;
   out->sps.num_short_term_ref_pic_sets = 1;
   out->sps.pProfileTierLevel = &out->ptl;
   out->sps.pDecPicBufMgr = &out->dpb;
   out->sps.pSequenceParameterSetVui = &out->vui;
   out->sps.pShortTermRefPicSet = &out->reference;
   out->reference.num_negative_pics = 1;
   out->reference.used_by_curr_pic_s0_flag = 1;
   out->pps.flags.deblocking_filter_control_present_flag = 1;
   out->pps.flags.pps_loop_filter_across_slices_enabled_flag = 1;
   out->vui.flags.aspect_ratio_info_present_flag = 1;
   out->vui.flags.video_signal_type_present_flag = 1;
   out->vui.flags.colour_description_present_flag = config->color != VULKAN_ENCODE_HEVC_UNSPECIFIED;
   out->vui.flags.video_full_range_flag = config->full_range;
   out->vui.flags.vui_timing_info_present_flag = 1;
   out->vui.flags.vui_poc_proportional_to_timing_flag = 1;
   out->vui.aspect_ratio_idc = STD_VIDEO_H265_ASPECT_RATIO_IDC_SQUARE;
   out->vui.video_format = 5;
   out->vui.colour_primaries = colors[config->color][0];
   out->vui.transfer_characteristics = colors[config->color][1];
   out->vui.matrix_coeffs = colors[config->color][2];
   out->vui.vui_num_units_in_tick = config->fps_den;
   out->vui.vui_time_scale = config->fps_num;
   return true;
}
#endif
