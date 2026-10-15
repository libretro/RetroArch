/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_FRAME_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_FRAME_H__

#include <formats/rav1_sequence.h>

enum rav1_frame_type
{
   RAV1_FRAME_KEY,
   RAV1_FRAME_INTER,
   RAV1_FRAME_INTRA_ONLY,
   RAV1_FRAME_SWITCH,
   RAV1_FRAME_UNRESOLVED
};

typedef struct rav1_frame_prefix
{
   uint32_t presentation_time, removal_time[32], removal_time_mask;
   unsigned show_existing, frame_to_show, display_frame_id;
   unsigned frame_type, frame_is_intra, show_frame, showable_frame;
   unsigned error_resilient, disable_cdf_update;
   unsigned allow_screen_content_tools, force_integer_mv;
   unsigned current_frame_id, size_override, order_hint, primary_ref_frame;
   unsigned presentation_time_present, removal_time_present,
       refresh_frame_flags;
   unsigned ref_order_hints_present, ref_order_hint[8];
} rav1_frame_prefix_t;

typedef struct rav1_frame_size
{
   unsigned width, height, upscaled_width, render_width, render_height;
   unsigned superres_denom, mi_cols, mi_rows;
} rav1_frame_size_t;

/* Parse through ref_order_hint[], without changing reference state. The
 * sequence must have passed rav1_sequence_parse(). A show-existing prefix
 * leaves frame type unresolved: the caller must validate and load the selected
 * reference. This does not validate the rest of the frame or its OBU trailing
 * bits. */
static INLINE bool rav1_frame_prefix_parse(rav1_frame_prefix_t *out,
                                           rav1_bits_t *input,
                                           const rav1_sequence_t *seq,
                                           unsigned temporal_id,
                                           unsigned spatial_id, bool frame_obu)
{
   rav1_bits_t bits;
   unsigned i, hint_bits;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!input || !seq || input->failed || !input->data ||
       input->byte > input->size || input->bit > 7 || temporal_id > 7 ||
       spatial_id > 3 || !seq->operating_point_count ||
       seq->operating_point_count > 32 || seq->force_screen_content_tools > 2 ||
       seq->force_integer_mv > 2 ||
       (seq->frame_id_numbers_present &&
        (!seq->frame_id_length || seq->frame_id_length > 16)) ||
       (seq->enable_order_hint &&
        (!seq->order_hint_bits || seq->order_hint_bits > 8)) ||
       (seq->decoder_model_info_present &&
        (!seq->frame_presentation_time_length ||
         seq->frame_presentation_time_length > 32 ||
         !seq->buffer_removal_time_length ||
         seq->buffer_removal_time_length > 32)))
      return false;
   bits = *input;
   hint_bits = seq->enable_order_hint ? seq->order_hint_bits : 0;
   if (seq->reduced_still_picture_header)
   {
      out->frame_type = RAV1_FRAME_KEY;
      out->frame_is_intra = out->show_frame = 1;
   }
   else
   {
      out->show_existing = rav1_bits_read(&bits, 1);
      if (out->show_existing)
      {
         if (frame_obu || seq->still_picture)
            goto fail;
         out->frame_to_show = rav1_bits_read(&bits, 3);
         out->frame_type = RAV1_FRAME_UNRESOLVED;
      }
      else
      {
         out->frame_type = rav1_bits_read(&bits, 2);
         out->frame_is_intra = out->frame_type == RAV1_FRAME_KEY ||
                               out->frame_type == RAV1_FRAME_INTRA_ONLY;
         out->show_frame = rav1_bits_read(&bits, 1);
      }
      if ((out->show_existing || out->show_frame) &&
          seq->decoder_model_info_present && !seq->equal_picture_interval)
      {
         out->presentation_time_present = 1;
         out->presentation_time =
             rav1_bits_read(&bits, seq->frame_presentation_time_length);
      }
      if (out->show_existing)
      {
         if (seq->frame_id_numbers_present)
            out->display_frame_id = rav1_bits_read(&bits, seq->frame_id_length);
         goto done;
      }
      if (out->show_frame)
         out->showable_frame = out->frame_type != RAV1_FRAME_KEY;
      else
         out->showable_frame = rav1_bits_read(&bits, 1);
      if (seq->still_picture &&
          (out->frame_type != RAV1_FRAME_KEY || !out->show_frame))
         goto fail;
      if (out->frame_type == RAV1_FRAME_SWITCH ||
          (out->frame_type == RAV1_FRAME_KEY && out->show_frame))
         out->error_resilient = 1;
      else
         out->error_resilient = rav1_bits_read(&bits, 1);
   }
   out->disable_cdf_update = rav1_bits_read(&bits, 1);
   out->allow_screen_content_tools = seq->force_screen_content_tools;
   if (out->allow_screen_content_tools == 2)
      out->allow_screen_content_tools = rav1_bits_read(&bits, 1);
   if (out->allow_screen_content_tools)
   {
      out->force_integer_mv = seq->force_integer_mv;
      if (out->force_integer_mv == 2)
         out->force_integer_mv = rav1_bits_read(&bits, 1);
   }
   if (out->frame_is_intra)
      out->force_integer_mv = 1;
   if (seq->frame_id_numbers_present)
      out->current_frame_id = rav1_bits_read(&bits, seq->frame_id_length);
   if (out->frame_type == RAV1_FRAME_SWITCH)
      out->size_override = 1;
   else if (!seq->reduced_still_picture_header)
      out->size_override = rav1_bits_read(&bits, 1);
   out->order_hint = rav1_bits_read(&bits, hint_bits);
   out->primary_ref_frame = 7;
   if (!out->frame_is_intra && !out->error_resilient)
      out->primary_ref_frame = rav1_bits_read(&bits, 3);
   if (seq->decoder_model_info_present)
   {
      out->removal_time_present = rav1_bits_read(&bits, 1);
      if (out->removal_time_present)
         for (i = 0; i < seq->operating_point_count; i++)
         {
            const rav1_operating_point_t *op = &seq->operating_points[i];
            if (op->decoder_model_present &&
                (!op->idc || (((op->idc >> temporal_id) & 1) &&
                              ((op->idc >> (spatial_id + 8)) & 1))))
            {
               out->removal_time_mask |= (uint32_t)1 << i;
               out->removal_time[i] =
                   rav1_bits_read(&bits, seq->buffer_removal_time_length);
            }
         }
   }
   if (out->frame_type == RAV1_FRAME_SWITCH ||
       (out->frame_type == RAV1_FRAME_KEY && out->show_frame))
      out->refresh_frame_flags = 255;
   else
      out->refresh_frame_flags = rav1_bits_read(&bits, 8);
   if ((!out->frame_is_intra || out->refresh_frame_flags != 255) &&
       out->error_resilient && seq->enable_order_hint)
   {
      out->ref_order_hints_present = 1;
      for (i = 0; i < 8; i++)
         out->ref_order_hint[i] = rav1_bits_read(&bits, hint_bits);
   }
done:
   if (!bits.failed)
   {
      *input = bits;
      return true;
   }
fail:
   memset(out, 0, sizeof(*out));
   return false;
}

/* frame_size() + render_size(), for the explicit/no-reference-size branch.
 * The caller must resolve frame_size_with_refs() separately. Both parsers
 * clear output and preserve the input cursor on failure. */
static INLINE bool rav1_frame_size_parse(rav1_frame_size_t *out,
                                         rav1_bits_t *input,
                                         const rav1_sequence_t *seq,
                                         bool size_override)
{
   rav1_bits_t bits;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!input || !seq || input->failed || !input->data ||
       input->byte > input->size || input->bit > 7 || !seq->frame_width_bits ||
       seq->frame_width_bits > 16 || !seq->frame_height_bits ||
       seq->frame_height_bits > 16 || !seq->max_width ||
       seq->max_width > 65536 || !seq->max_height || seq->max_height > 65536)
      return false;
   bits = *input;
   out->upscaled_width = seq->max_width;
   out->height = seq->max_height;
   if (size_override)
   {
      out->upscaled_width = rav1_bits_read(&bits, seq->frame_width_bits) + 1;
      out->height = rav1_bits_read(&bits, seq->frame_height_bits) + 1;
      if (out->upscaled_width > seq->max_width || out->height > seq->max_height)
         goto fail;
   }
   out->superres_denom = 8;
   if (seq->enable_superres && rav1_bits_read(&bits, 1))
      out->superres_denom = rav1_bits_read(&bits, 3) + 9;
   out->width = (out->upscaled_width * 8 + out->superres_denom / 2) /
                out->superres_denom;
   out->render_width = out->upscaled_width;
   out->render_height = out->height;
   if (rav1_bits_read(&bits, 1))
   {
      out->render_width = rav1_bits_read(&bits, 16) + 1;
      out->render_height = rav1_bits_read(&bits, 16) + 1;
   }
   out->mi_cols = 2 * ((out->width + 7) >> 3);
   out->mi_rows = 2 * ((out->height + 7) >> 3);
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
