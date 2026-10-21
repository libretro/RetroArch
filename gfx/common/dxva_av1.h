/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_DXVA_AV1_H
#define RARCH_DXVA_AV1_H

#include <formats/rav1_hw.h>

#define DXVA_AV1_TILE_SIZE 16
#define DXVA_AV1_PP_SIZE 912

static INLINE void dxva_av1_put16(uint8_t *out, unsigned value)
{
   out[0] = (uint8_t)value;
   out[1] = (uint8_t)(value >> 8);
}

static INLINE void dxva_av1_put32(uint8_t *out, uint32_t value)
{
   out[0] = (uint8_t)value;
   out[1] = (uint8_t)(value >> 8);
   out[2] = (uint8_t)(value >> 16);
   out[3] = (uint8_t)(value >> 24);
}

static INLINE int dxva_av1_clip16(int64_t value)
{
   return value < -32768 ? -32768 : value > 32767 ? 32767 : (int)value;
}

static INLINE int64_t dxva_av1_round(int64_t value, unsigned shift)
{
   int64_t half = (int64_t)1 << (shift - 1);
   return value < 0 ? -((-value + half) >> shift) : (value + half) >> shift;
}

/* AV1 7.11.3.6/7: the specified reciprocal table is nearest-integer division. */
static INLINE unsigned dxva_av1_warp_invalid(const int32_t matrix[6])
{
   unsigned exponent = 0, fraction, divisor, shift;
   int alpha, beta, gamma, delta;
   int64_t reciprocal, v, w;
   if (matrix[2] <= 0)
      return 1;
   divisor = (unsigned)matrix[2];
   while (divisor >> (exponent + 1)) exponent++;
   fraction = divisor - (1u << exponent);
   if (exponent > 8)
      fraction = (fraction + (1u << (exponent - 9))) >> (exponent - 8);
   else
      fraction <<= 8 - exponent;
   reciprocal = (4194304u + (256 + fraction) / 2) / (256 + fraction);
   shift = exponent + 14;
   v = (int64_t)matrix[4] * 65536;
   w = (int64_t)matrix[3] * matrix[4];
   alpha = dxva_av1_clip16((int64_t)matrix[2] - 65536);
   beta = dxva_av1_clip16(matrix[3]);
   gamma = dxva_av1_clip16(dxva_av1_round(v * reciprocal, shift));
   delta = dxva_av1_clip16((int64_t)matrix[5] -
         dxva_av1_round(w * reciprocal, shift) - 65536);
   alpha = (int)dxva_av1_round(alpha, 6) * 64;
   beta = (int)dxva_av1_round(beta, 6) * 64;
   gamma = (int)dxva_av1_round(gamma, 6) * 64;
   delta = (int)dxva_av1_round(delta, 6) * 64;
   if (alpha < 0) alpha = -alpha;
   if (beta < 0) beta = -beta;
   if (gamma < 0) gamma = -gamma;
   if (delta < 0) delta = -delta;
   return 4 * alpha + 7 * beta >= 65536 || 4 * gamma + 4 * delta >= 65536;
}

static INLINE bool dxva_av1_sequence(const rav1_sequence_t *s)
{
   return s && !s->profile && (s->bit_depth == 8 || s->bit_depth == 10) &&
       !s->monochrome && s->subsampling_x == 1 && s->subsampling_y == 1 &&
       s->max_width && s->max_width <= 65536 &&
       s->max_height && s->max_height <= 65536 &&
       ((!s->enable_order_hint && !s->order_hint_bits) ||
        (s->enable_order_hint && s->order_hint_bits && s->order_hint_bits <= 8));
}

/* Slot names in the parser remain logical. Only the surface map and current
 * target use physical indices; frame_refs[].Index addresses the eight-map. */
static INLINE bool dxva_av1_pic_params(uint8_t *out,
      const rav1_sequence_t *s, const rav1_hw_picture_t *p,
      const unsigned slot_map[32], unsigned target)
{
   const rav1_picture_t *q;
   const rav1_grain_t *g;
   unsigned i, j, a, b, sb, cols, rows, unit, flags;
   if (!out) return false;
   memset(out, 0, DXVA_AV1_PP_SIZE);
   if (!dxva_av1_sequence(s) || !p || !slot_map || target >= 255 ||
       p->slot < 0 || p->slot >= 32 || p->header.show_existing ||
       p->header.frame_type > RAV1_FRAME_SWITCH || p->header.primary_ref_frame > 7 ||
       p->header.order_hint > 255 || !p->setup.size.width || !p->setup.size.height ||
       p->setup.size.upscaled_width < p->setup.size.width ||
       p->setup.size.upscaled_width > s->max_width || p->setup.size.height > s->max_height ||
       p->setup.size.superres_denom < 8 || p->setup.size.superres_denom > 16 ||
       p->setup.interpolation_filter > 4)
      return false;
   cols = p->tiles.layout.columns; rows = p->tiles.layout.rows;
   if (!cols || cols > 64 || !rows || rows > 64 || p->tiles.context_update_tile_id >= cols * rows)
      return false;
   q = &p->picture; g = &p->grain.params;
   if (q->base_q_idx > 255 || q->tx_mode > 2 || q->delta_q_res > 3 ||
       q->delta_lf_res > 3 || q->loop_filter_sharpness > 7 || q->cdef_bits > 3 ||
       q->cdef_damping > 6 || (q->cdef_damping && q->cdef_damping < 3) ||
       q->delta_q_y_dc < -64 || q->delta_q_y_dc > 63 ||
       q->delta_q_u_dc < -64 || q->delta_q_u_dc > 63 ||
       q->delta_q_u_ac < -64 || q->delta_q_u_ac > 63 ||
       q->delta_q_v_dc < -64 || q->delta_q_v_dc > 63 ||
       q->delta_q_v_ac < -64 || q->delta_q_v_ac > 63 ||
       (q->using_qmatrix && (q->qm_y > 15 || q->qm_u > 15 || q->qm_v > 15)))
      return false;
   dxva_av1_put32(out, p->setup.size.upscaled_width);
   dxva_av1_put32(out + 4, p->setup.size.height);
   dxva_av1_put32(out + 8, s->max_width); dxva_av1_put32(out + 12, s->max_height);
   out[16] = (uint8_t)target; out[17] = (uint8_t)p->setup.size.superres_denom;
   out[18] = (uint8_t)s->bit_depth; out[19] = (uint8_t)s->profile;
   out[20] = (uint8_t)cols; out[21] = (uint8_t)rows;
   dxva_av1_put16(out + 22, p->tiles.context_update_tile_id);
   sb = s->use_128x128_superblock ? 32 : 16;
   if (p->tiles.mi_col_starts[0] || p->tiles.mi_row_starts[0] ||
       p->tiles.mi_col_starts[cols] != (p->setup.size.width + 7) / 8 * 2 ||
       p->tiles.mi_row_starts[rows] != (p->setup.size.height + 7) / 8 * 2)
      goto fail;
   for (i = 0; i < cols + rows; i++)
   {
      j = i < cols ? i : i - cols;
      a = i < cols ? p->tiles.mi_col_starts[j] : p->tiles.mi_row_starts[j];
      b = i < cols ? p->tiles.mi_col_starts[j + 1] : p->tiles.mi_row_starts[j + 1];
      if (b <= a || a % sb || b > 16384) goto fail;
      dxva_av1_put16(out + (i < cols ? 24 : 152) + j * 2,
            (b + sb - 1) / sb - a / sb);
   }
   flags = (!!s->use_128x128_superblock) | !!s->enable_intra_edge_filter << 1 |
       !!s->enable_interintra_compound << 2 | !!s->enable_masked_compound << 3 |
       !!p->motion.allow_warped_motion << 4 | !!s->enable_dual_filter << 5 |
       !!s->enable_jnt_comp << 6 | !!p->header.allow_screen_content_tools << 7 |
       !!p->header.force_integer_mv << 8 | !!s->enable_cdef << 9 |
       !!s->enable_restoration << 10 | !!s->film_grain_params_present << 11 |
       !!p->setup.allow_intrabc << 12 | !!p->setup.allow_high_precision_mv << 13 |
       !!p->setup.motion_mode_switchable << 14 | !!s->enable_filter_intra << 15 |
       !!p->setup.disable_frame_end_update_cdf << 16 | !!p->header.disable_cdf_update << 17 |
       !!p->motion.reference_select << 18 | !!p->motion.skip_mode_present << 19 |
       !!p->motion.reduced_tx_set << 20 | (p->setup.size.superres_denom != 8) << 21 |
       q->tx_mode << 22 | !!p->setup.use_ref_frame_mvs << 24 |
       !!s->enable_ref_frame_mvs << 25 | 1u << 26;
   dxva_av1_put32(out + 280, flags);
   out[284] = (uint8_t)(p->header.frame_type | !!p->header.show_frame << 2 |
         !!p->header.showable_frame << 3 | 1u << 4 | 1u << 5);
   out[285] = (uint8_t)p->header.primary_ref_frame;
   out[286] = (uint8_t)p->header.order_hint;
   out[287] = (uint8_t)s->order_hint_bits;
   memset(out + 540, 255, 8);
   for (i = 0; i < 8; i++)
   {
      int slot = p->reference_slots[i];
      if (slot < -1 || slot >= 32 || slot == p->slot) goto fail;
      if (slot >= 0)
      {
         if (!p->reference_info[i].valid || slot_map[slot] >= 255 || slot_map[slot] == target)
            goto fail;
         out[540 + i] = (uint8_t)slot_map[slot];
      }
      if (q->reference.segment_enabled[i] > 255 ||
          q->reference.loop_filter_ref_deltas[i] < -64 || q->reference.loop_filter_ref_deltas[i] > 63)
         goto fail;
      out[554 + i] = (uint8_t)q->reference.loop_filter_ref_deltas[i];
      out[610 + i] = (uint8_t)q->reference.segment_enabled[i];
      for (j = 0; j < 8; j++)
      {
         static const int limits[8] = {255, 63, 63, 63, 63, 7, 0, 0};
         int value = q->reference.segment_data[i][j];
         if (value < -limits[j] || value > limits[j] || (j == 5 && value < 0)) goto fail;
         dxva_av1_put16(out + 618 + (i * 8 + j) * 2, (unsigned)value);
      }
      if (q->cdef_y_pri[i] > 15 || q->cdef_uv_pri[i] > 15 ||
          q->cdef_y_sec[i] > 4 || q->cdef_y_sec[i] == 3 ||
          q->cdef_uv_sec[i] > 4 || q->cdef_uv_sec[i] == 3) goto fail;
      out[589 + i] = (uint8_t)(q->cdef_y_pri[i] | (q->cdef_y_sec[i] == 4 ? 3 : q->cdef_y_sec[i]) << 6);
      out[597 + i] = (uint8_t)(q->cdef_uv_pri[i] | (q->cdef_uv_sec[i] == 4 ? 3 : q->cdef_uv_sec[i]) << 6);
   }
   for (i = 0; i < 7; i++)
   {
      uint8_t *entry = out + 288 + i * 36;
      entry[33] = 255;
      if (!p->header.frame_is_intra)
      {
         const int32_t *m = p->motion.global.params[i];
         j = p->setup.ref_frame_idx[i];
         if (j >= 8 || p->reference_slots[j] < 0 || p->motion.global.type[i] > 3 ||
             !p->reference_info[j].size.upscaled_width ||
             p->reference_info[j].size.upscaled_width > s->max_width ||
             !p->reference_info[j].size.height || p->reference_info[j].size.height > s->max_height)
            goto fail;
         dxva_av1_put32(entry, p->reference_info[j].size.upscaled_width);
         dxva_av1_put32(entry + 4, p->reference_info[j].size.height);
         entry[33] = (uint8_t)j;
         for (j = 0; j < 6; j++)
         {
            if (m[j] < (j < 2 ? -16777216 : -131072) ||
                m[j] > (j < 2 ? 16777216 : 131072)) goto fail;
            dxva_av1_put32(entry + 8 + j * 4, (uint32_t)m[j]);
         }
         entry[32] = (uint8_t)(dxva_av1_warp_invalid(m) | p->motion.global.type[i] << 1);
      }
   }
   for (i = 0; i < 4; i++)
   {
      if (q->loop_filter_level[i] > 63) goto fail;
      out[548 + i] = (uint8_t)q->loop_filter_level[i];
   }
   out[552] = (uint8_t)q->loop_filter_sharpness;
   out[553] = (uint8_t)((!!q->loop_filter_delta_enabled) | !!q->loop_filter_delta_update << 1 |
         !!q->delta_lf_multi << 2 | !!q->delta_lf_present << 3);
   for (i = 0; i < 2; i++)
   {
      if (q->reference.loop_filter_mode_deltas[i] < -64 || q->reference.loop_filter_mode_deltas[i] > 63)
         goto fail;
      out[562 + i] = (uint8_t)q->reference.loop_filter_mode_deltas[i];
   }
   out[564] = (uint8_t)q->delta_lf_res;
   for (i = 0; i < 3; i++)
   {
      if (q->restoration_type[i] > 3) goto fail;
      out[565 + i] = (uint8_t)q->restoration_type[i];
      unit = q->restoration_size[i];
      if (!unit) unit = 256;
      if (unit < 32 || unit > 256 || (unit & (unit - 1))) goto fail;
      for (j = 0; unit > 1; unit >>= 1) j++;
      dxva_av1_put16(out + 568 + i * 2, j);
   }
   out[576] = (uint8_t)((!!q->delta_q_present) | q->delta_q_res << 1);
   out[577] = (uint8_t)q->base_q_idx;
   out[578] = (uint8_t)q->delta_q_y_dc; out[579] = (uint8_t)q->delta_q_u_dc;
   out[580] = (uint8_t)q->delta_q_v_dc; out[581] = (uint8_t)q->delta_q_u_ac;
   out[582] = (uint8_t)q->delta_q_v_ac;
   out[583] = q->using_qmatrix ? (uint8_t)q->qm_y : 255;
   out[584] = q->using_qmatrix ? (uint8_t)q->qm_u : 255;
   out[585] = q->using_qmatrix ? (uint8_t)q->qm_v : 255;
   out[588] = (uint8_t)((q->cdef_damping ? q->cdef_damping - 3 : 0) | q->cdef_bits << 2);
   out[605] = (uint8_t)p->setup.interpolation_filter;
   out[606] = (uint8_t)((!!q->segmentation_enabled) | !!q->segmentation_update_map << 1 |
         !!q->segmentation_update_data << 2 | !!q->segmentation_temporal_update << 3);
   /* Grain-bearing pictures require distinct reference/display storage. */
   if (g->apply) goto fail;
   /* Status queries are unused; nonzero AV1 feedback IDs break NVIDIA drivers. */
   return true;
fail:
   memset(out, 0, DXVA_AV1_PP_SIZE);
   return false;
}

/* Full-frame tile groups only. Validate before changing the caller's buffer.
 * Offsets address the submitted packet, excluding tile size prefixes. */
static INLINE bool dxva_av1_tiles(uint8_t *out, size_t capacity,
      unsigned *written, const uint8_t *packet, size_t size,
      const rav1_tile_group_t *input, unsigned columns, unsigned rows)
{
   rav1_tile_group_t group;
   rav1_tile_t tile;
   uintptr_t base, address;
   size_t offset;
   unsigned count, i;
   if (!written)
      return false;
   *written = 0;
   if (!out || !packet || !size || size > UINT32_MAX || !input ||
       !columns || columns > 64 || !rows || rows > 64)
      return false;
   count = columns * rows;
   if (capacity / DXVA_AV1_TILE_SIZE < count || !input->data ||
       input->first || input->next || input->last != count - 1 ||
       input->columns != columns || input->size_bytes > 4 ||
       (count > 1 && !input->size_bytes))
      return false;
   base = (uintptr_t)packet;
   address = (uintptr_t)input->data;
   if (address < base || address - base >= size)
      return false;
   offset = (size_t)(address - base);
   if (input->size != size - offset)
      return false;
   group = *input;
   for (i = 0; i < count; i++)
      if (rav1_tile_group_next(&tile, &group) != 1 || tile.index != i ||
          tile.row >= rows || tile.column >= columns)
         return false;
   if (group.offset != group.size)
      return false;
   group = *input;
   for (i = 0; i < count; i++)
   {
      uint8_t *entry = out + i * DXVA_AV1_TILE_SIZE;
      rav1_tile_group_next(&tile, &group);
      memset(entry, 0, DXVA_AV1_TILE_SIZE);
      dxva_av1_put32(entry, (uint32_t)(offset + (size_t)(tile.data - input->data)));
      dxva_av1_put32(entry + 4, (uint32_t)tile.size);
      dxva_av1_put16(entry + 8, tile.row);
      dxva_av1_put16(entry + 10, tile.column);
      entry[14] = 255;
   }
   *written = count;
   return true;
}

#endif
