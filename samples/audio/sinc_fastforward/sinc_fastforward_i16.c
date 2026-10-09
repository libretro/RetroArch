/* The integer driver's half: the same rejection down to its s16 floor,
 * an untouched nominal path, the frontend's bound and reset. */

#include <stdlib.h>
#include <string.h>

#include "../../../libretro-common/audio/resampler/drivers/sinc_resampler_int16.c"
#include "sinc_fastforward.h"

static int16_t in_i[SINC_FF_IN * 2];
static int16_t out_i[SINC_FF_OUT * 2];
static int16_t ref_i[SINC_FF_OUT * 2];

static size_t run_i(void *re, const int16_t *in, size_t frames, int16_t *out,
      const double *ratios, size_t nratios, size_t chunk)
{
   size_t i = 0, n = 0, c = 0;
   while (i < frames)
   {
      struct resampler_data_int16 d;
      size_t len     = (frames - i < chunk) ? frames - i : chunk;
      d.data_in      = in + 2 * i;
      d.input_frames = len;
      d.data_out     = out + 2 * n;
      d.ratio        = ratios[c++ % nratios];
      d.output_frames = 0;
      SINC_FF_GUARD_BEGIN();
      sinc_resampler_int16_process(re, &d);
      SINC_FF_GUARD_END();
      SINC_FF_CHECK(d.output_frames
            <= (size_t)((double)(len + 16) * d.ratio) + 1);
      n += d.output_frames;
      i += len;
   }
   return n;
}

static void to_i16(const float *in, size_t frames, int16_t *out)
{
   size_t i;
   for (i = 0; i < 2 * frames; i++)
      out[i] = (int16_t)floor(in[i] * 32768.0 + 0.5);
}

static double rms_i(const int16_t *buf, size_t frames)
{
   size_t i, lo = frames / 4, hi = frames - frames / 8;
   double sum = 0.0;
   if (hi <= lo)
      return 0.0;
   for (i = lo; i < hi; i++)
      sum += (double)buf[2 * i] * buf[2 * i];
   return sqrt(sum / (double)(hi - lo) * 2.0) / 32768.0;
}

size_t sinc_ff_int16_stream(unsigned quality, const float *in, size_t frames,
      float *out, const double *ratios, size_t nratios, size_t chunk)
{
   static const enum sinc_int16_quality qs[] = { SINC_INT16_QUALITY_NORMAL,
      SINC_INT16_QUALITY_HIGHER, SINC_INT16_QUALITY_HIGHEST };
   size_t n, i;
   void *re = sinc_resampler_int16_init_hq(1.0, qs[quality], 0);
   if (!re)
      exit(2);
   to_i16(in, frames, in_i);
   n = run_i(re, in_i, frames, out_i, ratios, nratios, chunk);
   for (i = 0; i < 2 * n; i++)
      out[i] = out_i[i] / 32768.0f;
   sinc_resampler_int16_free(re);
   return n;
}

/* Measured once the stage change and its crossfade are behind it. */
static double tone_i(enum sinc_int16_quality q, int hq, double nominal,
      double live, double f)
{
   static float tone[SINC_FF_IN * 2];
   size_t i = 0, n, settled = 0;
   rarch_sinc_resampler_int16_t *re = (rarch_sinc_resampler_int16_t*)
      sinc_resampler_int16_init_hq(nominal, q, hq);
   if (!re)
      exit(2);
   sinc_ff_tone_f(tone, SINC_FF_IN, f, 0.5);
   to_i16(tone, SINC_FF_IN, in_i);
   while (i + 1024 < SINC_FF_IN / 2 && settled < 4)
   {
      run_i(re, in_i + 2 * i, 1024, out_i, &live, 1, 1024);
      i += 1024;
      if (!re->fade_left)
         settled++;
   }
   SINC_FF_CHECK(settled == 4);
   n = run_i(re, in_i + 2 * i, SINC_FF_IN - i, out_i, &live, 1, 1024);
   sinc_resampler_int16_free(re);
   return rms_i(out_i, n) / 0.5;
}

static void stopband_i(void)
{
   /* Lowest .. Highest, then HQ; the last three meet the s16 floor. */
   static const double floor_db[] = { -55, -55, -55, -90, -90, -90 };
   static const double nominals[] = { 1.0, 1.5, 4.0 };
   static const double speeds[]   = { 2.0, 3.0, 16.0 };
   unsigned q, n, s;
   for (q = 0; q < 6; q++)
      for (n = 0; n < 3; n++)
         for (s = 0; s < 3; s++)
         {
            int hq      = q == 5;
            double live = nominals[n] / speeds[s];
            double fo   = 48000.0 * live;
            double f    = (fo * 0.5 * 1.08 + 24000.0 * 0.95) * 0.5;
            double db, pass;
            enum sinc_int16_quality quality = hq ? SINC_INT16_QUALITY_HIGHEST
               : (enum sinc_int16_quality)q;
            if (hq != (nominals[n] >= 2.0) || f <= fo * 0.5)
               continue;
            db   = 20.0 * log10(tone_i(quality, hq, nominals[n], live, f)
                  + 1e-30);
            pass = tone_i(quality, hq, nominals[n], live,
                  (fo * 0.15 < 7200.0) ? fo * 0.15 : 7200.0);
            if (db > floor_db[q])
               printf("FAIL int16 stopband q%u nominal %g speed %g: %.1f dB\n",
                     q, nominals[n], speeds[s], db);
            SINC_FF_CHECK(db <= floor_db[q]);
            SINC_FF_CHECK(fabs(20.0 * log10(pass)) < 0.05);
         }
}

static void nominal_path_i(void)
{
   static const double scale[] = { 0.98, 0.9995, 1.0, 1.0005, 1.02, 2.0 };
   unsigned q;
   static float noise[SINC_FF_IN * 2];
   sinc_ff_noise_f(noise, SINC_FF_IN / 4, 13);
   to_i16(noise, SINC_FF_IN / 4, in_i);
   for (q = SINC_INT16_QUALITY_LOWEST; q <= SINC_INT16_QUALITY_HIGHEST; q++)
   {
      rarch_sinc_resampler_int16_t *a = (rarch_sinc_resampler_int16_t*)
         sinc_resampler_int16_init_hq(1.0, (enum sinc_int16_quality)q, 0);
      rarch_sinc_resampler_int16_t *b = (rarch_sinc_resampler_int16_t*)
         sinc_resampler_int16_init_hq(1.0, (enum sinc_int16_quality)q, 0);
      size_t i = 0, na = 0, nb = 0, c = 0;
      if (!a || !b)
         exit(2);
      while (i + 333 <= SINC_FF_IN / 4)
      {
         struct resampler_data_int16 da, db;
         da.data_in = in_i + 2 * i; da.input_frames = 333;
         da.data_out = out_i + 2 * na; da.ratio = scale[c++ % 6];
         db = da; db.data_out = ref_i + 2 * nb;
         sinc_resampler_int16_process(a, &da);
         sinc_i16_kernel(b, &db);
         na += da.output_frames; nb += db.output_frames; i += 333;
         SINC_FF_CHECK(!a->dec_stages && !a->fade_left);
      }
      SINC_FF_CHECK(na == nb
            && memcmp(out_i, ref_i, na * 2 * sizeof(int16_t)) == 0);
      sinc_resampler_int16_free(a);
      sinc_resampler_int16_free(b);
   }
}

static void reset_mid_fade_i(void)
{
   static const double ratios[] = { 0.25, 0.5, 1.0 };
   static float noise[SINC_FF_IN * 2];
   unsigned r;
   sinc_ff_noise_f(noise, 8192, 17);
   to_i16(noise, 8192, in_i);
   for (r = 0; r < 3; r++)
   {
      double first = 1.0 / 3.0;
      rarch_sinc_resampler_int16_t *a = (rarch_sinc_resampler_int16_t*)
         sinc_resampler_int16_init_hq(1.0, SINC_INT16_QUALITY_HIGHER, 0);
      void *b = sinc_resampler_int16_init_hq(1.0,
            SINC_INT16_QUALITY_HIGHER, 0);
      size_t na, nb;
      if (!a || !b)
         exit(2);
      run_i(a, in_i, 4096, out_i, &first, 1, 512);
      SINC_FF_CHECK(a->dec_stages == 2);
      first = 1.0;
      run_i(a, in_i, 64, out_i, &first, 1, 64);
      SINC_FF_CHECK(a->fade_left != 0);
      SINC_FF_GUARD_BEGIN();
      sinc_resampler_int16_reset(a);
      SINC_FF_GUARD_END();
      na = run_i(a, in_i, 8192, out_i, &ratios[r], 1, 700);
      nb = run_i(b, in_i, 8192, ref_i, &ratios[r], 1, 700);
      SINC_FF_CHECK(na == nb
            && memcmp(out_i, ref_i, na * 2 * sizeof(int16_t)) == 0);
      sinc_resampler_int16_free(a);
      sinc_resampler_int16_free(b);
   }
}

/* Full-scale input through switches and odd chunks: saturating, never
 * past the bound. */
static void bounds_i(void)
{
   static const double ratios[] = { 1.0, 0.45, 0.3, 0.07, 0.0625, 0.6,
      0.99, 0.5, 1.02, 0.2 };
   static const size_t chunks[] = { 1, 7, 333, 2048 };
   unsigned q, c;
   size_t i;
   for (i = 0; i < SINC_FF_IN; i++)
      in_i[2 * i] = in_i[2 * i + 1] = (i & 1) ? 32767 : -32768;
   for (q = SINC_INT16_QUALITY_LOWEST; q <= SINC_INT16_QUALITY_HIGHEST; q++)
      for (c = 0; c < 4; c++)
      {
         void *re = sinc_resampler_int16_init_hq(1.0,
               (enum sinc_int16_quality)q, 0);
         if (!re)
            exit(2);
         SINC_FF_CHECK(run_i(re, in_i, SINC_FF_IN / 2, out_i, ratios, 10,
                  chunks[c]) > 0);
         sinc_resampler_int16_free(re);
      }
}

void sinc_ff_int16_tests(void)
{
   stopband_i();
   nominal_path_i();
   reset_mid_fade_i();
   bounds_i();
}
