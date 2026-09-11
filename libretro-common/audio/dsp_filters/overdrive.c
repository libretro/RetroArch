/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (overdrive.c).
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

/* int16 path: the shaper is a table of the float curve over
 * [-16, 16) in Q20 input units, 128 points per unit, read with linear
 * interpolation; signals run in Q28 (1.0 = full scale). */
#define OVERDRIVE_LUT_SHIFT 13
#define OVERDRIVE_LUT_SIZE  4096
#define OVERDRIVE_LUT_SPAN  (16 << 20)

/* Filter state below this magnitude is flushed to zero, well under
 * audibility and well above the denormal range. */
#define OVERDRIVE_FLUSH 1.0e-15f

struct overdrive_data
{
   /* Asymmetry is a bias into the tanh, with the bias's own output
    * subtracted and the small-signal gain restored, so the curve stays
    * monotonic at any drive. Kept in double so silence maps to exactly
    * zero. */
   double bias;
   double bias_out;
   double bias_norm;

   /* int16 path: shaper table, coefficients and state. */
   int32_t lut[OVERDRIVE_LUT_SIZE + 1];
   int32_t drive_q20;
   int32_t bias_q20;
   int32_t dc_r_q30;
   int32_t tone_alpha_q30;
   int32_t level_q24;
   int32_t mix_q16;
   int32_t prev_in_i[2];
   int32_t prev_hp_i[2];
   int32_t tone_lp_i[2];

   float drive;
   float tone_alpha;
   float mix;
   float level;
   float dc_r;
   float prev_in[2];
   float prev_hp[2];
   float tone_lp[2];
};

static INLINE float overdrive_flush(float v)
{
   return (fabs(v) < OVERDRIVE_FLUSH) ? 0.0f : v;
}

static void overdrive_run(struct overdrive_data *od,
      float *samples, unsigned frames)
{
   unsigned i, ch;

   for (i = 0; i < frames; i++, samples += 2)
   {
      for (ch = 0; ch < 2; ch++)
      {
         float hp, wet;
         float dry    = samples[ch];
         float driven = dry * od->drive;
         float shaped;

         if (driven < -12.0f)
            driven = -12.0f;
         else if (driven > 12.0f)
            driven = 12.0f;
         shaped = (float)((tanh((double)driven + od->bias) - od->bias_out)
               * od->bias_norm);

         /* DC blocker for the offset the asymmetric curve introduces. */
         hp               = shaped - od->prev_in[ch]
                          + od->dc_r * od->prev_hp[ch];
         hp               = overdrive_flush(hp);
         od->prev_in[ch]  = shaped;
         od->prev_hp[ch]  = hp;

         /* One-pole low-pass tone stage with a small direct component. */
         od->tone_lp[ch] += od->tone_alpha * (hp - od->tone_lp[ch]);
         od->tone_lp[ch]  = overdrive_flush(od->tone_lp[ch]);
         wet              = 0.30f * hp + 0.70f * od->tone_lp[ch];
         wet             *= od->level;

         samples[ch]      = dry + (wet - dry) * od->mix;
      }
   }
}

static void overdrive_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   output->samples = input->samples;
   output->frames  = input->frames;
   overdrive_run((struct overdrive_data*)data, input->samples, input->frames);
}

/* Round-half-away-from-zero arithmetic shift, defined for negative
 * values without relying on implementation-defined >> of signed. */
static INLINE int64_t overdrive_rsh(int64_t v, unsigned s)
{
   int64_t h = (int64_t)1 << (s - 1);
   return (v >= 0) ? ((v + h) >> s) : -(((-v) + h) >> s);
}

static INLINE int32_t overdrive_shape_i(const struct overdrive_data *od,
      int32_t v_q20)
{
   uint32_t pos  = (uint32_t)(v_q20 + OVERDRIVE_LUT_SPAN);
   uint32_t idx  = pos >> OVERDRIVE_LUT_SHIFT;
   int32_t  frac = (int32_t)(pos & ((1u << OVERDRIVE_LUT_SHIFT) - 1u));
   int32_t  a    = od->lut[idx];
   return a + (int32_t)overdrive_rsh(
         (int64_t)(od->lut[idx + 1] - a) * frac, OVERDRIVE_LUT_SHIFT);
}

static void overdrive_process_i16(void *data,
      struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   unsigned i, ch;
   struct overdrive_data *od = (struct overdrive_data*)data;
   int16_t *samples          = input->samples;
   const int32_t clip        = 12 << 20;

   output->samples           = input->samples;
   output->frames            = input->frames;

   for (i = 0; i < input->frames; i++, samples += 2)
   {
      for (ch = 0; ch < 2; ch++)
      {
         int32_t u, shaped, hp;
         int64_t wet, dry, out;
         int32_t x = samples[ch];

         u = (int32_t)overdrive_rsh((int64_t)x * od->drive_q20, 15);
         if (u < -clip)
            u = -clip;
         else if (u > clip)
            u = clip;
         shaped = overdrive_shape_i(od, u + od->bias_q20);

         hp                 = shaped - od->prev_in_i[ch] + (int32_t)
            overdrive_rsh((int64_t)od->dc_r_q30 * od->prev_hp_i[ch], 30);
         od->prev_in_i[ch]  = shaped;
         od->prev_hp_i[ch]  = hp;

         od->tone_lp_i[ch] += (int32_t)overdrive_rsh(
               (int64_t)od->tone_alpha_q30 * (hp - od->tone_lp_i[ch]), 30);

         /* 0.30 * hp + 0.70 * tone, both Q30 constants. */
         wet = overdrive_rsh((int64_t)322122547 * hp
               + (int64_t)751619277 * od->tone_lp_i[ch], 30);
         wet = overdrive_rsh(wet * od->level_q24, 24);

         dry = (int64_t)x * 8192;
         out = dry + overdrive_rsh((wet - dry) * od->mix_q16, 16);
         out = overdrive_rsh(out, 13);
         if      (out >  32767)
            out =  32767;
         else if (out < -32768)
            out = -32768;
         samples[ch] = (int16_t)out;
      }
   }
}

static void overdrive_free(void *data)
{
   free(data);
}

static float overdrive_clampf(float x, float lo, float hi)
{
   return x < lo ? lo : (x > hi ? hi : x);
}

static float overdrive_db_to_gain(float db)
{
   return (float)pow(10.0, db * 0.05f);
}

static void *overdrive_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   float drive_db, tone_hz, mix, level_db, asymmetry, sr, omega;
   double t;
   unsigned i;
   struct overdrive_data *od;

   if (!info || info->input_rate <= 1.0f)
      return NULL;

   if (!(od = (struct overdrive_data*)calloc(1, sizeof(*od))))
      return NULL;

   config->get_float(userdata, "drive_db", &drive_db, 18.0f);
   config->get_float(userdata, "tone_hz", &tone_hz, 6500.0f);
   config->get_float(userdata, "drywet", &mix, 1.0f);
   config->get_float(userdata, "level_db", &level_db, -4.0f);
   config->get_float(userdata, "asymmetry", &asymmetry, 0.12f);

   sr             = info->input_rate;
   tone_hz        = overdrive_clampf(tone_hz, 200.0f, sr * 0.45f);
   omega          = 2.0f * 3.14159265358979323846f * tone_hz / sr;

   od->drive      = overdrive_db_to_gain(overdrive_clampf(drive_db, 0.0f, 42.0f));
   od->tone_alpha = 1.0f - (float)exp(-omega);
   od->mix        = overdrive_clampf(mix, 0.0f, 1.0f);
   od->level      = overdrive_db_to_gain(overdrive_clampf(level_db, -30.0f, 12.0f));
   od->dc_r       = 0.995f;

   /* tanh(x + b) - tanh(b), scaled by 1 / (1 - tanh(b)^2), expands to
    * x - tanh(b) x^2 + ..., so tanh(b) = -0.15 * asymmetry keeps the
    * even-harmonic balance of the asymmetry control. */
   t              = -0.15 * overdrive_clampf(asymmetry, 0.0f, 1.0f);
   od->bias       = 0.5 * log((1.0 + t) / (1.0 - t));
   od->bias_out   = tanh(od->bias);
   od->bias_norm  = 1.0 / (1.0 - od->bias_out * od->bias_out);

   od->drive_q20      = (int32_t)floor((double)od->drive * 1048576.0 + 0.5);
   od->bias_q20       = (int32_t)floor(od->bias * 1048576.0 + 0.5);
   od->dc_r_q30       = (int32_t)floor((double)od->dc_r * 1073741824.0 + 0.5);
   od->tone_alpha_q30 = (int32_t)floor((double)od->tone_alpha * 1073741824.0 + 0.5);
   od->level_q24      = (int32_t)floor((double)od->level * 16777216.0 + 0.5);
   od->mix_q16        = (int32_t)floor((double)od->mix * 65536.0 + 0.5);
   for (i = 0; i <= OVERDRIVE_LUT_SIZE; i++)
      od->lut[i] = (int32_t)floor((tanh(-16.0 + (double)i / 128.0)
            - od->bias_out) * od->bias_norm * 268435456.0 + 0.5);

   return od;
}

static const struct dspfilter_implementation overdrive_plug = {
   overdrive_init,
   overdrive_process,
   overdrive_free,

   DSPFILTER_API_VERSION,
   "Distortion / Overdrive",
   "overdrive",

   overdrive_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation overdrive_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &overdrive_plug;
}

#undef dspfilter_get_implementation
