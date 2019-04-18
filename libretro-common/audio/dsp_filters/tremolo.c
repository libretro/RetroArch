/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (tremolo.c).
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
#include <string/stdstring.h>

#define sqr(a) ((a) * (a))

struct tremolo_core
{
      float freq;
      float depth;
	float* wavetable;
      int index;
	int maxindex;
};

struct tremolo
{
   struct tremolo_core left, right;
};

static void tremolo_free(void *data)
{
   struct tremolo *tre = (struct tremolo*)data;
   free(tre->left.wavetable);
   free(tre->right.wavetable);
   free(data);
}

static void tremolocore_init(struct tremolo_core *core,float depth,int samplerate,float freq)
{
      const double offset = 1. - depth / 2.;
      unsigned i;
      double env;
      core->index = 0;
	core->maxindex = samplerate/freq;
	core->wavetable = malloc(core->maxindex*sizeof(float));
	memset(core->wavetable, 0, core->maxindex * sizeof(float));
	for (i = 0; i < core->maxindex; i++) {
	env = freq * i / samplerate;
	env = sin((M_PI*2) * fmod(env + 0.25, 1.0));
	core->wavetable[i] = env * (1 - fabs(offset)) + offset;
      }
}

float tremolocore_core(struct tremolo_core *core,float in)
{
      core->index = core->index % core->maxindex;
      return in * core->wavetable[core->index++];
}

static void tremolo_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   float *out;
   struct tremolo *tre = (struct tremolo*)data;

   output->samples         = input->samples;
   output->frames          = input->frames;
   out                     = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float in[2] = { out[0], out[1] };

      out[0] = tremolocore_core(&tre->left, in[0]);
      out[1] = tremolocore_core(&tre->right, in[1]);
   }
}

static void *tremolo_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   float freq, depth;
   struct tremolo *tre   = (struct tremolo*)calloc(1, sizeof(*tre));
   if (!tre)
      return NULL;

   config->get_float(userdata, "freq", &freq,4.0f);
   config->get_float(userdata, "depth", &depth, 0.9f);
   tremolocore_init(&tre->left,depth,info->input_rate,freq);
   tremolocore_init(&tre->right,depth,info->input_rate,freq);
   return tre;
}

static const struct dspfilter_implementation tremolo_plug = {
   tremolo_init,
   tremolo_process,
   tremolo_free,

   DSPFILTER_API_VERSION,
   "Tremolo",
   "tremolo",
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation tremolo_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &tremolo_plug;
}

#undef dspfilter_get_implementation
