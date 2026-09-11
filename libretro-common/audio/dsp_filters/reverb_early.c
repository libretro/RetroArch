/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (reverb_early.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <math.h>
#include <stdlib.h>
#include <stdint.h>

#include <retro_inline.h>
#include <libretro_dspfilter.h>

#define EARLYREVERB_COMBS      4
#define EARLYREVERB_ALLPASS    2
#define EARLYREVERB_TAPS       8
/* int16 path: delay lines hold Q8 samples (1 LSB of s16 = 256), gains
 * are Q16 and recirculating coefficients Q30. Stored state saturates
 * far above full scale, so it can never wrap. */
#define EARLYREVERB_I16_LIMIT  ((int32_t)1 << 30)
/* Recirculating state below this magnitude is flushed to zero, well
 * under audibility and well above the denormal range, so a decaying
 * tail never drops into denormals. */
#define EARLYREVERB_FLUSH      1.0e-15f

struct earlyreverb_line
{
   float *buf;
   int32_t *buf_i;
   int32_t filter_store_i;
   unsigned len;
   unsigned pos;
   float filter_store;
};

struct earlyreverb_data
{
   struct earlyreverb_line comb[2][EARLYREVERB_COMBS];
   struct earlyreverb_line allpass[2][EARLYREVERB_ALLPASS];

   /* Every delay line, float and int16, is a view into this one block. */
   uint8_t *arena;
   float *predelay[2];
   float *early[2];
   int16_t *predelay_i[2];
   int16_t *early_i[2];

   /* int16 path coefficients. */
   int32_t early_gain_l_q16[EARLYREVERB_TAPS];
   int32_t early_gain_r_q16[EARLYREVERB_TAPS];
   int32_t early_cross_l_q16[EARLYREVERB_TAPS];
   int32_t early_cross_r_q16[EARLYREVERB_TAPS];
   int32_t comb_feedback_q30[2][EARLYREVERB_COMBS];
   int32_t damping_q30;
   int32_t undamped_q30;
   int32_t allpass_g_q30;
   int32_t dry_q16;
   int32_t wet_q16;
   int32_t early_mix_q16;
   int32_t late_mix_q16;
   int32_t width_q16;

   unsigned early_tap[EARLYREVERB_TAPS];
   unsigned predelay_len;
   unsigned predelay_pos;
   unsigned early_mask;
   unsigned early_pos;

   float comb_feedback[2][EARLYREVERB_COMBS];
   float early_gain_l[EARLYREVERB_TAPS];
   float early_gain_r[EARLYREVERB_TAPS];
   float dry;
   float wet;
   float early_mix;
   float late_mix;
   float width;
   float damping;
   float diffusion;
};

static INLINE float earlyreverb_flush(float v)
{
   return (fabs(v) < EARLYREVERB_FLUSH) ? 0.0f : v;
}

static float earlyreverb_comb(struct earlyreverb_line *d, float input,
      float feedback, float damping)
{
   float out         = d->buf[d->pos];
   d->filter_store   = earlyreverb_flush(
         out * (1.0f - damping) + d->filter_store * damping);
   d->buf[d->pos]    = earlyreverb_flush(input + d->filter_store * feedback);
   if (++d->pos >= d->len)
      d->pos = 0;
   return out;
}

static float earlyreverb_allpass(struct earlyreverb_line *d,
      float input, float g)
{
   float delayed     = d->buf[d->pos];
   float out         = delayed - input;
   d->buf[d->pos]    = earlyreverb_flush(input + delayed * g);
   if (++d->pos >= d->len)
      d->pos = 0;
   return out;
}

static void earlyreverb_run(struct earlyreverb_data *rv,
      float *samples, unsigned frames)
{
   unsigned f;

   for (f = 0; f < frames; f++, samples += 2)
   {
      unsigned t, i;
      float pre_l, pre_r, wet_l, wet_r, mid, side, mono_feed, g;
      float dry_l   = samples[0];
      float dry_r   = samples[1];
      float early_l = 0.0f;
      float early_r = 0.0f;
      float late_l  = 0.0f;
      float late_r  = 0.0f;

      pre_l = rv->predelay[0][rv->predelay_pos];
      pre_r = rv->predelay[1][rv->predelay_pos];
      rv->predelay[0][rv->predelay_pos] = dry_l;
      rv->predelay[1][rv->predelay_pos] = dry_r;

      rv->early[0][rv->early_pos] = pre_l;
      rv->early[1][rv->early_pos] = pre_r;

      for (t = 0; t < EARLYREVERB_TAPS; t++)
      {
         unsigned p = (rv->early_pos - rv->early_tap[t]) & rv->early_mask;
         float el   = rv->early[0][p];
         float er   = rv->early[1][p];

         early_l   += el * rv->early_gain_l[t] + er * rv->early_gain_r[t] * 0.30f;
         early_r   += er * rv->early_gain_r[t] + el * rv->early_gain_l[t] * 0.30f;
      }

      mono_feed = 0.5f * (pre_l + pre_r) + 0.20f * (early_l + early_r);
      for (i = 0; i < EARLYREVERB_COMBS; i++)
      {
         late_l += earlyreverb_comb(&rv->comb[0][i], mono_feed,
               rv->comb_feedback[0][i], rv->damping);
         late_r += earlyreverb_comb(&rv->comb[1][i], mono_feed,
               rv->comb_feedback[1][i], rv->damping);
      }
      late_l *= 0.25f;
      late_r *= 0.25f;

      g = 0.45f + 0.30f * rv->diffusion;
      for (i = 0; i < EARLYREVERB_ALLPASS; i++)
      {
         late_l = earlyreverb_allpass(&rv->allpass[0][i], late_l, g);
         late_r = earlyreverb_allpass(&rv->allpass[1][i], late_r, g);
      }

      wet_l = rv->early_mix * early_l + rv->late_mix * late_l;
      wet_r = rv->early_mix * early_r + rv->late_mix * late_r;

      mid   = 0.5f * (wet_l + wet_r);
      side  = 0.5f * (wet_l - wet_r) * rv->width;
      wet_l = mid + side;
      wet_r = mid - side;

      samples[0] = dry_l * rv->dry + wet_l * rv->wet;
      samples[1] = dry_r * rv->dry + wet_r * rv->wet;

      if (++rv->predelay_pos >= rv->predelay_len)
         rv->predelay_pos = 0;
      rv->early_pos = (rv->early_pos + 1) & rv->early_mask;
   }
}

static void earlyreverb_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   output->samples = input->samples;
   output->frames  = input->frames;
   earlyreverb_run((struct earlyreverb_data*)data,
         input->samples, input->frames);
}

static INLINE int64_t earlyreverb_rsh(int64_t v, unsigned s)
{
   int64_t h = (int64_t)1 << (s - 1);
   return (v >= 0) ? ((v + h) >> s) : -(((-v) + h) >> s);
}

static INLINE int32_t earlyreverb_sat(int64_t v)
{
   if (v > EARLYREVERB_I16_LIMIT)
      return EARLYREVERB_I16_LIMIT;
   if (v < -EARLYREVERB_I16_LIMIT)
      return -EARLYREVERB_I16_LIMIT;
   return (int32_t)v;
}

static int32_t earlyreverb_comb_i(struct earlyreverb_line *d, int32_t input,
      int32_t feedback, int32_t damping, int32_t undamped)
{
   int32_t out       = d->buf_i[d->pos];
   d->filter_store_i = earlyreverb_sat(earlyreverb_rsh(
         (int64_t)out * undamped + (int64_t)d->filter_store_i * damping, 30));
   d->buf_i[d->pos]  = earlyreverb_sat((int64_t)input + earlyreverb_rsh(
         (int64_t)d->filter_store_i * feedback, 30));
   if (++d->pos >= d->len)
      d->pos = 0;
   return out;
}

static int32_t earlyreverb_allpass_i(struct earlyreverb_line *d,
      int32_t input, int32_t g)
{
   int32_t delayed   = d->buf_i[d->pos];
   d->buf_i[d->pos]  = earlyreverb_sat((int64_t)input
         + earlyreverb_rsh((int64_t)delayed * g, 30));
   if (++d->pos >= d->len)
      d->pos = 0;
   return earlyreverb_sat((int64_t)delayed - input);
}

static void earlyreverb_process_i16(void *data,
      struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   unsigned f;
   struct earlyreverb_data *rv = (struct earlyreverb_data*)data;
   int16_t *samples            = input->samples;

   output->samples             = input->samples;
   output->frames              = input->frames;

   for (f = 0; f < input->frames; f++, samples += 2)
   {
      unsigned t, i;
      int32_t pre_l, pre_r, early_l, early_r, feed, late_l, late_r;
      int32_t wet_l, wet_r, mid, side;
      int64_t acc_l = 0, acc_r = 0, sum_l = 0, sum_r = 0, out;
      int32_t dry_l = samples[0];
      int32_t dry_r = samples[1];

      pre_l = rv->predelay_i[0][rv->predelay_pos];
      pre_r = rv->predelay_i[1][rv->predelay_pos];
      rv->predelay_i[0][rv->predelay_pos] = (int16_t)dry_l;
      rv->predelay_i[1][rv->predelay_pos] = (int16_t)dry_r;

      rv->early_i[0][rv->early_pos] = (int16_t)pre_l;
      rv->early_i[1][rv->early_pos] = (int16_t)pre_r;

      for (t = 0; t < EARLYREVERB_TAPS; t++)
      {
         unsigned p = (rv->early_pos - rv->early_tap[t]) & rv->early_mask;
         int32_t el = rv->early_i[0][p];
         int32_t er = rv->early_i[1][p];
         acc_l     += (int64_t)el * rv->early_gain_l_q16[t]
                    + (int64_t)er * rv->early_cross_r_q16[t];
         acc_r     += (int64_t)er * rv->early_gain_r_q16[t]
                    + (int64_t)el * rv->early_cross_l_q16[t];
      }
      /* s16 x Q16 -> Q8. */
      early_l = earlyreverb_sat(earlyreverb_rsh(acc_l, 8));
      early_r = earlyreverb_sat(earlyreverb_rsh(acc_r, 8));

      /* 0.5 * (pre_l + pre_r) + 0.20 * (early_l + early_r), in Q8. */
      feed = earlyreverb_sat(((int64_t)pre_l + pre_r) * 128
            + earlyreverb_rsh(((int64_t)early_l + early_r) * 13107, 16));

      for (i = 0; i < EARLYREVERB_COMBS; i++)
      {
         sum_l += earlyreverb_comb_i(&rv->comb[0][i], feed,
               rv->comb_feedback_q30[0][i], rv->damping_q30, rv->undamped_q30);
         sum_r += earlyreverb_comb_i(&rv->comb[1][i], feed,
               rv->comb_feedback_q30[1][i], rv->damping_q30, rv->undamped_q30);
      }
      late_l = earlyreverb_sat(earlyreverb_rsh(sum_l, 2));
      late_r = earlyreverb_sat(earlyreverb_rsh(sum_r, 2));

      for (i = 0; i < EARLYREVERB_ALLPASS; i++)
      {
         late_l = earlyreverb_allpass_i(&rv->allpass[0][i], late_l, rv->allpass_g_q30);
         late_r = earlyreverb_allpass_i(&rv->allpass[1][i], late_r, rv->allpass_g_q30);
      }

      wet_l = earlyreverb_sat(earlyreverb_rsh((int64_t)early_l * rv->early_mix_q16
            + (int64_t)late_l * rv->late_mix_q16, 16));
      wet_r = earlyreverb_sat(earlyreverb_rsh((int64_t)early_r * rv->early_mix_q16
            + (int64_t)late_r * rv->late_mix_q16, 16));

      mid   = earlyreverb_sat(earlyreverb_rsh((int64_t)wet_l + wet_r, 1));
      side  = earlyreverb_sat(earlyreverb_rsh(((int64_t)wet_l - wet_r)
               * rv->width_q16, 17));
      wet_l = earlyreverb_sat((int64_t)mid + side);
      wet_r = earlyreverb_sat((int64_t)mid - side);

      /* dry (s16) and wet (Q8) mixed with Q16 gains, back to s16. */
      out = earlyreverb_rsh((int64_t)dry_l * rv->dry_q16 * 256
            + (int64_t)wet_l * rv->wet_q16, 24);
      samples[0] = (int16_t)(out > 32767 ? 32767 : (out < -32768 ? -32768 : out));
      out = earlyreverb_rsh((int64_t)dry_r * rv->dry_q16 * 256
            + (int64_t)wet_r * rv->wet_q16, 24);
      samples[1] = (int16_t)(out > 32767 ? 32767 : (out < -32768 ? -32768 : out));

      if (++rv->predelay_pos >= rv->predelay_len)
         rv->predelay_pos = 0;
      rv->early_pos = (rv->early_pos + 1) & rv->early_mask;
   }
}

static void earlyreverb_free(void *data)
{
   struct earlyreverb_data *rv = (struct earlyreverb_data*)data;
   if (!rv)
      return;
   free(rv->arena);
   free(rv);
}

static float earlyreverb_clampf(float x, float lo, float hi)
{
   return x < lo ? lo : (x > hi ? hi : x);
}

static int32_t earlyreverb_q(float v, unsigned bits)
{
   return (int32_t)floor((double)v * (double)((int32_t)1 << bits) + 0.5);
}

static void *earlyreverb_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   static const float early_ms[EARLYREVERB_TAPS] =
      { 5.3f, 8.7f, 13.1f, 17.9f, 24.7f, 31.3f, 41.1f, 53.7f };
   static const float early_gain[EARLYREVERB_TAPS] =
      { 0.62f, 0.49f, 0.39f, 0.31f, 0.25f, 0.20f, 0.16f, 0.12f };
   static const float comb_ms[EARLYREVERB_COMBS] =
      { 25.31f, 26.94f, 28.96f, 30.75f };
   static const float ap_ms[EARLYREVERB_ALLPASS] =
      { 12.61f, 10.00f };

   float predelay_ms, room_size, decay_sec, damping, diffusion;
   float early_mix, late_mix, width, drywet, sr;
   unsigned ch, i, early_len, early_size;
   size_t total, total_i32, total_i16;
   float *cur;
   int32_t *cur_i32;
   int16_t *cur_i16;
   struct earlyreverb_data *rv;

   if (!info || info->input_rate <= 1.0f)
      return NULL;

   config->get_float(userdata, "predelay_ms", &predelay_ms, 18.0f);
   config->get_float(userdata, "room_size", &room_size, 1.0f);
   config->get_float(userdata, "decay_sec", &decay_sec, 2.6f);
   config->get_float(userdata, "damping", &damping, 0.42f);
   config->get_float(userdata, "diffusion", &diffusion, 0.72f);
   config->get_float(userdata, "early_mix", &early_mix, 0.42f);
   config->get_float(userdata, "late_mix", &late_mix, 0.78f);
   config->get_float(userdata, "width", &width, 1.15f);
   config->get_float(userdata, "drywet", &drywet, 0.35f);

   predelay_ms = earlyreverb_clampf(predelay_ms, 0.0f, 250.0f);
   room_size   = earlyreverb_clampf(room_size, 0.45f, 2.25f);
   decay_sec   = earlyreverb_clampf(decay_sec, 0.20f, 20.0f);
   damping     = earlyreverb_clampf(damping, 0.0f, 0.98f);
   diffusion   = earlyreverb_clampf(diffusion, 0.0f, 1.0f);
   early_mix   = earlyreverb_clampf(early_mix, 0.0f, 1.5f);
   late_mix    = earlyreverb_clampf(late_mix, 0.0f, 1.5f);
   width       = earlyreverb_clampf(width, 0.0f, 2.0f);
   drywet      = earlyreverb_clampf(drywet, 0.0f, 1.0f);

   if (!(rv = (struct earlyreverb_data*)calloc(1, sizeof(*rv))))
      return NULL;

   rv->dry       = 1.0f - drywet;
   rv->wet       = drywet;
   rv->early_mix = early_mix;
   rv->late_mix  = late_mix;
   rv->width     = width;
   rv->damping   = damping;
   rv->diffusion = diffusion;

   sr               = info->input_rate;
   rv->predelay_len = (unsigned)(predelay_ms * 0.001f * sr + 0.5f);
   if (rv->predelay_len < 1)
      rv->predelay_len = 1;

   early_len = (unsigned)(0.090f * room_size * sr + 8.0f);
   if (early_len < 64)
      early_len = 64;
   /* The early-reflection ring is rounded up to a power of two so the
    * taps index it with a mask; every tap stays below early_len. */
   early_size = 64;
   while (early_size < early_len)
      early_size <<= 1;
   rv->early_mask = early_size - 1;

   for (i = 0; i < EARLYREVERB_TAPS; i++)
   {
      unsigned tap = (unsigned)(early_ms[i] * 0.001f * room_size * sr + 0.5f);
      if (tap >= early_len)
         tap = early_len - 1;
      if (tap < 1)
         tap = 1;
      rv->early_tap[i] = tap;

      /* Alternating energy distribution decorrelates the stereo ERs. */
      if ((i & 1) == 0)
      {
         rv->early_gain_l[i] = early_gain[i];
         rv->early_gain_r[i] = early_gain[i] * 0.72f;
      }
      else
      {
         rv->early_gain_l[i] = early_gain[i] * 0.72f;
         rv->early_gain_r[i] = early_gain[i];
      }
   }

   total = 2 * (size_t)rv->predelay_len + 2 * (size_t)early_size;
   for (ch = 0; ch < 2; ch++)
   {
      for (i = 0; i < EARLYREVERB_COMBS; i++)
      {
         float spread_ms = ch ? (0.73f + 0.17f * (float)i) : 0.0f;
         float delay_ms  = (comb_ms[i] + spread_ms) * room_size;
         unsigned len    = (unsigned)(delay_ms * 0.001f * sr + 0.5f);
         float delay_sec;

         if (len < 2)
            len = 2;
         rv->comb[ch][i].len = len;
         total              += len;

         delay_sec = (float)len / sr;
         rv->comb_feedback[ch][i] = earlyreverb_clampf(
               (float)pow(10.0, (-3.0f * delay_sec) / decay_sec),
               0.0f, 0.985f);
      }

      for (i = 0; i < EARLYREVERB_ALLPASS; i++)
      {
         float spread_ms = ch ? (0.31f + 0.11f * (float)i) : 0.0f;
         unsigned len    = (unsigned)((ap_ms[i] + spread_ms) * 0.001f *
               room_size * sr + 0.5f);
         if (len < 2)
            len = 2;
         rv->allpass[ch][i].len = len;
         total                 += len;
      }
   }

   /* Floats first, then the int32 comb/allpass lines, then the int16
    * pre-delay and early rings; each region is naturally aligned. */
   total_i16 = 2 * (size_t)rv->predelay_len + 2 * (size_t)early_size;
   total_i32 = total - total_i16;
   if (!(rv->arena = (uint8_t*)calloc(1, total * sizeof(float)
               + total_i32 * sizeof(int32_t) + total_i16 * sizeof(int16_t))))
   {
      free(rv);
      return NULL;
   }

   cur     = (float*)rv->arena;
   cur_i32 = (int32_t*)(rv->arena + total * sizeof(float));
   cur_i16 = (int16_t*)(rv->arena + total * sizeof(float)
         + total_i32 * sizeof(int32_t));
   for (ch = 0; ch < 2; ch++)
   {
      rv->predelay[ch] = cur;
      cur             += rv->predelay_len;
      rv->early[ch]    = cur;
      cur             += early_size;
      rv->predelay_i[ch] = cur_i16;
      cur_i16           += rv->predelay_len;
      rv->early_i[ch]    = cur_i16;
      cur_i16           += early_size;
      for (i = 0; i < EARLYREVERB_COMBS; i++)
      {
         rv->comb[ch][i].buf   = cur;
         cur                  += rv->comb[ch][i].len;
         rv->comb[ch][i].buf_i = cur_i32;
         cur_i32              += rv->comb[ch][i].len;
      }
      for (i = 0; i < EARLYREVERB_ALLPASS; i++)
      {
         rv->allpass[ch][i].buf   = cur;
         cur                     += rv->allpass[ch][i].len;
         rv->allpass[ch][i].buf_i = cur_i32;
         cur_i32                 += rv->allpass[ch][i].len;
      }
      for (i = 0; i < EARLYREVERB_COMBS; i++)
         rv->comb_feedback_q30[ch][i] = earlyreverb_q(
               rv->comb_feedback[ch][i], 30);
   }

   for (i = 0; i < EARLYREVERB_TAPS; i++)
   {
      rv->early_gain_l_q16[i]  = earlyreverb_q(rv->early_gain_l[i], 16);
      rv->early_gain_r_q16[i]  = earlyreverb_q(rv->early_gain_r[i], 16);
      rv->early_cross_l_q16[i] = earlyreverb_q(rv->early_gain_l[i] * 0.30f, 16);
      rv->early_cross_r_q16[i] = earlyreverb_q(rv->early_gain_r[i] * 0.30f, 16);
   }
   rv->damping_q30   = earlyreverb_q(rv->damping, 30);
   rv->undamped_q30  = earlyreverb_q(1.0f - rv->damping, 30);
   rv->allpass_g_q30 = earlyreverb_q(0.45f + 0.30f * rv->diffusion, 30);
   rv->dry_q16       = earlyreverb_q(rv->dry, 16);
   rv->wet_q16       = earlyreverb_q(rv->wet, 16);
   rv->early_mix_q16 = earlyreverb_q(rv->early_mix, 16);
   rv->late_mix_q16  = earlyreverb_q(rv->late_mix, 16);
   rv->width_q16     = earlyreverb_q(rv->width, 16);

   return rv;
}

static const struct dspfilter_implementation earlyreverb_plug = {
   earlyreverb_init,
   earlyreverb_process,
   earlyreverb_free,

   DSPFILTER_API_VERSION,
   "Early Reflection Reverb",
   "earlyreverb",

   earlyreverb_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation earlyreverb_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &earlyreverb_plug;
}

#undef dspfilter_get_implementation
