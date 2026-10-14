/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <dxva.h>
#endif
#include "../../../gfx/common/dxva_hevc_rh265.h"
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
static dxva_hevc_picture_t p;
static dxva_hevc_scaling_t scaling;
static uint8_t bytes[DXVA_HEVC_QM_SIZE + 1];

static unsigned get16(const uint8_t *b) { return b[0] | (unsigned)b[1] << 8; }
static uint32_t get32(const uint8_t *b)
{
   return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}
static void setup(void)
{
   memset(&p, 0, sizeof(p));
   p.width = 1920; p.height = 1088; p.min_cb_log2 = 3; p.ctb_log2 = 6;
   p.min_tb_log2 = 2; p.max_tb_log2 = 5; p.bit_depth = 10; p.max_poc_lsb_log2 = 8;
   p.max_dec_pic_buffering = 4; p.max_reorder_pics = 2;
   p.hierarchy_inter = 3; p.hierarchy_intra = 2; p.num_st_rps = 5; p.num_lt_rps = 1;
   p.default_l0 = 2; p.default_l1 = 1; p.delta_pocs = 3; p.rps_bits = 71; p.slot = 4;
   p.diff_qp_depth = 1; p.parallel_merge_log2 = 3;
   p.init_qp = 24; p.cb_qp = -2; p.cr_qp = 3; p.beta_offset_div2 = -1; p.tc_offset_div2 = 2;
   p.poc = 0; p.num_refs = 3;
   p.refs[0].poc = -2; p.refs[0].slot = 1;
   p.refs[1].poc = 2; p.refs[1].slot = 2;
   p.refs[2].poc = -7; p.refs[2].slot = 3; p.refs[2].long_term = 1;
   p.num_before = p.num_after = p.num_longterm = 1;
   p.before[0] = 0; p.after[0] = 1; p.longterm[0] = 2;
   p.coding_tools = 7 | (1u << 17) | (1u << 18) | (1u << 19) | (1u << 21)
      | (3u << 22) | (1u << 25) | (1u << 26);
   p.coding_settings = 0x7f | (1u << 8) | (1u << 11) | (1u << 12) | (1u << 13)
      | (1u << 14) | (1u << 15);
}

#ifdef _WIN32
static void check_sdk(void)
{
   DXVA_PicParams_HEVC sdk;
   unsigned i;
   memset(&sdk, 0, sizeof(sdk));
   CHECK(sizeof(sdk) == DXVA_HEVC_PP_SIZE);
   sdk.PicWidthInMinCbsY = (USHORT)(p.width >> p.min_cb_log2);
   sdk.PicHeightInMinCbsY = (USHORT)(p.height >> p.min_cb_log2);
   sdk.chroma_format_idc = 1;
   sdk.bit_depth_luma_minus8 = sdk.bit_depth_chroma_minus8 = p.bit_depth - 8;
   sdk.log2_max_pic_order_cnt_lsb_minus4 = p.max_poc_lsb_log2 - 4;
   sdk.NoPicReorderingFlag = !p.max_reorder_pics; sdk.NoBiPredFlag = p.no_bipred;
   sdk.CurrPic.bPicEntry = (UCHAR)p.slot;
   sdk.sps_max_dec_pic_buffering_minus1 = (UCHAR)(p.max_dec_pic_buffering - 1);
   sdk.log2_min_luma_coding_block_size_minus3 = (UCHAR)(p.min_cb_log2 - 3);
   sdk.log2_diff_max_min_luma_coding_block_size = (UCHAR)(p.ctb_log2 - p.min_cb_log2);
   sdk.log2_min_transform_block_size_minus2 = (UCHAR)(p.min_tb_log2 - 2);
   sdk.log2_diff_max_min_transform_block_size = (UCHAR)(p.max_tb_log2 - p.min_tb_log2);
   sdk.max_transform_hierarchy_depth_inter = (UCHAR)p.hierarchy_inter;
   sdk.max_transform_hierarchy_depth_intra = (UCHAR)p.hierarchy_intra;
   sdk.num_short_term_ref_pic_sets = (UCHAR)p.num_st_rps;
   sdk.num_long_term_ref_pics_sps = (UCHAR)p.num_lt_rps;
   sdk.num_ref_idx_l0_default_active_minus1 = (UCHAR)(p.default_l0 - 1);
   sdk.num_ref_idx_l1_default_active_minus1 = (UCHAR)(p.default_l1 - 1);
   sdk.init_qp_minus26 = (CHAR)(p.init_qp - 26);
   sdk.ucNumDeltaPocsOfRefRpsIdx = (UCHAR)p.delta_pocs;
   sdk.wNumBitsForShortTermRPSInSlice = (USHORT)p.rps_bits;
   sdk.scaling_list_enabled_flag = !!(p.coding_tools & (1u << 0));
   sdk.amp_enabled_flag = !!(p.coding_tools & (1u << 1));
   sdk.sample_adaptive_offset_enabled_flag = !!(p.coding_tools & (1u << 2));
   sdk.long_term_ref_pics_present_flag = !!(p.coding_tools & (1u << 17));
   sdk.sps_temporal_mvp_enabled_flag = !!(p.coding_tools & (1u << 18));
   sdk.strong_intra_smoothing_enabled_flag = !!(p.coding_tools & (1u << 19));
   sdk.output_flag_present_flag = !!(p.coding_tools & (1u << 21));
   sdk.sign_data_hiding_enabled_flag = !!(p.coding_tools & (1u << 25));
   sdk.cabac_init_present_flag = !!(p.coding_tools & (1u << 26));
   sdk.num_extra_slice_header_bits = (p.coding_tools >> 22) & 7;
   sdk.constrained_intra_pred_flag = !!(p.coding_settings & (1u << 0));
   sdk.transform_skip_enabled_flag = !!(p.coding_settings & (1u << 1));
   sdk.cu_qp_delta_enabled_flag = !!(p.coding_settings & (1u << 2));
   sdk.pps_slice_chroma_qp_offsets_present_flag = !!(p.coding_settings & (1u << 3));
   sdk.weighted_pred_flag = !!(p.coding_settings & (1u << 4));
   sdk.weighted_bipred_flag = !!(p.coding_settings & (1u << 5));
   sdk.transquant_bypass_enabled_flag = !!(p.coding_settings & (1u << 6));
   sdk.entropy_coding_sync_enabled_flag = !!(p.coding_settings & (1u << 8));
   sdk.pps_loop_filter_across_slices_enabled_flag = !!(p.coding_settings & (1u << 11));
   sdk.deblocking_filter_override_enabled_flag = !!(p.coding_settings & (1u << 12));
   sdk.pps_deblocking_filter_disabled_flag = !!(p.coding_settings & (1u << 13));
   sdk.lists_modification_present_flag = !!(p.coding_settings & (1u << 14));
   sdk.slice_segment_header_extension_present_flag = !!(p.coding_settings & (1u << 15));
   sdk.IrapPicFlag = !!(p.coding_settings & (1u << 16));
   sdk.IdrPicFlag = !!(p.coding_settings & (1u << 17));
   sdk.IntraPicFlag = !!(p.coding_settings & (1u << 18));
   sdk.pps_cb_qp_offset = (CHAR)p.cb_qp; sdk.pps_cr_qp_offset = (CHAR)p.cr_qp;
   sdk.diff_cu_qp_delta_depth = (UCHAR)p.diff_qp_depth;
   sdk.pps_beta_offset_div2 = (CHAR)p.beta_offset_div2; sdk.pps_tc_offset_div2 = (CHAR)p.tc_offset_div2;
   sdk.log2_parallel_merge_level_minus2 = (UCHAR)(p.parallel_merge_log2 - 2);
   sdk.CurrPicOrderCntVal = p.poc;
   memset(sdk.RefPicList, 0xff, sizeof(sdk.RefPicList));
   memset(sdk.RefPicSetStCurrBefore, 0xff, 8); memset(sdk.RefPicSetStCurrAfter, 0xff, 8);
   memset(sdk.RefPicSetLtCurr, 0xff, 8);
   for (i = 0; i < p.num_refs; i++)
   {
      sdk.RefPicList[i].Index7Bits = p.refs[i].slot;
      sdk.RefPicList[i].AssociatedFlag = p.refs[i].long_term;
      sdk.PicOrderCntValList[i] = p.refs[i].poc;
   }
   memcpy(sdk.RefPicSetStCurrBefore, p.before, p.num_before);
   memcpy(sdk.RefPicSetStCurrAfter, p.after, p.num_after);
   memcpy(sdk.RefPicSetLtCurr, p.longterm, p.num_longterm);
   sdk.StatusReportFeedbackNumber = 0x87654321;
   CHECK(dxva_hevc_pic_params(bytes, &p, 0x87654321));
   CHECK(!memcmp(bytes, &sdk, sizeof(sdk)));
}
#endif

static void test_picture(void)
{
   unsigned i;
   setup(); CHECK(dxva_hevc_pic_params(bytes, &p, 0x87654321));
   CHECK(get16(bytes) == 240 && get16(bytes + 2) == 136);
   CHECK(get16(bytes + 4) == (1 | 2 << 3 | 2 << 6 | 4 << 9));
   CHECK(bytes[6] == 4 && bytes[7] == 3 && (int8_t)bytes[18] == -2);
   CHECK(bytes[124] == 1 && bytes[126] == 0x83 && bytes[127] == 0xff);
   CHECK((int32_t)get32(bytes + 140) == -2 && (int32_t)get32(bytes + 148) == -7);
   CHECK(bytes[200] == 0 && bytes[208] == 1 && bytes[216] == 2 && bytes[217] == 0xff);
   CHECK(get32(bytes + 228) == 0x87654321);
#ifdef _WIN32
   check_sdk();
#endif
   p.bit_depth = 8; p.max_reorder_pics = 0; p.no_bipred = 1;
#ifdef _WIN32
   check_sdk();
#endif
   setup(); p.slot = 16; p.max_dec_pic_buffering = 16; p.num_refs = 15;
   p.num_before = 8; p.num_after = 7; p.num_longterm = 0;
   for (i = 0; i < 15; i++)
   {
      p.refs[i].slot = i + 1; p.refs[i].long_term = 0;
      p.refs[i].poc = i < 8 ? (int32_t)i - 8 : (int32_t)i - 7;
      if (i < 8) p.before[i] = (uint8_t)i;
      else p.after[i-8] = (uint8_t)i;
   }
#ifdef _WIN32
   check_sdk();
#endif
   CHECK(dxva_hevc_pic_params(bytes, &p, 1));
   setup(); p.bit_depth = 8;
   p.num_refs = p.num_before = p.num_after = p.num_longterm = 0;
   p.coding_settings = (1u << 16) | (1u << 17) | (1u << 18);
#ifdef _WIN32
   check_sdk();
#endif
   CHECK(dxva_hevc_pic_params(bytes, &p, 1));
   for (i = 124; i < 139; i++) CHECK(bytes[i] == 0xff);
   setup(); p.bit_depth = 10; p.init_qp = -12;
   CHECK(dxva_hevc_pic_params(bytes, &p, 1) && (int8_t)bytes[18] == -38);
}

#define BAD(change) do { setup(); change; memset(bytes, 0xaa, 232); \
   CHECK(!dxva_hevc_pic_params(bytes, &p, 1)); \
   for (i = 0; i < 232; i++) CHECK(!bytes[i]); } while (0)
static void test_rejection(void)
{
   unsigned i;
   BAD(p.bit_depth = 12); BAD(p.min_cb_log2 = 32); BAD(p.ctb_log2 = 2);
   BAD(p.width = 1919); BAD(p.width = 0); BAD(p.max_tb_log2 = 6);
   BAD(p.max_dec_pic_buffering = 17); BAD(p.max_reorder_pics = 4);
   BAD(p.default_l0 = 0); BAD(p.default_l1 = 16); BAD(p.rps_bits = 65536);
   BAD(p.num_refs = 16); BAD(p.num_before = 9); BAD(p.before[0] = 15);
   BAD(p.after[0] = 0); BAD(p.longterm[0] = 1); BAD(p.refs[0].slot = p.slot);
   BAD(p.refs[1].slot = p.refs[0].slot); BAD(p.refs[0].poc = 1);
   BAD(p.coding_tools &= ~(1u << 17));
   BAD(p.coding_tools |= 1u << 3); BAD(p.coding_tools |= 1u << 20);
   BAD(p.coding_tools |= 1u << 31); BAD(p.coding_settings |= 1u << 7);
   BAD(p.coding_settings |= 1u << 31); BAD(p.cb_qp = -13); BAD(p.init_qp = -13);
   BAD(p.coding_settings |= 1u << 17);
   setup(); CHECK(!dxva_hevc_pic_params(bytes, &p, 0));
}

static void test_scaling(void)
{
   unsigned i;
   uint8_t *source = (uint8_t*)&scaling;
   CHECK(sizeof(scaling) == DXVA_HEVC_QM_SIZE);
   for (i = 0; i < sizeof(scaling); i++) source[i] = (uint8_t)(i % 255 + 1);
   CHECK(dxva_hevc_qmatrix(bytes, &scaling)); CHECK(!memcmp(bytes, source, sizeof(scaling)));
#ifdef _WIN32
   {
      DXVA_Qmatrix_HEVC sdk;
      CHECK(sizeof(sdk) == DXVA_HEVC_QM_SIZE);
      memcpy(sdk.ucScalingLists0, scaling.list4, sizeof(scaling.list4));
      memcpy(sdk.ucScalingLists1, scaling.list8, sizeof(scaling.list8));
      memcpy(sdk.ucScalingLists2, scaling.list16, sizeof(scaling.list16));
      memcpy(sdk.ucScalingLists3, scaling.list32, sizeof(scaling.list32));
      memcpy(sdk.ucScalingListDCCoefSizeID2, scaling.dc16, 6);
      memcpy(sdk.ucScalingListDCCoefSizeID3, scaling.dc32, 2);
      CHECK(!memcmp(bytes, &sdk, sizeof(sdk)));
   }
#endif
   scaling.dc32[1] = 0; CHECK(!dxva_hevc_qmatrix(bytes, &scaling));
   for (i = 0; i < DXVA_HEVC_QM_SIZE; i++) CHECK(!bytes[i]);
}

static void test_raster_scaling(void)
{
   static const uint8_t scan4[16] =
      {0,4,1,8,5,2,12,9,6,3,13,10,7,14,11,15};
   static const uint8_t scan8[64] =
   {
      0,8,1,16,9,2,24,17,10,3,32,25,18,11,4,40,
      33,26,19,12,5,48,41,34,27,20,13,6,56,49,42,35,
      28,21,14,7,57,50,43,36,29,22,15,58,51,44,37,30,
      23,59,52,45,38,31,60,53,46,39,61,54,47,62,55,63
   };
   static dxva_hevc_scaling_t saved;
   unsigned m, i;
   for (m = 0; m < 6; m++)
   {
      for (i = 0; i < 16; i++) scaling.list4[m][i] = (uint8_t)(1 + m * 16 + i);
      for (i = 0; i < 64; i++)
      {
         scaling.list8[m][i] = (uint8_t)(1 + m + i);
         scaling.list16[m][i] = (uint8_t)(80 + m + i);
      }
      scaling.dc16[m] = (uint8_t)(200 + m);
   }
   for (m = 0; m < 2; m++)
   {
      for (i = 0; i < 64; i++) scaling.list32[m][i] = (uint8_t)(150 + m + i);
      scaling.dc32[m] = (uint8_t)(230 + m);
   }
   saved = scaling;
   bytes[1000] = 0xa5;
   CHECK(dxva_hevc_qmatrix_raster(bytes, &scaling));
   CHECK(!memcmp(&saved, &scaling, sizeof(saved)) && bytes[1000] == 0xa5);
   for (m = 0; m < 6; m++)
   {
      for (i = 0; i < 16; i++) CHECK(bytes[m * 16 + i] == scaling.list4[m][scan4[i]]);
      for (i = 0; i < 64; i++)
      {
         CHECK(bytes[96 + m * 64 + i] == scaling.list8[m][scan8[i]]);
         CHECK(bytes[480 + m * 64 + i] == scaling.list16[m][scan8[i]]);
      }
      CHECK(bytes[992 + m] == scaling.dc16[m]);
   }
   for (m = 0; m < 2; m++)
   {
      for (i = 0; i < 64; i++) CHECK(bytes[864 + m * 64 + i] == scaling.list32[m][scan8[i]]);
      CHECK(bytes[998 + m] == scaling.dc32[m]);
   }
   scaling.list16[5][63] = 0;
   CHECK(!dxva_hevc_qmatrix_raster(bytes, &scaling));
   for (i = 0; i < 1000; i++) CHECK(!bytes[i]);
   CHECK(!dxva_hevc_qmatrix_raster(bytes, NULL));
   CHECK(!dxva_hevc_qmatrix_raster(NULL, &saved));
}

static void test_parser_adapter(void)
{
   static rh265_video_params_t s;
   static dxva_hevc_picture_t saved;
   unsigned i;
   setup();
   memset(&s, 0, sizeof(s));
   s.chroma_format_idc = 1;
   s.width = (int)p.width;
   s.height = (int)p.height;
   s.log2_min_cb = (int)p.min_cb_log2;
   s.log2_ctb = (int)p.ctb_log2;
   s.log2_min_tb = (int)p.min_tb_log2;
   s.log2_max_tb = (int)p.max_tb_log2;
   s.bit_depth = (int)p.bit_depth;
   s.log2_max_poc_lsb = (int)p.max_poc_lsb_log2;
   s.max_dec_pic_buffering = (int)p.max_dec_pic_buffering;
   s.max_num_reorder_pics = (int)p.max_reorder_pics;
   s.max_transform_hierarchy_depth_inter = (int)p.hierarchy_inter;
   s.max_transform_hierarchy_depth_intra = (int)p.hierarchy_intra;
   s.num_st_rps = (int)p.num_st_rps;
   s.num_long_term_ref_pics = (int)p.num_lt_rps;
   s.num_ref_idx_l0_default = (int)p.default_l0;
   s.num_ref_idx_l1_default = (int)p.default_l1;
   s.diff_cu_qp_delta_depth = (int)p.diff_qp_depth;
   s.log2_parallel_merge_level = (int)p.parallel_merge_log2;
   s.init_qp = (int)p.init_qp;
   s.cb_qp_offset = (int)p.cb_qp;
   s.cr_qp_offset = (int)p.cr_qp;
   s.beta_offset_div2 = (int)p.beta_offset_div2;
   s.tc_offset_div2 = (int)p.tc_offset_div2;
   s.scaling_list_enabled = 1;
   s.amp_enabled = 1;
   s.sao_enabled = 1;
   s.long_term_ref_pics_present = 1;
   s.temporal_mvp_enabled = 1;
   s.strong_intra_smoothing = 1;
   s.output_flag_present = 1;
   s.sign_data_hiding = 1;
   s.cabac_init_present = 1;
   s.constrained_intra_pred = 1;
   s.transform_skip_enabled = 1;
   s.cu_qp_delta_enabled = 1;
   s.slice_chroma_qp_offsets_present = 1;
   s.weighted_pred = 1;
   s.weighted_bipred = 1;
   s.transquant_bypass_enabled = 1;
   s.entropy_coding_sync_enabled = 1;
   s.loop_filter_across_slices = 1;
   s.deblocking_filter_override_enabled = 1;
   s.deblocking_filter_disabled = 1;
   s.lists_modification_present = 1;
   s.slice_segment_header_extension_present = 1;
   s.num_extra_slice_header_bits = 5;
   saved = p;
   p.width = 8; p.bit_depth = 8; p.coding_tools = 0;
   p.coding_settings = 1u << 16;
   CHECK(dxva_hevc_params_from_rh265(&p, &s));
   CHECK(p.width == saved.width && p.bit_depth == saved.bit_depth);
   CHECK(p.coding_tools == UINT32_C(0x076e0007));
   CHECK(p.coding_settings == UINT32_C(0x0001f97f));
   CHECK(p.slot == saved.slot && p.poc == saved.poc && p.rps_bits == saved.rps_bits);
   CHECK(p.num_refs == saved.num_refs && !memcmp(p.refs, saved.refs, sizeof(p.refs)));
   CHECK(p.num_before == saved.num_before && !memcmp(p.before, saved.before, sizeof(p.before)));
#ifdef _WIN32
   check_sdk();
#endif
   saved = p;
#define ADAPTER_BAD(field, value) do { s.field = value; \
   CHECK(!dxva_hevc_params_from_rh265(&p, &s)); \
   CHECK(!memcmp(&p, &saved, sizeof(p))); s.field = 0; } while (0)
   ADAPTER_BAD(pcm_enabled, 1);
   ADAPTER_BAD(tiles_enabled, 1);
   ADAPTER_BAD(dependent_slice_segments_enabled, 1);
   s.num_extra_slice_header_bits = 8;
   CHECK(!dxva_hevc_params_from_rh265(&p, &s));
   s.num_extra_slice_header_bits = 5;
   s.width = -8;
   CHECK(!dxva_hevc_params_from_rh265(&p, &s) && !memcmp(&p, &saved, sizeof(p)));
   s.width = (int)saved.width; s.bit_depth = 12;
   CHECK(!dxva_hevc_params_from_rh265(&p, &s));
   CHECK(!dxva_hevc_params_from_rh265(NULL, &s));
   CHECK(!dxva_hevc_params_from_rh265(&p, NULL));
   memset(s.list4, 16, sizeof(s.list4)); memset(s.list8, 16, sizeof(s.list8));
   memset(s.list16, 16, sizeof(s.list16)); memset(s.list32, 16, sizeof(s.list32));
   memset(s.dc16, 17, sizeof(s.dc16)); memset(s.dc32, 18, sizeof(s.dc32));
   CHECK(dxva_hevc_qmatrix_from_rh265(bytes, &s));
   for (i = 0; i < 992; i++) CHECK(bytes[i] == 16);
   for (i = 992; i < 998; i++) CHECK(bytes[i] == 17);
   CHECK(bytes[998] == 18 && bytes[999] == 18);
   s.dc32[1] = 0;
   CHECK(!dxva_hevc_qmatrix_from_rh265(bytes, &s));
   s.scaling_list_enabled = 0;
   CHECK(!dxva_hevc_qmatrix_from_rh265(bytes, &s));
   for (i = 0; i < 1000; i++) CHECK(!bytes[i]);
   CHECK(!dxva_hevc_qmatrix_from_rh265(bytes, NULL));
   CHECK(!dxva_hevc_qmatrix_from_rh265(NULL, &s));
}


static void test_rps_adapter(void)
{
   static rh265_slice_info_t s;
   static dxva_hevc_picture_t saved;
   unsigned i;
   setup(); memset(&s, 0, sizeof(s));
   s.nal_type = 1; s.first_slice_in_pic = 1; s.slice_type = 1;
   s.short_term_rps_sps_idx = -1; s.short_term_rps_bits = 16;
   s.num_negative = 2; s.num_positive = 1;
   s.delta_poc_before[0] = -2; s.delta_poc_before[1] = -4;
   s.delta_poc_after[0] = 2; s.used_before[0] = s.used_after[0] = 1;
   CHECK(dxva_hevc_slice_from_rh265(&p, &s));
   CHECK(p.num_before == 1 && p.before[0] == 0 && p.num_after == 1 && p.after[0] == 1);
   CHECK(p.num_longterm == 0 && p.rps_bits == 16 && !p.delta_pocs);
   for (i = 1; i < 8; i++) CHECK(p.before[i] == 0xff && p.after[i] == 0xff);
#ifdef _WIN32
   check_sdk();
#endif
   saved = p;
#define RPS_BAD(change) do { change; CHECK(!dxva_hevc_slice_from_rh265(&p, &s)); \
   CHECK(!memcmp(&p, &saved, sizeof(p))); } while (0)
   RPS_BAD(s.used_before[0] = 2); s.used_before[0] = 1;
   RPS_BAD(s.delta_poc_before[0] = -3); s.delta_poc_before[0] = -2;
   RPS_BAD(s.delta_poc_before[1] = -1); s.delta_poc_before[1] = -4;
   RPS_BAD(s.poc_lsb = 1); s.poc_lsb = 0;
   RPS_BAD(s.short_term_rps_sps_idx = 5); s.short_term_rps_sps_idx = -1;
   RPS_BAD(s.predictor_delta_pocs = 33); s.predictor_delta_pocs = 0;
   RPS_BAD(s.num_positive = 17); s.num_positive = 1;
   RPS_BAD(s.nal_type = 10); s.nal_type = 1;
   RPS_BAD(s.short_term_rps_bits = 65536); s.short_term_rps_bits = 16;
   s.first_slice_in_pic = 0; s.slice_type = 2;
   CHECK(dxva_hevc_slice_from_rh265(&p, &s) && !(p.coding_settings & (1u << 18)));
   RPS_BAD(s.short_term_rps_bits = 15); s.short_term_rps_bits = 16;
   s.first_slice_in_pic = 1; s.slice_type = 2;
   CHECK(dxva_hevc_slice_from_rh265(&p, &s) && (p.coding_settings & (1u << 18)));
   s.first_slice_in_pic = 0; s.slice_type = 1;
   CHECK(dxva_hevc_slice_from_rh265(&p, &s) && !(p.coding_settings & (1u << 18)));
   setup(); p.poc = INT32_MAX;
   s.first_slice_in_pic = 1; s.poc_lsb = 255; saved = p;
   CHECK(!dxva_hevc_slice_from_rh265(&p, &s) && !memcmp(&p, &saved, sizeof(p)));
   setup(); p.refs[2].poc = -2; p.refs[2].long_term = 0; saved = p;
   s.poc_lsb = 0;
   CHECK(!dxva_hevc_slice_from_rh265(&p, &s) && !memcmp(&p, &saved, sizeof(p)));
   setup(); p.poc = -256; p.refs[0].poc = -258; p.refs[1].poc = -254;
   CHECK(dxva_hevc_slice_from_rh265(&p, &s));
   setup(); p.slot = 0; p.max_dec_pic_buffering = 16; p.num_refs = 9;
   memset(&s, 0, sizeof(s)); s.nal_type = 1; s.slice_type = 1;
   s.first_slice_in_pic = 1; s.short_term_rps_sps_idx = -1;
   s.short_term_rps_bits = 20; s.num_negative = 9;
   for (i = 0; i < 9; i++)
   {
      p.refs[i].slot = i + 1; p.refs[i].poc = -(int32_t)i - 1;
      p.refs[i].long_term = 0;
      s.delta_poc_before[i] = -(int32_t)i - 1; s.used_before[i] = 1;
   }
   saved = p;
   CHECK(!dxva_hevc_slice_from_rh265(&p, &s) && !memcmp(&p, &saved, sizeof(p)));
   setup(); p.num_refs = 0;
   memset(&s, 0, sizeof(s)); s.nal_type = 19; s.slice_type = 2;
   s.first_slice_in_pic = 1; s.short_term_rps_sps_idx = -1;
   CHECK(dxva_hevc_slice_from_rh265(&p, &s));
   CHECK((p.coding_settings & (7u << 16)) == (7u << 16));
#ifdef _WIN32
   check_sdk();
#endif
   CHECK(!dxva_hevc_slice_from_rh265(NULL, &s));
   CHECK(!dxva_hevc_slice_from_rh265(&p, NULL));
}


static void test_borrowed_packing(void)
{
   uint8_t a[] = {0x26,1,0xaa,0,0,3,1}, b[] = {2,1,0xbb};
   uint8_t original_a[sizeof(a)], original_b[sizeof(b)];
   uint8_t packed[129], records[22], expected[20];
   uint8_t full[125];
   rh265_nal_t slices[2];
   uint32_t offsets[2] = {0,10};
   size_t written;
   unsigned i;
   memcpy(original_a, a, sizeof(a)); memcpy(original_b, b, sizeof(b));
   slices[0].data = a; slices[0].size = sizeof(a);
   slices[1].data = b; slices[1].size = sizeof(b);
   memset(packed, 0xa5, sizeof(packed)); memset(records, 0xa5, sizeof(records));
   CHECK(dxva_hevc_pack_slices(packed, 128, records, 20, slices, 2, &written));
   CHECK(written == 128 && packed[128] == 0xa5 && records[20] == 0xa5 && records[21] == 0xa5);
   CHECK(!packed[0] && !packed[1] && packed[2] == 1 && !memcmp(packed + 3, a, sizeof(a)));
   CHECK(!packed[10] && !packed[11] && packed[12] == 1 && !memcmp(packed + 13, b, sizeof(b)));
   for (i = 16; i < 128; i++) CHECK(!packed[i]);
   CHECK(!memcmp(original_a, a, sizeof(a)) && !memcmp(original_b, b, sizeof(b)));
   CHECK(dxva_hevc_slices(expected, 20, packed, 16, offsets, 2, 128));
   CHECK(!memcmp(expected, records, sizeof(expected)));
#ifdef _WIN32
   {
      DXVA_Slice_HEVC_Short sdk[2];
      memset(sdk, 0, sizeof(sdk)); sdk[0].SliceBytesInBuffer = 10;
      sdk[1].BSNALunitDataLocation = 10; sdk[1].SliceBytesInBuffer = 118;
      CHECK(!memcmp(sdk, records, sizeof(sdk)));
   }
#endif
#define PACK_BAD(cap, rec_cap, count) do { \
   memset(packed, 0xa5, sizeof(packed)); memset(records, 0xa5, sizeof(records)); written = 999; \
   CHECK(!dxva_hevc_pack_slices(packed, cap, records, rec_cap, slices, count, &written)); \
   CHECK(!written); for (i = 0; i < sizeof(packed); i++) CHECK(packed[i] == 0xa5); \
   for (i = 0; i < sizeof(records); i++) CHECK(records[i] == 0xa5); } while (0)
   PACK_BAD(127, 20, 2); PACK_BAD(128, 19, 2); PACK_BAD(128, 20, 0);
   b[1] = 0; PACK_BAD(128, 20, 2); b[1] = 1;
   b[0] = 66; PACK_BAD(128, 20, 2); b[0] = 2;
   slices[1].size = UINT32_MAX; PACK_BAD(128, 20, 2); slices[1].size = sizeof(b);
   slices[0].size = UINT32_MAX - 3; PACK_BAD(128, 20, 1); slices[0].size = sizeof(a);
   slices[1].data = NULL; PACK_BAD(128, 20, 2); slices[1].data = b;
   memset(full, 0xab, sizeof(full)); full[0] = 2; full[1] = 1;
   slices[0].data = full; slices[0].size = sizeof(full);
   CHECK(dxva_hevc_pack_slices(packed, 128, records, 10, slices, 1, &written));
   CHECK(written == 128 && get32(records + 4) == 128 && !memcmp(packed + 3, full, sizeof(full)));
   CHECK(!dxva_hevc_pack_slices(packed, 128, records, 10, slices, 1, NULL));
   CHECK(!dxva_hevc_pack_slices(NULL, 128, records, 10, slices, 1, &written));
   CHECK(!dxva_hevc_pack_slices(packed, 128, NULL, 10, slices, 1, &written));
   CHECK(!dxva_hevc_pack_slices(packed, 128, records, 10, NULL, 1, &written));
}

static void test_slices(void)
{
   uint8_t data[] = {0,0,1,0x26,1,0xab,0,0,0,1,2,1,0xcd};
   uint8_t saved[sizeof(data)], output[22];
   uint32_t offsets[2] = {0,6};
   memcpy(saved, data, sizeof(data)); memset(output, 0xaa, sizeof(output));
   CHECK(dxva_hevc_slices(output, 20, data, sizeof(data), offsets, 2, 128));
   CHECK(get32(output) == 0 && get32(output + 4) == 6);
   CHECK(get32(output + 10) == 6 && get32(output + 14) == 122);
   CHECK(!get16(output + 8) && !get16(output + 18) && !memcmp(saved, data, sizeof(data)));
   CHECK(output[20] == 0xaa && output[21] == 0xaa);
#ifdef _WIN32
   {
      DXVA_Slice_HEVC_Short sdk[2];
      memset(sdk, 0, sizeof(sdk)); sdk[0].SliceBytesInBuffer = 6;
      sdk[1].BSNALunitDataLocation = 6; sdk[1].SliceBytesInBuffer = 122;
      CHECK(sizeof(sdk) == 20 && !memcmp(sdk, output, sizeof(sdk)));
   }
#endif
   CHECK(!dxva_hevc_slices(output, 19, data, sizeof(data), offsets, 2, 128));
   CHECK(!dxva_hevc_slices(output, 20, data, sizeof(data), offsets, 2, 127));
   offsets[1] = 2; CHECK(!dxva_hevc_slices(output, 20, data, sizeof(data), offsets, 2, 128));
   offsets[1] = 6; data[11] = 0; CHECK(!dxva_hevc_slices(output, 20, data, sizeof(data), offsets, 2, 128));
   data[11] = 9; CHECK(!dxva_hevc_slices(output, 20, data, sizeof(data), offsets, 2, 128));
   data[11] = 1; data[10] = 0x40; CHECK(!dxva_hevc_slices(output, 20, data, sizeof(data), offsets, 2, 128));
   data[10] = 0x82; CHECK(!dxva_hevc_slices(output, 20, data, sizeof(data), offsets, 2, 128));
}
int main(void)
{
   test_picture(); test_rejection(); test_scaling(); test_raster_scaling(); test_parser_adapter(); test_rps_adapter(); test_borrowed_packing(); test_slices();
   printf("HEVC DXVA buffers: %u failures\n", failures);
   return failures ? 1 : 0;
}
