/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (wahwah.c).
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

#define WAHWAH_LFO_SKIP_SAMPLES 30

struct wahwah_data
{
   float phase;
   float lfoskip;
   float b0, b1, b2, a0, a1, a2;
   float freq, startphase;
   float depth, freqofs, res;
   unsigned long skipcount;

   struct
   {
      float xn1, xn2, yn1, yn2;
   } l, r;
};

static void wahwah_free(void *data)
{
   if (data)
      free(data);
}

static void wahwah_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   struct wahwah_data *wah = (struct wahwah_data*)data;
   float *out              = output->samples;

   output->samples         = input->samples;
   output->frames          = input->frames;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float out_l, out_r;
      float in[2] = { out[0], out[1] };

      if ((wah->skipcount++ % WAHWAH_LFO_SKIP_SAMPLES) == 0)
      {
         float omega, sn, cs, alpha;
         float frequency = (1.0f + cos(wah->skipcount * wah->lfoskip + wah->phase)) / 2.0f;

         frequency       = frequency * wah->depth * (1.0f - wah->freqofs) + wah->freqofs;
         frequency       = exp((frequency - 1.0f) * 6.0f);

         omega           = M_PI * frequency;
         sn              = sin(omega);
         cs              = cos(omega);
         alpha           = sn / (2.0f * wah->res);

         wah->b0         = (1.0f - cs) / 2.0f;
         wah->b1         = 1.0f  - cs;
         wah->b2         = (1.0f - cs) / 2.0f;
         wah->a0         = 1.0f + alpha;
         wah->a1         = -2.0f * cs;
         wah->a2         = 1.0f - alpha;
      }

      out_l              = (wah->b0 * in[0] + wah->b1 * wah->l.xn1 + wah->b2 * wah->l.xn2 - wah->a1 * wah->l.yn1 - wah->a2 * wah->l.yn2) / wah->a0;
      out_r              = (wah->b0 * in[1] + wah->b1 * wah->r.xn1 + wah->b2 * wah->r.xn2 - wah->a1 * wah->r.yn1 - wah->a2 * wah->r.yn2) / wah->a0;

      wah->l.xn2         = wah->l.xn1;
      wah->l.xn1         = in[0];
      wah->l.yn2         = wah->l.yn1;
      wah->l.yn1         = out_l;

      wah->r.xn2         = wah->r.xn1;
      wah->r.xn1         = in[1];
      wah->r.yn2         = wah->r.yn1;
      wah->r.yn1         = out_r;

      out[0]             = out_l;
      out[1]             = out_r;
   }
}

static void *wahwah_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   struct wahwah_data *wah = (struct wahwah_data*)calloc(1, sizeof(*wah));
   if (!wah)
      return NULL;

   config->get_float(userdata, "lfo_freq", &wah->freq, 1.5f);
   config->get_float(userdata, "lfo_start_phase", &wah->startphase, 0.0f);
   config->get_float(userdata, "freq_offset", &wah->freqofs, 0.3f);
   config->get_float(userdata, "depth", &wah->depth, 0.7f);
   config->get_float(userdata, "resonance", &wah->res, 2.5f);

   wah->lfoskip = wah->freq * 2.0f * M_PI / info->input_rate;
   wah->phase   = wah->startphase * M_PI / 180.0f;

   return wah;
}

static const struct dspfilter_implementation wahwah_plug = {
   wahwah_init,
   wahwah_process,
   wahwah_free,

   DSPFILTER_API_VERSION,
   "Wah-Wah",
   "wahwah",
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation wahwah_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *
dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &wahwah_plug;
}

#undef dspfilter_get_implementation
