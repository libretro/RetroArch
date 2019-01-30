/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (reverb.c).
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

#include <retro_inline.h>
#include <libretro_dspfilter.h>

struct comb
{
   float *buffer;
   unsigned bufsize;
   unsigned bufidx;

   float feedback;
   float filterstore;
   float damp1, damp2;
};

struct allpass
{
   float *buffer;
   float feedback;
   unsigned bufsize;
   unsigned bufidx;
};

static INLINE float comb_process(struct comb *c, float input)
{
   float output         = c->buffer[c->bufidx];
   c->filterstore       = (output * c->damp2) + (c->filterstore * c->damp1);

   c->buffer[c->bufidx] = input + (c->filterstore * c->feedback);

   c->bufidx++;
   if (c->bufidx >= c->bufsize)
      c->bufidx = 0;

   return output;
}

static INLINE float allpass_process(struct allpass *a, float input)
{
   float bufout         = a->buffer[a->bufidx];
   float output         = -input + bufout;
   a->buffer[a->bufidx] = input + bufout * a->feedback;

   a->bufidx++;
   if (a->bufidx >= a->bufsize)
      a->bufidx = 0;

   return output;
}

#define numcombs 8
#define numallpasses 4
static const float muted = 0;
static const float fixedgain = 0.015f;
static const float scalewet = 3;
static const float scaledry = 2;
static const float scaledamp = 0.4f;
static const float scaleroom = 0.28f;
static const float offsetroom = 0.7f;
static const float initialroom = 0.5f;
static const float initialdamp = 0.5f;
static const float initialwet = 1.0f / 3.0f;
static const float initialdry = 0;
static const float initialwidth = 1;
static const float initialmode = 0;
static const float freezemode = 0.5f;

struct revmodel
{
   struct comb combL[numcombs];
   struct allpass allpassL[numallpasses];

   float *bufcomb[numcombs];
   float *bufallpass[numallpasses];

   float gain;
   float roomsize, roomsize1;
   float damp, damp1;
   float wet, wet1, wet2;
   float dry;
   float width;
   float mode;
};

static float revmodel_process(struct revmodel *rev, float in)
{
   int i;
   float mono_out = 0.0f;
   float mono_in  = in;
   float input    = mono_in * rev->gain;

   for (i = 0; i < numcombs; i++)
      mono_out += comb_process(&rev->combL[i], input);

   for (i = 0; i < numallpasses; i++)
      mono_out = allpass_process(&rev->allpassL[i], mono_out);

   return mono_in * rev->dry + mono_out * rev->wet1;
}

static void revmodel_update(struct revmodel *rev)
{
   int i;
   rev->wet1 = rev->wet * (rev->width / 2.0f + 0.5f);

   if (rev->mode >= freezemode)
   {
      rev->roomsize1 = 1.0f;
      rev->damp1 = 0.0f;
      rev->gain = muted;
   }
   else
   {
      rev->roomsize1 = rev->roomsize;
      rev->damp1 = rev->damp;
      rev->gain = fixedgain;
   }

   for (i = 0; i < numcombs; i++)
   {
      rev->combL[i].feedback = rev->roomsize1;
      rev->combL[i].damp1 = rev->damp1;
      rev->combL[i].damp2 = 1.0f - rev->damp1;
   }
}

static void revmodel_setroomsize(struct revmodel *rev, float value)
{
   rev->roomsize = value * scaleroom + offsetroom;
   revmodel_update(rev);
}

static void revmodel_setdamp(struct revmodel *rev, float value)
{
   rev->damp = value * scaledamp;
   revmodel_update(rev);
}

static void revmodel_setwet(struct revmodel *rev, float value)
{
   rev->wet = value * scalewet;
   revmodel_update(rev);
}

static void revmodel_setdry(struct revmodel *rev, float value)
{
   rev->dry = value * scaledry;
   revmodel_update(rev);
}

static void revmodel_setwidth(struct revmodel *rev, float value)
{
   rev->width = value;
   revmodel_update(rev);
}

static void revmodel_setmode(struct revmodel *rev, float value)
{
   rev->mode = value;
   revmodel_update(rev);
}

static void revmodel_init(struct revmodel *rev,int srate)
{

  static const int comb_lengths[8] = { 1116,1188,1277,1356,1422,1491,1557,1617 };
  static const int allpass_lengths[4] = { 225,341,441,556 };
  double r = srate * (1 / 44100.0);
  unsigned c;

   for (c = 0; c < numcombs; ++c)
   {
	   rev->bufcomb[c] = malloc(r*comb_lengths[c]*sizeof(float));
	   rev->combL[c].buffer  =  rev->bufcomb[c];
         memset(rev->combL[c].buffer,0,r*comb_lengths[c]*sizeof(float));
         rev->combL[c].bufsize=r*comb_lengths[c];
  }

   for (c = 0; c < numallpasses; ++c)
   {
	   rev->bufallpass[c] = malloc(r*allpass_lengths[c]*sizeof(float));
	   rev->allpassL[c].buffer  =  rev->bufallpass[c];
         memset(rev->allpassL[c].buffer,0,r*allpass_lengths[c]*sizeof(float));
         rev->allpassL[c].bufsize=r*allpass_lengths[c];
         rev->allpassL[c].feedback = 0.5f;
  }

   revmodel_setwet(rev, initialwet);
   revmodel_setroomsize(rev, initialroom);
   revmodel_setdry(rev, initialdry);
   revmodel_setdamp(rev, initialdamp);
   revmodel_setwidth(rev, initialwidth);
   revmodel_setmode(rev, initialmode);
}

struct reverb_data
{
   struct revmodel left, right;
};

static void reverb_free(void *data)
{
   struct reverb_data *rev = (struct reverb_data*)data;
   unsigned i;

   for (i = 0; i < numcombs; i++) {
   free(rev->left.bufcomb[i]);
   free(rev->right.bufcomb[i]);
   }

   for (i = 0; i < numallpasses; i++) {
   free(rev->left.bufallpass[i]);
   free(rev->right.bufallpass[i]);
   }
   free(data);
}

static void reverb_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   float *out;
   struct reverb_data *rev = (struct reverb_data*)data;

   output->samples         = input->samples;
   output->frames          = input->frames;
   out                     = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float in[2] = { out[0], out[1] };

      out[0] = revmodel_process(&rev->left, in[0]);
      out[1] = revmodel_process(&rev->right, in[1]);
   }
}

static void *reverb_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   float drytime, wettime, damping, roomwidth, roomsize;
   struct reverb_data *rev = (struct reverb_data*)
      calloc(1, sizeof(*rev));
   if (!rev)
      return NULL;

   config->get_float(userdata, "drytime", &drytime, 0.43f);
   config->get_float(userdata, "wettime", &wettime, 0.4f);
   config->get_float(userdata, "damping", &damping, 0.8f);
   config->get_float(userdata, "roomwidth", &roomwidth, 0.56f);
   config->get_float(userdata, "roomsize", &roomsize, 0.56f);

   revmodel_init(&rev->left,info->input_rate);
   revmodel_init(&rev->right,info->input_rate);

   revmodel_setdamp(&rev->left, damping);
   revmodel_setdry(&rev->left, drytime);
   revmodel_setwet(&rev->left, wettime);
   revmodel_setwidth(&rev->left, roomwidth);
   revmodel_setroomsize(&rev->left, roomsize);

   revmodel_setdamp(&rev->right, damping);
   revmodel_setdry(&rev->right, drytime);
   revmodel_setwet(&rev->right, wettime);
   revmodel_setwidth(&rev->right, roomwidth);
   revmodel_setroomsize(&rev->right, roomsize);

   return rev;
}

static const struct dspfilter_implementation reverb_plug = {
   reverb_init,
   reverb_process,
   reverb_free,

   DSPFILTER_API_VERSION,
   "Reverb",
   "reverb",
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation reverb_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &reverb_plug;
}

#undef dspfilter_get_implementation
