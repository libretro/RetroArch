/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_VP9_H
#define RARCH_VAAPI_VP9_H
#include <va/va.h>
#include <formats/rvp9_hw.h>
#include <limits.h>

typedef struct vaapi_vp9_params
{
   VADecPictureParameterBufferVP9 picture;
   VASliceParameterBufferVP9 slice;
   VASurfaceID target;
} vaapi_vp9_params_t;

/* Prepared logical slots resolve to live, distinct VA surfaces.
 * No VA calls or leases; failure leaves output unchanged. */
static INLINE bool vaapi_vp9_picture(vaapi_vp9_params_t *out,
      const rvp9_hw_picture_t *p, size_t bytes,
      const VASurfaceID *surfaces, unsigned slots)
{
   vaapi_vp9_params_t ready;
   rvp9_hw_segment_t segments[8];
   const rvp9_hdr *h;
   unsigned i, j;
   if (!out || !p || !surfaces || slots < 10 || slots > 32
         || p->slot < 0 || (unsigned)p->slot >= slots
         || !bytes || bytes > UINT_MAX)
      return false;
   h = &p->header;
   if (!((h->profile == 0 && h->bit_depth == 8)
            || (h->profile == 2 && h->bit_depth == 10))
         || h->show_existing_frame || h->subsampling_x != 1
         || h->subsampling_y != 1 || h->w < 1 || h->w > 65535
         || h->h < 1 || h->h > 65535 || (unsigned)h->frame_type > 1
         || (unsigned)h->show_frame > 1 || (unsigned)h->error_resilient > 1
         || (unsigned)h->intra_only > 1
         || (unsigned)h->allow_high_precision_mv > 1
         || (unsigned)h->interp_filter > 4
         || (unsigned)h->frame_parallel_decoding > 1
         || (unsigned)h->reset_frame_context > 3
         || (unsigned)h->refresh_frame_context > 1
         || (unsigned)h->frame_context_idx > 3
         || (unsigned)h->seg_temporal_update > 1
         || (unsigned)h->seg_update_map > 1
         || (unsigned)h->lf_sharpness > 7
         || (unsigned)h->log2_tile_cols > 6
         || (unsigned)h->log2_tile_rows > 2
         || !h->uncomp_size || h->uncomp_size > 255
         || h->uncomp_size >= bytes || h->first_partition_size < 1
         || h->first_partition_size > 65535
         || (size_t)h->first_partition_size >= bytes - h->uncomp_size
         || (unsigned)p->use_prev_frame_mvs > 1
         || (p->use_prev_frame_mvs && (p->previous_slot < 0
            || (unsigned)p->previous_slot >= slots))
         || surfaces[p->slot] == VA_INVALID_SURFACE)
      return false;
   for (i = 0; i < slots; i++)
      if (surfaces[i] != VA_INVALID_SURFACE)
         for (j = 0; j < i; j++)
            if (surfaces[i] == surfaces[j])
               return false;
   for (i = 0; i < 8; i++)
      if (p->reference_slots[i] < -1
            || p->reference_slots[i] == p->slot
            || (p->reference_slots[i] >= 0
               && ((unsigned)p->reference_slots[i] >= slots
                  || surfaces[p->reference_slots[i]] == VA_INVALID_SURFACE)))
         return false;
   for (i = 0; i < 3; i++)
      if (h->frame_type && !h->intra_only)
      {
         if ((unsigned)h->ref_idx[i] >= 8 || p->named_slots[i] < 0
               || p->named_slots[i] != p->reference_slots[h->ref_idx[i]])
            return false;
      }
      else if (p->named_slots[i] != -1)
         return false;
   for (i = 0; i < 4; i++)
      if ((unsigned)h->ref_sign_bias[i] > 1)
         return false;
   if (!rvp9_hw_segments(h, segments))
      return false;
   memset(&ready, 0, sizeof(ready));
   ready.target = surfaces[p->slot];
   ready.picture.frame_width = (uint16_t)h->w;
   ready.picture.frame_height = (uint16_t)h->h;
   for (i = 0; i < 8; i++)
      ready.picture.reference_frames[i] = p->reference_slots[i] < 0
            ? VA_INVALID_SURFACE : surfaces[p->reference_slots[i]];
   ready.picture.pic_fields.bits.subsampling_x = 1;
   ready.picture.pic_fields.bits.subsampling_y = 1;
   ready.picture.pic_fields.bits.frame_type = h->frame_type;
   ready.picture.pic_fields.bits.show_frame = h->show_frame;
   ready.picture.pic_fields.bits.error_resilient_mode = h->error_resilient;
   ready.picture.pic_fields.bits.intra_only = h->intra_only;
   ready.picture.pic_fields.bits.allow_high_precision_mv = h->allow_high_precision_mv;
   ready.picture.pic_fields.bits.mcomp_filter_type = h->interp_filter;
   ready.picture.pic_fields.bits.frame_parallel_decoding_mode = h->frame_parallel_decoding;
   ready.picture.pic_fields.bits.reset_frame_context = h->reset_frame_context;
   ready.picture.pic_fields.bits.refresh_frame_context = h->refresh_frame_context;
   ready.picture.pic_fields.bits.frame_context_idx = h->frame_context_idx;
   ready.picture.pic_fields.bits.segmentation_enabled = h->seg_enabled;
   ready.picture.pic_fields.bits.segmentation_temporal_update = h->seg_temporal_update;
   ready.picture.pic_fields.bits.segmentation_update_map = h->seg_update_map;
   if (h->frame_type && !h->intra_only)
   {
      ready.picture.pic_fields.bits.last_ref_frame = h->ref_idx[0];
      ready.picture.pic_fields.bits.golden_ref_frame = h->ref_idx[1];
      ready.picture.pic_fields.bits.alt_ref_frame = h->ref_idx[2];
      ready.picture.pic_fields.bits.last_ref_frame_sign_bias = h->ref_sign_bias[1];
      ready.picture.pic_fields.bits.golden_ref_frame_sign_bias = h->ref_sign_bias[2];
      ready.picture.pic_fields.bits.alt_ref_frame_sign_bias = h->ref_sign_bias[3];
   }
   ready.picture.pic_fields.bits.lossless_flag = !h->base_qindex
         && !h->y_dc_delta_q && !h->uv_dc_delta_q && !h->uv_ac_delta_q;
   ready.picture.filter_level = (uint8_t)h->lf_level;
   ready.picture.sharpness_level = (uint8_t)h->lf_sharpness;
   ready.picture.log2_tile_rows = (uint8_t)h->log2_tile_rows;
   ready.picture.log2_tile_columns = (uint8_t)h->log2_tile_cols;
   ready.picture.frame_header_length_in_bytes = (uint8_t)h->uncomp_size;
   ready.picture.first_partition_size = (uint16_t)h->first_partition_size;
   memcpy(ready.picture.mb_segment_tree_probs, h->seg_tree_probs, 7);
   memcpy(ready.picture.segment_pred_probs, h->seg_pred_probs, 3);
   ready.picture.profile = (uint8_t)h->profile;
   ready.picture.bit_depth = (uint8_t)h->bit_depth;
   ready.slice.slice_data_size = (uint32_t)bytes;
   ready.slice.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
   for (i = 0; i < (h->seg_enabled ? 8u : 1u); i++)
   {
      VASegmentParameterVP9 *seg = &ready.slice.seg_param[i];
      seg->segment_flags.fields.segment_reference_enabled =
            h->seg_enabled && h->seg_feature_enabled[i][2];
      seg->segment_flags.fields.segment_reference =
            seg->segment_flags.fields.segment_reference_enabled
            ? h->seg_feature_data[i][2] : 0;
      seg->segment_flags.fields.segment_reference_skipped =
            h->seg_enabled && h->seg_feature_enabled[i][3];
      memcpy(seg->filter_level, segments[i].filter_level, sizeof(seg->filter_level));
      seg->luma_ac_quant_scale = segments[i].y_quant[1];
      seg->luma_dc_quant_scale = segments[i].y_quant[0];
      seg->chroma_ac_quant_scale = segments[i].uv_quant[1];
      seg->chroma_dc_quant_scale = segments[i].uv_quant[0];
   }
   *out = ready;
   return true;
}
#endif
