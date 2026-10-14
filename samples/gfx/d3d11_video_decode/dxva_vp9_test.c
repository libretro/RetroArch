/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <dxva.h>
#endif
#include "../../../gfx/common/dxva_vp9.h"

static rvp9_hw_picture_t p;
static uint8_t bytes[DXVA_VP9_PP_SIZE + 1];
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #c); failures++; } } while (0)

static void setup(void)
{
   unsigned i;
   rvp9_hdr *h = &p.header;
   memset(&p, 0, sizeof(p));
   h->profile = 2; h->bit_depth = 10; h->subsampling_x = h->subsampling_y = 1;
   h->w = 1920; h->h = 1080; h->frame_type = h->show_frame = 1;
   h->refresh_frame_context = 1; h->frame_context_idx = 2;
   h->reset_frame_context = 3; h->allow_high_precision_mv = 1; h->interp_filter = 4;
   h->lf_level = 32; h->lf_sharpness = 6; h->lf_delta_enabled = h->lf_delta_update = 1;
   h->lf_ref_deltas[0] = 1; h->lf_ref_deltas[2] = -2; h->lf_ref_deltas[3] = -3;
   h->lf_mode_deltas[0] = -4; h->lf_mode_deltas[1] = 5;
   h->base_qindex = 173; h->y_dc_delta_q = -7; h->uv_dc_delta_q = 8; h->uv_ac_delta_q = -9;
   h->seg_enabled = h->seg_update_map = h->seg_temporal_update = h->seg_abs_delta = 1;
   h->log2_tile_cols = 2; h->log2_tile_rows = 1;
   h->uncomp_size = 43; h->first_partition_size = 267;
   p.slot = 9; p.previous_slot = 8; p.use_prev_frame_mvs = 1;
   for (i = 0; i < 8; i++)
   {
      p.reference_slots[i] = i < 4 ? (int)i + 1 : -1;
      h->seg_feature_enabled[i][0] = 1; h->seg_feature_data[i][0] = -255 + i;
      h->seg_feature_enabled[i][1] = 1; h->seg_feature_data[i][1] = 63 - i;
      h->seg_feature_enabled[i][2] = 1; h->seg_feature_data[i][2] = i % 4;
      h->seg_feature_enabled[i][3] = i % 2;
      if (i < 7) h->seg_tree_probs[i] = 23 + i;
      if (i < 3) h->seg_pred_probs[i] = 154 + i;
   }
   for (i = 0; i < 3; i++)
   {
      h->ref_idx[i] = i; p.named_slots[i] = i + 1; h->ref_sign_bias[i + 1] = i % 2;
   }
}

#ifdef _WIN32
static void sdk_check(void)
{
   DXVA_PicParams_VP9 sdk;
   const rvp9_hdr *h = &p.header;
   unsigned i, j;
   memset(&sdk, 0, sizeof(sdk));
   CHECK(sizeof(sdk) == DXVA_VP9_PP_SIZE);
   CHECK(sizeof(DXVA_Slice_VPx_Short) == DXVA_VP9_SLICE_SIZE);
   sdk.CurrPic.bPicEntry = p.slot; sdk.profile = h->profile;
   sdk.frame_type = h->frame_type; sdk.show_frame = h->show_frame;
   sdk.error_resilient_mode = h->error_resilient;
   sdk.subsampling_x = h->subsampling_x; sdk.subsampling_y = h->subsampling_y;
   sdk.refresh_frame_context = h->refresh_frame_context;
   sdk.frame_parallel_decoding_mode = h->frame_parallel_decoding;
   sdk.intra_only = h->intra_only; sdk.frame_context_idx = h->frame_context_idx;
   sdk.reset_frame_context = h->reset_frame_context; sdk.allow_high_precision_mv = h->allow_high_precision_mv;
   sdk.width = h->w; sdk.height = h->h;
   sdk.BitDepthMinus8Luma = sdk.BitDepthMinus8Chroma = h->bit_depth - 8;
   sdk.interp_filter = h->interp_filter;
   for (i = 0; i < 8; i++)
   {
      sdk.ref_frame_map[i].bPicEntry = p.reference_slots[i] < 0 ? 255 : p.reference_slots[i];
      if (p.reference_slots[i] >= 0)
      { sdk.ref_frame_coded_width[i] = h->w; sdk.ref_frame_coded_height[i] = h->h; }
      for (j = 0; j < 4; j++)
      {
         sdk.stVP9Segments.feature_data[i][j] = h->seg_feature_data[i][j];
         sdk.stVP9Segments.feature_mask[i] |= h->seg_feature_enabled[i][j] << j;
      }
   }
   for (i = 0; i < 3; i++) sdk.frame_refs[i].bPicEntry = p.named_slots[i] < 0 ? 255 : p.named_slots[i];
   for (i = 0; i < 4; i++)
   { sdk.ref_frame_sign_bias[i] = h->ref_sign_bias[i]; sdk.ref_deltas[i] = h->lf_ref_deltas[i]; }
   sdk.filter_level = h->lf_level; sdk.sharpness_level = h->lf_sharpness;
   sdk.mode_ref_delta_enabled = h->lf_delta_enabled; sdk.mode_ref_delta_update = h->lf_delta_update;
   sdk.use_prev_in_find_mv_refs = p.use_prev_frame_mvs;
   for (i = 0; i < 2; i++) sdk.mode_deltas[i] = h->lf_mode_deltas[i];
   sdk.base_qindex = h->base_qindex; sdk.y_dc_delta_q = h->y_dc_delta_q;
   sdk.uv_dc_delta_q = h->uv_dc_delta_q; sdk.uv_ac_delta_q = h->uv_ac_delta_q;
   sdk.stVP9Segments.enabled = h->seg_enabled; sdk.stVP9Segments.update_map = h->seg_update_map;
   sdk.stVP9Segments.temporal_update = h->seg_temporal_update; sdk.stVP9Segments.abs_delta = h->seg_abs_delta;
   memcpy(sdk.stVP9Segments.tree_probs, h->seg_tree_probs, 7);
   memcpy(sdk.stVP9Segments.pred_probs, h->seg_pred_probs, 3);
   sdk.log2_tile_cols = h->log2_tile_cols; sdk.log2_tile_rows = h->log2_tile_rows;
   sdk.uncompressed_header_size_byte_aligned = h->uncomp_size;
   sdk.first_partition_size = h->first_partition_size; sdk.StatusReportFeedbackNumber = 123;
   CHECK(!memcmp(&sdk, bytes, sizeof(sdk)));
}
#endif

static void rejected(void)
{
   unsigned i;
   memset(bytes, 0x5a, sizeof(bytes));
   CHECK(!dxva_vp9_pic_params(bytes, &p, 123));
   for (i = 0; i < DXVA_VP9_PP_SIZE; i++) CHECK(!bytes[i]);
   CHECK(bytes[DXVA_VP9_PP_SIZE] == 0x5a);
}

int main(void)
{
   unsigned i;
   setup(); CHECK(dxva_vp9_pic_params(bytes, &p, 123));
   CHECK(bytes[0] == 9 && bytes[1] == 2 && bytes[12] == 2 && bytes[88] == 1);
   CHECK(bytes[20] == 255 && bytes[109] == 15 && bytes[120] == 1 && bytes[121] == 255);
#ifdef _WIN32
   sdk_check();
#endif
   p.header.profile = 0; p.header.bit_depth = 8;
   p.header.frame_type = 0; p.header.allow_high_precision_mv = 0;
   p.use_prev_frame_mvs = 0;
   for (i = 0; i < 3; i++) p.named_slots[i] = -1;
   CHECK(dxva_vp9_pic_params(bytes, &p, 123));
#ifdef _WIN32
   sdk_check();
#endif
   setup(); p.reference_slots[0] = p.slot; rejected();
   setup(); p.named_slots[0] = 5; rejected();
   setup(); p.header.bit_depth = 12; rejected();
   setup(); p.header.subsampling_x = 0; rejected();
   setup(); p.header.seg_feature_data[0][2] = -1; rejected();
   setup(); p.header.seg_feature_data[0][0] = 256; rejected();
   setup(); p.header.first_partition_size = 0; rejected();
   setup(); p.header.show_existing_frame = 1; rejected();
   setup(); p.header.frame_context_idx = 4; rejected();
   setup(); p.header.lf_ref_deltas[0] = -64; rejected();
   setup(); p.header.ref_sign_bias[0] = -1; rejected();
   if (failures) return 1;
   puts("DXVA VP9 packed parameters and capability gates passed");
   return 0;
}
