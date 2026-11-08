/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_vp9.h"

static rvp9_hw_picture_t source;
static VASurfaceID surfaces[10];
static vaapi_vp9_params_t params, unchanged;

static void reset(void)
{
   unsigned i;
   memset(&source, 0, sizeof(source));
   source.header.bit_depth = 8;
   source.header.subsampling_x = source.header.subsampling_y = 1;
   source.header.w = 1920;
   source.header.h = 1080;
   source.header.show_frame = 1;
   source.header.uncomp_size = 18;
   source.header.first_partition_size = 20;
   source.header.base_qindex = 100;
   source.header.lf_level = 32;
   source.header.interp_filter = 4;
   source.header.seg_tree_probs[0] = 83;
   source.header.seg_pred_probs[1] = 91;
   source.slot = 9;
   source.previous_slot = -1;
   for (i = 0; i < 10; i++) surfaces[i] = 100 + i;
   for (i = 0; i < 8; i++) source.reference_slots[i] = -1;
   for (i = 0; i < 3; i++) source.named_slots[i] = -1;
}

static void refuse(void)
{
   memset(&params, 0xa5, sizeof(params));
   memcpy(&unchanged, &params, sizeof(params));
   assert(!vaapi_vp9_picture(&params, &source, 128, surfaces, 10));
   assert(!memcmp(&params, &unchanged, sizeof(params)));
}

static void pictures(void)
{
   unsigned i;
   VASegmentParameterVP9 zero;
   reset();
   assert(vaapi_vp9_picture(&params, &source, 128, surfaces, 10));
   assert(params.target == 109);
   assert(params.picture.frame_width == 1920 && params.picture.frame_height == 1080);
   assert(params.picture.profile == 0 && params.picture.bit_depth == 8);
   assert(params.picture.frame_header_length_in_bytes == 18);
   assert(params.picture.first_partition_size == 20);
   assert(params.picture.pic_fields.bits.mcomp_filter_type == 4);
   assert(params.picture.pic_fields.bits.show_frame == 1);
   assert(!params.picture.pic_fields.bits.lossless_flag);
   assert(params.picture.mb_segment_tree_probs[0] == 83);
   assert(params.picture.segment_pred_probs[1] == 91);
   assert(params.slice.slice_data_size == 128 && !params.slice.slice_data_offset);
   assert(params.slice.slice_data_flag == VA_SLICE_DATA_FLAG_ALL);
   assert(params.slice.seg_param[0].luma_dc_quant_scale == 93);
   assert(params.slice.seg_param[0].luma_ac_quant_scale == 112);
   memset(&zero, 0, sizeof(zero));
   for (i = 1; i < 8; i++)
      assert(!memcmp(&zero, &params.slice.seg_param[i], sizeof(zero)));
   for (i = 0; i < 8; i++)
      assert(params.picture.reference_frames[i] == VA_INVALID_SURFACE);
   source.header.profile = 2;
   source.header.bit_depth = 10;
   source.header.frame_type = 1;
   source.header.frame_context_idx = 3;
   source.header.reset_frame_context = 2;
   source.header.ref_idx[0] = 7;
   source.header.ref_idx[1] = 2;
   source.header.ref_idx[2] = 0;
   source.header.ref_sign_bias[2] = 1;
   for (i = 0; i < 8; i++) source.reference_slots[i] = (int)i;
   source.named_slots[0] = 7;
   source.named_slots[1] = 2;
   source.named_slots[2] = 0;
   assert(vaapi_vp9_picture(&params, &source, 128, surfaces, 10));
   assert(params.picture.profile == 2 && params.picture.bit_depth == 10);
   assert(params.picture.pic_fields.bits.frame_type == 1);
   assert(params.picture.pic_fields.bits.frame_context_idx == 3);
   assert(params.picture.pic_fields.bits.reset_frame_context == 2);
   assert(params.picture.pic_fields.bits.last_ref_frame == 7);
   assert(params.picture.pic_fields.bits.golden_ref_frame == 2);
   assert(params.picture.pic_fields.bits.alt_ref_frame == 0);
   assert(params.picture.pic_fields.bits.golden_ref_frame_sign_bias == 1);
   for (i = 0; i < 8; i++) assert(params.picture.reference_frames[i] == 100 + i);
   source.header.seg_enabled = 1;
   source.header.seg_feature_enabled[4][0] = 1;
   source.header.seg_feature_data[4][0] = -255;
   source.header.seg_feature_enabled[4][2] = 1;
   source.header.seg_feature_data[4][2] = 3;
   source.header.seg_feature_enabled[4][3] = 1;
   assert(vaapi_vp9_picture(&params, &source, 128, surfaces, 10));
   assert(params.slice.seg_param[4].luma_dc_quant_scale == 4);
   assert(params.slice.seg_param[4].segment_flags.fields.segment_reference_enabled);
   assert(params.slice.seg_param[4].segment_flags.fields.segment_reference == 3);
   assert(params.slice.seg_param[4].segment_flags.fields.segment_reference_skipped);
   source.header.base_qindex = 0;
   assert(vaapi_vp9_picture(&params, &source, 128, surfaces, 10));
   assert(params.picture.pic_fields.bits.lossless_flag);
}

static void invalid(void)
{
   unsigned i;
   reset();
   assert(!vaapi_vp9_picture(NULL, &source, 128, surfaces, 10));
   memset(&params, 0xa5, sizeof(params));
   memcpy(&unchanged, &params, sizeof(params));
   assert(!vaapi_vp9_picture(&params, NULL, 128, surfaces, 10));
   assert(!vaapi_vp9_picture(&params, &source, 128, NULL, 10));
   assert(!vaapi_vp9_picture(&params, &source, 128, surfaces, 9));
   assert(!vaapi_vp9_picture(&params, &source, 128, surfaces, 33));
   assert(!vaapi_vp9_picture(&params, &source, 38, surfaces, 10));
   if (sizeof(size_t) > sizeof(unsigned))
      assert(!vaapi_vp9_picture(&params, &source, (size_t)-1, surfaces, 10));
   assert(!memcmp(&params, &unchanged, sizeof(params)));
   source.slot = -1; refuse();
   reset(); source.slot = 10; refuse();
   reset(); source.header.profile = 2; refuse();
   reset(); source.header.bit_depth = 12; refuse();
   reset(); source.header.show_existing_frame = 1; refuse();
   reset(); source.header.w = 65536; refuse();
   reset(); source.header.h = 0; refuse();
   reset(); source.header.subsampling_x = 0; refuse();
   reset(); source.header.uncomp_size = 256; refuse();
   reset(); source.header.first_partition_size = 0; refuse();
   reset(); source.header.first_partition_size = 65536; refuse();
   reset(); source.header.log2_tile_cols = 7; refuse();
   reset(); source.header.log2_tile_rows = 3; refuse();
   reset(); source.header.lf_sharpness = 8; refuse();
   reset(); source.header.lf_level = 64; refuse();
   reset(); source.header.base_qindex = 256; refuse();
   reset(); source.header.frame_context_idx = 4; refuse();
   reset(); source.header.interp_filter = -1; refuse();
   reset(); source.use_prev_frame_mvs = 1; refuse();
   reset(); surfaces[9] = VA_INVALID_SURFACE; refuse();
   reset(); surfaces[0] = surfaces[9]; refuse();
   for (i = 0; i < 8; i++)
   {
      reset(); source.reference_slots[i] = 10; refuse();
      reset(); source.reference_slots[i] = 9; refuse();
      reset(); source.reference_slots[i] = -2; refuse();
      reset(); source.reference_slots[i] = 0;
      surfaces[0] = VA_INVALID_SURFACE; refuse();
   }
   for (i = 0; i < 3; i++)
   {
      reset(); source.named_slots[i] = 0; refuse();
      reset(); source.header.frame_type = 1; source.header.ref_idx[i] = 8; refuse();
   }
}

int main(void)
{
   pictures();
   invalid();
   puts("VA VP9 picture and segment parameters passed");
   return 0;
}
