/* Fast-forward through the sinc drivers: a live ratio under the one the
 * table was built for is decimated first, so content above the live
 * output Nyquist is rejected as a table built for that ratio would, at
 * every preset, while the nominal path stays the plain kernel. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../../libretro-common/audio/resampler/drivers/sinc_resampler.c"
#include "sinc_fastforward.h"

unsigned sinc_ff_failures;
#ifdef SINC_TRACK_ALLOCATIONS
unsigned long sinc_ff_allocations, sinc_ff_mark, sinc_ff_guarded;
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
void *__wrap_malloc(size_t n) { sinc_ff_allocations++; return __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t s) { sinc_ff_allocations++; return __real_calloc(n, s); }
void *__wrap_realloc(void *p, size_t n) { sinc_ff_allocations++; return __real_realloc(p, n); }
void __wrap_free(void *p) { sinc_ff_allocations++; __real_free(p); }
#endif

void sinc_ff_tone_f(float *buf, size_t frames, double f, double amp)
{
   size_t i;
   for (i = 0; i < frames; i++)
      buf[2 * i] = buf[2 * i + 1] =
         (float)(amp * sin(2.0 * M_PI * f * (double)i / 48000.0));
}

void sinc_ff_noise_f(float *buf, size_t frames, unsigned seed)
{
   size_t i;
   unsigned long x = 2463534242ul + seed;
   for (i = 0; i < 2 * frames; i++)
   {
      x = (x * 1664525ul + 1013904223ul) & 0xfffffffful;
      buf[i] = (float)((double)(x >> 8) / 16777216.0 - 0.5) * 0.5f;
   }
}

double sinc_ff_rms_f(const float *buf, size_t frames)
{
   size_t i, lo = frames / 4, hi = frames - frames / 8;
   double sum = 0.0;
   if (hi <= lo)
      return 0.0;
   for (i = lo; i < hi; i++)
      sum += (double)buf[2 * i] * buf[2 * i];
   return sqrt(sum / (double)(hi - lo) * 2.0);
}

static float in_f[SINC_FF_IN * 2];
static float out_f[SINC_FF_OUT * 2];
static float ref_f[SINC_FF_OUT * 2];

/* Chunked like a flush; every call is held to the frontend's bound. */
static size_t run(void *re, const float *in, size_t frames, float *out,
      const double *ratios, size_t nratios, size_t chunk)
{
   size_t i = 0, n = 0, c = 0;
   while (i < frames)
   {
      struct resampler_data d;
      size_t len   = (frames - i < chunk) ? frames - i : chunk;
      d.data_in    = in + 2 * i;
      d.input_frames = len;
      d.data_out   = out + 2 * n;
      d.ratio      = ratios[c++ % nratios];
      d.output_frames = 0;
      SINC_FF_GUARD_BEGIN();
      sinc_resampler.process(re, &d);
      SINC_FF_GUARD_END();
      SINC_FF_CHECK(d.output_frames
            <= (size_t)((double)(len + 16) * d.ratio) + 1);
      n += d.output_frames;
      i += len;
   }
   return n;
}

/* Measured once the stage change and its crossfade are behind it. */
static double run_tone(void *re_, double live, double f)
{
   rarch_sinc_resampler_t *re = (rarch_sinc_resampler_t*)re_;
   size_t i = 0, n, settled = 0;
   sinc_ff_tone_f(in_f, SINC_FF_IN, f, 0.5);
   while (i + 1024 < SINC_FF_IN / 2 && settled < 4)
   {
      run(re, in_f + 2 * i, 1024, out_f, &live, 1, 1024);
      i += 1024;
      if (!re->fade_left)
         settled++;
   }
   SINC_FF_CHECK(settled == 4);
   n = run(re, in_f + 2 * i, SINC_FF_IN - i, out_f, &live, 1, 1024);
   return sinc_ff_rms_f(out_f, n) / 0.5;
}

static double leak_db(enum resampler_quality q, int hq, double nominal,
      double live)
{
   double fo    = 48000.0 * live;
   double worst = -400.0;
   double probes[3];
   unsigned p;
   probes[0] = fo * 0.5 * 1.08;
   probes[1] = (fo * 0.5 + 24000.0) * 0.5;
   probes[2] = 24000.0 * 0.95;
   for (p = 0; p < 3; p++)
   {
      double db;
      void *re;
      if (probes[p] <= fo * 0.5 || probes[p] >= 24000.0 * 0.99)
         continue;
      if (!(re = sinc_resampler_init_hq(nominal, q, RESAMPLER_SIMD_SSE
               | RESAMPLER_SIMD_AVX | RESAMPLER_SIMD_NEON, hq)))
         exit(2);
      db = 20.0 * log10(run_tone(re, live, probes[p]) + 1e-30);
      resampler_sinc_free(re);
      if (db > worst)
         worst = db;
   }
   return worst;
}

/* No worse than a table built for the live ratio, which is what init
 * would have made at that nominal; past where building one is sensible,
 * what such tables reach. */
static void stopband(void)
{
   static const double floor_db[] = { -55, -55, -55, -105, -125, -125 };
   static const double nominals[] = { 1.0, 1.5, 0.5, 4.0 };
   static const double speeds[]   = { 1.5, 2.0, 3.0, 8.0, 16.0 };
   unsigned q, n, s;
   for (q = 0; q < 6; q++)
      for (n = 0; n < 4; n++)
         for (s = 0; s < 5; s++)
         {
            int hq      = q == 5;
            double live = nominals[n] / speeds[s];
            double fo   = 48000.0 * live;
            double worst, bound, gain;
            void *re;
            enum resampler_quality quality = hq ? RESAMPLER_QUALITY_HIGHEST
               : (enum resampler_quality)(RESAMPLER_QUALITY_LOWEST + q);
            if (hq != (nominals[n] >= 2.0)
                  || (nominals[n] < 1.0 && speeds[s] > 3.0))
               continue;
            worst = leak_db(quality, hq, nominals[n], live);
            bound = (live >= 0.1) ? leak_db(quality, 0, live, live) + 3.0
                                  : floor_db[q];
            /* The float sums' own floor. */
            if (bound < -130.0)
               bound = -130.0;
            if (worst > bound)
               printf("FAIL stopband q%u nominal %g speed %g: %.1f dB,"
                     " bound %.1f dB\n", q, nominals[n], speeds[s], worst,
                     bound);
            SINC_FF_CHECK(worst <= bound);
            if (!(re = sinc_resampler_init_hq(nominals[n], quality,
                     RESAMPLER_SIMD_SSE, hq)))
               exit(2);
            gain = 20.0 * log10(run_tone(re, live,
                     (fo * 0.15 < 7200.0) ? fo * 0.15 : 7200.0));
            resampler_sinc_free(re);
            /* The Lanczos presets' own phase ripple. */
            if (fabs(gain) >= (q < 2 ? 0.25 : 0.05))
               printf("FAIL passband q%u nominal %g speed %g: %.3f dB\n",
                     q, nominals[n], speeds[s], gain);
            SINC_FF_CHECK(fabs(gain) < (q < 2 ? 0.25 : 0.05));
         }
}

/* Rate control and slow motion never engage it, and the dispatcher hands
 * them to the kernel untouched. */
static void nominal_path(void)
{
   static const double scale[] = { 0.98, 0.9995, 1.0, 1.0005, 1.02, 2.0, 4.0 };
   static const double nominals[] = { 1.0, 0.5, 1.5 };
   unsigned q, n, k;
   sinc_ff_noise_f(in_f, SINC_FF_IN, 7);
   for (q = RESAMPLER_QUALITY_LOWEST; q <= RESAMPLER_QUALITY_HIGHEST; q++)
      for (n = 0; n < 3; n++)
      {
         double ratios[7];
         rarch_sinc_resampler_t *a = (rarch_sinc_resampler_t*)
            sinc_resampler_init_hq(nominals[n], (enum resampler_quality)q,
                  RESAMPLER_SIMD_SSE, 0);
         rarch_sinc_resampler_t *b = (rarch_sinc_resampler_t*)
            sinc_resampler_init_hq(nominals[n], (enum resampler_quality)q,
                  RESAMPLER_SIMD_SSE, 0);
         size_t i = 0, na = 0, nb = 0, c = 0;
         if (!a || !b)
            exit(2);
         for (k = 0; k < 7; k++)
            ratios[k] = nominals[n] * scale[k];
         while (i < SINC_FF_IN / 4)
         {
            struct resampler_data da, db;
            da.data_in = in_f + 2 * i; da.input_frames = 333;
            da.data_out = out_f + 2 * na; da.ratio = ratios[c++ % 7];
            db = da; db.data_out = ref_f + 2 * nb;
            sinc_resampler.process(a, &da);
            b->process(b, &db);
            na += da.output_frames; nb += db.output_frames; i += 333;
            SINC_FF_CHECK(!a->dec_stages && !a->fade_left);
         }
         SINC_FF_CHECK(na == nb
               && memcmp(out_f, ref_f, na * 2 * sizeof(float)) == 0);
         resampler_sinc_free(a);
         resampler_sinc_free(b);
      }
}

/* Measured speed jitters; the stage count must not follow it. */
static void hysteresis(void)
{
   static const double around[] = { 2.0, 4.0, 8.0 };
   unsigned a, c;
   for (a = 0; a < 3; a++)
   {
      unsigned stages = 0, switches = 0;
      rarch_sinc_resampler_t *re = (rarch_sinc_resampler_t*)
         sinc_resampler_init_hq(1.0, RESAMPLER_QUALITY_HIGHEST,
               RESAMPLER_SIMD_SSE, 0);
      if (!re)
         exit(2);
      sinc_ff_noise_f(in_f, 256, 3);
      for (c = 0; c < 600; c++)
      {
         struct resampler_data d;
         double speed = around[a] * (1.0 + 0.05 * sin(c * 0.7));
         d.data_in = in_f; d.input_frames = 256; d.data_out = out_f;
         d.ratio = 1.0 / speed;
         sinc_resampler.process(re, &d);
         if (c && re->dec_stages != stages)
            switches++;
         stages = re->dec_stages;
      }
      SINC_FF_CHECK(switches <= 1);
      SINC_FF_CHECK(stages == (unsigned)(a + 1));
      for (c = 0; c < 200; c++)
      {
         struct resampler_data d;
         d.data_in = in_f; d.input_frames = 256; d.data_out = out_f;
         d.ratio = 1.0 + 0.02 * sin(c * 0.3);
         sinc_resampler.process(re, &d);
      }
      SINC_FF_CHECK(!re->dec_stages && !re->fade_left);
      resampler_sinc_free(re);
   }
}

/* Engaging and releasing crossfade: no dropout, no overshoot, and no
 * edge steeper than the tone at the faster pitch. */
static void transitions(void)
{
   static const enum resampler_quality qs[] = {
      RESAMPLER_QUALITY_NORMAL, RESAMPLER_QUALITY_HIGHEST };
   static const double speeds[] = { 2.0, 4.0, 16.0 };
   unsigned q, s;
   for (q = 0; q < 2; q++)
      for (s = 0; s < 3; s++)
      {
         size_t edge[2];
         size_t n = 0, i, k;
         double seg[3];
         void *re = sinc_resampler_init_hq(1.0, qs[q], RESAMPLER_SIMD_SSE, 0);
         if (!re)
            exit(2);
         seg[0] = 1.0; seg[1] = 1.0 / speeds[s]; seg[2] = 1.0;
         sinc_ff_tone_f(in_f, SINC_FF_IN, 1000.0, 0.5);
         for (k = 0; k < 3; k++)
         {
            n += run(re, in_f + 2 * (SINC_FF_IN / 3) * k, SINC_FF_IN / 3,
                  out_f + 2 * n, &seg[k], 1, 480);
            if (k < 2)
               edge[k] = n;
         }
         for (k = 0; k < 2; k++)
         {
            double fast  = 1000.0 * speeds[s] / 48000.0;
            double steep = 0.5 * 4.0 * sin(M_PI * (fast < 0.5 ? fast : 0.5))
               * sin(M_PI * (fast < 0.5 ? fast : 0.5));
            double worst = 0.0, peak = 0.0, dip = 1.0;
            size_t end   = (edge[k] + 1440 < n) ? edge[k] + 1440 : n;
            for (i = edge[k]; i < end; i++)
            {
               double d2 = fabs(out_f[2 * i] - 2.0 * out_f[2 * i - 2]
                     + out_f[2 * i - 4]);
               if (d2 > worst)
                  worst = d2;
               if (fabs(out_f[2 * i]) > peak)
                  peak = fabs(out_f[2 * i]);
            }
            for (i = edge[k]; i + 48 < end; i += 24)
            {
               size_t j;
               double m = 0.0;
               for (j = 0; j < 48; j++)
                  if (fabs(out_f[2 * (i + j)]) > m)
                     m = fabs(out_f[2 * (i + j)]);
               if (m < dip)
                  dip = m;
            }
            if (speeds[s] * 1000.0 < 24000.0 * seg[1] || k)
               SINC_FF_CHECK(dip > 0.05);
            SINC_FF_CHECK(peak < 0.5 * 1.05);
            SINC_FF_CHECK(worst < 2.5 * steep);
         }
         resampler_sinc_free(re);
      }
}

/* A reset mid-crossfade resumes as a fresh instance would. */
static void reset_mid_fade(void)
{
   static const double ratios[] = { 0.25, 0.5, 1.0 };
   unsigned r;
   sinc_ff_noise_f(in_f, SINC_FF_IN, 11);
   for (r = 0; r < 3; r++)
   {
      double first          = 1.0 / 3.0;
      rarch_sinc_resampler_t *a = (rarch_sinc_resampler_t*)
         sinc_resampler_init_hq(1.0, RESAMPLER_QUALITY_HIGHER,
               RESAMPLER_SIMD_SSE, 0);
      void *b               = sinc_resampler_init_hq(1.0,
            RESAMPLER_QUALITY_HIGHER, RESAMPLER_SIMD_SSE, 0);
      size_t na, nb;
      if (!a || !b)
         exit(2);
      run(a, in_f, 4096, out_f, &first, 1, 512);
      SINC_FF_CHECK(a->dec_stages == 2);
      first = 1.0;
      run(a, in_f, 64, out_f, &first, 1, 64);
      SINC_FF_CHECK(a->fade_left != 0);
      SINC_FF_GUARD_BEGIN();
      sinc_resampler.reset(a);
      SINC_FF_GUARD_END();
      na = run(a, in_f, 8192, out_f, &ratios[r], 1, 700);
      nb = run(b, in_f, 8192, ref_f, &ratios[r], 1, 700);
      SINC_FF_CHECK(na == nb
            && memcmp(out_f, ref_f, na * 2 * sizeof(float)) == 0);
      resampler_sinc_free(a);
      resampler_sinc_free(b);
   }
}

/* Odd chunk sizes through switches in both directions. */
static void bounds(void)
{
   static const double ratios[] = { 1.0, 0.45, 0.3, 0.07, 0.0625, 0.6,
      0.99, 0.5, 1.02, 0.2 };
   static const size_t chunks[] = { 1, 7, 64, 333, 2048 };
   unsigned q, c;
   sinc_ff_noise_f(in_f, SINC_FF_IN, 5);
   for (q = RESAMPLER_QUALITY_LOWEST; q <= RESAMPLER_QUALITY_HIGHEST; q++)
      for (c = 0; c < 5; c++)
      {
         void *re = sinc_resampler_init_hq(1.0, (enum resampler_quality)q,
               RESAMPLER_SIMD_SSE, 0);
         size_t n;
         if (!re)
            exit(2);
         n = run(re, in_f, SINC_FF_IN / 2, out_f, ratios, 10, chunks[c]);
         SINC_FF_CHECK(n > 0);
         for (n = n * 2; n; n--)
            SINC_FF_CHECK(out_f[n - 1] == out_f[n - 1]
                  && fabs(out_f[n - 1]) < 2.0);
         resampler_sinc_free(re);
      }
}

/* The integer driver implements the same design. */
static void agreement(void)
{
   static const double ratios[] = { 1.0, 0.5, 0.25, 0.3, 1.0 };
   unsigned q;
   for (q = 0; q < 3; q++)
   {
      static const enum resampler_quality fq[] = { RESAMPLER_QUALITY_NORMAL,
         RESAMPLER_QUALITY_HIGHER, RESAMPLER_QUALITY_HIGHEST };
      size_t n, ni, i;
      double worst = 0.0;
      void *re = sinc_resampler_init_hq(1.0, fq[q], RESAMPLER_SIMD_SSE, 0);
      if (!re)
         exit(2);
      sinc_ff_noise_f(in_f, SINC_FF_IN / 2, 9);
      for (i = 0; i < SINC_FF_IN; i++)
         in_f[i] = (float)floor(in_f[i] * 32768.0f + 0.5f) / 32768.0f;
      n  = run(re, in_f, SINC_FF_IN / 2, out_f, ratios, 5, 4096);
      ni = sinc_ff_int16_stream(q, in_f, SINC_FF_IN / 2, ref_f, ratios, 5,
            4096);
      SINC_FF_CHECK(n == ni);
      for (i = 0; i < 2 * (n < ni ? n : ni); i++)
         if (fabs(out_f[i] - ref_f[i]) > worst)
            worst = fabs(out_f[i] - ref_f[i]);
      SINC_FF_CHECK(worst * 32768.0 < 4.0);
      resampler_sinc_free(re);
   }
}

int main(void)
{
   stopband();
   nominal_path();
   hysteresis();
   transitions();
   reset_mid_fade();
   bounds();
   agreement();
   sinc_ff_int16_tests();
   SINC_FF_CHECK(!SINC_FF_GUARD_CALLS());
   printf("Sinc fast-forward: %u failures\n", sinc_ff_failures);
   return sinc_ff_failures ? 1 : 0;
}
