/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_SLICE_H
#define RARCH_VAAPI_HEVC_SLICE_H

#include "vaapi_hevc.h"
#include <limits.h>

/* Parsed, independent Main/Main10 slice and a picture already prepared by
 * vaapi_hevc_picture. Reference indices address that picture's ReferenceFrames,
 * never surface IDs. No allocation, native operation or DPB advancement.
 * Failure preserves out. Outputs must be disjoint from the inputs. Substream
 * lengths are supplied separately by the caller when packing a NORMAL AU. */
static INLINE bool vaapi_hevc_long_slice(VASliceParameterBufferHEVC *out,
      const rh265_video_params_t *params, const rh265_slice_decode_t *slice,
      const VAPictureParameterBufferHEVC *picture, size_t size, unsigned offset,
      bool last)
{
   VASliceParameterBufferHEVC v;
   const rh265_slice_info_t *h;
   uint8_t groups[3][16], counts[3], order[16];
   unsigned group, i, j, list, total = 0, seen = 0;
   size_t header;
   int qp_delta;
   if (!out || !params || !slice || !picture || size < 3 || size > UINT_MAX
         || offset > UINT_MAX - size
         || (params->bit_depth != 8 && params->bit_depth != 10)
         || params->log2_max_poc_lsb < 4 || params->log2_max_poc_lsb > 16
         || params->width <= 0 || params->width > 65535 || params->height <= 0
         || params->height > 65535 || params->log2_ctb < 3
         || params->log2_ctb > 6)
      return false;
   h = &slice->picture;
   if (h->slice_type < 0 || h->slice_type > 2 || h->pps_id != params->pps_id
         || h->slice_segment_address < 0 || h->num_negative < 0
         || h->num_negative > 16 || h->num_positive < 0 || h->num_positive > 16
         || h->long_term.count > 16 || h->header_bits % 8
         || h->header_bits / 8 > size - 2 || h->header_bytes > size
         || slice->num_entry_points > 65535 || slice->luma_log2_denom < 0
         || slice->luma_log2_denom > 7 || slice->chroma_log2_denom < 0
         || slice->chroma_log2_denom > 7
         || slice->qp < -(params->bit_depth - 8) * 6 || slice->qp > 51
         || params->init_qp < -(params->bit_depth - 8) * 6
         || params->init_qp > 51 || slice->cb_qp_offset < -12
         || slice->cb_qp_offset > 12 || slice->cr_qp_offset < -12
         || slice->cr_qp_offset > 12 || slice->beta_offset_div2 < -6
         || slice->beta_offset_div2 > 6 || slice->tc_offset_div2 < -6
         || slice->tc_offset_div2 > 6)
      return false;
   header = 2 + h->header_bits / 8;
   if (h->header_bytes < header || h->header_bytes - header > 65535)
      return false;
   if ((unsigned)h->slice_segment_address
         >= (((unsigned)params->width + (1u << params->log2_ctb) - 1)
                  >> params->log2_ctb)
                  * (((unsigned)params->height + (1u << params->log2_ctb) - 1)
                        >> params->log2_ctb))
      return false;
   memset(counts, 0, sizeof(counts));
   for (group = 0; group < 3; group++)
   {
      unsigned count =
            group == 2 ? h->long_term.count
                       : (unsigned)(group ? h->num_positive : h->num_negative);
      for (i = 0; i < count; i++)
      {
         unsigned active, found = 15;
         uint32_t flag = group == 2 ? VA_PICTURE_HEVC_RPS_LT_CURR
                         : group    ? VA_PICTURE_HEVC_RPS_ST_CURR_AFTER
                                    : VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE;
         int64_t poc;
         uint32_t mask = (1u << params->log2_max_poc_lsb) - 1;
         if (group == 2)
         {
            if (h->long_term.used[i] > 1 || h->long_term.msb_present[i] > 1
                  || h->long_term.poc_lsb[i] > mask)
               return false;
            active = h->long_term.used[i];
            poc    = (int64_t)picture->CurrPic.pic_order_cnt - h->poc_lsb
                  + h->long_term.poc_lsb[i]
                  - (int64_t)h->long_term.msb_cycle[i] * (mask + 1);
         }
         else
         {
            const int32_t *delta =
                  group ? h->delta_poc_after : h->delta_poc_before;
            const uint8_t *used = group ? h->used_after : h->used_before;
            if (used[i] > 1 || (group ? delta[i] <= 0 : delta[i] >= 0)
                  || (i
                        && (group ? delta[i] <= delta[i - 1]
                                  : delta[i] >= delta[i - 1])))
               return false;
            active = used[i];
            poc    = (int64_t)picture->CurrPic.pic_order_cnt + delta[i];
         }
         if (!active)
            continue;
         for (j = 0; j < 15; j++)
         {
            const VAPictureHEVC *ref = &picture->ReferenceFrames[j];
            bool match = group == 2 && !h->long_term.msb_present[i]
                               ? ((uint32_t)ref->pic_order_cnt & mask)
                                       == h->long_term.poc_lsb[i]
                               : (int64_t)ref->pic_order_cnt == poc;
            if (!(ref->flags & VA_PICTURE_HEVC_INVALID) && (ref->flags & flag)
                  && match
                  && !!(ref->flags & VA_PICTURE_HEVC_LONG_TERM_REFERENCE)
                           == (group == 2))
            {
               if (found != 15 || ref->picture_id == VA_INVALID_SURFACE)
                  return false;
               found = j;
            }
         }
         if (found == 15 || total >= 8 || (seen & (1u << found)))
            return false;
         seen |= 1u << found;
         total++;
         groups[group][counts[group]++] = (uint8_t)found;
      }
   }
   memset(&v, 0, sizeof(v));
   memset(v.RefPicList, 0xff, sizeof(v.RefPicList));
   v.collocated_ref_idx = 0xff;
   for (list = 0; list < (h->slice_type == 0   ? 2u
                          : h->slice_type == 1 ? 1u
                                               : 0u);
         list++)
   {
      unsigned n = 0;
      int refs   = slice->num_refs[list];
      if (!total || refs < 1 || refs > 15 || slice->list_modified[list] < 0
            || slice->list_modified[list] > 1)
         return false;
      for (group = 0; group < 3; group++)
      {
         unsigned g = group == 2 ? 2 : group ^ list;
         for (i = 0; i < counts[g]; i++)
            order[n++] = groups[g][i];
      }
      for (i = 0; i < (unsigned)refs; i++)
      {
         unsigned index = slice->list_modified[list]
                                ? slice->list_entry[list][i]
                                : i % total;
         int dw = slice->luma_weight[list][i] - (1 << slice->luma_log2_denom);
         int lo = slice->luma_offset[list][i];
         int8_t *luma_weight =
               list ? v.delta_luma_weight_l1 : v.delta_luma_weight_l0;
         int8_t *luma_offset = list ? v.luma_offset_l1 : v.luma_offset_l0;
         if (index >= total || dw < -128 || dw > 127 || lo < -128 || lo > 127)
            return false;
         v.RefPicList[list][i] = order[index];
         luma_weight[i]        = (int8_t)dw;
         luma_offset[i]        = (int8_t)lo;
         for (j = 0; j < 2; j++)
         {
            int cw = slice->chroma_weight[list][i][j]
                     - (1 << slice->chroma_log2_denom);
            int co = slice->chroma_offset[list][i][j];
            if (cw < -128 || cw > 127 || co < -128 || co > 127)
               return false;
            if (list)
            {
               v.delta_chroma_weight_l1[i][j] = (int8_t)cw;
               v.ChromaOffsetL1[i][j]         = (int8_t)co;
            }
            else
            {
               v.delta_chroma_weight_l0[i][j] = (int8_t)cw;
               v.ChromaOffsetL0[i][j]         = (int8_t)co;
            }
         }
      }
   }
   if (h->slice_type != 2)
   {
      if (slice->max_merge < 1 || slice->max_merge > 5)
         return false;
      v.num_ref_idx_l0_active_minus1 = (uint8_t)(slice->num_refs[0] - 1);
      if (h->slice_type == 0)
         v.num_ref_idx_l1_active_minus1 = (uint8_t)(slice->num_refs[1] - 1);
      v.five_minus_max_num_merge_cand = (uint8_t)(5 - slice->max_merge);
      if (slice->temporal_mvp)
      {
         unsigned cl = slice->collocated_from_l0 ? 0 : 1;
         if ((h->slice_type == 1 && cl) || slice->collocated_ref_idx < 0
               || slice->collocated_ref_idx >= slice->num_refs[cl])
            return false;
         v.collocated_ref_idx = (uint8_t)slice->collocated_ref_idx;
      }
   }
   qp_delta                         = slice->qp - params->init_qp;
   v.slice_data_size                = (uint32_t)size;
   v.slice_data_offset              = offset;
   v.slice_data_flag                = VA_SLICE_DATA_FLAG_ALL;
   v.slice_data_byte_offset         = (uint32_t)header;
   v.slice_data_num_emu_prevn_bytes = (uint16_t)(h->header_bytes - header);
   v.slice_segment_address          = (uint32_t)h->slice_segment_address;
   v.num_entry_point_offsets        = (uint16_t)slice->num_entry_points;
   v.slice_qp_delta                 = (int8_t)qp_delta;
   v.slice_cb_qp_offset             = (int8_t)slice->cb_qp_offset;
   v.slice_cr_qp_offset             = (int8_t)slice->cr_qp_offset;
   v.slice_beta_offset_div2         = (int8_t)slice->beta_offset_div2;
   v.slice_tc_offset_div2           = (int8_t)slice->tc_offset_div2;
   v.luma_log2_weight_denom         = (uint8_t)slice->luma_log2_denom;
   v.delta_chroma_log2_weight_denom =
         (int8_t)(slice->chroma_log2_denom - slice->luma_log2_denom);
   v.LongSliceFlags.fields.LastSliceOfPic = last;
   v.LongSliceFlags.fields.slice_type     = (unsigned)h->slice_type;
#define VAAPI_HEVC_LONG_FLAG(dst, src)                                         \
   v.LongSliceFlags.fields.dst = !!slice->src
   VAAPI_HEVC_LONG_FLAG(slice_sao_luma_flag, sao_luma);
   VAAPI_HEVC_LONG_FLAG(slice_sao_chroma_flag, sao_chroma);
   VAAPI_HEVC_LONG_FLAG(mvd_l1_zero_flag, mvd_l1_zero);
   VAAPI_HEVC_LONG_FLAG(cabac_init_flag, cabac_init);
   VAAPI_HEVC_LONG_FLAG(slice_temporal_mvp_enabled_flag, temporal_mvp);
   VAAPI_HEVC_LONG_FLAG(
         slice_deblocking_filter_disabled_flag, deblocking_filter_disabled);
   VAAPI_HEVC_LONG_FLAG(collocated_from_l0_flag, collocated_from_l0);
   VAAPI_HEVC_LONG_FLAG(slice_loop_filter_across_slices_enabled_flag,
         loop_filter_across_slices);
#undef VAAPI_HEVC_LONG_FLAG
   *out = v;
   return true;
}
#endif
