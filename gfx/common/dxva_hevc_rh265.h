/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_DXVA_HEVC_RH265_H
#define RARCH_DXVA_HEVC_RH265_H
#include <formats/rh265.h>
#include "dxva_hevc.h"

/* Fill parameter fields while preserving picture POC, slots, RPS and IRAP
 * flags. Invalid or unsupported parameters leave the picture unchanged. */
static INLINE bool dxva_hevc_params_from_rh265(dxva_hevc_picture_t *picture,
      const rh265_video_params_t *s)
{
   dxva_hevc_picture_t p;
   uint8_t validation[DXVA_HEVC_PP_SIZE];
   if (!picture || !s || s->chroma_format_idc != 1 || s->pcm_enabled
         || s->tiles_enabled || s->dependent_slice_segments_enabled
         || s->num_extra_slice_header_bits < 0 || s->num_extra_slice_header_bits > 7)
      return false;
   p = *picture;
   p.width = s->width;
   p.height = s->height;
   p.min_cb_log2 = s->log2_min_cb;
   p.ctb_log2 = s->log2_ctb;
   p.min_tb_log2 = s->log2_min_tb;
   p.max_tb_log2 = s->log2_max_tb;
   p.bit_depth = s->bit_depth;
   p.max_poc_lsb_log2 = s->log2_max_poc_lsb;
   p.max_dec_pic_buffering = s->max_dec_pic_buffering;
   p.max_reorder_pics = s->max_num_reorder_pics;
   p.hierarchy_inter = s->max_transform_hierarchy_depth_inter;
   p.hierarchy_intra = s->max_transform_hierarchy_depth_intra;
   p.num_st_rps = s->num_st_rps;
   p.num_lt_rps = s->num_long_term_ref_pics;
   p.default_l0 = s->num_ref_idx_l0_default;
   p.default_l1 = s->num_ref_idx_l1_default;
   p.diff_qp_depth = s->diff_cu_qp_delta_depth;
   p.parallel_merge_log2 = s->log2_parallel_merge_level;
   p.init_qp = s->init_qp;
   p.cb_qp = s->cb_qp_offset;
   p.cr_qp = s->cr_qp_offset;
   p.beta_offset_div2 = s->beta_offset_div2;
   p.tc_offset_div2 = s->tc_offset_div2;
   p.coding_tools = (uint32_t)s->num_extra_slice_header_bits << 22;
   p.coding_tools |= (uint32_t)!!s->scaling_list_enabled << 0;
   p.coding_tools |= (uint32_t)!!s->amp_enabled << 1;
   p.coding_tools |= (uint32_t)!!s->sao_enabled << 2;
   p.coding_tools |= (uint32_t)!!s->long_term_ref_pics_present << 17;
   p.coding_tools |= (uint32_t)!!s->temporal_mvp_enabled << 18;
   p.coding_tools |= (uint32_t)!!s->strong_intra_smoothing << 19;
   p.coding_tools |= (uint32_t)!!s->output_flag_present << 21;
   p.coding_tools |= (uint32_t)!!s->sign_data_hiding << 25;
   p.coding_tools |= (uint32_t)!!s->cabac_init_present << 26;
   p.coding_settings &= UINT32_C(7) << 16;
   p.coding_settings |= (uint32_t)!!s->constrained_intra_pred << 0;
   p.coding_settings |= (uint32_t)!!s->transform_skip_enabled << 1;
   p.coding_settings |= (uint32_t)!!s->cu_qp_delta_enabled << 2;
   p.coding_settings |= (uint32_t)!!s->slice_chroma_qp_offsets_present << 3;
   p.coding_settings |= (uint32_t)!!s->weighted_pred << 4;
   p.coding_settings |= (uint32_t)!!s->weighted_bipred << 5;
   p.coding_settings |= (uint32_t)!!s->transquant_bypass_enabled << 6;
   p.coding_settings |= (uint32_t)!!s->entropy_coding_sync_enabled << 8;
   p.coding_settings |= (uint32_t)!!s->loop_filter_across_slices << 11;
   p.coding_settings |= (uint32_t)!!s->deblocking_filter_override_enabled << 12;
   p.coding_settings |= (uint32_t)!!s->deblocking_filter_disabled << 13;
   p.coding_settings |= (uint32_t)!!s->lists_modification_present << 14;
   p.coding_settings |= (uint32_t)!!s->slice_segment_header_extension_present << 15;
   if (!dxva_hevc_pic_params(validation, &p, 1)) return false;
   *picture = p;
   return true;
}

/* Enabled effective lists only. Caller owns the fixed submission buffer. */
static INLINE bool dxva_hevc_qmatrix_from_rh265(uint8_t *out,
      const rh265_video_params_t *s)
{
   dxva_hevc_scaling_t raster;
   if (!out) return false;
   if (!s || !s->scaling_list_enabled)
   {
      memset(out, 0, DXVA_HEVC_QM_SIZE);
      return false;
   }
   memcpy(raster.list4, s->list4, sizeof(raster.list4));
   memcpy(raster.list8, s->list8, sizeof(raster.list8));
   memcpy(raster.list16, s->list16, sizeof(raster.list16));
   memcpy(raster.list32, s->list32, sizeof(raster.list32));
   memcpy(raster.dc16, s->dc16, sizeof(raster.dc16));
   memcpy(raster.dc32, s->dc32, sizeof(raster.dc32));
   return dxva_hevc_qmatrix_raster(out, &raster);
}
#endif
