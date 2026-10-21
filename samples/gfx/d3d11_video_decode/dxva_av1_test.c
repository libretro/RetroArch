/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <dxva.h>
#endif
#include "../../../gfx/common/dxva_av1.h"

static const uint8_t packet[] =
   { 0x32, 0, 0, 0, 0, 0, 2, 'a', 'b', 'c', 0, 'd', 1, 'e', 'f', 'g', 'h' };
static const unsigned offsets[4] = { 7, 11, 13, 15 };
static const unsigned lengths[4] = { 3, 1, 2, 2 };
static uint8_t bytes[4 * DXVA_AV1_TILE_SIZE + 1];
static uint8_t maximum_packet[8192], maximum_tiles[4096 * DXVA_AV1_TILE_SIZE + 1];
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #c); failures++; } } while (0)

static rav1_sequence_t sequence;
static rav1_hw_picture_t picture;
static uint8_t parameters[DXVA_AV1_PP_SIZE + 1];
static unsigned surface_map[32];

static void picture_setup(void)
{
   rav1_picture_t *q = &picture.picture;
   unsigned i, j;
   memset(&sequence, 0, sizeof(sequence));
   memset(&picture, 0, sizeof(picture));
   sequence.bit_depth = 10; sequence.subsampling_x = sequence.subsampling_y = 1;
   sequence.max_width = sequence.max_height = 256;
   sequence.enable_order_hint = 1; sequence.order_hint_bits = 5;
   sequence.enable_filter_intra = sequence.enable_intra_edge_filter = 1;
   sequence.enable_interintra_compound = sequence.enable_masked_compound = 1;
   sequence.enable_warped_motion = sequence.enable_dual_filter = sequence.enable_jnt_comp = 1;
   sequence.enable_ref_frame_mvs = sequence.enable_superres = 1;
   sequence.enable_cdef = sequence.enable_restoration = 1;
   picture.slot = 9;
   picture.header.frame_type = RAV1_FRAME_INTER;
   picture.header.show_frame = picture.header.showable_frame = 1;
   picture.header.allow_screen_content_tools = picture.header.force_integer_mv = 1;
   picture.header.disable_cdf_update = 1;
   picture.header.primary_ref_frame = 2; picture.header.order_hint = 17;
   picture.setup.size.width = 192; picture.setup.size.upscaled_width = 240;
   picture.setup.size.height = 144; picture.setup.size.superres_denom = 10;
   picture.setup.allow_intrabc = picture.setup.allow_high_precision_mv = 1;
   picture.setup.motion_mode_switchable = picture.setup.disable_frame_end_update_cdf = 1;
   picture.setup.use_ref_frame_mvs = 1; picture.setup.interpolation_filter = 4;
   picture.tiles.layout.columns = picture.tiles.layout.rows = 2;
   picture.tiles.context_update_tile_id = 3;
   picture.tiles.mi_col_starts[1] = 32; picture.tiles.mi_col_starts[2] = 48;
   picture.tiles.mi_row_starts[1] = 16; picture.tiles.mi_row_starts[2] = 36;
   picture.motion.allow_warped_motion = picture.motion.reference_select = 1;
   picture.motion.skip_mode_present = picture.motion.reduced_tx_set = 1;
   q->tx_mode = 2; q->base_q_idx = 193;
   q->delta_q_present = q->delta_lf_present = q->delta_lf_multi = 1;
   q->delta_q_res = 3; q->delta_lf_res = 2;
   q->delta_q_y_dc = -64; q->delta_q_u_dc = 63; q->delta_q_v_dc = -17;
   q->delta_q_u_ac = 21; q->delta_q_v_ac = -23;
   q->loop_filter_sharpness = 7; q->loop_filter_delta_enabled = q->loop_filter_delta_update = 1;
   q->reference.loop_filter_mode_deltas[0] = -3; q->reference.loop_filter_mode_deltas[1] = 4;
   q->segmentation_enabled = q->segmentation_update_data = 1;
   q->segmentation_update_map = q->segmentation_temporal_update = 1;
   q->cdef_damping = 6; q->cdef_bits = 3;
   q->restoration_type[0] = RAV1_RESTORE_SGRPROJ;
   q->restoration_type[1] = RAV1_RESTORE_WIENER;
   q->restoration_size[0] = 128; q->restoration_size[1] = q->restoration_size[2] = 64;
   for (i = 0; i < 32; i++) surface_map[i] = 31 - i;
   for (i = 0; i < 8; i++)
   {
      picture.reference_slots[i] = i + 1;
      picture.reference_info[i].valid = 1;
      picture.reference_info[i].size.upscaled_width = 240 - i * 8;
      picture.reference_info[i].size.height = 144 + i * 8;
      q->reference.segment_enabled[i] = 129 + i;
      q->reference.loop_filter_ref_deltas[i] = (int)i - 5;
      for (j = 0; j < 8; j++)
         q->reference.segment_data[i][j] = j < 5 ? (int)i - (int)j : j == 5 ? (int)i : 0;
      q->cdef_y_pri[i] = i + 2; q->cdef_y_sec[i] = 4;
      q->cdef_uv_pri[i] = 15 - i; q->cdef_uv_sec[i] = i % 3;
      if (i < 4) q->loop_filter_level[i] = 60 - i;
      if (i < 7)
      {
         picture.setup.ref_frame_idx[i] = (i + 2) % 8;
         picture.motion.global.type[i] = RAV1_GLOBAL_AFFINE;
         picture.motion.global.params[i][0] = 1024;
         picture.motion.global.params[i][1] = -2048;
         picture.motion.global.params[i][2] = 65600;
         picture.motion.global.params[i][3] = 128;
         picture.motion.global.params[i][4] = -64;
         picture.motion.global.params[i][5] = 65520;
      }
   }
}

#ifdef _WIN32
static void picture_sdk(void)
{
   DXVA_PicParams_AV1 sdk;
   unsigned i, j;
   memset(&sdk, 0, sizeof(sdk));
   sdk.width = 240; sdk.height = 144; sdk.max_width = sdk.max_height = 256;
   sdk.CurrPicTextureIndex = 34; sdk.superres_denom = 10; sdk.bitdepth = 10;
   sdk.tiles.cols = sdk.tiles.rows = 2; sdk.tiles.context_update_id = 3;
   sdk.tiles.widths[0] = 2; sdk.tiles.widths[1] = 1;
   sdk.tiles.heights[0] = 1; sdk.tiles.heights[1] = 2;
   sdk.coding.intra_edge_filter = sdk.coding.interintra_compound = 1;
   sdk.coding.masked_compound = sdk.coding.warped_motion = sdk.coding.dual_filter = 1;
   sdk.coding.jnt_comp = sdk.coding.screen_content_tools = sdk.coding.integer_mv = 1;
   sdk.coding.cdef = sdk.coding.restoration = sdk.coding.intrabc = 1;
   sdk.coding.high_precision_mv = sdk.coding.switchable_motion_mode = sdk.coding.filter_intra = 1;
   sdk.coding.disable_frame_end_update_cdf = sdk.coding.disable_cdf_update = 1;
   sdk.coding.reference_mode = sdk.coding.skip_mode = sdk.coding.reduced_tx_set = 1;
   sdk.coding.superres = sdk.coding.use_ref_frame_mvs = sdk.coding.enable_ref_frame_mvs = 1;
   sdk.coding.tx_mode = 2; sdk.coding.reference_frame_update = 1;
   sdk.format.frame_type = 1; sdk.format.show_frame = sdk.format.showable_frame = 1;
   sdk.format.subsampling_x = sdk.format.subsampling_y = 1;
   sdk.primary_ref_frame = 2; sdk.order_hint = 17; sdk.order_hint_bits = 5;
   for (i = 0; i < 8; i++)
   {
      sdk.RefFrameMapTextureIndex[i] = 30 - i;
      sdk.loop_filter.ref_deltas[i] = (int)i - 5;
      sdk.segmentation.feature_mask[i].mask = 129 + i;
      for (j = 0; j < 8; j++)
         sdk.segmentation.feature_data[i][j] = j < 5 ? (int)i - (int)j : j == 5 ? (int)i : 0;
      sdk.cdef.y_strengths[i].primary = i + 2; sdk.cdef.y_strengths[i].secondary = 3;
      sdk.cdef.uv_strengths[i].primary = 15 - i; sdk.cdef.uv_strengths[i].secondary = i % 3;
      if (i < 7)
      {
         j = (i + 2) % 8;
         sdk.frame_refs[i].width = 240 - j * 8; sdk.frame_refs[i].height = 144 + j * 8;
         sdk.frame_refs[i].Index = j; sdk.frame_refs[i].wmtype = 3;
         sdk.frame_refs[i].wmmat[0] = 1024; sdk.frame_refs[i].wmmat[1] = -2048;
         sdk.frame_refs[i].wmmat[2] = 65600; sdk.frame_refs[i].wmmat[3] = 128;
         sdk.frame_refs[i].wmmat[4] = -64; sdk.frame_refs[i].wmmat[5] = 65520;
      }
   }
   sdk.loop_filter.filter_level[0] = 60; sdk.loop_filter.filter_level[1] = 59;
   sdk.loop_filter.filter_level_u = 58; sdk.loop_filter.filter_level_v = 57;
   sdk.loop_filter.sharpness_level = 7;
   sdk.loop_filter.mode_ref_delta_enabled = sdk.loop_filter.mode_ref_delta_update = 1;
   sdk.loop_filter.delta_lf_multi = sdk.loop_filter.delta_lf_present = 1;
   sdk.loop_filter.delta_lf_res = 2;
   sdk.loop_filter.mode_deltas[0] = -3; sdk.loop_filter.mode_deltas[1] = 4;
   sdk.loop_filter.frame_restoration_type[0] = 2; sdk.loop_filter.frame_restoration_type[1] = 1;
   sdk.loop_filter.log2_restoration_unit_size[0] = 7;
   sdk.loop_filter.log2_restoration_unit_size[1] = sdk.loop_filter.log2_restoration_unit_size[2] = 6;
   sdk.quantization.delta_q_present = 1; sdk.quantization.delta_q_res = 3;
   sdk.quantization.base_qindex = 193;
   sdk.quantization.y_dc_delta_q = -64; sdk.quantization.u_dc_delta_q = 63;
   sdk.quantization.v_dc_delta_q = -17; sdk.quantization.u_ac_delta_q = 21;
   sdk.quantization.v_ac_delta_q = -23;
   sdk.quantization.qm_y = sdk.quantization.qm_u = sdk.quantization.qm_v = 255;
   sdk.cdef.damping = sdk.cdef.bits = 3; sdk.interp_filter = 4;
   sdk.segmentation.enabled = sdk.segmentation.update_map = 1;
   sdk.segmentation.update_data = sdk.segmentation.temporal_update = 1;
   CHECK(sdk.StatusReportFeedbackNumber == 0);
   CHECK(sizeof(sdk) == DXVA_AV1_PP_SIZE);
   CHECK(!memcmp(&sdk, parameters, sizeof(sdk)));
}
#endif

static void picture_rejected(void)
{
   unsigned i;
   memset(parameters, 0xa5, sizeof(parameters));
   CHECK(!dxva_av1_pic_params(parameters, &sequence, &picture, surface_map, 34));
   for (i = 0; i < DXVA_AV1_PP_SIZE; i++) CHECK(!parameters[i]);
   CHECK(parameters[DXVA_AV1_PP_SIZE] == 0xa5);
}

static void picture_tests(void)
{
   int32_t matrix[6] = {0, 0, 65536, 0, 0, 65536};
   picture_setup();
   memset(parameters, 0xa5, sizeof(parameters));
   CHECK(dxva_av1_pic_params(parameters, &sequence, &picture, surface_map, 34));
   CHECK(parameters[DXVA_AV1_PP_SIZE] == 0xa5);
#ifdef _WIN32
   picture_sdk();
#endif
   CHECK(!dxva_av1_warp_invalid(matrix));
   matrix[3] = 8192; CHECK(!dxva_av1_warp_invalid(matrix));
   matrix[3] = 9408; CHECK(dxva_av1_warp_invalid(matrix));
   matrix[3] = -9408; CHECK(dxva_av1_warp_invalid(matrix));
   matrix[2] = 0; CHECK(dxva_av1_warp_invalid(matrix));
   picture.tiles.mi_col_starts[1] = 31; picture_rejected();
   picture_setup(); picture.setup.ref_frame_idx[3] = 8; picture_rejected();
   picture_setup(); picture.reference_slots[2] = picture.slot; picture_rejected();
   picture_setup(); surface_map[3] = 34; picture_rejected();
   picture_setup(); picture.motion.global.params[0][2] = 131073; picture_rejected();
   picture_setup(); picture.picture.delta_q_v_dc = -65; picture_rejected();
   picture_setup(); picture.picture.cdef_y_sec[7] = 3; picture_rejected();
   picture_setup(); picture.picture.restoration_size[2] = 48; picture_rejected();
   picture_setup(); picture.grain.params.apply = 1; picture_rejected();
   picture_setup(); sequence.order_hint_bits = 9; picture_rejected();
}

static unsigned get16(const uint8_t *p)
{
   return p[0] | (unsigned)p[1] << 8;
}

static uint32_t get32(const uint8_t *p)
{
   return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void rejected(const rav1_tile_group_t *group, size_t size,
      size_t capacity, unsigned columns, unsigned rows)
{
   unsigned written = 99, i;
   memset(bytes, 0xa5, sizeof(bytes));
   CHECK(!dxva_av1_tiles(bytes, capacity, &written, packet, size,
         group, columns, rows));
   CHECK(!written);
   for (i = 0; i < sizeof(bytes); i++) CHECK(bytes[i] == 0xa5);
}

static void maximum_group(void)
{
   rav1_tile_layout_t layout;
   rav1_tile_group_t group;
   unsigned i, written = 0;
   memset(&layout, 0, sizeof(layout));
   layout.columns = layout.rows = 64;
   layout.columns_log2 = layout.rows_log2 = 6;
   layout.size_bytes = 1;
   memset(maximum_tiles, 0xa5, sizeof(maximum_tiles));
   CHECK(rav1_tile_group_init(&group, maximum_packet, sizeof(maximum_packet), &layout, true));
   CHECK(dxva_av1_tiles(maximum_tiles, sizeof(maximum_tiles) - 1, &written,
         maximum_packet, sizeof(maximum_packet), &group, 64, 64));
   CHECK(written == 4096 && maximum_tiles[sizeof(maximum_tiles) - 1] == 0xa5);
   for (i = 0; i < 4096; i++)
   {
      const uint8_t *entry = maximum_tiles + i * DXVA_AV1_TILE_SIZE;
      CHECK(get32(entry) == (i == 4095 ? 8191 : 2 + 2 * i));
      CHECK(get32(entry + 4) == 1);
      CHECK(get16(entry + 8) == i / 64 && get16(entry + 10) == i % 64);
   }
}

int main(void)
{
   rav1_tile_layout_t layout;
   rav1_tile_group_t group, bad;
   unsigned written = 0, i;
   memset(&layout, 0, sizeof(layout));
   layout.columns = layout.rows = 2;
   layout.columns_log2 = layout.rows_log2 = layout.size_bytes = 1;
   CHECK(rav1_tile_group_init(&group, packet + 5, sizeof(packet) - 5, &layout, true));
   memset(bytes, 0xa5, sizeof(bytes));
   CHECK(dxva_av1_tiles(bytes, sizeof(bytes) - 1, &written, packet,
         sizeof(packet), &group, 2, 2));
   CHECK(written == 4 && bytes[sizeof(bytes) - 1] == 0xa5);
   CHECK(group.next == 0 && group.offset == 1);
   for (i = 0; i < 4; i++)
   {
      const uint8_t *entry = bytes + i * DXVA_AV1_TILE_SIZE;
      CHECK(get32(entry) == offsets[i] && get32(entry + 4) == lengths[i]);
      CHECK(get16(entry + 8) == i / 2 && get16(entry + 10) == i % 2);
      CHECK(!get16(entry + 12) && entry[14] == 255 && !entry[15]);
#ifdef _WIN32
      {
         DXVA_Tile_AV1 sdk;
         memset(&sdk, 0, sizeof(sdk));
         sdk.DataOffset = offsets[i]; sdk.DataSize = lengths[i];
         sdk.row = i / 2; sdk.column = i % 2; sdk.anchor_frame = 255;
         CHECK(sizeof(sdk) == DXVA_AV1_TILE_SIZE);
         CHECK(!memcmp(&sdk, entry, sizeof(sdk)));
      }
#endif
   }
   rejected(&group, sizeof(packet), 63, 2, 2);
   rejected(&group, sizeof(packet) - 1, 64, 2, 2);
   rejected(&group, (size_t)-1, 64, 2, 2);
   rejected(&group, sizeof(packet), 64, 0, 2);
   rejected(&group, sizeof(packet), 64, 2, 65);
   bad = group; bad.next = 1; rejected(&bad, sizeof(packet), 64, 2, 2);
   bad = group; bad.first = 1; rejected(&bad, sizeof(packet), 64, 2, 2);
   bad = group; bad.offset = bad.size; rejected(&bad, sizeof(packet), 64, 2, 2);
   bad = group; bad.size_bytes = 4; rejected(&bad, sizeof(packet), 64, 2, 2);
   bad = group; bad.data = packet + sizeof(packet); rejected(&bad, sizeof(packet), 64, 2, 2);
   layout.columns = layout.rows = 1;
   layout.columns_log2 = layout.rows_log2 = layout.size_bytes = 0;
   CHECK(rav1_tile_group_init(&group, packet + 15, 2, &layout, true));
   CHECK(dxva_av1_tiles(bytes, 16, &written, packet, sizeof(packet), &group, 1, 1));
   CHECK(written == 1 && get32(bytes) == 15 && get32(bytes + 4) == 2);
   maximum_group();
   picture_tests();
   if (failures) return 1;
   puts("PASS dxva_av1_test (tile spans, packet offsets, SDK layout, rejection atomicity)");
   return 0;
}
