/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_hevc_slice.h"

static rh265_video_params_t params;
static rh265_slice_decode_t slice;
static VAPictureParameterBufferHEVC picture;
static VASliceParameterBufferHEVC native, saved;

static void setup(void)
{
   unsigned i, list, c;
   static const int poc[5]        = {265, 257, 262, 259, 263};
   static const uint32_t flags[5] = {VA_PICTURE_HEVC_RPS_ST_CURR_AFTER,
         VA_PICTURE_HEVC_RPS_LT_CURR | VA_PICTURE_HEVC_LONG_TERM_REFERENCE,
         VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE, 0,
         VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE};
   memset(&params, 0, sizeof(params));
   memset(&slice, 0, sizeof(slice));
   memset(&picture, 0, sizeof(picture));
   params.width                  = 192;
   params.height                 = 128;
   params.bit_depth              = 10;
   params.log2_ctb               = 6;
   params.log2_max_poc_lsb       = 8;
   params.init_qp                = 26;
   picture.CurrPic.pic_order_cnt = 264;
   for (i = 0; i < 15; i++)
   {
      picture.ReferenceFrames[i].picture_id =
            i < 5 ? 100 + i : VA_INVALID_SURFACE;
      picture.ReferenceFrames[i].flags =
            i < 5 ? flags[i] : VA_PICTURE_HEVC_INVALID;
      picture.ReferenceFrames[i].pic_order_cnt = i < 5 ? poc[i] : 0;
   }
   slice.picture.header_bits         = 80;
   slice.picture.header_bytes        = 14;
   slice.picture.poc_lsb             = 8;
   slice.picture.num_negative        = 2;
   slice.picture.delta_poc_before[0] = -1;
   slice.picture.delta_poc_before[1] = -2;
   slice.picture.used_before[0] = slice.picture.used_before[1] = 1;
   slice.picture.num_positive                                  = 1;
   slice.picture.delta_poc_after[0]                            = 1;
   slice.picture.used_after[0]                                 = 1;
   slice.picture.long_term.count                               = 1;
   slice.picture.long_term.used[0]                             = 1;
   slice.picture.long_term.msb_present[0]                      = 1;
   slice.picture.long_term.poc_lsb[0]                          = 1;
   slice.num_refs[0] = slice.num_refs[1] = 6;
   slice.max_merge                       = 3;
   slice.qp                              = 23;
   slice.cb_qp_offset                    = -2;
   slice.cr_qp_offset                    = 3;
   slice.beta_offset_div2                = -6;
   slice.tc_offset_div2                  = 6;
   slice.temporal_mvp                    = 1;
   slice.collocated_ref_idx              = 5;
   slice.collocated_from_l0              = 1;
   slice.sao_luma = slice.sao_chroma = 1;
   slice.cabac_init = slice.mvd_l1_zero = 1;
   slice.loop_filter_across_slices      = 1;
   slice.num_entry_points               = 2;
   for (list = 0; list < 2; list++)
      for (i = 0; i < 15; i++)
      {
         slice.luma_weight[list][i] = 1;
         for (c = 0; c < 2; c++)
            slice.chroma_weight[list][i][c] = 1;
      }
}

static void mapping(void)
{
   unsigned i;
   static const uint8_t l0[6] = {4, 2, 0, 1, 4, 2};
   static const uint8_t l1[6] = {0, 4, 2, 1, 0, 4};
   setup();
   assert(vaapi_hevc_long_slice(
         &native, &params, &slice, &picture, 100, 200, true));
   assert(!memcmp(native.RefPicList[0], l0, 6));
   assert(!memcmp(native.RefPicList[1], l1, 6));
   for (i = 6; i < 15; i++)
      assert(native.RefPicList[0][i] == 255 && native.RefPicList[1][i] == 255);
   assert(native.collocated_ref_idx == 5);
   assert(native.slice_data_byte_offset == 12);
   assert(native.slice_data_num_emu_prevn_bytes == 2);
   assert(native.slice_data_offset == 200 && native.slice_data_size == 100);
   assert(native.num_entry_point_offsets == 2
          && !native.entry_offset_to_subset_array);
   assert(native.slice_qp_delta == -3 && native.slice_cb_qp_offset == -2);
   assert(
         native.slice_cr_qp_offset == 3 && native.slice_beta_offset_div2 == -6);
   assert(native.slice_tc_offset_div2 == 6
          && native.five_minus_max_num_merge_cand == 2);
   assert(native.LongSliceFlags.fields.LastSliceOfPic);
   assert(native.LongSliceFlags.fields.slice_sao_luma_flag);
   assert(native.LongSliceFlags.fields.slice_sao_chroma_flag);
   assert(native.LongSliceFlags.fields.mvd_l1_zero_flag);
   assert(native.LongSliceFlags.fields.cabac_init_flag);
   assert(native.LongSliceFlags.fields.slice_temporal_mvp_enabled_flag);
   assert(native.LongSliceFlags.fields.collocated_from_l0_flag);
   assert(native.LongSliceFlags.fields
                .slice_loop_filter_across_slices_enabled_flag);
   for (i = 0; i < VA_PADDING_LOW - 2; i++)
      assert(!native.va_reserved[i]);
   slice.list_modified[0] = slice.list_modified[1] = 1;
   for (i = 0; i < 6; i++)
      slice.list_entry[0][i] = slice.list_entry[1][i] = (uint8_t)(3 - i % 4);
   assert(vaapi_hevc_long_slice(
         &native, &params, &slice, &picture, 100, 0, false));
   assert(native.RefPicList[0][0] == 1 && native.RefPicList[1][0] == 1);
   assert(native.RefPicList[0][1] == 0 && native.RefPicList[1][1] == 2);
   assert(!native.LongSliceFlags.fields.LastSliceOfPic);
   slice.picture.long_term.msb_present[0] = 0;
   assert(vaapi_hevc_long_slice(
         &native, &params, &slice, &picture, 100, 0, true));
   setup();
   slice.picture.slice_type = 1;
   assert(vaapi_hevc_long_slice(
         &native, &params, &slice, &picture, 100, 0, true));
   assert(native.RefPicList[1][0] == 255
          && !native.num_ref_idx_l1_active_minus1);
   slice.picture.slice_type = 2;
   slice.temporal_mvp       = 0;
   assert(vaapi_hevc_long_slice(
         &native, &params, &slice, &picture, 100, 0, true));
   assert(native.RefPicList[0][0] == 255 && native.collocated_ref_idx == 255);
}

static void weights(void)
{
   setup();
   slice.weighted        = 1;
   slice.luma_log2_denom = slice.chroma_log2_denom = 7;
   slice.luma_weight[0][0]                         = 255;
   slice.luma_weight[1][0]                         = 0;
   slice.luma_offset[0][0]                         = -128;
   slice.luma_offset[1][0]                         = 127;
   slice.chroma_weight[0][0][0]                    = 255;
   slice.chroma_weight[1][0][1]                    = 0;
   slice.chroma_offset[0][0][0]                    = -128;
   slice.chroma_offset[1][0][1]                    = 127;
   assert(vaapi_hevc_long_slice(
         &native, &params, &slice, &picture, 100, 0, true));
   assert(native.delta_luma_weight_l0[0] == 127
          && native.delta_luma_weight_l1[0] == -128);
   assert(native.luma_offset_l0[0] == -128 && native.luma_offset_l1[0] == 127);
   assert(native.delta_chroma_weight_l0[0][0] == 127
          && native.delta_chroma_weight_l1[0][1] == -128);
   assert(native.ChromaOffsetL0[0][0] == -128
          && native.ChromaOffsetL1[0][1] == 127);
}

static void refusal(void)
{
   setup();
   memset(&native, 0xa5, sizeof(native));
   saved = native;
#define REFUSE(change, restore)                                                \
   do                                                                          \
   {                                                                           \
      change;                                                                  \
      assert(!vaapi_hevc_long_slice(                                           \
            &native, &params, &slice, &picture, 100, 0, true));                \
      assert(!memcmp(&native, &saved, sizeof(native)));                        \
      restore;                                                                 \
   } while (0)
   REFUSE(slice.picture.num_negative = 17, slice.picture.num_negative = 2);
   REFUSE(slice.picture.delta_poc_before[1] = -1,
         slice.picture.delta_poc_before[1]  = -2);
   REFUSE(picture.ReferenceFrames[4].flags = 0,
         picture.ReferenceFrames[4].flags = VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE);
   REFUSE(slice.num_refs[0] = 16, slice.num_refs[0] = 6);
   REFUSE(slice.num_refs[1] = 0, slice.num_refs[1] = 6);
   REFUSE(slice.list_modified[0] = 2, slice.list_modified[0] = 0);
   slice.list_modified[0] = 1;
   REFUSE(slice.list_entry[0][0] = 4, slice.list_entry[0][0] = 0);
   slice.list_modified[0] = 0;
   REFUSE(slice.collocated_ref_idx = 6, slice.collocated_ref_idx = 5);
   REFUSE(slice.max_merge = 0, slice.max_merge = 3);
   REFUSE(slice.picture.header_bits = 81, slice.picture.header_bits = 80);
   REFUSE(slice.picture.header_bytes = 11, slice.picture.header_bytes = 14);
   REFUSE(slice.picture.header_bytes = 101, slice.picture.header_bytes = 14);
   REFUSE(slice.picture.slice_segment_address = 6,
         slice.picture.slice_segment_address  = 0);
   REFUSE(params.log2_ctb = 31, params.log2_ctb = 6);
   REFUSE(params.width = INT_MAX, params.width = 192);
   REFUSE(slice.luma_log2_denom = 8, slice.luma_log2_denom = 0);
   REFUSE(slice.luma_weight[0][0] = 129, slice.luma_weight[0][0] = 1);
   REFUSE(slice.luma_offset[1][0] = -129, slice.luma_offset[1][0] = 0);
   REFUSE(
         slice.chroma_weight[1][0][0] = -128, slice.chroma_weight[1][0][0] = 1);
   REFUSE(slice.chroma_offset[0][0][1] = 128, slice.chroma_offset[0][0][1] = 0);
   REFUSE(slice.qp = 52, slice.qp = 23);
   REFUSE(slice.num_entry_points = 65536, slice.num_entry_points = 2);
   REFUSE(slice.picture.long_term.msb_cycle[0] = 1,
         slice.picture.long_term.msb_cycle[0]  = 0);
   slice.picture.long_term.msb_present[0] = 0;
   picture.ReferenceFrames[3] = picture.ReferenceFrames[1];
   picture.ReferenceFrames[3].pic_order_cnt += 256;
   assert(!vaapi_hevc_long_slice(
         &native, &params, &slice, &picture, 100, 0, true));
   assert(!memcmp(&native, &saved, sizeof(native)));
   setup();
   REFUSE(picture.ReferenceFrames[1].picture_id = VA_INVALID_SURFACE,
         picture.ReferenceFrames[1].picture_id = 101);
   REFUSE(slice.picture.used_before[0] = 2, slice.picture.used_before[0] = 1);
   REFUSE(slice.picture.long_term.poc_lsb[0] = 256,
         slice.picture.long_term.poc_lsb[0] = 1);
#undef REFUSE
   assert(!vaapi_hevc_long_slice(
         &native, &params, &slice, &picture, 100, UINT_MAX, true));
   assert(!memcmp(&native, &saved, sizeof(native)));
}

int main(void)
{
   mapping();
   weights();
   refusal();
   puts("PASS: VA HEVC long-slice reference order, modifications, long-term matching, weights and header offsets; no native device execution");
   return 0;
}
