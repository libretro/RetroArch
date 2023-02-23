/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (chorus.c).
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
#include <string.h>

#include <retro_miscellaneous.h>
#include <libretro_dspfilter.h>

#define CHORUS_MAX_DELAY 4096
#define CHORUS_DELAY_MASK (CHORUS_MAX_DELAY - 1)

struct chorus_data
{
   float old[2][CHORUS_MAX_DELAY];
   float delay;
   float depth;
   float input_rate;
   float mix_dry;
   float mix_wet;
   unsigned old_ptr;
   unsigned lfo_ptr;
   unsigned lfo_period;
};

static void chorus_free(void *data)
{
   if (data)
      free(data);
}

static void chorus_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   float *out             = NULL;
   struct chorus_data *ch = (struct chorus_data*)data;

   output->samples        = input->samples;
   output->frames         = input->frames;
   out                    = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      unsigned delay_int;
      float delay_frac, l_a, l_b, r_a, r_b;
      float chorus_l, chorus_r;
      float in[2]             = { out[0], out[1] };
      float delay             = ch->delay + ch->depth * sin((2.0 * M_PI * ch->lfo_ptr++) / ch->lfo_period);

      delay                  *= ch->input_rate;
      if (ch->lfo_ptr >= ch->lfo_period)
         ch->lfo_ptr          = 0;

      delay_int               = (unsigned)delay;

      if (delay_int >= CHORUS_MAX_DELAY - 1)
         delay_int            = CHORUS_MAX_DELAY - 2;

      delay_frac              = delay - delay_int;

      ch->old[0][ch->old_ptr] = in[0];
      ch->old[1][ch->old_ptr] = in[1];

      l_a                     = ch->old[0][(ch->old_ptr - delay_int - 0) & CHORUS_DELAY_MASK];
      l_b                     = ch->old[0][(ch->old_ptr - delay_int - 1) & CHORUS_DELAY_MASK];
      r_a                     = ch->old[1][(ch->old_ptr - delay_int - 0) & CHORUS_DELAY_MASK];
      r_b                     = ch->old[1][(ch->old_ptr - delay_int - 1) & CHORUS_DELAY_MASK];

      /* Lerp introduces aliasing of the chorus component,
       * but doing full polyphase here is probably overkill. */
      chorus_l                = l_a * (1.0f - delay_frac) + l_b * delay_frac;
      chorus_r                = r_a * (1.0f - delay_frac) + r_b * delay_frac;

      out[0]                  = ch->mix_dry * in[0] + ch->mix_wet * chorus_l;
      out[1]                  = ch->mix_dry * in[1] + ch->mix_wet * chorus_r;

      ch->old_ptr             = (ch->old_ptr + 1) & CHORUS_DELAY_MASK;
   }
}

static void *chorus_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   float delay, depth, lfo_freq, drywet;
   struct chorus_data *ch = (struct chorus_data*)calloc(1, sizeof(*ch));
   if (!ch)
      return NULL;

   config->get_float(userdata, "delay_ms", &delay, 25.0f);
   config->get_float(userdata, "depth_ms", &depth, 1.0f);
   config->get_float(userdata, "lfo_freq", &lfo_freq, 0.5f);
   config->get_float(userdata, "drywet", &drywet, 0.8f);

   delay            /= 1000.0f;
   depth            /= 1000.0f;

   if (depth > delay)
      depth          = delay;

   if (drywet < 0.0f)
      drywet         = 0.0f;
   else if (drywet > 1.0f)
      drywet         = 1.0f;

   ch->mix_dry       = 1.0f - 0.5f * drywet;
   ch->mix_wet       = 0.5f * drywet;

   ch->delay         = delay;
   ch->depth         = depth;
   ch->lfo_period    = (1.0f / lfo_freq) * info->input_rate;
   ch->input_rate    = info->input_rate;
   if (!ch->lfo_period)
      ch->lfo_period = 1;
   return ch;
}

static const struct dspfilter_implementation chorus_plug = {
   chorus_init,
   chorus_process,
   chorus_free,

   DSPFILTER_API_VERSION,
   "Chorus",
   "chorus",
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation chorus_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *
dspfilter_get_implementation(dspfilter_simd_mask_t mask) { return &chorus_plug; }

#undef dspfilter_get_implementation
