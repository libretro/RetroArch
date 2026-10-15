/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_SEQUENCE_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_SEQUENCE_H__

#include <formats/rav1.h>

typedef struct rav1_operating_point
{
   uint32_t decoder_buffer_delay, encoder_buffer_delay;
   unsigned idc, level, tier, decoder_model_present;
   unsigned low_delay_mode, initial_display_delay;
} rav1_operating_point_t;

typedef struct rav1_sequence
{
   rav1_operating_point_t operating_points[32];
   uint32_t num_units_in_display_tick, time_scale;
   uint32_t num_ticks_per_picture_minus_one, num_units_in_decoding_tick;
   unsigned profile, still_picture, reduced_still_picture_header;
   unsigned timing_info_present, equal_picture_interval;
   unsigned decoder_model_info_present, initial_display_delay_present;
   unsigned buffer_delay_length, buffer_removal_time_length;
   unsigned frame_presentation_time_length, operating_point_count;
   unsigned frame_width_bits, frame_height_bits, max_width, max_height;
   unsigned frame_id_numbers_present, delta_frame_id_length, frame_id_length;
   unsigned use_128x128_superblock, enable_filter_intra,
       enable_intra_edge_filter;
   unsigned enable_interintra_compound, enable_masked_compound;
   unsigned enable_warped_motion, enable_dual_filter, enable_order_hint;
   unsigned enable_jnt_comp, enable_ref_frame_mvs;
   unsigned force_screen_content_tools, force_integer_mv, order_hint_bits;
   unsigned enable_superres, enable_cdef, enable_restoration;
   unsigned bit_depth, monochrome, color_description_present;
   unsigned color_primaries, transfer_characteristics, matrix_coefficients;
   unsigned color_range, subsampling_x, subsampling_y, chroma_sample_position;
   unsigned separate_uv_delta_q, film_grain_params_present;
} rav1_sequence_t;

typedef struct rav1_bits
{
   const uint8_t *data;
   size_t size, byte;
   unsigned bit, failed;
} rav1_bits_t;

static INLINE uint32_t rav1_bits_read(rav1_bits_t *bits, unsigned count)
{
   uint32_t value = 0;
   unsigned i;
   if (bits->failed || count > 32)
   {
      bits->failed = 1;
      return 0;
   }
   for (i = 0; i < count; i++)
   {
      if (bits->byte == bits->size)
      {
         bits->failed = 1;
         return 0;
      }
      value = (value << 1) | ((bits->data[bits->byte] >> (7 - bits->bit)) & 1);
      if (++bits->bit == 8)
      {
         bits->bit = 0;
         bits->byte++;
      }
   }
   return value;
}

/* The timing UVLC excludes UINT32_MAX. */
static INLINE uint32_t rav1_bits_uvlc(rav1_bits_t *bits)
{
   unsigned zeros = 0;
   while (!rav1_bits_read(bits, 1))
   {
      if (bits->failed || ++zeros == 32)
      {
         bits->failed = 1;
         return 0;
      }
   }
   return ((uint32_t)1 << zeros) - 1 + rav1_bits_read(bits, zeros);
}

/* Payload only, including trailing_bits(). Failure clears the caller's output.
 * Parse directly into caller storage to keep the stack bounded. */
static INLINE bool rav1_sequence_parse(rav1_sequence_t *out,
                                       const uint8_t *data, size_t size)
{
   rav1_bits_t bits;
   unsigned i, j, high;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!data || !size)
      return false;
   bits.data = data;
   bits.size = size;
   bits.byte = 0;
   bits.bit = bits.failed = 0;
   out->profile = rav1_bits_read(&bits, 3);
   out->still_picture = rav1_bits_read(&bits, 1);
   out->reduced_still_picture_header = rav1_bits_read(&bits, 1);
   if (out->profile > 2 ||
       (out->reduced_still_picture_header && !out->still_picture))
      goto fail;
   out->operating_point_count = 1;
   if (out->reduced_still_picture_header)
      out->operating_points[0].level = rav1_bits_read(&bits, 5);
   else
   {
      out->timing_info_present = rav1_bits_read(&bits, 1);
      if (out->timing_info_present)
      {
         out->num_units_in_display_tick = rav1_bits_read(&bits, 32);
         out->time_scale = rav1_bits_read(&bits, 32);
         out->equal_picture_interval = rav1_bits_read(&bits, 1);
         if (out->equal_picture_interval)
            out->num_ticks_per_picture_minus_one = rav1_bits_uvlc(&bits);
         out->decoder_model_info_present = rav1_bits_read(&bits, 1);
         if (!out->num_units_in_display_tick || !out->time_scale)
            goto fail;
         if (out->decoder_model_info_present)
         {
            out->buffer_delay_length = rav1_bits_read(&bits, 5) + 1;
            out->num_units_in_decoding_tick = rav1_bits_read(&bits, 32);
            out->buffer_removal_time_length = rav1_bits_read(&bits, 5) + 1;
            out->frame_presentation_time_length = rav1_bits_read(&bits, 5) + 1;
            if (!out->num_units_in_decoding_tick)
               goto fail;
         }
      }
      out->initial_display_delay_present = rav1_bits_read(&bits, 1);
      out->operating_point_count = rav1_bits_read(&bits, 5) + 1;
      for (i = 0; i < out->operating_point_count; i++)
      {
         rav1_operating_point_t *op = &out->operating_points[i];
         op->idc = rav1_bits_read(&bits, 12);
         op->level = rav1_bits_read(&bits, 5);
         if (op->level > 7)
            op->tier = rav1_bits_read(&bits, 1);
         for (j = 0; j < i; j++)
            if (op->idc == out->operating_points[j].idc)
               goto fail;
         if (out->decoder_model_info_present)
         {
            op->decoder_model_present = rav1_bits_read(&bits, 1);
            if (op->decoder_model_present)
            {
               op->decoder_buffer_delay =
                   rav1_bits_read(&bits, out->buffer_delay_length);
               op->encoder_buffer_delay =
                   rav1_bits_read(&bits, out->buffer_delay_length);
               op->low_delay_mode = rav1_bits_read(&bits, 1);
            }
         }
         if (out->initial_display_delay_present && rav1_bits_read(&bits, 1))
            op->initial_display_delay = rav1_bits_read(&bits, 4) + 1;
      }
   }
   for (i = 0; i < out->operating_point_count; i++)
      if (out->operating_points[i].level > 23 &&
          out->operating_points[i].level != 31)
         goto fail;
   out->frame_width_bits = rav1_bits_read(&bits, 4) + 1;
   out->frame_height_bits = rav1_bits_read(&bits, 4) + 1;
   out->max_width = rav1_bits_read(&bits, out->frame_width_bits) + 1;
   out->max_height = rav1_bits_read(&bits, out->frame_height_bits) + 1;
   if (!out->reduced_still_picture_header)
      out->frame_id_numbers_present = rav1_bits_read(&bits, 1);
   if (out->frame_id_numbers_present)
   {
      out->delta_frame_id_length = rav1_bits_read(&bits, 4) + 2;
      out->frame_id_length =
          rav1_bits_read(&bits, 3) + 1 + out->delta_frame_id_length;
      if (out->frame_id_length > 16)
         goto fail;
   }
   out->use_128x128_superblock = rav1_bits_read(&bits, 1);
   out->enable_filter_intra = rav1_bits_read(&bits, 1);
   out->enable_intra_edge_filter = rav1_bits_read(&bits, 1);
   out->force_screen_content_tools = out->force_integer_mv = 2;
   if (!out->reduced_still_picture_header)
   {
      out->enable_interintra_compound = rav1_bits_read(&bits, 1);
      out->enable_masked_compound = rav1_bits_read(&bits, 1);
      out->enable_warped_motion = rav1_bits_read(&bits, 1);
      out->enable_dual_filter = rav1_bits_read(&bits, 1);
      out->enable_order_hint = rav1_bits_read(&bits, 1);
      if (out->enable_order_hint)
      {
         out->enable_jnt_comp = rav1_bits_read(&bits, 1);
         out->enable_ref_frame_mvs = rav1_bits_read(&bits, 1);
      }
      if (!rav1_bits_read(&bits, 1))
         out->force_screen_content_tools = rav1_bits_read(&bits, 1);
      if (out->force_screen_content_tools && !rav1_bits_read(&bits, 1))
         out->force_integer_mv = rav1_bits_read(&bits, 1);
      if (out->enable_order_hint)
         out->order_hint_bits = rav1_bits_read(&bits, 3) + 1;
   }
   out->enable_superres = rav1_bits_read(&bits, 1);
   out->enable_cdef = rav1_bits_read(&bits, 1);
   out->enable_restoration = rav1_bits_read(&bits, 1);
   high = rav1_bits_read(&bits, 1);
   out->bit_depth = high ? 10 : 8;
   if (out->profile == 2 && high && rav1_bits_read(&bits, 1))
      out->bit_depth = 12;
   if (out->profile != 1)
      out->monochrome = rav1_bits_read(&bits, 1);
   out->color_description_present = rav1_bits_read(&bits, 1);
   out->color_primaries = out->transfer_characteristics =
       out->matrix_coefficients = 2;
   if (out->color_description_present)
   {
      out->color_primaries = rav1_bits_read(&bits, 8);
      out->transfer_characteristics = rav1_bits_read(&bits, 8);
      out->matrix_coefficients = rav1_bits_read(&bits, 8);
   }
   if (out->monochrome)
   {
      out->color_range = rav1_bits_read(&bits, 1);
      out->subsampling_x = out->subsampling_y = 1;
   }
   else
   {
      if (out->color_primaries == 1 && out->transfer_characteristics == 13 &&
          out->matrix_coefficients == 0)
         out->color_range = 1;
      else
      {
         out->color_range = rav1_bits_read(&bits, 1);
         if (out->profile == 0)
            out->subsampling_x = out->subsampling_y = 1;
         else if (out->profile == 2)
         {
            out->subsampling_x = 1;
            if (out->bit_depth == 12)
               out->subsampling_x = rav1_bits_read(&bits, 1);
            if (out->bit_depth == 12 && out->subsampling_x)
               out->subsampling_y = rav1_bits_read(&bits, 1);
         }
         if (out->subsampling_x && out->subsampling_y)
            out->chroma_sample_position = rav1_bits_read(&bits, 2);
      }
      out->separate_uv_delta_q = rav1_bits_read(&bits, 1);
   }
   if (out->chroma_sample_position == 3 ||
       (out->profile == 0 && (!out->subsampling_x || !out->subsampling_y)) ||
       (out->matrix_coefficients == 0 &&
        (out->subsampling_x || out->subsampling_y)) ||
       (out->profile == 2 && out->bit_depth != 12 && !out->monochrome &&
        (!out->subsampling_x || out->subsampling_y)))
      goto fail;
   out->film_grain_params_present = rav1_bits_read(&bits, 1);
   if (!rav1_bits_read(&bits, 1))
      goto fail;
   while (bits.byte < bits.size && !bits.failed)
      if (rav1_bits_read(&bits, 1))
         goto fail;
   if (!bits.failed)
      return true;
fail:
   memset(out, 0, sizeof(*out));
   return false;
}

/* Require the first operating point and color layout to agree with av1C. */
static INLINE bool rav1_config_sequence(rav1_sequence_t *out,
                                        const rav1_config_t *config)
{
   if (!out)
      return false;
   if (config && config->sequence.type == 1 &&
       rav1_sequence_parse(out, config->sequence.payload,
                           config->sequence.payload_size) &&
       out->profile == config->profile &&
       out->operating_points[0].level == config->level &&
       out->operating_points[0].tier == config->tier &&
       out->bit_depth == config->bit_depth &&
       out->monochrome == config->monochrome &&
       out->subsampling_x == config->subsampling_x &&
       out->subsampling_y == config->subsampling_y &&
       out->chroma_sample_position == config->chroma_sample_position)
      return true;
   memset(out, 0, sizeof(*out));
   return false;
}

#endif
