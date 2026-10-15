/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_GRAIN_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_GRAIN_H__

#include <formats/rav1_reference.h>

typedef struct rav1_grain
{
   unsigned apply, seed, chroma_scaling_from_luma;
   unsigned scaling_shift, ar_coeff_lag, ar_coeff_shift, scale_shift;
   unsigned chroma_mult[2], chroma_luma_mult[2], chroma_offset[2];
   unsigned overlap, clip_to_restricted_range;
   uint8_t num_points[3], point_value[3][14], point_scaling[3][14];
   int8_t ar_coeffs[3][25];
} rav1_grain_t;

typedef struct rav1_grain_header
{
   rav1_grain_t params;
   unsigned update, reference_idx;
} rav1_grain_header_t;

/* Parse film_grain_params() for a validated coded frame. Reference parameters
 * must come from successfully decoded frames in this sequence. They remain
 * immutable; aliases are rejected without clearing them. Other failures clear
 * output and preserve the cursor. This extracts metadata, not grain pixels. */
static INLINE bool rav1_grain_parse(rav1_grain_header_t *out,
                                    rav1_bits_t *input,
                                    const rav1_sequence_t *seq,
                                    const rav1_frame_prefix_t *frame,
                                    const rav1_frame_setup_t *setup,
                                    const rav1_grain_t *const references[8])
{
   rav1_bits_t bits;
   unsigned i, plane, count, seed, selected = 0;
   if (!out)
      return false;
   if (references)
      for (i = 0; i < 8; i++)
         if (references[i] == &out->params)
            return false;
   memset(out, 0, sizeof(*out));
   if (!input || !seq || !frame || input->failed || !input->data ||
       input->byte > input->size || input->bit > 7 || frame->show_existing ||
       frame->frame_type > RAV1_FRAME_SWITCH)
      return false;
   bits = *input;
   if (!seq->film_grain_params_present ||
       (!frame->show_frame && !frame->showable_frame))
      return true;
   out->params.apply = rav1_bits_read(&bits, 1);
   if (!out->params.apply)
      goto done;
   out->params.seed = rav1_bits_read(&bits, 16);
   out->update = 1;
   if (frame->frame_type == RAV1_FRAME_INTER)
      out->update = rav1_bits_read(&bits, 1);
   if (!out->update)
   {
      out->reference_idx = rav1_bits_read(&bits, 3);
      if (bits.failed || !setup || !references ||
          !references[out->reference_idx] ||
          !(setup->valid_mask & (1u << out->reference_idx)))
         goto fail;
      for (i = 0; i < 7; i++)
         if (setup->ref_frame_idx[i] == out->reference_idx)
            selected = 1;
      if (!selected)
         goto fail;
      seed = out->params.seed;
      out->params = *references[out->reference_idx];
      out->params.seed = seed;
      goto done;
   }
   out->params.num_points[0] = (uint8_t)rav1_bits_read(&bits, 4);
   for (plane = 0; plane < 3; plane++)
   {
      if (plane == 1)
      {
         if (!seq->monochrome)
            out->params.chroma_scaling_from_luma = rav1_bits_read(&bits, 1);
         if (seq->monochrome || out->params.chroma_scaling_from_luma ||
             (seq->subsampling_x && seq->subsampling_y &&
              !out->params.num_points[0]))
            break;
      }
      if (plane)
         out->params.num_points[plane] = (uint8_t)rav1_bits_read(&bits, 4);
      count = out->params.num_points[plane];
      if (count > (plane ? 10u : 14u))
         goto fail;
      for (i = 0; i < count; i++)
      {
         out->params.point_value[plane][i] = (uint8_t)rav1_bits_read(&bits, 8);
         out->params.point_scaling[plane][i] =
             (uint8_t)rav1_bits_read(&bits, 8);
         if (i && out->params.point_value[plane][i] <=
                      out->params.point_value[plane][i - 1])
            goto fail;
      }
   }
   if (seq->subsampling_x && seq->subsampling_y &&
       (!out->params.num_points[1] != !out->params.num_points[2]))
      goto fail;
   out->params.scaling_shift = rav1_bits_read(&bits, 2) + 8;
   out->params.ar_coeff_lag = rav1_bits_read(&bits, 2);
   count = 2 * out->params.ar_coeff_lag * (out->params.ar_coeff_lag + 1);
   for (plane = 0; plane < 3; plane++)
   {
      unsigned coefficients = count + (plane && out->params.num_points[0]);
      if (out->params.num_points[plane] ||
          (plane && out->params.chroma_scaling_from_luma))
         for (i = 0; i < coefficients; i++)
            out->params.ar_coeffs[plane][i] =
                (int8_t)((int)rav1_bits_read(&bits, 8) - 128);
   }
   out->params.ar_coeff_shift = rav1_bits_read(&bits, 2) + 6;
   out->params.scale_shift = rav1_bits_read(&bits, 2);
   for (plane = 0; plane < 2; plane++)
      if (out->params.num_points[plane + 1])
      {
         out->params.chroma_mult[plane] = rav1_bits_read(&bits, 8);
         out->params.chroma_luma_mult[plane] = rav1_bits_read(&bits, 8);
         out->params.chroma_offset[plane] = rav1_bits_read(&bits, 9);
      }
   out->params.overlap = rav1_bits_read(&bits, 1);
   out->params.clip_to_restricted_range = rav1_bits_read(&bits, 1);
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

#endif
