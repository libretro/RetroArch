/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_HEVC_PARAMS_H
#define RARCH_VULKAN_HEVC_PARAMS_H
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>
#include <formats/rh265.h>
#include "../include/vk_video/vulkan_video_codec_h265std.h"

/* Caller-owned, stable storage: internal pointers refer into this object.
 * Allocate on the heap; do not copy after building. */
typedef struct vulkan_hevc_params
{
   StdVideoH265VideoParameterSet vps;
   StdVideoH265SequenceParameterSet sps;
   StdVideoH265PictureParameterSet pps;
   StdVideoH265ProfileTierLevel vps_ptl, sps_ptl;
   StdVideoH265DecPicBufMgr vps_dpb, sps_dpb;
   StdVideoH265ShortTermRefPicSet short_term[64];
   StdVideoH265LongTermRefPicsSps long_term;
   StdVideoH265ScalingLists scaling;
   rh265_video_params_t params;
   rh265_sequence_info_t sequence;
   rh265_vps_info_t video;
   rh265_short_term_set_t scratch;
} vulkan_hevc_params_t;

static INLINE bool vulkan_hevc_ptl(StdVideoH265ProfileTierLevel *out,
      const rh265_sequence_info_t *s)
{
   static const unsigned levels[13] = {30,60,63,90,93,120,123,150,153,156,180,183,186};
   unsigned i;
   if (s->profile_space || s->profile_idc < 1 || s->profile_idc > 3) return false;
   for (i = 0; i < 13 && levels[i] != s->level_idc; i++) {}
   if (i == 13) return false;
   out->general_profile_idc = (StdVideoH265ProfileIdc)s->profile_idc;
   out->general_level_idc = (StdVideoH265LevelIdc)i;
   out->flags.general_tier_flag = !!s->tier_flag;
   out->flags.general_progressive_source_flag = (s->constraint_flags_hi >> 31) & 1;
   out->flags.general_interlaced_source_flag = (s->constraint_flags_hi >> 30) & 1;
   out->flags.general_non_packed_constraint_flag = (s->constraint_flags_hi >> 29) & 1;
   out->flags.general_frame_only_constraint_flag = (s->constraint_flags_hi >> 28) & 1;
   return true;
}

static INLINE void vulkan_hevc_dpb(StdVideoH265DecPicBufMgr *out,
      const rh265_sequence_info_t *s)
{
   unsigned i;
   for (i = 0; i <= s->max_sub_layers_minus1; i++)
   {
      out->max_dec_pic_buffering_minus1[i] = (uint8_t)(s->max_dec_pic_buffering[i] - 1);
      out->max_num_reorder_pics[i] = (uint8_t)s->max_num_reorder_pics[i];
      out->max_latency_increase_plus1[i] = s->max_latency_increase_plus1[i];
   }
}

static INLINE void vulkan_hevc_scaling(vulkan_hevc_params_t *out)
{
   unsigned size, diagonal, x, y, n, m;
   const rh265_video_params_t *p = &out->params;
   /* StdVideo lists use diagonal scan order, rh265 exposes raster order. */
   for (size = 4; size <= 8; size += 4)
   {
      n = 0;
      for (diagonal = 0; diagonal < 2 * size - 1; diagonal++)
         for (x = 0; x < size; x++)
         {
            if (diagonal < x || diagonal - x >= size) continue;
            y = diagonal - x;
            for (m = 0; m < 6; m++)
               if (size == 4)
                  out->scaling.ScalingList4x4[m][n] = p->list4[m][y * size + x];
               else
               {
                  out->scaling.ScalingList8x8[m][n] = p->list8[m][y * size + x];
                  out->scaling.ScalingList16x16[m][n] = p->list16[m][y * size + x];
                  if (m < 2) out->scaling.ScalingList32x32[m][n] = p->list32[m][y * size + x];
               }
            n++;
         }
   }
   memcpy(out->scaling.ScalingListDCCoef16x16, p->dc16, sizeof(p->dc16));
   memcpy(out->scaling.ScalingListDCCoef32x32, p->dc32, sizeof(p->dc32));
}

static INLINE bool vulkan_hevc_short_term(StdVideoH265ShortTermRefPicSet *out,
      const rh265_short_term_set_t *in)
{
   unsigned list, j;
   memset(out, 0, sizeof(*out));
   if (in->num_before > 16 || in->num_after > 16) return false;
   out->num_negative_pics = (uint8_t)in->num_before;
   out->num_positive_pics = (uint8_t)in->num_after;
   for (list = 0; list < 2; list++)
   {
      int64_t prev = 0;
      unsigned count = list ? in->num_after : in->num_before;
      for (j = 0; j < count; j++)
      {
         int64_t current = list ? in->delta_after[j] : -(int64_t)in->delta_before[j];
         int64_t delta = current - prev - 1;
         unsigned used = list ? in->used_after[j] : in->used_before[j];
         if (delta < 0 || delta > 65535 || used > 1) return false;
         if (list)
         {
            out->delta_poc_s1_minus1[j] = (uint16_t)delta;
            out->used_by_curr_pic_s1_flag |= (uint16_t)(used << j);
         }
         else
         {
            out->delta_poc_s0_minus1[j] = (uint16_t)delta;
            out->used_by_curr_pic_s0_flag |= (uint16_t)(used << j);
         }
         prev = current;
      }
   }
   return true;
}

static INLINE bool vulkan_hevc_params_build(vulkan_hevc_params_t *out,
      const rh265_video *video, unsigned pps_id)
{
   rh265_video_params_t *p;
   rh265_sequence_info_t *q, *v;
   StdVideoH265SequenceParameterSet *s;
   StdVideoH265PictureParameterSet *r;
   unsigned i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (rh265_video_get_params(video, pps_id, &out->params)
         || rh265_video_get_sequence(video, pps_id, &out->sequence)
         || rh265_video_get_vps(video, pps_id, &out->video)) goto fail;
   p = &out->params; q = &out->sequence; v = &out->video.sequence;
   if (q->field_seq || p->pcm_enabled || p->tiles_enabled || p->dependent_slice_segments_enabled
         || p->max_transform_hierarchy_depth_inter > 4 || p->max_transform_hierarchy_depth_intra > 4
         || p->num_ref_idx_l0_default < 1 || p->num_ref_idx_l0_default > 15
         || p->num_ref_idx_l1_default < 1 || p->num_ref_idx_l1_default > 15
         || p->init_qp < -12 || p->init_qp > 51 || p->cb_qp_offset < -12 || p->cb_qp_offset > 12
         || p->cr_qp_offset < -12 || p->cr_qp_offset > 12
         || p->beta_offset_div2 < -6 || p->beta_offset_div2 > 6
         || p->tc_offset_div2 < -6 || p->tc_offset_div2 > 6
         || p->log2_parallel_merge_level < 2 || p->log2_parallel_merge_level > p->log2_ctb
         || q->max_sub_layers_minus1 > v->max_sub_layers_minus1
         || !vulkan_hevc_ptl(&out->vps_ptl, v) || !vulkan_hevc_ptl(&out->sps_ptl, q)) goto fail;
   vulkan_hevc_dpb(&out->vps_dpb, v); vulkan_hevc_dpb(&out->sps_dpb, q);
   out->vps.flags.vps_temporal_id_nesting_flag = !!v->temporal_id_nesting;
   out->vps.flags.vps_sub_layer_ordering_info_present_flag = 1;
   out->vps.flags.vps_timing_info_present_flag = !!out->video.timing_present;
   out->vps.flags.vps_poc_proportional_to_timing_flag = !!out->video.poc_proportional;
   out->vps.vps_video_parameter_set_id = (uint8_t)v->vps_id;
   out->vps.vps_max_sub_layers_minus1 = (uint8_t)v->max_sub_layers_minus1;
   out->vps.vps_num_units_in_tick = out->video.num_units_in_tick;
   out->vps.vps_time_scale = out->video.time_scale;
   out->vps.vps_num_ticks_poc_diff_one_minus1 = out->video.num_ticks_poc_diff_one_minus1;
   out->vps.pProfileTierLevel = &out->vps_ptl; out->vps.pDecPicBufMgr = &out->vps_dpb;
   s = &out->sps;
   s->flags.sps_temporal_id_nesting_flag = !!q->temporal_id_nesting;
   s->flags.sps_sub_layer_ordering_info_present_flag = 1;
   s->flags.conformance_window_flag = !!(p->crop_left || p->crop_right || p->crop_top || p->crop_bottom);
   s->flags.scaling_list_enabled_flag = !!p->scaling_list_enabled;
   s->flags.amp_enabled_flag = !!p->amp_enabled;
   s->flags.sample_adaptive_offset_enabled_flag = !!p->sao_enabled;
   s->flags.long_term_ref_pics_present_flag = !!p->long_term_ref_pics_present;
   s->flags.sps_temporal_mvp_enabled_flag = !!p->temporal_mvp_enabled;
   s->flags.strong_intra_smoothing_enabled_flag = !!p->strong_intra_smoothing;
   s->chroma_format_idc = STD_VIDEO_H265_CHROMA_FORMAT_IDC_420;
   s->pic_width_in_luma_samples = (uint32_t)p->width; s->pic_height_in_luma_samples = (uint32_t)p->height;
   s->sps_video_parameter_set_id = (uint8_t)q->vps_id;
   s->sps_seq_parameter_set_id = (uint8_t)p->sps_id;
   s->sps_max_sub_layers_minus1 = (uint8_t)q->max_sub_layers_minus1;
   s->bit_depth_luma_minus8 = s->bit_depth_chroma_minus8 = (uint8_t)(p->bit_depth - 8);
   s->log2_max_pic_order_cnt_lsb_minus4 = (uint8_t)(p->log2_max_poc_lsb - 4);
   s->log2_min_luma_coding_block_size_minus3 = (uint8_t)(p->log2_min_cb - 3);
   s->log2_diff_max_min_luma_coding_block_size = (uint8_t)(p->log2_ctb - p->log2_min_cb);
   s->log2_min_luma_transform_block_size_minus2 = (uint8_t)(p->log2_min_tb - 2);
   s->log2_diff_max_min_luma_transform_block_size = (uint8_t)(p->log2_max_tb - p->log2_min_tb);
   s->max_transform_hierarchy_depth_inter = (uint8_t)p->max_transform_hierarchy_depth_inter;
   s->max_transform_hierarchy_depth_intra = (uint8_t)p->max_transform_hierarchy_depth_intra;
   s->num_short_term_ref_pic_sets = (uint8_t)q->num_short_term_sets;
   s->num_long_term_ref_pics_sps = (uint8_t)q->num_long_term_pics;
   s->conf_win_left_offset = (uint32_t)p->crop_left / 2; s->conf_win_right_offset = (uint32_t)p->crop_right / 2;
   s->conf_win_top_offset = (uint32_t)p->crop_top / 2; s->conf_win_bottom_offset = (uint32_t)p->crop_bottom / 2;
   s->pProfileTierLevel = &out->sps_ptl; s->pDecPicBufMgr = &out->sps_dpb;
   s->pShortTermRefPicSet = q->num_short_term_sets ? out->short_term : NULL;
   s->pLongTermRefPicsSps = q->num_long_term_pics ? &out->long_term : NULL;
   for (i = 0; i < q->num_short_term_sets; i++)
   {
      StdVideoH265ShortTermRefPicSet *st = &out->short_term[i];
      if (rh265_video_get_short_term_set(video, pps_id, i, &out->scratch)) goto fail;
      if (!vulkan_hevc_short_term(st, &out->scratch)) goto fail;
   }
   for (i = 0; i < q->num_long_term_pics; i++)
   {
      out->long_term.lt_ref_pic_poc_lsb_sps[i] = q->lt_poc_lsb[i];
      out->long_term.used_by_curr_pic_lt_sps_flag |= (uint32_t)q->lt_used[i] << i;
   }
   r = &out->pps;
   r->flags.output_flag_present_flag = !!p->output_flag_present;
   r->flags.sign_data_hiding_enabled_flag = !!p->sign_data_hiding;
   r->flags.cabac_init_present_flag = !!p->cabac_init_present;
   r->flags.constrained_intra_pred_flag = !!p->constrained_intra_pred;
   r->flags.transform_skip_enabled_flag = !!p->transform_skip_enabled;
   r->flags.cu_qp_delta_enabled_flag = !!p->cu_qp_delta_enabled;
   r->flags.pps_slice_chroma_qp_offsets_present_flag = !!p->slice_chroma_qp_offsets_present;
   r->flags.weighted_pred_flag = !!p->weighted_pred;
   r->flags.weighted_bipred_flag = !!p->weighted_bipred;
   r->flags.transquant_bypass_enabled_flag = !!p->transquant_bypass_enabled;
   r->flags.entropy_coding_sync_enabled_flag = !!p->entropy_coding_sync_enabled;
   r->flags.pps_loop_filter_across_slices_enabled_flag = !!p->loop_filter_across_slices;
   r->flags.deblocking_filter_control_present_flag = !!p->deblocking_filter_control_present;
   r->flags.deblocking_filter_override_enabled_flag = !!p->deblocking_filter_override_enabled;
   r->flags.pps_deblocking_filter_disabled_flag = !!p->deblocking_filter_disabled;
   r->flags.lists_modification_present_flag = !!p->lists_modification_present;
   r->flags.slice_segment_header_extension_present_flag = !!p->slice_segment_header_extension_present;
   r->pps_pic_parameter_set_id = (uint8_t)p->pps_id; r->pps_seq_parameter_set_id = (uint8_t)p->sps_id;
   r->sps_video_parameter_set_id = (uint8_t)q->vps_id;
   r->num_extra_slice_header_bits = (uint8_t)p->num_extra_slice_header_bits;
   r->num_ref_idx_l0_default_active_minus1 = (uint8_t)(p->num_ref_idx_l0_default - 1);
   r->num_ref_idx_l1_default_active_minus1 = (uint8_t)(p->num_ref_idx_l1_default - 1);
   r->init_qp_minus26 = (int8_t)(p->init_qp - 26);
   r->diff_cu_qp_delta_depth = (uint8_t)p->diff_cu_qp_delta_depth;
   r->pps_cb_qp_offset = (int8_t)p->cb_qp_offset; r->pps_cr_qp_offset = (int8_t)p->cr_qp_offset;
   r->pps_beta_offset_div2 = (int8_t)p->beta_offset_div2; r->pps_tc_offset_div2 = (int8_t)p->tc_offset_div2;
   r->log2_parallel_merge_level_minus2 = (uint8_t)(p->log2_parallel_merge_level - 2);
   if (p->scaling_list_enabled)
   {
      vulkan_hevc_scaling(out);
      r->flags.pps_scaling_list_data_present_flag = 1; r->pScalingLists = &out->scaling;
   }
   return true;
fail:
   memset(out, 0, sizeof(*out));
   return false;
}
#endif
