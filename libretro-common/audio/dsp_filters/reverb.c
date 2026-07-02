/* Copyright  (C) 2010-2020 The RetroArch team
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

   /* Q16 mirrors for the deterministic int16 path (buffer/filterstore in
    * int64 Q16 for headroom + precision on the small, resonant signal). */
   int64_t *buffer_i;
   int64_t filterstore_i;
   int32_t feedback_q, damp1_q, damp2_q;
};

struct allpass
{
   float *buffer;
   float feedback;
   unsigned bufsize;
   unsigned bufidx;

   int64_t *buffer_i;
   int32_t feedback_q;
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

/* Q16 multiply with round-half-away-from-zero, for the int16 path. */
static INLINE int64_t rmul_q16(int64_t x, int32_t g)
{
   int64_t p = x * (int64_t)g;
   return (p >= 0) ? ((p + 32768) >> 16) : -(((-p) + 32768) >> 16);
}

static INLINE int64_t comb_process_i16(struct comb *c, int64_t input)
{
   int64_t output         = c->buffer_i[c->bufidx];
   c->filterstore_i       = rmul_q16(output, c->damp2_q)
                          + rmul_q16(c->filterstore_i, c->damp1_q);
   c->buffer_i[c->bufidx] = input + rmul_q16(c->filterstore_i, c->feedback_q);

   c->bufidx++;
   if (c->bufidx >= c->bufsize)
      c->bufidx = 0;

   return output;
}

static INLINE int64_t allpass_process_i16(struct allpass *a, int64_t input)
{
   int64_t bufout         = a->buffer_i[a->bufidx];
   int64_t output         = -input + bufout;
   a->buffer_i[a->bufidx] = input + rmul_q16(bufout, a->feedback_q);

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

   int64_t *bufcomb_i[numcombs];
   int64_t *bufallpass_i[numallpasses];
   int32_t gain_q, wet1_q, dry_q;

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
   unsigned i;
   float mono_out = 0.0f;
   float mono_in  = in;
   float input    = mono_in * rev->gain;

   for (i = 0; i < numcombs; i++)
      mono_out += comb_process(&rev->combL[i], input);

   for (i = 0; i < numallpasses; i++)
      mono_out = allpass_process(&rev->allpassL[i], mono_out);

   return mono_in * rev->dry + mono_out * rev->wet1;
}

/* Deterministic int16 counterpart of revmodel_process(): the input is scaled
 * by gain into Q16, run through the comb and allpass networks in Q16 int64,
 * then mixed dry+wet and rounded/saturated to s16. */
static int32_t revmodel_process_i16(struct revmodel *rev, int32_t in)
{
   unsigned i;
   int64_t mono_out = 0;
   int64_t input    = (int64_t)in * rev->gain_q;   /* Q16 */
   int64_t res;
   int32_t v;

   for (i = 0; i < numcombs; i++)
      mono_out += comb_process_i16(&rev->combL[i], input);

   for (i = 0; i < numallpasses; i++)
      mono_out  = allpass_process_i16(&rev->allpassL[i], mono_out);

   res = (int64_t)in * rev->dry_q + rmul_q16(mono_out, rev->wet1_q);   /* Q16 */
   v   = (res >= 0) ?  (int32_t)(( res + 32768) >> 16)
                    : -(int32_t)((-res + 32768) >> 16);
   if      (v >  32767) v =  32767;
   else if (v < -32768) v = -32768;
   return v;
}

/* Quantize the finalized float coefficients to Q16 for the int16 path.
 * Called once after all setters have run. */
static void revmodel_quantize(struct revmodel *rev)
{
   unsigned i;
   rev->gain_q = (int32_t)floor((double)rev->gain * 65536.0 + 0.5);
   rev->wet1_q = (int32_t)floor((double)rev->wet1 * 65536.0 + 0.5);
   rev->dry_q  = (int32_t)floor((double)rev->dry  * 65536.0 + 0.5);

   for (i = 0; i < numcombs; i++)
   {
      rev->combL[i].feedback_q = (int32_t)floor((double)rev->combL[i].feedback * 65536.0 + 0.5);
      rev->combL[i].damp1_q    = (int32_t)floor((double)rev->combL[i].damp1    * 65536.0 + 0.5);
      rev->combL[i].damp2_q    = (int32_t)floor((double)rev->combL[i].damp2    * 65536.0 + 0.5);
   }

   for (i = 0; i < numallpasses; i++)
      rev->allpassL[i].feedback_q = (int32_t)floor((double)rev->allpassL[i].feedback * 65536.0 + 0.5);
}

static void revmodel_update(struct revmodel *rev)
{
   unsigned i;
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
   unsigned c;
   static const int comb_lengths[8]    = { 1116,1188,1277,1356,1422,1491,1557,1617 };
   static const int allpass_lengths[4] = { 225,341,441,556 };
   double r = srate * (1 / 44100.0);

   for (c = 0; c < numcombs; ++c)
   {
      unsigned bufsize         = (unsigned)(r * comb_lengths[c]);
      rev->bufcomb[c]          = (float*)calloc(bufsize, sizeof(float));
      rev->combL[c].buffer     = rev->bufcomb[c];
      rev->combL[c].bufsize    = bufsize;
      rev->bufcomb_i[c]        = (int64_t*)calloc(bufsize, sizeof(int64_t));
      rev->combL[c].buffer_i   = rev->bufcomb_i[c];
   }

   for (c = 0; c < numallpasses; ++c)
   {
      unsigned bufsize          = (unsigned)(r * allpass_lengths[c]);
      rev->bufallpass[c]        = (float*)calloc(bufsize, sizeof(float));
      rev->allpassL[c].buffer   = rev->bufallpass[c];
      rev->allpassL[c].bufsize  = bufsize;
      rev->allpassL[c].feedback = 0.5f;
      rev->bufallpass_i[c]      = (int64_t*)calloc(bufsize, sizeof(int64_t));
      rev->allpassL[c].buffer_i = rev->bufallpass_i[c];
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
   unsigned i;
   struct reverb_data *rev = (struct reverb_data*)data;

   for (i = 0; i < numcombs; i++)
   {
      free(rev->left.bufcomb[i]);
      free(rev->right.bufcomb[i]);
      free(rev->left.bufcomb_i[i]);
      free(rev->right.bufcomb_i[i]);
   }

   for (i = 0; i < numallpasses; i++)
   {
      free(rev->left.bufallpass[i]);
      free(rev->right.bufallpass[i]);
      free(rev->left.bufallpass_i[i]);
      free(rev->right.bufallpass_i[i]);
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

static void reverb_process_i16(void *data, struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   unsigned i;
   int16_t *out;
   struct reverb_data *rev = (struct reverb_data*)data;

   output->samples         = input->samples;
   output->frames          = input->frames;
   out                     = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      int32_t in0 = out[0];
      int32_t in1 = out[1];

      out[0] = (int16_t)revmodel_process_i16(&rev->left,  in0);
      out[1] = (int16_t)revmodel_process_i16(&rev->right, in1);
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

   revmodel_quantize(&rev->left);
   revmodel_quantize(&rev->right);

   return rev;
}

static const struct dspfilter_implementation reverb_plug = {
   reverb_init,
   reverb_process,
   reverb_free,

   DSPFILTER_API_VERSION,
   "Reverb",
   "reverb",

   reverb_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation reverb_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   return &reverb_plug;
}

#undef dspfilter_get_implementation
