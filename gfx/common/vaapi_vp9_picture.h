/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_VP9_PICTURE_H
#define RARCH_VAAPI_VP9_PICTURE_H
#include <va/va.h>
#include <formats/rvp9_hw.h>

typedef struct vaapi_vp9_picture
{
   VADecPictureParameterBufferVP9 picture;
   VASliceParameterBufferVP9 slice;
} vaapi_vp9_picture_t;

/* Decoder owner, effective inherited syntax and live surface IDs. No VA calls.
 * Whole coded frame, without a superframe index. Refusal leaves out unchanged.
 * show_existing_frame is served by the session's retained reference surface. */
static INLINE bool vaapi_vp9_picture(vaapi_vp9_picture_t *out,
      const rvp9_hw_picture_t *source, size_t size, const VASurfaceID *surfaces,
      unsigned slots)
{
   vaapi_vp9_picture_t value;
   const rvp9_hdr *h;
   unsigned i;
   if (!out || !source || !surfaces || slots < 10 || slots > 32)
      return false;
   h = &source->header;
   if (!((h->profile == 0 && h->bit_depth == 8)
             || (h->profile == 2 && h->bit_depth == 10))
         || h->show_existing_frame || h->subsampling_x != 1
         || h->subsampling_y != 1 || h->w < 1 || h->w > 65535 || h->h < 1
         || h->h > 65535 || source->slot < 0 || (unsigned)source->slot >= slots
         || surfaces[source->slot] == VA_INVALID_SURFACE
         || (unsigned)h->frame_type > 1 || (unsigned)h->show_frame > 1
         || (unsigned)h->error_resilient > 1 || (unsigned)h->intra_only > 1
         || (unsigned)h->allow_high_precision_mv > 1
         || (unsigned)h->interp_filter > 4
         || (unsigned)h->frame_parallel_decoding > 1
         || (unsigned)h->reset_frame_context > 3
         || (unsigned)h->refresh_frame_context > 1
         || (unsigned)h->frame_context_idx > 3
         || (unsigned)h->seg_temporal_update > 1
         || (unsigned)h->seg_update_map > 1 || (unsigned)h->lf_sharpness > 7
         || (unsigned)h->log2_tile_cols > 6 || (unsigned)h->log2_tile_rows > 2
         || !h->uncomp_size || h->uncomp_size > 255
         || h->first_partition_size < 1 || h->first_partition_size > 65535
         || size > UINT32_MAX || h->uncomp_size >= size
         || (size_t)h->first_partition_size >= size - h->uncomp_size)
      return false;
   memset(&value, 0, sizeof(value));
   for (i = 0; i < RVP9_REF_FRAMES; i++)
   {
      int slot = source->reference_slots[i];
      if (slot < -1 || slot == source->slot
            || (slot >= 0
                  && ((unsigned)slot >= slots
                        || surfaces[slot] == VA_INVALID_SURFACE
                        || surfaces[slot] == surfaces[source->slot])))
         return false;
      value.picture.reference_frames[i]
            = slot < 0 ? VA_INVALID_SURFACE : surfaces[slot];
   }
   for (i = 0; i < RVP9_REFS_PER_FRAME; i++)
      if (h->frame_type && !h->intra_only)
      {
         if ((unsigned)h->ref_idx[i] >= RVP9_REF_FRAMES
               || source->named_slots[i] < 0
               || source->named_slots[i]
                     != source->reference_slots[h->ref_idx[i]])
            return false;
      }
      else if (source->named_slots[i] != -1)
         return false;
   for (i = 0; i < 4; i++)
      if ((unsigned)h->ref_sign_bias[i] > 1)
         return false;
   value.picture.frame_width = (uint16_t)h->w;
   value.picture.frame_height = (uint16_t)h->h;
   value.picture.pic_fields.bits.subsampling_x = 1;
   value.picture.pic_fields.bits.subsampling_y = 1;
   value.picture.pic_fields.bits.frame_type = h->frame_type;
   value.picture.pic_fields.bits.show_frame = h->show_frame;
   value.picture.pic_fields.bits.error_resilient_mode = h->error_resilient;
   value.picture.pic_fields.bits.intra_only = h->intra_only;
   value.picture.pic_fields.bits.allow_high_precision_mv
         = h->allow_high_precision_mv;
   value.picture.pic_fields.bits.mcomp_filter_type = h->interp_filter;
   value.picture.pic_fields.bits.frame_parallel_decoding_mode
         = h->frame_parallel_decoding;
   value.picture.pic_fields.bits.reset_frame_context = h->reset_frame_context;
   value.picture.pic_fields.bits.refresh_frame_context
         = h->refresh_frame_context;
   value.picture.pic_fields.bits.frame_context_idx = h->frame_context_idx;
   value.picture.pic_fields.bits.segmentation_enabled = h->seg_enabled;
   value.picture.pic_fields.bits.segmentation_temporal_update
         = h->seg_temporal_update;
   value.picture.pic_fields.bits.segmentation_update_map = h->seg_update_map;
   if (h->frame_type && !h->intra_only)
   {
      value.picture.pic_fields.bits.last_ref_frame = h->ref_idx[0];
      value.picture.pic_fields.bits.golden_ref_frame = h->ref_idx[1];
      value.picture.pic_fields.bits.alt_ref_frame = h->ref_idx[2];
      value.picture.pic_fields.bits.last_ref_frame_sign_bias
            = h->ref_sign_bias[1];
      value.picture.pic_fields.bits.golden_ref_frame_sign_bias
            = h->ref_sign_bias[2];
      value.picture.pic_fields.bits.alt_ref_frame_sign_bias
            = h->ref_sign_bias[3];
   }
   value.picture.pic_fields.bits.lossless_flag = !h->base_qindex
         && !h->y_dc_delta_q && !h->uv_dc_delta_q && !h->uv_ac_delta_q;
   value.picture.filter_level = (uint8_t)h->lf_level;
   value.picture.sharpness_level = (uint8_t)h->lf_sharpness;
   value.picture.log2_tile_rows = (uint8_t)h->log2_tile_rows;
   value.picture.log2_tile_columns = (uint8_t)h->log2_tile_cols;
   value.picture.frame_header_length_in_bytes = (uint8_t)h->uncomp_size;
   value.picture.first_partition_size = (uint16_t)h->first_partition_size;
   value.picture.profile = (uint8_t)h->profile;
   value.picture.bit_depth = (uint8_t)h->bit_depth;
   memcpy(value.picture.mb_segment_tree_probs, h->seg_tree_probs,
         sizeof(value.picture.mb_segment_tree_probs));
   if (h->seg_temporal_update)
      memcpy(value.picture.segment_pred_probs, h->seg_pred_probs,
            sizeof(value.picture.segment_pred_probs));
   else
      memset(value.picture.segment_pred_probs, 255,
            sizeof(value.picture.segment_pred_probs));
   value.slice.slice_data_size = (uint32_t)size;
   value.slice.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
   for (i = 0; i < (h->seg_enabled ? RVP9_MAX_SEGMENTS : 1); i++)
   {
      rvp9_hw_segment_t segment;
      VASegmentParameterVP9 *native = &value.slice.seg_param[i];
      if (!rvp9_hw_segment(h, i, &segment))
         return false;
      if (h->seg_enabled)
      {
         native->segment_flags.fields.segment_reference_enabled
               = h->seg_feature_enabled[i][2];
         native->segment_flags.fields.segment_reference
               = h->seg_feature_data[i][2];
         native->segment_flags.fields.segment_reference_skipped
               = h->seg_feature_enabled[i][3];
      }
      native->luma_dc_quant_scale = segment.y_dc;
      native->luma_ac_quant_scale = segment.y_ac;
      native->chroma_dc_quant_scale = segment.uv_dc;
      native->chroma_ac_quant_scale = segment.uv_ac;
      memcpy(
            native->filter_level, segment.filter, sizeof(native->filter_level));
   }
   *out = value;
   return true;
}
#endif
