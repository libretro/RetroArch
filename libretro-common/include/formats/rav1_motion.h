/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_MOTION_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_MOTION_H__

#include <formats/rav1_reference.h>
#include <formats/rav1_tile_info.h>

enum rav1_global_type
{
   RAV1_GLOBAL_IDENTITY,
   RAV1_GLOBAL_TRANSLATION,
   RAV1_GLOBAL_ROTZOOM,
   RAV1_GLOBAL_AFFINE
};

typedef struct rav1_global_motion
{
   int32_t params[7][6];
   unsigned type[7];
} rav1_global_motion_t;

typedef struct rav1_motion
{
   rav1_global_motion_t global;
   unsigned reference_select, skip_mode_allowed, skip_mode_present;
   unsigned skip_mode_frame[2], allow_warped_motion, reduced_tx_set;
} rav1_motion_t;

/* Bounded AV1 subexponential code with reference recentering. */
static INLINE unsigned rav1_bits_subexp_ref(rav1_bits_t *bits, unsigned n,
                                            unsigned reference)
{
   unsigned i = 0, base = 0, width, step, value, center, result;
   if (!n || n > 65536 || reference >= n)
   {
      bits->failed = 1;
      return 0;
   }
   for (;;)
   {
      width = i ? i + 2 : 3;
      step = 1u << width;
      if (n <= base + 3 * step)
      {
         value = base + rav1_bits_ns(bits, n - base);
         break;
      }
      if (!rav1_bits_read(bits, 1))
      {
         value = base + rav1_bits_read(bits, width);
         break;
      }
      base += step;
      i++;
   }
   center = reference * 2 <= n ? reference : n - 1 - reference;
   if (value > 2 * center)
      result = value;
   else if (value & 1)
      result = center - ((value + 1) >> 1);
   else
      result = center + (value >> 1);
   return reference * 2 <= n ? result : n - 1 - result;
}

/* Starts after TX mode and stops before film_grain_params(). Previous global
 * parameters come from the primary reference; NONE uses identity. They must
 * not alias output. Failures preserve the cursor and clear distinct output.
 * This parses model coefficients, not per-block motion or warp validity. */
static INLINE bool rav1_motion_parse(rav1_motion_t *out, rav1_bits_t *input,
                                     const rav1_sequence_t *seq,
                                     const rav1_frame_prefix_t *frame,
                                     const rav1_frame_setup_t *setup,
                                     const rav1_reference_t refs[8],
                                     const rav1_global_motion_t *previous)
{
   rav1_bits_t bits;
   int forward = -1, backward = -1, second = -1;
   int hints[7], best_forward = 0, best_backward = 0, best_second = 0;
   unsigned i, j, index, type, abs_bits, prec_bits, shift, mx;
   int64_t predictor, scale;
   int value, round, sub;
   if (!out || previous == &out->global)
      return false;
   memset(out, 0, sizeof(*out));
   if (!input || !seq || !frame || !setup || input->failed || !input->data ||
       input->byte > input->size || input->bit > 7 || frame->show_existing ||
       frame->primary_ref_frame > 7 ||
       (frame->primary_ref_frame != 7 && !previous) ||
       (!frame->frame_is_intra && !refs) ||
       (seq->enable_order_hint &&
        (!seq->order_hint_bits || seq->order_hint_bits > 8)))
      return false;
   bits = *input;
   if (!frame->frame_is_intra)
   {
      out->reference_select = rav1_bits_read(&bits, 1);
      for (i = 0; i < 7; i++)
      {
         index = setup->ref_frame_idx[i];
         if (index > 7 || !(setup->valid_mask & (1u << index)))
            goto fail;
         hints[i] =
             seq->enable_order_hint
                 ? rav1_relative_dist(refs[index].order_hint, frame->order_hint,
                                      seq->order_hint_bits)
                 : 0;
      }
      if (out->reference_select && seq->enable_order_hint)
      {
         for (i = 0; i < 7; i++)
         {
            if (hints[i] < 0 && (forward < 0 || hints[i] > best_forward))
            {
               forward = (int)i;
               best_forward = hints[i];
            }
            else if (hints[i] > 0 && (backward < 0 || hints[i] < best_backward))
            {
               backward = (int)i;
               best_backward = hints[i];
            }
         }
         if (forward >= 0 && backward < 0)
            for (i = 0; i < 7; i++)
               if (rav1_relative_dist((unsigned)hints[i],
                                      (unsigned)best_forward,
                                      seq->order_hint_bits) < 0 &&
                   (second < 0 || rav1_relative_dist((unsigned)hints[i],
                                                     (unsigned)best_second,
                                                     seq->order_hint_bits) > 0))
               {
                  second = (int)i;
                  best_second = hints[i];
               }
         if (forward >= 0 && (backward >= 0 || second >= 0))
         {
            index = (unsigned)(backward >= 0 ? backward : second);
            out->skip_mode_allowed = 1;
            out->skip_mode_frame[0] =
                1 + ((unsigned)forward < index ? (unsigned)forward : index);
            out->skip_mode_frame[1] =
                1 + ((unsigned)forward > index ? (unsigned)forward : index);
            out->skip_mode_present = rav1_bits_read(&bits, 1);
         }
      }
      if (!frame->error_resilient && seq->enable_warped_motion)
         out->allow_warped_motion = rav1_bits_read(&bits, 1);
   }
   out->reduced_tx_set = rav1_bits_read(&bits, 1);
   for (i = 0; i < 7; i++)
   {
      out->global.params[i][2] = out->global.params[i][5] = 65536;
      if (frame->frame_is_intra)
         continue;
      type = RAV1_GLOBAL_IDENTITY;
      if (rav1_bits_read(&bits, 1))
      {
         if (rav1_bits_read(&bits, 1))
            type = RAV1_GLOBAL_ROTZOOM;
         else
            type = rav1_bits_read(&bits, 1) ? RAV1_GLOBAL_TRANSLATION
                                            : RAV1_GLOBAL_AFFINE;
      }
      out->global.type[i] = type;
      /* Linear terms precede translations in the bitstream. */
      for (j = 0; j < 6; j++)
      {
         index = (j + 2) % 6;
         if (type == RAV1_GLOBAL_IDENTITY ||
             (index >= 2 && type == RAV1_GLOBAL_TRANSLATION) ||
             ((index == 4 || index == 5) && type == RAV1_GLOBAL_ROTZOOM))
            continue;
         abs_bits = 12;
         prec_bits = index < 2 ? 6 : 15;
         if (index < 2 && type == RAV1_GLOBAL_TRANSLATION)
         {
            abs_bits = 9 - !setup->allow_high_precision_mv;
            prec_bits = 3 - !setup->allow_high_precision_mv;
         }
         shift = 16 - prec_bits;
         scale = (int64_t)1 << shift;
         mx = 1u << abs_bits;
         round = index % 3 == 2 ? 65536 : 0;
         sub = index % 3 == 2 ? 1 << prec_bits : 0;
         predictor = round;
         if (frame->primary_ref_frame != 7)
            predictor = previous->params[i][index];
         /* Division must round down for negative predictors, as in AV1 >>. */
         value = predictor < 0 && predictor % scale != 0;
         predictor = predictor / scale - value - sub;
         if (predictor < -(int)mx || predictor > (int)mx)
            goto fail;
         value = (int)rav1_bits_subexp_ref(&bits, 2 * mx + 1,
                                           (unsigned)(predictor + mx)) -
                 (int)mx;
         out->global.params[i][index] = (int32_t)(value * scale + round);
      }
      if (type == RAV1_GLOBAL_ROTZOOM)
      {
         out->global.params[i][4] = -out->global.params[i][3];
         out->global.params[i][5] = out->global.params[i][2];
      }
   }
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
