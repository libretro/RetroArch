/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_PICTURE_H
#define RARCH_VAAPI_AV1_PICTURE_H
#include "vaapi_av1_tiles.h"

static INLINE int vaapi_av1_clip16(int64_t value)
{
   return value < -32768 ? -32768 : value > 32767 ? 32767 : (int)value;
}

static INLINE int64_t vaapi_av1_round(int64_t value, unsigned shift)
{
   int64_t half = (int64_t)1 << (shift - 1);
   return value < 0 ? -((-value + half) >> shift) : (value + half) >> shift;
}

/* AV1 7.11.3.6/7: the specified reciprocal table is nearest-integer division.
 */
static INLINE unsigned vaapi_av1_warp_invalid(const int32_t matrix[6])
{
   unsigned exponent = 0, fraction, divisor, shift, i;
   int alpha, beta, gamma, delta;
   int64_t reciprocal, v, w;
   if (!matrix)
      return 1;
   for (i = 2; i < 6; i++)
   {
      int center = i == 2 || i == 5 ? 65536 : 0;
      if (matrix[i] < center - 8192 || matrix[i] > center + 8192)
         return 1;
   }
   if (matrix[2] <= 0)
      return 1;
   divisor = (unsigned)matrix[2];
   while (divisor >> (exponent + 1))
      exponent++;
   fraction = divisor - (1u << exponent);
   if (exponent > 8)
      fraction = (fraction + (1u << (exponent - 9))) >> (exponent - 8);
   else
      fraction <<= 8 - exponent;
   reciprocal = (4194304u + (256 + fraction) / 2) / (256 + fraction);
   shift = exponent + 14;
   v = (int64_t)matrix[4] * 65536;
   w = (int64_t)matrix[3] * matrix[4];
   alpha = vaapi_av1_clip16((int64_t)matrix[2] - 65536);
   beta = vaapi_av1_clip16(matrix[3]);
   gamma = vaapi_av1_clip16(vaapi_av1_round(v * reciprocal, shift));
   delta = vaapi_av1_clip16(
         (int64_t)matrix[5] - vaapi_av1_round(w * reciprocal, shift) - 65536);
   alpha = (int)vaapi_av1_round(alpha, 6) * 64;
   beta = (int)vaapi_av1_round(beta, 6) * 64;
   gamma = (int)vaapi_av1_round(gamma, 6) * 64;
   delta = (int)vaapi_av1_round(delta, 6) * 64;
   if (alpha < 0)
      alpha = -alpha;
   if (beta < 0)
      beta = -beta;
   if (gamma < 0)
      gamma = -gamma;
   if (delta < 0)
      delta = -delta;
   return 4 * alpha + 7 * beta >= 65536 || 4 * gamma + 4 * delta >= 65536;
}

static INLINE bool vaapi_av1_grain(
      VAFilmGrainStructAV1 *out, const rav1_grain_t *g)
{
   VAFilmGrainStructAV1 value;
   unsigned i, plane, positions;
   if (!out || !g || g->apply > 1
         || vaapi_av1_tiles_overlap(out, sizeof(*out), g, sizeof(*g)))
      return false;
   memset(&value, 0, sizeof(value));
   if (!g->apply)
   {
      *out = value;
      return true;
   }
   if (g->seed > 65535 || g->scaling_shift < 8 || g->scaling_shift > 11
         || g->ar_coeff_lag > 3 || g->ar_coeff_shift < 6
         || g->ar_coeff_shift > 9 || g->scale_shift > 3
         || g->chroma_scaling_from_luma > 1 || g->overlap > 1
         || g->clip_to_restricted_range > 1 || g->num_points[0] > 14
         || g->num_points[1] > 10 || g->num_points[2] > 10)
      return false;
   value.film_grain_info_fields.bits.apply_grain = 1;
   value.film_grain_info_fields.bits.chroma_scaling_from_luma
         = g->chroma_scaling_from_luma;
   value.film_grain_info_fields.bits.grain_scaling_minus_8
         = g->scaling_shift - 8;
   value.film_grain_info_fields.bits.ar_coeff_lag = g->ar_coeff_lag;
   value.film_grain_info_fields.bits.ar_coeff_shift_minus_6
         = g->ar_coeff_shift - 6;
   value.film_grain_info_fields.bits.grain_scale_shift = g->scale_shift;
   value.film_grain_info_fields.bits.overlap_flag = g->overlap;
   value.film_grain_info_fields.bits.clip_to_restricted_range
         = g->clip_to_restricted_range;
   value.grain_seed = (uint16_t)g->seed;
   for (plane = 0; plane < 3; plane++)
      for (i = 1; i < g->num_points[plane]; i++)
         if (g->point_value[plane][i] <= g->point_value[plane][i - 1])
            return false;
   for (i = 0; i < 2; i++)
      if (g->chroma_mult[i] > 255 || g->chroma_luma_mult[i] > 255
            || g->chroma_offset[i] > 511)
         return false;
   value.num_y_points = g->num_points[0];
   value.num_cb_points = g->num_points[1];
   value.num_cr_points = g->num_points[2];
   memcpy(value.point_y_value, g->point_value[0], g->num_points[0]);
   memcpy(value.point_y_scaling, g->point_scaling[0], g->num_points[0]);
   memcpy(value.point_cb_value, g->point_value[1], g->num_points[1]);
   memcpy(value.point_cb_scaling, g->point_scaling[1], g->num_points[1]);
   memcpy(value.point_cr_value, g->point_value[2], g->num_points[2]);
   memcpy(value.point_cr_scaling, g->point_scaling[2], g->num_points[2]);
   positions = 2 * g->ar_coeff_lag * (g->ar_coeff_lag + 1);
   if (g->num_points[0])
      memcpy(value.ar_coeffs_y, g->ar_coeffs[0], positions);
   positions += !!g->num_points[0];
   if (g->chroma_scaling_from_luma || g->num_points[1])
      memcpy(value.ar_coeffs_cb, g->ar_coeffs[1], positions);
   if (g->chroma_scaling_from_luma || g->num_points[2])
      memcpy(value.ar_coeffs_cr, g->ar_coeffs[2], positions);
   value.cb_mult = (uint8_t)g->chroma_mult[0];
   value.cb_luma_mult = (uint8_t)g->chroma_luma_mult[0];
   value.cb_offset = (uint16_t)g->chroma_offset[0];
   value.cr_mult = (uint8_t)g->chroma_mult[1];
   value.cr_luma_mult = (uint8_t)g->chroma_luma_mult[1];
   value.cr_offset = (uint16_t)g->chroma_offset[1];
   *out = value;
   return true;
}

/* Effective parsed state, native physical slots, separate grain destination.
 * Normal tiles only. Failure preserves out; no VA call or allocation. */
static INLINE bool vaapi_av1_picture(VADecPictureParameterBufferAV1 *out,
      const rav1_sequence_t *s, const rav1_hw_picture_t *p,
      const VASurfaceID *surfaces, unsigned slots, VASurfaceID display)
{
   static const int feature_max[8] = {255, 63, 63, 63, 63, 7, 0, 0};
   VADecPictureParameterBufferAV1 value;
   const rav1_picture_t *q;
   unsigned i, j, cols, rows, sb, a, b, shift, uv_shift = 0;
   bool restoration = false, chroma_restoration = false;
   if (!out || !s || !p || !surfaces || slots < 9 || slots > 32 || s->profile
         || (s->bit_depth != 8 && s->bit_depth != 10) || s->monochrome
         || s->subsampling_x != 1 || s->subsampling_y != 1
         || s->operating_point_count != 1 || s->operating_points[0].idc
         || !s->max_width || s->max_width > 65536 || !s->max_height
         || s->max_height > 65536 || s->matrix_coefficients > 255
         || p->header.show_existing || p->header.frame_type > RAV1_FRAME_SWITCH
         || p->header.primary_ref_frame > 7
         || p->header.frame_is_intra
               != (p->header.frame_type == RAV1_FRAME_KEY
                     || p->header.frame_type == RAV1_FRAME_INTRA_ONLY)
         || p->slot < 0 || (unsigned)p->slot >= slots
         || surfaces[p->slot] == VA_INVALID_SURFACE || !p->setup.size.width
         || !p->setup.size.height
         || p->setup.size.width > p->setup.size.upscaled_width
         || p->setup.size.upscaled_width > s->max_width
         || p->setup.size.height > s->max_height
         || p->setup.size.superres_denom < 8
         || p->setup.size.superres_denom > 16
         || p->setup.interpolation_filter > 4
         || vaapi_av1_tiles_overlap(out, sizeof(*out), p, sizeof(*p))
         || vaapi_av1_tiles_overlap(out, sizeof(*out), s, sizeof(*s))
         || vaapi_av1_tiles_overlap(
               out, sizeof(*out), surfaces, slots * sizeof(*surfaces)))
      return false;
   if (s->enable_order_hint
               ? (!s->order_hint_bits || s->order_hint_bits > 8
                       || p->header.order_hint >= (1u << s->order_hint_bits))
               : (s->order_hint_bits || p->header.order_hint))
      return false;
   cols = p->tiles.layout.columns;
   rows = p->tiles.layout.rows;
   if (!cols || cols > 64 || !rows || rows > 64
         || p->tiles.context_update_tile_id >= cols * rows)
      return false;
   q = &p->picture;
   if (q->base_q_idx > 255 || q->tx_mode > 2 || q->delta_q_res > 3
         || q->delta_lf_res > 3 || q->qm_y > 15 || q->qm_u > 15 || q->qm_v > 15
         || q->loop_filter_sharpness > 7 || q->cdef_bits > 3
         || q->cdef_damping > 6 || (q->cdef_damping && q->cdef_damping < 3)
         || q->delta_q_y_dc < -64 || q->delta_q_y_dc > 63
         || q->delta_q_u_dc < -64 || q->delta_q_u_dc > 63
         || q->delta_q_u_ac < -64 || q->delta_q_u_ac > 63
         || q->delta_q_v_dc < -64 || q->delta_q_v_dc > 63
         || q->delta_q_v_ac < -64 || q->delta_q_v_ac > 63)
      return false;
   memset(&value, 0, sizeof(value));
   value.profile = 0;
   value.bit_depth_idx = s->bit_depth == 10;
   value.matrix_coefficients = (uint8_t)s->matrix_coefficients;
   value.order_hint_bits_minus_1
         = s->enable_order_hint ? (uint8_t)(s->order_hint_bits - 1) : 0;
   value.current_frame = surfaces[p->slot];
   value.current_display_picture = value.current_frame;
   value.frame_width_minus1 = (uint16_t)(p->setup.size.upscaled_width - 1);
   value.frame_height_minus1 = (uint16_t)(p->setup.size.height - 1);
   value.primary_ref_frame = (uint8_t)p->header.primary_ref_frame;
   value.order_hint = (uint8_t)p->header.order_hint;
   for (i = 0; i < 8; i++)
   {
      int slot = p->reference_slots[i];
      if (slot < -1 || slot == p->slot
            || (slot >= 0
                  && ((unsigned)slot >= slots
                        || surfaces[slot] == VA_INVALID_SURFACE
                        || surfaces[slot] == value.current_frame)))
         return false;
      value.ref_frame_map[i] = slot < 0 ? VA_INVALID_SURFACE : surfaces[slot];
   }
   for (i = 0; i < 7; i++)
      if (!p->header.frame_is_intra)
      {
         j = p->setup.ref_frame_idx[i];
         if (j >= 8 || p->reference_slots[j] < 0 || !p->reference_info[j].valid)
            return false;
         value.ref_frame_idx[i] = (uint8_t)j;
      }
   if (p->grain.params.apply)
   {
      if (!s->film_grain_params_present || display == VA_INVALID_SURFACE
            || display == value.current_frame)
         return false;
      for (i = 0; i < 8; i++)
         if (display == value.ref_frame_map[i])
            return false;
      value.current_display_picture = display;
   }
   if (!vaapi_av1_grain(&value.film_grain_info, &p->grain.params))
      return false;
#define VAAPI_AV1_FLAG(field, source)                                          \
   do                                                                          \
   {                                                                           \
      if ((source) > 1)                                                        \
         return false;                                                         \
      value.field = (source);                                                  \
   } while (0)
   VAAPI_AV1_FLAG(seq_info_fields.fields.still_picture, s->still_picture);
   VAAPI_AV1_FLAG(seq_info_fields.fields.use_128x128_superblock,
         s->use_128x128_superblock);
   VAAPI_AV1_FLAG(
         seq_info_fields.fields.enable_filter_intra, s->enable_filter_intra);
   VAAPI_AV1_FLAG(seq_info_fields.fields.enable_intra_edge_filter,
         s->enable_intra_edge_filter);
   VAAPI_AV1_FLAG(seq_info_fields.fields.enable_interintra_compound,
         s->enable_interintra_compound);
   VAAPI_AV1_FLAG(seq_info_fields.fields.enable_masked_compound,
         s->enable_masked_compound);
   VAAPI_AV1_FLAG(
         seq_info_fields.fields.enable_dual_filter, s->enable_dual_filter);
   VAAPI_AV1_FLAG(
         seq_info_fields.fields.enable_order_hint, s->enable_order_hint);
   VAAPI_AV1_FLAG(seq_info_fields.fields.enable_jnt_comp, s->enable_jnt_comp);
   VAAPI_AV1_FLAG(seq_info_fields.fields.enable_cdef, s->enable_cdef);
   VAAPI_AV1_FLAG(seq_info_fields.fields.color_range, s->color_range);
   VAAPI_AV1_FLAG(seq_info_fields.fields.subsampling_x, s->subsampling_x);
   VAAPI_AV1_FLAG(seq_info_fields.fields.subsampling_y, s->subsampling_y);
   VAAPI_AV1_FLAG(seq_info_fields.fields.film_grain_params_present,
         s->film_grain_params_present);
   VAAPI_AV1_FLAG(pic_info_fields.bits.show_frame, p->header.show_frame);
   VAAPI_AV1_FLAG(
         pic_info_fields.bits.showable_frame, p->header.showable_frame);
   VAAPI_AV1_FLAG(
         pic_info_fields.bits.error_resilient_mode, p->header.error_resilient);
   VAAPI_AV1_FLAG(
         pic_info_fields.bits.disable_cdf_update, p->header.disable_cdf_update);
   VAAPI_AV1_FLAG(pic_info_fields.bits.allow_screen_content_tools,
         p->header.allow_screen_content_tools);
   VAAPI_AV1_FLAG(
         pic_info_fields.bits.force_integer_mv, p->header.force_integer_mv);
   VAAPI_AV1_FLAG(pic_info_fields.bits.allow_intrabc, p->setup.allow_intrabc);
   VAAPI_AV1_FLAG(pic_info_fields.bits.allow_high_precision_mv,
         p->setup.allow_high_precision_mv);
   VAAPI_AV1_FLAG(pic_info_fields.bits.is_motion_mode_switchable,
         p->setup.motion_mode_switchable);
   VAAPI_AV1_FLAG(
         pic_info_fields.bits.use_ref_frame_mvs, p->setup.use_ref_frame_mvs);
   VAAPI_AV1_FLAG(pic_info_fields.bits.disable_frame_end_update_cdf,
         p->setup.disable_frame_end_update_cdf);
   VAAPI_AV1_FLAG(pic_info_fields.bits.uniform_tile_spacing_flag,
         p->tiles.uniform_spacing);
   VAAPI_AV1_FLAG(pic_info_fields.bits.allow_warped_motion,
         p->motion.allow_warped_motion);
   VAAPI_AV1_FLAG(
         seg_info.segment_info_fields.bits.enabled, q->segmentation_enabled);
   VAAPI_AV1_FLAG(seg_info.segment_info_fields.bits.update_map,
         q->segmentation_update_map);
   VAAPI_AV1_FLAG(seg_info.segment_info_fields.bits.temporal_update,
         q->segmentation_temporal_update);
   VAAPI_AV1_FLAG(seg_info.segment_info_fields.bits.update_data,
         q->segmentation_update_data);
   VAAPI_AV1_FLAG(loop_filter_info_fields.bits.mode_ref_delta_enabled,
         q->loop_filter_delta_enabled);
   VAAPI_AV1_FLAG(loop_filter_info_fields.bits.mode_ref_delta_update,
         q->loop_filter_delta_update);
   VAAPI_AV1_FLAG(qmatrix_fields.bits.using_qmatrix, q->using_qmatrix);
   VAAPI_AV1_FLAG(
         mode_control_fields.bits.delta_q_present_flag, q->delta_q_present);
   VAAPI_AV1_FLAG(
         mode_control_fields.bits.delta_lf_present_flag, q->delta_lf_present);
   VAAPI_AV1_FLAG(mode_control_fields.bits.delta_lf_multi, q->delta_lf_multi);
   VAAPI_AV1_FLAG(
         mode_control_fields.bits.reference_select, p->motion.reference_select);
   VAAPI_AV1_FLAG(mode_control_fields.bits.reduced_tx_set_used,
         p->motion.reduced_tx_set);
   VAAPI_AV1_FLAG(mode_control_fields.bits.skip_mode_present,
         p->motion.skip_mode_present);
#undef VAAPI_AV1_FLAG
   value.pic_info_fields.bits.frame_type = p->header.frame_type;
   value.pic_info_fields.bits.use_superres = p->setup.size.superres_denom != 8;
   value.superres_scale_denominator = (uint8_t)p->setup.size.superres_denom;
   value.interp_filter = (uint8_t)p->setup.interpolation_filter;
   value.tile_cols = (uint8_t)cols;
   value.tile_rows = (uint8_t)rows;
   value.context_update_tile_id = (uint16_t)p->tiles.context_update_tile_id;
   sb = s->use_128x128_superblock ? 32 : 16;
   if (p->tiles.mi_col_starts[0] || p->tiles.mi_row_starts[0]
         || p->tiles.mi_col_starts[cols] != (p->setup.size.width + 7) / 8 * 2
         || p->tiles.mi_row_starts[rows] != (p->setup.size.height + 7) / 8 * 2)
      return false;
   for (i = 0; i < cols + rows; i++)
   {
      j = i < cols ? i : i - cols;
      a = i < cols ? p->tiles.mi_col_starts[j] : p->tiles.mi_row_starts[j];
      b = i < cols ? p->tiles.mi_col_starts[j + 1]
                   : p->tiles.mi_row_starts[j + 1];
      if (b <= a || a % sb || b > 16384)
         return false;
      if (i < cols && j + 1 < cols)
         value.width_in_sbs_minus_1[j]
               = (uint16_t)((b + sb - 1) / sb - a / sb - 1);
      else if (i >= cols && j + 1 < rows)
         value.height_in_sbs_minus_1[j]
               = (uint16_t)((b + sb - 1) / sb - a / sb - 1);
   }
   value.base_qindex = (uint8_t)q->base_q_idx;
   value.y_dc_delta_q = (int8_t)q->delta_q_y_dc;
   value.u_dc_delta_q = (int8_t)q->delta_q_u_dc;
   value.u_ac_delta_q = (int8_t)q->delta_q_u_ac;
   value.v_dc_delta_q = (int8_t)q->delta_q_v_dc;
   value.v_ac_delta_q = (int8_t)q->delta_q_v_ac;
   if (q->using_qmatrix)
   {
      value.qmatrix_fields.bits.qm_y = q->qm_y;
      value.qmatrix_fields.bits.qm_u = q->qm_u;
      value.qmatrix_fields.bits.qm_v = q->qm_v;
   }
   value.mode_control_fields.bits.log2_delta_q_res = q->delta_q_res;
   value.mode_control_fields.bits.log2_delta_lf_res = q->delta_lf_res;
   value.mode_control_fields.bits.tx_mode = q->tx_mode;
   value.loop_filter_info_fields.bits.sharpness_level
         = q->loop_filter_sharpness;
   for (i = 0; i < 4; i++)
      if (q->loop_filter_level[i] > 63)
         return false;
   value.filter_level[0] = (uint8_t)q->loop_filter_level[0];
   value.filter_level[1] = (uint8_t)q->loop_filter_level[1];
   value.filter_level_u = (uint8_t)q->loop_filter_level[2];
   value.filter_level_v = (uint8_t)q->loop_filter_level[3];
   value.cdef_damping_minus_3
         = q->cdef_damping ? (uint8_t)(q->cdef_damping - 3) : 0;
   value.cdef_bits = (uint8_t)q->cdef_bits;
   for (i = 0; i < 8; i++)
   {
      if (q->reference.loop_filter_ref_deltas[i] < -64
            || q->reference.loop_filter_ref_deltas[i] > 63
            || q->reference.segment_enabled[i] > 255 || q->cdef_y_pri[i] > 15
            || q->cdef_uv_pri[i] > 15
            || (q->cdef_y_sec[i] > 2 && q->cdef_y_sec[i] != 4)
            || (q->cdef_uv_sec[i] > 2 && q->cdef_uv_sec[i] != 4))
         return false;
      value.ref_deltas[i] = (int8_t)q->reference.loop_filter_ref_deltas[i];
      value.seg_info.feature_mask[i] = (uint8_t)q->reference.segment_enabled[i];
      for (j = 0; j < 8; j++)
         if (q->reference.segment_enabled[i] & (1u << j))
         {
            int v = q->reference.segment_data[i][j];
            if (v > feature_max[j] || v < (j < 5 ? -feature_max[j] : 0))
               return false;
            value.seg_info.feature_data[i][j] = (int16_t)v;
         }
      value.cdef_y_strengths[i] = (uint8_t)((q->cdef_y_pri[i] << 2)
            | (q->cdef_y_sec[i] == 4 ? 3 : q->cdef_y_sec[i]));
      value.cdef_uv_strengths[i] = (uint8_t)((q->cdef_uv_pri[i] << 2)
            | (q->cdef_uv_sec[i] == 4 ? 3 : q->cdef_uv_sec[i]));
   }
   for (i = 0; i < 2; i++)
   {
      if (q->reference.loop_filter_mode_deltas[i] < -64
            || q->reference.loop_filter_mode_deltas[i] > 63)
         return false;
      value.mode_deltas[i] = (int8_t)q->reference.loop_filter_mode_deltas[i];
   }
   for (i = 0; i < 3; i++)
   {
      if (q->restoration_type[i] > RAV1_RESTORE_SWITCHABLE)
         return false;
      restoration |= q->restoration_type[i] != RAV1_RESTORE_NONE;
      if (i)
         chroma_restoration |= q->restoration_type[i] != RAV1_RESTORE_NONE;
   }
   value.loop_restoration_fields.bits.yframe_restoration_type
         = q->restoration_type[0];
   value.loop_restoration_fields.bits.cbframe_restoration_type
         = q->restoration_type[1];
   value.loop_restoration_fields.bits.crframe_restoration_type
         = q->restoration_type[2];
   if (restoration)
   {
      a = q->restoration_size[0];
      if (a < (s->use_128x128_superblock ? 128u : 64u) || a > 256
            || (a & (a - 1)))
         return false;
      for (shift = 0; a > 64; a >>= 1)
         shift++;
      if (chroma_restoration)
      {
         b = q->restoration_size[1];
         if (b < 32 || b > 256 || b != q->restoration_size[2]
               || (b != q->restoration_size[0]
                     && b * 2 != q->restoration_size[0]))
            return false;
         uv_shift = b != q->restoration_size[0];
      }
      value.loop_restoration_fields.bits.lr_unit_shift = shift;
      value.loop_restoration_fields.bits.lr_uv_shift = uv_shift;
   }
   for (i = 0; i < 7; i++)
   {
      unsigned type = p->motion.global.type[i];
      if (type > RAV1_GLOBAL_AFFINE)
         return false;
      value.wm[i].wmtype = (VAAV1TransformationType)type;
      if (type == RAV1_GLOBAL_IDENTITY)
      {
         value.wm[i].wmmat[2] = value.wm[i].wmmat[5] = 65536;
         continue;
      }
      for (j = 0; j < 6; j++)
      {
         int32_t v = p->motion.global.params[i][j];
         int center = j == 2 || j == 5 ? 65536 : 0;
         int limit = j < 2 ? 4194304 : 8192;
         if (v < center - limit || v > center + limit)
            return false;
         value.wm[i].wmmat[j] = v;
      }
      if ((type == RAV1_GLOBAL_TRANSLATION
                && (value.wm[i].wmmat[2] != 65536
                      || value.wm[i].wmmat[5] != 65536 || value.wm[i].wmmat[3]
                      || value.wm[i].wmmat[4]))
            || (type == RAV1_GLOBAL_ROTZOOM
                  && (value.wm[i].wmmat[4] != -value.wm[i].wmmat[3]
                        || value.wm[i].wmmat[5] != value.wm[i].wmmat[2])))
         return false;
      value.wm[i].invalid
            = (uint8_t)vaapi_av1_warp_invalid(p->motion.global.params[i]);
   }
   *out = value;
   return true;
}
#endif
