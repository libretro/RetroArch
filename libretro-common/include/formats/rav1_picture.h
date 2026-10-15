/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_PICTURE_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_PICTURE_H__

#include <formats/rav1_frame.h>

enum rav1_restoration_type
{
   RAV1_RESTORE_NONE,
   RAV1_RESTORE_WIENER,
   RAV1_RESTORE_SGRPROJ,
   RAV1_RESTORE_SWITCHABLE
};

typedef struct rav1_picture_reference
{
   int segment_data[8][8];
   int loop_filter_ref_deltas[8], loop_filter_mode_deltas[2];
   unsigned segment_enabled[8];
} rav1_picture_reference_t;

typedef struct rav1_picture
{
   rav1_picture_reference_t reference;
   int delta_q_y_dc, delta_q_u_dc, delta_q_u_ac, delta_q_v_dc, delta_q_v_ac;
   unsigned base_q_idx, using_qmatrix, qm_y, qm_u, qm_v, diff_uv_delta;
   unsigned segmentation_enabled, segmentation_update_map;
   unsigned segmentation_temporal_update, segmentation_update_data;
   unsigned last_active_segment, segment_id_pre_skip;
   unsigned delta_q_present, delta_q_res, delta_lf_present, delta_lf_res,
       delta_lf_multi;
   unsigned lossless_mask, coded_lossless, all_lossless;
   unsigned loop_filter_level[4], loop_filter_sharpness;
   unsigned loop_filter_delta_enabled, loop_filter_delta_update;
   unsigned loop_filter_update_ref_delta, loop_filter_update_mode_delta;
   unsigned cdef_damping, cdef_bits;
   unsigned cdef_y_pri[8], cdef_y_sec[8], cdef_uv_pri[8], cdef_uv_sec[8];
   unsigned restoration_type[3], restoration_size[3], tx_mode;
} rav1_picture_t;

/* AV1 signed deltas use a fixed two's-complement width, optionally preceded
 * by a presence bit. The widths used by these picture tools are at most 9. */
static INLINE int rav1_bits_delta(rav1_bits_t *bits, unsigned width,
                                  bool presence)
{
   unsigned value;
   if (!width || width > 16)
   {
      bits->failed = 1;
      return 0;
   }
   if (presence && !rav1_bits_read(bits, 1))
      return 0;
   value = rav1_bits_read(bits, width);
   return (int)value - ((value & (1u << (width - 1))) ? (int)(1u << width) : 0);
}

/* Starts at base_q_idx, after tile_info(), and ends after read_tx_mode().
 * primary supplies inherited segmentation/filter metadata when
 * primary_ref_frame is not NONE. It must not alias output; an alias error
 * leaves it untouched. Other failures clear output and preserve the bit cursor.
 * Slot validation and committing this metadata after successful decode belong
 * to the caller. */
static INLINE bool rav1_picture_parse(rav1_picture_t *out, rav1_bits_t *input,
                                      const rav1_sequence_t *seq,
                                      const rav1_frame_prefix_t *frame,
                                      const rav1_frame_size_t *geometry,
                                      bool allow_intrabc,
                                      const rav1_picture_reference_t *primary)
{
   static const unsigned feature_bits[8] = {8, 6, 6, 6, 6, 3, 0, 0};
   static const int feature_max[8] = {255, 63, 63, 63, 63, 7, 0, 0};
   static const int default_deltas[8] = {1, 0, 0, 0, -1, 0, -1, -1};
   static const unsigned restoration_map[4] = {
       RAV1_RESTORE_NONE, RAV1_RESTORE_SWITCHABLE, RAV1_RESTORE_WIENER,
       RAV1_RESTORE_SGRPROJ};
   rav1_bits_t bits;
   unsigned i, j, different_uv = 0, uses_lr = 0, chroma_lr = 0, shift, uv_shift;
   int value, qindex;
   if (!out || primary == &out->reference)
      return false;
   memset(out, 0, sizeof(*out));
   if (!input || !seq || !frame || !geometry || input->failed || !input->data ||
       input->byte > input->size || input->bit > 7 || frame->show_existing ||
       frame->primary_ref_frame > 7 || !geometry->width || !geometry->height ||
       !geometry->upscaled_width ||
       geometry->width > geometry->upscaled_width ||
       geometry->upscaled_width > 65536 || geometry->height > 65536 ||
       (frame->primary_ref_frame != 7 && !primary))
      return false;
   bits = *input;
   if (frame->primary_ref_frame != 7)
      out->reference = *primary;
   else
      for (i = 0; i < 8; i++)
         out->reference.loop_filter_ref_deltas[i] = default_deltas[i];
   out->base_q_idx = rav1_bits_read(&bits, 8);
   out->delta_q_y_dc = rav1_bits_delta(&bits, 7, true);
   if (!seq->monochrome)
   {
      if (seq->separate_uv_delta_q)
         different_uv = rav1_bits_read(&bits, 1);
      out->delta_q_u_dc = rav1_bits_delta(&bits, 7, true);
      out->delta_q_u_ac = rav1_bits_delta(&bits, 7, true);
      if (different_uv)
      {
         out->delta_q_v_dc = rav1_bits_delta(&bits, 7, true);
         out->delta_q_v_ac = rav1_bits_delta(&bits, 7, true);
      }
      else
      {
         out->delta_q_v_dc = out->delta_q_u_dc;
         out->delta_q_v_ac = out->delta_q_u_ac;
      }
   }
   out->diff_uv_delta = different_uv;
   out->using_qmatrix = rav1_bits_read(&bits, 1);
   if (out->using_qmatrix)
   {
      out->qm_y = rav1_bits_read(&bits, 4);
      out->qm_u = rav1_bits_read(&bits, 4);
      out->qm_v = out->qm_u;
      if (seq->separate_uv_delta_q)
         out->qm_v = rav1_bits_read(&bits, 4);
   }
   out->segmentation_enabled = rav1_bits_read(&bits, 1);
   if (out->segmentation_enabled)
   {
      if (frame->primary_ref_frame == 7)
         out->segmentation_update_map = out->segmentation_update_data = 1;
      else
      {
         out->segmentation_update_map = rav1_bits_read(&bits, 1);
         if (out->segmentation_update_map)
            out->segmentation_temporal_update = rav1_bits_read(&bits, 1);
         out->segmentation_update_data = rav1_bits_read(&bits, 1);
      }
      if (out->segmentation_update_data)
         for (i = 0; i < 8; i++)
         {
            out->reference.segment_enabled[i] = 0;
            for (j = 0; j < 8; j++)
            {
               value = 0;
               if (rav1_bits_read(&bits, 1))
               {
                  out->reference.segment_enabled[i] |= 1u << j;
                  if (j < 5)
                     value = rav1_bits_delta(&bits, feature_bits[j] + 1, false);
                  else
                     value = (int)rav1_bits_read(&bits, feature_bits[j]);
                  if (value < -feature_max[j])
                     value = -feature_max[j];
                  if (value > feature_max[j])
                     value = feature_max[j];
               }
               out->reference.segment_data[i][j] = value;
            }
         }
   }
   else
   {
      memset(out->reference.segment_enabled, 0,
             sizeof(out->reference.segment_enabled));
      memset(out->reference.segment_data, 0,
             sizeof(out->reference.segment_data));
   }
   for (i = 0; i < 8; i++)
   {
      if (out->reference.segment_enabled[i])
         out->last_active_segment = i;
      if (out->reference.segment_enabled[i] & 0xe0)
         out->segment_id_pre_skip = 1;
   }
   if (out->base_q_idx)
      out->delta_q_present = rav1_bits_read(&bits, 1);
   if (out->delta_q_present)
   {
      out->delta_q_res = rav1_bits_read(&bits, 2);
      if (!allow_intrabc)
         out->delta_lf_present = rav1_bits_read(&bits, 1);
      if (out->delta_lf_present)
      {
         out->delta_lf_res = rav1_bits_read(&bits, 2);
         out->delta_lf_multi = rav1_bits_read(&bits, 1);
      }
   }
   for (i = 0; i < 8; i++)
   {
      qindex = (int)out->base_q_idx;
      if (out->reference.segment_enabled[i] & 1)
         qindex += out->reference.segment_data[i][0];
      if (qindex < 0)
         qindex = 0;
      if (qindex > 255)
         qindex = 255;
      if (!qindex && !out->delta_q_y_dc && !out->delta_q_u_dc &&
          !out->delta_q_u_ac && !out->delta_q_v_dc && !out->delta_q_v_ac)
         out->lossless_mask |= 1u << i;
   }
   out->coded_lossless = out->lossless_mask == 255;
   out->all_lossless =
       out->coded_lossless && geometry->width == geometry->upscaled_width;
   if (out->coded_lossless || allow_intrabc)
   {
      for (i = 0; i < 8; i++)
         out->reference.loop_filter_ref_deltas[i] = default_deltas[i];
      out->reference.loop_filter_mode_deltas[0] =
          out->reference.loop_filter_mode_deltas[1] = 0;
   }
   else
   {
      out->loop_filter_level[0] = rav1_bits_read(&bits, 6);
      out->loop_filter_level[1] = rav1_bits_read(&bits, 6);
      if (!seq->monochrome &&
          (out->loop_filter_level[0] || out->loop_filter_level[1]))
      {
         out->loop_filter_level[2] = rav1_bits_read(&bits, 6);
         out->loop_filter_level[3] = rav1_bits_read(&bits, 6);
      }
      out->loop_filter_sharpness = rav1_bits_read(&bits, 3);
      out->loop_filter_delta_enabled = rav1_bits_read(&bits, 1);
      if (out->loop_filter_delta_enabled)
      {
         out->loop_filter_delta_update = rav1_bits_read(&bits, 1);
         if (out->loop_filter_delta_update)
         {
            for (i = 0; i < 8; i++)
               if (rav1_bits_read(&bits, 1))
               {
                  out->loop_filter_update_ref_delta |= 1u << i;
                  out->reference.loop_filter_ref_deltas[i] =
                      rav1_bits_delta(&bits, 7, false);
               }
            for (i = 0; i < 2; i++)
               if (rav1_bits_read(&bits, 1))
               {
                  out->loop_filter_update_mode_delta |= 1u << i;
                  out->reference.loop_filter_mode_deltas[i] =
                      rav1_bits_delta(&bits, 7, false);
               }
         }
      }
   }
   out->cdef_damping = 3;
   if (!out->coded_lossless && !allow_intrabc && seq->enable_cdef)
   {
      out->cdef_damping = rav1_bits_read(&bits, 2) + 3;
      out->cdef_bits = rav1_bits_read(&bits, 2);
      for (i = 0; i < (1u << out->cdef_bits); i++)
      {
         out->cdef_y_pri[i] = rav1_bits_read(&bits, 4);
         out->cdef_y_sec[i] = rav1_bits_read(&bits, 2);
         if (out->cdef_y_sec[i] == 3)
            out->cdef_y_sec[i] = 4;
         if (!seq->monochrome)
         {
            out->cdef_uv_pri[i] = rav1_bits_read(&bits, 4);
            out->cdef_uv_sec[i] = rav1_bits_read(&bits, 2);
            if (out->cdef_uv_sec[i] == 3)
               out->cdef_uv_sec[i] = 4;
         }
      }
   }
   if (!out->all_lossless && !allow_intrabc && seq->enable_restoration)
   {
      for (i = 0; i < (seq->monochrome ? 1u : 3u); i++)
      {
         out->restoration_type[i] = restoration_map[rav1_bits_read(&bits, 2)];
         if (out->restoration_type[i] != RAV1_RESTORE_NONE)
         {
            uses_lr = 1;
            if (i)
               chroma_lr = 1;
         }
      }
      if (uses_lr)
      {
         shift = rav1_bits_read(&bits, 1);
         if (seq->use_128x128_superblock)
            shift++;
         else if (shift)
            shift += rav1_bits_read(&bits, 1);
         uv_shift = 0;
         if (seq->subsampling_x && seq->subsampling_y && chroma_lr)
            uv_shift = rav1_bits_read(&bits, 1);
         out->restoration_size[0] = 64u << shift;
         out->restoration_size[1] = out->restoration_size[2] =
             out->restoration_size[0] >> uv_shift;
      }
   }
   if (!out->coded_lossless)
      out->tx_mode = rav1_bits_read(&bits, 1) ? 2 : 1;
   if (!bits.failed)
   {
      *input = bits;
      return true;
   }
   memset(out, 0, sizeof(*out));
   return false;
}

#endif
