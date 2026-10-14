/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_DXVA_HEVC_RH265_H
#define RARCH_DXVA_HEVC_RH265_H
#include <formats/rh265.h>
#include <formats/rh265_hw.h>
#include "dxva_hevc.h"

/* Fill parameter fields while preserving picture POC, slots, RPS and IRAP
 * flags. Invalid or unsupported parameters leave the picture unchanged. */
static INLINE bool dxva_hevc_params_from_rh265(dxva_hevc_picture_t *picture,
      const rh265_video_params_t *s)
{
   dxva_hevc_picture_t p;
   uint8_t validation[DXVA_HEVC_PP_SIZE];
   if (!picture || !s || s->chroma_format_idc != 1 || s->pcm_enabled
         || s->tiles_enabled || s->dependent_slice_segments_enabled
         || s->num_extra_slice_header_bits < 0 || s->num_extra_slice_header_bits > 7)
      return false;
   p = *picture;
   p.width = s->width;
   p.height = s->height;
   p.min_cb_log2 = s->log2_min_cb;
   p.ctb_log2 = s->log2_ctb;
   p.min_tb_log2 = s->log2_min_tb;
   p.max_tb_log2 = s->log2_max_tb;
   p.bit_depth = s->bit_depth;
   p.max_poc_lsb_log2 = s->log2_max_poc_lsb;
   p.max_dec_pic_buffering = s->max_dec_pic_buffering;
   p.max_reorder_pics = s->max_num_reorder_pics;
   p.hierarchy_inter = s->max_transform_hierarchy_depth_inter;
   p.hierarchy_intra = s->max_transform_hierarchy_depth_intra;
   p.num_st_rps = s->num_st_rps;
   p.num_lt_rps = s->num_long_term_ref_pics;
   p.default_l0 = s->num_ref_idx_l0_default;
   p.default_l1 = s->num_ref_idx_l1_default;
   p.diff_qp_depth = s->diff_cu_qp_delta_depth;
   p.parallel_merge_log2 = s->log2_parallel_merge_level;
   p.init_qp = s->init_qp;
   p.cb_qp = s->cb_qp_offset;
   p.cr_qp = s->cr_qp_offset;
   p.beta_offset_div2 = s->beta_offset_div2;
   p.tc_offset_div2 = s->tc_offset_div2;
   p.coding_tools = (uint32_t)s->num_extra_slice_header_bits << 22;
   p.coding_tools |= (uint32_t)!!s->scaling_list_enabled << 0;
   p.coding_tools |= (uint32_t)!!s->amp_enabled << 1;
   p.coding_tools |= (uint32_t)!!s->sao_enabled << 2;
   p.coding_tools |= (uint32_t)!!s->long_term_ref_pics_present << 17;
   p.coding_tools |= (uint32_t)!!s->temporal_mvp_enabled << 18;
   p.coding_tools |= (uint32_t)!!s->strong_intra_smoothing << 19;
   p.coding_tools |= (uint32_t)!!s->output_flag_present << 21;
   p.coding_tools |= (uint32_t)!!s->sign_data_hiding << 25;
   p.coding_tools |= (uint32_t)!!s->cabac_init_present << 26;
   p.coding_settings &= UINT32_C(7) << 16;
   p.coding_settings |= (uint32_t)!!s->constrained_intra_pred << 0;
   p.coding_settings |= (uint32_t)!!s->transform_skip_enabled << 1;
   p.coding_settings |= (uint32_t)!!s->cu_qp_delta_enabled << 2;
   p.coding_settings |= (uint32_t)!!s->slice_chroma_qp_offsets_present << 3;
   p.coding_settings |= (uint32_t)!!s->weighted_pred << 4;
   p.coding_settings |= (uint32_t)!!s->weighted_bipred << 5;
   p.coding_settings |= (uint32_t)!!s->transquant_bypass_enabled << 6;
   p.coding_settings |= (uint32_t)!!s->entropy_coding_sync_enabled << 8;
   p.coding_settings |= (uint32_t)!!s->loop_filter_across_slices << 11;
   p.coding_settings |= (uint32_t)!!s->deblocking_filter_override_enabled << 12;
   p.coding_settings |= (uint32_t)!!s->deblocking_filter_disabled << 13;
   p.coding_settings |= (uint32_t)!!s->lists_modification_present << 14;
   p.coding_settings |= (uint32_t)!!s->slice_segment_header_extension_present << 15;
   if (!dxva_hevc_pic_params(validation, &p, 1)) return false;
   *picture = p;
   return true;
}

/* The caller supplies the full POC and live reference slots. This maps
 * current short-term RPS entries without changing DPB or frame ownership. */
static INLINE bool dxva_hevc_slice_from_rh265(dxva_hevc_picture_t *picture,
      const rh265_slice_info_t *s)
{
   dxva_hevc_picture_t p;
   uint8_t validation[DXVA_HEVC_PP_SIZE];
   unsigned list, i, j, type;
   if (!picture || !s || s->nal_type < 0 || s->slice_type < 0 || s->slice_type > 2
         || s->first_slice_in_pic < 0 || s->first_slice_in_pic > 1
         || s->num_negative < 0 || s->num_negative > 16
         || s->num_positive < 0 || s->num_positive > 16
         || s->long_term.count > 16
         || s->predictor_delta_pocs < 0 || s->predictor_delta_pocs > 32
         || s->short_term_rps_sps_idx < -1) return false;
   type = (unsigned)s->nal_type;
   if (!(type <= 9 || (type >= 16 && type <= 21))) return false;
   p = *picture;
   if (p.max_poc_lsb_log2 < 4 || p.max_poc_lsb_log2 > 16 || p.num_refs > 15
         || (s->short_term_rps_sps_idx >= 0 &&
            ((unsigned)s->short_term_rps_sps_idx >= p.num_st_rps
             || s->short_term_rps_bits || s->predictor_delta_pocs))) return false;
   if (type == 19 || type == 20)
   {
      if (s->slice_type != 2 || p.poc || s->poc_lsb || s->num_negative
            || s->num_positive || s->long_term.count || s->short_term_rps_bits || s->predictor_delta_pocs)
         return false;
   }
   else if ((s->short_term_rps_sps_idx == -1 && !s->short_term_rps_bits)
         || s->poc_lsb < 0 || (uint32_t)s->poc_lsb !=
         ((uint32_t)p.poc & ((1u << p.max_poc_lsb_log2) - 1))) return false;
   if (!s->first_slice_in_pic &&
         ((!!(p.coding_settings & (1u << 16)) != (type >= 16))
          || (!!(p.coding_settings & (1u << 17)) != (type == 19 || type == 20))))
      return false;
   i = (unsigned)(s->slice_type == 2 && (s->first_slice_in_pic
            || (p.coding_settings & (1u << 18))));
   p.coding_settings &= ~(UINT32_C(7) << 16);
   p.coding_settings |= (uint32_t)(type >= 16) << 16;
   p.coding_settings |= (uint32_t)(type == 19 || type == 20) << 17;
   p.coding_settings |= (uint32_t)i << 18;
   p.rps_bits = s->short_term_rps_bits;
   p.delta_pocs = (unsigned)s->predictor_delta_pocs;
   p.num_before = p.num_after = p.num_longterm = 0;
   memset(p.before, 0xff, sizeof(p.before));
   memset(p.after, 0xff, sizeof(p.after));
   memset(p.longterm, 0xff, sizeof(p.longterm));
   for (list = 0; list < 2; list++)
   {
      unsigned count = (unsigned)(list ? s->num_positive : s->num_negative);
      const int32_t *deltas = list ? s->delta_poc_after : s->delta_poc_before;
      const uint8_t *used = list ? s->used_after : s->used_before;
      unsigned *num = list ? &p.num_after : &p.num_before;
      uint8_t *set = list ? p.after : p.before;
      for (i = 0; i < count; i++)
      {
         unsigned found = p.num_refs;
         int64_t poc = (int64_t)p.poc + deltas[i];
         if (used[i] > 1 || (list ? deltas[i] <= 0 : deltas[i] >= 0)
               || (i && (list ? deltas[i] <= deltas[i-1] : deltas[i] >= deltas[i-1])))
            return false;
         if (!used[i]) continue;
         if (*num >= 8 || poc < INT32_MIN || poc > INT32_MAX) return false;
         for (j = 0; j < p.num_refs; j++)
            if (!p.refs[j].long_term && p.refs[j].poc == poc)
            {
               if (found != p.num_refs) return false;
               found = j;
            }
         if (found == p.num_refs) return false;
         set[(*num)++] = (uint8_t)found;
      }
   }
   for (i = 0; i < s->long_term.count; i++)
   {
      unsigned found = p.num_refs;
      uint32_t max_lsb = 1u << p.max_poc_lsb_log2;
      int64_t poc = (int64_t)p.poc - s->poc_lsb + s->long_term.poc_lsb[i]
         - (int64_t)s->long_term.msb_cycle[i] * max_lsb;
      if (s->long_term.used[i] > 1 || s->long_term.msb_present[i] > 1
            || s->long_term.poc_lsb[i] >= max_lsb
            || !(p.coding_tools & (1u << 17))) return false;
      if (!s->long_term.used[i]) continue;
      if (p.num_longterm == 8) return false;
      for (j = 0; j < p.num_refs; j++)
         if (p.refs[j].long_term && (s->long_term.msb_present[i]
                  ? p.refs[j].poc == poc
                  : ((uint32_t)p.refs[j].poc & (max_lsb - 1)) == s->long_term.poc_lsb[i]))
         {
            if (found != p.num_refs) return false;
            found = j;
         }
      if (found == p.num_refs) return false;
      p.longterm[p.num_longterm++] = (uint8_t)found;
   }
   if (!s->first_slice_in_pic &&
         (picture->rps_bits != p.rps_bits || picture->delta_pocs != p.delta_pocs
          || picture->num_before != p.num_before || picture->num_after != p.num_after
          || picture->num_longterm != p.num_longterm
          || memcmp(picture->before, p.before, p.num_before)
          || memcmp(picture->after, p.after, p.num_after)
          || memcmp(picture->longterm, p.longterm, p.num_longterm))) return false;
   if (!dxva_hevc_pic_params(validation, &p, 1)) return false;
   *picture = p;
   return true;
}

/* Serialize a prepared owner transaction; committing it is the caller's
 * responsibility after the backend accepts decode. No ownership changes. */
static INLINE bool dxva_hevc_picture_from_state(dxva_hevc_picture_t *out,
      const rh265_video_params_t *params, const rh265_access_unit_t *au,
      const rh265_hw_picture_t *state)
{
   dxva_hevc_picture_t p;
   uint8_t validation[DXVA_HEVC_PP_SIZE];
   unsigned i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!params || !au || !state || !state->serial || state->num_refs > 15
         || state->width != (unsigned)params->width || state->height != (unsigned)params->height
         || state->bit_depth != (unsigned)params->bit_depth
         || state->poc_log2 != (unsigned)params->log2_max_poc_lsb) return false;
   memset(&p, 0, sizeof(p));
   p.poc = state->poc; p.slot = state->slot; p.num_refs = state->num_refs;
   for (i = 0; i < p.num_refs; i++)
   {
      p.refs[i].poc = state->refs[i].poc; p.refs[i].slot = state->refs[i].slot;
      p.refs[i].long_term = state->refs[i].long_term;
   }
   if (!dxva_hevc_params_from_rh265(&p, params)
         || !dxva_hevc_slice_from_rh265(&p, &au->first)) return false;
   if (!au->intra) p.coding_settings &= ~(UINT32_C(1) << 18);
   if (!dxva_hevc_pic_params(validation, &p, 1)) return false;
   *out = p;
   return true;
}

/* Write borrowed VCL NALs straight to the backend's bitstream buffer.
 * Input/output spans must not overlap. Failure changes only written (zero).
 * Descriptors include final 128-byte padding; payload bytes stay escaped. */
static INLINE bool dxva_hevc_pack_slices(uint8_t *bitstream, size_t capacity,
      uint8_t *records, size_t records_capacity, const rh265_nal_t *slices,
      unsigned count, size_t *written)
{
   unsigned i, type;
   size_t total = 0, padded, pos = 0;
   if (!written) return false;
   *written = 0;
   if (!bitstream || !records || !slices || !count
         || count > records_capacity / DXVA_HEVC_SLICE_SIZE) return false;
   for (i = 0; i < count; i++)
   {
      const uint8_t *data = slices[i].data;
      size_t size = slices[i].size;
      if (!data || size < 2 || size > UINT32_MAX - 3
            || total > UINT32_MAX - 3 - size) return false;
      type = (data[0] >> 1) & 63;
      if ((data[0] & 0x81) || (data[1] >> 3) || !(data[1] & 7)
            || !(type <= 9 || (type >= 16 && type <= 21))) return false;
      total += 3 + size;
   }
   if (total > UINT32_MAX - 127) return false;
   padded = (total + 127) & ~(size_t)127;
   if (padded > capacity) return false;
   for (i = 0; i < count; i++)
   {
      uint8_t *record = records + (size_t)i * DXVA_HEVC_SLICE_SIZE;
      size_t bytes = 3 + slices[i].size;
      bitstream[pos] = bitstream[pos+1] = 0; bitstream[pos+2] = 1;
      memcpy(bitstream + pos + 3, slices[i].data, slices[i].size);
      dxva_hevc_put32(record, (uint32_t)pos);
      dxva_hevc_put32(record + 4, (uint32_t)(i + 1 == count ? padded - pos : bytes));
      dxva_hevc_put16(record + 8, 0);
      pos += bytes;
   }
   memset(bitstream + total, 0, padded - total);
   *written = padded;
   return true;
}

/* Enabled effective lists only. Caller owns the fixed submission buffer. */
static INLINE bool dxva_hevc_qmatrix_from_rh265(uint8_t *out,
      const rh265_video_params_t *s)
{
   dxva_hevc_scaling_t raster;
   if (!out) return false;
   if (!s || !s->scaling_list_enabled)
   {
      memset(out, 0, DXVA_HEVC_QM_SIZE);
      return false;
   }
   memcpy(raster.list4, s->list4, sizeof(raster.list4));
   memcpy(raster.list8, s->list8, sizeof(raster.list8));
   memcpy(raster.list16, s->list16, sizeof(raster.list16));
   memcpy(raster.list32, s->list32, sizeof(raster.list32));
   memcpy(raster.dc16, s->dc16, sizeof(raster.dc16));
   memcpy(raster.dc32, s->dc32, sizeof(raster.dc32));
   return dxva_hevc_qmatrix_raster(out, &raster);
}
#endif
