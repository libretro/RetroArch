/* Bit-exactness / accuracy harness: diffs the integer s16 sinc resampler
 * against the faithful float reference across a rate x quality x signal matrix.
 * Build:  cc -O2 -std=c89 -Wall test_sinc_int16.c sinc_resampler_int16.c \
 *            sinc_ref_float.c -lm -o test_sinc_int16 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <audio/sinc_resampler_int16.h>
#include "sinc_ref_float.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int16_t sat16(double v)
{
   long r;
   if (v >= 0.0) r = (long)floor(v + 0.5);
   else          r = (long)ceil(v - 0.5);
   if (r >  32767) return  32767;
   if (r < -32768) return -32768;
   return (int16_t)r;
}

/* THD+N SNR (dB) of a resampled pure tone: fit-and-remove the fundamental. */
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
   /* Solve 3x3 (Gaussian elimination). */
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

static void run_case(double in_rate, double out_rate, int quality,
      const char *qname, double freq, double level_db)
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
   void *re_i = sinc_resampler_int16_init(bw, (enum sinc_int16_quality)quality);
   void *re_f = sinc_ref_float_init(bw, quality);
   struct resampler_data_int16 di;
   struct resampler_data_float df;
   size_t n;
   long maxd = 0, mism = 0;
   size_t common;
   double snr_fx, snr_rf;

   for (n = 0; n < Nin; n++)
   {
      double s = amp * sin(2.0 * M_PI * freq * (double)n / in_rate);
      int16_t q = sat16(s * 32767.0);
      in_i16[2*n] = in_i16[2*n+1] = q;
      in_f[2*n]   = in_f[2*n+1]   = (float)q / 32768.0f;
   }

   di.data_in = in_i16; di.data_out = out_fx; di.input_frames = Nin; di.ratio = bw;
   sinc_resampler_int16_process(re_i, &di);

   df.data_in = in_f; df.data_out = out_ff; df.input_frames = Nin; df.ratio = bw;
   sinc_ref_float_process(re_f, &df);

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

   printf("%-8s %6.0f->%-6.0f %5.0fHz %4.0fdB | frames fx=%lu rf=%lu | maxdiff=%ld LSB  mism=%ld/%lu (%.3f%%) | SNR fx=%6.2f rf=%6.2f dB\n",
      qname, in_rate, out_rate, freq, level_db,
      (unsigned long)di.output_frames, (unsigned long)df.output_frames,
      maxd, mism, (unsigned long)common,
      common ? 100.0 * (double)mism / (double)common : 0.0,
      snr_fx, snr_rf);

   sinc_resampler_int16_free(re_i);
   sinc_ref_float_free(re_f);
   free(in_i16); free(in_f); free(out_fx); free(out_ff); free(out_rf);
}

int main(void)
{
   struct { double in, out; } rates[] = {
      { 32040.0, 96000.0 }, { 44100.0, 96000.0 }, { 48000.0, 96000.0 },
      { 32040.0, 48000.0 }, { 22050.0, 44100.0 }, { 48000.0, 32040.0 }
   };
   struct { int q; const char *n; } quals[] = {
      { 0, "LOWEST" }, { 1, "LOWER" }, { 2, "NORMAL" }, { 3, "HIGHER" }, { 4, "HIGHEST" }
   };
   size_t r, q;

   printf("=== integer s16 sinc  vs  float sinc reference (int16 endpoint, both round-half-away) ===\n\n");
   for (q = 0; q < sizeof(quals)/sizeof(quals[0]); q++)
   {
      for (r = 0; r < sizeof(rates)/sizeof(rates[0]); r++)
         run_case(rates[r].in, rates[r].out, quals[q].q, quals[q].n, 1000.0, 0.0);
      printf("\n");
   }

   printf("=== stress: near-Nyquist tone + low level (NORMAL / HIGHEST, 32040->96000) ===\n\n");
   run_case(32040.0, 96000.0, 2, "NORMAL",  12000.0,   0.0);
   run_case(32040.0, 96000.0, 4, "HIGHEST", 12000.0,   0.0);
   run_case(32040.0, 96000.0, 2, "NORMAL",   1000.0, -40.0);
   run_case(32040.0, 96000.0, 2, "NORMAL",   1000.0, -60.0);
   run_case(32040.0, 96000.0, 4, "HIGHEST",  1000.0, -60.0);
   return 0;
}
