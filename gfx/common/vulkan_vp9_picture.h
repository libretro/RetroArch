/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_VP9_PICTURE_H
#define RARCH_VULKAN_VP9_PICTURE_H
#include <formats/rvp9_hw.h>
#include "../include/vulkan/vulkan.h"

typedef struct vulkan_vp9_picture
{
   StdVideoDecodeVP9PictureInfo picture;
   StdVideoVP9ColorConfig color;
   StdVideoVP9LoopFilter filter;
   StdVideoVP9Segmentation segmentation;
   VkVideoDecodeVP9PictureInfoKHR info;
} vulkan_vp9_picture_t;

static INLINE bool vulkan_vp9_picture(vulkan_vp9_picture_t *out,
      const rvp9_hw_picture_t *p, size_t size)
{
   const rvp9_hdr *h;
   unsigned i, j;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!p) return false;
   h = &p->header;
   if (!((h->profile == 0 && h->bit_depth == 8)
            || (h->profile == 2 && h->bit_depth == 10))
         || h->show_existing_frame || h->subsampling_x != 1 || h->subsampling_y != 1
         || h->w <= 0 || h->w > 65536 || h->h <= 0 || h->h > 65536
         || p->slot < 0 || p->slot >= 32
         || (unsigned)h->frame_type > 1 || (unsigned)h->show_frame > 1
         || (unsigned)h->error_resilient > 1 || (unsigned)h->intra_only > 1
         || (unsigned)h->refresh_frame_context > 1 || (unsigned)h->frame_parallel_decoding > 1
         || (unsigned)h->frame_context_idx > 3 || (unsigned)h->reset_frame_context > 3
         || (unsigned)h->allow_high_precision_mv > 1 || (unsigned)h->interp_filter > 4
         || (unsigned)h->lf_level > 63 || (unsigned)h->lf_sharpness > 7
         || (unsigned)h->lf_delta_enabled > 1 || (unsigned)h->lf_delta_update > 1
         || (unsigned)p->use_prev_frame_mvs > 1
         || (p->use_prev_frame_mvs && (p->previous_slot < 0 || p->previous_slot >= 32))
         || (unsigned)h->base_qindex > 255
         || h->y_dc_delta_q < -15 || h->y_dc_delta_q > 15
         || h->uv_dc_delta_q < -15 || h->uv_dc_delta_q > 15
         || h->uv_ac_delta_q < -15 || h->uv_ac_delta_q > 15
         || (unsigned)h->seg_enabled > 1 || (unsigned)h->seg_update_map > 1
         || (unsigned)h->seg_temporal_update > 1 || (unsigned)h->seg_abs_delta > 1
         || (unsigned)h->log2_tile_cols > 6 || (unsigned)h->log2_tile_rows > 2
         || !h->uncomp_size || h->uncomp_size > 65535
         || h->first_partition_size < 1 || h->first_partition_size > 65535)
      return false;
   for (i = 0; i < 8; i++)
   {
      if (p->reference_slots[i] < -1 || p->reference_slots[i] >= 32
            || p->reference_slots[i] == p->slot) return false;
      for (j = 0; j < 4; j++)
      {
         static const int max_data[4] = {255, 63, 3, 0};
         if ((unsigned)h->seg_feature_enabled[i][j] > 1
               || h->seg_feature_data[i][j] < -max_data[j]
               || h->seg_feature_data[i][j] > max_data[j]
               || (j == 2 && h->seg_feature_data[i][j] < 0)) return false;
      }
   }
   for (i = 0; i < 3; i++)
      if (h->frame_type && !h->intra_only)
      {
         if ((unsigned)h->ref_idx[i] >= 8 || p->named_slots[i] < 0
               || p->named_slots[i] != p->reference_slots[h->ref_idx[i]]) return false;
      }
      else if (p->named_slots[i] != -1) return false;
   for (i = 0; i < 4; i++)
      if ((unsigned)h->ref_sign_bias[i] > 1
            || h->lf_ref_deltas[i] < -63 || h->lf_ref_deltas[i] > 63) return false;
   for (i = 0; i < 2; i++)
      if (h->lf_mode_deltas[i] < -63 || h->lf_mode_deltas[i] > 63) return false;
   if (size > UINT32_MAX || h->uncomp_size > size
         || (size_t)h->first_partition_size >= size - h->uncomp_size
         || (unsigned)h->color_space > 7 || (unsigned)h->color_range > 1
         || (unsigned)h->refresh_frame_flags > 255
         || (unsigned)h->seg_update_data > 1) return false;
   out->picture.flags.error_resilient_mode = h->error_resilient;
   out->picture.flags.intra_only = h->intra_only;
   out->picture.flags.allow_high_precision_mv = h->allow_high_precision_mv;
   out->picture.flags.refresh_frame_context = h->refresh_frame_context;
   out->picture.flags.frame_parallel_decoding_mode = h->frame_parallel_decoding;
   out->picture.flags.segmentation_enabled = h->seg_enabled;
   out->picture.flags.show_frame = h->show_frame;
   out->picture.flags.UsePrevFrameMvs = p->use_prev_frame_mvs;
   out->picture.profile = (StdVideoVP9Profile)h->profile;
   out->picture.frame_type = (StdVideoVP9FrameType)h->frame_type;
   out->picture.frame_context_idx = (uint8_t)h->frame_context_idx;
   out->picture.reset_frame_context = (uint8_t)h->reset_frame_context;
   out->picture.refresh_frame_flags = (uint8_t)h->refresh_frame_flags;
   out->picture.interpolation_filter = (StdVideoVP9InterpolationFilter)h->interp_filter;
   out->picture.base_q_idx = (uint8_t)h->base_qindex;
   out->picture.delta_q_y_dc = (int8_t)h->y_dc_delta_q;
   out->picture.delta_q_uv_dc = (int8_t)h->uv_dc_delta_q;
   out->picture.delta_q_uv_ac = (int8_t)h->uv_ac_delta_q;
   out->picture.tile_cols_log2 = (uint8_t)h->log2_tile_cols;
   out->picture.tile_rows_log2 = (uint8_t)h->log2_tile_rows;
   out->picture.pColorConfig = &out->color;
   out->picture.pLoopFilter = &out->filter;
   out->picture.pSegmentation = &out->segmentation;
   out->color.flags.color_range = h->color_range;
   out->color.BitDepth = (uint8_t)h->bit_depth;
   out->color.subsampling_x = out->color.subsampling_y = 1;
   out->color.color_space = (StdVideoVP9ColorSpace)h->color_space;
   out->filter.flags.loop_filter_delta_enabled = h->lf_delta_enabled;
   out->filter.flags.loop_filter_delta_update = h->lf_delta_update;
   out->filter.loop_filter_level = (uint8_t)h->lf_level;
   out->filter.loop_filter_sharpness = (uint8_t)h->lf_sharpness;
   for (i = 0; i < 4; i++)
   {
      out->picture.ref_frame_sign_bias_mask |= h->ref_sign_bias[i] << i;
      out->filter.loop_filter_ref_deltas[i] = (int8_t)h->lf_ref_deltas[i];
      if (h->lf_ref_delta_upd[i]) out->filter.update_ref_delta |= 1u << i;
   }
   for (i = 0; i < 2; i++)
   {
      out->filter.loop_filter_mode_deltas[i] = (int8_t)h->lf_mode_deltas[i];
      if (h->lf_mode_delta_upd[i]) out->filter.update_mode_delta |= 1u << i;
   }
   out->segmentation.flags.segmentation_update_map = h->seg_update_map;
   out->segmentation.flags.segmentation_temporal_update = h->seg_temporal_update;
   out->segmentation.flags.segmentation_update_data = h->seg_update_data;
   out->segmentation.flags.segmentation_abs_or_delta_update = h->seg_abs_delta;
   memcpy(out->segmentation.segmentation_tree_probs, h->seg_tree_probs, RVP9_SEG_TREE_PROBS);
   memcpy(out->segmentation.segmentation_pred_prob, h->seg_pred_probs, RVP9_PRED_PROBS);
   for (i = 0; i < RVP9_MAX_SEGMENTS; i++)
      for (j = 0; j < RVP9_SEG_LVL_MAX; j++)
      {
         if (h->seg_feature_enabled[i][j]) out->segmentation.FeatureEnabled[i] |= 1u << j;
         out->segmentation.FeatureData[i][j] = (int16_t)h->seg_feature_data[i][j];
      }
   out->info.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_PICTURE_INFO_KHR;
   out->info.pStdPictureInfo = &out->picture;
   for (i = 0; i < RVP9_REFS_PER_FRAME; i++)
      out->info.referenceNameSlotIndices[i] = p->named_slots[i];
   out->info.compressedHeaderOffset = (uint32_t)h->uncomp_size;
   out->info.tilesOffset = (uint32_t)(h->uncomp_size + h->first_partition_size);
   return true;
}
#endif
