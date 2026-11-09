/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_H
#define RARCH_VAAPI_AV1_H
#include <va/va.h>
#include <formats/rav1_hw.h>
#include <limits.h>
#include <string.h>

#if VA_CHECK_VERSION(1, 8, 0)
#include <va/va_dec_av1.h>
#define RARCH_VAAPI_AV1_PARAMS_AVAILABLE 1

typedef struct vaapi_av1_params
{
   VADecPictureParameterBufferAV1 picture;
   VASurfaceID target;
   unsigned tile_count;
} vaapi_av1_params_t;

static INLINE bool vaapi_av1_overlap(const void *a, size_t an, const void *b,
                                     size_t bn)
{
   uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
   return an && bn && (x <= y ? y - x < an : x - y < bn);
}

static INLINE bool vaapi_av1_sequence_valid(const rav1_sequence_t *s)
{
   if (!s || s->profile || (s->bit_depth != 8 && s->bit_depth != 10) ||
       s->monochrome || s->subsampling_x != 1 || s->subsampling_y != 1 ||
       !s->max_width || s->max_width > 65536 || !s->max_height ||
       s->max_height > 65536 || !s->frame_width_bits ||
       s->frame_width_bits > 16 || !s->frame_height_bits ||
       s->frame_height_bits > 16 ||
       s->max_width > (1u << s->frame_width_bits) ||
       s->max_height > (1u << s->frame_height_bits) ||
       s->operating_point_count != 1 || s->operating_points[0].idc ||
       s->operating_points[0].level > 23 || s->matrix_coefficients > 255 ||
       s->force_integer_mv > 2 || s->force_screen_content_tools > 2 ||
       s->chroma_sample_position > 2 ||
       (s->enable_order_hint &&
        (!s->order_hint_bits || s->order_hint_bits > 8)) ||
       (s->frame_id_numbers_present &&
        (s->delta_frame_id_length < 2 || s->delta_frame_id_length > 17 ||
         s->frame_id_length > 16 ||
         s->frame_id_length <= s->delta_frame_id_length)))
      return false;
   return (s->still_picture | s->reduced_still_picture_header |
           s->use_128x128_superblock | s->enable_filter_intra |
           s->enable_intra_edge_filter | s->enable_interintra_compound |
           s->enable_masked_compound | s->enable_dual_filter |
           s->enable_order_hint | s->enable_jnt_comp | s->enable_cdef |
           s->color_range | s->film_grain_params_present |
           s->enable_restoration | s->enable_superres |
           s->enable_warped_motion | s->enable_ref_frame_mvs) <= 1;
}

static INLINE bool vaapi_av1_delta(int value)
{
   return value >= -64 && value <= 63;
}

static INLINE bool vaapi_av1_global_valid(unsigned type, const int32_t *m)
{
   if (type > RAV1_GLOBAL_AFFINE || m[0] < -4194304 || m[0] > 4194304
         || m[1] < -4194304 || m[1] > 4194304)
      return false;
   if (type <= RAV1_GLOBAL_TRANSLATION)
      return m[2] == 65536 && !m[3] && !m[4] && m[5] == 65536
            && (type || (!m[0] && !m[1]));
   if (m[2] < 57344 || m[2] > 73728 || m[5] < 57344 || m[5] > 73728
         || m[3] < -8192 || m[3] > 8192 || m[4] < -8192 || m[4] > 8192
         || m[0] % 1024 || m[1] % 1024 || m[2] % 2 || m[3] % 2
         || m[4] % 2 || m[5] % 2)
      return false;
   return type != RAV1_GLOBAL_ROTZOOM || (m[4] == -m[3] && m[5] == m[2]);
}

static INLINE int32_t vaapi_av1_round_signed(int64_t value, unsigned shift)
{
   int64_t half = (int64_t)1 << (shift - 1);
   return value < 0 ? -(int32_t)((-value + half) >> shift)
                    : (int32_t)((value + half) >> shift);
}

/* AV1 7.11.3.6/7: validated linear coefficients bound every intermediate.
 * Rounded reciprocal division reproduces all 257 normative Div_Lut entries. */
static INLINE unsigned vaapi_av1_global_invalid(const int32_t *m)
{
   unsigned shift = m[2] >= 65536 ? 16 : 15;
   unsigned fraction = ((unsigned)m[2] - (1u << shift)
         + (1u << (shift - 9))) >> (shift - 8);
   unsigned denominator = 256 + fraction;
   unsigned reciprocal = (4194304u + denominator / 2) / denominator;
   int32_t a, b, c, d;
   a = vaapi_av1_round_signed(m[2] - 65536, 6) * 64;
   b = vaapi_av1_round_signed(m[3], 6) * 64;
   c = vaapi_av1_round_signed(
         (int64_t)m[4] * 65536 * reciprocal, shift + 14);
   d = m[5] - 65536 - vaapi_av1_round_signed(
         (int64_t)m[3] * m[4] * reciprocal, shift + 14);
   c = vaapi_av1_round_signed(c, 6) * 64;
   d = vaapi_av1_round_signed(d, 6) * 64;
   if (a < 0) a = -a;
   if (b < 0) b = -b;
   if (c < 0) c = -c;
   if (d < 0) d = -d;
   return 4 * a + 7 * b >= 65536 || 4 * c + 4 * d >= 65536;
}

static INLINE bool vaapi_av1_grain_valid(const rav1_grain_t *g)
{
   unsigned plane, i;
   if (g->apply > 1)
      return false;
   if (!g->apply)
      return true;
   if (g->seed > 65535 || g->scaling_shift < 8 || g->scaling_shift > 11 ||
       g->ar_coeff_lag > 3 || g->ar_coeff_shift < 6 || g->ar_coeff_shift > 9 ||
       g->scale_shift > 3 ||
       (g->chroma_scaling_from_luma | g->overlap |
        g->clip_to_restricted_range) > 1 ||
       (!g->num_points[1] != !g->num_points[2]) ||
       ((g->chroma_scaling_from_luma || !g->num_points[0]) &&
        (g->num_points[1] || g->num_points[2])))
      return false;
   for (plane = 0; plane < 3; plane++)
   {
      if (g->num_points[plane] > (plane ? 10 : 14))
         return false;
      for (i = 1; i < g->num_points[plane]; i++)
         if (g->point_value[plane][i] <= g->point_value[plane][i - 1])
            return false;
   }
   for (plane = 0; plane < 2; plane++)
      if (g->chroma_mult[plane] > 255 || g->chroma_luma_mult[plane] > 255 ||
          g->chroma_offset[plane] > 511)
         return false;
   return true;
}

/* Writes directly into the caller's zeroed parameter buffer. */
static INLINE void vaapi_av1_grain(VAFilmGrainStructAV1 *v,
                                   const rav1_grain_t *g)
{
   unsigned i, count = 2 * g->ar_coeff_lag * (g->ar_coeff_lag + 1);
   v->film_grain_info_fields.bits.apply_grain = 1;
   v->film_grain_info_fields.bits.chroma_scaling_from_luma =
      g->chroma_scaling_from_luma;
   v->film_grain_info_fields.bits.grain_scaling_minus_8 = g->scaling_shift - 8;
   v->film_grain_info_fields.bits.ar_coeff_lag = g->ar_coeff_lag;
   v->film_grain_info_fields.bits.ar_coeff_shift_minus_6 = g->ar_coeff_shift - 6;
   v->film_grain_info_fields.bits.grain_scale_shift = g->scale_shift;
   v->film_grain_info_fields.bits.overlap_flag = g->overlap;
   v->film_grain_info_fields.bits.clip_to_restricted_range =
      g->clip_to_restricted_range;
   v->grain_seed = (uint16_t)g->seed;
   v->num_y_points = g->num_points[0];
   v->num_cb_points = g->num_points[1];
   v->num_cr_points = g->num_points[2];
   for (i = 0; i < g->num_points[0]; i++)
   {
      v->point_y_value[i] = g->point_value[0][i];
      v->point_y_scaling[i] = g->point_scaling[0][i];
   }
   for (i = 0; i < g->num_points[1]; i++)
   {
      v->point_cb_value[i] = g->point_value[1][i];
      v->point_cb_scaling[i] = g->point_scaling[1][i];
   }
   for (i = 0; i < g->num_points[2]; i++)
   {
      v->point_cr_value[i] = g->point_value[2][i];
      v->point_cr_scaling[i] = g->point_scaling[2][i];
   }
   for (i = 0; i < count; i++)
      if (g->num_points[0])
         v->ar_coeffs_y[i] = g->ar_coeffs[0][i];
   count += g->num_points[0] != 0;
   for (i = 0; i < count; i++)
   {
      if (g->num_points[1] || g->chroma_scaling_from_luma)
         v->ar_coeffs_cb[i] = g->ar_coeffs[1][i];
      if (g->num_points[2] || g->chroma_scaling_from_luma)
         v->ar_coeffs_cr[i] = g->ar_coeffs[2][i];
   }
   if (g->num_points[1])
   {
      v->cb_mult = (uint8_t)g->chroma_mult[0];
      v->cb_luma_mult = (uint8_t)g->chroma_luma_mult[0];
      v->cb_offset = (uint16_t)g->chroma_offset[0];
   }
   if (g->num_points[2])
   {
      v->cr_mult = (uint8_t)g->chroma_mult[1];
      v->cr_luma_mult = (uint8_t)g->chroma_luma_mult[1];
      v->cr_offset = (uint16_t)g->chroma_offset[1];
   }
}

static INLINE bool vaapi_av1_tools_valid(const rav1_hw_picture_t *p)
{
   static const int maxima[8] = {255, 63, 63, 63, 63, 7, 0, 0};
   const rav1_picture_t *q = &p->picture;
   unsigned i, j, lr = 0;
   if (q->base_q_idx > 255 || q->qm_y > 15 || q->qm_u > 15 || q->qm_v > 15 ||
       !vaapi_av1_delta(q->delta_q_y_dc) || !vaapi_av1_delta(q->delta_q_u_dc) ||
       !vaapi_av1_delta(q->delta_q_u_ac) || !vaapi_av1_delta(q->delta_q_v_dc) ||
       !vaapi_av1_delta(q->delta_q_v_ac) || q->tx_mode > 2 ||
       q->delta_q_res > 3 || q->delta_lf_res > 3 ||
       q->loop_filter_sharpness > 7 || q->cdef_bits > 3 ||
       q->cdef_damping < 3 || q->cdef_damping > 6 ||
       (q->using_qmatrix | q->segmentation_enabled |
        q->segmentation_update_map | q->segmentation_temporal_update |
        q->segmentation_update_data | q->delta_q_present | q->delta_lf_present |
        q->delta_lf_multi | q->loop_filter_delta_enabled |
        q->loop_filter_delta_update | p->motion.reference_select |
        p->motion.reduced_tx_set | p->motion.skip_mode_present |
        p->motion.allow_warped_motion) > 1)
      return false;
   for (i = 0; i < 8; i++)
   {
      if (!vaapi_av1_delta(q->reference.loop_filter_ref_deltas[i]) ||
          q->reference.segment_enabled[i] > 255 || q->cdef_y_pri[i] > 15 ||
          q->cdef_uv_pri[i] > 15 || q->cdef_y_sec[i] > 4 ||
          q->cdef_y_sec[i] == 3 || q->cdef_uv_sec[i] > 4 ||
          q->cdef_uv_sec[i] == 3)
         return false;
      for (j = 0; j < 8; j++)
         if (q->reference.segment_data[i][j] > maxima[j] ||
             q->reference.segment_data[i][j] < (j < 5 ? -maxima[j] : 0))
            return false;
   }
   for (i = 0; i < 2; i++)
      if (!vaapi_av1_delta(q->reference.loop_filter_mode_deltas[i]))
         return false;
   for (i = 0; i < 4; i++)
      if (q->loop_filter_level[i] > 63)
         return false;
   for (i = 0; i < 3; i++)
   {
      if (q->restoration_type[i] > RAV1_RESTORE_SWITCHABLE)
         return false;
      lr |= q->restoration_type[i];
   }
   if (lr && ((q->restoration_size[0] != 64 && q->restoration_size[0] != 128 &&
               q->restoration_size[0] != 256) ||
              q->restoration_size[1] != q->restoration_size[2] ||
              (q->restoration_size[1] != q->restoration_size[0] &&
               q->restoration_size[1] != q->restoration_size[0] / 2)))
      return false;
   for (i = 0; i < 7; i++)
   {
      if ((p->header.frame_is_intra && p->motion.global.type[i])
            || !vaapi_av1_global_valid(p->motion.global.type[i],
                p->motion.global.params[i]))
         return false;
   }
   return true;
}

/* Main 8/10-bit FRAME OBUs; grain requires a separate display surface.
 * Caller storage must not alias borrowed views. No VA calls or leases;
 * admission failure preserves both output buffers. */
static INLINE bool
vaapi_av1_picture_display(vaapi_av1_params_t *out, VASliceParameterBufferAV1 *slices,
                  unsigned capacity, const uint8_t *packet, size_t size,
                  const rav1_sequence_t *s, const rav1_hw_picture_t *p,
                  const VASurfaceID *surfaces, unsigned slots,
                  VASurfaceID display_surface)
{
   VADecPictureParameterBufferAV1 *v;
   const rav1_picture_t *q;
   rav1_obu_t obu;
   rav1_tile_group_t group;
   rav1_tile_t tile;
   size_t offset = 0, base, output_bytes;
   uintptr_t address;
   unsigned count, i, j, sb, a, z, lr = 0;
   if (!out || !slices || !packet || !size || size > UINT_MAX || !p ||
       !surfaces || slots < 9 || slots > 32 || !vaapi_av1_sequence_valid(s) ||
       p->slot < 0 || (unsigned)p->slot >= slots || p->header.show_existing ||
       p->header.frame_type > RAV1_FRAME_SWITCH ||
       p->header.frame_is_intra !=
          (p->header.frame_type == RAV1_FRAME_KEY ||
           p->header.frame_type == RAV1_FRAME_INTRA_ONLY) ||
       p->header.primary_ref_frame > 7 || p->header.order_hint > 255 ||
       p->header.refresh_frame_flags > 255 || p->setup.valid_mask > 255 ||
       (p->header.frame_type == RAV1_FRAME_SWITCH &&
        !p->header.error_resilient) ||
       (p->header.frame_type == RAV1_FRAME_KEY && p->header.show_frame &&
        p->header.refresh_frame_flags != 255) ||
       ((p->header.frame_is_intra || p->header.error_resilient) &&
        p->header.primary_ref_frame != 7) ||
       !p->setup.size.width || !p->setup.size.height ||
       p->setup.size.upscaled_width < p->setup.size.width ||
       p->setup.size.upscaled_width > s->max_width ||
       p->setup.size.height > s->max_height ||
       p->setup.size.superres_denom < 8 || p->setup.size.superres_denom > 16 ||
       (p->setup.size.superres_denom != 8 && !s->enable_superres) ||
       p->setup.size.render_width != p->setup.size.upscaled_width ||
       p->setup.size.render_height != p->setup.size.height ||
       !p->tiles.layout.columns || p->tiles.layout.columns > 64 ||
       !p->tiles.layout.rows || p->tiles.layout.rows > 64 ||
       p->tiles.layout.size_bytes > 4 || p->setup.interpolation_filter > 4 ||
       (p->header.show_frame | p->header.showable_frame |
        p->header.error_resilient | p->header.disable_cdf_update |
        p->header.allow_screen_content_tools | p->header.force_integer_mv |
        p->setup.allow_intrabc | p->setup.allow_high_precision_mv |
        p->setup.motion_mode_switchable | p->setup.use_ref_frame_mvs |
        p->setup.disable_frame_end_update_cdf | p->tiles.uniform_spacing) > 1 ||
       !vaapi_av1_tools_valid(p) || !vaapi_av1_grain_valid(&p->grain.params) ||
       (p->grain.params.apply && (!s->film_grain_params_present ||
        !(p->header.show_frame || p->header.showable_frame))) ||
       (p->grain.params.apply != (unsigned)(display_surface != VA_INVALID_SURFACE)) ||
       rav1_obu_next(&obu, packet, size, &offset, true) != 1 ||
       offset != size || obu.type != 6 || obu.temporal_id || obu.spatial_id)
      return false;
   if (p->setup.size.width != (p->setup.size.upscaled_width * 8 +
                               p->setup.size.superres_denom / 2) /
                                 p->setup.size.superres_denom ||
       p->setup.size.mi_cols != (p->setup.size.width + 7) / 8 * 2 ||
       p->setup.size.mi_rows != (p->setup.size.height + 7) / 8 * 2)
      return false;
   count = p->tiles.layout.columns * p->tiles.layout.rows;
   if (capacity < count || capacity > 4096 ||
       p->tiles.context_update_tile_id >= count)
      return false;
   output_bytes = count * sizeof(*slices);
   if (vaapi_av1_overlap(out, sizeof(*out), slices, output_bytes) ||
       vaapi_av1_overlap(out, sizeof(*out), packet, size) ||
       vaapi_av1_overlap(out, sizeof(*out), s, sizeof(*s)) ||
       vaapi_av1_overlap(out, sizeof(*out), p, sizeof(*p)) ||
       vaapi_av1_overlap(out, sizeof(*out), surfaces,
                         slots * sizeof(*surfaces)) ||
       vaapi_av1_overlap(slices, output_bytes, packet, size) ||
       vaapi_av1_overlap(slices, output_bytes, s, sizeof(*s)) ||
       vaapi_av1_overlap(slices, output_bytes, p, sizeof(*p)) ||
       vaapi_av1_overlap(slices, output_bytes, surfaces,
                         slots * sizeof(*surfaces)))
      return false;
   for (i = 0; i < slots; i++)
      if (surfaces[i] != VA_INVALID_SURFACE)
      {
         if (surfaces[i] == display_surface)
            return false;
         for (j = 0; j < i; j++)
            if (surfaces[i] == surfaces[j])
               return false;
      }
   if (surfaces[p->slot] == VA_INVALID_SURFACE)
      return false;
   for (i = 0; i < 8; i++)
      if (p->reference_slots[i] < -1 || p->reference_slots[i] == p->slot ||
          p->reference_info[i].valid !=
             (unsigned)(p->reference_slots[i] >= 0) ||
          (p->reference_slots[i] >= 0 &&
           ((unsigned)p->reference_slots[i] >= slots ||
            surfaces[p->reference_slots[i]] == VA_INVALID_SURFACE)))
         return false;
   if (!p->header.frame_is_intra)
      for (i = 0; i < 7; i++)
         if (p->setup.ref_frame_idx[i] > 7 ||
             !(p->setup.valid_mask & (1u << p->setup.ref_frame_idx[i])) ||
             p->reference_slots[p->setup.ref_frame_idx[i]] < 0)
            return false;
   address = (uintptr_t)p->tile_group.data;
   if (address < (uintptr_t)obu.payload || address - (uintptr_t)packet > size)
      return false;
   base = (size_t)(address - (uintptr_t)packet);
   if (p->tile_group.size != size - base || p->tile_group.first ||
       p->tile_group.next || p->tile_group.last != count - 1 ||
       p->tile_group.offset > p->tile_group.size ||
       p->tile_group.columns != p->tiles.layout.columns ||
       p->tile_group.size_bytes != p->tiles.layout.size_bytes)
      return false;
   if (p->tile_group.offset != (count > 1 ? 1u : 0u) ||
       (count > 1 && (!p->tile_group.size || p->tile_group.data[0])))
      return false;
   group = p->tile_group;
   for (i = 0; i < count; i++)
      if (rav1_tile_group_next(&tile, &group) != 1 || tile.index != i ||
          !tile.size || tile.size > UINT_MAX)
         return false;
   if (group.offset != group.size)
      return false;
   sb = s->use_128x128_superblock ? 32 : 16;
   for (j = 0; j < 2; j++)
   {
      unsigned limit = j ? p->tiles.layout.rows : p->tiles.layout.columns;
      const unsigned *starts =
         j ? p->tiles.mi_row_starts : p->tiles.mi_col_starts;
      unsigned extent =
         ((j ? p->setup.size.height : p->setup.size.width) + 7) / 8 * 2;
      if (starts[0] || starts[limit] != extent)
         return false;
      for (i = 0; i < limit; i++)
      {
         a = starts[i];
         z = starts[i + 1];
         if (a >= z || z > extent || a % sb)
            return false;
      }
   }
   memset(out, 0, sizeof(*out));
   v = &out->picture;
   for (i = 0; i < p->tiles.layout.columns && i < 63; i++)
      v->width_in_sbs_minus_1[i] =
         (uint16_t)((p->tiles.mi_col_starts[i + 1] + sb - 1) / sb -
                    p->tiles.mi_col_starts[i] / sb - 1);
   for (i = 0; i < p->tiles.layout.rows && i < 63; i++)
      v->height_in_sbs_minus_1[i] =
         (uint16_t)((p->tiles.mi_row_starts[i + 1] + sb - 1) / sb -
                    p->tiles.mi_row_starts[i] / sb - 1);
   q = &p->picture;
   v->profile = 0;
   v->bit_depth_idx = s->bit_depth == 10 ? 1 : 0;
   v->matrix_coefficients = (uint8_t)s->matrix_coefficients;
   v->order_hint_bits_minus_1 =
      s->enable_order_hint ? (uint8_t)(s->order_hint_bits - 1) : 0;
   v->seq_info_fields.fields.still_picture = s->still_picture;
   v->seq_info_fields.fields.use_128x128_superblock = s->use_128x128_superblock;
   v->seq_info_fields.fields.enable_filter_intra = s->enable_filter_intra;
   v->seq_info_fields.fields.enable_intra_edge_filter =
      s->enable_intra_edge_filter;
   v->seq_info_fields.fields.enable_interintra_compound =
      s->enable_interintra_compound;
   v->seq_info_fields.fields.enable_masked_compound = s->enable_masked_compound;
   v->seq_info_fields.fields.enable_dual_filter = s->enable_dual_filter;
   v->seq_info_fields.fields.enable_order_hint = s->enable_order_hint;
   v->seq_info_fields.fields.enable_jnt_comp = s->enable_jnt_comp;
   v->seq_info_fields.fields.enable_cdef = s->enable_cdef;
   v->seq_info_fields.fields.color_range = s->color_range;
   v->seq_info_fields.fields.subsampling_x =
      v->seq_info_fields.fields.subsampling_y = 1;
   v->seq_info_fields.fields.film_grain_params_present =
      s->film_grain_params_present;
   v->current_frame = out->target = surfaces[p->slot];
   v->current_display_picture = display_surface;
   if (p->grain.params.apply)
      vaapi_av1_grain(&v->film_grain_info, &p->grain.params);
   v->frame_width_minus1 = (uint16_t)(p->setup.size.upscaled_width - 1);
   v->frame_height_minus1 = (uint16_t)(p->setup.size.height - 1);
   v->primary_ref_frame = (uint8_t)p->header.primary_ref_frame;
   v->order_hint = (uint8_t)p->header.order_hint;
   for (i = 0; i < 8; i++)
   {
      v->ref_frame_map[i] = p->reference_slots[i] < 0
                               ? VA_INVALID_SURFACE
                               : surfaces[p->reference_slots[i]];
      v->seg_info.feature_mask[i] = (uint8_t)q->reference.segment_enabled[i];
      v->ref_deltas[i] = (int8_t)q->reference.loop_filter_ref_deltas[i];
      v->cdef_y_strengths[i] =
         (uint8_t)((q->cdef_y_pri[i] << 2) |
                   (q->cdef_y_sec[i] == 4 ? 3 : q->cdef_y_sec[i]));
      v->cdef_uv_strengths[i] =
         (uint8_t)((q->cdef_uv_pri[i] << 2) |
                   (q->cdef_uv_sec[i] == 4 ? 3 : q->cdef_uv_sec[i]));
      for (j = 0; j < 8; j++)
         v->seg_info.feature_data[i][j] =
            (int16_t)q->reference.segment_data[i][j];
   }
   for (i = 0; i < 7; i++)
   {
      v->ref_frame_idx[i] = (uint8_t)p->setup.ref_frame_idx[i];
      v->wm[i].wmtype = (VAAV1TransformationType)p->motion.global.type[i];
      v->wm[i].invalid = p->motion.global.type[i] > RAV1_GLOBAL_TRANSLATION
            ? (uint8_t)vaapi_av1_global_invalid(p->motion.global.params[i]) : 0;
      for (j = 0; j < 6; j++)
         v->wm[i].wmmat[j] = p->motion.global.params[i][j];
   }
   v->seg_info.segment_info_fields.bits.enabled = q->segmentation_enabled;
   v->seg_info.segment_info_fields.bits.update_map = q->segmentation_update_map;
   v->seg_info.segment_info_fields.bits.temporal_update =
      q->segmentation_temporal_update;
   v->seg_info.segment_info_fields.bits.update_data =
      q->segmentation_update_data;
   v->tile_cols = (uint8_t)p->tiles.layout.columns;
   v->tile_rows = (uint8_t)p->tiles.layout.rows;
   v->context_update_tile_id = (uint16_t)p->tiles.context_update_tile_id;
   v->pic_info_fields.bits.frame_type = p->header.frame_type;
   v->pic_info_fields.bits.show_frame = p->header.show_frame;
   v->pic_info_fields.bits.showable_frame = p->header.showable_frame;
   v->pic_info_fields.bits.error_resilient_mode = p->header.error_resilient;
   v->pic_info_fields.bits.disable_cdf_update = p->header.disable_cdf_update;
   v->pic_info_fields.bits.allow_screen_content_tools =
      p->header.allow_screen_content_tools;
   v->pic_info_fields.bits.force_integer_mv = p->header.force_integer_mv;
   v->pic_info_fields.bits.allow_intrabc = p->setup.allow_intrabc;
   v->pic_info_fields.bits.use_superres = p->setup.size.superres_denom != 8;
   v->pic_info_fields.bits.allow_high_precision_mv =
      p->setup.allow_high_precision_mv;
   v->pic_info_fields.bits.is_motion_mode_switchable =
      p->setup.motion_mode_switchable;
   v->pic_info_fields.bits.use_ref_frame_mvs = p->setup.use_ref_frame_mvs;
   v->pic_info_fields.bits.disable_frame_end_update_cdf =
      p->setup.disable_frame_end_update_cdf;
   v->pic_info_fields.bits.uniform_tile_spacing_flag = p->tiles.uniform_spacing;
   v->pic_info_fields.bits.allow_warped_motion = p->motion.allow_warped_motion;
   v->superres_scale_denominator = (uint8_t)p->setup.size.superres_denom;
   v->interp_filter = (uint8_t)p->setup.interpolation_filter;
   v->filter_level[0] = (uint8_t)q->loop_filter_level[0];
   v->filter_level[1] = (uint8_t)q->loop_filter_level[1];
   v->filter_level_u = (uint8_t)q->loop_filter_level[2];
   v->filter_level_v = (uint8_t)q->loop_filter_level[3];
   v->loop_filter_info_fields.bits.sharpness_level = q->loop_filter_sharpness;
   v->loop_filter_info_fields.bits.mode_ref_delta_enabled =
      q->loop_filter_delta_enabled;
   v->loop_filter_info_fields.bits.mode_ref_delta_update =
      q->loop_filter_delta_update;
   for (i = 0; i < 2; i++)
      v->mode_deltas[i] = (int8_t)q->reference.loop_filter_mode_deltas[i];
   v->base_qindex = (uint8_t)q->base_q_idx;
   v->y_dc_delta_q = (int8_t)q->delta_q_y_dc;
   v->u_dc_delta_q = (int8_t)q->delta_q_u_dc;
   v->u_ac_delta_q = (int8_t)q->delta_q_u_ac;
   v->v_dc_delta_q = (int8_t)q->delta_q_v_dc;
   v->v_ac_delta_q = (int8_t)q->delta_q_v_ac;
   v->qmatrix_fields.bits.using_qmatrix = q->using_qmatrix;
   v->qmatrix_fields.bits.qm_y = q->qm_y;
   v->qmatrix_fields.bits.qm_u = q->qm_u;
   v->qmatrix_fields.bits.qm_v = q->qm_v;
   v->mode_control_fields.bits.delta_q_present_flag = q->delta_q_present;
   v->mode_control_fields.bits.log2_delta_q_res = q->delta_q_res;
   v->mode_control_fields.bits.delta_lf_present_flag = q->delta_lf_present;
   v->mode_control_fields.bits.log2_delta_lf_res = q->delta_lf_res;
   v->mode_control_fields.bits.delta_lf_multi = q->delta_lf_multi;
   v->mode_control_fields.bits.tx_mode = q->tx_mode;
   v->mode_control_fields.bits.reference_select = p->motion.reference_select;
   v->mode_control_fields.bits.reduced_tx_set_used = p->motion.reduced_tx_set;
   v->mode_control_fields.bits.skip_mode_present = p->motion.skip_mode_present;
   v->cdef_damping_minus_3 = (uint8_t)(q->cdef_damping - 3);
   v->cdef_bits = (uint8_t)q->cdef_bits;
   for (i = 0; i < 3; i++)
      lr |= q->restoration_type[i];
   if (lr)
   {
      v->loop_restoration_fields.bits.yframe_restoration_type =
         q->restoration_type[0];
      v->loop_restoration_fields.bits.cbframe_restoration_type =
         q->restoration_type[1];
      v->loop_restoration_fields.bits.crframe_restoration_type =
         q->restoration_type[2];
      v->loop_restoration_fields.bits.lr_unit_shift =
         q->restoration_size[0] / 128;
      v->loop_restoration_fields.bits.lr_uv_shift =
         q->restoration_size[1] != q->restoration_size[0];
   }
   out->tile_count = count;
   group = p->tile_group;
   memset(slices, 0, output_bytes);
   for (i = 0; i < count; i++)
   {
      (void)rav1_tile_group_next(&tile, &group);
      slices[i].slice_data_size = (uint32_t)tile.size;
      slices[i].slice_data_offset =
         (uint32_t)(base + (size_t)(tile.data - group.data));
      slices[i].slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
      slices[i].tile_row = (uint16_t)tile.row;
      slices[i].tile_column = (uint16_t)tile.column;
   }
   return true;
}

static INLINE bool
vaapi_av1_picture(vaapi_av1_params_t *out, VASliceParameterBufferAV1 *slices,
                  unsigned capacity, const uint8_t *packet, size_t size,
                  const rav1_sequence_t *s, const rav1_hw_picture_t *p,
                  const VASurfaceID *surfaces, unsigned slots)
{
   return vaapi_av1_picture_display(out, slices, capacity, packet, size, s, p,
                                    surfaces, slots, VA_INVALID_SURFACE);
}
#else
#define RARCH_VAAPI_AV1_PARAMS_AVAILABLE 0
#endif
#endif
