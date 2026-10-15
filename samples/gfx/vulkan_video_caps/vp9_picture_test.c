/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "../../../gfx/common/vulkan_vp9_picture.h"
static rvp9_hw_picture_t p;
static vulkan_vp9_picture_t out;
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

static void rejected(void)
{
   vulkan_vp9_picture_t zero;
   memset(&zero, 0, sizeof(zero));
   memset(&out, 255, sizeof(out));
   CHECK(!vulkan_vp9_picture(&out, &p, 1024));
   CHECK(!memcmp(&out, &zero, sizeof(out)));
}

int main(void)
{
   unsigned i;
   setup();
   p.header.lf_ref_delta_upd[2] = p.header.lf_mode_delta_upd[1] = 1;
   p.header.seg_update_data = p.header.refresh_frame_flags = 1;
   CHECK(vulkan_vp9_picture(&out, &p, 1024));
   CHECK(out.info.pStdPictureInfo == &out.picture);
   CHECK(out.picture.pColorConfig == &out.color && out.picture.pLoopFilter == &out.filter
         && out.picture.pSegmentation == &out.segmentation);
   CHECK(out.picture.profile == STD_VIDEO_VP9_PROFILE_2 && out.color.BitDepth == 10);
   CHECK(out.picture.flags.UsePrevFrameMvs && out.picture.flags.allow_high_precision_mv);
   CHECK(out.picture.ref_frame_sign_bias_mask == 4 && out.picture.frame_context_idx == 2);
   CHECK(out.picture.delta_q_y_dc == -7 && out.picture.delta_q_uv_dc == 8 && out.picture.delta_q_uv_ac == -9);
   CHECK(out.filter.update_ref_delta == 4 && out.filter.update_mode_delta == 2);
   CHECK(out.filter.loop_filter_ref_deltas[3] == -3 && out.filter.loop_filter_mode_deltas[0] == -4);
   CHECK(out.info.uncompressedHeaderOffset == 0 && out.info.compressedHeaderOffset == 43 && out.info.tilesOffset == 310);
   for (i = 0; i < 8; i++)
   {
      CHECK(out.segmentation.FeatureEnabled[i] == (i % 2 ? 15 : 7));
      CHECK(out.segmentation.FeatureData[i][0] == -255 + (int)i);
   }
   setup(); p.header.ref_idx[2] = 0; p.named_slots[2] = 1;
   CHECK(vulkan_vp9_picture(&out, &p, 1024) && out.info.referenceNameSlotIndices[2] == 1);
   setup(); p.header.frame_type = 0; p.header.profile = 0; p.header.bit_depth = 8;
   for (i = 0; i < 3; i++) p.named_slots[i] = -1;
   CHECK(vulkan_vp9_picture(&out, &p, 1024) && out.picture.profile == STD_VIDEO_VP9_PROFILE_0);
   CHECK(!vulkan_vp9_picture(NULL, &p, 1024));
   setup(); CHECK(!vulkan_vp9_picture(&out, &p, 310));
   setup(); p.header.profile = 1; rejected();
   setup(); p.header.bit_depth = 12; rejected();
   setup(); p.header.uncomp_size = 1025; rejected();
   setup(); p.header.first_partition_size = 1024; rejected();
   setup(); p.header.seg_feature_data[7][0] = 256; rejected();
   setup(); p.header.seg_update_data = 2; rejected();
   setup(); p.header.color_space = 8; rejected();
   setup(); p.header.color_range = -1; rejected();
   setup(); p.header.refresh_frame_flags = 256; rejected();
   setup(); p.named_slots[1] = 30; rejected();
   setup(); p.slot = 1; rejected();
   printf("Vulkan VP9 picture translation: %u failures\n", failures);
   return failures ? 1 : 0;
}
