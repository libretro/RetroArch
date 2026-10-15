/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VULKAN_AV1_PARAMS_H
#define RARCH_VULKAN_AV1_PARAMS_H

#include <formats/rav1_hw.h>
#include "../include/vulkan/vulkan.h"

/* Stable caller storage: internal pointers refer into these bundles. */
typedef struct vulkan_av1_sequence
{
   StdVideoAV1SequenceHeader header;
   StdVideoAV1ColorConfig color;
   StdVideoAV1TimingInfo timing;
} vulkan_av1_sequence_t;

typedef struct vulkan_av1_picture
{
   VkVideoDecodeAV1PictureInfoKHR info;
   StdVideoDecodeAV1PictureInfo header;
   StdVideoDecodeAV1ReferenceInfo reference;
   StdVideoAV1TileInfo tiles;
   StdVideoAV1Quantization quant;
   StdVideoAV1Segmentation segments;
   StdVideoAV1LoopFilter filter;
   StdVideoAV1CDEF cdef;
   StdVideoAV1LoopRestoration restoration;
   StdVideoAV1GlobalMotion global;
   StdVideoAV1FilmGrain grain;
   uint32_t offsets[4096], sizes[4096];
   uint16_t col_starts[65], row_starts[65];
   uint16_t widths[64], heights[64];
} vulkan_av1_picture_t;

/* Input is a parsed sequence. Main 8/10-bit 4:2:0, one operating point. */
static INLINE bool vulkan_av1_sequence(vulkan_av1_sequence_t *out,
                                       const rav1_sequence_t *s)
{
   StdVideoAV1SequenceHeader *h;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!s || s->profile || (s->bit_depth != 8 && s->bit_depth != 10) ||
       s->monochrome || s->subsampling_x != 1 || s->subsampling_y != 1 ||
       !s->max_width || s->max_width > 65536 || !s->max_height ||
       s->max_height > 65536 || !s->frame_width_bits ||
       s->frame_width_bits > 16 || !s->frame_height_bits ||
       s->frame_height_bits > 16 ||
       s->max_width > (1u << s->frame_width_bits) ||
       s->max_height > (1u << s->frame_height_bits) ||
       s->operating_point_count != 1 || s->operating_points[0].idc ||
       s->operating_points[0].level > 23 || s->force_integer_mv > 2 ||
       s->force_screen_content_tools > 2 ||
       (s->enable_order_hint &&
        (!s->order_hint_bits || s->order_hint_bits > 8)) ||
       (s->frame_id_numbers_present &&
        (s->delta_frame_id_length < 2 || s->delta_frame_id_length > 17 ||
         s->frame_id_length > 16 ||
         s->frame_id_length <= s->delta_frame_id_length)))
      return false;
   h = &out->header;
   h->seq_profile = STD_VIDEO_AV1_PROFILE_MAIN;
   h->flags.still_picture = !!s->still_picture;
   h->flags.reduced_still_picture_header = !!s->reduced_still_picture_header;
   h->flags.use_128x128_superblock = !!s->use_128x128_superblock;
   h->flags.enable_filter_intra = !!s->enable_filter_intra;
   h->flags.enable_intra_edge_filter = !!s->enable_intra_edge_filter;
   h->flags.enable_interintra_compound = !!s->enable_interintra_compound;
   h->flags.enable_masked_compound = !!s->enable_masked_compound;
   h->flags.enable_warped_motion = !!s->enable_warped_motion;
   h->flags.enable_dual_filter = !!s->enable_dual_filter;
   h->flags.enable_order_hint = !!s->enable_order_hint;
   h->flags.enable_jnt_comp = !!s->enable_jnt_comp;
   h->flags.enable_ref_frame_mvs = !!s->enable_ref_frame_mvs;
   h->flags.frame_id_numbers_present_flag = !!s->frame_id_numbers_present;
   h->flags.enable_superres = !!s->enable_superres;
   h->flags.enable_cdef = !!s->enable_cdef;
   h->flags.enable_restoration = !!s->enable_restoration;
   h->flags.film_grain_params_present = !!s->film_grain_params_present;
   h->flags.timing_info_present_flag = !!s->timing_info_present;
   h->flags.initial_display_delay_present_flag =
       !!s->initial_display_delay_present;
   h->frame_width_bits_minus_1 = (uint8_t)(s->frame_width_bits - 1);
   h->frame_height_bits_minus_1 = (uint8_t)(s->frame_height_bits - 1);
   h->max_frame_width_minus_1 = (uint16_t)(s->max_width - 1);
   h->max_frame_height_minus_1 = (uint16_t)(s->max_height - 1);
   if (s->frame_id_numbers_present)
   {
      h->delta_frame_id_length_minus_2 =
          (uint8_t)(s->delta_frame_id_length - 2);
      h->additional_frame_id_length_minus_1 =
          (uint8_t)(s->frame_id_length - s->delta_frame_id_length - 1);
   }
   h->order_hint_bits_minus_1 =
       s->enable_order_hint ? (uint8_t)(s->order_hint_bits - 1) : 0;
   h->seq_force_integer_mv = (uint8_t)s->force_integer_mv;
   h->seq_force_screen_content_tools = (uint8_t)s->force_screen_content_tools;
   h->pColorConfig = &out->color;
   h->pTimingInfo = s->timing_info_present ? &out->timing : NULL;
   out->color.flags.color_range = !!s->color_range;
   out->color.flags.separate_uv_delta_q = !!s->separate_uv_delta_q;
   out->color.flags.color_description_present_flag =
       !!s->color_description_present;
   out->color.BitDepth = (uint8_t)s->bit_depth;
   out->color.subsampling_x = out->color.subsampling_y = 1;
   out->color.color_primaries = (StdVideoAV1ColorPrimaries)s->color_primaries;
   out->color.transfer_characteristics =
       (StdVideoAV1TransferCharacteristics)s->transfer_characteristics;
   out->color.matrix_coefficients =
       (StdVideoAV1MatrixCoefficients)s->matrix_coefficients;
   out->color.chroma_sample_position =
       (StdVideoAV1ChromaSamplePosition)s->chroma_sample_position;
   out->timing.flags.equal_picture_interval = !!s->equal_picture_interval;
   out->timing.num_units_in_display_tick = s->num_units_in_display_tick;
   out->timing.time_scale = s->time_scale;
   out->timing.num_ticks_per_picture_minus_1 =
       s->num_ticks_per_picture_minus_one;
   return true;
}

/* Input comes from rav1_hw_session_submit's decode callback. No pixel or
 * compressed-data copies; offsets address the caller's complete FRAME OBU.
 * Allocate this bundle on the heap and retain it through command recording. */
static INLINE bool vulkan_av1_picture(vulkan_av1_picture_t *out,
                                      const uint8_t *packet, size_t size,
                                      const rav1_sequence_t *s,
                                      const rav1_hw_picture_t *p)
{
   StdVideoDecodeAV1PictureInfo *h;
   const rav1_picture_t *q;
   const rav1_grain_t *g;
   rav1_tile_group_t group;
   rav1_tile_t tile;
   rav1_obu_t obu;
   size_t offset = 0, base;
   unsigned i, j, count, sb, a, b, lr, bias = 0;
   uintptr_t address;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!packet || !size || size > UINT32_MAX || !s || !p || s->profile ||
       (s->bit_depth != 8 && s->bit_depth != 10) || s->monochrome ||
       s->subsampling_x != 1 || s->subsampling_y != 1 || p->header.show_existing ||
       p->header.frame_type > RAV1_FRAME_SWITCH || p->slot < 0 ||
       p->slot >= 32 || p->header.primary_ref_frame > 7 ||
       p->header.order_hint > 255 || p->header.refresh_frame_flags > 255 ||
       !p->setup.size.width || !p->setup.size.height ||
       p->setup.size.upscaled_width < p->setup.size.width ||
       p->setup.size.upscaled_width > s->max_width ||
       p->setup.size.height > s->max_height ||
       p->setup.size.superres_denom < 8 || p->setup.size.superres_denom > 16 ||
       !p->tiles.layout.columns || p->tiles.layout.columns > 64 ||
       !p->tiles.layout.rows || p->tiles.layout.rows > 64 ||
       p->tiles.layout.size_bytes > 4 || p->setup.interpolation_filter > 4 ||
       p->picture.tx_mode > 2 ||
       rav1_obu_next(&obu, packet, size, &offset, true) != 1 ||
       offset != size || obu.type != 6 || obu.temporal_id || obu.spatial_id)
      return false;
   count = p->tiles.layout.columns * p->tiles.layout.rows;
   address = (uintptr_t)p->tile_group.data;
   if (address < (uintptr_t)obu.payload || address - (uintptr_t)packet > size)
      return false;
   base = (size_t)(address - (uintptr_t)packet);
   if (p->tile_group.size != size - base || p->tile_group.first ||
       p->tile_group.next || p->tile_group.last != count - 1 ||
       p->tile_group.columns != p->tiles.layout.columns ||
       p->tile_group.size_bytes != p->tiles.layout.size_bytes ||
       p->tiles.context_update_tile_id >= count)
      return false;
   group = p->tile_group;
   for (i = 0; i < count; i++)
   {
      if (rav1_tile_group_next(&tile, &group) != 1 || tile.index != i ||
          tile.size > UINT32_MAX)
         goto fail;
      out->offsets[i] = (uint32_t)(base + (size_t)(tile.data - group.data));
      out->sizes[i] = (uint32_t)tile.size;
   }
   if (group.offset != group.size)
      goto fail;
   sb = s->use_128x128_superblock ? 32 : 16;
   for (i = 0; i <= p->tiles.layout.columns; i++)
   {
      a = p->tiles.mi_col_starts[i];
      if (a > (p->setup.size.width + 7) / 8 * 2 || a > UINT16_MAX)
         goto fail;
      out->col_starts[i] = (uint16_t)a;
      if (i == p->tiles.layout.columns)
         break;
      b = p->tiles.mi_col_starts[i + 1];
      if (b <= a || a % sb)
         goto fail;
      out->widths[i] = (uint16_t)((b + sb - 1) / sb - a / sb - 1);
   }
   for (i = 0; i <= p->tiles.layout.rows; i++)
   {
      a = p->tiles.mi_row_starts[i];
      if (a > (p->setup.size.height + 7) / 8 * 2 || a > UINT16_MAX)
         goto fail;
      out->row_starts[i] = (uint16_t)a;
      if (i == p->tiles.layout.rows)
         break;
      b = p->tiles.mi_row_starts[i + 1];
      if (b <= a || a % sb)
         goto fail;
      out->heights[i] = (uint16_t)((b + sb - 1) / sb - a / sb - 1);
   }
   if (out->col_starts[0] || out->row_starts[0] ||
       out->col_starts[p->tiles.layout.columns] !=
           (p->setup.size.width + 7) / 8 * 2 ||
       out->row_starts[p->tiles.layout.rows] !=
           (p->setup.size.height + 7) / 8 * 2)
      goto fail;
   h = &out->header;
   q = &p->picture;
   g = &p->grain.params;
   h->flags.error_resilient_mode = !!p->header.error_resilient;
   h->flags.disable_cdf_update = !!p->header.disable_cdf_update;
   h->flags.use_superres = p->setup.size.superres_denom != 8;
   h->flags.render_and_frame_size_different =
       p->setup.size.render_width != p->setup.size.upscaled_width ||
       p->setup.size.render_height != p->setup.size.height;
   h->flags.allow_screen_content_tools = !!p->header.allow_screen_content_tools;
   h->flags.is_filter_switchable = p->setup.interpolation_filter == 4;
   h->flags.force_integer_mv = !!p->header.force_integer_mv;
   h->flags.frame_size_override_flag = !!p->header.size_override;
   h->flags.buffer_removal_time_present_flag = !!p->header.removal_time_present;
   h->flags.allow_intrabc = !!p->setup.allow_intrabc;
   h->flags.frame_refs_short_signaling = !!p->setup.short_signaling;
   h->flags.allow_high_precision_mv = !!p->setup.allow_high_precision_mv;
   h->flags.is_motion_mode_switchable = !!p->setup.motion_mode_switchable;
   h->flags.use_ref_frame_mvs = !!p->setup.use_ref_frame_mvs;
   h->flags.disable_frame_end_update_cdf =
       !!p->setup.disable_frame_end_update_cdf;
   h->flags.allow_warped_motion = !!p->motion.allow_warped_motion;
   h->flags.reduced_tx_set = !!p->motion.reduced_tx_set;
   h->flags.reference_select = !!p->motion.reference_select;
   h->flags.skip_mode_present = !!p->motion.skip_mode_present;
   h->flags.delta_q_present = !!q->delta_q_present;
   h->flags.delta_lf_present = !!q->delta_lf_present;
   h->flags.delta_lf_multi = !!q->delta_lf_multi;
   h->flags.segmentation_enabled = !!q->segmentation_enabled;
   h->flags.segmentation_update_map = !!q->segmentation_update_map;
   h->flags.segmentation_temporal_update = !!q->segmentation_temporal_update;
   h->flags.segmentation_update_data = !!q->segmentation_update_data;
   h->flags.apply_grain = !!g->apply;
   h->frame_type = (StdVideoAV1FrameType)p->header.frame_type;
   h->current_frame_id = p->header.current_frame_id;
   h->OrderHint = (uint8_t)p->header.order_hint;
   h->primary_ref_frame = (uint8_t)p->header.primary_ref_frame;
   h->refresh_frame_flags = (uint8_t)p->header.refresh_frame_flags;
   h->interpolation_filter =
       (StdVideoAV1InterpolationFilter)p->setup.interpolation_filter;
   h->TxMode = (StdVideoAV1TxMode)q->tx_mode;
   h->delta_q_res = (uint8_t)q->delta_q_res;
   h->delta_lf_res = (uint8_t)q->delta_lf_res;
   h->coded_denom =
       h->flags.use_superres ? (uint8_t)(p->setup.size.superres_denom - 9) : 0;
   for (i = 0; i < 2; i++)
      h->SkipModeFrame[i] = (uint8_t)p->motion.skip_mode_frame[i];
   memcpy(h->OrderHints, p->order_hints, sizeof(h->OrderHints));
   for (i = 0; i < 7; i++)
   {
      out->info.referenceNameSlotIndices[i] = -1;
      if (!p->header.frame_is_intra)
      {
         j = p->setup.ref_frame_idx[i];
         if (j >= 8 || p->reference_slots[j] < 0 ||
             p->reference_slots[j] >= 32 || p->reference_slots[j] == p->slot ||
             !p->reference_info[j].valid)
            goto fail;
         out->info.referenceNameSlotIndices[i] = p->reference_slots[j];
         if (s->frame_id_numbers_present)
            h->expectedFrameId[i] = p->reference_info[j].frame_id;
         bias |= !!p->setup.sign_bias[i] << (i + 1);
      }
      out->global.GmType[i + 1] = (uint8_t)p->motion.global.type[i];
      for (j = 0; j < 6; j++)
         out->global.gm_params[i + 1][j] = p->motion.global.params[i][j];
   }
   out->global.gm_params[0][2] = out->global.gm_params[0][5] = 1 << 16;
   out->reference.flags.disable_frame_end_update_cdf =
       !!p->setup.disable_frame_end_update_cdf;
   out->reference.flags.segmentation_enabled = !!q->segmentation_enabled;
   out->reference.frame_type = (uint8_t)p->header.frame_type;
   out->reference.RefFrameSignBias = (uint8_t)bias;
   out->reference.OrderHint = h->OrderHint;
   memcpy(out->reference.SavedOrderHints, p->order_hints, 8);
   out->tiles.flags.uniform_tile_spacing_flag = !!p->tiles.uniform_spacing;
   out->tiles.TileCols = (uint8_t)p->tiles.layout.columns;
   out->tiles.TileRows = (uint8_t)p->tiles.layout.rows;
   out->tiles.context_update_tile_id =
       (uint16_t)p->tiles.context_update_tile_id;
   out->tiles.tile_size_bytes_minus_1 =
       p->tiles.layout.size_bytes ? (uint8_t)(p->tiles.layout.size_bytes - 1)
                                  : 0;
   out->tiles.pMiColStarts = out->col_starts;
   out->tiles.pMiRowStarts = out->row_starts;
   out->tiles.pWidthInSbsMinus1 = out->widths;
   out->tiles.pHeightInSbsMinus1 = out->heights;
   out->quant.flags.using_qmatrix = !!q->using_qmatrix;
   out->quant.flags.diff_uv_delta = !!q->diff_uv_delta;
   out->quant.base_q_idx = (uint8_t)q->base_q_idx;
   out->quant.DeltaQYDc = (int8_t)q->delta_q_y_dc;
   out->quant.DeltaQUDc = (int8_t)q->delta_q_u_dc;
   out->quant.DeltaQUAc = (int8_t)q->delta_q_u_ac;
   out->quant.DeltaQVDc = (int8_t)q->delta_q_v_dc;
   out->quant.DeltaQVAc = (int8_t)q->delta_q_v_ac;
   out->quant.qm_y = (uint8_t)q->qm_y;
   out->quant.qm_u = (uint8_t)q->qm_u;
   out->quant.qm_v = (uint8_t)q->qm_v;
   out->filter.flags.loop_filter_delta_enabled = !!q->loop_filter_delta_enabled;
   out->filter.flags.loop_filter_delta_update = !!q->loop_filter_delta_update;
   out->filter.loop_filter_sharpness = (uint8_t)q->loop_filter_sharpness;
   out->filter.update_ref_delta = (uint8_t)q->loop_filter_update_ref_delta;
   out->filter.update_mode_delta = (uint8_t)q->loop_filter_update_mode_delta;
   for (i = 0; i < 4; i++)
      out->filter.loop_filter_level[i] = (uint8_t)q->loop_filter_level[i];
   for (i = 0; i < 8; i++)
   {
      out->filter.loop_filter_ref_deltas[i] =
          (int8_t)q->reference.loop_filter_ref_deltas[i];
      out->segments.FeatureEnabled[i] =
          (uint8_t)q->reference.segment_enabled[i];
      for (j = 0; j < 8; j++)
         out->segments.FeatureData[i][j] =
             (int16_t)q->reference.segment_data[i][j];
      out->cdef.cdef_y_pri_strength[i] = (uint8_t)q->cdef_y_pri[i];
      out->cdef.cdef_y_sec_strength[i] =
          (uint8_t)(q->cdef_y_sec[i] == 4 ? 3 : q->cdef_y_sec[i]);
      out->cdef.cdef_uv_pri_strength[i] = (uint8_t)q->cdef_uv_pri[i];
      out->cdef.cdef_uv_sec_strength[i] =
          (uint8_t)(q->cdef_uv_sec[i] == 4 ? 3 : q->cdef_uv_sec[i]);
   }
   for (i = 0; i < 2; i++)
      out->filter.loop_filter_mode_deltas[i] =
          (int8_t)q->reference.loop_filter_mode_deltas[i];
   out->cdef.cdef_damping_minus_3 = (uint8_t)(q->cdef_damping - 3);
   out->cdef.cdef_bits = (uint8_t)q->cdef_bits;
   for (i = 0; i < 3; i++)
   {
      if (q->restoration_type[i])
      {
         h->flags.UsesLr = 1;
         if (i)
            h->flags.usesChromaLr = 1;
      }
      out->restoration.FrameRestorationType[i] =
          (StdVideoAV1FrameRestorationType)q->restoration_type[i];
      lr = q->restoration_size[i];
      if (lr)
      {
         if (lr < 32 || lr > 256 || (lr & (lr - 1)))
            goto fail;
         for (j = 0; lr > 32; lr >>= 1)
            j++;
         out->restoration.LoopRestorationSize[i] = (uint16_t)j;
      }
   }
   if (g->apply)
   {
      if (g->scaling_shift < 8 || g->scaling_shift > 11 ||
          g->ar_coeff_shift < 6 || g->ar_coeff_shift > 9 ||
          g->num_points[0] > 14 || g->num_points[1] > 10 ||
          g->num_points[2] > 10)
         goto fail;
      out->grain.flags.chroma_scaling_from_luma = !!g->chroma_scaling_from_luma;
      out->grain.flags.overlap_flag = !!g->overlap;
      out->grain.flags.clip_to_restricted_range = !!g->clip_to_restricted_range;
      out->grain.flags.update_grain = !!p->grain.update;
      out->grain.grain_scaling_minus_8 = (uint8_t)(g->scaling_shift - 8);
      out->grain.ar_coeff_lag = (uint8_t)g->ar_coeff_lag;
      out->grain.ar_coeff_shift_minus_6 = (uint8_t)(g->ar_coeff_shift - 6);
      out->grain.grain_scale_shift = (uint8_t)g->scale_shift;
      out->grain.grain_seed = (uint16_t)g->seed;
      out->grain.film_grain_params_ref_idx = (uint8_t)p->grain.reference_idx;
      out->grain.num_y_points = g->num_points[0];
      out->grain.num_cb_points = g->num_points[1];
      out->grain.num_cr_points = g->num_points[2];
      memcpy(out->grain.point_y_value, g->point_value[0], 14);
      memcpy(out->grain.point_y_scaling, g->point_scaling[0], 14);
      memcpy(out->grain.point_cb_value, g->point_value[1], 10);
      memcpy(out->grain.point_cb_scaling, g->point_scaling[1], 10);
      memcpy(out->grain.point_cr_value, g->point_value[2], 10);
      memcpy(out->grain.point_cr_scaling, g->point_scaling[2], 10);
      for (i = 0; i < 25; i++)
      {
         if (i < 24)
            out->grain.ar_coeffs_y_plus_128[i] =
                (int8_t)((int)g->ar_coeffs[0][i] + 128);
         out->grain.ar_coeffs_cb_plus_128[i] =
             (int8_t)((int)g->ar_coeffs[1][i] + 128);
         out->grain.ar_coeffs_cr_plus_128[i] =
             (int8_t)((int)g->ar_coeffs[2][i] + 128);
      }
      out->grain.cb_mult = (uint8_t)g->chroma_mult[0];
      out->grain.cb_luma_mult = (uint8_t)g->chroma_luma_mult[0];
      out->grain.cb_offset = (uint16_t)g->chroma_offset[0];
      out->grain.cr_mult = (uint8_t)g->chroma_mult[1];
      out->grain.cr_luma_mult = (uint8_t)g->chroma_luma_mult[1];
      out->grain.cr_offset = (uint16_t)g->chroma_offset[1];
   }
   h->pTileInfo = &out->tiles;
   h->pQuantization = &out->quant;
   h->pSegmentation = &out->segments;
   h->pLoopFilter = &out->filter;
   h->pCDEF = &out->cdef;
   h->pLoopRestoration = &out->restoration;
   h->pGlobalMotion = &out->global;
   h->pFilmGrain = s->film_grain_params_present ? &out->grain : NULL;
   out->info.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PICTURE_INFO_KHR;
   out->info.pNext = NULL;
   out->info.pStdPictureInfo = h;
   out->info.frameHeaderOffset = 0;
   out->info.tileCount = count;
   out->info.pTileOffsets = out->offsets;
   out->info.pTileSizes = out->sizes;
   return true;
fail:
   memset(out, 0, sizeof(*out));
   return false;
}

#endif
