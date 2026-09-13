#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <audio/sinc_resampler.h>
#include <audio/sinc_resampler_int16.h>

#if defined(__AVX__)
#define TEST_SIMD RESAMPLER_SIMD_AVX
#else
#define TEST_SIMD RESAMPLER_SIMD_SSE
#endif

#define INPUT 2048
#define CAP (INPUT * 9 + 64)
static float input[INPUT * 2], a[CAP * 2], b[CAP * 2];
static int16_t input_i[INPUT * 2], ai[CAP * 2], bi[CAP * 2];
static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); failures++; } } while (0)
#ifdef SINC_REFERENCE
extern retro_resampler_t reference_sinc;
extern void *reference_i_init(double, enum sinc_int16_quality);
extern void reference_i_process(void *, struct resampler_data_int16 *);
extern void reference_i_free(void *);
#else
#define reference_sinc sinc_resampler
#define reference_i_init sinc_resampler_int16_init
#define reference_i_process sinc_resampler_int16_process
#define reference_i_free sinc_resampler_int16_free
#endif

static size_t run(void *state, const retro_resampler_t *driver,
      float *out, double ratio, unsigned chunk)
{
   size_t pos = 0, n = 0;
   while (pos < INPUT)
   {
      struct resampler_data d;
      d.input_frames = INPUT - pos < chunk ? INPUT - pos : chunk;
      d.data_in = input + pos * 2;
      d.data_out = out + n * 2;
      d.ratio = ratio;
      d.output_frames = 0;
      driver->process(state, &d);
      pos += d.input_frames;
      n += d.output_frames;
   }
   CHECK(n < CAP);
   return n;
}

static size_t run_i(void *state, int16_t *out, double ratio, unsigned chunk,
      void (*process)(void *, struct resampler_data_int16 *))
{
   size_t pos = 0, n = 0;
   while (pos < INPUT)
   {
      struct resampler_data_int16 d;
      d.input_frames = INPUT - pos < chunk ? INPUT - pos : chunk;
      d.data_in = input_i + pos * 2;
      d.data_out = out + n * 2;
      d.ratio = ratio;
      d.output_frames = 0;
      process(state, &d);
      pos += d.input_frames;
      n += d.output_frames;
   }
   CHECK(n < CAP);
   return n;
}

static void bypass(double ratio, enum resampler_quality quality, int hq)
{
   enum sinc_int16_quality iq = quality == RESAMPLER_QUALITY_DONTCARE ? SINC_INT16_QUALITY_NORMAL
      : (enum sinc_int16_quality)(quality - 1);
   void *old = reference_sinc.init(NULL, ratio, quality, 0);
   void *now = sinc_resampler_init_hq(ratio, quality, 0, hq);
   void *old_i = reference_i_init(ratio, iq);
   void *now_i = sinc_resampler_int16_init_hq(ratio, iq, hq);
   size_t na, nb;
   CHECK(old && now && old_i && now_i);
   if (!old || !now || !old_i || !now_i) exit(2);
   na = run(old, &reference_sinc, a, ratio, 127);
   nb = run(now, &sinc_resampler, b, ratio, 127);
   CHECK(na == nb && memcmp(a, b, na * 2 * sizeof(float)) == 0);
   na = run_i(old_i, ai, ratio, 127, reference_i_process);
   nb = run_i(now_i, bi, ratio, 127, sinc_resampler_int16_process);
   CHECK(na == nb && memcmp(ai, bi, na * 2 * sizeof(int16_t)) == 0);
   reference_sinc.free(old); sinc_resampler.free(now);
   reference_i_free(old_i); sinc_resampler_int16_free(now_i);
}

static void active(double ratio)
{
   void *c = sinc_resampler_init_hq(ratio, RESAMPLER_QUALITY_NORMAL, 0, 1);
   void *simd = sinc_resampler_init_hq(ratio, RESAMPLER_QUALITY_NORMAL,
         TEST_SIMD, 1);
   void *integer = sinc_resampler_int16_init_hq(ratio, SINC_INT16_QUALITY_NORMAL, 1);
   size_t na, nb, ni, j;
   unsigned step;
   double max_error = 0.0;
   CHECK(c && simd && integer);
   if (!c || !simd || !integer) exit(2);
   for (step = 0; step < 3; step++)
   {
      double live_ratio = ratio * (1.0 + ((int)step - 1) * 0.0005);
      na = run(c, &sinc_resampler, a, live_ratio, INPUT);
      nb = run(simd, &sinc_resampler, b, live_ratio, 127);
      ni = run_i(integer, ai, live_ratio, 127, sinc_resampler_int16_process);
      CHECK(na == nb && na == ni);
      for (j = 0; j < na * 2; j++)
      {
         double error = fabs(a[j] - b[j]);
         if (error > max_error) max_error = error;
         CHECK(a[j] == a[j] && fabs(a[j]) < 2.0);
         CHECK(fabs((double)ai[j] - a[j] * 32768.0) < 1.1);
      }
   }
   CHECK(max_error < 2e-6);
   sinc_resampler.reset(c);
   sinc_resampler.reset(simd);
   na = run(c, &sinc_resampler, a, ratio, INPUT);
   nb = run(simd, &sinc_resampler, b, ratio, 1);
   CHECK(na == nb);
   for (j = 0; j < na * 2; j++) CHECK(fabs(a[j] - b[j]) < 2e-6);
   printf("HQ ratio %.6f: scalar/SIMD max error %.9g\n", ratio, max_error);
   sinc_resampler.free(c); sinc_resampler.free(simd);
   sinc_resampler_int16_free(integer);
}

static void impulse(const char *prefix)
{
   unsigned hq;
   memset(input, 0, sizeof(input));
   input[0] = input[1] = 1.0f;
   for (hq = 0; hq < 2; hq++)
   {
      char path[1024];
      FILE *file;
      size_t n;
      void *r = sinc_resampler_init_hq(4.0, RESAMPLER_QUALITY_HIGHEST, 0, hq);
      if (!r) exit(2);
      n = run(r, &sinc_resampler, a, 4.0, INPUT);
      sprintf(path, "%s-%s.f32", prefix, hq ? "hq" : "highest");
      file = fopen(path, "wb");
      if (!file) exit(2);
      CHECK(fwrite(a, sizeof(float) * 2, n, file) == n);
      fclose(file);
      sinc_resampler.free(r);
   }
}

static void benchmark(void)
{
   unsigned hq, lane, repeat, j;
   for (hq = 0; hq < 2; hq++)
      for (lane = 0; lane < 2; lane++)
      {
         double times[5];
         for (repeat = 0; repeat < 5; repeat++)
         {
            clock_t begin;
            void *state = lane ? sinc_resampler_int16_init_hq(4,
                  SINC_INT16_QUALITY_HIGHEST, hq)
               : sinc_resampler_init_hq(4, RESAMPLER_QUALITY_HIGHEST,
                     RESAMPLER_SIMD_SSE, hq);
            if (!state) exit(2);
            begin = clock();
            for (j = 0; j < 256; j++)
            {
               if (lane) run_i(state, ai, 4, INPUT, sinc_resampler_int16_process);
               else run(state, &sinc_resampler, a, 4, INPUT);
            }
            times[repeat] = (double)(clock() - begin) / CLOCKS_PER_SEC;
            if (lane) sinc_resampler_int16_free(state);
            else sinc_resampler.free(state);
         }
         printf("BENCH %s %s, 10.923 seconds audio: %.3f %.3f %.3f %.3f %.3f seconds\n",
               hq ? "HQ" : "Highest", lane ? "int16" : "SSE",
               times[0], times[1], times[2], times[3], times[4]);
      }
}

int main(int argc, char **argv)
{
   const double ratios[] = {0.5, 1.0, 1.5, 1.9999, 2.0, 4.0, 8.0, 384000.0/44100.0};
   unsigned r, q, j;
   uint32_t noise = 1;
   for (j = 0; j < INPUT * 2; j++)
   {
      noise = noise * 1664525u + 1013904223u;
      input_i[j] = (int16_t)((int)(noise >> 18) - 8192);
      input[j] = input_i[j] / 32768.0f;
   }
   for (r = 0; r < sizeof(ratios)/sizeof(ratios[0]); r++)
   {
      for (q = RESAMPLER_QUALITY_DONTCARE; q <= RESAMPLER_QUALITY_HIGHEST; q++)
      {
         bypass(ratios[r], (enum resampler_quality)q, 0);
         if (ratios[r] < 2.0) bypass(ratios[r], (enum resampler_quality)q, 1);
      }
      if (ratios[r] >= 2.0) active(ratios[r]);
   }
   if (argc == 2 && strcmp(argv[1], "--bench") == 0) benchmark();
   else if (argc == 2 && strlen(argv[1]) < 990) impulse(argv[1]);
   printf("Sinc HQ: %u failures\n", failures);
   return failures ? 1 : 0;
}
