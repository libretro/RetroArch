/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (iir.c).
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

#define sqr(a) ((a) * (a))

/* filter types */
enum IIRFilter
{
   LPF,        /* low pass filter */
   HPF,        /* High pass filter */
   BPCSGF,     /* band pass filter 1 */
   BPZPGF,     /* band pass filter 2 */
   APF,        /* Allpass filter*/
   NOTCH,      /* Notch Filter */
   RIAA_phono, /* RIAA record/tape deemphasis */
   PEQ,        /* Peaking band EQ filter */
   BBOOST,     /* Bassboost filter */
   LSH,        /* Low shelf filter */
   HSH,        /* High shelf filter */
   RIAA_CD     /* CD de-emphasis */
};

struct iir_data
{
   float b0, b1, b2;
   float a0, a1, a2;

   /* Deterministic int16 path: normalized coefficients (b/a0, a/a0) in Q24,
    * and per-channel state.  The feedback history yn1/yn2 is kept in Q16
    * int64 so the recursion runs at higher-than-int16 precision (no
    * in-loop quantization noise) and cannot overflow on boost/overshoot. */
   int32_t nb0_q, nb1_q, nb2_q, na1_q, na2_q;

   struct
   {
      float xn1, xn2;
      float yn1, yn2;
   } l, r;

   struct
   {
      int32_t xn1, xn2;
      int64_t yn1, yn2;
   } li, ri;
};

static void iir_free(void *data)
{
   free(data);
}

static void iir_process(void *data, struct dspfilter_output *output,
      const struct dspfilter_input *input)
{
   unsigned i;
   struct iir_data *iir = (struct iir_data*)data;
   float *out           = output->samples;

   float b0             = iir->b0;
   float b1             = iir->b1;
   float b2             = iir->b2;
   float a0             = iir->a0;
   float a1             = iir->a1;
   float a2             = iir->a2;

   float xn1_l          = iir->l.xn1;
   float xn2_l          = iir->l.xn2;
   float yn1_l          = iir->l.yn1;
   float yn2_l          = iir->l.yn2;

   float xn1_r          = iir->r.xn1;
   float xn2_r          = iir->r.xn2;
   float yn1_r          = iir->r.yn1;
   float yn2_r          = iir->r.yn2;

   output->samples      = input->samples;
   output->frames       = input->frames;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      float in_l = out[0];
      float in_r = out[1];

      float l    = (b0 * in_l + b1 * xn1_l + b2 * xn2_l - a1 * yn1_l - a2 * yn2_l) / a0;
      float r    = (b0 * in_r + b1 * xn1_r + b2 * xn2_r - a1 * yn1_r - a2 * yn2_r) / a0;

      xn2_l      = xn1_l;
      xn1_l      = in_l;
      yn2_l      = yn1_l;
      yn1_l      = l;

      xn2_r      = xn1_r;
      xn1_r      = in_r;
      yn2_r      = yn1_r;
      yn1_r      = r;

      out[0]     = l;
      out[1]     = r;
   }

   iir->l.xn1 = xn1_l;
   iir->l.xn2 = xn2_l;
   iir->l.yn1 = yn1_l;
   iir->l.yn2 = yn2_l;

   iir->r.xn1 = xn1_r;
   iir->r.xn2 = xn2_r;
   iir->r.yn1 = yn1_r;
   iir->r.yn2 = yn2_r;
}

/* Deterministic int16 Direct Form I biquad.  Forward terms accumulate to Q40
 * (int64), the a0-normalized Q24 coefficients multiply the Q16 feedback
 * history, and a single symmetric rounding produces the int16 output while
 * the Q16 history is retained for the next iteration. */
static void iir_process_i16(void *data,
      struct dspfilter_output_i16 *output,
      const struct dspfilter_input_i16 *input)
{
   unsigned i;
   struct iir_data *iir = (struct iir_data*)data;
   int16_t *out;
   int32_t nb0   = iir->nb0_q;
   int32_t nb1   = iir->nb1_q;
   int32_t nb2   = iir->nb2_q;
   int32_t na1   = iir->na1_q;
   int32_t na2   = iir->na2_q;
   int32_t xn1_l = iir->li.xn1, xn2_l = iir->li.xn2;
   int64_t yn1_l = iir->li.yn1, yn2_l = iir->li.yn2;
   int32_t xn1_r = iir->ri.xn1, xn2_r = iir->ri.xn2;
   int64_t yn1_r = iir->ri.yn1, yn2_r = iir->ri.yn2;

   output->samples = input->samples;
   output->frames  = input->frames;
   out             = output->samples;

   for (i = 0; i < input->frames; i++, out += 2)
   {
      int32_t in_l = out[0];
      int32_t in_r = out[1];
      int64_t fwd, acc;
      int32_t v;

      fwd = (int64_t)nb0 * in_l + (int64_t)nb1 * xn1_l + (int64_t)nb2 * xn2_l;
      acc = (fwd << 16) - (int64_t)na1 * yn1_l - (int64_t)na2 * yn2_l;
      v   = (acc >= 0) ?  (int32_t)(( acc + (1LL << 39)) >> 40)
                       : -(int32_t)((-acc + (1LL << 39)) >> 40);
      if      (v >  32767) v =  32767;
      else if (v < -32768) v = -32768;
      xn2_l = xn1_l; xn1_l = in_l;
      yn2_l = yn1_l;
      yn1_l = (acc >= 0) ?  (( acc + (1LL << 23)) >> 24)
                         : -((-acc + (1LL << 23)) >> 24);
      out[0] = (int16_t)v;

      fwd = (int64_t)nb0 * in_r + (int64_t)nb1 * xn1_r + (int64_t)nb2 * xn2_r;
      acc = (fwd << 16) - (int64_t)na1 * yn1_r - (int64_t)na2 * yn2_r;
      v   = (acc >= 0) ?  (int32_t)(( acc + (1LL << 39)) >> 40)
                       : -(int32_t)((-acc + (1LL << 39)) >> 40);
      if      (v >  32767) v =  32767;
      else if (v < -32768) v = -32768;
      xn2_r = xn1_r; xn1_r = in_r;
      yn2_r = yn1_r;
      yn1_r = (acc >= 0) ?  (( acc + (1LL << 23)) >> 24)
                         : -((-acc + (1LL << 23)) >> 24);
      out[1] = (int16_t)v;
   }

   iir->li.xn1 = xn1_l; iir->li.xn2 = xn2_l;
   iir->li.yn1 = yn1_l; iir->li.yn2 = yn2_l;
   iir->ri.xn1 = xn1_r; iir->ri.xn2 = xn2_r;
   iir->ri.yn1 = yn1_r; iir->ri.yn2 = yn2_r;
}

#define CHECK(x) if (strcmp(str, #x) == 0) return x
static enum IIRFilter str_to_type(const char *str)
{
   CHECK(LPF);
   CHECK(HPF);
   CHECK(BPCSGF);
   CHECK(BPZPGF);
   CHECK(APF);
   CHECK(NOTCH);
   CHECK(RIAA_phono);
   CHECK(PEQ);
   CHECK(BBOOST);
   CHECK(LSH);
   CHECK(HSH);
   CHECK(RIAA_CD);

   return LPF; /* Fallback. */
}
#undef CHECK

static void make_poly_from_roots(
      const double *roots, unsigned num_roots, float *poly)
{
   unsigned i, j;

   poly[0] = 1;
   poly[1] = -roots[0];
   memset(poly + 2, 0, (num_roots + 1 - 2) * sizeof(*poly));

   for (i = 1; i < num_roots; i++)
      for (j = num_roots; j > 0; j--)
         poly[j] -= poly[j - 1] * roots[i];
}

static void iir_filter_init(struct iir_data *iir,
      float sample_rate, float freq, float qual, float gain, enum IIRFilter filter_type)
{
	double omega = 2.0 * M_PI * freq / sample_rate;
   double cs    = cos(omega);
   double sn    = sin(omega);
   double a1pha = sn / (2.0 * qual);
   double A     = exp(log(10.0) * gain / 40.0);
   double beta  = sqrt(A + A);

   float b0     = 0.0, b1 = 0.0, b2 = 0.0, a0 = 0.0, a1 = 0.0, a2 = 0.0;

   /* Set up filter coefficients according to type */
   switch (filter_type)
   {
      case LPF:
         b0 =  (1.0 - cs) / 2.0;
         b1 =   1.0 - cs ;
         b2 =  (1.0 - cs) / 2.0;
         a0 =   1.0 + a1pha;
         a1 =  -2.0 * cs;
         a2 =   1.0 - a1pha;
         break;
      case HPF:
         b0 =  (1.0 + cs) / 2.0;
         b1 = -(1.0 + cs);
         b2 =  (1.0 + cs) / 2.0;
         a0 =   1.0 + a1pha;
         a1 =  -2.0 * cs;
         a2 =   1.0 - a1pha;
         break;
      case APF:
         b0 =  1.0 - a1pha;
         b1 = -2.0 * cs;
         b2 =  1.0 + a1pha;
         a0 =  1.0 + a1pha;
         a1 = -2.0 * cs;
         a2 =  1.0 - a1pha;
         break;
      case BPZPGF:
         b0 =  a1pha;
         b1 =  0.0;
         b2 = -a1pha;
         a0 =  1.0 + a1pha;
         a1 = -2.0 * cs;
         a2 =  1.0 - a1pha;
         break;
      case BPCSGF:
         b0 =  sn / 2.0;
         b1 =  0.0;
         b2 = -sn / 2.0;
         a0 =  1.0 + a1pha;
         a1 = -2.0 * cs;
         a2 =  1.0 - a1pha;
         break;
      case NOTCH:
         b0 =  1.0;
         b1 = -2.0 * cs;
         b2 =  1.0;
         a0 =  1.0 + a1pha;
         a1 = -2.0 * cs;
         a2 =  1.0 - a1pha;
         break;
      case RIAA_phono: /* http://www.dsprelated.com/showmessage/73300/3.php */
      {
         double y, b_re, a_re, b_im, a_im, g;
         float b[3] = {0.0f};
         float a[3] = {0.0f};

         if ((int)sample_rate == 44100)
         {
            static const double zeros[] = {-0.2014898, 0.9233820};
            static const double poles[] = {0.7083149, 0.9924091};
            make_poly_from_roots(zeros, 2, b);
            make_poly_from_roots(poles, 2, a);
         }
         else if ((int)sample_rate == 48000)
         {
            static const double zeros[] = {-0.1766069, 0.9321590};
            static const double poles[] = {0.7396325, 0.9931330};
            make_poly_from_roots(zeros, 2, b);
            make_poly_from_roots(poles, 2, a);
         }
         else if ((int)sample_rate == 88200)
         {
            static const double zeros[] = {-0.1168735, 0.9648312};
            static const double poles[] = {0.8590646, 0.9964002};
            make_poly_from_roots(zeros, 2, b);
            make_poly_from_roots(poles, 2, a);
         }
         else if ((int)sample_rate == 96000)
         {
            static const double zeros[] = {-0.1141486, 0.9676817};
            static const double poles[] = {0.8699137, 0.9966946};
            make_poly_from_roots(zeros, 2, b);
            make_poly_from_roots(poles, 2, a);
         }

         b0    = b[0];
         b1    = b[1];
         b2    = b[2];
         a0    = a[0];
         a1    = a[1];
         a2    = a[2];

         /* Normalise to 0dB at 1kHz (Thanks to Glenn Davis) */
         y     = 2.0 * M_PI * 1000.0 / sample_rate;
         b_re  = b0 + b1 * cos(-y) + b2 * cos(-2.0 * y);
         a_re  = a0 + a1 * cos(-y) + a2 * cos(-2.0 * y);
         b_im  = b1 * sin(-y) + b2 * sin(-2.0 * y);
         a_im  = a1 * sin(-y) + a2 * sin(-2.0 * y);
         g     = 1.0 / sqrt((sqr(b_re) + sqr(b_im)) / (sqr(a_re) + sqr(a_im)));
         b0   *= g; b1 *= g; b2 *= g;
         break;
      }
      case PEQ:
         b0 =  1.0 + a1pha * A;
         b1 = -2.0 * cs;
         b2 =  1.0 - a1pha * A;
         a0 =  1.0 + a1pha / A;
         a1 = -2.0 * cs;
         a2 =  1.0 - a1pha / A;
         break;
      case BBOOST:
         beta = sqrt((A * A + 1) / 1.0 - (pow((A - 1), 2)));
         b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
         b1 = 2 * A * ((A - 1) - (A + 1) * cs);
         b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
         a0 = ((A + 1) + (A - 1) * cs + beta * sn);
         a1 = -2 * ((A - 1) + (A + 1) * cs);
         a2 = (A + 1) + (A - 1) * cs - beta * sn;
         break;
      case LSH:
         b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
         b1 = 2 * A * ((A - 1) - (A + 1) * cs);
         b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
         a0 = (A + 1) + (A - 1) * cs + beta * sn;
         a1 = -2 * ((A - 1) + (A + 1) * cs);
         a2 = (A + 1) + (A - 1) * cs - beta * sn;
         break;
      case RIAA_CD:
         omega = 2.0 * M_PI * 5283.0 / sample_rate;
         cs = cos(omega);
         sn = sin(omega);
         a1pha = sn / (2.0 * 0.4845);
         A = exp(log(10.0) * -9.477 / 40.0);
         beta = sqrt(A + A);
         (void)a1pha;
      case HSH:
         b0 = A * ((A + 1.0) + (A - 1.0) * cs + beta * sn);
         b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
         b2 = A * ((A + 1.0) + (A - 1.0) * cs - beta * sn);
         a0 = (A + 1.0) - (A - 1.0) * cs + beta * sn;
         a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cs);
         a2 = (A + 1.0) - (A - 1.0) * cs - beta * sn;
         break;
      default:
         break;
   }

   iir->b0 = b0;
   iir->b1 = b1;
   iir->b2 = b2;
   iir->a0 = a0;
   iir->a1 = a1;
   iir->a2 = a2;
}

static void *iir_init(const struct dspfilter_info *info,
      const struct dspfilter_config *config, void *userdata)
{
   float freq, qual, gain;
   enum IIRFilter filter  = LPF;
   char           *type   = NULL;
   struct iir_data *iir   = (struct iir_data*)calloc(1, sizeof(*iir));
   if (!iir)
      return NULL;

   config->get_float(userdata, "frequency", &freq, 1024.0f);
   config->get_float(userdata, "quality", &qual, 0.707f);
   config->get_float(userdata, "gain", &gain, 0.0f);

   config->get_string(userdata, "type", &type, "LPF");

   filter = str_to_type(type);
   config->free(type);

   iir_filter_init(iir, info->input_rate, freq, qual, gain, filter);

   /* Precompute a0-normalized Q24 coefficients for the int16 path. */
   {
      double inv_a0 = (iir->a0 != 0.0f) ? 1.0 / (double)iir->a0 : 0.0;
      iir->nb0_q = (int32_t)floor((double)iir->b0 * inv_a0 * 16777216.0 + 0.5);
      iir->nb1_q = (int32_t)floor((double)iir->b1 * inv_a0 * 16777216.0 + 0.5);
      iir->nb2_q = (int32_t)floor((double)iir->b2 * inv_a0 * 16777216.0 + 0.5);
      iir->na1_q = (int32_t)floor((double)iir->a1 * inv_a0 * 16777216.0 + 0.5);
      iir->na2_q = (int32_t)floor((double)iir->a2 * inv_a0 * 16777216.0 + 0.5);
   }
   return iir;
}

static const struct dspfilter_implementation iir_plug = {
   iir_init,
   iir_process,
   iir_free,

   DSPFILTER_API_VERSION,
   "IIR",
   "iir",

   iir_process_i16,
};

#ifdef HAVE_FILTERS_BUILTIN
#define dspfilter_get_implementation iir_dspfilter_get_implementation
#endif

const struct dspfilter_implementation *dspfilter_get_implementation(dspfilter_simd_mask_t mask)
{
   return &iir_plug;
}

#undef dspfilter_get_implementation
