/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vaapi_vp9_picture.h"
static rvp9_hw_picture_t source;
static VASurfaceID surfaces[10];
static vaapi_vp9_picture_t output, unchanged;
static void init(void)
{
   unsigned i;
   memset(&source, 0, sizeof(source));
   source.header.bit_depth = 8;
   source.header.subsampling_x = source.header.subsampling_y = 1;
   source.header.w = 64;
   source.header.h = 32;
   source.header.show_frame = 1;
   source.header.uncomp_size = 16;
   source.header.first_partition_size = 7;
   for (i = 0; i < 10; i++)
      surfaces[i] = 100 + i;
   for (i = 0; i < 8; i++)
      source.reference_slots[i] = -1;
   for (i = 0; i < 3; i++)
      source.named_slots[i] = -1;
}
static void refuses(size_t size)
{
   unchanged = output;
   assert(!vaapi_vp9_picture(&output, &source, size, surfaces, 10));
   assert(!memcmp(&output, &unchanged, sizeof(output)));
}
int main(void)
{
   unsigned i;
   init();
   assert(vaapi_vp9_picture(&output, &source, 100, surfaces, 10));
   assert(
         output.picture.frame_width == 64 && output.picture.frame_height == 32);
   assert(output.picture.pic_fields.bits.lossless_flag);
   assert(output.picture.frame_header_length_in_bytes == 16);
   assert(output.slice.slice_data_size == 100
         && !output.slice.slice_data_offset);
   assert(output.slice.slice_data_flag == VA_SLICE_DATA_FLAG_ALL);
   assert(output.slice.seg_param[0].luma_dc_quant_scale == 4);
   for (i = 1; i < 8; i++)
   {
      VASegmentParameterVP9 zero;
      memset(&zero, 0, sizeof(zero));
      assert(!memcmp(&zero, &output.slice.seg_param[i], sizeof(zero)));
   }
   for (i = 0; i < 8; i++)
      assert(output.picture.reference_frames[i] == VA_INVALID_SURFACE);
   for (i = 0; i < 3; i++)
      assert(output.picture.segment_pred_probs[i] == 255);
   source.header.profile = 2;
   source.header.bit_depth = 10;
   source.header.base_qindex = 255;
   source.header.seg_enabled = 1;
   source.header.lf_level = 32;
   source.header.lf_delta_enabled = 1;
   source.header.lf_ref_deltas[1] = 10;
   source.header.lf_mode_deltas[0] = -3;
   source.header.seg_feature_enabled[2][0] = 1;
   source.header.seg_feature_data[2][0] = -255;
   source.header.seg_feature_enabled[2][1] = 1;
   source.header.seg_feature_data[2][1] = -32;
   source.header.seg_feature_enabled[2][2] = 1;
   source.header.seg_feature_data[2][2] = 3;
   source.header.seg_feature_enabled[2][3] = 1;
   source.header.seg_temporal_update = 1;
   source.header.seg_update_map = 1;
   source.header.seg_pred_probs[0] = 19;
   source.header.frame_type = 1;
   for (i = 0; i < 3; i++)
   {
      source.header.ref_idx[i] = (int)i;
      source.reference_slots[i] = (int)i + 1;
      source.named_slots[i] = (int)i + 1;
   }
   assert(vaapi_vp9_picture(&output, &source, 100, surfaces, 10));
   assert(output.picture.profile == 2 && output.picture.bit_depth == 10);
   assert(output.picture.reference_frames[1] == 102);
   assert(output.picture.pic_fields.bits.golden_ref_frame == 1);
   assert(output.picture.segment_pred_probs[0] == 19);
   assert(output.slice.seg_param[0].luma_dc_quant_scale == 5347);
   assert(output.slice.seg_param[0].luma_ac_quant_scale == 7312);
   assert(output.slice.seg_param[2].luma_ac_quant_scale == 4);
   assert(output.slice.seg_param[2].filter_level[1][0] == 14);
   assert(
         output.slice.seg_param[2].segment_flags.fields.segment_reference == 3);
   assert(output.slice.seg_param[2]
               .segment_flags.fields.segment_reference_skipped);
   source.header.uncomp_size = 256;
   refuses(300);
   source.header.uncomp_size = 16;
   refuses(23);
   source.header.w = 65536;
   refuses(100);
   source.header.w = 64;
   source.reference_slots[0] = 0;
   refuses(100);
   source.reference_slots[0] = 1;
   source.named_slots[0] = 2;
   refuses(100);
   source.named_slots[0] = 1;
   surfaces[1] = surfaces[0];
   refuses(100);
   surfaces[1] = 101;
   source.header.show_existing_frame = 1;
   refuses(100);
   source.header.show_existing_frame = 0;
   source.header.seg_feature_data[7][0] = 256;
   refuses(100);
   puts("PASS: VA VP9 coded-frame bounds, DPB mapping and segment parameters");
   return 0;
}
