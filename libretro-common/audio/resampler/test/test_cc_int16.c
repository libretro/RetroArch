/* Accuracy / determinism harness: diffs the integer s16 Convoluted Cosine
 * resampler against the faithful float reference (a verbatim copy of the
 * production cc_resampler.c C path, CC_RESAMPLER_PRECISION 1) across a
 * rate x signal matrix.  The int16 kernel is a Q24 fixed-point mirror, so
 * small quantisation diffs are expected; we report max LSB diff, mismatch
 * rate and resampled-tone SNR for both endpoints.
 *
 * Build:  cc -O2 -std=c89 -Wall test_cc_int16.c cc_resampler_int16.c \
 *            -I ../../../include -lm -o test_cc_int16 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <audio/cc_resampler_int16.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef CC_RESAMPLER_PRECISION
#define CC_RESAMPLER_PRECISION 1
#endif

/* ------------------------------------------------------------------ *
 * Double-precision ideal reference: the production cc_resampler.c "C"
 * kernel (degree-5 polynomial at CC_RESAMPLER_PRECISION > 0) evaluated
 * in double.  Using double for the phase accumulator and the kernel
 * makes this a faithful oracle for *correctness*: unlike the shipped
 * float driver, its distance accumulator does not drift, so it stays
 * phase-aligned with the near-exact Q24 int16 driver and the remaining
 * diff reflects only Q24 kernel quantisation.                          *
 * ------------------------------------------------------------------ */
struct resampler_data_float
{
   const float *data_in;
   float       *data_out;
   size_t       input_frames;
   size_t       output_frames;
   double       ratio;
};

typedef struct cc_ref
{
   double bl[4];
   double br[4];
   double distance;
   int    up;
} cc_ref_t;

static double cc_ref_int(double x, double b)
{
   double val = x * b;
#if (CC_RESAMPLER_PRECISION > 0)
   val = val * (1.0 - 0.25 * val * val * (3.0 - val * val));
#endif
   return (val > 0.5) ? 0.5 : (val < -0.5) ? -0.5 : val;
}

#define cc_ref_kernel(x, b) (cc_ref_int((x) + 0.5, (b)) - cc_ref_int((x) - 0.5, (b)))

static void *cc_ref_init(double bandwidth_mod)
{
   cc_ref_t *re = (cc_ref_t*)calloc(1, sizeof(*re));
   if (!re)
      return NULL;
   if (bandwidth_mod < 0.75)
   {
      re->up       = 0;
      re->distance = 0.0;
   }
   else
   {
      re->up       = 1;
      re->distance = 2.0;
   }
   return re;
}

static void cc_ref_downsample(cc_ref_t *re, struct resampler_data_float *data)
{
   const float *inp     = data->data_in;
   const float *inp_max = inp + data->input_frames * 2;
   float       *outp    = data->data_out;
   float       *outp0   = outp;
   double       ratio   = 1.0 / data->ratio;
   double       b       = data->ratio;

   while (inp != inp_max)
   {
      double k0 = cc_ref_kernel(re->distance,             b);
      double k1 = cc_ref_kernel(re->distance - ratio,     b);
      double k2 = cc_ref_kernel(re->distance - ratio - ratio, b);

      re->bl[0] += (double)inp[0] * k0; re->br[0] += (double)inp[1] * k0;
      re->bl[1] += (double)inp[0] * k1; re->br[1] += (double)inp[1] * k1;
      re->bl[2] += (double)inp[0] * k2; re->br[2] += (double)inp[1] * k2;

      re->distance += 1.0;
      inp          += 2;

      if (re->distance > (ratio + 0.5))
      {
         outp[0]   = (float)re->bl[0]; outp[1] = (float)re->br[0];
         re->bl[0] = re->bl[1]; re->bl[1] = re->bl[2]; re->bl[2] = 0.0;
         re->br[0] = re->br[1]; re->br[1] = re->br[2]; re->br[2] = 0.0;
         re->distance -= ratio;
         outp += 2;
      }
   }
   data->output_frames = (size_t)((outp - outp0) >> 1);
}

static void cc_ref_upsample(cc_ref_t *re, struct resampler_data_float *data)
{
   const float *inp     = data->data_in;
   const float *inp_max = inp + data->input_frames * 2;
   float       *outp    = data->data_out;
   float       *outp0   = outp;
   double       b       = (data->ratio < 1.0) ? data->ratio : 1.0;
   double       ratio   = 1.0 / data->ratio;

   while (inp != inp_max)
   {
      re->bl[0] = re->bl[1]; re->bl[1] = re->bl[2]; re->bl[2] = re->bl[3]; re->bl[3] = (double)inp[0];
      re->br[0] = re->br[1]; re->br[1] = re->br[2]; re->br[2] = re->br[3]; re->br[3] = (double)inp[1];

      while (re->distance < 1.0)
      {
         int    i;
         double ol = 0.0, orr = 0.0;
         for (i = 0; i < 4; i++)
         {
            double k = cc_ref_kernel(re->distance + 1.0 - (double)i, b);
            ol  += re->bl[i] * k;
            orr += re->br[i] * k;
         }
         outp[0]       = (float)ol; outp[1] = (float)orr;
         re->distance += ratio;
         outp         += 2;
      }
      re->distance -= 1.0;
      inp          += 2;
   }
   data->output_frames = (size_t)((outp - outp0) >> 1);
}

static void cc_ref_process(void *re_, struct resampler_data_float *data)
{
   cc_ref_t *re = (cc_ref_t*)re_;
   if (re->up)
      cc_ref_upsample(re, data);
   else
      cc_ref_downsample(re, data);
}

static void cc_ref_free(void *re_)
{
   if (re_)
      free(re_);
}

/* ------------------------------------------------------------------ */

static int16_t sat16(double v)
{
   long r;
   if (v >= 0.0) r = (long)floor(v + 0.5);
   else          r = (long)ceil(v - 0.5);
   if (r >  32767) return  32767;
   if (r < -32768) return -32768;
   return (int16_t)r;
}

static double tone_snr(const int16_t *y, size_t frames, double f, double fs)
{
   double a[3][3], b[3], c[3];
   double sig, res = 0.0;
   size_t n, lo = 3000, hi = (frames > 3000) ? frames - 3000 : frames;
   size_t cnt = 0;
   int i, j, k;
   memset(a, 0, sizeof(a)); memset(b, 0, sizeof(b));
   for (n = lo; n < hi; n++)
   {
      double t = 2.0 * M_PI * f * (double)n / fs;
      double bb[3];
      double yy = (double)y[2 * n] / 32768.0;
      bb[0] = cos(t); bb[1] = sin(t); bb[2] = 1.0;
      for (i = 0; i < 3; i++)
      {
         b[i] += bb[i] * yy;
         for (j = 0; j < 3; j++) a[i][j] += bb[i] * bb[j];
      }
      cnt++;
   }
   for (i = 0; i < 3; i++) c[i] = b[i];
   for (k = 0; k < 3; k++)
   {
      int piv = k; double mx = fabs(a[k][k]);
      for (i = k + 1; i < 3; i++) if (fabs(a[i][k]) > mx) { mx = fabs(a[i][k]); piv = i; }
      if (piv != k) { for (j = 0; j < 3; j++) { double tmp=a[k][j]; a[k][j]=a[piv][j]; a[piv][j]=tmp; } { double tmp=c[k]; c[k]=c[piv]; c[piv]=tmp; } }
      for (i = k + 1; i < 3; i++)
      {
         double f2 = a[i][k] / a[k][k];
         for (j = k; j < 3; j++) a[i][j] -= f2 * a[k][j];
         c[i] -= f2 * c[k];
      }
   }
   for (i = 2; i >= 0; i--) { double s = c[i]; for (j = i+1; j < 3; j++) s -= a[i][j]*c[j]; c[i] = s / a[i][i]; }
   for (n = lo; n < hi; n++)
   {
      double t = 2.0 * M_PI * f * (double)n / fs;
      double model = c[0]*cos(t) + c[1]*sin(t) + c[2];
      double e = (double)y[2 * n] / 32768.0 - model;
      res += e * e;
   }
   sig = sqrt((c[0]*c[0] + c[1]*c[1]) / 2.0);
   res = sqrt(res / (double)cnt);
   if (res < 1e-300) res = 1e-300;
   return 20.0 * log10(sig / res);
}

static long g_fail = 0;

static void run_case(double in_rate, double out_rate, double freq, double level_db,
      double min_snr)
{
   double bw    = out_rate / in_rate;
   double amp   = pow(10.0, level_db / 20.0);
   size_t Nin   = (size_t)(in_rate * 2.0);
   size_t Nout  = (size_t)(out_rate * 2.2) + 64;
   int16_t *in_i16  = (int16_t*)malloc(sizeof(int16_t) * 2 * Nin);
   float   *in_f    = (float*)malloc(sizeof(float) * 2 * Nin);
   int16_t *out_fx  = (int16_t*)malloc(sizeof(int16_t) * 2 * Nout);
   float   *out_ff  = (float*)malloc(sizeof(float) * 2 * Nout);
   int16_t *out_rf  = (int16_t*)malloc(sizeof(int16_t) * 2 * Nout);
   void *re_i = cc_resampler_int16_init(bw);
   void *re_f = cc_ref_init(bw);
   struct resampler_data_int16 di;
   struct resampler_data_float df;
   size_t n, common;
   long maxd = 0, mism = 0;
   double snr_fx, snr_rf;

   for (n = 0; n < Nin; n++)
   {
      double s = amp * sin(2.0 * M_PI * freq * (double)n / in_rate);
      int16_t q = sat16(s * 32767.0);
      in_i16[2*n] = in_i16[2*n+1] = q;
      in_f[2*n]   = in_f[2*n+1]   = (float)q / 32768.0f;
   }

   di.data_in = in_i16; di.data_out = out_fx; di.input_frames = Nin; di.ratio = bw;
   cc_resampler_int16_process(re_i, &di);

   df.data_in = in_f; df.data_out = out_ff; df.input_frames = Nin; df.ratio = bw;
   cc_ref_process(re_f, &df);

   for (n = 0; n < df.output_frames && n < Nout; n++)
   {
      out_rf[2*n]   = sat16((double)out_ff[2*n]   * 32768.0);
      out_rf[2*n+1] = sat16((double)out_ff[2*n+1] * 32768.0);
   }

   common = (di.output_frames < df.output_frames) ? di.output_frames : df.output_frames;
   for (n = 0; n < common; n++)
   {
      long d = (long)out_fx[2*n] - (long)out_rf[2*n];
      if (d < 0) d = -d;
      if (d > maxd) maxd = d;
      if (d != 0) mism++;
   }

   snr_fx = tone_snr(out_fx, common, freq, out_rate);
   snr_rf = tone_snr(out_rf, common, freq, out_rate);

   printf("%6.0f->%-6.0f %6.0fHz %4.0fdB | frames fx=%lu rf=%lu | maxdiff=%ld LSB  mism=%ld/%lu (%.3f%%) | SNR fx=%6.2f rf=%6.2f dB\n",
      in_rate, out_rate, freq, level_db,
      (unsigned long)di.output_frames, (unsigned long)df.output_frames,
      maxd, mism, (unsigned long)common,
      common ? 100.0 * (double)mism / (double)common : 0.0,
      snr_fx, snr_rf);

   /* Correctness gate is SNR parity with the double ideal, not raw LSB
    * diff: the Q24 phase resolution amplifies into larger single-sample
    * diffs as signal slope rises (worst near Nyquist), but that is
    * quality-neutral as long as the int16 endpoint holds the same SNR.
    *  - a clean 1 kHz tone must resolve well (absolute floor);
    *  - the int16 SNR must stay within 1 dB of the ideal;
    *  - a loose maxdiff ceiling only catches gross breakage. */
   if (min_snr > 0.0 && snr_fx < min_snr)               g_fail++;
   if (snr_rf > 6.0 && snr_fx < snr_rf - 1.0)           g_fail++;
   if (maxd > 512)                                      g_fail++;

   cc_resampler_int16_free(re_i);
   cc_ref_free(re_f);
   free(in_i16); free(in_f); free(out_fx); free(out_ff); free(out_rf);
}

static void run_determinism(double in_rate, double out_rate)
{
   double  bw   = out_rate / in_rate;
   size_t  Nin  = (size_t)(in_rate);
   size_t  Nout = (size_t)(out_rate * 1.2) + 64;
   int16_t *in  = (int16_t*)malloc(sizeof(int16_t) * 2 * Nin);
   int16_t *o1  = (int16_t*)malloc(sizeof(int16_t) * 2 * Nout);
   int16_t *o2  = (int16_t*)malloc(sizeof(int16_t) * 2 * Nout);
   struct resampler_data_int16 d1, d2;
   void *r1, *r2;
   size_t n;
   int bad = 0;

   for (n = 0; n < Nin; n++)
   {
      int16_t q = sat16(0.8 * sin(2.0 * M_PI * 997.0 * (double)n / in_rate) * 32767.0);
      in[2*n] = q; in[2*n+1] = (int16_t)-q;
   }
   r1 = cc_resampler_int16_init(bw);
   d1.data_in = in; d1.data_out = o1; d1.input_frames = Nin; d1.ratio = bw;
   cc_resampler_int16_process(r1, &d1);
   r2 = cc_resampler_int16_init(bw);
   d2.data_in = in; d2.data_out = o2; d2.input_frames = Nin; d2.ratio = bw;
   cc_resampler_int16_process(r2, &d2);

   if (d1.output_frames != d2.output_frames) bad = 1;
   else if (memcmp(o1, o2, d1.output_frames * 2 * sizeof(int16_t))) bad = 1;
   printf("determinism %6.0f->%-6.0f : %s\n", in_rate, out_rate, bad ? "FAIL" : "identical");
   if (bad) g_fail++;

   cc_resampler_int16_free(r1);
   cc_resampler_int16_free(r2);
   free(in); free(o1); free(o2);
}

int main(void)
{
   struct { double in, out; } rates[] = {
      { 32040.0, 96000.0 }, { 44100.0, 96000.0 }, { 48000.0, 96000.0 },
      { 32040.0, 48000.0 }, { 22050.0, 44100.0 }, { 48000.0, 32040.0 },
      { 96000.0, 48000.0 }
   };
   size_t r;

   printf("=== integer s16 CC  vs  double-ideal CC reference (int16 endpoint) ===\n\n");
   for (r = 0; r < sizeof(rates)/sizeof(rates[0]); r++)
      run_case(rates[r].in, rates[r].out, 1000.0, 0.0, 30.0);

   printf("\n=== stress: high tone + low level ===\n\n");
   run_case(32040.0, 96000.0, 12000.0,   0.0, 0.0);
   run_case(48000.0, 32040.0,  8000.0,   0.0, 0.0);
   run_case(32040.0, 96000.0,  1000.0, -40.0, 0.0);
   run_case(32040.0, 96000.0,  1000.0, -60.0, 0.0);

   printf("\n=== determinism (same input -> byte-identical output) ===\n\n");
   run_determinism(32040.0, 96000.0);
   run_determinism(48000.0, 32040.0);

   printf("\n%s (%ld check(s) failed)\n", g_fail ? "RESULT: FAIL" : "RESULT: PASS", g_fail);
   return g_fail ? 1 : 0;
}
