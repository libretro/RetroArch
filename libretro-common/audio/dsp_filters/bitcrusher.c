/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (bitcrusher.c).
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

#include <libretro_dspfilter.h>

struct bitcrusher_data
{
   int32_t held_i[2];
   int32_t quant_scale_i;
   int32_t mix_q16;
   unsigned hold_count;
   unsigned downsample;
   float mix;
   float quant_scale;
   float held[2];
};

/* Symmetric round-half-away quantisation of [-1, 1] onto
 * quant_scale steps per unit. */
static float bitcrusher_quantize(float x, float scale)
{
   if (x < -1.0f)
      x = -1.0f;
   else if (x > 1.0f)
      x = 1.0f;
   if (x >= 0.0f)
      return (float)floor(x * scale + 0.5f) / scale;
   return (float)ceil(x * scale - 0.5f) / scale;
}

/* int16 counterpart: the same quantiser expressed on the s16 scale,
 * round-half-away-from-zero throughout. */
static int32_t bitcrusher_quantize_i16(int32_t s, int32_t scale)
{
   int64_t num = (int64_t)s * scale;
   int64_t q   = (num >= 0) ? ((num + 16384) >> 15)
                            : -(((-num) + 16384) >> 15);
   int64_t y;

   num = q * 32768;
   y   = (num >= 0) ? ((num + scale / 2) / scale)
                    : -(((-num) + scale / 2) / scale);
   if      (y >  32767)
      y =  32767;
   else if (y < -32768)
      y = -32768;
   return (int32_t)y;
}

static void bitcrusher_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   struct bitcrusher_data *bc = (struct bitcrusher_data*)data;
   float *samples             = input->samples;

   output->samples            = samples;
   output->frames             = input->frames;

   for (i = 0; i < input->frames; i++, samples += 2)
   {
      float in_l = samples[0];
      float in_r = samples[1];

      if (bc->hold_count == 0)
      {
         bc->held[0] = bitcrusher_quantize(in_l, bc->quant_scale);
         bc->held[1] = bitcrusher_quantize(in_r, bc->quant_scale);
      }

      samples[0] = in_l + (bc->held[0] - in_l) * bc->mix;
      samples[1] = in_r + (bc->held[1] - in_r) * bc->mix;

      if (++bc->hold_count >= bc->downsample)
         bc->hold_count = 0;
   }
}

static int16_t bitcrusher_mix_i16(int32_t in, int32_t held, int32_t mix_q16)
{
   int64_t d = (int64_t)(held - in) * mix_q16;
   int64_t v;
   d = (d >= 0) ? ((d + 32768) >> 16) : -(((-d) + 32768) >> 16);
   v = in + d;
   if      (v >  32767)
      v =  32767;
   else if (v < -32768)
      v = -32768;
   return (int16_t)v;
}

static void bitcrusher_process_i16(void *data,
      struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   unsigned i;
   struct bitcrusher_data *bc = (struct bitcrusher_data*)data;
   int16_t *samples           = input->samples;

   output->samples            = samples;
   output->frames             = input->frames;

   for (i = 0; i < input->frames; i++, samples += 2)
   {
      int32_t in_l = samples[0];
      int32_t in_r = samples[1];

      if (bc->hold_count == 0)
      {
         bc->held_i[0] = bitcrusher_quantize_i16(in_l, bc->quant_scale_i);
         bc->held_i[1] = bitcrusher_quantize_i16(in_r, bc->quant_scale_i);
      }

      samples[0] = bitcrusher_mix_i16(in_l, bc->held_i[0], bc->mix_q16);
      samples[1] = bitcrusher_mix_i16(in_r, bc->held_i[1], bc->mix_q16);

      if (++bc->hold_count >= bc->downsample)
         bc->hold_count = 0;
   }
}

static void bitcrusher_free(void *data)
{
   free(data);
}

static void *bitcrusher_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   int bits, downsample;
   float mix;
   struct bitcrusher_data *bc = (struct bitcrusher_data*)
      calloc(1, sizeof(*bc));

   (void)info;

   if (!bc)
      return NULL;

   config->get_int(userdata, "bits", &bits, 8);
   config->get_int(userdata, "downsample", &downsample, 4);
   config->get_float(userdata, "drywet", &mix, 1.0f);

   if (bits < 2)
      bits = 2;
   else if (bits > 24)
      bits = 24;
   if (downsample < 1)
      downsample = 1;
   else if (downsample > 64)
      downsample = 64;
   if (mix < 0.0f)
      mix = 0.0f;
   else if (mix > 1.0f)
      mix = 1.0f;

   bc->downsample    = (unsigned)downsample;
   bc->mix           = mix;
   bc->mix_q16       = (int32_t)floor((double)mix * 65536.0 + 0.5);
   bc->quant_scale_i = (int32_t)((1UL << (unsigned)(bits - 1)) - 1UL);
   bc->quant_scale   = (float)bc->quant_scale_i;

   return bc;
}

static const struct dspfilter_implementation bitcrusher_plug = {
   bitcrusher_init,
   bitcrusher_process,
   bitcrusher_free,

   DSPFILTER_API_VERSION,
   "Bitcrusher",
   "bitcrusher",

   bitcrusher_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation bitcrusher_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   (void)mask;
   return &bitcrusher_plug;
}

#undef dspfilter_get_implementation
