/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Brad Miller
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "rarch_dsp.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#ifndef M_PI
#define M_PI		3.1415926535897932384626433832795
#endif

#define sqr(a) ((a) * (a))

#ifdef RARCH_INTERNAL
#define rarch_dsp_plugin_init  iir_dsp_plugin_init
#endif

struct iir_filter
{
#ifdef __SSE2__
   __m128 fir_coeff[2];
   __m128 fir_buf[2];

   __m128 iir_coeff;
   __m128 iir_buf;
#endif
	float pf_freq, pf_qfact, pf_gain;
	int type, pf_q_is_bandwidth; 
	float xn1,xn2,yn1,yn2;
	float omega, cs, a1pha, beta, b0, b1, b2, a0, a1,a2, A, sn;
};

struct iir_filter_data
{
   struct iir_filter iir_l __attribute__((aligned(16)));
   struct iir_filter iir_r __attribute__((aligned(16)));
   float buf[4096] __attribute__((aligned(16)));
   int rate;
   unsigned type;
};

/* filter types */
enum
{
   LPF, /* low pass filter */
   HPF, /* High pass filter */
   BPCSGF,/* band pass filter 1 */
   BPZPGF,/* band pass filter 2 */
   APF, /* Allpass filter*/
   NOTCH, /* Notch Filter */
   RIAA_phono, /* RIAA record/tape deemphasis */
   PEQ, /* Peaking band EQ filter */
   BBOOST, /* Bassboost filter */
   LSH, /* Low shelf filter */
   HSH, /* High shelf filter */
   RIAA_CD /* CD de-emphasis */
};

//lynched from SoX >w>
static void iir_make_poly_from_roots(double const * roots, size_t num_roots, float * poly)
{
	size_t i, j;
	poly[0] = 1;
	poly[1] = -roots[0];
	memset(poly + 2, 0, (num_roots + 1 - 2) * sizeof(*poly));
	for (i = 1; i < num_roots; ++i)
		for (j = num_roots; j > 0; --j)
			poly[j] -= poly[j - 1] * roots[i];
}

static void iir_init(void *data, int samplerate, int filter_type)
{
   struct iir_filter *iir = (struct iir_filter*)data;

   if (!iir)
      return;

   iir->xn1=0;
   iir->xn2=0;
   iir->yn1=0;
   iir->yn2=0;
   iir->omega = 2 * M_PI * iir->pf_freq/samplerate;
   iir->cs = cos(iir->omega);
   iir->sn = sin(iir->omega);
   iir->a1pha = iir->sn / (2.0 * iir->pf_qfact);
   iir->A = exp(log(10.0) * iir->pf_gain  / 40);
   iir->beta = sqrt(iir->A + iir->A);
   //Set up filter coefficients according to type
   switch (filter_type)
   {
      case LPF:
         iir->b0 =  (1.0 - iir->cs) / 2.0 ;
         iir->b1 =   1.0 - iir->cs ;
         iir->b2 =  (1.0 - iir->cs) / 2.0 ;
         iir->a0 =   1.0 + iir->a1pha ;
         iir->a1 =  -2.0 * iir->cs ;
         iir->a2 =   1.0 - iir->a1pha ;
         break;
      case HPF:
         iir->b0 =  (1.0 + iir->cs) / 2.0 ;
         iir->b1 = -(1.0 + iir->cs) ;
         iir->b2 =  (1.0 + iir->cs) / 2.0 ;
         iir->a0 =   1.0 + iir->a1pha ;
         iir->a1 =  -2.0 * iir->cs ;
         iir->a2 =   1.0 - iir->a1pha ;
         break;
      case APF:
         iir->b0 = 1.0 - iir->a1pha;
         iir->b1 = -2.0 * iir->cs;
         iir->b2 = 1.0 + iir->a1pha;
         iir->a0 = 1.0 + iir->a1pha;
         iir->a1 = -2.0 * iir->cs;
         iir->a2 = 1.0 - iir->a1pha;
         break;
      case BPZPGF:
         iir->b0 =   iir->a1pha ;
         iir->b1 =   0.0 ;
         iir->b2 =  -iir->a1pha ;
         iir->a0 =   1.0 + iir->a1pha ;
         iir->a1 =  -2.0 * iir->cs ;
         iir->a2 =   1.0 - iir->a1pha ;
         break;
      case BPCSGF:
         iir->b0=iir->sn/2.0;
         iir->b1=0.0;
         iir->b2=-iir->sn/2;
         iir->a0=1.0+iir->a1pha;
         iir->a1=-2.0*iir->cs;
         iir->a2=1.0-iir->a1pha;
         break;
      case NOTCH: 
         iir->b0 = 1;
         iir->b1 = -2 * iir->cs;
         iir->b2 = 1;
         iir->a0 = 1 + iir->a1pha;
         iir->a1 = -2 * iir->cs;
         iir->a2 = 1 - iir->a1pha;
         break;
      case RIAA_phono: /* http://www.dsprelated.com/showmessage/73300/3.php */
         if (samplerate == 44100) {
            static const double zeros[] = {-0.2014898, 0.9233820};
            static const double poles[] = {0.7083149, 0.9924091};
            iir_make_poly_from_roots(zeros, (size_t)2, &iir->b0);
            iir_make_poly_from_roots(poles, (size_t)2, &iir->a0);
         }
         else if (samplerate == 48000) {
            static const double zeros[] = {-0.1766069, 0.9321590};
            static const double poles[] = {0.7396325, 0.9931330};
            iir_make_poly_from_roots(zeros, (size_t)2, &iir->b0);
            iir_make_poly_from_roots(poles, (size_t)2, &iir->a0);
         }
         else if (samplerate == 88200) {
            static const double zeros[] = {-0.1168735, 0.9648312};
            static const double poles[] = {0.8590646, 0.9964002};
            iir_make_poly_from_roots(zeros, (size_t)2, &iir->b0);
            iir_make_poly_from_roots(poles, (size_t)2, &iir->a0);
         }
         else if (samplerate == 96000) {
            static const double zeros[] = {-0.1141486, 0.9676817};
            static const double poles[] = {0.8699137, 0.9966946};
            iir_make_poly_from_roots(zeros, (size_t)2, &iir->b0);
            iir_make_poly_from_roots(poles, (size_t)2, &iir->a0);
         }
         { /* Normalise to 0dB at 1kHz (Thanks to Glenn Davis) */
            double y = 2 * M_PI * 1000 / samplerate ;
            double b_re = iir->b0 + iir->b1 * cos(-y) + iir->b2 * cos(-2 * y);
            double a_re = iir->a0 + iir->a1 * cos(-y) + iir->a2 * cos(-2 * y);
            double b_im = iir->b1 * sin(-y) + iir->b2 * sin(-2 * y);
            double a_im = iir->a1 * sin(-y) + iir->a2 * sin(-2 * y);
            double g = 1 / sqrt((sqr(b_re) + sqr(b_im)) / (sqr(a_re) + sqr(a_im)));
            iir->b0 *= g;
            iir->b1 *= g;
            iir->b2 *= g;
         }
         break;
      case PEQ: 
         iir->b0 =   1 + iir->a1pha * iir->A ;
         iir->b1 =  -2 * iir->cs ;
         iir->b2 =   1 - iir->a1pha * iir->A ;
         iir->a0 =   1 + iir->a1pha / iir->A ;
         iir->a1 =  -2 * iir->cs ;
         iir->a2 =   1 - iir->a1pha / iir->A ;
         break; 
      case BBOOST:       
         iir->beta = sqrt((iir->A * iir->A + 1) / 1.0 - (pow((iir->A - 1), 2)));
         iir->b0 = iir->A * ((iir->A + 1) - (iir->A - 1) * iir->cs + iir->beta * iir->sn);
         iir->b1 = 2 * iir->A * ((iir->A - 1) - (iir->A + 1) * iir->cs);
         iir->b2 = iir->A * ((iir->A + 1) - (iir->A - 1) * iir->cs - iir->beta * iir->sn);
         iir->a0 = ((iir->A + 1) + (iir->A - 1) * iir->cs + iir->beta * iir->sn);
         iir->a1 = -2 * ((iir->A - 1) + (iir->A + 1) * iir->cs);
         iir->a2 = (iir->A + 1) + (iir->A - 1) * iir->cs - iir->beta * iir->sn;
         break;
      case LSH:
         iir->b0 = iir->A * ((iir->A + 1) - (iir->A - 1) * iir->cs + iir->beta * iir->sn);
         iir->b1 = 2 * iir->A * ((iir->A - 1) - (iir->A + 1) * iir->cs);
         iir->b2 = iir->A * ((iir->A + 1) - (iir->A - 1) * iir->cs - iir->beta * iir->sn);
         iir->a0 = (iir->A + 1) + (iir->A - 1) * iir->cs + iir->beta * iir->sn;
         iir->a1 = -2 * ((iir->A - 1) + (iir->A + 1) * iir->cs);
         iir->a2 = (iir->A + 1) + (iir->A - 1) * iir->cs - iir->beta * iir->sn;
         break;
      case RIAA_CD:
         iir->omega = 2 * M_PI * 5283 / samplerate;
         iir->cs = cos(iir->omega);
         iir->sn = sin(iir->omega);
         iir->a1pha = iir->sn / (2.0 * 0.4845);
         iir->A = exp(log(10.0) * -9.477  / 40);
         iir->beta = sqrt(iir->A + iir->A);
      case HSH:
         iir->b0 = iir->A * ((iir->A + 1) + (iir->A - 1) * iir->cs + iir->beta * iir->sn);
         iir->b1 = -2 * iir->A * ((iir->A - 1) + (iir->A + 1) * iir->cs);
         iir->b2 = iir->A * ((iir->A + 1) + (iir->A - 1) * iir->cs - iir->beta * iir->sn);
         iir->a0 = (iir->A + 1) - (iir->A - 1) * iir->cs + iir->beta * iir->sn;
         iir->a1 = 2 * ((iir->A - 1) - (iir->A + 1) * iir->cs);
         iir->a2 = (iir->A + 1) - (iir->A - 1) * iir->cs - iir->beta * iir->sn;
         break;
      default:
         break;
   }

#ifdef __SSE2__
   iir->fir_coeff[0] = _mm_set_ps(iir->b1 / iir->a0, iir->b1 / iir->a0, iir->b0 / iir->a0, iir->b0 / iir->a0);
   iir->fir_coeff[1] = _mm_set_ps(0.0f, 0.0f, iir->b2 / iir->a0, iir->b2 / iir->a0);
   iir->iir_coeff = _mm_set_ps(-iir->a2 / iir->a0, -iir->a2 / iir->a0, -iir->a1 / iir->a0, -iir->a1 / iir->a0);
#endif
}

#ifdef __SSE2__
static void iir_process_batch(void *data, float *out, const float *in, unsigned frames)
{
   struct iir_filter *iir = (struct iir_filter*)data;

   __m128 fir_coeff[2] = { iir->fir_coeff[0], iir->fir_coeff[1] };
   __m128 iir_coeff    = iir->iir_coeff;
   __m128 fir_buf[2]   = { iir->fir_buf[0],   iir->fir_buf[1] }; 
   __m128 iir_buf      = iir->iir_buf;

   for (unsigned i = 0; (i + 4) <= (2 * frames); in += 4, i += 4, out += 4)
   {
      __m128 input = _mm_loadu_ps(in);

      fir_buf[1] = _mm_shuffle_ps(fir_buf[0], fir_buf[1],  _MM_SHUFFLE(1, 0, 3, 2));
      fir_buf[0] = _mm_shuffle_ps(input, fir_buf[0], _MM_SHUFFLE(1, 0, 1, 0));

      __m128 res[3] = {
         _mm_mul_ps(fir_buf[0], fir_coeff[0]),
         _mm_mul_ps(fir_buf[1], fir_coeff[1]),
         _mm_mul_ps(iir_buf, iir_coeff),
      };

      __m128 result = _mm_add_ps(_mm_add_ps(res[0], res[1]), res[2]);
      result = _mm_add_ps(result, _mm_shuffle_ps(result, result, _MM_SHUFFLE(0, 0, 3, 2)));

      iir_buf = _mm_shuffle_ps(result, iir_buf, _MM_SHUFFLE(1, 0, 1, 0));

      fir_buf[1] = _mm_shuffle_ps(fir_buf[0], fir_buf[1],  _MM_SHUFFLE(1, 0, 3, 2));
      fir_buf[0] = _mm_shuffle_ps(input, fir_buf[0], _MM_SHUFFLE(1, 0, 3, 2));

      res[0] = _mm_mul_ps(fir_buf[0], fir_coeff[0]);
      res[1] = _mm_mul_ps(fir_buf[1], fir_coeff[1]);
      res[2] = _mm_mul_ps(iir_buf, iir_coeff);

      __m128 result2 = _mm_add_ps(_mm_add_ps(res[0], res[1]), res[2]);
      result2 = _mm_add_ps(result2, _mm_shuffle_ps(result2, result2, _MM_SHUFFLE(0, 0, 3, 2)));

      iir_buf = _mm_shuffle_ps(result2, iir_buf, _MM_SHUFFLE(1, 0, 1, 0));

      _mm_store_ps(out, _mm_shuffle_ps(result, result2, _MM_SHUFFLE(1, 0, 1, 0)));
   }

   iir->fir_buf[0] = fir_buf[0];
   iir->fir_buf[1] = fir_buf[1];
   iir->iir_buf    = iir_buf;
}
#else
static float iir_process(void *data, float samp)
{
   struct iir_filter *iir = (struct iir_filter*)data;

   float out, in = 0;
   in = samp;
   out = (iir->b0 * in + iir->b1 * iir->xn1 + iir->b2 * iir->xn2 - iir->a1 * iir->yn1 - iir->a2 * iir->yn2) / iir->a0;
   iir->xn2 = iir->xn1;
   iir->xn1 = in;
   iir->yn2 = iir->yn1;
   iir->yn1 = out;
   return out;
}
#endif

static void * iir_dsp_init(const rarch_dsp_info_t *info)
{
   struct iir_filter_data *iir = (struct iir_filter_data*)calloc(1, sizeof(*iir));

   if (!iir)
      return NULL;

   iir->rate = info->input_rate;
   iir->type = 0;
   iir->iir_l.pf_freq = 1024;
   iir->iir_l.pf_qfact = 0.707;
   iir->iir_l.pf_gain = 0;
   iir_init(&iir->iir_l, info->input_rate, 0);
   iir->iir_r.pf_freq = 1024;
   iir->iir_r.pf_qfact = 0.707;
   iir->iir_r.pf_gain = 0;
   iir_init(&iir->iir_r, info->input_rate, 0);

   return iir;
}

static void iir_dsp_process(void *data, rarch_dsp_output_t *output,
      const rarch_dsp_input_t *input)
{
   struct iir_filter_data *iir = (struct iir_filter_data*)data;

   output->samples = iir->buf;

#ifdef __SSE2__
   iir_process_batch(&iir->iir_l, iir->buf, input->samples, input->frames);
#else
   int num_samples = input->frames * 2;
   for (int i = 0; i<num_samples;)
	{
		iir->buf[i] = iir_process(&iir->iir_l, input->samples[i]);
		i++;
		iir->buf[i] = iir_process(&iir->iir_r, input->samples[i]);
		i++;
	}
#endif

	output->frames = input->frames;
}

static void iir_dsp_free(void *data)
{
   struct iir_filter_data *iir = (struct iir_filter_data*)data;

   if (iir)
      free(iir);
}

static void iir_dsp_config(void* data)
{
}

const rarch_dsp_plugin_t dsp_plug = {
	iir_dsp_init,
	iir_dsp_process,
	iir_dsp_free,
	RARCH_DSP_API_VERSION,
	iir_dsp_config,
#ifdef __SSE2__
	"IIR (SSE2)",
#else
	"IIR",
#endif
   NULL
};


const rarch_dsp_plugin_t *rarch_dsp_plugin_init(void)
{
   return &dsp_plug;
}

#ifdef RARCH_INTERNAL
#undef rarch_dsp_plugin_init
#endif
