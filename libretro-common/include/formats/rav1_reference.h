/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_REFERENCE_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_REFERENCE_H__

#include <formats/rav1_frame.h>

typedef struct rav1_reference
{
   rav1_frame_size_t size;
   unsigned valid, order_hint, frame_id;
} rav1_reference_t;

typedef struct rav1_frame_setup
{
   rav1_frame_size_t size;
   unsigned ref_frame_idx[7], sign_bias[7], valid_mask;
   unsigned short_signaling, last_frame_idx, gold_frame_idx;
   unsigned allow_intrabc, allow_high_precision_mv, interpolation_filter;
   unsigned motion_mode_switchable, use_ref_frame_mvs;
   unsigned disable_frame_end_update_cdf;
} rav1_frame_setup_t;

/* Order hints wrap modulo 2^width; width is validated by the caller. */
static INLINE int rav1_relative_dist(unsigned a, unsigned b, unsigned width)
{
   unsigned mask = 1u << (width - 1);
   unsigned diff = a - b;
   return (int)(diff & (mask - 1)) - (int)(diff & mask);
}

/* Resolve short signaling, including the specified tie breaking. */
static INLINE bool rav1_reference_select(unsigned indices[7],
                                         const rav1_reference_t refs[8],
                                         unsigned current, unsigned width,
                                         unsigned last, unsigned gold)
{
   static const unsigned order[5] = {1, 2, 4, 5, 6};
   int hints[8], best, selected, earliest;
   unsigned used;
   unsigned i, j, target;
   if (!indices || !refs || !width || width > 8 || last > 7 || gold > 7)
      return false;
   used = (1u << last) | (1u << gold);
   for (i = 0; i < 8; i++)
      hints[i] = rav1_relative_dist(refs[i].order_hint, current, width);
   if (hints[last] >= 0 || hints[gold] >= 0)
      return false;
   for (i = 0; i < 7; i++)
      indices[i] = 8;
   indices[0] = last;
   indices[3] = gold;
   for (j = 0; j < 3; j++)
   {
      selected = -1;
      best = 0;
      for (i = 0; i < 8; i++)
         if (!(used & (1u << i)) && hints[i] >= 0 &&
             (selected < 0 || (j == 0 ? hints[i] >= best : hints[i] < best)))
         {
            selected = (int)i;
            best = hints[i];
         }
      target = j == 0 ? 6 : j == 1 ? 4 : 5;
      if (selected >= 0)
      {
         indices[target] = (unsigned)selected;
         used |= 1u << selected;
      }
   }
   for (j = 0; j < 5; j++)
   {
      target = order[j];
      if (indices[target] != 8)
         continue;
      selected = -1;
      best = 0;
      for (i = 0; i < 8; i++)
         if (!(used & (1u << i)) && hints[i] < 0 &&
             (selected < 0 || hints[i] >= best))
         {
            selected = (int)i;
            best = hints[i];
         }
      if (selected >= 0)
      {
         indices[target] = (unsigned)selected;
         used |= 1u << selected;
      }
   }
   earliest = 0;
   for (i = 1; i < 8; i++)
      if (hints[i] < hints[earliest])
         earliest = (int)i;
   for (i = 0; i < 7; i++)
      if (indices[i] == 8)
         indices[i] = (unsigned)earliest;
   return true;
}

/* Continue a validated coded prefix through disable_frame_end_update_cdf.
 * Slots are immutable caller metadata from successfully decoded frames.
 * valid_mask reports tentative invalidations; commit them only after the full
 * frame succeeds. This does not validate previous/current frame-ID progression,
 * show-existing frames, reference scaling limits or sequence transitions. */
static INLINE bool rav1_frame_setup_parse(rav1_frame_setup_t *out,
                                          rav1_bits_t *input,
                                          const rav1_sequence_t *seq,
                                          const rav1_frame_prefix_t *frame,
                                          const rav1_reference_t refs[8])
{
   rav1_bits_t bits;
   unsigned i, index, delta, modulus, distance, found = 0;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!input || !seq || !frame || input->failed || !input->data ||
       input->byte > input->size || input->bit > 7 || frame->show_existing ||
       frame->frame_type > RAV1_FRAME_SWITCH ||
       (seq->enable_order_hint &&
        (!seq->order_hint_bits || seq->order_hint_bits > 8)) ||
       (seq->frame_id_numbers_present &&
        (!seq->delta_frame_id_length || seq->delta_frame_id_length > 15 ||
         seq->frame_id_length <= seq->delta_frame_id_length ||
         seq->frame_id_length > 16)) ||
       (!frame->frame_is_intra && !refs))
      return false;
   bits = *input;
   if (refs && !(frame->frame_type == RAV1_FRAME_KEY && frame->show_frame))
      for (i = 0; i < 8; i++)
      {
         if (!refs[i].valid)
            continue;
         if (frame->ref_order_hints_present &&
             refs[i].order_hint != frame->ref_order_hint[i])
            continue;
         if (seq->frame_id_numbers_present)
         {
            modulus = 1u << seq->frame_id_length;
            distance =
                (frame->current_frame_id - refs[i].frame_id) & (modulus - 1);
            if (refs[i].frame_id >= modulus ||
                distance > (1u << seq->delta_frame_id_length))
               continue;
         }
         out->valid_mask |= 1u << i;
      }
   if (!frame->frame_is_intra)
   {
      if (seq->enable_order_hint)
         out->short_signaling = rav1_bits_read(&bits, 1);
      if (out->short_signaling)
      {
         out->last_frame_idx = rav1_bits_read(&bits, 3);
         out->gold_frame_idx = rav1_bits_read(&bits, 3);
         if (!rav1_reference_select(out->ref_frame_idx, refs, frame->order_hint,
                                    seq->order_hint_bits, out->last_frame_idx,
                                    out->gold_frame_idx))
            goto fail;
      }
      for (i = 0; i < 7; i++)
      {
         if (!out->short_signaling)
            out->ref_frame_idx[i] = rav1_bits_read(&bits, 3);
         index = out->ref_frame_idx[i];
         if (!(out->valid_mask & (1u << index)))
            goto fail;
         if (seq->frame_id_numbers_present)
         {
            delta = rav1_bits_read(&bits, seq->delta_frame_id_length) + 1;
            if (((frame->current_frame_id - delta) &
                 ((1u << seq->frame_id_length) - 1)) != refs[index].frame_id)
               goto fail;
         }
         if (seq->enable_order_hint)
            out->sign_bias[i] =
                rav1_relative_dist(refs[index].order_hint, frame->order_hint,
                                   seq->order_hint_bits) > 0;
      }
      if (frame->size_override && !frame->error_resilient)
         for (i = 0; i < 7; i++)
            if (rav1_bits_read(&bits, 1))
            {
               out->size = refs[out->ref_frame_idx[i]].size;
               found = 1;
               break;
            }
   }
   if (found)
   {
      if (!out->size.upscaled_width ||
          out->size.upscaled_width > seq->max_width || !out->size.height ||
          out->size.height > seq->max_height || !out->size.render_width ||
          out->size.render_width > 65536 || !out->size.render_height ||
          out->size.render_height > 65536 || seq->max_width > 65536 ||
          seq->max_height > 65536)
         goto fail;
      out->size.superres_denom = 8;
      if (seq->enable_superres && rav1_bits_read(&bits, 1))
         out->size.superres_denom = rav1_bits_read(&bits, 3) + 9;
      out->size.width =
          (out->size.upscaled_width * 8 + out->size.superres_denom / 2) /
          out->size.superres_denom;
      out->size.mi_cols = 2 * ((out->size.width + 7) >> 3);
      out->size.mi_rows = 2 * ((out->size.height + 7) >> 3);
   }
   else if (!rav1_frame_size_parse(&out->size, &bits, seq,
                                   frame->size_override != 0))
      goto fail;
   if (frame->frame_is_intra)
   {
      if (frame->allow_screen_content_tools &&
          out->size.width == out->size.upscaled_width)
         out->allow_intrabc = rav1_bits_read(&bits, 1);
   }
   else
   {
      if (!frame->force_integer_mv)
         out->allow_high_precision_mv = rav1_bits_read(&bits, 1);
      out->interpolation_filter = 4;
      if (!rav1_bits_read(&bits, 1))
         out->interpolation_filter = rav1_bits_read(&bits, 2);
      out->motion_mode_switchable = rav1_bits_read(&bits, 1);
      if (!frame->error_resilient && seq->enable_ref_frame_mvs)
         out->use_ref_frame_mvs = rav1_bits_read(&bits, 1);
   }
   out->disable_frame_end_update_cdf = 1;
   if (!seq->reduced_still_picture_header && !frame->disable_cdf_update)
      out->disable_frame_end_update_cdf = rav1_bits_read(&bits, 1);
   if (!bits.failed)
   {
      *input = bits;
      return true;
   }
fail:
   memset(out, 0, sizeof(*out));
   return false;
}

#endif
