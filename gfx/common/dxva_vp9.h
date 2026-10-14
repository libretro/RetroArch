/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_DXVA_VP9_H
#define RARCH_DXVA_VP9_H
#include <formats/rvp9_hw.h>

#define DXVA_VP9_PP_SIZE 208
#define DXVA_VP9_SLICE_SIZE 10

static INLINE void dxva_vp9_put16(uint8_t *p, unsigned n)
{
   p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
}
static INLINE void dxva_vp9_put32(uint8_t *p, uint32_t n)
{
   p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
   p[2] = (uint8_t)(n >> 16); p[3] = (uint8_t)(n >> 24);
}

/* Fixed-size sessions: each valid reference has the current coded geometry.
 * Validate before writing so failures leave the packed SDK structure zero. */
static INLINE bool dxva_vp9_pic_params(uint8_t *out,
      const rvp9_hw_picture_t *p, uint32_t feedback)
{
   const rvp9_hdr *h;
   unsigned i, j, flags;
   if (!out) return false;
   memset(out, 0, DXVA_VP9_PP_SIZE);
   if (!p || !feedback) return false;
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
   flags = h->frame_type | h->show_frame << 1 | h->error_resilient << 2
         | 1u << 3 | 1u << 4 | h->refresh_frame_context << 6
         | h->frame_parallel_decoding << 7 | h->intra_only << 8
         | h->frame_context_idx << 9 | h->reset_frame_context << 11
         | h->allow_high_precision_mv << 13;
   out[0] = (uint8_t)p->slot; out[1] = (uint8_t)h->profile;
   dxva_vp9_put16(out + 2, flags);
   dxva_vp9_put32(out + 4, h->w); dxva_vp9_put32(out + 8, h->h);
   out[12] = out[13] = (uint8_t)(h->bit_depth - 8);
   out[14] = (uint8_t)h->interp_filter;
   for (i = 0; i < 8; i++)
   {
      out[16 + i] = p->reference_slots[i] < 0 ? 255 : (uint8_t)p->reference_slots[i];
      if (p->reference_slots[i] >= 0)
      {
         dxva_vp9_put32(out + 24 + i * 4, h->w);
         dxva_vp9_put32(out + 56 + i * 4, h->h);
      }
      for (j = 0; j < 4; j++)
      {
         dxva_vp9_put16(out + 120 + (i * 4 + j) * 2,
               (unsigned)h->seg_feature_data[i][j]);
         out[184 + i] |= h->seg_feature_enabled[i][j] << j;
      }
   }
   for (i = 0; i < 3; i++) out[88 + i] = p->named_slots[i] < 0 ? 255 : (uint8_t)p->named_slots[i];
   for (i = 0; i < 4; i++)
   {
      out[91 + i] = (uint8_t)h->ref_sign_bias[i];
      out[98 + i] = (uint8_t)h->lf_ref_deltas[i];
   }
   out[95] = (uint8_t)h->lf_level; out[96] = (uint8_t)h->lf_sharpness;
   out[97] = h->lf_delta_enabled | h->lf_delta_update << 1 | p->use_prev_frame_mvs << 2;
   for (i = 0; i < 2; i++) out[102 + i] = (uint8_t)h->lf_mode_deltas[i];
   dxva_vp9_put16(out + 104, h->base_qindex);
   out[106] = (uint8_t)h->y_dc_delta_q;
   out[107] = (uint8_t)h->uv_dc_delta_q; out[108] = (uint8_t)h->uv_ac_delta_q;
   out[109] = h->seg_enabled | h->seg_update_map << 1
         | h->seg_temporal_update << 2 | h->seg_abs_delta << 3;
   memcpy(out + 110, h->seg_tree_probs, 7); memcpy(out + 117, h->seg_pred_probs, 3);
   out[192] = (uint8_t)h->log2_tile_cols; out[193] = (uint8_t)h->log2_tile_rows;
   dxva_vp9_put16(out + 194, (unsigned)h->uncomp_size);
   dxva_vp9_put16(out + 196, h->first_partition_size);
   dxva_vp9_put32(out + 204, feedback);
   return true;
}
#endif
